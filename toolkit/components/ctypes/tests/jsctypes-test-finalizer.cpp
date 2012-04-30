#include "errno.h"

#include "jsctypes-test.h"
#include "jsctypes-test-finalizer.h"
#include "jsapi.h"

#if defined(XP_WIN)
#define snprintf _snprintf
#endif // defined(XP_WIN)

/**
 * Shared infrastructure
 */


/**
 * An array of integers representing resources.
 * - 0: unacquired
 * - 1: acquired
 * - < 0: error, resource has been released several times.
 */
int *gFinalizerTestResources = NULL;
char **gFinalizerTestNames = NULL;
size_t gFinalizerTestSize;

void
test_finalizer_start(size_t size)
{
  gFinalizerTestResources = new int[size];
  gFinalizerTestNames = new char*[size];
  gFinalizerTestSize = size;
  for (size_t i = 0; i < size; ++i) {
    gFinalizerTestResources[i] = 0;
    gFinalizerTestNames[i] = NULL;
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
    MOZ_NOT_REACHED("Assertion failed");
  }
}

size_t
test_finalizer_rel_size_t_return_size_t(size_t i)
{
  if (-- gFinalizerTestResources[i] < 0) {
    MOZ_NOT_REACHED("Assertion failed");
  }
  return i;
}

RECT
test_finalizer_rel_size_t_return_struct_t(size_t i)
{
  if (-- gFinalizerTestResources[i] < 0) {
    MOZ_NOT_REACHED("Assertion failed");
  }
  const PRInt32 narrowed = (PRInt32)i;
  RECT result = { narrowed, narrowed, narrowed, narrowed };
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
    MOZ_NOT_REACHED("Assertion failed");
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
    MOZ_NOT_REACHED("Assertion failed");
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
    MOZ_NOT_REACHED("Assertion failed");
  }
}

bool
test_finalizer_cmp_ptr_t(void *a, void *b)
{
  return a==b;
}

// Resource type: NULL

// Acquire resource i
void*
test_finalizer_acq_null_t(size_t i)
{
  gFinalizerTestResources[0] = 1;//Always index 0
  return NULL;
}

// Release resource i
void
test_finalizer_rel_null_t(void *i)
{
  if (i != NULL) {
    MOZ_NOT_REACHED("Assertion failed");
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
    char* buf = new char[10];
    snprintf(buf, 10, "%d", i);
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
    MOZ_NOT_REACHED("Assertion failed");
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

// Resource type: RECT

// Acquire resource i
RECT
test_finalizer_acq_struct_t(int i)
{
  gFinalizerTestResources[i] = 1;
  RECT result = { i, i, i, i };
  return result;
}

// Release resource i
void
test_finalizer_rel_struct_t(RECT i)
{
  int index = i.top;
  if (index < 0 || index >= (int)gFinalizerTestSize) {
    MOZ_NOT_REACHED("Assertion failed");
  }
  gFinalizerTestResources[index] --;
}

bool
test_finalizer_struct_resource_is_acquired(RECT i)
{
  int index = i.top;
  if (index < 0 || index >= (int)gFinalizerTestSize) {
    MOZ_NOT_REACHED("Assertion failed");
  }
  return gFinalizerTestResources[index] == 1;
}

bool
test_finalizer_cmp_struct_t(RECT a, RECT b)
{
  return a.top == b.top;
}

// Support for checking that we reject NULL finalizer
afun* test_finalizer_rel_null_function()
{
  return NULL;
}

void
test_finalizer_rel_size_t_set_errno(size_t i)
{
  if (-- gFinalizerTestResources[i] < 0) {
    MOZ_NOT_REACHED("Assertion failed");
  }
  errno = 10;
}

void
reset_errno()
{
  errno = 0;
}

