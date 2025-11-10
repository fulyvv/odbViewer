#include "creategrid.h"

CreateVTKUnstucturedGrid::CreateVTKUnstucturedGrid(const readOdb& odb)
    : m_odb(odb)
{
    m_grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    this->buildGeometry();
}

void CreateVTKUnstucturedGrid::buildGeometry()
{
    // 防御式检查：若 readOdb 的几何缓存被释放或不一致，按可用长度构建，避免越界
    std::size_t nodesCount = std::min(m_odb.m_nodesNum, m_odb.m_nodesCoord.size());
    std::size_t elementsCount = m_odb.m_elementsNum;
    elementsCount = std::min(elementsCount, m_odb.m_elementsConn.size());
    elementsCount = std::min(elementsCount, m_odb.m_elementTypes.size());
    if (nodesCount != m_odb.m_nodesNum || elementsCount != m_odb.m_elementsNum) {
        std::cerr << "[Warning] Geometry caches incomplete (possibly released). Using available sizes: nodes="
                  << nodesCount << ", elements=" << elementsCount << std::endl;
    }

    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkFloatArray> coordsArray = vtkSmartPointer<vtkFloatArray>::New();
    coordsArray->SetNumberOfComponents(3);
    coordsArray->SetNumberOfTuples(static_cast<vtkIdType>(nodesCount));
    for (std::size_t i = 0; i < nodesCount; ++i) {
        const nodeCoord& nc = m_odb.m_nodesCoord[i];
        coordsArray->SetTuple3(static_cast<vtkIdType>(i), static_cast<float>(nc.x), static_cast<float>(nc.y), static_cast<float>(nc.z));
    }
    points->SetData(coordsArray);
    m_grid->SetPoints(points);

    // 批量设置单元（types + offsets + connectivity）
    vtkSmartPointer<vtkUnsignedCharArray> types = vtkSmartPointer<vtkUnsignedCharArray>::New();
    types->SetNumberOfComponents(1);
    types->SetNumberOfTuples(static_cast<vtkIdType>(elementsCount));

    vtkSmartPointer<vtkIdTypeArray> offsets = vtkSmartPointer<vtkIdTypeArray>::New();
    offsets->SetNumberOfComponents(1);
    offsets->SetNumberOfTuples(static_cast<vtkIdType>(elementsCount + 1));

    // 预先统计连通性总长度以优化分配
    vtkIdType totalConn = 0;
    for (std::size_t e = 0; e < m_odb.m_elementsNum; ++e) {
        totalConn += static_cast<vtkIdType>(m_odb.m_elementsConn[e].size());
    }
    vtkSmartPointer<vtkIdTypeArray> connectivity = vtkSmartPointer<vtkIdTypeArray>::New();
    connectivity->SetNumberOfComponents(1);
    connectivity->SetNumberOfTuples(totalConn);

    vtkIdType writePos = 0;
    for (std::size_t e = 0; e < elementsCount; ++e) {
        const std::vector<std::size_t>& conn = m_odb.m_elementsConn[e];
        const std::string& abaqusType = m_odb.m_elementTypes[e];
        int vtkCellType = abaqusToVTKCellType(abaqusType);
        if (vtkCellType < 0) {
            std::cerr << "[Warning] Unsupported element type \"" << abaqusType << "\" (element " << e + 1 << "). Skipped.\n";
            types->SetValue(static_cast<vtkIdType>(e), VTK_EMPTY_CELL);
            offsets->SetValue(static_cast<vtkIdType>(e), writePos);
            continue;
        }
        types->SetValue(static_cast<vtkIdType>(e), static_cast<unsigned char>(vtkCellType));
        offsets->SetValue(static_cast<vtkIdType>(e), writePos);
        for (std::size_t j = 0; j < conn.size(); ++j) {
            connectivity->SetValue(writePos++, static_cast<vtkIdType>(conn[j]));
        }
    }
    // 末尾偏移，指向连接数组总长度
    offsets->SetValue(static_cast<vtkIdType>(elementsCount), writePos);
    vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New();
    cells->SetData(offsets, connectivity);
    m_grid->SetCells(types, cells);
}

