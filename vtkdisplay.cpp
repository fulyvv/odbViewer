#include "vtkdisplay.h"
#include <algorithm>
#include <vtkArrayCalculator.h>
#include <vtkDataArray.h>
#include <vtkDataSet.h>

VTKDisplayManager::VTKDisplayManager()
{
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();

    m_renderWindow->AddRenderer(m_renderer);
    m_renderer->SetBackground(0.7, 0.7, 0.7);

    m_mapper = nullptr;
    m_actor = nullptr;
    m_scalarBar = nullptr;
    m_lut = nullptr;
}

void VTKDisplayManager::displayWireframe(vtkUnstructuredGrid* grid)
{
    if (!m_mapper)
        m_mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    if (!m_actor)
        m_actor = vtkSmartPointer<vtkActor>::New();

    m_mapper->SetInputData(grid);
    m_mapper->SetScalarVisibility(false);

    m_actor->SetMapper(m_mapper);
    m_actor->GetProperty()->SetRepresentationToWireframe();
    m_actor->GetProperty()->SetColor(1.0, 1.0, 1.0); // 白色线框
    m_actor->GetProperty()->SetLineWidth(1.0);

    if (!m_actorAdded) {
        m_renderer->AddActor(m_actor);
        m_actorAdded = true;
    }

    if (m_scalarBar && m_scalarBarAdded) {
        m_renderer->RemoveActor2D(m_scalarBar);
        m_scalarBarAdded = false;
    }
}

void VTKDisplayManager::displaySolid(vtkUnstructuredGrid* grid)
{
    if (!m_mapper)
        m_mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    if (!m_actor)
        m_actor = vtkSmartPointer<vtkActor>::New();

    m_mapper->SetInputData(grid);
    m_mapper->SetScalarVisibility(false);

    m_actor->SetMapper(m_mapper);
    m_actor->GetProperty()->SetRepresentationToSurface();
    m_actor->GetProperty()->SetColor(0.8, 0.8, 0.9); // 浅蓝色
    m_actor->GetProperty()->SetOpacity(0.8);

    if (!m_actorAdded) {
        m_renderer->AddActor(m_actor);
        m_actorAdded = true;
    }

    // 去除旧色标
    if (m_scalarBar && m_scalarBarAdded) {
        m_renderer->RemoveActor2D(m_scalarBar);
        m_scalarBarAdded = false;
    }
}

void VTKDisplayManager::displayWithScalarField(vtkUnstructuredGrid* grid,
                                               const std::string& scalarName,
                                               bool usePointData = true)
{
    if (!setActiveScalar(grid, scalarName, usePointData)) {
        std::cerr << "[Error] 标量显示失败: " << scalarName << std::endl;
    }
}

void VTKDisplayManager::addAxes()
{
    vtkSmartPointer<vtkAxesActor> axes = vtkSmartPointer<vtkAxesActor>::New();
    axes->SetTotalLength(1.0, 1.0, 1.0);

    m_axesWidget = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    m_axesWidget->SetOrientationMarker(axes);
    m_axesWidget->SetInteractor(m_renderWindowInteractor);
    m_axesWidget->SetViewport(0.0, 0.0, 0.2, 0.2);
    m_axesWidget->SetEnabled(1);
    m_axesWidget->InteractiveOff();
}

// 设置相机视角
void VTKDisplayManager::setCameraView()
{
    m_renderer->ResetCamera();

    vtkCamera* camera = m_renderer->GetActiveCamera();

    double bounds[6];
    m_renderer->ComputeVisiblePropBounds(bounds);

    // 计算场景中心和大小
    double center[3] = {
        (bounds[0] + bounds[1]) / 2.0,
        (bounds[2] + bounds[3]) / 2.0,
        (bounds[4] + bounds[5]) / 2.0
    };

    double maxDim = std::max({
        bounds[1] - bounds[0],
        bounds[3] - bounds[2],
        bounds[5] - bounds[4]
    });

    double distance = maxDim * 2.0; // 距离是最大尺寸的2倍
    camera->SetPosition(
        center[0] + distance * 0.7,
        center[1] + distance * 0.7,
        center[2] + distance * 0.7
        );
    camera->SetFocalPoint(center[0], center[1], center[2]);
    camera->SetViewUp(0, 0, 1);

    camera->SetViewAngle(30.0);
    m_renderer->ResetCamera();
    camera->Zoom(0.8);
}

void VTKDisplayManager::start()
{
    setCameraView();
    addAxes();
    m_renderWindow->Render();
    m_renderWindowInteractor->Start();
}

void VTKDisplayManager::addScalarBar(vtkSmartPointer<vtkDataSetMapper> mapper, const std::string& title)
{
    if (!m_scalarBar)
        m_scalarBar = vtkSmartPointer<vtkScalarBarActor>::New();

    m_scalarBar->SetLookupTable(mapper->GetLookupTable());
    m_scalarBar->SetTitle(title.c_str());
    m_scalarBar->SetNumberOfLabels(5);
    m_scalarBar->SetPosition(0.85, 0.1);
    m_scalarBar->SetWidth(0.1);
    m_scalarBar->SetHeight(0.8);

    m_scalarBar->GetTitleTextProperty()->SetColor(1.0, 1.0, 1.0);
    m_scalarBar->GetLabelTextProperty()->SetColor(1.0, 1.0, 1.0);

    if (!m_scalarBarAdded) {
        m_renderer->AddActor2D(m_scalarBar);
        m_scalarBarAdded = true;
    }
}

