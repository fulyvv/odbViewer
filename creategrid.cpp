#include "creategrid.h"

CreateVTKUnstucturedGrid::CreateVTKUnstucturedGrid(const readOdb& odb)
    : m_odb(odb)
{
    m_grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    this->buildGeometry();
}

void CreateVTKUnstucturedGrid::buildGeometry()
{
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    points->SetNumberOfPoints(static_cast<vtkIdType>(m_odb.m_nodesNum));

    for (std::size_t i = 0; i < m_odb.m_nodesNum; ++i) {
        const nodeCoord& nc = m_odb.m_nodesCoord[i];
        points->SetPoint(static_cast<vtkIdType>(i), nc.x, nc.y, nc.z);
    }
    m_grid->SetPoints(points);

    for (std::size_t e = 0; e < m_odb.m_elementsNum; ++e) {
        const std::vector<std::size_t>& conn = m_odb.m_elementsConn[e];
        const std::string& abaqusType = m_odb.m_elementTypes[e];

        int vtkCellType = abaqusToVTKCellType(abaqusType);
        //std::cout << "Element " << e + 1 << ": Abaqus type = " << abaqusType << ", VTK type = " << vtkCellType << "\n";
        if (vtkCellType < 0) {
            std::cerr << "[Warning] Unsupported element type \"" << abaqusType << "\" (element " << e + 1 << "). Skipped.\n";
            continue; //跳过不支持的单元
        }
        m_grid->InsertNextCell(vtkCellType, static_cast<vtkIdType>(conn.size()), reinterpret_cast<vtkIdType const*>(conn.data()));
    }
}
int CreateVTKUnstucturedGrid::abaqusToVTKCellType(const std::string& abaqusType)
{
    static const std::unordered_map<std::string, int> map = {
        {"C3D4",  VTK_TETRA},          // 10
        {"C3D6",  VTK_WEDGE},          // 13
        {"C3D8",  VTK_HEXAHEDRON},     // 12
        {"C3D10", VTK_QUADRATIC_TETRA}, // 24
        {"C3D15", VTK_QUADRATIC_WEDGE}, // 26
        {"C3D20", VTK_QUADRATIC_HEXAHEDRON}, // 25
        {"S3",    VTK_TRIANGLE},       // 5
        {"S4",    VTK_QUAD},           // 9
        {"S8",    VTK_QUADRATIC_QUAD}, // 23
        {"S9",    VTK_BIQUADRATIC_QUAD}, // 28
        {"B31",   VTK_LINE},           // 3
        {"R3D3",  VTK_TRIANGLE},       // 5 (2‑D shell)
        {"R3D4",  VTK_QUAD}            // 9 (2‑D shell)
    };

    for (const auto& kv : map) {
        if (abaqusType.find(kv.first) != std::string::npos) {
            return kv.second;
        }
    }
    std::cerr << "[Error] Element type \"" << abaqusType << "\" not supported by VTK converter.\n";
    return -1;
}

void CreateVTKUnstucturedGrid::addPointScalar(const std::string& name, const std::vector<double>& values)
{
    if (values.size() != m_odb.m_nodesNum) {
        throw std::runtime_error("addPointScalar: size mismatch with node count");
    }
    vtkSmartPointer<vtkDoubleArray> arr = vtkSmartPointer<vtkDoubleArray>::New();
    arr->SetName(name.c_str());
    arr->SetNumberOfComponents(1);
    arr->SetNumberOfTuples(static_cast<vtkIdType>(values.size()));
    for (std::size_t i = 0; i < values.size(); ++i) {
        arr->SetValue(static_cast<vtkIdType>(i), values[i]);
    }
    m_grid->GetPointData()->AddArray(arr);
}

void CreateVTKUnstucturedGrid::addCellScalar(const std::string& name, const std::vector<double>& values)
{
    if (values.size() != m_odb.m_elementsNum) {
        throw std::runtime_error("addCellScalar: size mismatch with element count");
    }
    vtkSmartPointer<vtkDoubleArray> arr = vtkSmartPointer<vtkDoubleArray>::New();
    arr->SetName(name.c_str());
    arr->SetNumberOfComponents(1);
    arr->SetNumberOfTuples(static_cast<vtkIdType>(values.size()));
    for (std::size_t i = 0; i < values.size(); ++i) {
        arr->SetValue(static_cast<vtkIdType>(i), values[i]);
    }
    m_grid->GetCellData()->AddArray(arr);
}

