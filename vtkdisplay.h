#ifndef VTKDISPLAY_H
#define VTKDISPLAY_H

#include <string>
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
#include <vtkTextProperty.h>

#include "odbmanager.h"
#include "creategrid.h"


class VTKDisplayManager
{
public:
    VTKDisplayManager();

    void displayWireframe(vtkUnstructuredGrid* grid);
    void displaySolid(vtkUnstructuredGrid* grid);

    void displayWithScalarField(vtkUnstructuredGrid* grid, const std::string& scalarName, bool usePointData);
    bool addPointVectorMagnitude(vtkUnstructuredGrid* grid, const std::string& vectorName, const std::string& outputName);
    void addAxes();
    void setCameraView();
    void start();

public:
    void setInteractor(vtkRenderWindowInteractor* interactor);
    vtkGenericOpenGLRenderWindow* getRenderWindow() const { return m_renderWindow.Get(); }
    vtkRenderer* getRenderer() const { return m_renderer.Get(); }
    vtkRenderWindowInteractor* getRenderWindowInteractor() const { return m_renderWindowInteractor.Get(); }
private:
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkRenderWindowInteractor> m_renderWindowInteractor;

    vtkSmartPointer<vtkDataSetMapper> m_mapper;
    vtkSmartPointer<vtkActor> m_actor;
    vtkSmartPointer<vtkScalarBarActor> m_scalarBar;
    vtkSmartPointer<vtkLookupTable> m_lut;
    vtkSmartPointer<vtkOrientationMarkerWidget> m_axesWidget;
    bool m_actorAdded = false;
    bool m_scalarBarAdded = false;

    void addScalarBar(vtkSmartPointer<vtkDataSetMapper> mapper, const std::string& title);
    bool setActiveScalar(vtkUnstructuredGrid* grid, const std::string& name, bool usePointData);
};
#endif // VTKDISPLAY_H
