/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Remote Workplace Server - Project headers.
 *
 * The Initial Developer of the Original Code is Richard L. Walsh.
 * 
 * Portions created by the Initial Developer are Copyright (C) 2004-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/****************************************************************************/
/* RWS - beta version 0.80                                                  */
/****************************************************************************/

// RWS.H
// Remote Workplace Server - main header

/****************************************************************************/

#ifndef RWS_H_INCLUDED
    #define RWS_H_INCLUDED

#ifdef __cplusplus
    extern "C" {
#endif

/****************************************************************************/
/* Server Request Structures                                                */
/****************************************************************************/

// These structures are created & initialized in shared memory by
// RwsClient and are passed to RwsServer.  On return, the client app
// can access any data they contain.

// Every request to RwsServer starts with one of these
typedef struct _RWSHDR
{
    ULONG       Size;       // total size of the request block
    USHORT      MsgId;      // used by RwsDispatch
    USHORT      Flags;      // used by RwsDispatch & RwsFreeMem
    struct _RWSHDR * PrevHdr;    // not used
    USHORT      Rc;         // return code from server
    USHORT      Cnt;        // argument count
    ULONG       Id;         // for use by application
} RWSHDR;
typedef RWSHDR* PRWSHDR;

// Every request to RwsServer has at least two of these in a linked list.
// The first immediately follows RWSHDR and describes the type of call;
// the second describes the return value.  Additional structs describe
// each of the arguments;  the last in the chain has a zero in pnext.
// Note:  in case you were wondering, "DSC" is short for descriptor
typedef struct _RWSDSC
{
    struct _RWSDSC *pnext;
    ULONG       value;      // data actually passed to or returned by procedure
    ULONG       type;       // describes how RWS should handle this argument
    USHORT      flags;      // currently, identifies this as an arg/rtn/proc
    USHORT      rc;         // error code generated while handling this item
    USHORT      cbgive;     // size of the give (input) buffer
    USHORT      cbget;      // size of the get (output) buffer
    VOID *      pget;       // pointer to the get buffer
} RWSDSC;                   // note:  the give buffer immediately follows
typedef RWSDSC* PRWSDSC;    // this structure (i.e. it's at &RWSDSC[1])


/****************************************************************************/
/* Client Request Structures                                                */
/****************************************************************************/

// These structures describe the layout of the stack when an app calls
// RwsCall or RwsBuild.  They can be used with RwsCallIndirect and
// RwsBuildIndirect to avoid pushing a large number of arguments onto
// the stack

// header structure - required
typedef struct _RWSBLD
{
    PRWSHDR*    ppHdr;
    ULONG       callType;
    ULONG       callValue;
    ULONG       rtnType;
    ULONG       rtnSize;
    ULONG       argCnt;
} RWSBLD;
typedef RWSBLD* PRWSBLD;

// argument structures - as needed
typedef struct _RWSARG
{
    ULONG       type;
    ULONG       size;
    ULONG       value;
} RWSARG;
typedef RWSARG* PRWSARG;


/****************************************************************************/
/* Exported Procedures used by Clients                                      */
/****************************************************************************/

// These functions' va list is equivalent to an array of zero or more
// RWSARG structures (i.e. {ULONG type, ULONG size, ULONG value}...)

ULONG   _System RwsCall( PRWSHDR* ppHdr, ULONG callType, ULONG callValue,
                         ULONG rtnType,  ULONG rtnSize,  ULONG argCnt, ...);
ULONG   _System RwsCallIndirect( PRWSBLD pBld);
ULONG   _System RwsCallAsync( HWND hwnd, PRWSHDR* ppHdr,
                              ULONG callType, ULONG callValue,
                              ULONG rtnType,  ULONG rtnSize,
                              ULONG argCnt, ...);
ULONG   _System RwsCallIndirectAsync( HWND hwnd, PRWSBLD pBld);
ULONG   _System RwsBuild( PRWSHDR* ppHdr, ULONG callType, ULONG callValue,
                          ULONG rtnType,  ULONG rtnSize,  ULONG argCnt, ...);
ULONG   _System RwsBuildIndirect( PRWSBLD pBld);
ULONG   _System RwsQueryVersion( PULONG pulReserved);
ULONG   _System RwsClientInit( BOOL fRegister);
ULONG   _System RwsClientTerminate( void);
ULONG   _System RwsDispatch( PRWSHDR pHdr);
ULONG   _System RwsDispatchStatus( PBOOL pfReady);
ULONG   _System RwsCancelDispatch( HWND hNotify, ULONG idNotify);
ULONG   _System RwsNotify( HWND hNotify, ULONG idNotify);
ULONG   _System RwsDispatchAsync( HWND hwnd, PRWSHDR pHdr);
ULONG   _System RwsFreeMem( PRWSHDR pHdr);
ULONG   _System RwsGetResult( PRWSHDR pHdr, ULONG ndxArg, PULONG pSize);
ULONG   _System RwsGetArgPtr( PRWSHDR pHdr, ULONG ndxArg, PRWSDSC* ppArg);
ULONG   _System RwsGetServerMsgID( PULONG pulMsgID);
ULONG   _System RwsGetRcString( ULONG rc, ULONG cbBuf, char * pszBuf);
ULONG   _System RwsGetTimeout( PULONG pulSecs, PULONG pulUser);
ULONG   _System RwsSetTimeout( ULONG ulSecs);


/****************************************************************************/

// == Ordinals for functions exported by RWSCLIxx ==

#define ORD_RWSCLIENTINIT           2
#define ORD_RWSCALL                 3
#define ORD_RWSCALLINDIRECT         4
#define ORD_RWSCALLASYNC            5
#define ORD_RWSCALLINDIRECTASYNC    6
#define ORD_RWSBUILD                7
#define ORD_RWSBUILDINDIRECT        8
#define ORD_RWSDISPATCH             9
#define ORD_RWSDISPATCHASYNC        10
#define ORD_RWSFREEMEM              11
#define ORD_RWSGETRESULT            12
#define ORD_RWSGETARGPTR            13
#define ORD_RWSGETSERVERMSGID       14
#define ORD_RWSGETRCSTRING          15
#define ORD_RWSGETTIMEOUT           16
#define ORD_RWSSETTIMEOUT           17
#define ORD_RWSCLIENTTERMINATE      18
#define ORD_RWSQUERYVERSION         19
#define ORD_RWSDISPATCHSTATUS       20
#define ORD_RWSCANCELDISPATCH       21
#define ORD_RWSNOTIFY               22
                         
/****************************************************************************/

// == RwsCommand values ==

#define RWSCMD_POPUPMENU    1
#define RWSCMD_OPEN         2
#define RWSCMD_LOCATE       3
#define RWSCMD_LISTOBJECTS  4
#define RWSCMD_OPENUSING    5
#define RWSCMD_DELETE       6

#define RWSCMD_FIRST        1
#define RWSCMD_LAST         6

// == Flags used by specific commands ==

#define LISTOBJ_STANDARD    0x00000000     // abstract & transient objects
#define LISTOBJ_FILESHADOWS 0x00000001     // handles of shadowed files
#define LISTOBJ_ALL         0x00000002     // all objects - NOT recommended
#define LISTOBJ_MASK        0x00000003
#define LISTOBJ_OVERFLOW    0x80000000     // buffer overflow

// == WM_CONTROL Notifications used with RwsNotify() ==

// WM_CONTROL msgs will be posted to hNotify in this format:
//  mp1 = MPFROM2SHORT( idNotify, RWSN_*)
//  mp2 - depends on RWSN_ code:
//          RWSN_ENTER      mp2 = 0
//          RWSN_EXIT       mp2 = rc returned by RwsDispatch()
//          RWSN_BLOCKED    mp2 = 0
//          RWSN_CANCEL     mp2 = (RwsDispatch will now exit) ? TRUE : FALSE

#define RWSN_ENTER          1
#define RWSN_EXIT           2
#define RWSN_BLOCKED        3
#define RWSN_CANCEL         4

/****************************************************************************/
/* Macros & #defines                                                        */
/****************************************************************************/

// == Name & Version stuff ==

#define RWSVERSION      "0.80"                  // current version for text
#define RWSVERSIONNBR   "08"                    // current version for files

// RWSFULLVERSION for RWS v0.80 GA, oo v1.1, FPos v0.80, & Iconomize v0.80
// is 0x08000100
#define RWSFULLVERSION  0x08000100              // fussy version

#define RWSSRVSTEM      "RWSSRV"                // server name stem
#define RWSCLISTEM      "RWSCLI"                // client name stem
#define RWSCLASSSTEM    "RWS"                   // WPS class name stem

#define RWSSRVNAME      RWSSRVSTEM RWSVERSIONNBR    // server name
#define RWSCLINAME      RWSCLISTEM RWSVERSIONNBR    // client name
#define RWSCLASSNAME    RWSCLASSSTEM RWSVERSIONNBR  // class name

#define RWSSRVMOD       RWSSRVNAME              // server module name
#define RWSCLIMOD       RWSCLINAME              // client module name
#define RWSSRVDLL       RWSSRVMOD ".DLL"        // server dll name
#define RWSCLIDLL       RWSCLIMOD ".DLL"        // client dll name


/****************************************************************************/

// == Address macros ==

// use these to calculate the address of a structure or buffer
// which immediately follows the referenced structure

// takes a ptr to RWSHDR, returns a ptr to the RWSDSC for the procedure
#define CALCPROCPTR(p)      ((PRWSDSC)(PVOID)(&((PRWSHDR)p)[1]))

// takes a ptr to RWSHDR, returns a ptr to the RWSDSC for the return value
#define CALCRTNPTR(p)       (CALCPROCPTR(p)->pnext)

// takes a ptr to RWSDSC, returns a ptr to the give buffer which follows it
#define CALCGIVEPTR(p)      ((PBYTE)(PVOID)(&((PRWSDSC)p)[1]))

// takes a ptr to RWSBLD, returns a ptr to the first RWSARG following it
#define CALCARGPTR(p)        ((PRWSARG)(PVOID)(&((PRWSBLD)p)[1]))


/****************************************************************************/

// == Error Handler macros ==

// error macros that make it easy to break out of a function bracketed by
// a do{...}while(FALSE) loop - they assume 'rc' has already been defined

// used to set an error code
#define ERRNBR(x)   {rc = (x); break;}

// used to evaluate the error code returned by a function
#define ERRRTN(x)   {rc = (x); if (rc) break;}


/****************************************************************************/

// == Miscellanea ==

#define RWSMAXARGS      16          // this only applies to methods & functions
#define RWSSTRINGSIZE   CCHMAXPATH  // the default string buffer size
#define RWSMEMSIZE      0x10000     // the amount of gettable shared
                                    // memory each client allocates

// arranges character sequences so they're recognizable
// when viewed as a DWORD (e.g. 'INIT' becomes 0x494e4954)
#define READABLEULONG(a,b,c,d) ((ULONG)((((ULONG)(a))<<24) | \
                                        (((ULONG)(b))<<16) | \
                                        (((ULONG)(c))<<8) | \
                                         ((ULONG)(d))))


