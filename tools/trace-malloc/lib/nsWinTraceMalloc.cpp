
#include <stdio.h>
#include <stdlib.h>

#include "prtypes.h"
#include "prlock.h"
#include "nscore.h"
#include "nsAutoLock.h"

#include "nsDebugHelpWin32.h"
#include "nsTraceMallocCallbacks.h"

/***************************************************************************/
// shows how to use the dhw stuff to hook imported functions

#if _MSC_VER < 1300
#define NS_DEBUG_CRT "MSVCRTD.dll"
#elif _MSC_VER == 1300
#define NS_DEBUG_CRT "msvcr70d.dll"
#elif _MSC_VER == 1310
#define NS_DEBUG_CRT "msvcr71d.dll"
#elif _MSC_VER == 1400
#define NS_DEBUG_CRT "msvcr80d.dll"
#elif _MSC_VER == 1500
#define NS_DEBUG_CRT "msvcr90d.dll"
#else
#error "Don't know filename of MSVC debug library."
#endif

DHW_DECLARE_FUN_TYPE_AND_PROTO(dhw_malloc, void*, __cdecl, MALLOC_, (size_t));

DHWImportHooker &getMallocHooker()
{
  static DHWImportHooker gMallocHooker(NS_DEBUG_CRT, "malloc", (PROC) dhw_malloc);
  return gMallocHooker;
}

void * __cdecl dhw_malloc( size_t size )
{
    PRUint32 start = PR_IntervalNow();
    void* result = DHW_ORIGINAL(MALLOC_, getMallocHooker())(size);
    PRUint32 end = PR_IntervalNow();
    MallocCallback(result, size, start, end);
    return result;    
}

DHW_DECLARE_FUN_TYPE_AND_PROTO(dhw_calloc, void*, __cdecl, CALLOC_, (size_t,size_t));

DHWImportHooker &getCallocHooker()
{
  static DHWImportHooker gCallocHooker(NS_DEBUG_CRT, "calloc", (PROC) dhw_calloc);
  return gCallocHooker;
}

void * __cdecl dhw_calloc( size_t count, size_t size )
{
    PRUint32 start = PR_IntervalNow();
    void* result = DHW_ORIGINAL(CALLOC_, getCallocHooker())(count,size);
    PRUint32 end = PR_IntervalNow();
    CallocCallback(result, count, size, start, end);
    return result;    
}

DHW_DECLARE_FUN_TYPE_AND_PROTO(dhw_free, void, __cdecl, FREE_, (void*));
DHWImportHooker &getFreeHooker()
{
  static DHWImportHooker gFreeHooker(NS_DEBUG_CRT, "free", (PROC) dhw_free);
  return gFreeHooker;
}

void __cdecl dhw_free( void* p )
{
    PRUint32 start = PR_IntervalNow();
    DHW_ORIGINAL(FREE_, getFreeHooker())(p);
    PRUint32 end = PR_IntervalNow();
    FreeCallback(p, start, end);
}


DHW_DECLARE_FUN_TYPE_AND_PROTO(dhw_realloc, void*, __cdecl, REALLOC_, (void*, size_t));
DHWImportHooker &getReallocHooker()
{
  static DHWImportHooker gReallocHooker(NS_DEBUG_CRT, "realloc", (PROC) dhw_realloc);
  return gReallocHooker;
}

void * __cdecl dhw_realloc(void * pin, size_t size)
{
    PRUint32 start = PR_IntervalNow();
    void* pout = DHW_ORIGINAL(REALLOC_, getReallocHooker())(pin, size);
    PRUint32 end = PR_IntervalNow();
    ReallocCallback(pin, pout, size, start, end);
    return pout;
}

// Note the mangled name!
DHW_DECLARE_FUN_TYPE_AND_PROTO(dhw_new, void*, __cdecl, NEW_, (size_t));
DHWImportHooker &getNewHooker()
{
  static DHWImportHooker gNewHooker(NS_DEBUG_CRT, "??2@YAPAXI@Z", (PROC) dhw_new);
  return gNewHooker;
}

void * __cdecl dhw_new(size_t size)
{
    PRUint32 start = PR_IntervalNow();
    void* result = DHW_ORIGINAL(NEW_, getNewHooker())(size);
    PRUint32 end = PR_IntervalNow();
    MallocCallback(result, size, start, end);//do we need a different one for new?
    return result;
}

// Note the mangled name!
DHW_DECLARE_FUN_TYPE_AND_PROTO(dhw_delete, void, __cdecl, DELETE_, (void*));
DHWImportHooker &getDeleteHooker()
{
  static DHWImportHooker gDeleteHooker(NS_DEBUG_CRT, "??3@YAXPAX@Z", (PROC) dhw_delete);
  return gDeleteHooker;
}

void __cdecl dhw_delete(void* p)
{
    PRUint32 start = PR_IntervalNow();
    DHW_ORIGINAL(DELETE_, getDeleteHooker())(p);
    PRUint32 end = PR_IntervalNow();
    FreeCallback(p, start, end);
}

/*C Callbacks*/
PR_IMPLEMENT(void)
StartupHooker()
{
  //run through get all hookers
  DHWImportHooker &loadlibraryW = DHWImportHooker::getLoadLibraryWHooker();
  DHWImportHooker &loadlibraryExW = DHWImportHooker::getLoadLibraryExWHooker();
  DHWImportHooker &loadlibraryA = DHWImportHooker::getLoadLibraryAHooker();
  DHWImportHooker &loadlibraryExA = DHWImportHooker::getLoadLibraryExAHooker();
  DHWImportHooker &mallochooker = getMallocHooker();
  DHWImportHooker &reallochooker = getReallocHooker();
  DHWImportHooker &callochooker = getCallocHooker();
  DHWImportHooker &freehooker = getFreeHooker();
  DHWImportHooker &newhooker = getNewHooker();
  DHWImportHooker &deletehooker = getDeleteHooker();
  printf("Startup Hooker\n");
}

PR_IMPLEMENT(void)
ShutdownHooker()
{
}

extern "C" {
  void* dhw_orig_malloc(size_t);
  void* dhw_orig_calloc(size_t, size_t);
  void* dhw_orig_realloc(void*, size_t);
  void dhw_orig_free(void*);
}

void*
dhw_orig_malloc(size_t size)
{
    return DHW_ORIGINAL(MALLOC_, getMallocHooker())(size);
}

void*
dhw_orig_calloc(size_t count, size_t size)
{
    return DHW_ORIGINAL(CALLOC_, getCallocHooker())(count,size);
}

void*
dhw_orig_realloc(void* pin, size_t size)
{
    return DHW_ORIGINAL(REALLOC_, getReallocHooker())(pin, size);
}

void
dhw_orig_free(void* p)
{
    DHW_ORIGINAL(FREE_, getFreeHooker())(p);
}
