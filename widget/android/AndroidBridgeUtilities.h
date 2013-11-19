// A collection of macros to simplify getting class and method references from Java.

#define initInit() jclass jClass

// note that this also sets jClass
#define getClassGlobalRef(cname) \
    (jClass = AndroidBridge::GetClassGlobalRef(jEnv, cname))

#define getField(fname, ftype) \
    AndroidBridge::GetFieldID(jEnv, jClass, fname, ftype)

#define getMethod(fname, ftype) \
    AndroidBridge::GetMethodID(jEnv, jClass, fname, ftype)

#define getStaticField(fname, ftype) \
    AndroidBridge::GetStaticFieldID(jEnv, jClass, fname, ftype)

#define getStaticMethod(fname, ftype) \
    AndroidBridge::GetStaticMethodID(jEnv, jClass, fname, ftype)
