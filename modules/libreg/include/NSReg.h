/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Daniel Veditz <dveditz@netscape.com>
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

/* ---------------------------------------------------------------------
 * NR_RegOpen - Open a netscape XP registry
 *
 * Parameters:
 *    filename   - registry file to open. NULL or ""  opens the standard
 *                 local registry.
 *    hReg       - OUT: handle to opened registry
 *
 * Output:
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegOpen(
         char *filename,   /* reg. file to open (NULL == standard registry) */
         HREG *hReg        /* OUT: handle to opened registry */
       );


/* ---------------------------------------------------------------------
 * NR_RegClose - Close a netscape XP registry
 *
 * Parameters:
 *    hReg     - handle of open registry to be closed.
 *
 * After calling this routine the handle is no longer valid
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegClose(
         HREG hReg         /* handle of open registry to close */
       );


/* ---------------------------------------------------------------------
 * NR_RegIsWritable - Check read/write status of open registry
 *
 * Parameters:
 *    hReg     - handle of open registry to query
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegIsWritable(
         HREG hReg         /* handle of open registry to query */
       );

VR_INTERFACE(REGERR) NR_RegPack(
         HREG hReg,         /* handle of open registry to pack */
         void *userData,
         nr_RegPackCallbackFunc fn
       );


/* ---------------------------------------------------------------------
 * NR_RegSetUsername - Set the current username
 * 
 * If the current user profile name is not set then trying to use
 * HKEY_CURRENT_USER will result in an error.
 *
 * Parameters:
 *     name     - name of the current user
 *
 * Output:
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegSetUsername(
         const char *name  /* name of current user */
       );

/* ---------------------------------------------------------------------
 * DO NOT USE -- Will be removed 
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegGetUsername(
         char **name        /* on return, an alloc'ed copy of the current user name */
       );






/* ---------------------------------------------------------------------
 * Registry API -- Key Management functions
 * ---------------------------------------------------------------------
 */

/* ---------------------------------------------------------------------
 * NR_RegAddKey - Add a key node to the registry
 *
 * Can also be used to find an existing node for convenience.
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - registry key obtained from NR_RegGetKey(),
 *               or one of the standard top-level keys
 *    path     - relative path of key to be added.  Intermediate
 *               nodes will be added if necessary.
 *    newkey   - If not null returns RKEY of new or found node
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegAddKey(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* root key */
         char *path,       /* relative path of subkey to add */
         RKEY *newKey      /* if not null returns newly created key */
       );


/* ---------------------------------------------------------------------
 * NR_RegAddKeyRaw - Add a key node to the registry
 *
 *      This routine is different from NR_RegAddKey() in that it takes 
 *      a keyname rather than a path.
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - registry key obtained from NR_RegGetKey(),
 *               or one of the standard top-level keys
 *    keyname  - name of key to be added. No parsing of this
 *               name happens.
 *    newkey   - if not null the RKEY of the new key is returned
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegAddKeyRaw(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* root key */
         char *keyname,    /* name of key to add */
         RKEY *newKey      /* if not null returns newly created key */
       );


/* ---------------------------------------------------------------------
 * NR_RegDeleteKey - Delete the specified key
 *
 * Note that delete simply orphans blocks and makes no attempt
 * to reclaim space in the file. Use NR_RegPack()
 *
 * Cannot be used to delete keys with child keys
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - starting node RKEY, typically one of the standard ones.
 *    path     - relative path of key to delete
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegDeleteKey(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* root key */
         char *path        /* relative path of subkey to delete */
       );


/* ---------------------------------------------------------------------
 * NR_RegDeleteKeyRaw - Delete the specified raw key
 *
 * Note that delete simply orphans blocks and makes no attempt
 * to reclaim space in the file. Use NR_RegPack()
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY or parent to the raw key you wish to delete
 *    keyname  - name of child key to delete
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegDeleteKeyRaw(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* root key */
         char *keyname     /* name subkey to delete */
       );


/* ---------------------------------------------------------------------
 * NR_RegGetKey - Get the RKEY value of a node from its path
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - starting node RKEY, typically one of the standard ones.
 *    path     - relative path of key to find.  (a blank path just gives you
 *               the starting key--useful for verification, VersionRegistry)
 *    result   - if successful the RKEY of the specified sub-key
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegGetKey(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* root key */
         char *path,       /* relative path of subkey to find */
         RKEY *result      /* returns RKEY of specified sub-key */
       );


