#include "vtkDisplay.h"

VTKDisplayManager::VTKDisplayManager()
{
    // 创建渲染器和窗口
    renderer = vtkSmartPointer<vtkRenderer>::New();
    renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    // renderWindowInteractor = vtkSmartPointer<vtkRenderWindowInteractor>::New();

    // 设置窗口
    renderWindow->AddRenderer(renderer);

    // // 设置交互器
    // renderWindowInteractor->SetRenderWindow(renderWindow);

    // // 设置交互样式
    // vtkSmartPointer<vtkInteractorStyleTrackballCamera> style =
    //     vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
    // renderWindowInteractor->SetInteractorStyle(style);

    // 设置背景色
    renderer->SetBackground(0.1, 0.2, 0.3); // 深蓝色背景
}

// 显示网格（线框模式）
void VTKDisplayManager::displayWireframe(vtkUnstructuredGrid* grid)
{
    vtkSmartPointer<vtkDataSetMapper> mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    mapper->SetInputData(grid);

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetRepresentationToWireframe();
    actor->GetProperty()->SetColor(1.0, 1.0, 1.0); // 白色线框
    actor->GetProperty()->SetLineWidth(1.0);

    renderer->AddActor(actor);
    std::cout << "[Info] 添加线框显示" << std::endl;
}

// 显示网格（实体模式）
void VTKDisplayManager::displaySolid(vtkUnstructuredGrid* grid)
{
    vtkSmartPointer<vtkDataSetMapper> mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    mapper->SetInputData(grid);

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetRepresentationToSurface();
    actor->GetProperty()->SetColor(0.8, 0.8, 0.9); // 浅蓝色
    actor->GetProperty()->SetOpacity(0.8);

    renderer->AddActor(actor);
    std::cout << "[Info] 添加实体显示" << std::endl;
}

// 显示带标量场的网格（彩色云图）
void VTKDisplayManager::displayWithScalarField(vtkUnstructuredGrid* grid,
                                               const std::string& scalarName,
                                               bool usePointData = true)
{
    vtkSmartPointer<vtkDataSetMapper> mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    mapper->SetInputData(grid);

    // 设置标量场
    if (usePointData) {
        mapper->SetScalarModeToUsePointData();
        grid->GetPointData()->SetActiveScalars(scalarName.c_str());
    }
    else {
        mapper->SetScalarModeToUseCellData();
        grid->GetCellData()->SetActiveScalars(scalarName.c_str());
    }

    // 创建颜色映射
    vtkSmartPointer<vtkLookupTable> lut = vtkSmartPointer<vtkLookupTable>::New();
    lut->SetNumberOfTableValues(256);
    lut->SetRange(mapper->GetInput()->GetScalarRange());
    lut->SetHueRange(0.667, 0.0); // 蓝色到红色
    lut->Build();

    mapper->SetLookupTable(lut);
    mapper->SetScalarVisibility(true);

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);

    renderer->AddActor(actor);

    // 添加标量条
    addScalarBar(mapper, scalarName);

    std::cout << "[Info] 添加标量场显示: " << scalarName << std::endl;
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
    vtkSmartPointer<vtkScalarBarActor> scalarBar = vtkSmartPointer<vtkScalarBarActor>::New();
    scalarBar->SetLookupTable(mapper->GetLookupTable());
    scalarBar->SetTitle(title.c_str());
    scalarBar->SetNumberOfLabels(5);
    scalarBar->SetPosition(0.85, 0.1);
    scalarBar->SetWidth(0.1);
    scalarBar->SetHeight(0.8);

    scalarBar->GetTitleTextProperty()->SetColor(1.0, 1.0, 1.0);
    scalarBar->GetLabelTextProperty()->SetColor(1.0, 1.0, 1.0);

    renderer->AddActor2D(scalarBar);
}

void VTKDisplayManager::setInteractor(vtkRenderWindowInteractor* interactor)
{
    renderWindowInteractor = interactor;
    if (renderWindowInteractor)
    {
        renderWindowInteractor->SetRenderWindow(renderWindow);
    }
}