/****************************************************************************/
/* Type Descriptor Primitives                                               */
/****************************************************************************/

// These describe the data the client will GIVE to the server and what it
// will GET back from the server.  GIVEP/GETP identify Primary categories;
// GIVEQ/GETQ identify Qualifiers, i.e. subcategories and/or special handling.
//
// Client apps have little reason to use these - they should use the RWS*_
// macros listed in the "RWSDSC Type Descriptors" section below.

/****************************************************************************/

// == Argument/Return Descriptors ==

// GIVEP() values
#define GIVE_ASIS            0  // dword
#define GIVE_VOID            1  // nothing (rtn only)
#define GIVE_PSTR            2  // ptr to string
#define GIVE_PBUF            3  // ptr to fixed lth buf
#define GIVE_PPVOID          4  // ptr to pvoid or ulong
#define GIVE_PULONG          GIVE_PPVOID  // a synonym for those who need one
#define GIVE_CONV            5  // none - value derived by conversion
#define GIVE_COPYSTR         6  // none - value derived by strcpy into alloc
#define GIVE_COPYBUF         7  // none - value derived by memcpy into alloc

// GETP() values
#define GET_ASIS             0  // no action needed
#define GET_VOID             1  // nothing (rtn only)
#define GET_CONV             2  // value converted into buf
#define GET_PPSTR            3  // strcpy *value into buf
#define GET_PPBUF            4  // memcpy *value into buf
#define GET_PSTR             5  // strcpy value into buf (rtn only)
#define GET_PBUF             6  // memcpy value into buf (rtn only)
#define GET_COPYICON         7  // copy icon handle to client process (rtn only)
#define GET_COPYMINI         8  // copy mini-icon to client process (rtn only)

