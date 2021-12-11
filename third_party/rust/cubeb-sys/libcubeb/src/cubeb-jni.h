#ifndef _CUBEB_JNI_H_
#define _CUBEB_JNI_H_

typedef struct cubeb_jni cubeb_jni;

cubeb_jni * cubeb_jni_init();
int cubeb_get_output_latency_from_jni(cubeb_jni * cubeb_jni_ptr);
void cubeb_jni_destroy(cubeb_jni * cubeb_jni_ptr);

#endif // _CUBEB_JNI_H_