/* ---------------------------------------------------------------------
 * NR_RegGetKeyRaw - Get the RKEY value of a node from its keyname
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - starting node RKEY, typically one of the standard ones.
 *    keyname  - keyname of key to find.  (a blank keyname just gives you
 *               the starting key--useful for verification, VersionRegistry)
 *    result   - if successful the RKEY of the specified sub-key
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegGetKeyRaw(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* root key */
         char *keyname,       /* name of key to get */
         RKEY *result      /* returns RKEY of specified sub-key */
       );


/* ---------------------------------------------------------------------
 * NR_RegEnumSubkeys - Enumerate the subkey names for the specified key
 *
 * Returns REGERR_NOMORE at end of enumeration.
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key to enumerate--obtain with NR_RegGetKey()
 *    eState   - enumerations state, must contain NULL to start
 *    buffer   - location to store subkey names.  Once an enumeration
 *               is started user must not modify contents since values
 *               are built using the previous contents.
 *    bufsize  - size of buffer for names
 *    style    - 0 returns direct child keys only, REGENUM_DESCEND
 *               returns entire sub-tree
 * ---------------------------------------------------------------------
 */
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


/* ---------------------------------------------------------------------
 * NR_RegGetEntryInfo - Get some basic info about the entry data
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key that contains entry--obtain with NR_RegGetKey()
 *    name     - name of entry
 *    info     - return: Entry info object
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegGetEntryInfo(
         HREG    hReg,     /* handle of open registry */
         RKEY    key,      /* containing key */
         char    *name,    /* entry name */
         REGINFO *info     /* returned entry info */
       );

       
/* ---------------------------------------------------------------------
 * NR_RegGetEntryString - Get the UTF string value associated with the
 *                       named entry of the specified key.
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key that contains entry--obtain with NR_RegGetKey()
 *    name     - name of entry
 *    buffer   - destination for string
 *    bufsize  - size of buffer
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegGetEntryString(
         HREG   hReg,      /* handle of open registry */
         RKEY   key,       /* containing key */
         char   *name,     /* entry name */
         char   *buffer,   /* buffer to hold value (UTF String) */
         uint32 bufsize    /* length of buffer */
       );

/* ---------------------------------------------------------------------
 * NR_RegGetEntry - Get the value data associated with the
 *                  named entry of the specified key.
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key that contains entry--obtain with NR_RegGetKey()
 *    name     - name of entry
 *    buffer   - destination for data
 *    size     - in:  size of buffer
 *               out: size of actual data (incl. \0 term. for strings)
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegGetEntry(
         HREG   hReg,      /* handle of open registry */
         RKEY   key,       /* containing key */
         char   *name,     /* entry name */
         void   *buffer,   /* buffer to hold value */
         uint32 *size      /* in:length of buffer */
       );                  /* out: data length, >>includes<< null terminator*/


/* ---------------------------------------------------------------------
 * NR_RegSetEntryString - Store a UTF-8 string value associated with the
 *                       named entry of the specified key.  Used for
 *                       both creation and update.
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key that contains entry--obtain with NR_RegGetKey()
 *    name     - name of entry
 *    buffer   - UTF-8 String to store
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegSetEntryString(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* containing key */
         char *name,       /* entry name */
         char *buffer      /* UTF String value */
       );


/* ---------------------------------------------------------------------
 * NR_RegSetEntry - Store value data associated with the named entry
 *                  of the specified key.  Used for both creation and update.
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key that contains entry--obtain with NR_RegGetKey()
 *    name     - name of entry
 *    type     - type of data to be stored
 *    buffer   - data to store
 *    size     - length of data to store in bytes
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegSetEntry(
         HREG   hReg,        /* handle of open registry */
         RKEY   key,         /* containing key */
         char   *name,       /* entry name */
         uint16 type,        /* type of value data */
         void   *buffer,     /* data buffer */
         uint32 size         /* data length in bytes; incl. null term for strings */
       );


/* ---------------------------------------------------------------------
 * NR_RegDeleteEntry - Delete the named entry
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key that contains entry--obtain with NR_RegGetKey()
 *    name     - name of entry
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) NR_RegDeleteEntry(
         HREG hReg,        /* handle of open registry */
         RKEY key,         /* containing key */
         char *name        /* value name */
       );


/* ---------------------------------------------------------------------
 * NR_RegEnumEntries - Enumerate the entry names for the specified key
 *
 * Returns REGERR_NOMORE at end of enumeration.
 *
 * Parameters:
 *    hReg     - handle of open registry
 *    key      - RKEY of key that contains entry--obtain with NR_RegGetKey()
 *    eState   - enumerations state, must contain NULL to start
 *    buffer   - location to store entry names
 *    bufsize  - size of buffer for names
 * ---------------------------------------------------------------------
 */
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