void CreateVTKUnstucturedGrid::addCellScalar(const std::string& name, const std::vector<int>& values)
{
    if (values.size() != m_odb.m_elementsNum) {
        throw std::runtime_error("addCellScalar: size mismatch with element count");
    }
    vtkSmartPointer<vtkIntArray> arr = vtkSmartPointer<vtkIntArray>::New();
    arr->SetName(name.c_str());
    arr->SetNumberOfComponents(1);
    arr->SetNumberOfTuples(static_cast<vtkIdType>(values.size()));
    for (std::size_t i = 0; i < values.size(); ++i) {
        arr->SetValue(static_cast<vtkIdType>(i), values[i]);
    }
    m_grid->GetCellData()->AddArray(arr);
}

bool CreateVTKUnstucturedGrid::writeToFile(const std::string& filename) const
{
    vtkSmartPointer<vtkXMLUnstructuredGridWriter> writer = vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
    writer->SetFileName(filename.c_str());
    writer->SetInputData(m_grid);

    try {
        writer->Write();
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[Error] Failed to write VTK file: " << e.what() << std::endl;
        return false;
    }
}

bool CreateVTKUnstucturedGrid::addFieldData(const FieldData& fieldData)
{
    if (fieldData.type == FieldType::DISPLACEMENT || fieldData.type == FieldType::ROTATION) {
        // 节点数据
        if (fieldData.nodeValues.empty()) {
            std::cerr << "[Warning] No node values found for field: " << fieldData.name << std::endl;
            return false;
        }

        int numComponents = fieldData.componentLabels.size();
        vtkSmartPointer<vtkDoubleArray> arr = vtkSmartPointer<vtkDoubleArray>::New();
        arr->SetName(fieldData.name.c_str());
        arr->SetNumberOfComponents(numComponents);
        arr->SetNumberOfTuples(static_cast<vtkIdType>(fieldData.nodeValues.size()));

        for (std::size_t i = 0; i < fieldData.nodeValues.size(); ++i) {
            if (fieldData.nodeValidFlags[i]) {
                for (int comp = 0; comp < numComponents; ++comp) {
                    arr->SetComponent(static_cast<vtkIdType>(i), comp, fieldData.nodeValues[i][comp]);
                }
            }
            else {
                // 无效节点设置为0
                for (int comp = 0; comp < numComponents; ++comp) {
                    arr->SetComponent(static_cast<vtkIdType>(i), comp, 0.0);
                }
            }
        }

        m_grid->GetPointData()->AddArray(arr);

    }
    else if (fieldData.type == FieldType::STRESS) {
        // 单元数据
        if (fieldData.elementValues.empty()) {
            std::cerr << "[Warning] No element values found for field: " << fieldData.name << std::endl;
            return false;
        }

        int numComponents = fieldData.componentLabels.size();
        vtkSmartPointer<vtkDoubleArray> arr = vtkSmartPointer<vtkDoubleArray>::New();
        arr->SetName(fieldData.name.c_str());
        arr->SetNumberOfComponents(numComponents);
        arr->SetNumberOfTuples(static_cast<vtkIdType>(fieldData.elementValues.size()));

        for (std::size_t i = 0; i < fieldData.elementValues.size(); ++i) {
            if (fieldData.elementValidFlags[i]) {
                for (int comp = 0; comp < numComponents; ++comp) {
                    arr->SetComponent(static_cast<vtkIdType>(i), comp, fieldData.elementValues[i][comp]);
                }
            }
            else {
                // 无效单元设置为0
                for (int comp = 0; comp < numComponents; ++comp) {
                    arr->SetComponent(static_cast<vtkIdType>(i), comp, 0.0);
                }
            }
        }

        m_grid->GetCellData()->AddArray(arr);
    }

    std::cout << "[Info] Added field data: " << fieldData.name
              << " with " << fieldData.componentLabels.size() << " components." << std::endl;
    return true;
}

bool CreateVTKUnstucturedGrid::addDisplacementField(const FieldData& displacementField, double scaleFactor)
{
    if (displacementField.type != FieldType::DISPLACEMENT) {
        std::cerr << "[Error] Field is not a displacement field." << std::endl;
        return false;
    }

    // 首先添加原始位移数据
    if (!addFieldData(displacementField)) {
        return false;
    }

    // 如果缩放因子不为0，应用位移到几何体
    if (scaleFactor != 0.0) {
        applyDisplacement(displacementField.nodeValues, scaleFactor);
        std::cout << "[Info] Applied displacement with scale factor: " << scaleFactor << std::endl;
    }

    return true;
}