void VTKDisplayManager::setInteractor(vtkRenderWindowInteractor* interactor)
{
    m_renderWindowInteractor = interactor;
    if (m_renderWindowInteractor)
    {
        m_renderWindowInteractor->SetRenderWindow(m_renderWindow);
    }
}

bool VTKDisplayManager::setActiveScalar(vtkUnstructuredGrid* grid, const std::string& name, bool usePointData)
{
    if (!grid) {
        std::cerr << "[Error] setActiveScalar: grid 为空" << std::endl;
        return false;
    }

    if (!m_mapper)
        m_mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    if (!m_actor)
        m_actor = vtkSmartPointer<vtkActor>::New();
    if (!m_lut)
        m_lut = vtkSmartPointer<vtkLookupTable>::New();

    m_mapper->SetInputData(grid);

    // 读取数组并做校验
    vtkDataArray* arr = nullptr;
    if (usePointData) {
        arr = grid->GetPointData()->GetArray(name.c_str());
        if (!arr) {
            std::cerr << "[Error] 点数据未找到数组: " << name << std::endl;
            return false;
        }
        if (arr->GetNumberOfTuples() != grid->GetNumberOfPoints()) {
            std::cerr << "[Error] 点数组大小与点数不一致: " << name << std::endl;
            return false;
        }
        m_mapper->SetScalarModeToUsePointData();
        grid->GetPointData()->SetActiveScalars(name.c_str());
    } else {
        arr = grid->GetCellData()->GetArray(name.c_str());
        if (!arr) {
            std::cerr << "[Error] 单元数据未找到数组: " << name << std::endl;
            return false;
        }
        if (arr->GetNumberOfTuples() != grid->GetNumberOfCells()) {
            std::cerr << "[Error] 单元数组大小与单元数不一致: " << name << std::endl;
            return false;
        }
        m_mapper->SetScalarModeToUseCellData();
        grid->GetCellData()->SetActiveScalars(name.c_str());
    }

    double range[2] = {0.0, 1.0};
    arr->GetRange(range);
    m_lut->SetNumberOfTableValues(256);
    m_lut->SetRange(range);
    m_lut->SetHueRange(0.667, 0.0);
    m_lut->Build();

    m_mapper->SetLookupTable(m_lut);
    m_mapper->SetScalarVisibility(true);

    m_actor->SetMapper(m_mapper);
    m_actor->GetProperty()->SetRepresentationToSurface();
    m_actor->GetProperty()->SetOpacity(1.0);

    if (!m_actorAdded) {
        m_renderer->AddActor(m_actor);
        m_actorAdded = true;
    }

    // 更新/添加色标
    addScalarBar(m_mapper, name);

    // 渲染并输出确认信息
    m_renderWindow->Render();
    return true;
}

bool VTKDisplayManager::addPointVectorMagnitude(vtkUnstructuredGrid* grid, const std::string& vectorName, const std::string& outputName)
{
    if (!grid) {
        std::cerr << "[Error] addPointVectorMagnitude: grid 为空" << std::endl;
        return false;
    }
    if (!grid->GetPointData()) {
        std::cerr << "[Error] addPointVectorMagnitude: grid 无点数据" << std::endl;
        return false;
    }
    vtkDataArray* vec = grid->GetPointData()->GetArray(vectorName.c_str());
    if (!vec) {
        std::cerr << "[Error] addPointVectorMagnitude: 未找到点矢量数组 " << vectorName << std::endl;
        return false;
    }
    vtkNew<vtkArrayCalculator> calc;
    calc->SetInputData(grid);
    calc->SetAttributeTypeToPointData();
    calc->AddVectorArrayName(vectorName.c_str());
    const std::string expr = std::string("mag(") + vectorName + ")";
    calc->SetFunction(expr.c_str());
    calc->SetResultArrayName(outputName.c_str());
    calc->Update();

    vtkDataSet* outDs = vtkDataSet::SafeDownCast(calc->GetOutput());
    if (!outDs) {
        std::cerr << "[Error] addPointVectorMagnitude: 计算输出不是 vtkDataSet" << std::endl;
        return false;
    }
    vtkDataArray* mag = outDs->GetPointData()->GetArray(outputName.c_str());
    if (!mag) {
        std::cerr << "[Error] addPointVectorMagnitude: 计算失败，输出数组不存在 " << outputName << std::endl;
        return false;
    }
    vtkSmartPointer<vtkDataArray> magCopy;
    magCopy.TakeReference(mag->NewInstance());
    magCopy->DeepCopy(mag);
    magCopy->SetName(outputName.c_str());
    grid->GetPointData()->AddArray(magCopy);
    return true;
}
