
#include <stdio.h>
#include <stdlib.h>

#include "prtypes.h"
#include "prlock.h"
#include "nscore.h"
#include "nsAutoLock.h"

#include "nsStackFrameWin.h"
#include "nsDebugHelpWin32.h"
#include "nsTraceMallocCallbacks.h"

// XXX These are *very* quick hacks and need improvement!

static PRBool GetSymbolFromAddress(uint32 addr, char* outBuf)
{
    PRBool ok;
    ok = EnsureSymInitialized();
    if(!ok)
        return ok;

    char buf[sizeof(IMAGEHLP_SYMBOL) + 512];
    PIMAGEHLP_SYMBOL symbol = (PIMAGEHLP_SYMBOL) buf;
    symbol->SizeOfStruct = sizeof(buf);
    symbol->MaxNameLength = 512;

    DWORD displacement;

    ok = SymGetSymFromAddr(::GetCurrentProcess(),
                           addr,
                           &displacement,
                           symbol);

    if(ok)
    {
        char buf2[512];
        sprintf(buf2, "%s+0x%08X", symbol->Name, displacement);
        strcat(outBuf, buf2);
    }
    else
        strcat(outBuf, "dunno");
    return ok;
}

static PRBool GetModuleFromAddress(uint32 addr, char* outBuf)
{
    PRBool ok;
    ok = EnsureSymInitialized();
    if(!ok)
        return ok;


    IMAGEHLP_MODULE module;
    module.SizeOfStruct = sizeof(IMAGEHLP_MODULE);

    ok = SymGetModuleInfo(::GetCurrentProcess(),
                          addr,
                          &module);

    if(ok)
        strcat(outBuf, module.ModuleName);
    else
        strcat(outBuf, "dunno");
    return ok;
}

/***************************************************************************/
#ifdef VERBOSE
#define SHOW(x_, buf_) \
    printf(#x_" = %#x... %s\n", x_,             \
    (buf_[0] = 0,                               \
     GetModuleFromAddress((uint32)x_, buf_),    \
     strcat(buf," :: "),                        \
     GetSymbolFromAddress((uint32)x_, buf_),    \
     buf_));
#else
#define SHOW(x_, buf_)
#endif  //VERBOSE


/***************************************************************************/

#ifdef VERBOSE
// XXX This is a quick hack to show that x86 Win32 stack walking can be done
// with this sort of loop following the bp. 

void dumpStack()
{
    uint32* bp_;
    uint32* bpdown;
    uint32  pc;

    char buf[512];

    _asm { mov bp_ , ebp }

    /* Stack walking code adapted from Kipp's "leaky". */
    while (1) {
        bpdown = (uint32*) *bp_++;
        pc = *bp_;
        // These addresses are iffy...
        if (pc < 0x00400000 || pc > 0x7fffffff || bpdown < bp_)
            break;
        SHOW(pc, buf);
        bp_ = bpdown;
    }
    printf("\n");
}
#endif

char* _stdcall call2(void* v)
{
//    dumpStack();
//    return 0; 

    return (char *)malloc(123);   
}

int call1(char c, int i, double d, ... )
{
    free(call2(0));
    return 0;    
}

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

static BOOL g_lockOut = FALSE; //stop reentrancy

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
    if (g_lockOut)
      return result;
    g_lockOut = TRUE;
#ifdef VERBOSE
    printf("* malloc called to get %d bytes. returned %#x\n", size, result);
#endif
    MallocCallback(result, size, start, end);
//    dumpStack(); 
//    printf("\n");
    g_lockOut = FALSE;
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
    if (g_lockOut)
      return result;
    g_lockOut = TRUE;
#ifdef VERBOSE
    printf("* calloc called to get %d many of %d bytes. returned %#x\n", count, size, result);
#endif
    CallocCallback(result, count, size, start, end);
//    dumpStack(); 
//    printf("\n");
    g_lockOut = FALSE;
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
    if (g_lockOut)
      return;
    g_lockOut = TRUE;
#ifdef VERBOSE
    printf("* free called for %#x\n", p);
#endif
    FreeCallback(p, start, end);
//    dumpStack(); 
//    printf("\n");
    g_lockOut = FALSE;
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
    if (g_lockOut)
      return pout;
    g_lockOut = TRUE;

