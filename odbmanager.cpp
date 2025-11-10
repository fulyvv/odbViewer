#include "odbmanager.h"

readOdb::readOdb(const char* odbFullname)
{
    odb_initializeAPI();
    odb_String odbFile = odb_String(odbFullname);
    m_odbFullName = std::string(odbFullname);
    m_odbPath = m_odbFullName.substr(0, m_odbFullName.find_last_of("/\\"));
    m_odbBaseName = m_odbFullName.substr(m_odbFullName.find_last_of("/\\") + 1);
    m_odb = &openOdb(odbFile.CStr(), true);
    readStepFrameInfo();
    initializeGeometry();
}

readOdb::~readOdb()
{
    m_odb->close();
}

void readOdb::initializeGeometry()
{
    m_nodesCoord.clear();
    m_elementsConn.clear();
    m_elementTypes.clear();
    m_instanceInfos.clear();

    std::size_t nodeGlobalIndex = 0;
    std::size_t elementGlobalIndex = 0;

    odb_Assembly& rootAssy = m_odb->rootAssembly();
    odb_InstanceRepositoryIT instIter(rootAssy.instances());
    for (instIter.first(); !instIter.isDone(); instIter.next()) {
        InstanceInfo info;
        info.name = instIter.currentKey().CStr();
        const odb_Instance& inst = instIter.currentValue();

        auto node_list = inst.nodes();
        auto element_list = inst.elements();

        info.nodeStartIndex = nodeGlobalIndex;

        // 节点：建立映射并填充坐标
        for (int i = 0; i < node_list.size(); ++i) {
            auto node = node_list[i];
            info.nodeLabelToIndex[node.label()] = nodeGlobalIndex;
            const float* const coord = node.coordinates();
            m_nodesCoord.emplace_back(coord[0], coord[1], coord[2]);
            ++nodeGlobalIndex;
        }
        info.nodeCount = static_cast<std::size_t>(node_list.size());

        // 单元：建立映射并填充连通性与类型
        info.elementStartIndex = elementGlobalIndex;
        for (int i = 0; i < element_list.size(); ++i) {
            auto element = element_list[i];
            info.elementLabelToIndex[element.label()] = elementGlobalIndex;

            int nNodes = 0;
            const int* const conn = element.connectivity(nNodes);
            std::vector<std::size_t> globalConn;
            globalConn.resize(nNodes);
            for (int j = 0; j < nNodes; ++j) {
                auto it = info.nodeLabelToIndex.find(conn[j]);
                if (it != info.nodeLabelToIndex.end()) {
                    globalConn[j] = it->second;
                } else {
                    std::cerr << "[Error] Node label " << conn[j] << " not found in instance " << info.name << std::endl;
                    globalConn[j] = 0; // 使用默认值
                }
            }
            m_elementsConn.emplace_back(std::move(globalConn));
            m_elementTypes.emplace_back(element.type().CStr());
            ++elementGlobalIndex;
        }
        info.elementCount = static_cast<std::size_t>(element_list.size());

        m_instanceInfos.emplace_back(std::move(info));
    }
    m_nodesNum = nodeGlobalIndex;
    m_elementsNum = elementGlobalIndex;
}

//读取所有可用的step和frame信息
void readOdb::readStepFrameInfo()
{
    m_availableStepsFrames.clear();
    m_hasFieldData = false;

    odb_StepRepositoryIT stepIter(m_odb->steps());
    for (stepIter.first(); !stepIter.isDone(); stepIter.next()) {
        const odb_Step& step = stepIter.currentValue();
        std::string stepName = step.name().cStr();

        const odb_SequenceFrame& allFramesInStep = step.frames();
        int numFrames = allFramesInStep.size();
        for (int frameIdx = 0; frameIdx < numFrames; ++frameIdx) {
            const odb_Frame& frame = allFramesInStep[frameIdx];
            StepFrameInfo info;
            info.stepName = stepName;
            info.frameIndex = frame.frameId();
            info.frameValue = frame.frameValue();
            info.description = frame.description().cStr();
            m_availableStepsFrames.push_back(info);
        }
    }
    std::cout << "[Info] Found " << m_availableStepsFrames.size() << " frames across all steps." << std::endl;
}

