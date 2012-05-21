/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Splash screen dialog ID.
#define IDD_SPLASH  100

// Splash screen bitmap ID.
#define IDB_SPLASH  101

// DDE application name
#define ID_DDE_APPLICATION_NAME 102

/* 
 * OS/2 DDEML library headers
 * For more information, please refer to the Windows documentation
 */

#ifndef _H_DDEML
#define _H_DDEML

/* all structures must be byte aligned */
#pragma pack(1)

/* custom type definitions */
typedef LHANDLE HCONV;
typedef LHANDLE HCONVLIST;
typedef LHANDLE HDATA;
typedef LHANDLE HDDEINST;
typedef LHANDLE HSZ;
typedef HCONV *PHCONV;
typedef HCONVLIST *PHCONVLIST;
typedef HDATA *PHDATA;
typedef HDDEINST *PHDDEINST;
typedef HSZ *PHSZ;
typedef HDATA (APIENTRY FNDDECB)(ULONG, USHORT, USHORT, HCONV, HSZ, HSZ, HDATA, ULONG, ULONG);
typedef FNDDECB *PFNDDECB;

/* constant definitions */
#define XCLASS_BOOL 0x1000
#define XCLASS_DATA 0x2000
#define XCLASS_FLAGS 0x4000
#define XCLASS_NOTIFICATION 0x8000
#define XTYPF_NOBLOCK 0x0002
#define XTYPF_NODATA 0x0004
#define XTYPF_ACKREQ 0x0008
#define XTYP_ERROR (0x0000 | XCLASS_NOTIFICATION | XTYPF_NOBLOCK)
#define XTYP_ADVDATA (0x0010 | XCLASS_FLAGS)
#define XTYP_ADVREQ (0x0020 | XCLASS_DATA | XTYPF_NOBLOCK)
#define XTYP_ADVSTART (0x0030 | XCLASS_BOOL)
#define XTYP_ADVSTOP (0x0040 | XCLASS_NOTIFICATION)
#define XTYP_EXECUTE (0x0050 | XCLASS_FLAGS)
#define XTYP_CONNECT (0x0060 | XCLASS_BOOL | XTYPF_NOBLOCK)
#define XTYP_CONNECT_CONFIRM (0x0070 | XCLASS_NOTIFICATION | XTYPF_NOBLOCK)
#define XTYP_XACT_COMPLETE (0x0080 | XCLASS_NOTIFICATION)
#define XTYP_POKE (0x0090 | XCLASS_FLAGS)
#define XTYP_REGISTER (0x00A0 | XCLASS_NOTIFICATION | XTYPF_NOBLOCK)
#define XTYP_REQUEST (0x00B0 | XCLASS_DATA)
#define XTYP_DISCONNECT (0x00C0 | XCLASS_NOTIFICATION | XTYPF_NOBLOCK)
#define XTYP_UNREGISTER (0x00D0 | XCLASS_NOTIFICATION | XTYPF_NOBLOCK)
#define XTYP_WILDCONNECT (0x00E0 | XCLASS_DATA | XTYPF_NOBLOCK)
#define XTYP_MONITOR (0x00F0 | XCLASS_NOTIFICATION | XTYPF_NOBLOCK)
#define ST_CONNECTED ((USHORT)0x0001)
#define ST_ADVISE ((USHORT)0x0002)
#define ST_ISLOCAL ((USHORT)0x0004)
#define ST_BLOCKED ((USHORT)0x0008)
#define ST_CLIENT ((USHORT)0x0010)
#define ST_TERMINATED ((USHORT)0x0020)
#define ST_INLIST ((USHORT)0x0040)
#define ST_BLOCKNEXT ((USHORT)0x0080)
#define ST_ISSELF ((USHORT)0x0100)
#define XST_NULL 0
#define XST_INCOMPLETE 1
#define XST_CONNECTED 2
#define XST_INITSENT 3
#define XST_INITACKRCVD 4
#define XST_REQSENT 5
#define XST_DATARCVD 6
#define XST_POKESENT 7
#define XST_POKEACKRCVD 8
#define XST_EXECSENT 9
#define XST_EXECACKRCVD 10
#define XST_ADVSENT 11
#define XST_UNADVSENT 12
#define XST_ADVACKRCVD 13
#define XST_UNADVACKRCVD 14
#define XST_ADVDATASENT 15
#define XST_ADVDATAACKRCVD 16
#define MF_HSZ ((ULONG)0x01000000L)
#define MF_SENDMSGS ((ULONG)0x02000000L)
#define MF_POSTMSGS ((ULONG)0x04000000L)
#define MF_CALLBACKS ((ULONG)0x08000000L)
#define MF_ERRORS ((ULONG)0x10000000L)
#define MF_LINKS ((ULONG)0x20000000L)
#define MF_CONV ((ULONG)0x40000000L)
#define CBF_FAIL_SELFCONNECTIONS ((ULONG)0x00001000L)
#define CBF_FAIL_CONNECTIONS ((ULONG)0x00002000L)
#define CBF_FAIL_ADVISES ((ULONG)0x00004000L)
#define CBF_FAIL_EXECUTES ((ULONG)0x00008000L)
#define CBF_FAIL_POKES ((ULONG)0x00010000L)
#define CBF_FAIL_REQUESTS ((ULONG)0x00020000L)
#define CBF_FAIL_ALLSVRXACTIONS ((ULONG)0x0003f000L)
#define CBF_SKIP_CONNECT_CONFIRMS ((ULONG)0x00040000L)
#define CBF_SKIP_REGISTRATIONS ((ULONG)0x00080000L)
#define CBF_SKIP_UNREGISTRATIONS ((ULONG)0x00100000L)
#define CBF_SKIP_DISCONNECTS ((ULONG)0x00200000L)
#define CBF_SKIP_ALLNOTIFICATIONS ((ULONG)0x003c0000L)
#define CBF_MASK ((ULONG)0x00FFF000L)
#define APPF_CLIENTONLY ((ULONG)0x00000010L)
#define APPF_FILTERINITS ((ULONG)0x00000020L)
#define APPF_MASK ((ULONG)0x00000FF0L)
#define APPCLASS_STANDARD ((ULONG)0x00000000L)
#define APPCLASS_MONITOR ((ULONG)0x00000001L)
#define APPCLASS_MASK ((ULONG)0x0000000FL)
#define CBR_BLOCK ((ULONG)-1L)
#define QID_SYNC ((ULONG)-1L)
#define TIMEOUT_ASYNC ((ULONG)-1L)
#define CADV_LATEACK 0xFFFF
#define MH_CREATE ((USHORT)0x0001)
#define MH_KEEP ((USHORT)0x0002)
#define MH_DELETE ((USHORT)0x0003)
#define MH_CLEANUP ((USHORT)0x0004)
#define MH_SYS_CREATE ((USHORT)0x0005)
#define MH_SYS_KEEP ((USHORT)0x0006)
#define MH_SYS_DELETE ((USHORT)0x0007)
#define DDEERR_NO_ERROR 0
#define DDEERR_FIRST 0x7000
#define DDEERR_BUSY 0x7001
#define DDEERR_INVALID_USAGE 0x7002
#define DDEERR_INVALID_PARAMETER 0x7003
#define DDEERR_MEMORY_ERROR 0x7004
#define DDEERR_NO_CONV_ESTABLISHED 0x7005
#define DDEERR_NO_MEMORY 0x7006
#define DDEERR_NO_MSG_QUEUE 0x7007
#define DDEERR_NOT_INITIALIZED 0x7008
#define DDEERR_NOT_PROCESSED 0x7009
#define DDEERR_POSTMSG_FAILED 0x700a
#define DDEERR_REENTRANCY 0x700b
#define DDEERR_SERVER_DIED 0x700c
#define DDEERR_SYSTEM_ERROR 0x700d
#define DDEERR_TIMEOUT_ADVACK 0x700e
#define DDEERR_TIMEOUT_EXECACK 0x700f
#define DDEERR_TIMEOUT_POKEACK 0x7010
#define DDEERR_TIMEOUT_DATAACK 0x7011
#define DDEERR_TIMEOUT_UNADVACK 0x7012
#define DDEERR_UNKNOWN_QUEUE_ID 0x7013
#define DDEERR_LAST 0x70FF
#define CSH_CASESENSITIVE ((ULONG)0x00000001L)
#define CSH_ERROR 0L
#define CSH_EQ 1L
#define CSH_LT 2L
#define CSH_GT 3L
#define HDATA_APPOWNED ((ULONG)DDEPM_NOFREE)
#define EC_ENABLEALL ((USHORT) 0x0000)
#define EC_ENABLEONE ST_BLOCKNEXT
#define EC_DISABLE ST_BLOCKED
#define EC_QUERYWAITING ((USHORT)0x0002)
#define DNS_REGISTER ((ULONG)0x00000001L)
#define DNS_UNREGISTER ((ULONG)0x00000002L)
#define DNS_FILTERON ((ULONG)0x00000004L)
#define DNS_FILTEROFF ((ULONG)0x00000008L)