#ifdef VERBOSE
    printf("* realloc called to resize to %d. old ptr: %#x. new ptr: %#x\n", 
           size, pin, pout);
#endif
    ReallocCallback(pin, pout, size, start, end);
//    dumpStack(); 
//    printf("\n");
    g_lockOut = FALSE;
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
    if (g_lockOut)
      return result;
    g_lockOut = TRUE;

#ifdef VERBOSE
    printf("* new called to get %d bytes. returned %#x\n", size, result);
    dumpStack(); 
#endif
    MallocCallback(result, size, start, end);//do we need a different one for new?
//    printf("\n");
    g_lockOut = FALSE;
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
    if (g_lockOut)
      return;
    g_lockOut = TRUE;
#ifdef VERBOSE
    printf("* delete called for %#x\n", p);
    dumpStack(); 
#endif
    FreeCallback(p, start, end);
//    printf("\n");
    g_lockOut = FALSE;
}




/***************************************************************************/
// A demonstration of using the _CrtSetAllocHook based hooking.
// This system sucks because you don't get to see the allocated pointer.
#if 0
class myAllocationSizePrinter : public DHWAllocationSizeDebugHook
{
public:
    PRBool AllocHook(size_t size)
    {
        alloc_calls++ ;
        total_mem += size;
        if(verbosity)
        {
            printf("alloc called to get %d bytes.\n", size);
            dumpStack();
        }
        return PR_TRUE;
    }

    PRBool ReallocHook(size_t size, size_t sizeOld)
    {
        realloc_calls++ ;
        total_mem += size;
        total_mem -= sizeOld;
        if(verbosity)
        {
            printf("realloc called to size to %d bytes. Old size: %d.\n",
                   size, sizeOld);
            dumpStack();
        }
        return PR_TRUE;
    }

    PRBool FreeHook(size_t size)
    {
        free_calls++ ;
        total_mem -= size;
        if(verbosity)
        {
            printf("free called to release %d bytes.\n", size);
            dumpStack();
        }
        return PR_TRUE;
    }

    myAllocationSizePrinter(int v)
        : verbosity(v),
          alloc_calls(0),
          realloc_calls(0),
          free_calls(0),
          total_mem(0) {}
    virtual ~myAllocationSizePrinter(){}

    void report()
    {
        printf("%d allocs, %d reallocs, %d frees, %d bytes leaked\n",
               alloc_calls, realloc_calls, free_calls, total_mem);
    }

private:
    void dumpStack()
    {
        if(verbosity == 2)
            ::dumpStack();
    }
 
    int verbosity;
    int alloc_calls;
    int realloc_calls;
    int free_calls;
    size_t total_mem;
};
#endif




/*C Callbacks*/
PR_IMPLEMENT(void)
StartupHooker()
{
  if (!EnsureSymInitialized())
    return;

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



#if 0

int main()
{
    // A demonstration of using the (sucky) _CrtSetAllocHook based hooking.
    myAllocationSizePrinter ap(0);
    dhwSetAllocationSizeDebugHook(&ap);

    // show that the ImportHooker is hooking calls from loaded dll
    DHW_DECLARE_FUN_TYPE(void, __stdcall, SOMECALL_, (void));
    HMODULE module = ::LoadLibrary("Other.dll");
    if(module) {
        SOMECALL_ _SomeCall = (SOMECALL_) GetProcAddress(module, "SomeCall");
        if(_SomeCall)
            _SomeCall(); 
    }

    // show that the ImportHooker is hooking sneaky calls made from this dll.
    HMODULE module2 = ::LoadLibrary(NS_DEBUG_CRT);
    if(module2) {
        MALLOC_ _sneakyMalloc = (MALLOC_) GetProcAddress(module2, "malloc");
        if(_sneakyMalloc)
        {
            void* p = _sneakyMalloc(987); 
            free(p);
        }
    }

    call1('x', 1, 1.0, "hi there", 2);

    char* p = new char[10];
    delete p;

    void* v = malloc(10);
    v = realloc(v, 15);
    v = realloc(v, 5);
    free(v);`

    ap.report();
    dhwClearAllocationSizeDebugHook();
    return 0;
}
#endif //0