int CreateVTKUnstucturedGrid::abaqusToVTKCellType(const std::string& abaqusType)
{
    static const std::unordered_map<std::string, int> map = {
        // 3D solids
        {"C3D4",  VTK_TETRA},
        {"C3D10", VTK_QUADRATIC_TETRA},
        {"C3D6",  VTK_WEDGE},
        {"C3D15", VTK_QUADRATIC_WEDGE},
        {"C3D8",  VTK_HEXAHEDRON},
        {"C3D8R", VTK_HEXAHEDRON},
        {"C3D8I", VTK_HEXAHEDRON},
        {"C3D20", VTK_QUADRATIC_HEXAHEDRON},
        {"C3D20R", VTK_QUADRATIC_HEXAHEDRON},

        // Shells
        {"S3",    VTK_TRIANGLE},
        {"S3R",   VTK_TRIANGLE},
        {"S4",    VTK_QUAD},
        {"S4R",   VTK_QUAD},
        {"S6",    VTK_QUADRATIC_TRIANGLE},
        {"S6R",   VTK_QUADRATIC_TRIANGLE},
        {"S8",    VTK_QUADRATIC_QUAD},
        {"S8R",   VTK_QUADRATIC_QUAD},
        {"S9",    VTK_BIQUADRATIC_QUAD},
        {"S9R",   VTK_BIQUADRATIC_QUAD},

        // 2D plane stress/strain and axisymmetric
        {"CPS3",  VTK_TRIANGLE}, {"CPE3",  VTK_TRIANGLE}, {"CAX3",  VTK_TRIANGLE},
        {"CPS4",  VTK_QUAD},     {"CPE4",  VTK_QUAD},     {"CAX4",  VTK_QUAD},
        {"CPS4R", VTK_QUAD},     {"CPE4R", VTK_QUAD},     {"CAX4R", VTK_QUAD},
        {"CPS6",  VTK_QUADRATIC_TRIANGLE}, {"CPE6",  VTK_QUADRATIC_TRIANGLE}, {"CAX6",  VTK_QUADRATIC_TRIANGLE},
        {"CPS8",  VTK_QUADRATIC_QUAD},     {"CPE8",  VTK_QUADRATIC_QUAD},     {"CAX8",  VTK_QUADRATIC_QUAD},
        {"CPS8R", VTK_QUADRATIC_QUAD},     {"CPE8R", VTK_QUADRATIC_QUAD},     {"CAX8R", VTK_QUADRATIC_QUAD},
        {"CPS9",  VTK_BIQUADRATIC_QUAD},   {"CPE9",  VTK_BIQUADRATIC_QUAD},   {"CAX9",  VTK_BIQUADRATIC_QUAD},

        // Membrane
        {"M3D3",  VTK_TRIANGLE},
        {"M3D4",  VTK_QUAD},
        {"M3D8",  VTK_QUADRATIC_QUAD},
        {"M3D9",  VTK_BIQUADRATIC_QUAD},

        // Rigid/analytical 2D
        {"R3D3",  VTK_TRIANGLE},
        {"R3D4",  VTK_QUAD},
        {"R3D8",  VTK_QUADRATIC_QUAD},
        {"R3D9",  VTK_BIQUADRATIC_QUAD},

        // Beams, trusses, pipes
        {"B31",   VTK_LINE},
        {"B32",   VTK_QUADRATIC_EDGE},
        {"T3D2",  VTK_LINE},
        {"T3D3",  VTK_QUADRATIC_EDGE},
        {"PIPE31",VTK_LINE},
        {"PIPE32",VTK_QUADRATIC_EDGE}
    };

    for (const auto& kv : map) {
        if (abaqusType.find(kv.first) != std::string::npos) {
            return kv.second;
        }
    }
    std::cerr << "[Error] Element type \"" << abaqusType << "\" not supported by VTK converter.\n";
    return -1;
}

void CreateVTKUnstucturedGrid::addCellScalar(const std::string& name, const std::vector<float>& values)
{
    if (values.size() != m_odb.m_elementsNum) {
        throw std::runtime_error("addCellScalar: size mismatch with element count");
    }
    vtkSmartPointer<vtkFloatArray> arr = vtkSmartPointer<vtkFloatArray>::New();
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
    int numComponents = fieldData.components;
    if (fieldData.isNodal) { // 点数据
        if (fieldData.values.empty()) {
            std::cerr << "[Warning] No node values found for field: " << fieldData.name << std::endl;
            return false;
        }
        auto arr = makeFloatArray(fieldData.name, numComponents,
                                  m_odb.m_nodesNum, fieldData.values, fieldData.validFlags);
        m_grid->GetPointData()->AddArray(arr);
    } else {
        // 单元数据
        if (fieldData.values.empty()) {
            std::cerr << "[Warning] No element values found for field: " << fieldData.name << std::endl;
            return false;
        }

        auto arr = makeFloatArray(fieldData.name, numComponents,
                                  m_odb.m_elementsNum, fieldData.values, fieldData.validFlags);
        m_grid->GetCellData()->AddArray(arr);
    }

    std::cout << "[Info] Added field data: " << fieldData.name
              << " with " << fieldData.components << " components." << std::endl;
    return true;
}