bool CreateVTKUnstucturedGrid::addStressField(const FieldData& stressField, const std::string& component)
{
    if (stressField.type != FieldType::STRESS) {
        std::cerr << "[Error] Field is not a stress field." << std::endl;
        return false;
    }

    // 添加完整的应力张量
    if (!addFieldData(stressField)) {
        return false;
    }

    // 如果指定了特定组件，创建单独的标量场
    if (!component.empty() && component != "ALL") {
        auto it = std::find(stressField.componentLabels.begin(),
                            stressField.componentLabels.end(), component);
        if (it != stressField.componentLabels.end()) {
            int compIndex = static_cast<int>(std::distance(stressField.componentLabels.begin(), it));

            std::vector<double> componentValues;
            componentValues.reserve(stressField.elementValues.size());

            for (std::size_t i = 0; i < stressField.elementValues.size(); ++i) {
                if (stressField.elementValidFlags[i]) {
                    componentValues.push_back(stressField.elementValues[i][compIndex]);
                }
                else {
                    componentValues.push_back(0.0);
                }
            }

            addCellScalar("S_" + component, componentValues);
            std::cout << "[Info] Added stress component: " << component << std::endl;
        }
    }

    // 计算von Mises应力
    calculateVonMisesStress(stressField);

    return true;
}

void CreateVTKUnstucturedGrid::calculateVonMisesStress(const FieldData& stressField)
{
    if (stressField.componentLabels.size() < 6) {
        std::cerr << "[Warning] Insufficient stress components for von Mises calculation." << std::endl;
        return;
    }

    std::vector<double> vonMisesValues;
    vonMisesValues.reserve(stressField.elementValues.size());

    for (std::size_t i = 0; i < stressField.elementValues.size(); ++i) {
        if (stressField.elementValidFlags[i]) {
            const auto& stress = stressField.elementValues[i];

            // von Mises应力计算: sqrt(0.5 * ((s11-s22)^2 + (s22-s33)^2 + (s33-s11)^2 + 6*(s12^2 + s23^2 + s13^2)))
            double s11 = stress[0]; // S11
            double s22 = stress[1]; // S22
            double s33 = stress[2]; // S33
            double s12 = stress[3]; // S12
            double s13 = stress[4]; // S13
            double s23 = stress[5]; // S23

            double vonMises = std::sqrt(0.5 * (
                                            std::pow(s11 - s22, 2) +
                                            std::pow(s22 - s33, 2) +
                                            std::pow(s33 - s11, 2) +
                                            6.0 * (std::pow(s12, 2) + std::pow(s23, 2) + std::pow(s13, 2))
                                            ));

            vonMisesValues.push_back(vonMises);
        }
        else {
            vonMisesValues.push_back(0.0);
        }
    }

    addCellScalar("VonMises", vonMisesValues);
    std::cout << "[Info] Calculated von Mises stress." << std::endl;
}

void CreateVTKUnstucturedGrid::applyDisplacement(const std::vector<std::vector<double>>& displacements, double scaleFactor)
{
    vtkPoints* points = m_grid->GetPoints();
    if (!points) {
        std::cerr << "[Error] No points found in VTK grid." << std::endl;
        return;
    }

    vtkIdType numPoints = points->GetNumberOfPoints();
    if (static_cast<std::size_t>(numPoints) != displacements.size()) {
        std::cerr << "[Error] Displacement size mismatch with point count." << std::endl;
        return;
    }

    for (vtkIdType i = 0; i < numPoints; ++i) {
        double point[3];
        points->GetPoint(i, point);

        // 应用位移
        if (displacements[i].size() >= 3) {
            point[0] += displacements[i][0] * scaleFactor;
            point[1] += displacements[i][1] * scaleFactor;
            point[2] += displacements[i][2] * scaleFactor;
        }
        else if (displacements[i].size() >= 2) {
            // 2D情况
            point[0] += displacements[i][0] * scaleFactor;
            point[1] += displacements[i][1] * scaleFactor;
        }

        points->SetPoint(i, point);
    }

    points->Modified();
    std::cout << "[Info] Applied displacement to " << numPoints << " points." << std::endl;
}
