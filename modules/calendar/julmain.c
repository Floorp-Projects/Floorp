#ifdef XP_PC 
#include <windows.h>

#ifndef MOZ_TREX
#include "nlsloc.h"
#endif

#ifdef _WIN32
BOOL WINAPI DllMain (HINSTANCE hDLL, DWORD dwReason, LPVOID lpReserved)
{
  switch (dwReason)
  {
    case DLL_PROCESS_ATTACH:
      break;

    case DLL_THREAD_ATTACH:
      break;

    case DLL_THREAD_DETACH:
      break;

    case DLL_PROCESS_DETACH:

#ifndef MOZ_TREX
        NLS_Terminate();
#endif
      break;
  }

    return TRUE;
}

#else  /* ! _WIN32 */

int CALLBACK LibMain( HINSTANCE hInst, WORD wDataSeg, 
                      WORD cbHeapSize, LPSTR lpszCmdLine )
{
    return TRUE;
}

BOOL CALLBACK __loadds WEP(BOOL fSystemExit)
{
    return TRUE;
}
#endif /* ! _WIN32 */


#endif    /* XP_PC */
