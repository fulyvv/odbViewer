#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QVTKOpenGLNativeWidget.h>
#include <QFileDialog>
#include <QMessageBox>
#include <map>
#include <memory>
#include "vtkdisplay.h"
#include "odbmanager.h"


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
	void openFile();
    void saveFile();
    void onTreeItemActivated(const QModelIndex& index);

private:
    void buildModelTree();

private:
    Ui::MainWindow *ui;

	VTKDisplayManager m_vtkDisplay;
	std::unique_ptr<readOdb> m_odb;
    std::unique_ptr<CreateVTKUnstucturedGrid> m_gridBuilder;
    StepFrameInfo m_selectedStepFrame;
    QStandardItemModel* m_treeModel{nullptr};
};
#endif // MAINWINDOW_H
