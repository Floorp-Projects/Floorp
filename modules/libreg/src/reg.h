/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Daniel Veditz <dveditz@netscape.com>
 */
/* reg.h
 * XP Registry functions (prototype)
 */

#ifndef _REG_H_
#define _REG_H_

#include "vr_stubs.h"

#ifndef STANDALONE_REGISTRY
#include "prlock.h"
#endif

/* --------------------------------------------------------------------
 * Miscellaneous Definitions
 * --------------------------------------------------------------------
 */
#define MAGIC_NUMBER    0x76644441L
#define MAJOR_VERSION   1          /* major version for incompatible changes */
#define MINOR_VERSION   2          /* minor ver for new (compatible) features */
#define PATHDEL         '/'
#define HDRRESERVE      128        /* number of bytes reserved for hdr */
#define INTSIZE         4
#define DOUBLESIZE      8

#define PACKBUFFERSIZE  2048


/* Node types */
#define REGTYPE_KEY                   (1)
#define REGTYPE_DELETED               (0x0080)

/* Private standard keys */
#define ROOTKEY                       (0x20)
#define ROOTKEY_VERSIONS              (0x21)

/* strings for standard keys */
#define ROOTKEY_STR             "/"
#define ROOTKEY_VERSIONS_STR    "Version Registry"
#define ROOTKEY_USERS_STR       "Users"
#define ROOTKEY_COMMON_STR      "Common"
#define ROOTKEY_PRIVATE_STR     "Private Arenas"

#define OLD_VERSIONS_STR        "ROOTKEY_VERSIONS"
#define OLD_USERS_STR           "ROOTKEY_USERS"
#define OLD_COMMON_STR          "ROOTKEY_COMMON"

/* needs to be kept in sync with PE. see ns/cmd/winfe/profile.h */
/* and ns/cmd/macfe/central/profile.cp */
#define ASW_MAGIC_PROFILE_NAME "User1"

/* macros */
#define COPYDESC(dest,src)  memcpy((dest),(src),sizeof(REGDESC))

#define VALID_FILEHANDLE(fh)    ((fh) != NULL)

#define INVALID_NAME_CHAR(p)    ( ((unsigned char)(p) < 0x20) )

#define TYPE_IS_ENTRY(type)       ( (type) & REGTYPE_ENTRY )
#define TYPE_IS_KEY(type)         ( !((type) & REGTYPE_ENTRY) )

#define VERIFY_HREG(h)\
    ( ((h) == NULL) ? REGERR_PARAM : \
    ( (((REGHANDLE*)(h))->magic == MAGIC_NUMBER) ? REGERR_OK : REGERR_BADMAGIC ) )



/* --------------------------------------------------------------------
 * Types and Objects
 * --------------------------------------------------------------------
 */
#undef REGOFF
typedef int32 REGOFF;   /* offset into registry file */

typedef struct _desc
{
    REGOFF  location;   /* this object's offset (for verification) */
    REGOFF  name;       /* name string */
    uint16  namelen;    /* length of name string (including terminator) */
    uint16  type;       /* node type (key, or entry style) */
    REGOFF  left;       /* next object at this level (0 if none) */
    REGOFF  down;       /* KEY: first subkey        VALUE: 0 */
    REGOFF  value;      /* KEY: first entry object  VALUE: value string */
    uint32  valuelen;   /* KEY: 0  VALUE: length of value data */
    uint32  valuebuf;   /* KEY: 0  VALUE: length available */
    REGOFF  parent;     /* the node on the immediate level above */
} REGDESC;

/* offsets into structure on disk */
#define DESC_LOCATION   0
#define DESC_NAME       4
#define DESC_NAMELEN    8
#define DESC_TYPE       10
#define DESC_LEFT       12
#define DESC_DOWN       16
#define DESC_VALUE      20
#define DESC_VALUELEN   24
#define DESC_VALUEBUF   16    /* stored in place of "down" for entries */
#define DESC_PARENT     28

#define DESC_SIZE       32    /* size of desc on disk */

typedef struct _hdr
{
    uint32  magic;      /* must equal MAGIC_NUMBER */
    uint16  verMajor;   /* major version number */
    uint16  verMinor;   /* minor version number */
    REGOFF  avail;      /* next available offset */
    REGOFF  root;       /* root object */
} REGHDR;

/* offsets into structure on disk*/
#define HDR_MAGIC       0
#define HDR_VERMAJOR    4
#define HDR_VERMINOR    6
#define HDR_AVAIL       8
#define HDR_ROOT        12

typedef XP_File FILEHANDLE; /* platform-specific file reference */

typedef struct _stdnodes {
    REGOFF          versions;
    REGOFF          users;
    REGOFF          common;
    REGOFF          current_user;
    REGOFF          privarea;
} STDNODES;

typedef struct _regfile
{
    FILEHANDLE      fh;
    REGHDR          hdr;
    int             refCount;
    int             hdrDirty;
    int             inInit;
    int             readOnly;
    char *          filename;
    STDNODES        rkeys;
    struct _regfile *next;
    struct _regfile *prev;
#ifndef STANDALONE_REGISTRY
    PRLock          *lock;
    PRUint64        uniqkey;
#endif
} REGFILE;

typedef struct _reghandle
{
    uint32          magic;     /* for validating reg handles */
    REGFILE         *pReg;     /* the real registry file object */
} REGHANDLE;


#endif  /* _REG_H_ */

/* EOF: reg.h */

