#ifndef BROWSER_DIAGNOSTICS_H
#define BROWSER_DIAGNOSTICS_H

#ifndef _DEBUG
#  include <assert.h>
#  define NG_TRACE ATLTRACE
#  define NG_ASSERT(expr) assert(expr)
#  define NG_ASSERT_POINTER(p, type) \
	NG_ASSERT(((p) != NULL) && NgIsValidAddress((p), sizeof(type), FALSE))
#  define NG_ASSERT_NULL_OR_POINTER(p, type) \
	NG_ASSERT(((p) == NULL) || NgIsValidAddress((p), sizeof(type), FALSE))
#else
#  define NG_ASSERT(X)
#  define NG_ASSERT_POINTER(p, type)
#  define NG_ASSERT_NULL_OR_POINTER(p, type)
#endif

inline BOOL NgIsValidAddress(const void* lp, UINT nBytes, BOOL bReadWrite = TRUE)
{
	return (lp != NULL && !IsBadReadPtr(lp, nBytes) &&
		(!bReadWrite || !IsBadWritePtr((LPVOID)lp, nBytes)));
}

#endif