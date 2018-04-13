#include "errno.h"
#include "string.h"

#include "jsctypes-test.h"
#include "jsctypes-test-finalizer.h"

/**
 * Shared infrastructure
 */


/**
 * An array of integers representing resources.
 * - 0: unacquired
 * - 1: acquired
 * - < 0: error, resource has been released several times.
 */
int *gFinalizerTestResources = nullptr;
char **gFinalizerTestNames = nullptr;
size_t gFinalizerTestSize;

void
test_finalizer_start(size_t size)
{
  gFinalizerTestResources = new int[size];
  gFinalizerTestNames = new char*[size];
  gFinalizerTestSize = size;
  for (size_t i = 0; i < size; ++i) {
    gFinalizerTestResources[i] = 0;
    gFinalizerTestNames[i] = nullptr;
  }
}

void
test_finalizer_stop()
{
  delete[] gFinalizerTestResources;
}

/**
 * Check if an acquired resource has been released
 */
bool
test_finalizer_resource_is_acquired(size_t i)
{
  return gFinalizerTestResources[i] == 1;
}
// Resource type: size_t

// Acquire resource i
size_t
test_finalizer_acq_size_t(size_t i)
{
  gFinalizerTestResources[i] = 1;
  return i;
}

// Release resource i
void
test_finalizer_rel_size_t(size_t i)
{
  if (--gFinalizerTestResources[i] < 0) {
    MOZ_CRASH("Assertion failed");
  }
}

size_t
test_finalizer_rel_size_t_return_size_t(size_t i)
{
  if (-- gFinalizerTestResources[i] < 0) {
    MOZ_CRASH("Assertion failed");
  }
  return i;
}

myRECT
test_finalizer_rel_size_t_return_struct_t(size_t i)
{
  if (-- gFinalizerTestResources[i] < 0) {
    MOZ_CRASH("Assertion failed");
  }
  const int32_t narrowed = (int32_t)i;
  myRECT result = { narrowed, narrowed, narrowed, narrowed };
  return result;
}

bool
test_finalizer_cmp_size_t(size_t a, size_t b)
{
  return a==b;
}

// Resource type: int32_t

// Acquire resource i
int32_t
test_finalizer_acq_int32_t(size_t i)
{
  gFinalizerTestResources[i] = 1;
  return i;
}

// Release resource i
void
test_finalizer_rel_int32_t(int32_t i)
{
  if (--gFinalizerTestResources[i] < 0) {
    MOZ_CRASH("Assertion failed");
  }
}

bool
test_finalizer_cmp_int32_t(int32_t a, int32_t b)
{
  return a==b;
}

// Resource type: int64_t

// Acquire resource i
int64_t
test_finalizer_acq_int64_t(size_t i)
{
  gFinalizerTestResources[i] = 1;
  return i;
}

// Release resource i
void
test_finalizer_rel_int64_t(int64_t i)
{
  if (-- gFinalizerTestResources[i] < 0) {
    MOZ_CRASH("Assertion failed");
  }
}

bool
test_finalizer_cmp_int64_t(int64_t a, int64_t b)
{
  return a==b;
}

// Resource type: void*

// Acquire resource i
void*
test_finalizer_acq_ptr_t(size_t i)
{
  gFinalizerTestResources[i] = 1;
  return (void*)&gFinalizerTestResources[i];
}

// Release resource i
void
test_finalizer_rel_ptr_t(void *i)
{
  int *as_int = (int*)i;
  -- (*as_int);
  if (*as_int < 0) {
    MOZ_CRASH("Assertion failed");
  }
}

bool
test_finalizer_cmp_ptr_t(void *a, void *b)
{
  return a==b;
}

// Resource type: int32_t*

// Acquire resource i
int32_t*
test_finalizer_acq_int32_ptr_t(size_t i)
{
  gFinalizerTestResources[i] = 1;
  return (int32_t*)&gFinalizerTestResources[i];
}

// Release resource i
void
test_finalizer_rel_int32_ptr_t(int32_t *i)
{
  -- (*i);
  if (*i < 0) {
    MOZ_CRASH("Assertion failed");
  }
}

bool
test_finalizer_cmp_int32_ptr_t(int32_t *a, int32_t *b)
{
  return a==b;
}

// Resource type: nullptr

// Acquire resource i
void*
test_finalizer_acq_null_t(size_t i)
{
  gFinalizerTestResources[0] = 1;//Always index 0
  return nullptr;
}

// Release resource i
void
test_finalizer_rel_null_t(void *i)
{
  if (i != nullptr) {
    MOZ_CRASH("Assertion failed");
  }
  gFinalizerTestResources[0] --;
}

bool
test_finalizer_null_resource_is_acquired(size_t)
{
  return gFinalizerTestResources[0] == 1;
}

bool
test_finalizer_cmp_null_t(void *a, void *b)
{
  return a==b;
}

// Resource type: char*

// Acquire resource i
char*
test_finalizer_acq_string_t(int i)
{
  gFinalizerTestResources[i] = 1;
  if (!gFinalizerTestNames[i]) {
    char* buf = new char[12];
    snprintf(buf, 12, "%d", i);
    gFinalizerTestNames[i] = buf;
    return buf;
  }
  return gFinalizerTestNames[i];
}

// Release resource i
void
test_finalizer_rel_string_t(char *i)
{
  int index = atoi(i);
  if (index < 0 || index >= (int)gFinalizerTestSize) {
    MOZ_CRASH("Assertion failed");
  }
  gFinalizerTestResources[index] --;
}

bool
test_finalizer_string_resource_is_acquired(size_t i)
{
  return gFinalizerTestResources[i] == 1;
}

bool
test_finalizer_cmp_string_t(char *a, char *b)
{
  return !strncmp(a, b, 10);
}

// Resource type: myRECT

// Acquire resource i
myRECT
test_finalizer_acq_struct_t(int i)
{
  gFinalizerTestResources[i] = 1;
  myRECT result = { i, i, i, i };
  return result;
}

// Release resource i
void
test_finalizer_rel_struct_t(myRECT i)
{
  int index = i.top;
  if (index < 0 || index >= (int)gFinalizerTestSize) {
    MOZ_CRASH("Assertion failed");
  }
  gFinalizerTestResources[index] --;
}

bool
test_finalizer_struct_resource_is_acquired(myRECT i)
{
  int index = i.top;
  if (index < 0 || index >= (int)gFinalizerTestSize) {
    MOZ_CRASH("Assertion failed");
  }
  return gFinalizerTestResources[index] == 1;
}

bool
test_finalizer_cmp_struct_t(myRECT a, myRECT b)
{
  return a.top == b.top;
}

// Support for checking that we reject nullptr finalizer
afun* test_finalizer_rel_null_function()
{
  return nullptr;
}

void
test_finalizer_rel_size_t_set_errno(size_t i)
{
  if (-- gFinalizerTestResources[i] < 0) {
    MOZ_CRASH("Assertion failed");
  }
  errno = 10;
}

void
reset_errno()
{
  errno = 0;
}
