/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* NSReg.h
 */
#ifndef _NSREG_H_
#define _NSREG_H_

#include "xp_core.h"

typedef void (*nr_RegPackCallbackFunc) (void *userData, int32 bytes, int32 totalBytes);

typedef int32   REGERR;
typedef int32   RKEY;
typedef uint32  REGENUM;
typedef void *  HREG;

typedef struct _reginfo
{
   uint16  size;        /* must be initialized to sizeof(REGINFO) */
   uint16  entryType;
   uint32  entryLength;
} REGINFO;

#define REGERR_OK           (0)
#define REGERR_FAIL         (1)
#define REGERR_NOMORE       (2)
#define REGERR_NOFIND       (3)
#define REGERR_BADREAD      (4)
#define REGERR_BADLOCN      (5)
#define REGERR_PARAM        (6)
#define REGERR_BADMAGIC     (7)
#define REGERR_BADCHECK     (8)
#define REGERR_NOFILE       (9)
#define REGERR_MEMORY       (10)
#define REGERR_BUFTOOSMALL  (11)
#define REGERR_NAMETOOLONG  (12)
#define REGERR_REGVERSION   (13)
#define REGERR_DELETED      (14)
#define REGERR_BADTYPE      (15)
#define REGERR_NOPATH       (16)
#define REGERR_BADNAME      (17)
#define REGERR_READONLY     (18)
#define REGERR_BADUTF8      (19)


/* Total path length */
#define MAXREGPATHLEN   (2048)
/* Name on the path (including null terminator) */
#define MAXREGNAMELEN   (512)
/* Value of an entry */
#define MAXREGVALUELEN  (0x7FFF)

/* Standard keys */
#define ROOTKEY_USERS                   (0x01)
#define ROOTKEY_COMMON                  (0x02)
#define ROOTKEY_CURRENT_USER            (0x03)
#define ROOTKEY_PRIVATE                 (0x04)

/* enumeration styles */
#define REGENUM_NORMAL                  (0x00)
#define REGENUM_CHILDREN                REGENUM_NORMAL
#define REGENUM_DESCEND                 (0x01)
#define REGENUM_DEPTH_FIRST             (0x02)

/* entry data types */
#define REGTYPE_ENTRY                 (0x0010)
#define REGTYPE_ENTRY_STRING_UTF      (REGTYPE_ENTRY + 1)
#define REGTYPE_ENTRY_INT32_ARRAY     (REGTYPE_ENTRY + 2)
#define REGTYPE_ENTRY_BYTES           (REGTYPE_ENTRY + 3)
#define REGTYPE_ENTRY_FILE            (REGTYPE_ENTRY + 4)

#define REG_DELETE_LIST_KEY  "Netscape/Communicator/SoftwareUpdate/Delete List"
#define REG_REPLACE_LIST_KEY "Netscape/Communicator/SoftwareUpdate/Replace List"
#define REG_UNINSTALL_DIR    "Netscape/Communicator/SoftwareUpdate/Uninstall/"

#define UNINSTALL_NAV_STR "_"


#define UNIX_GLOBAL_FLAG     "MOZILLA_SHARED_REGISTRY"

/* Platform-dependent declspec for library interface */
#if defined(XP_PC)
  #if defined(WIN32)

    #if defined (STANDALONE_REGISTRY)
       #define VR_INTERFACE(type)     __declspec(dllexport) type __cdecl
    #else
       #define VR_INTERFACE(type)     __declspec(dllexport) type __stdcall
    #endif

  #elif defined(XP_OS2)
  #define VR_INTERFACE(type)     type _Optlink
  #else
  #define VR_INTERFACE(type)     type _far _pascal _export
  #endif
#elif defined XP_MAC
  #define VR_INTERFACE(__x)  __declspec(export) __x
#else
  #define VR_INTERFACE(type)     type
#endif

XP_BEGIN_PROTOS
/* ---------------------------------------------------------------------
 * Registry API -- General
 * ---------------------------------------------------------------------
 */

VR_INTERFACE(REGERR) NR_RegOpen(
         char *filename,   /* reg. file to open (NULL == standard registry) */
         HREG *hReg        /* OUT: handle to opened registry */
       );

VR_INTERFACE(REGERR) NR_RegClose(
         HREG hReg         /* handle of open registry to close */
       );

