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

        const auto frames = m_odb->getAvailableStepsFrames();


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

    // 3) 可用场变量（轻探测：不读取 bulkData）
    if (!frames.empty()) {
        const auto& first = frames.front();
        auto fieldInfos = m_odb->listFieldNames(first.stepName, first.frameIndex);
        if (!fieldInfos.empty()) {
            for (const auto& kv : fieldInfos) {
                const QString fieldName = QString::fromStdString(kv.first);
                QStandardItem* fieldItem = new QStandardItem(fieldName);
                fieldsRoot->appendRow(fieldItem);
                for (const auto& compLabel : kv.second) {
                    fieldItem->appendRow(new QStandardItem(QString::fromStdString(compLabel)));
                }
            }
        } else {
            fieldsRoot->appendRow(new QStandardItem(tr("未发现场变量")));
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

