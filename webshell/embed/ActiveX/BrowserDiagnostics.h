#ifndef BROWSER_DIAGNOSTICS_H
#define BROWSER_DIAGNOSTICS_H

#ifndef _DEBUG
#define NG_ASSERT(X)
#define NG_ASSERT_POINTER(p, type) \
	NG_ASSERT(((p) != NULL) && NgIsValidAddress((p), sizeof(type), FALSE))

#define NG_ASSERT_NULL_OR_POINTER(p, type) \
	NG_ASSERT(((p) == NULL) || NgIsValidAddress((p), sizeof(type), FALSE))
#else
#define NG_ASSERT(X)
#define NG_ASSERT_POINTER(p, type)
#define NG_ASSERT_NULL_OR_POINTER(p, type)
#endif

inline BOOL NgIsValidAddress(const void* lp, UINT nBytes, BOOL bReadWrite = TRUE)
{
	if (lp == NULL)
	{
		return FALSE;
	}
	return TRUE;
}

#endif