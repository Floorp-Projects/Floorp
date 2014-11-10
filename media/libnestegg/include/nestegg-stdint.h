#ifdef _WIN32
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#define INT64_MAX 9223372036854775807LL
#else
#include <stdint.h>
#endif

