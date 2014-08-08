// JavaScript to Java bridge via the Java Native Interface
// Allows calling into Android SDK from JavaScript in Firefox Add-On.
// Released into the public domain.
//  C. Scott Ananian <cscott@laptop.org> (http://cscott.net)

// NOTE: All changes to this file should first be pushed to the repo at:
// https://github.com/cscott/skeleton-addon-fxandroid/tree/jni

var EXPORTED_SYMBOLS = ["JNI","android_log"];

Components.utils.import("resource://gre/modules/ctypes.jsm")

var liblog = ctypes.open('liblog.so');
var android_log = liblog.declare("__android_log_write",
                             ctypes.default_abi,
                             ctypes.int32_t,
                             ctypes.int32_t,
                             ctypes.char.ptr,
                             ctypes.char.ptr);

var libxul = ctypes.open('libxul.so');

var jenvptr = ctypes.voidptr_t;
var jclass = ctypes.voidptr_t;
var jobject = ctypes.voidptr_t;
var jvalue = ctypes.voidptr_t;
var jmethodid = ctypes.voidptr_t;
var jfieldid = ctypes.voidptr_t;

var jboolean = ctypes.uint8_t;
var jbyte = ctypes.int8_t;
var jchar = ctypes.uint16_t;
var jshort = ctypes.int16_t;
var jint = ctypes.int32_t;
var jlong = ctypes.int64_t;
var jfloat = ctypes.float32_t;
var jdouble = ctypes.float64_t;

var jsize = jint;
var jstring = jobject;
var jarray = jobject;
var jthrowable = jobject;

