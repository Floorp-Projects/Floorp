/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef APKOpen_h
#define APKOpen_h

#include <jni.h>
#include <pthread.h>

#ifndef NS_EXPORT
#define NS_EXPORT __attribute__ ((visibility("default")))
#endif

struct mapping_info {
  char * name;
  uintptr_t base;
  size_t len;
  size_t offset;
};

NS_EXPORT const struct mapping_info * getLibraryMapping();
NS_EXPORT void abortThroughJava(const char* msg);
NS_EXPORT pthread_t getJavaUiThread();

static const int SUCCESS = 0;
static const int FAILURE = 1;
void JNI_Throw(JNIEnv* jenv, const char* classname, const char* msg);

// Bug 1207642 - Work around Dalvik bug by realigning stack on JNI entry
#ifndef MOZ_JNICALL
# ifdef __i386__
#  define MOZ_JNICALL JNICALL __attribute__((force_align_arg_pointer))
# else
#  define MOZ_JNICALL JNICALL
# endif
#endif

#endif /* APKOpen_h */
