#ifndef nlsloc_h__
#define nlsloc_h__

#include "ptypes.h"

typedef PRUint32 NLS_ThreadInfo;
typedef PRUint32 NLS_ErrorCode;

NLS_ErrorCode NLS_Initialize(const NLS_ThreadInfo * aThreadInfo, const char * aDataDirectory);
NLS_ErrorCode NLS_Terminate(void);

#endif
