#ifndef NSTRACEMALLOCCALLBACKS_H
#define NSTRACEMALLOCCALLBACKS_H

PR_BEGIN_EXTERN_C


PR_EXTERN(void) StartupHooker();/*implemented in TraceMalloc.cpp*/
PR_EXTERN(void) ShutdownHooker();

PR_EXTERN(void) MallocCallback(void *aPtr, size_t aSize, PRUint32 start, PRUint32 end);/*implemented in nsTraceMalloc.c*/
PR_EXTERN(void) CallocCallback(void *aPtr, size_t aCount, size_t aSize, PRUint32 start, PRUint32 end);
PR_EXTERN(void) ReallocCallback(void *aPin, void* aPout, size_t aSize, PRUint32 start, PRUint32 end);
PR_EXTERN(void) FreeCallback(void *aPtr, PRUint32 start, PRUint32 end);


PR_END_EXTERN_C


#endif //NSTRACEMALLOCCALLBACKS_H