// GIVEQ()/GETQ() values
// used only by GIVE_CONV & GET_CONV
#define CONV_OBJPATH         1  // object from/to f/q name (or from obj ID)
#define CONV_OBJFQTITLE      2  // object from/to f/q title (or from obj ID)
#define CONV_OBJHNDL         3  // object from/to hobj
#define CONV_OBJHWND         4  // object from/to hwnd
#define CONV_OBJPREC         5  // object from/to pminirecordcore
#define CONV_OBJNAME         6  // object to name
#define CONV_OBJTITLE        7  // object to title
#define CONV_OBJID           8  // object to object ID
#define CONV_OBJFLDR         9  // object to folder containing object
#define CONV_SHDFQTITLE     10  // original obj from shadow f/q title or obj ID
#define CONV_SHDHNDL        11  // original obj from shadow hobj
#define CONV_SHDPREC        12  // original obj from shadow pminirecordcore
#define CONV_CLSNAME        13  // class from/to classname
#define CONV_CLSOBJ         14  // class from object
#define CONV_CLSOBJPATH     15  // class from object's f/q name or obj ID
#define CONV_CLSOBJFQTITLE  16  // class from object's f/q title or obj ID
#define CONV_CLSOBJHNDL     17  // class from object's hobj
#define CONV_CLSOBJHWND     18  // class from object's hwnd
#define CONV_CLSOBJPREC     19  // class from object's pminirecordcore
#define CONV_CLSSHDFQTITLE  20  // class from object from shadow f/q title
#define CONV_CLSSHDHNDL     21  // class from object from shadow hobj
#define CONV_CLSSHDPREC     22  // class from object from shadow pminirecordcore
#define CONV_SOMID          23  // somid to/from string
#define CONV_OBJICON        24  // object to icon
#define CONV_OBJMINI        25  // object to mini-icon
#define CONV_CLSICON        26  // class to icon
#define CONV_CLSMINI        27  // class to mini-icon
#define CONV_SOMCLSMGR      28  // SOM Class Manager object (input ignored)
#define CONV_PREVIOUS       29  // reuse previous arg's object (RWSC_* only)

// GETQ() values
// used only by GET_(P)PSTR & GET_(P)PBUF
#define COPY_CNTRTN         40  // nbr of bytes to copy is in return
#define COPY_CNTARG1        41  // nbr of bytes to copy is in Arg1
#define COPY_CNTARG2        42  // nbr of bytes to copy is in Arg2
#define COPY_CNTARG3        43  // nbr of bytes to copy is in Arg3
#define COPY_CNTARG4        44  // nbr of bytes to copy is in Arg4
#define COPY_CNTARG5        45  // nbr of bytes to copy is in Arg5
#define COPY_CNTARG6        46  // nbr of bytes to copy is in Arg6
#define COPY_CNTARG7        47  // nbr of bytes to copy is in Arg7
#define COPY_CNTARG8        48  // nbr of bytes to copy is in Arg8
#define COPY_CNTARG9        49  // nbr of bytes to copy is in Arg9
#define COPY_CNTULONG       50  // copy cnt is in ulong  at start of buf
#define COPY_CNTUSHORT      51  // copy cnt is in ushort at start of buf
#define COPY_CNTULZERO      52  // copy array of ulongs, last one is zero

#define COPY_MASK           63  // mask off memory bits
#define COPY_CNTFIRST       COPY_CNTRTN
#define COPY_CNTLAST        COPY_CNTULZERO

// GIVEQ()/GETQ() flags
// used by GIVE_COPYSTR & GIVE_COPYBUF to alloc object/SOM memory
// used by GET_(P)PSTR & GET_(P)PBUF to free object/SOM memory
// also used to free somId's used as input args
#define COPY_OBJMEM         64  // alloc/free object memory
#define FREE_OBJMEM         COPY_OBJMEM
#define COPY_SOMMEM        128  // alloc/free som memory
#define FREE_SOMMEM        COPY_SOMMEM
#define COPY_MEMMASK       192  // mask off copy bits

// note:  since CONV_* & COPY_* are used in different contexts, their
// values could overlap;  for now they don't as a safety precaution


/****************************************************************************/

// == Procedure Descriptors ==

// GIVEP() values
#define PROC_MNAM            1  // method by pointer to name
#define PROC_MPFN      	     2  // method by pointer to function
#define PROC_KORD            3  // SOM kernel function by ordinal
#define PROC_KPFN            4  // SOM kernel function by pointer to function
#define PROC_CONV            5  // data conversion only
#define PROC_CMD             6  // built-in command

// GIVEQ() values
#define PRTN_BOOL            0  // non-zero return indicates success
#define PRTN_ZERO            1  // zero return indicates success
#define PRTN_IGNORE          2  // return doesn't indicate outcome
#define PRTN_LASTGIVEQ       PRTN_IGNORE  // applies to methods & functions
#define PRTN_NA              3  // not applicable (PROC_CONV & PROC_CMD)


// == RWSDSC Flag values ==

#define DSC_PROC             1  // procedure
#define DSC_RTN              2  // return
#define DSC_ARG              3  // arg
#define DSC_CONV             4  // convert


/****************************************************************************/

// == Structure/Macros to access the Type Descriptors ==

typedef struct _RWSTYPE
{
    BYTE        givep;
    BYTE        giveq;
    BYTE        getp;
    BYTE        getq;
} RWSTYPE;

#define TORWSTYPE(a,b,c,d)  ((ULONG)(((ULONG)a) | (((ULONG)b)<<8) | \
                            (((ULONG)c)<<16) | (((ULONG)d)<<24)))

