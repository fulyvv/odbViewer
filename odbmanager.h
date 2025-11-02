#ifndef ODBMANAGER_H
#define ODBMANAGER_H

#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>

#include <odb_API.h>

#include "global.h"

struct nodeCoord {
    double x{ 0 }, y{ 0 }, z{ 0 };
    nodeCoord() = default;
    nodeCoord(double _x, double _y, double _z) : x(_x), y(_y), z(_z) {}
};

enum class FieldType {
    DISPLACEMENT,
    ROTATION,
    STRESS,
};

struct StepFrameInfo {
    std::string stepName;
    int frameIndex;
    double frameValue;
    std::string description;

    StepFrameInfo() = default;
    StepFrameInfo(const std::string& name, int frame, double value, const std::string& desc = "")
        : stepName(name), frameIndex(frame), frameValue(value), description(desc) {
    }
};
// 场变量数据结构
struct FieldData {
    FieldType type;
    std::string name;           // 场变量名称 (U, UR, S)
    std::string description;    // 描述信息
    std::vector<std::string> componentLabels;  // 分量名称 (U1,U2,U3 或 S11,S22,S33,S12,S13,S23)

    // 节点数据 (位移、旋转)
    std::vector<std::vector<double>> nodeValues;  // [nodeIndex][componentIndex]
    std::vector<bool> nodeValidFlags;             // [nodeIndex] 节点数据有效性标志

    // 单元数据 (应力)
    std::vector<std::vector<double>> elementValues; // [elementIndex][componentIndex]
    std::vector<bool> elementValidFlags;            // [elementIndex] 单元数据有效性标志

    // 元数据
    std::string unit;          // 单位mm或者MPa

    FieldData() : type(FieldType::DISPLACEMENT) {}
};

//从odb读取数据
class readOdb
{
public:
    readOdb(const char* odbFullname);
    ~readOdb();

    nodeCoord getNodeCoordNodeLabel(const std::string& inst_name, int nodeLabel);

    nodeCoord getNodeCoordNodeLabel(std::size_t nodeLabel) const;

    nodeCoord getNodeCoordGlobal(std::size_t globaIdx) const;

    std::vector<std::size_t> getEelementConnElementLabel(std::string inst_name, int elementLabel);

    std::vector<std::size_t> getEelementConnElementLabel(std::size_t elementLabel) const;

    std::vector<std::size_t> getEelementConnGlobal(std::size_t globalIdx) const;

    bool readFieldOutput(const std::string& stepName, int frameIndex);
    std::vector<StepFrameInfo> getAvailableStepsFrames() const;
    const FieldData* getFieldData(const std::string& fieldName) const;
    bool hasFieldData(const std::string& fieldName) const;
    StepFrameInfo getCurrentStepFrame() const;

    // 公开的文件路径/名称访问器
    const std::string& getOdbPath() const;
    const std::string& getOdbBaseName() const;
    const std::string& getOdbFullName() const;


private:
    readOdb(const readOdb&) = delete;
    readOdb& operator=(const readOdb&) = delete;

    void readModelInfo();
    void constructMap();

    void readStepFrameInfo();
    void readDisplacementField(const odb_FieldOutput& fieldOutput);
    void readRotationField(const odb_FieldOutput& fieldOutput);
    void readStressField(const odb_FieldOutput& fieldOutput);

    // 数据处理辅助方法
    void extractFieldValues2(const odb_FieldOutput& fieldOutput, FieldData& fieldData);
    std::size_t mapFieldDataToGlobalIndices(int label, bool isNode);

private:
    std::string m_odbFullName;
    std::string m_odbPath;
    std::string m_odbBaseName;
    odb_Odb* m_odb;

public:
    size_t m_nodesNum; //节点数量
    size_t m_elementsNum; //单元数量
    std::vector<std::string> m_instanceNames; //模型实例的名称

    // 局部标签 → 全局 0‑based 索引（instance → {label : globalIdx})
    std::unordered_map<std::string, std::unordered_map<int, std::size_t>> m_nodeLocalToGlobalMap;
    std::unordered_map<std::string, std::unordered_map<int, std::size_t>> m_elementLocalToGlobalMap;

    // 全局坐标、连通性、单元类型（与全局索引 1:1 对应）
    std::vector<nodeCoord>                m_nodesCoord;          // size = m_nodesNum
    std::vector<std::vector<std::size_t>> m_elementsConn;        // size = m_elementsNum
    std::vector<std::string>              m_elementTypes;        // size = m_elementsNum

    // === 场变量相关成员变量 ===
    // 场变量相关数据
    std::vector<StepFrameInfo> m_availableStepsFrames;
    StepFrameInfo m_currentStepFrame;

    // 场变量数据存储 (按名称索引)
    std::unordered_map<std::string, FieldData> m_fieldDataMap;

    // 状态标志
    bool m_hasFieldData = false;
};

class OdbManager
{
public:
    OdbManager();
};

#endif // ODBMANAGER_H
