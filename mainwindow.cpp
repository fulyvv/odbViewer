#include "mainwindow.h"
#include "ui_mainwindow.h"

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

    auto style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
    ui->vtkWidget->interactor()->SetInteractorStyle(style);

    connect(ui->actionopen, &QAction::triggered, this, &MainWindow::openFile);
    connect(ui->actionsave_as, &QAction::triggered, this, &MainWindow::saveFile);
    connect(ui->treeView, &QTreeView::activated, this, &MainWindow::onTreeItemActivated);

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
        m_gridBuilder = std::make_unique<CreateVTKUnstucturedGrid>(*m_odb);
        m_vtkDisplay.displaySolid(m_gridBuilder->getGrid());
        m_vtkDisplay.setCameraView();
        m_vtkDisplay.addAxes();

        m_odb->releaseGeometryCache();
        // 打开时不读取 U/UR/S，按需加载
        const auto frames = m_odb->getAvailableStepsFrames();
        if (!frames.empty()) {
            m_selectedStepFrame = frames.front();
        }

        m_vtkDisplay.getRenderWindow()->Render();
        ui->statusBar->showMessage(tr("Successfully opened ODB file: %1").arg(fileName), 5000);

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

    // 顶层
    QStandardItem* instancesRoot = new QStandardItem(tr("实例"));
    QStandardItem* stepsRoot = new QStandardItem(tr("步与帧"));
    QStandardItem* fieldsRoot = new QStandardItem(tr("场变量"));

    m_treeModel->appendRow(instancesRoot);
    m_treeModel->appendRow(stepsRoot);
    m_treeModel->appendRow(fieldsRoot);

    if (!m_odb)
        return;

    //实例
    for (const auto& info : m_odb->getInstanceInfos()) {
        QStandardItem* instItem = new QStandardItem(QString::fromStdString(info.name));
        instancesRoot->appendRow(instItem);
    }

    //步与帧（按步分组）
    const auto frames = m_odb->getAvailableStepsFrames();
    std::map<QString, std::vector<StepFrameInfo>> grouped;
    for (const auto& sf : frames) {
        grouped[QString::fromStdString(sf.stepName)].push_back(sf);
    }
    for (const auto& [stepName, vec] : grouped) {
        QStandardItem* stepItem = new QStandardItem(stepName);
        stepsRoot->appendRow(stepItem);
        for (const auto& sf : vec) {
            QString frameText = tr("Frame %1, Time %2")
                                    .arg(sf.frameIndex)
                                    .arg(sf.frameValue);
            QStandardItem* frameItem = new QStandardItem(frameText);
            frameItem->setData(sf.frameIndex, Qt::UserRole + 1);
            frameItem->setData(QString::fromStdString(sf.stepName), Qt::UserRole + 2);
            stepItem->appendRow(frameItem);
        }
    }

    //可用场变量
    if (!frames.empty()) {
        const auto& first = frames.front();
        auto fieldInfos = m_odb->listFieldNames(first.stepName, first.frameIndex);
        if (!fieldInfos.empty()) {
            for (const auto& kv : fieldInfos) {
                const QString fieldName = QString::fromStdString(kv.first);
                QStandardItem* fieldItem = new QStandardItem(fieldName);
                fieldItem->setData(fieldName, Qt::UserRole + 1);
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

void MainWindow::onTreeItemActivated(const QModelIndex& index)
{
    if (!m_odb || !m_treeModel) return;
    QStandardItem* item = m_treeModel->itemFromIndex(index);
    if (!item) return;

    // 找到顶层根节点
    QStandardItem* root = item;
    while (root->parent()) root = root->parent();
    const QString rootName = root->text();

    if (rootName == tr("步与帧")) {
        // 选择帧：更新当前选中帧
        const int frameIndex = item->data(Qt::UserRole + 1).toInt();
        const QString stepName = item->data(Qt::UserRole + 2).toString();
        if (!stepName.isEmpty()) {
            StepFrameInfo sf;
            sf.stepName = stepName.toStdString();
            sf.frameIndex = frameIndex;
            sf.frameValue = 0.0;
            sf.description = item->text().toStdString();
            m_selectedStepFrame = sf;
            ui->statusBar->showMessage(tr("当前帧: %1 / %2").arg(stepName).arg(frameIndex), 3000);
        }
        return;
    }

    if (rootName == tr("场变量")) {
        // 选择场
        QString fieldName = item->data(Qt::UserRole + 1).toString();
        if (fieldName.isEmpty() && item->parent()) {
            fieldName = item->parent()->data(Qt::UserRole + 1).toString();
        }
        if (fieldName.isEmpty()) return;

        // 若未选中帧，使用第一个可用帧
        StepFrameInfo sf = m_selectedStepFrame;
        const auto frames = m_odb->getAvailableStepsFrames();
        if (sf.stepName.empty() && !frames.empty()) {
            sf = frames.front();
        }

        try {
            // 按需读取：只读取用户选择的场变量，减少内存占用
            m_odb->readSingleField(sf.stepName, sf.frameIndex, fieldName.toStdString());
            const FieldData* fdPtr = m_odb->getFieldData(fieldName.toStdString());
            if (!fdPtr) {
                QMessageBox::warning(this, tr("Warning"), tr("字段 %1 不存在于当前帧").arg(fieldName));
                return;
            }
            if (!m_gridBuilder) {
                m_gridBuilder = std::make_unique<CreateVTKUnstucturedGrid>(*m_odb);
            }
            const FieldData& fd = *fdPtr;
            if (!m_gridBuilder->addFieldData(fd)) {
                QMessageBox::warning(this, tr("Warning"), tr("添加字段失败: %1").arg(fieldName));
                return;
            }

            // 对 U/UR 计算模长并显示
            if (fd.type == FieldType::DISPLACEMENT || fd.type == FieldType::ROTATION) {
                const QString magName = fieldName + ".Magnitude";
                m_vtkDisplay.addPointVectorMagnitude(m_gridBuilder->getGrid(), fieldName.toStdString(), magName.toStdString());
                m_vtkDisplay.displayWithScalarField(m_gridBuilder->getGrid(), magName.toStdString(), true);
            } else if (fd.type == FieldType::STRESS) {
                // 默认显示张量第一个分量
                m_vtkDisplay.displayWithScalarField(m_gridBuilder->getGrid(), fieldName.toStdString(), false);
            }

            m_vtkDisplay.getRenderWindow()->Render();
            ui->statusBar->showMessage(tr("显示字段: %1 (帧 %2)").arg(fieldName).arg(sf.frameIndex), 3000);
        } catch (const std::exception& e) {
            QMessageBox::critical(this, tr("Error"), tr("加载字段失败: %1").arg(e.what()));
            return;
        }
    }
}

void MainWindow::saveFile()
{
    if (!m_odb) {
        QMessageBox::warning(this, tr("Warning"), tr("No ODB file is loaded."));
        return;
    }

    QString defaultPath = QString::fromStdString(m_odb->getOdbPath());
    QString defaultName = QString::fromStdString(m_odb->getOdbBaseName()) + ".vtu";
    QString defaultFull = defaultPath + "/" + defaultName;

    QString fileName = QFileDialog::getSaveFileName(this, tr("Save VTU"), defaultFull, 
        tr("VTK Unstructured Grid (*.vtu)"));

    if (fileName.isEmpty()) {
        return;
    }

    try {
        if (!m_gridBuilder || !m_gridBuilder->getGrid()) {
            throw std::runtime_error("Grid is not available to save");
        }

        if (!m_gridBuilder->writeToFile(fileName.toStdString())) {
            throw std::runtime_error("Failed to write VTU file");
        }

        ui->statusBar->showMessage(tr("Saved VTU: %1").arg(fileName), 5000);
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save VTU file:\n%1").arg(e.what()));
        return;
    }
}
