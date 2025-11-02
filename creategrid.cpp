#include "creategrid.h"
#include <vtkUnsignedCharArray.h>
#include <vtkIdTypeArray.h>
#include <vtkFloatArray.h>
#include <vtkCellType.h>
#include <unordered_map>

CreateVTKUnstucturedGrid::CreateVTKUnstucturedGrid(const readOdb& odb)
    : m_odb(odb)
{
    m_grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    // 预分配单元（用于传统 InsertNextCell 路径）；批量 SetCells 也可保留此估计
    m_grid->Allocate(static_cast<vtkIdType>(m_odb.m_elementsNum), 1);
    this->buildGeometry();
}

void CreateVTKUnstucturedGrid::buildGeometry()
{
    // 点坐标一次性提交（AOS: 3 组件）
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkFloatArray> coordsArray = vtkSmartPointer<vtkFloatArray>::New();
    coordsArray->SetNumberOfComponents(3);
    coordsArray->SetNumberOfTuples(static_cast<vtkIdType>(m_odb.m_nodesNum));
    for (std::size_t i = 0; i < m_odb.m_nodesNum; ++i) {
        const nodeCoord& nc = m_odb.m_nodesCoord[i];
        coordsArray->SetTuple3(static_cast<vtkIdType>(i), static_cast<float>(nc.x), static_cast<float>(nc.y), static_cast<float>(nc.z));
    }
    points->SetData(coordsArray);
    m_grid->SetPoints(points);

    // 批量设置单元（types + offsets + connectivity）
    vtkSmartPointer<vtkUnsignedCharArray> types = vtkSmartPointer<vtkUnsignedCharArray>::New();
    types->SetNumberOfComponents(1);
    types->SetNumberOfTuples(static_cast<vtkIdType>(m_odb.m_elementsNum));

    vtkSmartPointer<vtkIdTypeArray> offsets = vtkSmartPointer<vtkIdTypeArray>::New();
    offsets->SetNumberOfComponents(1);
    offsets->SetNumberOfTuples(static_cast<vtkIdType>(m_odb.m_elementsNum + 1));

    // 预先统计连通性总长度以优化分配
    vtkIdType totalConn = 0;
    for (std::size_t e = 0; e < m_odb.m_elementsNum; ++e) {
        totalConn += static_cast<vtkIdType>(m_odb.m_elementsConn[e].size());
    }
    vtkSmartPointer<vtkIdTypeArray> connectivity = vtkSmartPointer<vtkIdTypeArray>::New();
    connectivity->SetNumberOfComponents(1);
    connectivity->SetNumberOfTuples(totalConn);

    vtkIdType writePos = 0;
    for (std::size_t e = 0; e < m_odb.m_elementsNum; ++e) {
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
    offsets->SetValue(static_cast<vtkIdType>(m_odb.m_elementsNum), writePos);
    vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New();
    cells->SetData(offsets, connectivity);
    m_grid->SetCells(types, cells);
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

void CreateVTKUnstucturedGrid::addPointScalar(const std::string& name, const std::vector<float>& values)
{
    if (values.size() != m_odb.m_nodesNum) {
        throw std::runtime_error("addPointScalar: size mismatch with node count");
    }
    vtkSmartPointer<vtkFloatArray> arr = vtkSmartPointer<vtkFloatArray>::New();
    arr->SetName(name.c_str());
    arr->SetNumberOfComponents(1);
    arr->SetNumberOfTuples(static_cast<vtkIdType>(values.size()));
    for (std::size_t i = 0; i < values.size(); ++i) {
        arr->SetValue(static_cast<vtkIdType>(i), values[i]);
    }
    m_grid->GetPointData()->AddArray(arr);
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

bool CreateVTKUnstucturedGrid::addFieldData(FieldData& fieldData)
{
    if (fieldData.type == FieldType::DISPLACEMENT || fieldData.type == FieldType::ROTATION) {
        // 节点数据
        if (fieldData.nodeValues.empty()) {
            std::cerr << "[Warning] No node values found for field: " << fieldData.name << std::endl;
            return false;
        }

        int numComponents = fieldData.components;
        vtkSmartPointer<vtkFloatArray> arr = vtkSmartPointer<vtkFloatArray>::New();
        arr->SetName(fieldData.name.c_str());
        arr->SetNumberOfComponents(numComponents);
        arr->SetNumberOfTuples(static_cast<vtkIdType>(m_odb.m_nodesNum));

        for (std::size_t i = 0; i < m_odb.m_nodesNum; ++i) {
            if (fieldData.nodeValidFlags[i]) {
                const std::size_t base = i * numComponents;
                for (int comp = 0; comp < numComponents; ++comp) {
                    arr->SetComponent(static_cast<vtkIdType>(i), comp, fieldData.nodeValues[base + comp]);
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

        int numComponents = fieldData.components;
        vtkSmartPointer<vtkFloatArray> arr = vtkSmartPointer<vtkFloatArray>::New();
        arr->SetName(fieldData.name.c_str());
        arr->SetNumberOfComponents(numComponents);
        arr->SetNumberOfTuples(static_cast<vtkIdType>(m_odb.m_elementsNum));

        for (std::size_t i = 0; i < m_odb.m_elementsNum; ++i) {
            if (fieldData.elementValidFlags[i]) {
                const std::size_t base = i * numComponents;
                for (int comp = 0; comp < numComponents; ++comp) {
                    arr->SetComponent(static_cast<vtkIdType>(i), comp, fieldData.elementValues[base + comp]);
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
              << " with " << fieldData.components << " components." << std::endl;
    return true;
}

bool CreateVTKUnstucturedGrid::addDisplacementField(FieldData& displacementField, double scaleFactor)
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

    // 减少双驻留：位移场已推入 VTK，可丢弃缓存
    std::vector<float>().swap(displacementField.nodeValues);
    std::vector<uint8_t>().swap(displacementField.nodeValidFlags);

    return true;
}

bool CreateVTKUnstucturedGrid::addStressField(FieldData& stressField, const std::string& component)
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

            std::vector<float> componentValues;
            componentValues.reserve(m_odb.m_elementsNum);

            for (std::size_t i = 0; i < m_odb.m_elementsNum; ++i) {
                if (stressField.elementValidFlags[i]) {
                    const std::size_t base = i * stressField.components;
                    componentValues.push_back(stressField.elementValues[base + compIndex]);
                }
                else {
                    componentValues.push_back(0.0f);
                }
            }

            addCellScalar("S_" + component, componentValues);
            std::cout << "[Info] Added stress component: " << component << std::endl;
        }
    }

    // 计算von Mises应力
    calculateVonMisesStress(stressField);

    // 减少双驻留：应力场已推入 VTK，可丢弃缓存
    std::vector<float>().swap(stressField.elementValues);
    std::vector<uint8_t>().swap(stressField.elementValidFlags);

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
        if (stressField.elementValidFlags[i]) {
            const std::size_t base = i * stressField.components;
            const double s11 = stressField.elementValues[base + 0];
            const double s22 = stressField.elementValues[base + 1];
            const double s33 = stressField.elementValues[base + 2];
            const double s12 = stressField.elementValues[base + 3];
            const double s13 = stressField.elementValues[base + 4];
            const double s23 = stressField.elementValues[base + 5];

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
            point[0] += static_cast<double>(displacementField.nodeValues[base + 0]) * scaleFactor;
            point[1] += static_cast<double>(displacementField.nodeValues[base + 1]) * scaleFactor;
            point[2] += static_cast<double>(displacementField.nodeValues[base + 2]) * scaleFactor;
        } else if (displacementField.components >= 2) {
            point[0] += static_cast<double>(displacementField.nodeValues[base + 0]) * scaleFactor;
            point[1] += static_cast<double>(displacementField.nodeValues[base + 1]) * scaleFactor;
        }

        points->SetPoint(i, point);
    }

    points->Modified();
    std::cout << "[Info] Applied displacement to " << numPoints << " points." << std::endl;
}