var JNINativeInterface = new ctypes.StructType(
    "JNINativeInterface",
    [{reserved0: ctypes.voidptr_t},
     {reserved1: ctypes.voidptr_t},
     {reserved2: ctypes.voidptr_t},
     {reserved3: ctypes.voidptr_t},
     {GetVersion: new ctypes.FunctionType(ctypes.default_abi,
                                          ctypes.int32_t,
                                          [ctypes.voidptr_t]).ptr},
     {DefineClass: new ctypes.FunctionType(ctypes.default_abi,
                                           jclass,
                                           [jenvptr, ctypes.char.ptr, jobject,
                                            jbyte.array(), jsize]).ptr},
     {FindClass: new ctypes.FunctionType(ctypes.default_abi,
                                         jclass,
                                         [jenvptr,
                                          ctypes.char.ptr]).ptr},
     {FromReflectedMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                   jmethodid,
                                                   [jenvptr, jobject]).ptr},
     {FromReflectedField: new ctypes.FunctionType(ctypes.default_abi,
                                                  jfieldid,
                                                  [jenvptr, jobject]).ptr},
     {ToReflectedMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                 jobject,
                                                 [jenvptr, jclass,
                                                  jmethodid]).ptr},
     {GetSuperclass: new ctypes.FunctionType(ctypes.default_abi,
                                             jclass, [jenvptr, jclass]).ptr},
     {IsAssignableFrom: new ctypes.FunctionType(ctypes.default_abi,
                                                jboolean,
                                                [jenvptr, jclass, jclass]).ptr},
     {ToReflectedField: new ctypes.FunctionType(ctypes.default_abi,
                                                jobject,
                                                [jenvptr, jclass,
                                                 jfieldid]).ptr},
     {Throw: new ctypes.FunctionType(ctypes.default_abi,
                                     jint, [jenvptr, jthrowable]).ptr},
     {ThrowNew: new ctypes.FunctionType(ctypes.default_abi,
                                        jint, [jenvptr, jclass,
                                               ctypes.char.ptr]).ptr},
     {ExceptionOccurred: new ctypes.FunctionType(ctypes.default_abi,
                                                 jthrowable, [jenvptr]).ptr},
     {ExceptionDescribe: new ctypes.FunctionType(ctypes.default_abi,
                                                 ctypes.void_t, [jenvptr]).ptr},
     {ExceptionClear: new ctypes.FunctionType(ctypes.default_abi,
                                              ctypes.void_t, [jenvptr]).ptr},
     {FatalError: new ctypes.FunctionType(ctypes.default_abi,
                                          ctypes.void_t,
                                          [jenvptr,
                                           ctypes.char.ptr]).ptr},
     {PushLocalFrame: new ctypes.FunctionType(ctypes.default_abi,
                                              jint,
                                              [jenvptr, jint]).ptr},
     {PopLocalFrame: new ctypes.FunctionType(ctypes.default_abi,
                                             jobject,
                                             [jenvptr, jobject]).ptr},
     {NewGlobalRef: new ctypes.FunctionType(ctypes.default_abi,
                                            jobject, [jenvptr, jobject]).ptr},
     {DeleteGlobalRef: new ctypes.FunctionType(ctypes.default_abi,
                                              ctypes.void_t,
                                              [jenvptr,
                                               jobject]).ptr},
     {DeleteLocalRef: new ctypes.FunctionType(ctypes.default_abi,
                                              ctypes.void_t,
                                              [jenvptr,
                                               jobject]).ptr},
     {IsSameObject: new ctypes.FunctionType(ctypes.default_abi,
                                            jboolean,
                                            [jenvptr, jobject, jobject]).ptr},
     {NewLocalRef: new ctypes.FunctionType(ctypes.default_abi,
                                            jobject, [jenvptr, jobject]).ptr},
     {EnsureLocalCapacity: new ctypes.FunctionType(ctypes.default_abi,
                                                   jint, [jenvptr, jint]).ptr},
     {AllocObject: new ctypes.FunctionType(ctypes.default_abi,
                                           jobject, [jenvptr, jclass]).ptr},
     {NewObject: new ctypes.FunctionType(ctypes.default_abi,
                                         jobject,
                                         [jenvptr,
                                          jclass,
                                          jmethodid,
                                          "..."]).ptr},
     {NewObjectV: ctypes.voidptr_t},
     {NewObjectA: ctypes.voidptr_t},
     {GetObjectClass: new ctypes.FunctionType(ctypes.default_abi,
                                              jclass,
                                              [jenvptr, jobject]).ptr},
     {IsInstanceOf: new ctypes.FunctionType(ctypes.default_abi,
                                            jboolean,
                                            [jenvptr, jobject, jclass]).ptr},
     {GetMethodID: new ctypes.FunctionType(ctypes.default_abi,
                                           jmethodid,
                                           [jenvptr,
                                            jclass,
                                            ctypes.char.ptr,
                                            ctypes.char.ptr]).ptr},
     {CallObjectMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                jobject,
                                                [jenvptr, jobject, jmethodid,
                                                 "..."]).ptr},
     {CallObjectMethodV: ctypes.voidptr_t},
     {CallObjectMethodA: ctypes.voidptr_t},
     {CallBooleanMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                 jboolean,
                                                 [jenvptr,
                                                  jobject,
                                                  jmethodid,
                                                  "..."]).ptr},
     {CallBooleanMethodV: ctypes.voidptr_t},
     {CallBooleanMethodA: ctypes.voidptr_t},
     {CallByteMethod: new ctypes.FunctionType(ctypes.default_abi,
                                              jbyte,
                                              [jenvptr,
                                               jobject,
                                               jmethodid,
                                               "..."]).ptr},
     {CallByteMethodV: ctypes.voidptr_t},
     {CallByteMethodA: ctypes.voidptr_t},
     {CallCharMethod: new ctypes.FunctionType(ctypes.default_abi,
                                              jchar,
                                              [jenvptr,
                                               jobject,
                                               jmethodid,
                                               "..."]).ptr},
     {CallCharMethodV: ctypes.voidptr_t},
     {CallCharMethodA: ctypes.voidptr_t},
     {CallShortMethod: new ctypes.FunctionType(ctypes.default_abi,
                                               jshort,
                                               [jenvptr,
                                                jobject,
                                                jmethodid,
                                                "..."]).ptr},
     {CallShortMethodV: ctypes.voidptr_t},
     {CallShortMethodA: ctypes.voidptr_t},
     {CallIntMethod: new ctypes.FunctionType(ctypes.default_abi,
                                             jint,
                                             [jenvptr,
                                              jobject,
                                              jmethodid,
                                              "..."]).ptr},
     {CallIntMethodV: ctypes.voidptr_t},
     {CallIntMethodA: ctypes.voidptr_t},
     {CallLongMethod: new ctypes.FunctionType(ctypes.default_abi,
                                              jlong,
                                              [jenvptr,
                                               jobject,
                                               jmethodid,
                                               "..."]).ptr},
     {CallLongMethodV: ctypes.voidptr_t},
     {CallLongMethodA: ctypes.voidptr_t},
     {CallFloatMethod: new ctypes.FunctionType(ctypes.default_abi,
                                               jfloat,
                                               [jenvptr,
                                                jobject,
                                                jmethodid,
                                                "..."]).ptr},
     {CallFloatMethodV: ctypes.voidptr_t},
     {CallFloatMethodA: ctypes.voidptr_t},
     {CallDoubleMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                jdouble,
                                                [jenvptr,
                                                 jobject,
                                                 jmethodid,
                                                 "..."]).ptr},
     {CallDoubleMethodV: ctypes.voidptr_t},
     {CallDoubleMethodA: ctypes.voidptr_t},
     {CallVoidMethod: new ctypes.FunctionType(ctypes.default_abi,
                                              ctypes.void_t,
                                              [jenvptr,
                                               jobject,
                                               jmethodid,
                                               "..."]).ptr},
     {CallVoidMethodV: ctypes.voidptr_t},
     {CallVoidMethodA: ctypes.voidptr_t},
     {CallNonvirtualObjectMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                          jobject,
                                                          [jenvptr, jobject,
                                                           jclass, jmethodid,
                                                           "..."]).ptr},
     {CallNonvirtualObjectMethodV: ctypes.voidptr_t},
     {CallNonvirtualObjectMethodA: ctypes.voidptr_t},
     {CallNonvirtualBooleanMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                           jboolean,
                                                           [jenvptr, jobject,
                                                            jclass, jmethodid,
                                                            "..."]).ptr},
     {CallNonvirtualBooleanMethodV: ctypes.voidptr_t},
     {CallNonvirtualBooleanMethodA: ctypes.voidptr_t},
     {CallNonvirtualByteMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                        jbyte,
                                                        [jenvptr, jobject,
                                                         jclass, jmethodid,
                                                         "..."]).ptr},
     {CallNonvirtualByteMethodV: ctypes.voidptr_t},
     {CallNonvirtualByteMethodA: ctypes.voidptr_t},
     {CallNonvirtualCharMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                        jchar,
                                                        [jenvptr, jobject,
                                                         jclass, jmethodid,
                                                         "..."]).ptr},
     {CallNonvirtualCharMethodV: ctypes.voidptr_t},
     {CallNonvirtualCharMethodA: ctypes.voidptr_t},
     {CallNonvirtualShortMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                         jshort,
                                                         [jenvptr, jobject,
                                                          jclass, jmethodid,
                                                          "..."]).ptr},
     {CallNonvirtualShortMethodV: ctypes.voidptr_t},
     {CallNonvirtualShortMethodA: ctypes.voidptr_t},
     {CallNonvirtualIntMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                       jint,
                                                       [jenvptr, jobject,
                                                        jclass, jmethodid,
                                                        "..."]).ptr},
     {CallNonvirtualIntMethodV: ctypes.voidptr_t},
     {CallNonvirtualIntMethodA: ctypes.voidptr_t},
     {CallNonvirtualLongMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                        jlong,
                                                        [jenvptr, jobject,
                                                         jclass, jmethodid,
                                                         "..."]).ptr},
     {CallNonvirtualLongMethodV: ctypes.voidptr_t},
     {CallNonvirtualLongMethodA: ctypes.voidptr_t},
     {CallNonvirtualFloatMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                         jfloat,
                                                         [jenvptr, jobject,
                                                          jclass, jmethodid,
                                                          "..."]).ptr},
     {CallNonvirtualFloatMethodV: ctypes.voidptr_t},
     {CallNonvirtualFloatMethodA: ctypes.voidptr_t},
     {CallNonvirtualDoubleMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                          jdouble,
                                                          [jenvptr, jobject,
                                                           jclass, jmethodid,
                                                           "..."]).ptr},
     {CallNonvirtualDoubleMethodV: ctypes.voidptr_t},
     {CallNonvirtualDoubleMethodA: ctypes.voidptr_t},
     {CallNonvirtualVoidMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                        ctypes.void_t,
                                                        [jenvptr, jobject,
                                                         jclass, jmethodid,
                                                         "..."]).ptr},
     {CallNonvirtualVoidMethodV: ctypes.voidptr_t},
     {CallNonvirtualVoidMethodA: ctypes.voidptr_t},
     {GetFieldID: new ctypes.FunctionType(ctypes.default_abi,
                                          jfieldid,
                                          [jenvptr, jclass,
                                           ctypes.char.ptr,
                                           ctypes.char.ptr]).ptr},
     {GetObjectField: new ctypes.FunctionType(ctypes.default_abi,
                                              jobject,
                                              [jenvptr, jobject,
                                               jfieldid]).ptr},
     {GetBooleanField: new ctypes.FunctionType(ctypes.default_abi,
                                               jboolean,
                                               [jenvptr, jobject,
                                                jfieldid]).ptr},
     {GetByteField: new ctypes.FunctionType(ctypes.default_abi,
                                            jbyte,
                                            [jenvptr, jobject,
                                             jfieldid]).ptr},
     {GetCharField: new ctypes.FunctionType(ctypes.default_abi,
                                            jchar,
                                            [jenvptr, jobject,
                                             jfieldid]).ptr},
     {GetShortField: new ctypes.FunctionType(ctypes.default_abi,
                                             jshort,
                                             [jenvptr, jobject,
                                              jfieldid]).ptr},
     {GetIntField: new ctypes.FunctionType(ctypes.default_abi,
                                           jint,
                                           [jenvptr, jobject,
                                            jfieldid]).ptr},
     {GetLongField: new ctypes.FunctionType(ctypes.default_abi,
                                            jlong,
                                            [jenvptr, jobject,
                                             jfieldid]).ptr},
     {GetFloatField: new ctypes.FunctionType(ctypes.default_abi,
                                             jfloat,
                                             [jenvptr, jobject,
                                              jfieldid]).ptr},
     {GetDoubleField: new ctypes.FunctionType(ctypes.default_abi,
                                              jdouble,
                                              [jenvptr, jobject,
                                               jfieldid]).ptr},
     {SetObjectField: new ctypes.FunctionType(ctypes.default_abi,
                                              ctypes.void_t,
                                              [jenvptr, jobject,
                                               jfieldid, jobject]).ptr},
     {SetBooleanField: new ctypes.FunctionType(ctypes.default_abi,
                                              ctypes.void_t,
                                              [jenvptr, jobject,
                                               jfieldid, jboolean]).ptr},
     {SetByteField: new ctypes.FunctionType(ctypes.default_abi,
                                              ctypes.void_t,
                                              [jenvptr, jobject,
                                               jfieldid, jbyte]).ptr},
     {SetCharField: new ctypes.FunctionType(ctypes.default_abi,
                                              ctypes.void_t,
                                              [jenvptr, jobject,
                                               jfieldid, jchar]).ptr},
     {SetShortField: new ctypes.FunctionType(ctypes.default_abi,
                                              ctypes.void_t,
                                              [jenvptr, jobject,
                                               jfieldid, jshort]).ptr},
     {SetIntField: new ctypes.FunctionType(ctypes.default_abi,
                                              ctypes.void_t,
                                              [jenvptr, jobject,
                                               jfieldid, jint]).ptr},
     {SetLongField: new ctypes.FunctionType(ctypes.default_abi,
                                              ctypes.void_t,
                                              [jenvptr, jobject,
                                               jfieldid, jlong]).ptr},
     {SetFloatField: new ctypes.FunctionType(ctypes.default_abi,
                                              ctypes.void_t,
                                              [jenvptr, jobject,
                                               jfieldid, jfloat]).ptr},
     {SetDoubleField: new ctypes.FunctionType(ctypes.default_abi,
                                              ctypes.void_t,
                                              [jenvptr, jobject,
                                               jfieldid, jdouble]).ptr},
     {GetStaticMethodID: new ctypes.FunctionType(ctypes.default_abi,
                                           jmethodid,
                                           [jenvptr,
                                            jclass,
                                            ctypes.char.ptr,
                                            ctypes.char.ptr]).ptr},
     {CallStaticObjectMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                      jobject,
                                                      [jenvptr, jclass,
                                                       jmethodid,
                                                       "..."]).ptr},
     {CallStaticObjectMethodV: ctypes.voidptr_t},
     {CallStaticObjectMethodA: ctypes.voidptr_t},
     {CallStaticBooleanMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                       jboolean,
                                                       [jenvptr, jclass,
                                                        jmethodid,
                                                        "..."]).ptr},
     {CallStaticBooleanMethodV: ctypes.voidptr_t},
     {CallStaticBooleanMethodA: ctypes.voidptr_t},
     {CallStaticByteMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                    jbyte,
                                                    [jenvptr, jclass,
                                                     jmethodid,
                                                     "..."]).ptr},
     {CallStaticByteMethodV: ctypes.voidptr_t},
     {CallStaticByteMethodA: ctypes.voidptr_t},
     {CallStaticCharMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                    jchar,
                                                    [jenvptr, jclass,
                                                     jmethodid,
                                                     "..."]).ptr},
     {CallStaticCharMethodV: ctypes.voidptr_t},
     {CallStaticCharMethodA: ctypes.voidptr_t},
     {CallStaticShortMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                     jshort,
                                                     [jenvptr, jclass,
                                                      jmethodid,
                                                      "..."]).ptr},
     {CallStaticShortMethodV: ctypes.voidptr_t},
     {CallStaticShortMethodA: ctypes.voidptr_t},
     {CallStaticIntMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                   jint,
                                                   [jenvptr, jclass,
                                                    jmethodid,
                                                    "..."]).ptr},
     {CallStaticIntMethodV: ctypes.voidptr_t},
     {CallStaticIntMethodA: ctypes.voidptr_t},
     {CallStaticLongMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                    jlong,
                                                    [jenvptr, jclass,
                                                     jmethodid,
                                                     "..."]).ptr},
     {CallStaticLongMethodV: ctypes.voidptr_t},
     {CallStaticLongMethodA: ctypes.voidptr_t},
     {CallStaticFloatMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                     jfloat,
                                                     [jenvptr, jclass,
                                                      jmethodid,
                                                      "..."]).ptr},
     {CallStaticFloatMethodV: ctypes.voidptr_t},
     {CallStaticFloatMethodA: ctypes.voidptr_t},
     {CallStaticDoubleMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                      jdouble,
                                                      [jenvptr, jclass,
                                                       jmethodid,
                                                       "..."]).ptr},
     {CallStaticDoubleMethodV: ctypes.voidptr_t},
     {CallStaticDoubleMethodA: ctypes.voidptr_t},
     {CallStaticVoidMethod: new ctypes.FunctionType(ctypes.default_abi,
                                                    ctypes.void_t,
                                                    [jenvptr, jclass,
                                                     jmethodid,
                                                     "..."]).ptr},
     {CallStaticVoidMethodV: ctypes.voidptr_t},
     {CallStaticVoidMethodA: ctypes.voidptr_t},
     {GetStaticFieldID: new ctypes.FunctionType(ctypes.default_abi,
                                                jfieldid,
                                                [jenvptr, jclass,
                                                 ctypes.char.ptr,
                                                 ctypes.char.ptr]).ptr},
     {GetStaticObjectField: new ctypes.FunctionType(ctypes.default_abi,
                                                    jobject,
                                                    [jenvptr, jclass,
                                                     jfieldid]).ptr},
     {GetStaticBooleanField: new ctypes.FunctionType(ctypes.default_abi,
                                                     jboolean,
                                                     [jenvptr, jclass,
                                                      jfieldid]).ptr},
     {GetStaticByteField: new ctypes.FunctionType(ctypes.default_abi,
                                                  jbyte,
                                                  [jenvptr, jclass,
                                                   jfieldid]).ptr},
     {GetStaticCharField: new ctypes.FunctionType(ctypes.default_abi,
                                                  jchar,
                                                  [jenvptr, jclass,
                                                   jfieldid]).ptr},
     {GetStaticShortField: new ctypes.FunctionType(ctypes.default_abi,
                                                   jshort,
                                                   [jenvptr, jclass,
                                                    jfieldid]).ptr},
     {GetStaticIntField: new ctypes.FunctionType(ctypes.default_abi,
                                                 jint,
                                                 [jenvptr, jclass,
                                                  jfieldid]).ptr},
     {GetStaticLongField: new ctypes.FunctionType(ctypes.default_abi,
                                                  jlong,
                                                  [jenvptr, jclass,
                                                   jfieldid]).ptr},
     {GetStaticFloatField: new ctypes.FunctionType(ctypes.default_abi,
                                                   jfloat,
                                                   [jenvptr, jclass,
                                                    jfieldid]).ptr},
     {GetStaticDoubleField: new ctypes.FunctionType(ctypes.default_abi,
                                                    jdouble,
                                                    [jenvptr, jclass,
                                                     jfieldid]).ptr},
     {SetStaticObjectField: new ctypes.FunctionType(ctypes.default_abi,
                                                    ctypes.void_t,
                                                    [jenvptr, jclass,
                                                     jfieldid, jobject]).ptr},
     {SetStaticBooleanField: new ctypes.FunctionType(ctypes.default_abi,
                                                    ctypes.void_t,
                                                    [jenvptr, jclass,
                                                     jfieldid, jboolean]).ptr},
     {SetStaticByteField: new ctypes.FunctionType(ctypes.default_abi,
                                                    ctypes.void_t,
                                                    [jenvptr, jclass,
                                                     jfieldid, jbyte]).ptr},
     {SetStaticCharField: new ctypes.FunctionType(ctypes.default_abi,
                                                    ctypes.void_t,
                                                    [jenvptr, jclass,
                                                     jfieldid, jchar]).ptr},
     {SetStaticShortField: new ctypes.FunctionType(ctypes.default_abi,
                                                    ctypes.void_t,
                                                    [jenvptr, jclass,
                                                     jfieldid, jshort]).ptr},
     {SetStaticIntField: new ctypes.FunctionType(ctypes.default_abi,
                                                    ctypes.void_t,
                                                    [jenvptr, jclass,
                                                     jfieldid, jint]).ptr},
     {SetStaticLongField: new ctypes.FunctionType(ctypes.default_abi,
                                                    ctypes.void_t,
                                                    [jenvptr, jclass,
                                                     jfieldid, jlong]).ptr},
     {SetStaticFloatField: new ctypes.FunctionType(ctypes.default_abi,
                                                    ctypes.void_t,
                                                    [jenvptr, jclass,
                                                     jfieldid, jfloat]).ptr},
     {SetStaticDoubleField: new ctypes.FunctionType(ctypes.default_abi,
                                                    ctypes.void_t,
                                                    [jenvptr, jclass,
                                                     jfieldid, jdouble]).ptr},

     {NewString: new ctypes.FunctionType(ctypes.default_abi,
                                         jstring,
                                         [jenvptr, jchar.ptr, jsize]).ptr},
     {GetStringLength: new ctypes.FunctionType(ctypes.default_abi,
                                               jsize,
                                               [jenvptr, jstring]).ptr},
     {GetStringChars: new ctypes.FunctionType(ctypes.default_abi,
                                              jchar.ptr,
                                              [jenvptr, jstring,
                                               jboolean.ptr]).ptr},
     {ReleaseStringChars: new ctypes.FunctionType(ctypes.default_abi,
                                                  ctypes.void_t,
                                                  [jenvptr, jstring,
                                                   jchar.ptr]).ptr},

     {NewStringUTF: new ctypes.FunctionType(ctypes.default_abi,
                                            jstring,
                                            [jenvptr,
                                             ctypes.char.ptr]).ptr},
     {GetStringUTFLength: new ctypes.FunctionType(ctypes.default_abi,
                                                  jsize,
                                                  [jenvptr, jstring]).ptr},
     {GetStringUTFChars: new ctypes.FunctionType(ctypes.default_abi,
                                                 ctypes.char.ptr,
                                                 [jenvptr, jstring,
                                                  jboolean.ptr]).ptr},
     {ReleaseStringUTFChars: new ctypes.FunctionType(ctypes.default_abi,
                                                     ctypes.void_t,
                                                     [jenvptr, jstring,
                                                      ctypes.char.ptr]).ptr},
     {GetArrayLength: new ctypes.FunctionType(ctypes.default_abi,
                                              jsize,
                                              [jenvptr, jarray]).ptr},
     {NewObjectArray: new ctypes.FunctionType(ctypes.default_abi,
                                              jarray,
                                              [jenvptr, jsize,
                                               jclass, jobject]).ptr},
     {GetObjectArrayElement: new ctypes.FunctionType(ctypes.default_abi,
                                                     jobject,
                                                     [jenvptr, jarray,
                                                      jsize]).ptr},
     {SetObjectArrayElement: new ctypes.FunctionType(ctypes.default_abi,
                                                     ctypes.void_t,
                                                     [jenvptr, jarray,
                                                      jsize, jobject]).ptr},
     {NewBooleanArray: new ctypes.FunctionType(ctypes.default_abi,
                                               jarray,
                                               [jenvptr, jsize]).ptr},
     {NewByteArray: new ctypes.FunctionType(ctypes.default_abi,
                                            jarray,
                                            [jenvptr, jsize]).ptr},
     {NewCharArray: new ctypes.FunctionType(ctypes.default_abi,
                                            jarray,
                                            [jenvptr, jsize]).ptr},
     {NewShortArray: new ctypes.FunctionType(ctypes.default_abi,
                                             jarray,
                                             [jenvptr, jsize]).ptr},
     {NewIntArray: new ctypes.FunctionType(ctypes.default_abi,
                                           jarray,
                                           [jenvptr, jsize]).ptr},
     {NewLongArray: new ctypes.FunctionType(ctypes.default_abi,
                                            jarray,
                                            [jenvptr, jsize]).ptr},
     {NewFloatArray: new ctypes.FunctionType(ctypes.default_abi,
                                             jarray,
                                             [jenvptr, jsize]).ptr},
     {NewDoubleArray: new ctypes.FunctionType(ctypes.default_abi,
                                              jarray,
                                              [jenvptr, jsize]).ptr},
     {GetBooleanArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                       jboolean.ptr,
                                                       [jenvptr, jarray,
                                                        jboolean.ptr]).ptr},
     {GetByteArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                    jbyte.ptr,
                                                    [jenvptr, jarray,
                                                     jboolean.ptr]).ptr},
     {GetCharArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                    jchar.ptr,
                                                    [jenvptr, jarray,
                                                     jboolean.ptr]).ptr},
     {GetShortArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                     jshort.ptr,
                                                     [jenvptr, jarray,
                                                      jboolean.ptr]).ptr},
     {GetIntArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                   jint.ptr,
                                                   [jenvptr, jarray,
                                                    jboolean.ptr]).ptr},
     {GetLongArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                    jlong.ptr,
                                                    [jenvptr, jarray,
                                                     jboolean.ptr]).ptr},
     {GetFloatArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                     jfloat.ptr,
                                                     [jenvptr, jarray,
                                                      jboolean.ptr]).ptr},
     {GetDoubleArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                      jdouble.ptr,
                                                      [jenvptr, jarray,
                                                       jboolean.ptr]).ptr},
     {ReleaseBooleanArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                           ctypes.void_t,
                                                           [jenvptr, jarray,
                                                            jboolean.ptr,
                                                            jint]).ptr},
     {ReleaseByteArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                        ctypes.void_t,
                                                        [jenvptr, jarray,
                                                         jbyte.ptr,
                                                         jint]).ptr},
     {ReleaseCharArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                        ctypes.void_t,
                                                        [jenvptr, jarray,
                                                         jchar.ptr,
                                                         jint]).ptr},
     {ReleaseShortArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                         ctypes.void_t,
                                                         [jenvptr, jarray,
                                                          jshort.ptr,
                                                          jint]).ptr},
     {ReleaseIntArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                       ctypes.void_t,
                                                       [jenvptr, jarray,
                                                        jint.ptr,
                                                        jint]).ptr},
     {ReleaseLongArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                        ctypes.void_t,
                                                        [jenvptr, jarray,
                                                         jlong.ptr,
                                                         jint]).ptr},
     {ReleaseFloatArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                         ctypes.void_t,
                                                         [jenvptr, jarray,
                                                          jfloat.ptr,
                                                          jint]).ptr},
     {ReleaseDoubleArrayElements: new ctypes.FunctionType(ctypes.default_abi,
                                                          ctypes.void_t,
                                                          [jenvptr, jarray,
                                                           jdouble.ptr,
                                                           jint]).ptr},
     {GetBooleanArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                     ctypes.void_t,
                                                     [jenvptr, jarray,
                                                      jsize, jsize,
                                                      jboolean.array()]).ptr},
     {GetByteArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                     ctypes.void_t,
                                                     [jenvptr, jarray,
                                                      jsize, jsize,
                                                      jbyte.array()]).ptr},
     {GetCharArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                  ctypes.void_t,
                                                  [jenvptr, jarray,
                                                   jsize, jsize,
                                                   jchar.array()]).ptr},
     {GetShortArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                   ctypes.void_t,
                                                   [jenvptr, jarray,
                                                    jsize, jsize,
                                                    jshort.array()]).ptr},
     {GetIntArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                 ctypes.void_t,
                                                 [jenvptr, jarray,
                                                  jsize, jsize,
                                                  jint.array()]).ptr},
     {GetLongArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                  ctypes.void_t,
                                                  [jenvptr, jarray,
                                                   jsize, jsize,
                                                   jlong.array()]).ptr},
     {GetFloatArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                   ctypes.void_t,
                                                   [jenvptr, jarray,
                                                    jsize, jsize,
                                                    jfloat.array()]).ptr},
     {GetDoubleArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                    ctypes.void_t,
                                                    [jenvptr, jarray,
                                                     jsize, jsize,
                                                     jdouble.array()]).ptr},
     {SetBooleanArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                     ctypes.void_t,
                                                     [jenvptr, jarray,
                                                      jsize, jsize,
                                                      jboolean.array()]).ptr},
     {SetByteArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                  ctypes.void_t,
                                                  [jenvptr, jarray,
                                                   jsize, jsize,
                                                   jbyte.array()]).ptr},
     {SetCharArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                  ctypes.void_t,
                                                  [jenvptr, jarray,
                                                   jsize, jsize,
                                                   jchar.array()]).ptr},
     {SetShortArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                   ctypes.void_t,
                                                   [jenvptr, jarray,
                                                    jsize, jsize,
                                                    jshort.array()]).ptr},
     {SetIntArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                 ctypes.void_t,
                                                 [jenvptr, jarray,
                                                  jsize, jsize,
                                                  jint.array()]).ptr},
     {SetLongArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                  ctypes.void_t,
                                                  [jenvptr, jarray,
                                                   jsize, jsize,
                                                   jlong.array()]).ptr},
     {SetFloatArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                   ctypes.void_t,
                                                   [jenvptr, jarray,
                                                    jsize, jsize,
                                                    jfloat.array()]).ptr},
     {SetDoubleArrayRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                    ctypes.void_t,
                                                    [jenvptr, jarray,
                                                     jsize, jsize,
                                                     jdouble.array()]).ptr},
     {RegisterNatives: ctypes.voidptr_t},
     {UnregisterNatives: ctypes.voidptr_t},
     {MonitorEnter: new ctypes.FunctionType(ctypes.default_abi,
                                            jint, [jenvptr, jobject]).ptr},
     {MonitorExit: new ctypes.FunctionType(ctypes.default_abi,
                                            jint, [jenvptr, jobject]).ptr},
     {GetJavaVM: ctypes.voidptr_t},
     {GetStringRegion: new ctypes.FunctionType(ctypes.default_abi,
                                               ctypes.void_t,
                                               [jenvptr, jstring,
                                                jsize, jsize,
                                                jchar.array()]).ptr},
     {GetStringUTFRegion: new ctypes.FunctionType(ctypes.default_abi,
                                                  ctypes.void_t,
                                                  [jenvptr, jstring,
                                                   jsize, jsize,
                                                   ctypes.char.array()]).ptr},
     {GetPrimitiveArrayCritical: ctypes.voidptr_t},
     {ReleasePrimitiveArrayCritical: ctypes.voidptr_t},
     {GetStringCritical: ctypes.voidptr_t},
     {ReleaseStringCritical: ctypes.voidptr_t},
     {NewWeakGlobalRef: ctypes.voidptr_t},
     {DeleteWeakGlobalRef: ctypes.voidptr_t},
     {ExceptionCheck: new ctypes.FunctionType(ctypes.default_abi,
                                              jboolean, [jenvptr]).ptr},
     {NewDirectByteBuffer: ctypes.voidptr_t},
     {GetDirectBufferAddress: ctypes.voidptr_t},
     {GetDirectBufferCapacity: ctypes.voidptr_t},
     {GetObjectRefType: ctypes.voidptr_t}]
);