/* structure definitions */
typedef struct
{
   ULONG       cb;
   ULONG       hUser;
   HCONV       hConvPartner;
   HSZ         hszServicePartner;
   HSZ         hszServiceRequest;
   HSZ         hszTopic;
   HSZ         hszItem;
   USHORT      usFormat;
   USHORT      fsType;
   USHORT      fsStatus;
   USHORT      usState;
   ULONG       ulLastError;
   HCONVLIST   hConvList;
   CONVCONTEXT ConvCtxt;
   HWND        hwnd;
   HWND        hwndPartner;
} CONVINFO, *PCONVINFO;

typedef struct
{
   HSZ hszService;
   HSZ hszTopic;
} HSZPAIR, *PHSZPAIR;

typedef struct
{
   ULONG       cb;
   ULONG       ulTime;
   ULONG       hTask;
   ULONG       ulRet;
   USHORT      fsType;
   USHORT      usFormat;
   HCONV       hConv;
   HSZ         hsz1;
   HSZ         hsz2;
   HDATA       hData;
   ULONG       ulData1;
   ULONG       ulData2;
   CONVCONTEXT ConvCtxt;
   ULONG       cbData;
   BYTE        abData[32];
} MONCBSTRUCT, *PMONCBSTRUCT;

typedef struct
{
   ULONG cb;
   BOOL  fConnect;
   ULONG ulTime;
   ULONG hTaskPartner;
   HSZ   hszService;
   HSZ   hszTopic;
   HCONV hConvClient;
   HCONV hConvServer;
} MONCONVSTRUCT, *PMONCONVSTRUCT;