#define GIVEP(arg)          (((RWSTYPE*)(PVOID)(&arg))->givep)
#define GIVEQ(arg)          (((RWSTYPE*)(PVOID)(&arg))->giveq)
#define GETP(arg)           (((RWSTYPE*)(PVOID)(&arg))->getp)
#define GETQ(arg)           (((RWSTYPE*)(PVOID)(&arg))->getq)


/****************************************************************************/
/* RWSDSC Type Descriptors                                                  */
/****************************************************************************/

// Client apps should use the following macros rather than the
// Type Descriptor Primitives listed above.

/****************************************************************************/

// == Procedures ==

#define RWSP_MNAM           TORWSTYPE( PROC_MNAM,  PRTN_BOOL,    0,  0)
#define RWSP_MNAMZ          TORWSTYPE( PROC_MNAM,  PRTN_ZERO,    0,  0)
#define RWSP_MNAMI          TORWSTYPE( PROC_MNAM,  PRTN_IGNORE,  0,  0)
#define RWSP_MPFN      	    TORWSTYPE( PROC_MPFN,  PRTN_BOOL,    0,  0)
#define RWSP_MPFNZ          TORWSTYPE( PROC_MPFN,  PRTN_ZERO,    0,  0)
#define RWSP_MPFNI          TORWSTYPE( PROC_MPFN,  PRTN_IGNORE,  0,  0)
#define RWSP_KORD           TORWSTYPE( PROC_KORD,  PRTN_BOOL,    0,  0)
#define RWSP_KORDZ          TORWSTYPE( PROC_KORD,  PRTN_ZERO,    0,  0)
#define RWSP_KORDI          TORWSTYPE( PROC_KORD,  PRTN_IGNORE,  0,  0)
#define RWSP_KPFN           TORWSTYPE( PROC_KPFN,  PRTN_BOOL,    0,  0)
#define RWSP_KPFNZ          TORWSTYPE( PROC_KPFN,  PRTN_ZERO,    0,  0)
#define RWSP_KPFNI          TORWSTYPE( PROC_KPFN,  PRTN_IGNORE,  0,  0)
#define RWSP_CONV           TORWSTYPE( PROC_CONV,  PRTN_NA,      0,  0)
#define RWSP_CMD            TORWSTYPE( PROC_CMD,   PRTN_NA,      0,  0)


/****************************************************************************/

// == In-only Arguments ==

#define RWSI_ASIS           TORWSTYPE( GIVE_ASIS,    0,               GET_ASIS, 0)
#define RWSI_PSTR           TORWSTYPE( GIVE_PSTR,    0,               GET_ASIS, 0)
#define RWSI_PBUF           TORWSTYPE( GIVE_PBUF,    0,               GET_ASIS, 0)
#define RWSI_PPVOID         TORWSTYPE( GIVE_PPVOID,  0,               GET_ASIS, 0)
#define RWSI_PULONG         TORWSTYPE( GIVE_PPVOID,  0,               GET_ASIS, 0)
#define RWSI_OPATH          TORWSTYPE( GIVE_CONV,    CONV_OBJPATH,    GET_ASIS, 0)
#define RWSI_OFTITLE        TORWSTYPE( GIVE_CONV,    CONV_OBJFQTITLE, GET_ASIS, 0)
#define RWSI_OHNDL          TORWSTYPE( GIVE_CONV,    CONV_OBJHNDL,    GET_ASIS, 0)
#define RWSI_OHWND          TORWSTYPE( GIVE_CONV,    CONV_OBJHWND,    GET_ASIS, 0)
#define RWSI_OPREC          TORWSTYPE( GIVE_CONV,    CONV_OBJPREC,    GET_ASIS, 0)
#define RWSI_SFTITLE        TORWSTYPE( GIVE_CONV,    CONV_SHDFQTITLE, GET_ASIS, 0)
#define RWSI_SHNDL          TORWSTYPE( GIVE_CONV,    CONV_SHDHNDL,    GET_ASIS, 0)
#define RWSI_SPREC          TORWSTYPE( GIVE_CONV,    CONV_SHDPREC,    GET_ASIS, 0)
#define RWSI_CNAME          TORWSTYPE( GIVE_CONV,    CONV_CLSNAME,    GET_ASIS, 0)
#define RWSI_COBJ           TORWSTYPE( GIVE_CONV,    CONV_CLSOBJ,     GET_ASIS, 0)
#define RWSI_COPATH         TORWSTYPE( GIVE_CONV,    CONV_CLSOBJPATH, GET_ASIS, 0)
#define RWSI_COFTITLE       TORWSTYPE( GIVE_CONV,    CONV_CLSOBJFQTITLE,GET_ASIS, 0)
#define RWSI_COHNDL         TORWSTYPE( GIVE_CONV,    CONV_CLSOBJHNDL, GET_ASIS, 0)
#define RWSI_COHWND         TORWSTYPE( GIVE_CONV,    CONV_CLSOBJHWND, GET_ASIS, 0)
#define RWSI_COPREC         TORWSTYPE( GIVE_CONV,    CONV_CLSOBJPREC, GET_ASIS, 0)
#define RWSI_CSFTITLE       TORWSTYPE( GIVE_CONV,    CONV_CLSSHDFQTITLE,GET_ASIS, 0)
#define RWSI_CSHNDL         TORWSTYPE( GIVE_CONV,    CONV_CLSSHDHNDL, GET_ASIS, 0)
#define RWSI_CSPREC         TORWSTYPE( GIVE_CONV,    CONV_CLSSHDPREC, GET_ASIS, 0)
#define RWSI_SOMID          TORWSTYPE( GIVE_CONV,    CONV_SOMID,      GET_ASIS, 0)
#define RWSI_SOMIDFREE      TORWSTYPE( GIVE_CONV,    CONV_SOMID,      GET_ASIS, FREE_SOMMEM)
#define RWSI_SOMCLSMGR      TORWSTYPE( GIVE_CONV,    CONV_SOMCLSMGR,  GET_ASIS, 0)
#define RWSI_POBJSTR        TORWSTYPE( GIVE_COPYSTR, COPY_OBJMEM,     GET_ASIS, 0)
#define RWSI_PSOMSTR        TORWSTYPE( GIVE_COPYSTR, COPY_SOMMEM,     GET_ASIS, 0)
#define RWSI_POBJBUF        TORWSTYPE( GIVE_COPYBUF, COPY_OBJMEM,     GET_ASIS, 0)
#define RWSI_PSOMBUF        TORWSTYPE( GIVE_COPYBUF, COPY_SOMMEM,     GET_ASIS, 0)


