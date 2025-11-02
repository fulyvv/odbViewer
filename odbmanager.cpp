#include "odbmanager.h"

readOdb::readOdb(const char* odbFullname)
{
    odb_initializeAPI();
    odb_String odbFile = odb_String(odbFullname);
    m_odbFullName = std::string(odbFullname);
    m_odbPath = m_odbFullName.substr(0, m_odbFullName.find_last_of("/\\"));
    m_odbBaseName = m_odbFullName.substr(m_odbFullName.find_last_of("/\\") + 1);
    //m_odb = &openOdb(odbFile);
    m_odb = &openOdb(odbFile.CStr(), /*readOnly*/ true);
    // readModelInfo();
    readStepFrameInfo();
    constructMap();
}

readOdb::~readOdb()
{
    m_odb->close();
}


nodeCoord readOdb::getNodeCoordNodeLabel(const std::string& inst_name, int nodeLabel)
{
    std::size_t globalIdx = m_nodeLocalToGlobalMap[inst_name][nodeLabel];
    return m_nodesCoord[globalIdx];
}

nodeCoord readOdb::getNodeCoordNodeLabel(std::size_t nodeLabel) const
{
    for (const auto& [inst, umap] : m_nodeLocalToGlobalMap) {
        auto it = umap.find(nodeLabel);
        if (it != umap.end()) {
            return m_nodesCoord[it->second];
        }
    }
    // 如果找不到节点，抛出异常或返回默认值
    std::cerr << "[Warning] Node with label " << nodeLabel << " not found. Returning default coordinates (0,0,0)." << std::endl;
    return nodeCoord(0.0, 0.0, 0.0);
}

nodeCoord readOdb::getNodeCoordGlobal(std::size_t globaIdx) const
{
    return this->m_nodesCoord[globaIdx];
}

std::vector<std::size_t> readOdb::getEelementConnElementLabel(std::string inst_name, int elementLabel)
{
    int globalIdx = m_elementLocalToGlobalMap[inst_name][elementLabel];
    return m_elementsConn[globalIdx];
}

std::vector<std::size_t> readOdb::getEelementConnElementLabel(std::size_t elementLabel) const
{
    for (const auto& [inst, umap] : m_elementLocalToGlobalMap) {
        auto it = umap.find(elementLabel);
        if (it != umap.end()) {
            return m_elementsConn[it->second];
        }
    }
    // 如果找不到单元，返回空vector并输出警告
    std::cerr << "[Warning] Element with label " << elementLabel << " not found. Returning empty connectivity." << std::endl;
    return std::vector<std::size_t>();
}

std::vector<std::size_t> readOdb::getEelementConnGlobal(std::size_t globalIdx) const
{
    return m_elementsConn[globalIdx];
}

//读取模型的instances steps frames
//读取节点和单元总数
void readOdb::readModelInfo()
{
    //读取instance
    m_nodesNum = 0;
    m_elementsNum = 0;
    odb_Assembly& rootAssy = m_odb->rootAssembly();
    odb_InstanceRepositoryIT instIter(rootAssy.instances());
    for (instIter.first(); !instIter.isDone(); instIter.next()) {
        m_instanceNames.emplace_back(instIter.currentKey().CStr());
        auto& curInst = instIter.currentValue(); //获取当前实例
        auto nodeSize = curInst.nodes().size();
        m_nodesNum += nodeSize;
        auto elementSize = curInst.elements().size();
        m_elementsNum += elementSize;
    }

    //TODO : 获取step和frame的信息，后续读取场输出有用

}

