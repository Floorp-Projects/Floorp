#ifndef _sdinst_h
#define _sdinst_h

#include <ConditionalMacros.h>
#include <MacTypes.h>
#include <Quickdraw.h>
#include <MixedMode.h>
#include <Events.h>

#if PRAGMA_ONCE
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if PRAGMA_IMPORT
#pragma import on
#endif

#if PRAGMA_STRUCT_ALIGN
	#pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
	#pragma pack(2)
#endif

#define	DEFAULT_TIMEOUT		60
#define DEFAULT_RETRIES		5

typedef enum
{
	idle = 0,
	inProgress,
	userCanceled,
	failure,
	success
} DLStatus;

#ifdef WANT_WINTYPES
	typedef unsigned long DWORD;
	typedef int HINSTANCE;
	#define CONST const
	typedef char CHAR;
	typedef CONST CHAR* LPCSTR;
	typedef LPCSTR LPCTSTR;
	typedef long LONG;
	typedef LONG LPARAM;
	typedef void* LPVOID;
	typedef long* LPLONG;
	#define _cdecl
	typedef long HRESULT;
	typedef int HWND;
#endif

typedef struct SDI_tag
{
	DWORD		dwStructSize;
	HWND		hwndOwner;
	HINSTANCE	hInstance;
	DWORD		dwFlags;		// see flag definitions below
	DWORD		dwTimeOut;		// timeout for network operations
	DWORD		dwRetries;		// how many times to retry downloading from a URL
#if macintosh
	FSSpec		fsIDIFile;		// The IDI file
	short		dlDirVRefNum;	// where to download components
	long		dlDirID;		// ditto
#else
	LPCTSTR		lpFileName;		// the name of the IDI file
	LPCTSTR		lpDownloadDir;	// where to download components
#endif
	LPCTSTR		lpTemplate;		// not implemented
#if 0 // later ###
	DLGPROC		lpDlgHookProc;	// not implemented
#endif
	LPARAM		lUserData;		// not implemented
	DWORD		dwReserved;
	
} SDISTRUCT, *LPSDISTRUCT;

// Flags

#define SDI_STARTOVER		0x00000001
	/* Force to start over, even if an interrupted session is detected.
	 */

#define SDI_USETEMPLATE		0x00000002
	/* If this flag is set, hInstance identifies a module that contains
	 * a dialog box template named by the lpTemplateName member.
	 * If silent mode is specified in the IDI file, the flag is ignored. 
	 */

#define SDI_USEHOOK			0x00000004
	/* The hook procedure receives notification messages sent from the dialog.
	 * The hook procedure also receives messages for any additional controls
	 * defined by specifying a child dialog template. The hook procedure does 
	 * not receive messages intended for the standard controls of the default
	 * dialog box. 
	 */

#define SDI_DEFAULT_RETRIES	0x00000008

#define SDI_DEFAULT_TIMEOUT	0x00000010



HRESULT _cdecl SDI_NetInstall (LPSDISTRUCT);
HRESULT _cdecl SDI_QueryLog (	LPCTSTR,/* item name to query					*/
								LPVOID,	/* address of buffer for returned info	*/
								LPLONG);/* address of buffer size				*/

#if MACINTOSH
Boolean SDI_HandleEvent(const EventRecord* event);
#endif

#if PRAGMA_STRUCT_ALIGN
	#pragma options align=reset
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(pop)
#elif PRAGMA_STRUCT_PACK
	#pragma pack()
#endif

#ifdef PRAGMA_IMPORT_OFF
#pragma import off
#elif PRAGMA_IMPORT
#pragma import reset
#endif

#ifdef __cplusplus
}
#endif

#endif /* _sdinst_h */