bool readOdb::readFieldOutput(const std::string& stepName, int frameIndex)
{
    return readAllFields(stepName, frameIndex);
}

void readOdb::readDisplacementField(const odb_FieldOutput& fieldOutput)
{
    FieldData fieldData;
    fieldData.type = FieldType::DISPLACEMENT;
    fieldData.name = "U";
    fieldData.description = fieldOutput.description().cStr();

    // 获取组件标签
    const odb_SequenceString& componentLabels = fieldOutput.componentLabels();
    for (int i = 0; i < componentLabels.size(); ++i) {
        fieldData.componentLabels.push_back(componentLabels[i].cStr());
    }
    fieldData.components = static_cast<int>(fieldOutput.componentLabels().size());
    extractFieldData(fieldOutput, fieldData);
    m_fieldDataMap["U"] = std::move(fieldData);

    std::cout << "[Info] Read displacement field with " << m_nodesNum
              << " nodes, " << m_fieldDataMap["U"].components << " components." << std::endl;
}

void readOdb::readRotationField(const odb_FieldOutput& fieldOutput)
{
    FieldData fieldData;
    fieldData.type = FieldType::ROTATION;
    fieldData.name = "UR";
    fieldData.description = fieldOutput.description().cStr();

    // 获取组件标签
    const odb_SequenceString& componentLabels = fieldOutput.componentLabels();
    for (int i = 0; i < componentLabels.size(); ++i) {
        fieldData.componentLabels.push_back(componentLabels[i].cStr());
    }
    fieldData.components = static_cast<int>(fieldOutput.componentLabels().size());
    extractFieldData(fieldOutput, fieldData);
    m_fieldDataMap["UR"] = std::move(fieldData);

    std::cout << "[Info] Read rotation field with " << m_nodesNum
              << " nodes, " << m_fieldDataMap["UR"].components << " components." << std::endl;
}

void readOdb::readStressField(const odb_FieldOutput& fieldOutput)
{
    FieldData fieldData;
    fieldData.type = FieldType::STRESS;
    fieldData.name = "S";
    fieldData.description = fieldOutput.description().cStr();

    // 获取组件标签
    const odb_SequenceString& componentLabels = fieldOutput.componentLabels();
    for (int i = 0; i < componentLabels.size(); ++i) {
        fieldData.componentLabels.push_back(componentLabels[i].cStr());
    }
    fieldData.components = static_cast<int>(fieldOutput.componentLabels().size());
    extractFieldData(fieldOutput, fieldData);
    m_fieldDataMap["S"] = std::move(fieldData);

    std::cout << "[Info] Read stress field with " << m_elementsNum
              << " elements, " << m_fieldDataMap["S"].components << " components." << std::endl;
}

