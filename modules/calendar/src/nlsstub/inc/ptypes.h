#ifndef ptypes_h__
#define ptypes_h__

typedef double Date;

#define ZERO_ERROR 0

typedef PRInt32   ErrorCode;
typedef PRInt8    t_int8;
typedef PRInt32   t_int32;
typedef PRBool    t_bool;
typedef PRInt32   TextOffset;

#define NS_NLS NS_BASE

#define SUCCESS(x) ((x)<=ZERO_ERROR)
#define FAILURE(x) ((x)>ZERO_ERROR)

#define kMillisPerSecond (PR_INT32(1000))
#define kMillisPerMinute (PR_INT32(60) * kMillisPerSecond)
#define kMillisPerHour   (PR_INT32(60) * kMillisPerMinute)
#define kMillisPerDay    (PR_INT32(24) * kMillisPerHour)

#endif