/****************************************************************************/

// == In/Out Arguments ==

#define RWSO_PPSTR          TORWSTYPE( GIVE_PPVOID, 0, GET_PPSTR, 0)
#define RWSO_PPBUF          TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, 0)
#define RWSO_PPBUFCNTRTN    TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, COPY_CNTRTN )
#define RWSO_PPBUFCNTARG1   TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, COPY_CNTARG1)
#define RWSO_PPBUFCNTARG2   TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, COPY_CNTARG2)
#define RWSO_PPBUFCNTARG3   TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, COPY_CNTARG3)
#define RWSO_PPBUFCNTARG4   TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, COPY_CNTARG4)
#define RWSO_PPBUFCNTARG5   TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, COPY_CNTARG5)
#define RWSO_PPBUFCNTARG6   TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, COPY_CNTARG6)
#define RWSO_PPBUFCNTARG7   TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, COPY_CNTARG7)
#define RWSO_PPBUFCNTARG8   TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, COPY_CNTARG8)
#define RWSO_PPBUFCNTARG9   TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, COPY_CNTARG9)
#define RWSO_PPBUFCNTULONG  TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, COPY_CNTULONG)
#define RWSO_PPBUFCNTUSHORT TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, COPY_CNTUSHORT)
#define RWSO_PPBUFCNTULZERO TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, COPY_CNTULZERO)
#define RWSO_PPSTROBJFREE   TORWSTYPE( GIVE_PPVOID, 0, GET_PPSTR, FREE_OBJMEM)
#define RWSO_PPSTRSOMFREE   TORWSTYPE( GIVE_PPVOID, 0, GET_PPSTR, FREE_SOMMEM)
#define RWSO_PPBUFOBJFREE   TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, FREE_OBJMEM)
#define RWSO_PPBUFSOMFREE   TORWSTYPE( GIVE_PPVOID, 0, GET_PPBUF, FREE_SOMMEM)
#define RWSO_OPATH          TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_OBJPATH)
#define RWSO_OFTITLE        TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_OBJFQTITLE)
#define RWSO_ONAME          TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_OBJNAME)
#define RWSO_OTITLE         TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_OBJTITLE)
#define RWSO_OHNDL          TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_OBJHNDL)
#define RWSO_OHWND          TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_OBJHWND)
#define RWSO_OPREC          TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_OBJPREC)
#define RWSO_OID            TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_OBJID)
#define RWSO_OFLDR          TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_OBJFLDR)
#define RWSO_OICON          TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_OBJICON)
#define RWSO_OMINI          TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_OBJMINI)
#define RWSO_CICON          TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_CLSICON)
#define RWSO_CMINI          TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_CLSMINI)
#define RWSO_CNAME          TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_CLSNAME)
#define RWSO_SOMID          TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_SOMID)
#define RWSO_SOMIDFREE      TORWSTYPE( GIVE_PPVOID, 0, GET_CONV,  CONV_SOMID+FREE_SOMMEM)


/****************************************************************************/

// == Return ==

#define RWSR_ASIS           TORWSTYPE( GIVE_VOID, 0, GET_ASIS,  0)
#define RWSR_VOID           TORWSTYPE( GIVE_VOID, 0, GET_VOID,  0)
#define RWSR_OPATH          TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_OBJPATH)
#define RWSR_OFTITLE        TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_OBJFQTITLE)
#define RWSR_ONAME          TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_OBJNAME)
#define RWSR_OTITLE         TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_OBJTITLE)
#define RWSR_OHNDL          TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_OBJHNDL)
#define RWSR_OHWND          TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_OBJHWND)
#define RWSR_OPREC          TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_OBJPREC)
#define RWSR_OID            TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_OBJID)
#define RWSR_OFLDR          TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_OBJFLDR)
#define RWSR_OICON          TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_OBJICON)
#define RWSR_OMINI          TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_OBJMINI)
#define RWSR_CICON          TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_CLSICON)
#define RWSR_CMINI          TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_CLSMINI)
#define RWSR_CNAME          TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_CLSNAME)
#define RWSR_SOMID          TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_SOMID)
#define RWSR_SOMIDFREE      TORWSTYPE( GIVE_VOID, 0, GET_CONV,  CONV_SOMID+FREE_SOMMEM)
#define RWSR_COPYICON       TORWSTYPE( GIVE_VOID, 0, GET_COPYICON, 0)
#define RWSR_COPYMINI       TORWSTYPE( GIVE_VOID, 0, GET_COPYMINI, 0)
#define RWSR_PSTR           TORWSTYPE( GIVE_VOID, 0, GET_PSTR,  0)
#define RWSR_PBUF           TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  0)
#define RWSR_PBUFCNTARG1    TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  COPY_CNTARG1)
#define RWSR_PBUFCNTARG2    TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  COPY_CNTARG2)
#define RWSR_PBUFCNTARG3    TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  COPY_CNTARG3)
#define RWSR_PBUFCNTARG4    TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  COPY_CNTARG4)
#define RWSR_PBUFCNTARG5    TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  COPY_CNTARG5)
#define RWSR_PBUFCNTARG6    TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  COPY_CNTARG6)
#define RWSR_PBUFCNTARG7    TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  COPY_CNTARG7)
#define RWSR_PBUFCNTARG8    TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  COPY_CNTARG8)
#define RWSR_PBUFCNTARG9    TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  COPY_CNTARG9)
#define RWSR_PBUFCNTULONG   TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  COPY_CNTULONG)
#define RWSR_PBUFCNTUSHORT  TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  COPY_CNTUSHORT)
#define RWSR_PBUFCNTULZERO  TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  COPY_CNTULZERO)
#define RWSR_PPSTR          TORWSTYPE( GIVE_VOID, 0, GET_PPSTR, 0)
#define RWSR_PPBUF          TORWSTYPE( GIVE_VOID, 0, GET_PPBUF, 0)
#define RWSR_PSTROBJFREE    TORWSTYPE( GIVE_VOID, 0, GET_PSTR,  FREE_OBJMEM)
#define RWSR_PBUFOBJFREE    TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  FREE_OBJMEM)
#define RWSR_PPSTROBJFREE   TORWSTYPE( GIVE_VOID, 0, GET_PPSTR, FREE_OBJMEM)
#define RWSR_PPBUFOBJFREE   TORWSTYPE( GIVE_VOID, 0, GET_PPBUF, FREE_OBJMEM)
#define RWSR_PSTRSOMFREE    TORWSTYPE( GIVE_VOID, 0, GET_PSTR,  FREE_SOMMEM)
#define RWSR_PBUFSOMFREE    TORWSTYPE( GIVE_VOID, 0, GET_PBUF,  FREE_SOMMEM)
#define RWSR_PPSTRSOMFREE   TORWSTYPE( GIVE_VOID, 0, GET_PPSTR, FREE_SOMMEM)
#define RWSR_PPBUFSOMFREE   TORWSTYPE( GIVE_VOID, 0, GET_PPBUF, FREE_SOMMEM)