void readOdb::constructMap()
{
    // 合并：一次遍历装配中的实例，同时统计总数、建立局部→全局映射、填充坐标与连通性

    // 清理旧数据
    m_nodesCoord.clear();
    m_elementsConn.clear();
    m_elementTypes.clear();
    m_nodeLocalToGlobalMap.clear();
    m_elementLocalToGlobalMap.clear();
    m_instanceNames.clear();

    std::size_t nodeGlobalIndex = 0;
    std::size_t elementGlobalIndex = 0;

    odb_Assembly& rootAssy = m_odb->rootAssembly();
    odb_InstanceRepositoryIT instIter(rootAssy.instances());
    for (instIter.first(); !instIter.isDone(); instIter.next()) {
        const std::string inst_name = instIter.currentKey().CStr();
        const odb_Instance& inst = instIter.currentValue();
        m_instanceNames.emplace_back(inst_name);

        auto node_list = inst.nodes();
        auto element_list = inst.elements();

        auto& nodeLocalToGlobal = m_nodeLocalToGlobalMap[inst_name];
        auto& elementLocalToGlobal = m_elementLocalToGlobalMap[inst_name];
        nodeLocalToGlobal.reserve(node_list.size());
        elementLocalToGlobal.reserve(element_list.size());

        // 节点：建立映射并填充坐标（push_back 连续内存）
        for (int i = 0; i < node_list.size(); ++i) {
            auto node = node_list[i];
            nodeLocalToGlobal[node.label()] = nodeGlobalIndex;
            const float* const coord = node.coordinates();
            m_nodesCoord.emplace_back(coord[0], coord[1], coord[2]);
            ++nodeGlobalIndex;
        }

        // 单元：建立映射并填充连通性与类型（使用已构建的节点映射）
        for (int i = 0; i < element_list.size(); ++i) {
            auto element = element_list[i];
            elementLocalToGlobal[element.label()] = elementGlobalIndex;

            int nNodes = 0;
            const int* const conn = element.connectivity(nNodes);
            std::vector<std::size_t> globalConn;
            globalConn.resize(nNodes);
            for (int j = 0; j < nNodes; ++j) {
                auto it = nodeLocalToGlobal.find(conn[j]);
                if (it != nodeLocalToGlobal.end()) {
                    globalConn[j] = it->second;
                } else {
                    std::cerr << "[Error] Node label " << conn[j] << " not found in instance " << inst_name << std::endl;
                    globalConn[j] = 0; // 使用默认值
                }
            }
            m_elementsConn.emplace_back(std::move(globalConn));
            m_elementTypes.emplace_back(element.type().CStr());
            ++elementGlobalIndex;
        }
    }

    // 更新总数
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

        //遍历所有frame
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
    //查找指定step和frame，读取场输出数据
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

    // 更新当前step/frame信息
    m_currentStepFrame.stepName = stepName;
    m_currentStepFrame.frameIndex = frameIndex;
    m_currentStepFrame.frameValue = targetFrame->frameValue();
    m_currentStepFrame.description = targetFrame->description().cStr();

    // 清除之前的场数据
    m_fieldDataMap.clear();

    // 读取场输出数据
    const odb_FieldOutputRepository& fieldOutputs = targetFrame->fieldOutputs();

    // 读取位移场 (U)
    if (fieldOutputs.isMember("U")) {
        readDisplacementField(fieldOutputs["U"]);
    }

    // 读取旋转场 (UR)
    if (fieldOutputs.isMember("UR")) {
        readRotationField(fieldOutputs["UR"]);
    }

    // 读取应力场 (S)
    if (fieldOutputs.isMember("S")) {
        readStressField(fieldOutputs["S"]);
    }

    m_hasFieldData = !m_fieldDataMap.empty();

    std::cout << "[Info] Successfully read field output for step '" << stepName
              << "', frame " << frameIndex << ". Found " << m_fieldDataMap.size()
              << " field variables." << std::endl;

    return true;
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

    // 提取场值
    extractFieldValues2(fieldOutput, fieldData);

    // 存储到map中
    m_fieldDataMap["U"] = fieldData;

    std::cout << "[Info] Read displacement field with " << fieldData.nodeValues.size()
              << " nodes, " << fieldData.componentLabels.size() << " components." << std::endl;
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

    // 提取场值
    extractFieldValues2(fieldOutput, fieldData);

    // 存储到map中
    m_fieldDataMap["UR"] = fieldData;

    std::cout << "[Info] Read rotation field with " << fieldData.nodeValues.size()
              << " nodes, " << fieldData.componentLabels.size() << " components." << std::endl;
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

    // 提取场值
    extractFieldValues2(fieldOutput, fieldData);

    // 存储到map中
    m_fieldDataMap["S"] = fieldData;

    std::cout << "[Info] Read stress field with " << fieldData.elementValues.size()
              << " elements, " << fieldData.componentLabels.size() << " components." << std::endl;
}

