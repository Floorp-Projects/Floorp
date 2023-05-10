/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "InterposerHelper.h"

// The interposers in this file cover all the functions used to access the
// environment (getenv(), putenv(), setenv(), unsetenv() and clearenv()). They
// all use the mutex below for synchronization to prevent races that caused
// startup crashes, see bug 1752703.
static pthread_mutex_t gEnvLock = PTHREAD_MUTEX_INITIALIZER;

static char* internal_getenv(const char* aName) {
  if (environ == nullptr || aName[0] == '\0') {
    return nullptr;
  }

  size_t len = strlen(aName);
  for (char** env_ptr = environ; *env_ptr != nullptr; ++env_ptr) {
    if ((aName[0] == (*env_ptr)[0]) && (strncmp(aName, *env_ptr, len) == 0) &&
        ((*env_ptr)[len] == '=')) {
      return *env_ptr + len + 1;
    }
  }

  return nullptr;
}

extern "C" {

MFBT_API char* getenv(const char* name) {
  pthread_mutex_lock(&gEnvLock);
  char* result = internal_getenv(name);
  pthread_mutex_unlock(&gEnvLock);

  return result;
}

MFBT_API int putenv(char* string) {
  static const auto real_putenv = GET_REAL_SYMBOL(putenv);

  pthread_mutex_lock(&gEnvLock);
  int result = real_putenv(string);
  pthread_mutex_unlock(&gEnvLock);
  return result;
}

MFBT_API int setenv(const char* name, const char* value, int replace) {
  static const auto real_setenv = GET_REAL_SYMBOL(setenv);

  pthread_mutex_lock(&gEnvLock);
  int result = real_setenv(name, value, replace);
  pthread_mutex_unlock(&gEnvLock);
  return result;
}

MFBT_API int unsetenv(const char* name) {
  static const auto real_unsetenv = GET_REAL_SYMBOL(unsetenv);

  pthread_mutex_lock(&gEnvLock);
  int result = real_unsetenv(name);
  pthread_mutex_unlock(&gEnvLock);
  return result;
}

MFBT_API int clearenv(void) {
  static const auto real_clearenv = GET_REAL_SYMBOL(clearenv);

  pthread_mutex_lock(&gEnvLock);
  int result = real_clearenv();
  pthread_mutex_unlock(&gEnvLock);
  return result;
}

}  // extern "C"