void readOdb::extractFieldData(const odb_FieldOutput& fieldOutput, FieldData& fieldData)
{
    const odb_SequenceFieldBulkData& bulkDataBlocks = fieldOutput.bulkDataBlocks();
    int numBlocks = bulkDataBlocks.size();
    int numComponents = fieldData.components;

    // 根据场输出位置确定是节点数据还是单元数据
    bool isNodalData = false;
    odb_Enum::odb_ResultPositionEnum position = fieldOutput.locations()[0].position();
    if (position == odb_Enum::NODAL) {
        isNodalData = true;
    }
    else {
        //这里简单处理，默认非节点位置均为单元数据
        isNodalData = false;
    }
    fieldData.isNodal = isNodalData;

    if (isNodalData) {
        fieldData.values.assign(m_nodesNum * numComponents, 0.0f);
        fieldData.validFlags.assign(m_nodesNum, 0);

        for (int iblock = 0; iblock < numBlocks; iblock++) {
            const odb_FieldBulkData& bulkData = bulkDataBlocks[iblock];
            int numNodes = bulkData.length();        // 节点数量
            int numComp = bulkData.width();          // 组件数量
            float* data = bulkData.data();           // 数据数组
            int* nodeLabels = bulkData.nodeLabels(); // 节点标签数组

            int pos = 0;
            for (int node = 0; node < numNodes; node++) {
                int nodeLabel = nodeLabels[node];
                // 未区分实例时，遍历所有实例查找
                std::size_t globalIdx = findGlobalIndex("", nodeLabel, true);
                if (globalIdx < m_nodesNum) {
                    const std::size_t base = globalIdx * numComponents;
                    for (int comp = 0; comp < numComp; comp++) {
                        fieldData.values[base + comp] = data[pos++];
                    }
                    fieldData.validFlags[globalIdx] = 1;
                } else {
                    pos += numComp; // 跳过无效节点的数据
                }
            }
        }
    } 
    else {
        fieldData.values.assign(m_elementsNum * numComponents, 0.0f);
        fieldData.validFlags.assign(m_elementsNum, 0);

        for (int jblock = 0; jblock < numBlocks; jblock++) {
            const odb_FieldBulkData& bulkData = bulkDataBlocks[jblock];
            int numValues = bulkData.length();           // 总输出位置数
            int numComp = bulkData.width();              // 组件数量
            float* data = bulkData.data();               // 数据数组
            int nElems = bulkData.numberOfElements();    // 单元数量
            int* elementLabels = bulkData.elementLabels(); // 单元标签数组

            int numIP = (nElems > 0) ? numValues / nElems : 1; // 每单元积分点数
            int dataPosition = 0;

            for (int elem = 0; elem < nElems; elem++) {
                int elementLabel = elementLabels[elem];
                std::size_t globalIdx = findGlobalIndex("", elementLabel, false);
                if (globalIdx < m_elementsNum) {
                    const std::size_t base = globalIdx * numComponents;
                    // 取第一个积分点的值
                    for (int comp = 0; comp < numComp; comp++) {
                        fieldData.values[base + comp] = data[dataPosition + comp];
                    }
                    fieldData.validFlags[globalIdx] = 1;
                }
                dataPosition += numIP * numComp; // 跳过该单元其他积分点
            }
        }
    }

    std::cout << "[Info] Bulk data extraction completed. Processed " << numBlocks
              << " blocks with " << numComponents << " components." << std::endl;
}

std::size_t readOdb::findGlobalIndex(const std::string& instanceName, int label, bool isNode)
{
    // 如提供实例名，优先在该实例查找；否则遍历所有实例
    if (!instanceName.empty()) {
        for (const auto& info : m_instanceInfos) {
            if (info.name == instanceName) {
                const auto& mapRef = isNode ? info.nodeLabelToIndex : info.elementLabelToIndex;
                auto it = mapRef.find(label);
                if (it != mapRef.end()) return it->second;
                break;
            }
        }
    }
    for (const auto& info : m_instanceInfos) {
        const auto& mapRef = isNode ? info.nodeLabelToIndex : info.elementLabelToIndex;
        auto it = mapRef.find(label);
        if (it != mapRef.end()) return it->second;
    }
    return SIZE_MAX;
}

