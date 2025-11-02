#include "vtkDisplay.h"

VTKDisplayManager::VTKDisplayManager()
{
    // 创建渲染器和窗口
    renderer = vtkSmartPointer<vtkRenderer>::New();
    renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();

    // 设置窗口
    renderWindow->AddRenderer(renderer);

    // 设置背景色
    renderer->SetBackground(0.1, 0.2, 0.3); // 深蓝色背景

    // 显示对象延迟创建以便按需复用
    m_mapper = nullptr;
    m_actor = nullptr;
    m_scalarBar = nullptr;
    m_lut = nullptr;
}

// 显示网格（线框模式）
void VTKDisplayManager::displayWireframe(vtkUnstructuredGrid* grid)
{
    if (!m_mapper)
        m_mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    if (!m_actor)
        m_actor = vtkSmartPointer<vtkActor>::New();

    m_mapper->SetInputData(grid);
    m_mapper->SetScalarVisibility(false); // 线框禁用标量显示

    m_actor->SetMapper(m_mapper);
    m_actor->GetProperty()->SetRepresentationToWireframe();
    m_actor->GetProperty()->SetColor(1.0, 1.0, 1.0); // 白色线框
    m_actor->GetProperty()->SetLineWidth(1.0);

    if (!m_actorAdded) {
        renderer->AddActor(m_actor);
        m_actorAdded = true;
    }

    // 若之前存在色标，线框模式不显示色标
    if (m_scalarBar && m_scalarBarAdded) {
        renderer->RemoveActor2D(m_scalarBar);
        m_scalarBarAdded = false;
    }

    std::cout << "[Info] 线框显示（复用 actor/mapper）" << std::endl;
}

// 显示网格（实体模式）
void VTKDisplayManager::displaySolid(vtkUnstructuredGrid* grid)
{
    if (!m_mapper)
        m_mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    if (!m_actor)
        m_actor = vtkSmartPointer<vtkActor>::New();

    m_mapper->SetInputData(grid);
    m_mapper->SetScalarVisibility(false); // 实体基础显示禁用标量

    m_actor->SetMapper(m_mapper);
    m_actor->GetProperty()->SetRepresentationToSurface();
    m_actor->GetProperty()->SetColor(0.8, 0.8, 0.9); // 浅蓝色
    m_actor->GetProperty()->SetOpacity(0.8);

    if (!m_actorAdded) {
        renderer->AddActor(m_actor);
        m_actorAdded = true;
    }

    // 去除旧色标
    if (m_scalarBar && m_scalarBarAdded) {
        renderer->RemoveActor2D(m_scalarBar);
        m_scalarBarAdded = false;
    }

    std::cout << "[Info] 实体显示（复用 actor/mapper）" << std::endl;
}

// 显示带标量场的网格（彩色云图）
void VTKDisplayManager::displayWithScalarField(vtkUnstructuredGrid* grid,
                                               const std::string& scalarName,
                                               bool usePointData = true)
{
    // 统一走 setActiveScalar 的闭合流程
    if (!setActiveScalar(grid, scalarName, usePointData)) {
        std::cerr << "[Error] 标量显示失败: " << scalarName << std::endl;
    }
}

// 添加坐标轴
void VTKDisplayManager::addAxes()
{
    vtkSmartPointer<vtkAxesActor> axes = vtkSmartPointer<vtkAxesActor>::New();
    axes->SetTotalLength(1.0, 1.0, 1.0);

    vtkSmartPointer<vtkOrientationMarkerWidget> widget =
        vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    widget->SetOrientationMarker(axes);
    widget->SetInteractor(renderWindowInteractor);
    widget->SetViewport(0.0, 0.0, 0.2, 0.2);
    widget->SetEnabled(1);
    widget->InteractiveOff();

    std::cout << "[Info] 添加坐标轴" << std::endl;
}

// 设置相机视角
void VTKDisplayManager::setCameraView()
{
    renderer->ResetCamera();

    vtkCamera* camera = renderer->GetActiveCamera();

    // 获取场景边界
    double bounds[6];
    renderer->ComputeVisiblePropBounds(bounds);

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

    // 设置相机位置，确保模型完整可见
    double distance = maxDim * 2.0; // 距离是最大尺寸的2倍
    camera->SetPosition(
        center[0] + distance * 0.7,
        center[1] + distance * 0.7,
        center[2] + distance * 0.7
        );
    camera->SetFocalPoint(center[0], center[1], center[2]);
    camera->SetViewUp(0, 0, 1);

    // 调整视角以确保模型完整显示
    camera->SetViewAngle(30.0); // 设置较小的视角

    // 再次重置相机以确保所有对象都在视野内
    renderer->ResetCamera();

    // 稍微放大一点以留出边距
    camera->Zoom(0.8);

    std::cout << "[Info] 相机设置完成 - 场景中心: ("
              << center[0] << ", " << center[1] << ", " << center[2]
              << "), 最大尺寸: " << maxDim << std::endl;
}

// 开始显示
void VTKDisplayManager::start()
{
    setCameraView();
    addAxes();
    renderWindow->Render();
    renderWindowInteractor->Start();
}

void VTKDisplayManager::addScalarBar(vtkSmartPointer<vtkDataSetMapper> mapper, const std::string& title)
{
    // 复用持久化的标量条
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
        renderer->AddActor2D(m_scalarBar);
        m_scalarBarAdded = true;
    }
}

void VTKDisplayManager::setInteractor(vtkRenderWindowInteractor* interactor)
{
    renderWindowInteractor = interactor;
    if (renderWindowInteractor)
    {
        renderWindowInteractor->SetRenderWindow(renderWindow);
    }
}

bool VTKDisplayManager::setActiveScalar(vtkUnstructuredGrid* grid, const std::string& name, bool usePointData)
{
    if (!grid) {
        std::cerr << "[Error] setActiveScalar: grid 为空" << std::endl;
        return false;
    }

    // 准备复用对象
    if (!m_mapper)
        m_mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    if (!m_actor)
        m_actor = vtkSmartPointer<vtkActor>::New();
    if (!m_lut)
        m_lut = vtkSmartPointer<vtkLookupTable>::New();

    m_mapper->SetInputData(grid);

    // 读取数组并做存在性/大小校验
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

    // 设置查找表范围（基于目标数组）
    double range[2] = {0.0, 1.0};
    arr->GetRange(range);
    m_lut->SetNumberOfTableValues(256);
    m_lut->SetRange(range);
    m_lut->SetHueRange(0.667, 0.0); // 蓝到红
    m_lut->Build();

    m_mapper->SetLookupTable(m_lut);
    m_mapper->SetScalarVisibility(true);

    m_actor->SetMapper(m_mapper);
    m_actor->GetProperty()->SetRepresentationToSurface();
    m_actor->GetProperty()->SetOpacity(1.0);

    if (!m_actorAdded) {
        renderer->AddActor(m_actor);
        m_actorAdded = true;
    }

    // 更新/添加色标
    addScalarBar(m_mapper, name);

    // 渲染并输出确认信息
    renderWindow->Render();
    std::cout << "[Info] 激活标量数组: " << name
              << ", 范围: [" << range[0] << ", " << range[1] << "]"
              << ", Actors: " << renderer->GetActors()->GetNumberOfItems()
              << ", Actors2D: " << renderer->GetActors2D()->GetNumberOfItems()
              << std::endl;

    return true;
}