/****************************************************************************/

// == Data Conversion ==

#define RWSC_OPATH_OBJ      TORWSTYPE( GIVE_CONV,  CONV_OBJPATH,  GET_ASIS,  0)
#define RWSC_OPATH_OTITLE   TORWSTYPE( GIVE_CONV,  CONV_OBJPATH,  GET_CONV,  CONV_OBJTITLE)
#define RWSC_OPATH_OHNDL    TORWSTYPE( GIVE_CONV,  CONV_OBJPATH,  GET_CONV,  CONV_OBJHNDL)
#define RWSC_OPATH_OHWND    TORWSTYPE( GIVE_CONV,  CONV_OBJPATH,  GET_CONV,  CONV_OBJHWND)
#define RWSC_OPATH_OPREC    TORWSTYPE( GIVE_CONV,  CONV_OBJPATH,  GET_CONV,  CONV_OBJPREC)
#define RWSC_OPATH_OID      TORWSTYPE( GIVE_CONV,  CONV_OBJPATH,  GET_CONV,  CONV_OBJID)
#define RWSC_OPATH_OICON    TORWSTYPE( GIVE_CONV,  CONV_OBJPATH,  GET_CONV,  CONV_OBJICON)
#define RWSC_OPATH_OMINI    TORWSTYPE( GIVE_CONV,  CONV_OBJPATH,  GET_CONV,  CONV_OBJMINI)
#define RWSC_OPATH_CLASS    TORWSTYPE( GIVE_CONV,  CONV_OBJPATH,  GET_CONV,  CONV_CLSOBJ)
#define RWSC_OPATH_CNAME    TORWSTYPE( GIVE_CONV,  CONV_OBJPATH,  GET_CONV,  CONV_CLSNAME)
#define RWSC_OPATH_CICON    TORWSTYPE( GIVE_CONV,  CONV_OBJPATH,  GET_CONV,  CONV_CLSICON)
#define RWSC_OPATH_CMINI    TORWSTYPE( GIVE_CONV,  CONV_OBJPATH,  GET_CONV,  CONV_CLSMINI)

#define RWSC_OFTITLE_OBJ    TORWSTYPE( GIVE_CONV,  CONV_OBJFQTITLE, GET_ASIS, 0)
#define RWSC_OFTITLE_ONAME  TORWSTYPE( GIVE_CONV,  CONV_OBJFQTITLE, GET_CONV, CONV_OBJNAME)
#define RWSC_OFTITLE_OHNDL  TORWSTYPE( GIVE_CONV,  CONV_OBJFQTITLE, GET_CONV, CONV_OBJHNDL)
#define RWSC_OFTITLE_OHWND  TORWSTYPE( GIVE_CONV,  CONV_OBJFQTITLE, GET_CONV, CONV_OBJHWND)
#define RWSC_OFTITLE_OPREC  TORWSTYPE( GIVE_CONV,  CONV_OBJFQTITLE, GET_CONV, CONV_OBJPREC)
#define RWSC_OFTITLE_OID    TORWSTYPE( GIVE_CONV,  CONV_OBJFQTITLE, GET_CONV,  CONV_OBJID)
#define RWSC_OFTITLE_OICON  TORWSTYPE( GIVE_CONV,  CONV_OBJFQTITLE, GET_CONV, CONV_OBJICON)
#define RWSC_OFTITLE_OMINI  TORWSTYPE( GIVE_CONV,  CONV_OBJFQTITLE, GET_CONV, CONV_OBJMINI)
#define RWSC_OFTITLE_CLASS  TORWSTYPE( GIVE_CONV,  CONV_OBJFQTITLE, GET_CONV, CONV_CLSOBJ)
#define RWSC_OFTITLE_CNAME  TORWSTYPE( GIVE_CONV,  CONV_OBJFQTITLE, GET_CONV, CONV_CLSNAME)
#define RWSC_OFTITLE_CICON  TORWSTYPE( GIVE_CONV,  CONV_OBJFQTITLE, GET_CONV, CONV_CLSICON)
#define RWSC_OFTITLE_CMINI  TORWSTYPE( GIVE_CONV,  CONV_OBJFQTITLE, GET_CONV, CONV_CLSMINI)