bool readOdb::readAllFields(const std::string& stepName, int frameIndex)
{
    const odb_String& stepNameOdbStr = odb_String(stepName.c_str());
    const odb_StepRepository& steps = m_odb->steps();
    if (!steps.isMember(stepNameOdbStr)) {
        std::cerr << "[Error] Step '" << stepName << "' not found." << std::endl;
        return false;
    }

    const odb_Step& step = steps.constGet(stepNameOdbStr);
    const odb_SequenceFrame& allFramesInStep = step.frames();

    // 查找指定的frame
    odb_Frame* targetFrame = nullptr;
    int numFrames = allFramesInStep.size();
    for (int i = 0; i < numFrames; ++i) {
        const odb_Frame& frame = allFramesInStep[i];
        if (frame.frameId() == frameIndex) {
            targetFrame = const_cast<odb_Frame*>(&frame);
            break;
        }
    }

    if (!targetFrame) {
        std::cerr << "[Error] Frame " << frameIndex << " not found in step '" << stepName << "'." << std::endl;
        return false;
    }

    m_currentStepFrame.stepName = stepName;
    m_currentStepFrame.frameIndex = frameIndex;
    m_currentStepFrame.frameValue = targetFrame->frameValue();
    m_currentStepFrame.description = targetFrame->description().cStr();
    m_fieldDataMap.clear();

    // 读取场输出数据
    const odb_FieldOutputRepository& fieldOutputs = targetFrame->fieldOutputs();
    if (fieldOutputs.isMember("U")) {
        readDisplacementField(fieldOutputs["U"]);
    }

    if (fieldOutputs.isMember("UR")) {
        readRotationField(fieldOutputs["UR"]);
    }

    if (fieldOutputs.isMember("S")) {
        readStressField(fieldOutputs["S"]);
    }

    m_hasFieldData = !m_fieldDataMap.empty();
    std::cout << "[Info] Successfully read field output for step '" << stepName
              << "', frame " << frameIndex << ". Found " << m_fieldDataMap.size()
              << " field variables." << std::endl;

    return true;
}

bool readOdb::readSingleField(const std::string& stepName, int frameIndex, const std::string& fieldName)
{
    const odb_String& stepNameOdbStr = odb_String(stepName.c_str());
    const odb_StepRepository& steps = m_odb->steps();
    if (!steps.isMember(stepNameOdbStr)) {
        std::cerr << "[Error] Step '" << stepName << "' not found." << std::endl;
        return false;
    }

    const odb_Step& step = steps.constGet(stepNameOdbStr);
    const odb_SequenceFrame& allFramesInStep = step.frames();

    odb_Frame* targetFrame = nullptr;
    int numFrames = allFramesInStep.size();
    for (int i = 0; i < numFrames; ++i) {
        const odb_Frame& frame = allFramesInStep[i];
        if (frame.frameId() == frameIndex) {
            targetFrame = const_cast<odb_Frame*>(&frame);
            break;
        }
    }

    if (!targetFrame) {
        std::cerr << "[Error] Frame " << frameIndex << " not found in step '" << stepName << "'." << std::endl;
        return false;
    }

    m_currentStepFrame.stepName = stepName;
    m_currentStepFrame.frameIndex = frameIndex;
    m_currentStepFrame.frameValue = targetFrame->frameValue();
    m_currentStepFrame.description = targetFrame->description().cStr();
    m_fieldDataMap.clear();

    const odb_FieldOutputRepository& fieldOutputs = targetFrame->fieldOutputs();
    if (!fieldOutputs.isMember(fieldName.c_str())) {
        std::cerr << "[Error] Field '" << fieldName << "' not found in frame." << std::endl;
        return false;
    }
    const odb_FieldOutput& F = fieldOutputs[fieldName.c_str()];

    if (fieldName == "U") {
        readDisplacementField(F);
    } else if (fieldName == "UR") {
        readRotationField(F);
    } else if (fieldName == "S") {
        readStressField(F);
    } else {
        readGenericField(F, fieldName);
    }

    m_hasFieldData = !m_fieldDataMap.empty();

    return true;
}

void readOdb::readGenericField(const odb_FieldOutput& fieldOutput, const std::string& name)
{
    FieldData fieldData;
    fieldData.type = FieldType::GENERIC;
    fieldData.name = name;
    fieldData.description = fieldOutput.description().cStr();

    // 组件标签
    const odb_SequenceString& componentLabels = fieldOutput.componentLabels();
    for (int i = 0; i < componentLabels.size(); ++i) {
        fieldData.componentLabels.push_back(componentLabels[i].cStr());
    }
    fieldData.components = static_cast<int>(fieldOutput.componentLabels().size());

    extractFieldData(fieldOutput, fieldData);
    m_fieldDataMap[name] = std::move(fieldData);
}

std::vector<StepFrameInfo> readOdb::getAvailableStepsFrames() const
{
    return m_availableStepsFrames;
}