void readOdb::extractFieldValues2(const odb_FieldOutput& fieldOutput, FieldData& fieldData)
{
    try {
        // 获取bulk data blocks
        const odb_SequenceFieldBulkData& bulkDataBlocks = fieldOutput.bulkDataBlocks();
        int numBlocks = bulkDataBlocks.size();

        // 获取组件数量
        int numComponents = fieldOutput.componentLabels().size();

        // 根据场输出位置确定是节点数据还是单元数据
        odb_Enum::odb_ResultPositionEnum position = fieldOutput.locations()[0].position();
        bool isNodalData = (position == odb_Enum::NODAL);

        if (isNodalData) {
            // 节点数据处理
            fieldData.nodeValues.resize(m_nodesNum, std::vector<double>(numComponents, 0.0));
            fieldData.nodeValidFlags.resize(m_nodesNum, false);

            // 遍历所有bulk data blocks
            for (int iblock = 0; iblock < numBlocks; iblock++) {
                const odb_FieldBulkData& bulkData = bulkDataBlocks[iblock];

                int numNodes = bulkData.length();        // 节点数量
                int numComp = bulkData.width();          // 组件数量
                float* data = bulkData.data();           // 数据数组
                int* nodeLabels = bulkData.nodeLabels(); // 节点标签数组

                // 遍历当前block中的所有节点
                for (int node = 0, pos = 0; node < numNodes; node++) {
                    int nodeLabel = nodeLabels[node];

                    // 映射到全局索引
                    std::size_t globalIdx = mapFieldDataToGlobalIndices(nodeLabel, true);
                    if (globalIdx < m_nodesNum) {
                        // 复制数据到fieldData
                        for (int comp = 0; comp < numComp; comp++) {
                            fieldData.nodeValues[globalIdx][comp] = static_cast<double>(data[pos++]);
                        }
                        fieldData.nodeValidFlags[globalIdx] = true;
                    }
                    else {
                        // 跳过无效节点的数据
                        pos += numComp;
                    }
                }
            }
        }
        else {
            // 单元数据处理
            fieldData.elementValues.resize(m_elementsNum, std::vector<double>(numComponents, 0.0));
            fieldData.elementValidFlags.resize(m_elementsNum, false);

            // 遍历所有bulk data blocks
            for (int jblock = 0; jblock < numBlocks; jblock++) {
                const odb_FieldBulkData& bulkData = bulkDataBlocks[jblock];

                int numValues = bulkData.length();           // 总输出位置数
                int numComp = bulkData.width();              // 组件数量
                float* data = bulkData.data();               // 数据数组
                int nElems = bulkData.numberOfElements();    // 单元数量
                int* elementLabels = bulkData.elementLabels(); // 单元标签数组

                // 计算每个单元的积分点数量
                int numIP = (nElems > 0) ? numValues / nElems : 1;

                // 如果有积分点信息
                int* integrationPoints = nullptr;
                try {
                    integrationPoints = bulkData.integrationPoints();
                }
                catch (...) {
                    // 某些情况下可能没有积分点信息
                }

                // 遍历当前block中的所有单元
                for (int elem = 0, dataPosition = 0; elem < nElems; elem++) {
                    int elementLabel = elementLabels[elem];

                    // 映射到全局索引
                    std::size_t globalIdx = mapFieldDataToGlobalIndices(elementLabel, false);

                    if (globalIdx < m_elementsNum) {
                        // 对于单元数据，通常取第一个积分点的值或者平均值
                        // 这里我们取第一个积分点的值
                        for (int comp = 0; comp < numComp; comp++) {
                            fieldData.elementValues[globalIdx][comp] = static_cast<double>(data[dataPosition + comp]);
                        }
                        fieldData.elementValidFlags[globalIdx] = true;
                    }

                    // 跳过当前单元的所有积分点数据
                    dataPosition += numIP * numComp;
                }
            }
        }

        std::cout << "[Info] Bulk data extraction completed. Processed " << numBlocks
                  << " blocks with " << numComponents << " components." << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "[Error] Failed to extract field values using bulk data: " << e.what() << std::endl;
    }
}

std::size_t readOdb::mapFieldDataToGlobalIndices(int label, bool isNode)
{
    if (isNode) {
        // 节点标签到全局索引的映射
        for (const auto& pair : m_nodeLocalToGlobalMap) {
            for (const auto& nodePair : pair.second) {
                if (nodePair.first == label) {
                    return nodePair.second;
                }
            }
        }
    }
    else {
        // 单元标签到全局索引的映射
        for (const auto& pair : m_elementLocalToGlobalMap) {
            for (const auto& elemPair : pair.second) {
                if (elemPair.first == label) {
                    return elemPair.second;
                }
            }
        }
    }
    return SIZE_MAX; // 未找到时返回最大值
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

// 轻探测：列出某帧可用场变量及其分量标签（不读取 bulkData）
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
        // 迭代仓库键值以获取所有场及其分量标签
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


OdbManager::OdbManager() {}
