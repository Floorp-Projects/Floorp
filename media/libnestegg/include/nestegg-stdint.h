#ifdef _WIN32
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#if !defined(INT64_MAX)
#define INT64_MAX 9223372036854775807LL
#endif
#else
#include <stdint.h>
#endif