typedef struct
{
   ULONG cb;
   ULONG ulLastError;
   ULONG ulTime;
   ULONG hTask;
} MONERRSTRUCT, *PMONERRSTRUCT;

typedef struct
{
   ULONG  cb;
   USHORT fsAction;
   ULONG  ulTime;
   HSZ    hsz;
   ULONG  hTask;
   ULONG  ulReserved;
   CHAR    szString[1];
} MONHSZSTRUCT, *PMONHSZSTRUCT;

typedef struct
{
   ULONG  cb;
   ULONG  ulTime;
   ULONG  hTask;
   HSZ    hszService;
   HSZ    hszTopic;
   HSZ    hszItem;
   HCONV  hConvServer;
   HCONV  hConvClient;
   BOOL   fServer;
   BOOL   fEstablished;
   USHORT fsStatus;
   USHORT usFormat;
} MONLINKSTRUCT, *PMONLINKSTRUCT;

typedef struct
{
   ULONG  cb;
   HWND   hwndTo;
   ULONG  ulTime;
   ULONG  hTask;
   ULONG  idMsg;
   MPARAM mp1;
   MPARAM mp2;
   USHORT fsStatus;
   USHORT usFormat;
   USHORT offszString1;
   USHORT offszString2;
   ULONG  cbData;
   BYTE   abData[32];
} MONMSGSTRUCT, *PMONMSGSTRUCT;


/* API definitions */
BOOL (* APIENTRY WinDdeAbandonTransaction)(HDDEINST, HCONV, ULONG);
PVOID (* APIENTRY WinDdeAccessData)(HDATA, PULONG);
HDATA (* APIENTRY WinDdeAddData)(HDATA, PVOID, ULONG, ULONG);
ULONG (* APIENTRY WinDdeCompareStringHandles)(HSZ, HSZ, ULONG);
HCONV (* APIENTRY WinDdeConnect)(HDDEINST, HSZ, HSZ, PCONVCONTEXT);
HCONVLIST (* APIENTRY WinDdeConnectList)(HDDEINST, HSZ, HSZ, HCONVLIST, PCONVCONTEXT);
HDATA (* APIENTRY WinDdeCreateDataHandle)(PVOID, ULONG, ULONG, HSZ, USHORT, ULONG);
HSZ (* APIENTRY WinDdeCreateStringHandle)(PSZ, ULONG);
BOOL (* APIENTRY WinDdeDisconnect)(HCONV);
BOOL (* APIENTRY WinDdeDisconnectList)(HCONVLIST);
BOOL (* APIENTRY  WinDdeEnableCallback)(HDDEINST, HCONV, ULONG);
BOOL (* APIENTRY WinDdeFreeDataHandle)(HDATA);
BOOL (* APIENTRY WinDdeFreeStringHandle)(HSZ);
ULONG (* APIENTRY WinDdeGetData)(HDATA, PVOID, ULONG, ULONG);
ULONG (* APIENTRY WinDdeInitialize)(PHDDEINST, PFNDDECB, ULONG, ULONG);
BOOL (* APIENTRY WinDdeKeepStringHandle)(HSZ);
HDATA (* APIENTRY WinDdeNameService)(HDDEINST, HSZ, HSZ, ULONG);
BOOL (* APIENTRY WinDdePostAdvise)(HDDEINST, HSZ, HSZ);
ULONG (* APIENTRY WinDdeQueryConvInfo)(HCONV, ULONG, PCONVINFO);
HCONV (* APIENTRY WinDdeQueryNextServer)(HCONVLIST, HCONV);
ULONG (* APIENTRY WinDdeQueryString)(HSZ, PSZ, ULONG, ULONG);
HCONV (* APIENTRY WinDdeReconnect)(HCONV);
BOOL (* APIENTRY WinDdeSetUserHandle)(HCONV, ULONG, ULONG);
HDATA (* APIENTRY WinDdeSubmitTransaction)(PVOID, ULONG, HCONV, HSZ, USHORT, USHORT, ULONG, PULONG);
BOOL (* APIENTRY WinDdeUninitialize)(HDDEINST);

/* restore structure packing */
#pragma pack()

#endif /* _H_DDEML */

typedef ULONG DWORD;
typedef PBYTE LPBYTE;
typedef HDATA HDDEDATA;

#define CP_WINANSI    0  //  When 0 is specified for codepage on these
                         //  dde functions, it will use the codepage
                         //  that is associated with the current thread.
                         //  CP_WINANSI in win32 means that the non
                         //  unicode version of DdeCreateStringHandle
                         //  was used.
