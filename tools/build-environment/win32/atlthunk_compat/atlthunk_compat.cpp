#include <atlbase.h>
#include <windows.h>

PVOID
__AllocstdCallThunk(VOID)
{
	return
		HeapAlloc(GetProcessHeap(), 0,
			      sizeof(_stdcallthunk));
}

VOID
__FreeStdCallThunk(PVOID p)
{
	HeapFree(GetProcessHeap(), 0, p);
}