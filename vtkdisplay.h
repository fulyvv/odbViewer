#ifndef VTKDISPLAY_H
#define VTKDISPLAY_H

#include <algorithm>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkDataSetMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkScalarBarActor.h>
#include <vtkLookupTable.h>
#include <vtkCamera.h>
#include <vtkAxesActor.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkColorTransferFunction.h>
#include <vtkTextProperty.h>

#include "odbmanager.h"
#include "creategrid.h"


// VTK显示管理器类
class VTKDisplayManager
{
public:
    VTKDisplayManager();

    // 显示网格（线框模式）
    void displayWireframe(vtkUnstructuredGrid* grid);

    void displaySolid(vtkUnstructuredGrid* grid);

    // 显示带标量场的网格（彩色云图）
    void displayWithScalarField(vtkUnstructuredGrid* grid, const std::string& scalarName, bool usePointData);

    // 添加坐标轴
    void addAxes();

    // 设置相机视角
    void setCameraView();

    // 开始显示
    void start();

    // 由外部（如 QVTK 小部件）传入交互器，以避免重复创建
    void setInteractor(vtkRenderWindowInteractor* interactor);

    vtkGenericOpenGLRenderWindow* getRenderWindow() const { return renderWindow.Get(); }
    vtkRenderer* getRenderer() const { return renderer.Get(); }
    vtkRenderWindowInteractor* getRenderWindowInteractor() const { return renderWindowInteractor.Get(); }
private:
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow;
    vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor;

    void addScalarBar(vtkSmartPointer<vtkDataSetMapper> mapper, const std::string& title);
};
#endif // VTKDISPLAY_H
