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
 * The Original Code is the PKIX-C library.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are
 * Copyright 2004-2007 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems, Inc.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
/*
 * pkix_pl_object.h
 *
 * Object Construction, Destruction and Callback Definitions
 *
 */

#ifndef _PKIX_PL_OBJECT_H
#define _PKIX_PL_OBJECT_H

#include "pkix_pl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Object Implementation Notes:
 *
 * Allocating a new object creates an object header and a block of
 * uninitialized user data. A pointer to this uninitialized data is
 * returned to the user. The structure looks as follows:
 *
 * +--------------------+
 * | MAGIC HEADER       |
 * | (object header)    |
 * +--------------------+
 * | user data          | -- pointer returned from PKIX_PL_Object_Alloc
 * +--------------------+
 *
 * Object operations receive a pointer to raw user data as an argument.
 * The macro HEADER(object) returns a pointer to the object header.
 * An assertion then verifies that the first field is the MAGIC_HEADER.
 */

/* PKIX_PL_Object Structure Definition */
struct PKIX_PL_ObjectStruct {
        PKIX_UInt32 magicHeader;
        PKIX_UInt32 type;
        PKIX_Int32 references;
        PRLock *lock;
        PKIX_PL_String *stringRep;
        PKIX_UInt32 hashcode;
        PKIX_Boolean hashcodeCached;
};

/* see source file for function documentation */

PKIX_Error *
pkix_pl_Object_RetrieveEqualsCallback(
        PKIX_PL_Object *object,
        PKIX_PL_EqualsCallback *equalsCallback,
        void *plContext);

extern PKIX_Boolean initializing;
extern PKIX_Boolean initialized;

#ifdef PKIX_USER_OBJECT_TYPE

extern PRLock *classTableLock;

#endif

extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];

PKIX_Error *
pkix_pl_Object_RegisterSelf(void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_PL_OBJECT_H */
