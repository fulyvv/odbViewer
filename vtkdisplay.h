#ifndef VTKDISPLAY_H
#define VTKDISPLAY_H

#include <algorithm>
#include <string>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkDataSetMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkActorCollection.h>
#include <vtkActor2DCollection.h>
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
#include <vtkDataArray.h>
#include <vtkArrayCalculator.h>
#include <vtkDataSet.h>

#include "odbmanager.h"
#include "creategrid.h"


// VTK显示管理器类
class VTKDisplayManager
{
public:
    VTKDisplayManager();

    void displayWireframe(vtkUnstructuredGrid* grid);
    void displaySolid(vtkUnstructuredGrid* grid);

    void displayWithScalarField(vtkUnstructuredGrid* grid, const std::string& scalarName, bool usePointData);
    bool setActiveScalar(vtkUnstructuredGrid* grid, const std::string& name, bool usePointData);

    bool addPointVectorMagnitude(vtkUnstructuredGrid* grid, const std::string& vectorName, const std::string& outputName);
    void addAxes();
    void setCameraView();
    void start();

public:
    void setInteractor(vtkRenderWindowInteractor* interactor);
    vtkGenericOpenGLRenderWindow* getRenderWindow() const { return renderWindow.Get(); }
    vtkRenderer* getRenderer() const { return renderer.Get(); }
    vtkRenderWindowInteractor* getRenderWindowInteractor() const { return renderWindowInteractor.Get(); }
private:
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow;
    vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor;

    vtkSmartPointer<vtkDataSetMapper> m_mapper;
    vtkSmartPointer<vtkActor> m_actor;
    vtkSmartPointer<vtkScalarBarActor> m_scalarBar;
    vtkSmartPointer<vtkLookupTable> m_lut;
    vtkSmartPointer<vtkOrientationMarkerWidget> axes_widget;
    bool m_actorAdded = false;
    bool m_scalarBarAdded = false;

    void addScalarBar(vtkSmartPointer<vtkDataSetMapper> mapper, const std::string& title);
};
#endif // VTKDISPLAY_H
