    COMPONENT(MEMORY, nsMemoryImpl::Create)
    COMPONENT(DEBUG,  nsDebugImpl::Create)
    COMPONENT(ERRORSERVICE, nsErrorService::Create)

    COMPONENT(CATEGORYMANAGER, nsCategoryManager::Create)

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
    COMPONENT(ARRAY, nsArray::XPCOMConstructor)
    COMPONENT(CONSOLESERVICE, nsConsoleServiceConstructor)
    COMPONENT(EXCEPTIONSERVICE, nsExceptionServiceConstructor)
    COMPONENT(ATOMSERVICE, nsAtomServiceConstructor)
    COMPONENT(OBSERVERSERVICE, nsObserverService::Create)
#ifdef MOZ_VISUAL_EVENT_TRACER
    COMPONENT(VISUALEVENTTRACER, VisualEventTracerConstructor)
#endif

    COMPONENT(TIMER, nsTimerImplConstructor)

#define COMPONENT_SUPPORTS(TYPE, Type)                                         \
  COMPONENT(SUPPORTS_##TYPE, nsSupports##Type##ImplConstructor)

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

    COMPONENT(VARIANT, nsVariantConstructor)
    COMPONENT(INTERFACEINFOMANAGER_SERVICE, nsXPTIInterfaceInfoManagerGetSingleton)

    COMPONENT(HASH_PROPERTY_BAG, nsHashPropertyBagConstructor)

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