#define RWSC_OHNDL_OBJ      TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_ASIS,  0)
#define RWSC_OHNDL_OPATH    TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_CONV,  CONV_OBJPATH)
#define RWSC_OHNDL_OFTITLE  TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_CONV,  CONV_OBJFQTITLE)
#define RWSC_OHNDL_ONAME    TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_CONV,  CONV_OBJNAME)
#define RWSC_OHNDL_OTITLE   TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_CONV,  CONV_OBJTITLE)
#define RWSC_OHNDL_OHWND    TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_CONV,  CONV_OBJHWND)
#define RWSC_OHNDL_OPREC    TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_CONV,  CONV_OBJPREC)
#define RWSC_OHNDL_OID      TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_CONV,  CONV_OBJID)
#define RWSC_OHNDL_OFLDR    TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_CONV,  CONV_OBJFLDR)
#define RWSC_OHNDL_OICON    TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_CONV,  CONV_OBJICON)
#define RWSC_OHNDL_OMINI    TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_CONV,  CONV_OBJMINI)
#define RWSC_OHNDL_CLASS    TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_CONV,  CONV_CLSOBJ)
#define RWSC_OHNDL_CNAME    TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_CONV,  CONV_CLSNAME)
#define RWSC_OHNDL_CICON    TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_CONV,  CONV_CLSICON)
#define RWSC_OHNDL_CMINI    TORWSTYPE( GIVE_CONV,  CONV_OBJHNDL,  GET_CONV,  CONV_CLSMINI)

#define RWSC_OHWND_OBJ      TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_ASIS,  0)
#define RWSC_OHWND_OPATH    TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_CONV,  CONV_OBJPATH)
#define RWSC_OHWND_OFTITLE  TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_CONV,  CONV_OBJFQTITLE)
#define RWSC_OHWND_ONAME    TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_CONV,  CONV_OBJNAME)
#define RWSC_OHWND_OTITLE   TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_CONV,  CONV_OBJTITLE)
#define RWSC_OHWND_OHNDL    TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_CONV,  CONV_OBJHNDL)
#define RWSC_OHWND_OPREC    TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_CONV,  CONV_OBJPREC)
#define RWSC_OHWND_OID      TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_CONV,  CONV_OBJID)
#define RWSC_OHWND_OFLDR    TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_CONV,  CONV_OBJFLDR)
#define RWSC_OHWND_OICON    TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_CONV,  CONV_OBJICON)
#define RWSC_OHWND_OMINI    TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_CONV,  CONV_OBJMINI)
#define RWSC_OHWND_CLASS    TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_CONV,  CONV_CLSOBJ)
#define RWSC_OHWND_CNAME    TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_CONV,  CONV_CLSNAME)
#define RWSC_OHWND_CICON    TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_CONV,  CONV_CLSICON)
#define RWSC_OHWND_CMINI    TORWSTYPE( GIVE_CONV,  CONV_OBJHWND,  GET_CONV,  CONV_CLSMINI)

#define RWSC_OPREC_OBJ      TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_ASIS,  0)
#define RWSC_OPREC_OPATH    TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_CONV,  CONV_OBJPATH)
#define RWSC_OPREC_OFTITLE  TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_CONV,  CONV_OBJFQTITLE)
#define RWSC_OPREC_ONAME    TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_CONV,  CONV_OBJNAME)
#define RWSC_OPREC_OTITLE   TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_CONV,  CONV_OBJTITLE)
#define RWSC_OPREC_OHNDL    TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_CONV,  CONV_OBJHNDL)
#define RWSC_OPREC_OHWND    TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_CONV,  CONV_OBJHWND)
#define RWSC_OPREC_OID      TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_CONV,  CONV_OBJID)
#define RWSC_OPREC_OFLDR    TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_CONV,  CONV_OBJFLDR)
#define RWSC_OPREC_OICON    TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_CONV,  CONV_OBJICON)
#define RWSC_OPREC_OMINI    TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_CONV,  CONV_OBJMINI)
#define RWSC_OPREC_CLASS    TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_CONV,  CONV_CLSOBJ)
#define RWSC_OPREC_CNAME    TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_CONV,  CONV_CLSNAME)
#define RWSC_OPREC_CICON    TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_CONV,  CONV_CLSICON)
#define RWSC_OPREC_CMINI    TORWSTYPE( GIVE_CONV,  CONV_OBJPREC,  GET_CONV,  CONV_CLSMINI)

#define RWSC_SFTITLE_OBJ    TORWSTYPE( GIVE_CONV,  CONV_SHDFQTITLE, GET_ASIS, 0)
#define RWSC_SFTITLE_OPATH  TORWSTYPE( GIVE_CONV,  CONV_SHDFQTITLE, GET_CONV, CONV_OBJPATH)
#define RWSC_SFTITLE_OFTITLE TORWSTYPE( GIVE_CONV, CONV_SHDFQTITLE, GET_CONV, CONV_OBJFQTITLE)
#define RWSC_SFTITLE_ONAME  TORWSTYPE( GIVE_CONV,  CONV_SHDFQTITLE, GET_CONV, CONV_OBJNAME)
#define RWSC_SFTITLE_OHNDL  TORWSTYPE( GIVE_CONV,  CONV_SHDFQTITLE, GET_CONV, CONV_OBJHNDL)
#define RWSC_SFTITLE_OHWND  TORWSTYPE( GIVE_CONV,  CONV_SHDFQTITLE, GET_CONV, CONV_OBJHWND)
#define RWSC_SFTITLE_OPREC  TORWSTYPE( GIVE_CONV,  CONV_SHDFQTITLE, GET_CONV, CONV_OBJPREC)
#define RWSC_SFTITLE_OID    TORWSTYPE( GIVE_CONV,  CONV_SHDFQTITLE, GET_CONV, CONV_OBJID)
#define RWSC_SFTITLE_OFLDR  TORWSTYPE( GIVE_CONV,  CONV_SHDFQTITLE, GET_CONV, CONV_OBJFLDR)
#define RWSC_SFTITLE_OICON  TORWSTYPE( GIVE_CONV,  CONV_SHDFQTITLE, GET_CONV, CONV_OBJICON)
#define RWSC_SFTITLE_OMINI  TORWSTYPE( GIVE_CONV,  CONV_SHDFQTITLE, GET_CONV, CONV_OBJMINI)
#define RWSC_SFTITLE_CLASS  TORWSTYPE( GIVE_CONV,  CONV_SHDFQTITLE, GET_CONV, CONV_CLSOBJ)
#define RWSC_SFTITLE_CNAME  TORWSTYPE( GIVE_CONV,  CONV_SHDFQTITLE, GET_CONV, CONV_CLSNAME)
#define RWSC_SFTITLE_CICON  TORWSTYPE( GIVE_CONV,  CONV_SHDFQTITLE, GET_CONV, CONV_CLSICON)
#define RWSC_SFTITLE_CMINI  TORWSTYPE( GIVE_CONV,  CONV_SHDFQTITLE, GET_CONV, CONV_CLSMINI)