const FieldData* readOdb::getFieldData(const std::string& fieldName) const
{
    auto it = m_fieldDataMap.find(fieldName);
    return (it != m_fieldDataMap.end()) ? &it->second : nullptr;
}

bool readOdb::hasFieldData(const std::string& fieldName) const
{
    return m_fieldDataMap.find(fieldName) != m_fieldDataMap.end();
}

StepFrameInfo readOdb::getCurrentStepFrame() const
{
    return m_currentStepFrame;
}

const std::string& readOdb::getOdbPath() const { return m_odbPath; }
const std::string& readOdb::getOdbBaseName() const { return m_odbBaseName; }
const std::string& readOdb::getOdbFullName() const { return m_odbFullName; }

void readOdb::releaseGeometryCache()
{
    std::vector<nodeCoord>().swap(m_nodesCoord);
    std::vector<std::vector<std::size_t>>().swap(m_elementsConn);
    std::vector<std::string>().swap(m_elementTypes);
    std::cout << "[Info] Released geometry caches: nodesCoord, elementsConn, elementTypes." << std::endl;
}

//字段名与该字段的组件标签列表（例如 {"U", {"U1","U2","U3"}} ）
std::vector<std::pair<std::string, std::vector<std::string>>>
readOdb::listFieldNames(const std::string& stepName, int frameIndex) const
{
    std::vector<std::pair<std::string, std::vector<std::string>>> result;

    const odb_String& stepNameOdbStr = odb_String(stepName.c_str());
    const odb_StepRepository& steps = m_odb->steps();
    if (!steps.isMember(stepNameOdbStr)) {
        std::cerr << "[Error] Step '" << stepName << "' not found." << std::endl;
        return result;
    }

    const odb_Step& step = steps.constGet(stepNameOdbStr);
    const odb_SequenceFrame& allFramesInStep = step.frames();

    const odb_Frame* targetFrame = nullptr;
    for (int i = 0; i < allFramesInStep.size(); ++i) {
        const odb_Frame& frame = allFramesInStep[i];
        if (frame.frameId() == frameIndex) {
            targetFrame = &frame;
            break;
        }
    }
    if (!targetFrame) {
        std::cerr << "[Error] Frame " << frameIndex << " not found in step '" << stepName << "'." << std::endl;
        return result;
    }

    const odb_FieldOutputRepository& fieldOutputs = targetFrame->fieldOutputs();
    try {
        odb_FieldOutputRepositoryIT foIter(fieldOutputs);
        for (foIter.first(); !foIter.isDone(); foIter.next()) {
            std::string fname = foIter.currentKey().CStr();
            const odb_FieldOutput& fo = foIter.currentValue();
            std::vector<std::string> comps;
            const odb_SequenceString& componentLabels = fo.componentLabels();
            for (int c = 0; c < componentLabels.size(); ++c) {
                comps.emplace_back(componentLabels[c].cStr());
            }
            result.emplace_back(std::move(fname), std::move(comps));
        }
    } catch (const std::exception& e) {
        std::cerr << "[Error] Failed to list field names: " << e.what() << std::endl;
    }

    // 若迭代方式不可用或无结果，回退到常用字段探测
    if (result.empty()) {
        const char* commonFields[] = {"U", "UR", "S"};
        for (const char* name : commonFields) {
            if (fieldOutputs.isMember(name)) {
                const odb_FieldOutput& fo = fieldOutputs[name];
                std::vector<std::string> comps;
                const odb_SequenceString& componentLabels = fo.componentLabels();
                for (int c = 0; c < componentLabels.size(); ++c) {
                    comps.emplace_back(componentLabels[c].cStr());
                }
                result.emplace_back(std::string(name), std::move(comps));
            }
        }
    }
    return result;
}

std::vector<std::string> readOdb::getLoadedFieldNames() const
{
    std::vector<std::string> names;
    names.reserve(m_fieldDataMap.size());
    for (const auto& kv : m_fieldDataMap) {
        names.emplace_back(kv.first);
    }
    return names;
}
