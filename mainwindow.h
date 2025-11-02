#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
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

private:
    Ui::MainWindow *ui;

	VTKDisplayManager m_vtkDisplay;
	std::unique_ptr<readOdb> m_odb;

    // 模型树
    QStandardItemModel* m_treeModel{nullptr};
    void buildModelTree();
};
#endif // MAINWINDOW_H