var GetJNIForThread = libxul.declare("GetJNIForThread",
                                     ctypes.default_abi,
                                     JNINativeInterface.ptr.ptr);

var registry = Object.create(null);
var classes = Object.create(null);

function JNIUnloadClasses(jenv) {
  Object.getOwnPropertyNames(registry).forEach(function(classname) {
    var jcls = unwrap(registry[classname]);
    jenv.contents.contents.DeleteGlobalRef(jenv, jcls);

    // Purge the registry, so we don't try to reuse stale global references
    // in JNI calls and we garbage-collect the JS global reference objects.
    delete registry[classname];
  });

  // The refs also get added to the 'classes' object, so we should purge it too.
  // That object is a hierarchical data structure organized by class path parts,
  // but deleting its own properties should be sufficient to break its refs.
  Object.getOwnPropertyNames(classes).forEach(function(topLevelPart) {
    delete classes[topLevelPart];
  });
}

var PREFIX = 'js#';
// this regex matches one component of a type signature:
// any number of array modifiers, followed by either a
// primitive type character or L<classname>;
var sigRegex = function() /\[*([VZBCSIJFD]|L([^.\/;]+(\/[^.\/;]+)*);)/g;
var ensureSig = function(classname_or_signature) {
    // convert a classname into a signature,
    // leaving unchanged signatures.  We assume that
    // anything not a valid signature is a classname.
  var m = sigRegex().exec(classname_or_signature);
  return (m && m[0] === classname_or_signature) ? classname_or_signature :
    'L' + classname_or_signature.replace(/\./g, '/') + ';';
};
var wrap = function(obj, classSig) {
  if (!classSig) { return obj; }
  // don't wrap primitive types.
  if (classSig.charAt(0)!=='L' &&
      classSig.charAt(0)!=='[') { return obj; }
  var proto = registry[classSig][PREFIX+'proto'];
  return new proto(obj);
};
var unwrap = function(obj, opt_jenv, opt_ctype) {
  if (obj && typeof(obj)==='object' && (PREFIX+'obj') in obj) {
    return obj[PREFIX+'obj'];
  } else if (opt_jenv && opt_ctype) {
    if (opt_ctype !== jobject)
      return opt_ctype(obj); // cast to given primitive ctype
    if (typeof(obj)==='string')
      return unwrap(JNINewString(opt_jenv, obj)); // create Java String
  }
  return obj;
};
var ensureLoaded = function(jenv, classSig) {
  if (!Object.hasOwnProperty.call(registry, classSig)) {
    JNILoadClass(jenv, classSig);
  }
  return registry[classSig];
};

function JNINewString(jenv, value) {
  var s = jenv.contents.contents.NewStringUTF(jenv, ctypes.char.array()(value));
  ensureLoaded(jenv, "Ljava/lang/String;");
  return wrap(s, "Ljava/lang/String;");
}

function JNIReadString(jenv, jstring_value) {
  var val = unwrap(jstring_value);
  if ((!val) || val.isNull()) { return null; }
  var chars = jenv.contents.contents.GetStringUTFChars(jenv, val, null);
  var result = chars.readString();
  jenv.contents.contents.ReleaseStringUTFChars(jenv, val, chars);
  return result;
}

var sigInfo = {
  'V': { name: 'Void', longName: 'Void', ctype: ctypes.void_t },
  'Z': { name: 'Boolean', longName: 'Boolean', ctype: jboolean },
  'B': { name: 'Byte', longName: 'Byte', ctype: jbyte },
  'C': { name: 'Char', longName: 'Char', ctype: jchar },
  'S': { name: 'Short', longName: 'Short', ctype: jshort },
  'I': { name: 'Int', longName: 'Integer', ctype: jint },
  'J': { name: 'Long', longName: 'Long', ctype: jlong },
  'F': { name: 'Float', longName: 'Float', ctype: jfloat },
  'D': { name: 'Double', longName: 'Double', ctype: jdouble },
  'L': { name: 'Object', longName: 'Object', ctype: jobject },
  '[': { name: 'Object', longName: 'Object', ctype: jarray }
};

var sig2type = function(sig) { return sigInfo[sig.charAt(0)].name; };
var sig2ctype = function(sig) { return sigInfo[sig.charAt(0)].ctype; };
var sig2prim = function(sig) { return sigInfo[sig.charAt(0)].longName; };

// return the class object for a signature string.
// allocates 1 or 2 local refs
function JNIClassObj(jenv, classSig) {
    var jenvpp = function() { return jenv.contents.contents; };
    // Deal with funny calling convention of JNI FindClass method.
    // Classes get the leading & trailing chars stripped; primitives
    // have to be looked up via their wrapper type.
    var prim = function(ty) {
        var jcls = jenvpp().FindClass(jenv, "java/lang/"+ty);
        var jfld = jenvpp().GetStaticFieldID(jenv, jcls, "TYPE",
                                             "Ljava/lang/Class;");
        return jenvpp().GetStaticObjectField(jenv, jcls, jfld);
    };
    switch (classSig.charAt(0)) {
    case '[':
        return jenvpp().FindClass(jenv, classSig);
    case 'L':
        classSig = classSig.substring(1, classSig.indexOf(';'));
        return jenvpp().FindClass(jenv, classSig);
    default:
      return prim(sig2prim(classSig));
    }
}

// return the signature string for a Class object.
// allocates 2 local refs
function JNIClassSig(jenv, jcls) {
  var jenvpp = function() { return jenv.contents.contents; };
  var jclscls = jenvpp().FindClass(jenv, "java/lang/Class");
  var jmtd = jenvpp().GetMethodID(jenv, jclscls,
                                  "getName", "()Ljava/lang/String;");
  var name = jenvpp().CallObjectMethod(jenv, jcls, jmtd);
  name = JNIReadString(jenv, name);
  // API is weird.  Make sure we're using slashes not dots
  name = name.replace(/\./g, '/');
  // special case primitives, arrays
  if (name.charAt(0)==='[') return name;
  switch(name) {
  case 'void': return 'V';
  case 'boolean': return 'Z';
  case 'byte': return 'B';
  case 'char': return 'C';
  case 'short': return 'S';
  case 'int': return 'I';
  case 'long': return 'J';
  case 'float': return 'F';
  case 'double': return 'D';
  default:
    return 'L' + name + ';';
  }
}

// create dispatch method
// we resolve overloaded methods only by # of arguments.  If you need
// further resolution, use the 'long form' of the method name, ie:
//    obj['toString()Ljava/lang/String'].call(obj);
var overloadFunc = function(basename) {
  return function() {
    return this[basename+'('+arguments.length+')'].apply(this, arguments);
  };
};

// Create appropriate wrapper fields/methods for a Java class.
function JNILoadClass(jenv, classSig, opt_props) {
  var jenvpp = function() { return jenv.contents.contents; };
  var props = opt_props || {};

  // allocate a local reference frame with enough space
  // this class (1 or 2 local refs) plus superclass (3 refs)
  // plus array element class (1 or 2 local refs)
  var numLocals = 7;
  jenvpp().PushLocalFrame(jenv, numLocals);

  var jcls;
  if (Object.hasOwnProperty.call(registry, classSig)) {
    jcls = unwrap(registry[classSig]);
  } else {
    jcls = jenvpp().NewGlobalRef(jenv, JNIClassObj(jenv, classSig));

    // get name of superclass
    var jsuper = jenvpp().GetSuperclass(jenv, jcls);
    if (jsuper.isNull()) {
      jsuper = null;
    } else {
      jsuper = JNIClassSig(jenv, jsuper);
    }

    registry[classSig] = Object.create(jsuper?ensureLoaded(jenv, jsuper):null);
    registry[classSig][PREFIX+'obj'] = jcls; // global ref, persistent.
    registry[classSig][PREFIX+'proto'] =
      function(o) { this[PREFIX+'obj'] = o; };
    registry[classSig][PREFIX+'proto'].prototype =
      Object.create(jsuper ?
                    ensureLoaded(jenv, jsuper)[PREFIX+'proto'].prototype :
                    null);
    // Add a __cast__ method to the wrapper corresponding to the class
    registry[classSig].__cast__ = function(obj) {
      return wrap(unwrap(obj), classSig);
    };

    // make wrapper accessible via the classes object.
    var path = sig2type(classSig).toLowerCase();
    if (classSig.charAt(0)==='L') {
      path = classSig.substring(1, classSig.length-1);
    }
    if (classSig.charAt(0)!=='[') {
      var root = classes, i;
      var parts = path.split('/');
      for (i = 0; i < parts.length-1; i++) {
        if (!Object.hasOwnProperty.call(root, parts[i])) {
          root[parts[i]] = Object.create(null);
        }
        root = root[parts[i]];
      }
      root[parts[parts.length-1]] = registry[classSig];
    }
  }

  var r = registry[classSig];
  var rpp = r[PREFIX+'proto'].prototype;

  if (classSig.charAt(0)==='[') {
    // add 'length' field for arrays
    Object.defineProperty(rpp, 'length', {
      get: function() {
        return jenvpp().GetArrayLength(jenv, unwrap(this));
      }
    });
    // add 'get' and 'set' methods, 'new' constructor
    var elemSig = classSig.substring(1);
    ensureLoaded(jenv, elemSig);

    registry[elemSig].__array__ = r;
    if (!Object.hasOwnProperty.call(registry[elemSig], 'array'))
      registry[elemSig].array = r;

    if (elemSig.charAt(0)==='L' || elemSig.charAt(0)==='[') {
      var elemClass = unwrap(registry[elemSig]);

      rpp.get = function(idx) {
        return wrap(jenvpp().GetObjectArrayElement(jenv, unwrap(this), idx),
                    elemSig);
      };
      rpp.set = function(idx, value) {
        jenvpp().SetObjectArrayElement(jenv, unwrap(this), idx,
                                       unwrap(value, jenv, jobject));
      };
      rpp.getElements = function(start, len) {
        var i, r=[];
        for (i=0; i<len; i++) { r.push(this.get(start+i)); }
        return r;
      };
      rpp.setElements = function(start, vals) {
        vals.forEach(function(v, i) { this.set(start+i, v); }.bind(this));
      };
      r['new'] = function(length) {
        return wrap(jenvpp().NewObjectArray(jenv, length, elemClass, null),
                    classSig);
      };
    } else {
      var ty = sig2type(elemSig), ctype = sig2ctype(elemSig);
      var constructor = "New"+ty+"Array";
      var getter = "Get"+ty+"ArrayRegion";
      var setter = "Set"+ty+"ArrayRegion";
      rpp.get = function(idx) { return this.getElements(idx, 1)[0]; };
      rpp.set = function(idx, val) { this.setElements(idx, [val]); };
      rpp.getElements = function(start, len) {
        var j = jenvpp();
        var buf = new (ctype.array())(len);
        j[getter].call(j, jenv, unwrap(this), start, len, buf);
        return buf;
      };
      rpp.setElements = function(start, vals) {
        var j = jenvpp();
        j[setter].call(j, jenv, unwrap(this), start, vals.length,
                       ctype.array()(vals));
      };
      r['new'] = function(length) {
        var j = jenvpp();
        return wrap(j[constructor].call(j, jenv, length), classSig);
      };
    }
  }

  (props.static_fields || []).forEach(function(fld) {
    var jfld = jenvpp().GetStaticFieldID(jenv, jcls, fld.name, fld.sig);
    var ty = sig2type(fld.sig), nm = fld.sig;
    var getter = "GetStatic"+ty+"Field", setter = "SetStatic"+ty+"Field";
    ensureLoaded(jenv, nm);
    var props =  {
      get: function() {
        var j = jenvpp();
        return wrap(j[getter].call(j, jenv, jcls, jfld), nm);
      },
      set: function(newValue) {
        var j = jenvpp();
        j[setter].call(j, jenv, jcls, jfld, unwrap(newValue));
      }
    };
    Object.defineProperty(r, fld.name, props);
    // add static fields to object instances, too.
    Object.defineProperty(rpp, fld.name, props);
  });
  (props.static_methods || []).forEach(function(mtd) {
    var jmtd = jenvpp().GetStaticMethodID(jenv, jcls, mtd.name, mtd.sig);
    var argctypes = mtd.sig.match(sigRegex()).map(function(s) sig2ctype(s));
    var returnSig = mtd.sig.substring(mtd.sig.indexOf(')')+1);
    var ty = sig2type(returnSig), nm = returnSig;
    var call = "CallStatic"+ty+"Method";
    ensureLoaded(jenv, nm);
    r[mtd.name] = rpp[mtd.name] = overloadFunc(mtd.name);
    r[mtd.name + mtd.sig] = r[mtd.name+'('+(argctypes.length-1)+')'] =
    // add static methods to object instances, too.
    rpp[mtd.name + mtd.sig] = rpp[mtd.name+'('+(argctypes.length-1)+')'] = function() {
      var i, j = jenvpp();
      var args = [jenv, jcls, jmtd];
      for (i=0; i<arguments.length; i++) {
        args.push(unwrap(arguments[i], jenv, argctypes[i]));
      }
      return wrap(j[call].apply(j, args), nm);
    };
  });
  (props.constructors || []).forEach(function(mtd) {
    mtd.name = "<init>";
    var jmtd = jenvpp().GetMethodID(jenv, jcls, mtd.name, mtd.sig);
    var argctypes = mtd.sig.match(sigRegex()).map(function(s) sig2ctype(s));
    var returnSig = mtd.sig.substring(mtd.sig.indexOf(')')+1);

    r['new'] = overloadFunc('new');
    r['new'+mtd.sig] = r['new('+(argctypes.length-1)+')'] = function() {
      var i, j = jenvpp();
      var args = [jenv, jcls, jmtd];
      for (i=0; i<arguments.length; i++) {
        args.push(unwrap(arguments[i], jenv, argctypes[i]));
      }
      return wrap(j.NewObject.apply(j, args), classSig);
    };
  });
  (props.fields || []).forEach(function(fld) {
    var jfld = jenvpp().GetFieldID(jenv, jcls, fld.name, fld.sig);
    var ty = sig2type(fld.sig), nm = fld.sig;
    var getter = "Get"+ty+"Field", setter = "Set"+ty+"Field";
    ensureLoaded(jenv, nm);
    Object.defineProperty(rpp, fld.name, {
      get: function() {
        var j = jenvpp();
        return wrap(j[getter].call(j, jenv, unwrap(this), jfld), nm);
      },
      set: function(newValue) {
        var j = jenvpp();
        j[setter].call(j, jenv, unwrap(this), jfld, unwrap(newValue));
      }
    });
  });
  (props.methods || []).forEach(function(mtd) {
    var jmtd = jenvpp().GetMethodID(jenv, jcls, mtd.name, mtd.sig);
    var argctypes = mtd.sig.match(sigRegex()).map(function(s) sig2ctype(s));
    var returnSig = mtd.sig.substring(mtd.sig.indexOf(')')+1);
    var ty = sig2type(returnSig), nm = returnSig;
    var call = "Call"+ty+"Method";
    ensureLoaded(jenv, nm);
    rpp[mtd.name] = overloadFunc(mtd.name);
    rpp[mtd.name + mtd.sig] = rpp[mtd.name+'('+(argctypes.length-1)+')'] = function() {
      var i, j = jenvpp();
      var args = [jenv, unwrap(this), jmtd];
      for (i=0; i<arguments.length; i++) {
        args.push(unwrap(arguments[i], jenv, argctypes[i]));
      }
      return wrap(j[call].apply(j, args), nm);
    };
  });
  jenvpp().PopLocalFrame(jenv, null);
  return r;
}

// exported object
var JNI = {
  // primitive types
  jboolean: jboolean,
  jbyte: jbyte,
  jchar: jchar,
  jshort: jshort,
  jint: jint,
  jlong: jlong,
  jfloat: jfloat,
  jdouble: jdouble,
  jsize: jsize,

  // class registry
  classes: classes,

  // methods
  GetForThread: GetJNIForThread,
  NewString: JNINewString,
  ReadString: JNIReadString,
  LoadClass: function(jenv, classname_or_signature, props) {
    return JNILoadClass(jenv, ensureSig(classname_or_signature), props);
  },
  UnloadClasses: JNIUnloadClasses
};
