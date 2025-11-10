#ifndef ODBMANAGER_H
#define ODBMANAGER_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <iostream>
#include <odb_API.h>

#include "global.h"

struct nodeCoord {
    double x{0}, y{0}, z{0};
    nodeCoord() = default;
    nodeCoord(double _x, double _y, double _z) : x(_x), y(_y), z(_z) {}
};

enum class FieldType {
    DISPLACEMENT,
    ROTATION,
    STRESS,
    GENERIC
};

struct StepFrameInfo {
    std::string stepName;
    int frameIndex;
    double frameValue;
    std::string description;
};

struct FieldData {
    FieldType type;
    std::string name;
    std::string description;
    std::vector<std::string> componentLabels;
    int components{0};

    std::vector<float> values;       // 统一存储场数据 [globalIdx * components + comp]
    std::vector<uint8_t> validFlags; // 统一有效性标志 (0/1)
    bool isNodal{true};              // 标记是节点数据还是单元数据
    std::string unit;
};

struct InstanceInfo {
    std::string name;
    std::size_t nodeStartIndex{0};
    std::size_t nodeCount{0};
    std::size_t elementStartIndex{0};
    std::size_t elementCount{0};

    std::unordered_map<int, std::size_t> nodeLabelToIndex;
    std::unordered_map<int, std::size_t> elementLabelToIndex;
};

class readOdb {
public:
    readOdb(const char* odbFullname);
    ~readOdb();

	// 模型实例信息接口
    const std::vector<InstanceInfo>& getInstanceInfos() const { return m_instanceInfos; }

    //场数据接口
    bool readFieldOutput(const std::string& stepName, int frameIndex);
    bool readSingleField(const std::string& stepName, int frameIndex, const std::string& fieldName);
    const FieldData* getFieldData(const std::string& fieldName) const;
    bool hasFieldData(const std::string& fieldName) const;
    std::vector<std::pair<std::string, std::vector<std::string>>>
        listFieldNames(const std::string& stepName, int frameIndex) const;
    std::vector<std::string> getLoadedFieldNames() const;

	// 步与帧信息接口
    StepFrameInfo getCurrentStepFrame() const;
    std::vector<StepFrameInfo> getAvailableStepsFrames() const;

    // odb文件路径
    const std::string& getOdbPath() const;
    const std::string& getOdbBaseName() const;
    const std::string& getOdbFullName() const;
    
    void releaseGeometryCache();

public:
    std::size_t m_nodesNum{0};
    std::size_t m_elementsNum{0};
    std::vector<nodeCoord> m_nodesCoord;
    std::vector<std::vector<std::size_t>> m_elementsConn;
    std::vector<std::string> m_elementTypes;

private:
    readOdb(const readOdb&) = delete;
    readOdb& operator=(const readOdb&) = delete;

    void initializeGeometry();
    void readStepFrameInfo();
    std::size_t findGlobalIndex(const std::string& instanceName, int label, bool isNode);

	//场数据读取辅助函数
    bool readAllFields(const std::string& stepName, int frameIndex);
    void readDisplacementField(const odb_FieldOutput& fieldOutput);
    void readRotationField(const odb_FieldOutput& fieldOutput);
    void readStressField(const odb_FieldOutput& fieldOutput);
    void readGenericField(const odb_FieldOutput& fieldOutput, const std::string& name);
    void extractFieldData(const odb_FieldOutput& fieldOutput, FieldData& fieldData);

private:
    std::string m_odbFullName;
    std::string m_odbPath;
    std::string m_odbBaseName;
    odb_Odb* m_odb;

    std::vector<InstanceInfo> m_instanceInfos;

    std::vector<StepFrameInfo> m_availableStepsFrames;
    StepFrameInfo m_currentStepFrame;

    std::unordered_map<std::string, FieldData> m_fieldDataMap;
    bool m_hasFieldData{false};
};

#endif // ODBMANAGER_H
