    COMPONENT_M(MEMORY, nsMemoryImpl::Create, Module::ALLOW_IN_GPU_PROCESS)
    COMPONENT_M(DEBUG,  nsDebugImpl::Create, Module::ALLOW_IN_GPU_PROCESS)
    COMPONENT(ERRORSERVICE, nsErrorService::Create)

    COMPONENT_M(CATEGORYMANAGER, nsCategoryManager::Create, Module::ALLOW_IN_GPU_PROCESS)

    COMPONENT(SCRIPTABLEINPUTSTREAM, nsScriptableInputStream::Create)
    COMPONENT(BINARYINPUTSTREAM, nsBinaryInputStreamConstructor)
    COMPONENT(BINARYOUTPUTSTREAM, nsBinaryOutputStreamConstructor)
    COMPONENT(STORAGESTREAM, nsStorageStreamConstructor)
    COMPONENT(VERSIONCOMPARATOR, nsVersionComparatorImplConstructor)
    COMPONENT(SCRIPTABLEBASE64ENCODER, nsScriptableBase64EncoderConstructor)
    COMPONENT(PIPE, nsPipeConstructor)

    COMPONENT(PROPERTIES, nsPropertiesConstructor)

    COMPONENT(PERSISTENTPROPERTIES, nsPersistentProperties::Create)

    COMPONENT(SUPPORTSARRAY, nsSupportsArray::Create)
    COMPONENT(ARRAY, nsArrayBase::XPCOMConstructor)
    COMPONENT(CONSOLESERVICE, nsConsoleServiceConstructor)
    COMPONENT(ATOMSERVICE, nsAtomServiceConstructor)
    COMPONENT_M(OBSERVERSERVICE, nsObserverService::Create, Module::ALLOW_IN_GPU_PROCESS)

    COMPONENT_M(TIMER, nsTimerConstructor, Module::ALLOW_IN_GPU_PROCESS)

#define COMPONENT_SUPPORTS(TYPE, Type)                                         \
  COMPONENT(SUPPORTS_##TYPE, nsSupports##Type##Constructor)

    COMPONENT_SUPPORTS(ID, ID)
    COMPONENT_SUPPORTS(STRING, String)
    COMPONENT_SUPPORTS(CSTRING, CString)
    COMPONENT_SUPPORTS(PRBOOL, PRBool)
    COMPONENT_SUPPORTS(PRUINT8, PRUint8)
    COMPONENT_SUPPORTS(PRUINT16, PRUint16)
    COMPONENT_SUPPORTS(PRUINT32, PRUint32)
    COMPONENT_SUPPORTS(PRUINT64, PRUint64)
    COMPONENT_SUPPORTS(PRTIME, PRTime)
    COMPONENT_SUPPORTS(CHAR, Char)
    COMPONENT_SUPPORTS(PRINT16, PRInt16)
    COMPONENT_SUPPORTS(PRINT32, PRInt32)
    COMPONENT_SUPPORTS(PRINT64, PRInt64)
    COMPONENT_SUPPORTS(FLOAT, Float)
    COMPONENT_SUPPORTS(DOUBLE, Double)
    COMPONENT_SUPPORTS(VOID, Void)
    COMPONENT_SUPPORTS(INTERFACE_POINTER, InterfacePointer)

#undef COMPONENT_SUPPORTS
    COMPONENT(LOCAL_FILE, nsLocalFile::nsLocalFileConstructor)
    COMPONENT(DIRECTORY_SERVICE, nsDirectoryService::Create)
    COMPONENT(PROCESS, nsProcessConstructor)
    COMPONENT(ENVIRONMENT, nsEnvironment::Create)

    COMPONENT(THREADMANAGER, nsThreadManagerGetSingleton)
    COMPONENT(THREADPOOL, nsThreadPoolConstructor)

    COMPONENT(STRINGINPUTSTREAM, nsStringInputStreamConstructor)
    COMPONENT(MULTIPLEXINPUTSTREAM, nsMultiplexInputStreamConstructor)

    COMPONENT(VARIANT, nsVariantCCConstructor)
    COMPONENT(INTERFACEINFOMANAGER_SERVICE, nsXPTIInterfaceInfoManagerGetSingleton)

    COMPONENT(HASH_PROPERTY_BAG, nsHashPropertyBagCCConstructor)

    COMPONENT(UUID_GENERATOR, nsUUIDGeneratorConstructor)

#if defined(XP_WIN)
    COMPONENT(WINDOWSREGKEY, nsWindowsRegKeyConstructor)
#endif

#if defined(MOZ_WIDGET_COCOA)
    COMPONENT(MACUTILSIMPL, nsMacUtilsImplConstructor)
#endif

    COMPONENT(SYSTEMINFO, nsSystemInfoConstructor)
    COMPONENT(MEMORY_REPORTER_MANAGER, nsMemoryReporterManagerConstructor)
    COMPONENT(MEMORY_INFO_DUMPER, nsMemoryInfoDumperConstructor)
    COMPONENT(IOUTIL, nsIOUtilConstructor)
    COMPONENT(CYCLE_COLLECTOR_LOGGER, nsCycleCollectorLoggerConstructor)
    COMPONENT(MESSAGE_LOOP, nsMessageLoopConstructor)
    COMPONENT(STATUS_REPORTER_MANAGER, nsStatusReporterManagerConstructor)