#define RWSC_SHNDL_OBJ      TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_ASIS,  0)
#define RWSC_SHNDL_OPATH    TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_OBJPATH)
#define RWSC_SHNDL_OFTITLE  TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_OBJFQTITLE)
#define RWSC_SHNDL_ONAME    TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_OBJNAME)
#define RWSC_SHNDL_OTITLE   TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_OBJTITLE)
#define RWSC_SHNDL_OHNDL    TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_OBJHNDL)
#define RWSC_SHNDL_OHWND    TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_OBJHWND)
#define RWSC_SHNDL_OPREC    TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_OBJPREC)
#define RWSC_SHNDL_OID      TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_OBJID)
#define RWSC_SHNDL_OFLDR    TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_OBJFLDR)
#define RWSC_SHNDL_OICON    TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_OBJICON)
#define RWSC_SHNDL_OMINI    TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_OBJMINI)
#define RWSC_SHNDL_CLASS    TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_CLSOBJ)
#define RWSC_SHNDL_CNAME    TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_CLSNAME)
#define RWSC_SHNDL_CICON    TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_CLSICON)
#define RWSC_SHNDL_CMINI    TORWSTYPE( GIVE_CONV,  CONV_SHDHNDL,  GET_CONV,  CONV_CLSMINI)

#define RWSC_SPREC_OBJ      TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_ASIS,  0)
#define RWSC_SPREC_OPATH    TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_OBJPATH)
#define RWSC_SPREC_OFTITLE  TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_OBJFQTITLE)
#define RWSC_SPREC_ONAME    TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_OBJNAME)
#define RWSC_SPREC_OTITLE   TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_OBJTITLE)
#define RWSC_SPREC_OHNDL    TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_OBJHNDL)
#define RWSC_SPREC_OHWND    TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_OBJHWND)
#define RWSC_SPREC_OPREC    TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_OBJPREC)
#define RWSC_SPREC_OID      TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_OBJID)
#define RWSC_SPREC_OFLDR    TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_OBJFLDR)
#define RWSC_SPREC_OICON    TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_OBJICON)
#define RWSC_SPREC_OMINI    TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_OBJMINI)
#define RWSC_SPREC_CLASS    TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_CLSOBJ)
#define RWSC_SPREC_CNAME    TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_CLSNAME)
#define RWSC_SPREC_CICON    TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_CLSICON)
#define RWSC_SPREC_CMINI    TORWSTYPE( GIVE_CONV,  CONV_SHDPREC,  GET_CONV,  CONV_CLSMINI)

#define RWSC_PREV_OPATH     TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_OBJPATH)
#define RWSC_PREV_OFTITLE   TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_OBJFQTITLE)
#define RWSC_PREV_ONAME     TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_OBJNAME)
#define RWSC_PREV_OTITLE    TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_OBJTITLE)
#define RWSC_PREV_OHNDL     TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_OBJHNDL)
#define RWSC_PREV_OHWND     TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_OBJHWND)
#define RWSC_PREV_OPREC     TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_OBJPREC)
#define RWSC_PREV_OID       TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_OBJID)
#define RWSC_PREV_OFLDR     TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_OBJFLDR)
#define RWSC_PREV_OICON     TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_OBJICON)
#define RWSC_PREV_OMINI     TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_OBJMINI)
#define RWSC_PREV_CLASS     TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_CLSOBJ)
#define RWSC_PREV_CNAME     TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_CLSNAME)
#define RWSC_PREV_CICON     TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_CLSICON)
#define RWSC_PREV_CMINI     TORWSTYPE( GIVE_CONV,  CONV_PREVIOUS, GET_CONV,  CONV_CLSMINI)

#define RWSC_OBJ_OPATH      TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_OBJPATH)
#define RWSC_OBJ_OFTITLE    TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_OBJFQTITLE)
#define RWSC_OBJ_ONAME      TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_OBJNAME)
#define RWSC_OBJ_OTITLE     TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_OBJTITLE)
#define RWSC_OBJ_OHNDL      TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_OBJHNDL)
#define RWSC_OBJ_OHWND      TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_OBJHWND)
#define RWSC_OBJ_OPREC      TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_OBJPREC)
#define RWSC_OBJ_OID        TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_OBJID)
#define RWSC_OBJ_OFLDR      TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_OBJFLDR)
#define RWSC_OBJ_OICON      TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_OBJICON)
#define RWSC_OBJ_OMINI      TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_OBJMINI)
#define RWSC_OBJ_CLASS      TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_CLSOBJ)
#define RWSC_OBJ_CNAME      TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_CLSNAME)
#define RWSC_OBJ_CICON      TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_CLSICON)
#define RWSC_OBJ_CMINI      TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_CLSMINI)

#define RWSC_CLASS_CNAME    TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_CLSNAME)
#define RWSC_CLASS_CICON    TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_CLSICON)
#define RWSC_CLASS_CMINI    TORWSTYPE( GIVE_ASIS,  0,             GET_CONV,  CONV_CLSMINI)
#define RWSC_CNAME_CLASS    TORWSTYPE( GIVE_CONV,  CONV_CLSNAME,  GET_ASIS,  0)
#define RWSC_CNAME_CICON    TORWSTYPE( GIVE_CONV,  CONV_CLSNAME,  GET_CONV,  CONV_CLSICON)
#define RWSC_CNAME_CMINI    TORWSTYPE( GIVE_CONV,  CONV_CLSNAME,  GET_CONV,  CONV_CLSMINI)

#define RWSC_NULL_SOMCLSMGR TORWSTYPE( GIVE_CONV,  CONV_SOMCLSMGR, GET_ASIS, 0)

#define RWSC_ADDR_PSTR      TORWSTYPE( GIVE_ASIS,  0,             GET_PSTR,  0)
#define RWSC_ADDR_PBUF      TORWSTYPE( GIVE_ASIS,  0,             GET_PBUF,  0)

/****************************************************************************/

#ifdef __cplusplus
    }
#endif

#endif // RWS_H_INCLUDED

/****************************************************************************/

