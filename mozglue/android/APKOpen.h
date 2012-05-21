/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef APKOpen_h
#define APKOpen_h

#include <jni.h>

#ifndef NS_EXPORT
#define NS_EXPORT __attribute__ ((visibility("default")))
#endif

struct mapping_info {
  char * name;
  char * file_id;
  uintptr_t base;
  size_t len;
  size_t offset;
};

const struct mapping_info * getLibraryMapping();

static const int SUCCESS = 0;
static const int FAILURE = 1;
void JNI_Throw(JNIEnv* jenv, const char* classname, const char* msg);

#define MAX_LIB_CACHE_ENTRIES 32
#define MAX_LIB_CACHE_NAME_LEN 32

struct lib_cache_info {
  char name[MAX_LIB_CACHE_NAME_LEN];
  int fd;
  uint32_t lib_size;
  void* buffer;
};

NS_EXPORT const struct lib_cache_info * getLibraryCache();


#endif /* APKOpen_h */