VR_INTERFACE(REGERR) NR_RegPack(
         HREG hReg,         /* handle of open registry to pack */
         void *userData,
         nr_RegPackCallbackFunc fn
       );

VR_INTERFACE(REGERR) NR_RegGetUsername(
         char **name        /* on return, an alloc'ed copy of the current user name */
       );

VR_INTERFACE(REGERR) NR_RegSetUsername(
         const char *name  /* name of current user */
       );

/* ---------------------------------------------------------------------
 * Registry API -- Key Management functions
 * ---------------------------------------------------------------------
 */

VR_INTERFACE(REGERR) NR_RegAddKey(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* root key */
         char *path,       /* relative path of subkey to add */
         RKEY *newKey      /* if not null returns newly created key */
       );

VR_INTERFACE(REGERR) NR_RegAddKeyRaw(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* root key */
         char *keyname,    /* name of key to add */
         RKEY *newKey      /* if not null returns newly created key */
       );

VR_INTERFACE(REGERR) NR_RegDeleteKey(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* root key */
         char *path        /* relative path of subkey to delete */
       );

VR_INTERFACE(REGERR) NR_RegDeleteKeyRaw(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* root key */
         char *keyname     /* name subkey to delete */
       );

VR_INTERFACE(REGERR) NR_RegGetKey(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* root key */
         char *path,       /* relative path of subkey to find */
         RKEY *result      /* returns RKEY of specified sub-key */
       );

VR_INTERFACE(REGERR) NR_RegGetKeyRaw(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* root key */
         char *keyname,       /* name of key to get */
         RKEY *result      /* returns RKEY of specified sub-key */
       );

VR_INTERFACE(REGERR) NR_RegEnumSubkeys(
         HREG    hReg,        /* handle of open registry */
         RKEY    key,         /* containing key */
         REGENUM *state,      /* enum state, must be NULL to start */
         char    *buffer,     /* buffer for entry names */
         uint32  bufsize,     /* size of buffer */
         uint32  style        /* 0: children only; REGENUM_DESCEND: sub-tree */
       );

/* ---------------------------------------------------------------------
 * Registry API -- Entry Management functions
 * ---------------------------------------------------------------------
 */

VR_INTERFACE(REGERR) NR_RegGetEntryInfo(
         HREG    hReg,     /* handle of open registry */
         RKEY    key,      /* containing key */
         char    *name,    /* entry name */
         REGINFO *info     /* returned entry info */
       );

VR_INTERFACE(REGERR) NR_RegGetEntryString(
         HREG   hReg,      /* handle of open registry */
         RKEY   key,       /* containing key */
         char   *name,     /* entry name */
         char   *buffer,   /* buffer to hold value (UTF String) */
         uint32 bufsize    /* length of buffer */
       );

VR_INTERFACE(REGERR) NR_RegGetEntry(
         HREG   hReg,      /* handle of open registry */
         RKEY   key,       /* containing key */
         char   *name,     /* entry name */
         void   *buffer,   /* buffer to hold value */
         uint32 *size      /* in:length of buffer */
       );                  /* out: data length, >>includes<< null terminator*/

VR_INTERFACE(REGERR) NR_RegSetEntryString(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* containing key */
         char *name,       /* entry name */
         char *buffer      /* UTF String value */
       );

VR_INTERFACE(REGERR) NR_RegSetEntry(
         HREG   hReg,        /* handle of open registry */
         RKEY   key,         /* containing key */
         char   *name,       /* entry name */
         uint16 type,        /* type of value data */
         void   *buffer,     /* data buffer */
         uint32 size         /* data length in bytes; incl. null term for strings */
       );

VR_INTERFACE(REGERR) NR_RegDeleteEntry(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* containing key */
         char *name        /* value name */
       );

VR_INTERFACE(REGERR) NR_RegEnumEntries(
         HREG    hReg,        /* handle of open registry */
         RKEY    key,         /* containing key */
         REGENUM *state,      /* enum state, must be NULL to start */
         char    *buffer,     /* buffer for entry names */
         uint32  bufsize,     /* size of buffer */
         REGINFO *info        /* optional; returns info about entry */
       );


VR_INTERFACE(void)      NR_ShutdownRegistry(void);
VR_INTERFACE(REGERR)    NR_StartupRegistry(void);


XP_END_PROTOS

#endif   /* _NSREG_H_ */

/* EOF: NSReg.h */

