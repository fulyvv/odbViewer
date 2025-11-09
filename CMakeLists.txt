cmake_minimum_required(VERSION 3.19)

# 如果支持，请为 MSVC 编译器启用热重载。
if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project(odbViewer LANGUAGES CXX)
add_compile_options(/EHsc)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

# ==================== VTK 配置 ====================
set(VTK_DIR "E:\\cppenv\\VTK\\VTK_install\\VTK-9.5.2\\Debug\\lib\\cmake\\vtk-9.5")
find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    FiltersSources
    RenderingCore
    RenderingOpenGL2
    InteractionStyle
    RenderingFreeType
    GUISupportQt
    IOXML
    RenderingAnnotation
)

# ==================== Abaqus ODB C++ API 配置 ====================
# include 目录
include_directories(
    "D:/SIMULIA2/EstProducts/2022/win_b64/code/include"
    "D:/SIMULIA2/EstProducts/2022"
)

# 库目录
link_directories("D:/SIMULIA2/EstProducts/2022/win_b64/code/lib")

# 需要链接的库
set(ABAQUS_LIBS
    ABQDMP_Core.lib
    ABQSMAAbuBasicUtils.lib
    ABQSMAAbuGeom.lib
    ABQSMAAspCommunications.lib
    ABQSMAAspDiagExtractor.lib
    ABQSMAAspExpKernelComm.lib
    ABQSMAAspSupport.lib
    ABQSMABasAlloc.lib
    ABQSMABasCoreUtils.lib
    ABQSMABasGenericsLib.lib
    ABQSMABasPrfTrkLib.lib
    ABQSMABasRtvUtility.lib
    ABQSMABasShared.lib
    ABQSMABasXmlDocument.lib
    ABQSMABasXmlParser.lib
    ABQSMABlaModule.lib
    ABQSMACseModules.lib
    ABQSMAEliBaseModule.lib
    ABQSMAEliLicenseModule.lib
    ABQSMAEliStaticModule.lib
    ABQSMAElkCore.lib
    ABQSMAFeoModules.lib
    ABQSMAFsmShared.lib
    ABQSMAISimInterface.lib
    ABQSMAISrvInterface.lib
    ABQSMAMsgCommModules.lib
    ABQSMAMsgModules.lib
    ABQSMAMtkApiMod.lib
    ABQSMAMtxCoreModule.lib
    ABQSMAObjSimObjectsMod.lib
    ABQSMAOdbApi.lib
    ABQSMAOdbAttrEO.lib
    ABQSMAOdbAttrEO2.lib
    ABQSMAOdbCalcK.lib
    ABQSMAOdbCore.lib
    ABQSMAOdbCoreGeom.lib
    ABQSMAOdbDdbOdb.lib
    ABQSMARfmInterface.lib
    ABQSMARomDiagEx.lib
    ABQSMASfsModule.lib
    ABQSMASglSharedLib.lib
    ABQSMASglSimXmlFndLib.lib
    ABQSMAShaDbIface-D.lib
    ABQSMAShaDbIface.lib
    ABQSMAShaShared-D.lib
    ABQSMAShaShared.lib
    ABQSMAShpCore.lib
    ABQSMASimBCompress.lib
    ABQSMASimBulkAPI.lib
    ABQSMASimContainers.lib
    ABQSMASimInterface.lib
    ABQSMASimManifestSubcomp.lib
    ABQSMASimPoolManager.lib
    ABQSMASimS2fSubcomp.lib
    ABQSMASimSerializerAPI.lib
    ABQSMASrvBasic.lib
    ABQSMASrvSimXmlConverters.lib
    ABQSMASspUmaCore.lib
    ABQSMAUsubsLib.lib
    ABQSMAUzlZlib.lib
    CATBBMagic.lib
    CATComBase.lib
    CATComDrvBB.lib
    CATComHTTPEndPoint.lib
    CATComServices.lib
    CATComSidl.lib
    CATComSidlFile.lib
    CATLic.lib
    CATLMjni.lib
    CATP2PBaseUUID.lib
    CATP2PCore.lib
    CATPLMDispatcherItf.lib
    CATPLMDispatcherSpecificItf.lib
    CATScriptEngine.lib
    CATSysAllocator.lib
    CATSysCATIAAI.lib
    CATSysCATIASF.lib
    CATSysCommunication.lib
    CATSysDbSettings.lib
    CATSysExternApp.lib
    CATSysMainThreadMQ.lib
    CATSysMotifDrv.lib
    CATSysMultiThreading.lib
    CATSysMultiThreadingSecured.lib
    CATSysPreview.lib
    CATSysProxy.lib
    CATSysRunBrw.lib
    CATSysTS.lib
    CATSysTSObjectModeler.lib
    CommunicationsUUID.lib
    CSICommandBinder.lib
    CSINodesLauncherSrc.lib
    CSIQueuingModule.lib
    CSIUtilities.lib
    DSYApplicationMainArch.lib
    DSYSysCnxExit.lib
    DSYSysDlg.lib
    DSYSysIRDriver.lib
    DSYSysIRManagerPlus.lib
    DSYSysIRMSysAdapter.lib
    DSYSysIRSendReport00.lib
    DSYSysIRSendReportCom.lib
    DSYSysIRSendReportItfPlugin.lib
    DSYSysProgressHandler.lib
    DSYSysTrayIcon.lib
    DSYSysWatchDogHelp.lib
    DSYSysWatchDogWERRegister.lib
    DSYSysWMIDriver.lib
    EKCrypto.lib
    EKPrivateArchive.lib
    EKSSL.lib
    ExperienceKernel.lib
    explicitB-D.lib
    explicitB.lib
    explicitU-D.lib
    explicitU-D_static.lib
    explicitU.lib
    explicitU_static.lib
    HTTPArch.lib
    InstArch.lib
    JS0BASEILB.lib
    JS0CRYPTEXIT.lib
    JS0DLK.lib
    JS0FM.lib
    JS0GROUP.lib
    JS0PCC.lib
    JS0SMT.lib
    lz4_static.lib
    mkl_core_dll.lib
    mkl_intel_lp64_dll.lib
    mkl_intel_thread_dll.lib
    mkl_rt.lib
    mkl_sequential_dll.lib
    msmpi.lib
    SecurityContext.lib
    SMAAbuCodeGen.lib
    SMAAspCodeGen.lib
    SMABasCodeGen.lib
    SMACylyntModule.lib
    SMAFeaBackbone.lib
    SMAFsmCodeGen.lib
    SMAPIRCylyntModule.lib
    SMAShaCodeGen_DP.lib
    SMAShaCodeGen_SP.lib
    SMASimCodeGen.lib
    standardB.lib
    standardU.lib
    standardU_static.lib
    StringUtilities.lib
    SysSqlite.lib
    SystemTSUUID.lib
    SystemUUID.lib
)

# ==================== Qt + VTK 配置 ====================

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

qt_standard_project_setup()

qt_add_executable(odbViewer
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    creategrid.h creategrid.cpp
    odbmanager.h odbmanager.cpp
    vtkdisplay.h vtkdisplay.cpp
    global.h
    toolicons.qrc
)

# 设置 C++ 标准
if(CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET odbViewer PROPERTY CXX_STANDARD 17)
endif()

set_source_files_properties(odbmanager.cpp PROPERTIES COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:MSVC>:/permissive>")

# Ensure INT64_MAX and related C limit macros are available across all TUs.
target_compile_definitions(odbViewer
    PRIVATE
        __STDC_LIMIT_MACROS
        __STDC_CONSTANT_MACROS
        INT64_MAX=9223372036854775807LL
)

target_link_libraries(odbViewer
    PRIVATE
        Qt::Core
        Qt::Widgets
        ${VTK_LIBRARIES}
        ${ABAQUS_LIBS}
)

vtk_module_autoinit(
    TARGETS odbViewer
    MODULES ${VTK_LIBRARIES}
)


include(GNUInstallDirs)

install(TARGETS odbViewer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET odbViewer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
