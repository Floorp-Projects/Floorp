#define INCL_PM
#define INCL_GPI
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>
#include <sys/stat.h>

typedef ULONG DWORD;
typedef PSZ LPSTR;
typedef PFNWP WNDPROC;
typedef long HRESULT;
typedef long HFONT;
typedef HMODULE HINSTANCE;
typedef MPARAM WPARAM;
typedef MPARAM LPARAM;
typedef void* HGLOBAL;
typedef HMODULE HANDLE;
typedef long COLORREF;
typedef MRESULT LRESULT;
#define CALLBACK APIENTRY
typedef POWNERITEM LPDRAWITEMSTRUCT;
typedef long HKEY;
typedef UCHAR *LPBYTE;
typedef QMSG MSG;
typedef CHAR TCHAR;

#define WM_INITDIALOG WM_INITDLG
#define IDYES MBID_YES
#define MF_GRAYED 1
#define MF_BYCOMMAND 1
#define SWP_NOSIZE 1
#define IDCANCEL DID_CANCEL
#define LB_SETCURSEL 1
#define WM_SETFONT 1
#define MAX_PATH CCHMAXPATH
#define KEY_CREATE_FOLDER 1


#include "nsINIParser.h"

#ifndef LIBPATHSTRICT
#define LIBPATHSTRICT 3
#endif