vtkSmartPointer<vtkFloatArray> CreateVTKUnstucturedGrid::makeFloatArray(const std::string& name,
                                                  int numComponents,
                                                  std::size_t tupleCount,
                                                  const std::vector<float>& values,
                                                  const std::vector<uint8_t>& validFlags)
{
    vtkSmartPointer<vtkFloatArray> arr = vtkSmartPointer<vtkFloatArray>::New();
    arr->SetName(name.c_str());
    arr->SetNumberOfComponents(numComponents);
    arr->SetNumberOfTuples(static_cast<vtkIdType>(tupleCount));

    const std::size_t expected = tupleCount * static_cast<std::size_t>(numComponents);
    if (values.size() < expected) {
        std::cerr << "[Warning] makeFloatArray: values size (" << values.size()
                  << ") < expected (" << expected << ") for " << name << std::endl;
    }

    for (std::size_t i = 0; i < tupleCount; ++i) {
        const bool valid = (i < validFlags.size() && validFlags[i]);
        const std::size_t base = i * static_cast<std::size_t>(numComponents);
        if (valid && (base + static_cast<std::size_t>(numComponents) <= values.size())) {
            for (int comp = 0; comp < numComponents; ++comp) {
                arr->SetComponent(static_cast<vtkIdType>(i), comp, values[base + static_cast<std::size_t>(comp)]);
            }
        } else {
            for (int comp = 0; comp < numComponents; ++comp) {
                arr->SetComponent(static_cast<vtkIdType>(i), comp, 0.0);
            }
        }
    }
    return arr;
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
        applyDisplacement(displacementField, scaleFactor);
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

    if (!addFieldData(stressField)) {
        return false;
    }

    // 如果指定了特定组件，创建单独的标量场
    if (!component.empty() && component != "ALL") {
        auto it = std::find(stressField.componentLabels.begin(),
                            stressField.componentLabels.end(), component);
        if (it != stressField.componentLabels.end()) {
            int compIndex = static_cast<int>(std::distance(stressField.componentLabels.begin(), it));

            std::vector<float> componentValues;
            componentValues.reserve(m_odb.m_elementsNum);

            for (std::size_t i = 0; i < m_odb.m_elementsNum; ++i) {
                if (i < stressField.validFlags.size() && stressField.validFlags[i]) {
                    const std::size_t base = i * stressField.components;
                    componentValues.push_back(stressField.values[base + compIndex]);
                } else {
                    componentValues.push_back(0.0f);
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
    if (stressField.components < 6) {
        std::cerr << "[Warning] Insufficient stress components for von Mises calculation." << std::endl;
        return;
    }
    std::vector<float> vonMisesValues;
    vonMisesValues.reserve(m_odb.m_elementsNum);

    for (std::size_t i = 0; i < m_odb.m_elementsNum; ++i) {
        if (i < stressField.validFlags.size() && stressField.validFlags[i]) {
            const std::size_t base = i * stressField.components;
            const double s11 = stressField.values[base + 0];
            const double s22 = stressField.values[base + 1];
            const double s33 = stressField.values[base + 2];
            const double s12 = stressField.values[base + 3];
            const double s13 = stressField.values[base + 4];
            const double s23 = stressField.values[base + 5];

            const double vm = std::sqrt(0.5 * (
                                            std::pow(s11 - s22, 2) +
                                            std::pow(s22 - s33, 2) +
                                            std::pow(s33 - s11, 2) +
                                            6.0 * (std::pow(s12, 2) + std::pow(s23, 2) + std::pow(s13, 2))
                                            ));
            vonMisesValues.push_back(static_cast<float>(vm));
        } else {
            vonMisesValues.push_back(0.0f);
        }
    }

    addCellScalar("VonMises", vonMisesValues);
    std::cout << "[Info] Calculated von Mises stress." << std::endl;
}

void CreateVTKUnstucturedGrid::applyDisplacement(const FieldData& displacementField, double scaleFactor)
{
    vtkPoints* points = m_grid->GetPoints();
    if (!points) {
        std::cerr << "[Error] No points found in VTK grid." << std::endl;
        return;
    }

    vtkIdType numPoints = points->GetNumberOfPoints();
    if (displacementField.components <= 0) {
        std::cerr << "[Error] Displacement components invalid." << std::endl;
        return;
    }

    for (vtkIdType i = 0; i < numPoints; ++i) {
        double point[3];
        points->GetPoint(i, point);

        const std::size_t base = static_cast<std::size_t>(i) * displacementField.components;
        if (displacementField.components >= 3) {
            point[0] += static_cast<double>(displacementField.values[base + 0]) * scaleFactor;
            point[1] += static_cast<double>(displacementField.values[base + 1]) * scaleFactor;
            point[2] += static_cast<double>(displacementField.values[base + 2]) * scaleFactor;
        } else if (displacementField.components >= 2) {
            point[0] += static_cast<double>(displacementField.values[base + 0]) * scaleFactor;
            point[1] += static_cast<double>(displacementField.values[base + 1]) * scaleFactor;
        }

        points->SetPoint(i, point);
    }

    points->Modified();
    std::cout << "[Info] Applied displacement to " << numPoints << " points." << std::endl;
}
