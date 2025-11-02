#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QVTKOpenGLNativeWidget.h>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QStandardItem>
#include <map>

#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include "creategrid.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->vtkWidget->setRenderWindow(m_vtkDisplay.getRenderWindow());
    m_vtkDisplay.setInteractor(ui->vtkWidget->interactor());

    // 将交互样式设置为轨迹球相机，确保良好交互体验
    auto style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
    ui->vtkWidget->interactor()->SetInteractorStyle(style);

	connect(ui->actionopen, &QAction::triggered, this, &MainWindow::openFile);
    connect(ui->actionsave_as, &QAction::triggered, this, &MainWindow::saveFile);

    // 初始化左侧模型树
    m_treeModel = new QStandardItemModel(this);
    m_treeModel->setColumnCount(1);
    m_treeModel->setHorizontalHeaderLabels({tr("模型")});
    ui->treeView->setModel(m_treeModel);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openFile() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open ODB File"), QString(),
        tr("Abaqus ODB File (*.odb)"));
    
    if (fileName.isEmpty()) {
        return;
    }

    try {
        m_odb = std::make_unique<readOdb>(fileName.toStdString().c_str());
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to open ODB file:\n%1").arg(e.what()));
        return;
    }

    try {
		CreateVTKUnstucturedGrid grid(*m_odb);
        m_vtkDisplay.displaySolid(grid.getGrid());
        m_vtkDisplay.setCameraView();
        m_vtkDisplay.addAxes();
        m_vtkDisplay.getRenderWindow()->Render();

        // 读取首帧的场变量（用于测试显示 U）
        const auto frames = m_odb->getAvailableStepsFrames();
        if (!frames.empty()) {
            const auto& sf = frames.front();
            if (m_odb->readFieldOutput(sf.stepName, sf.frameIndex) && m_odb->hasFieldData("U")) {
                const FieldData* uData = m_odb->getFieldData("U");
                if (uData) {
                    // 计算位移模长 U.Magnitude 并作为点标量添加
                    std::vector<double> uMag(m_odb->m_nodesNum, 0.0);
                    for (std::size_t i = 0; i < m_odb->m_nodesNum; ++i) {
                        if (i < uData->nodeValues.size() && uData->nodeValidFlags[i]) {
                            const auto& v = uData->nodeValues[i];
                            const double u1 = v.size() > 0 ? v[0] : 0.0;
                            const double u2 = v.size() > 1 ? v[1] : 0.0;
                            const double u3 = v.size() > 2 ? v[2] : 0.0;
                            uMag[i] = std::sqrt(u1 * u1 + u2 * u2 + u3 * u3);
                        } else {
                            uMag[i] = 0.0;
                        }
                    }
                    grid.addPointScalar("U.Magnitude", uMag);
                    // 激活标量显示（点数据）
                    m_vtkDisplay.setActiveScalar(grid.getGrid(), "U.Magnitude", /*usePointData*/ true);
                }
            }
        }


		ui->statusBar->showMessage(tr("Successfully opened ODB file: %1").arg(fileName), 5000);
        // 构建左侧模型树（实例、步/帧、可用场变量）
        buildModelTree();
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to display ODB data:\n%1").arg(e.what()));
        return;
	}
}

void MainWindow::buildModelTree()
{
    if (!m_treeModel)
        return;
    m_treeModel->clear();
    m_treeModel->setColumnCount(1);
    m_treeModel->setHorizontalHeaderLabels({tr("模型")});

    // 顶层分类节点
    QStandardItem* instancesRoot = new QStandardItem(tr("实例"));
    QStandardItem* stepsRoot = new QStandardItem(tr("步与帧"));
    QStandardItem* fieldsRoot = new QStandardItem(tr("场变量"));

    m_treeModel->appendRow(instancesRoot);
    m_treeModel->appendRow(stepsRoot);
    m_treeModel->appendRow(fieldsRoot);

    if (!m_odb)
        return;

    // 1) 实例
    for (const auto& instName : m_odb->m_instanceNames) {
        QStandardItem* instItem = new QStandardItem(QString::fromStdString(instName));
        instancesRoot->appendRow(instItem);
    }

    // 2) 步与帧（按步分组）
    const auto frames = m_odb->getAvailableStepsFrames();
    std::map<QString, std::vector<StepFrameInfo>> grouped;
    for (const auto& sf : frames) {
        grouped[QString::fromStdString(sf.stepName)].push_back(sf);
    }
    for (const auto& [stepName, vec] : grouped) {
        QStandardItem* stepItem = new QStandardItem(stepName);
        stepsRoot->appendRow(stepItem);
        for (const auto& sf : vec) {
            QString frameText = tr("帧 %1 (id=%2, value=%3)")
                                    .arg(QString::fromStdString(sf.description))
                                    .arg(sf.frameIndex)
                                    .arg(sf.frameValue);
            QStandardItem* frameItem = new QStandardItem(frameText);
            stepItem->appendRow(frameItem);
        }
    }

    // 3) 可用场变量（取第一帧快速探测）
    if (!frames.empty()) {
        const auto& first = frames.front();
        if (m_odb->readFieldOutput(first.stepName, first.frameIndex)) {
            // m_fieldDataMap 的 key 为字段名，例如 "U"、"UR"、"S"
            for (const auto& kv : m_odb->m_fieldDataMap) {
                const QString fieldName = QString::fromStdString(kv.first);
                QStandardItem* fieldItem = new QStandardItem(fieldName);
                fieldsRoot->appendRow(fieldItem);
                // 展示分量标签
                for (const auto& compLabel : kv.second.componentLabels) {
                    fieldItem->appendRow(new QStandardItem(QString::fromStdString(compLabel)));
                }
            }
        } else {
            fieldsRoot->appendRow(new QStandardItem(tr("未能读取第一帧的场变量")));
        }
    } else {
        fieldsRoot->appendRow(new QStandardItem(tr("无可用帧")));
    }
}

void MainWindow::saveFile()
{
    if (!m_odb) {
        QMessageBox::warning(this, tr("Warning"), tr("No ODB file is loaded."));
        return;
    }

    // 默认保存到 ODB 同目录，文件名同名 .vtu
    QString defaultPath = QString::fromStdString(m_odb->getOdbPath());
    QString defaultName = QString::fromStdString(m_odb->getOdbBaseName()) + ".vtu";
    QString defaultFull = defaultPath + "/" + defaultName;

    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Save VTU"),
        defaultFull,
        tr("VTK Unstructured Grid (*.vtu)")
        );

    if (fileName.isEmpty()) {
        return;
    }

    try {
        CreateVTKUnstucturedGrid grid(*m_odb);

        // 若当前已读取某帧场数据，则将所有可用场变量写入
        if (!m_odb->m_fieldDataMap.empty()) {
            for (const auto& kv : m_odb->m_fieldDataMap) {
                const FieldData& fd = kv.second;
                if (fd.type == FieldType::STRESS) {
                    // 写入应力张量并计算/输出 Mises
                    grid.addStressField(fd, "ALL");
                } else {
                    // 位移/旋转作为多分量点数据写入（不应用形变）
                    grid.addFieldData(fd);
                }
            }
        }

        if (!grid.writeToFile(fileName.toStdString())) {
            throw std::runtime_error("Failed to write VTU file");
        }
        ui->statusBar->showMessage(tr("Saved VTU: %1").arg(fileName), 5000);
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save VTU file:\n%1").arg(e.what()));
        return;
    }
}

