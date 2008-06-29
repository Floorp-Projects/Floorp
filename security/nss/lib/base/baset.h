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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef BASET_H
#define BASET_H

#ifdef DEBUG
static const char BASET_CVS_ID[] = "@(#) $RCSfile: baset.h,v $ $Revision: 1.8 $ $Date: 2005/01/20 02:25:45 $";
#endif /* DEBUG */

/*
 * baset.h
 *
 * This file contains definitions for the basic types used throughout
 * nss but not available publicly.
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

#include "plhash.h"

PR_BEGIN_EXTERN_C

/*
 * nssArenaMark
 *
 * This type is used to mark the current state of an NSSArena.
 */

struct nssArenaMarkStr;
typedef struct nssArenaMarkStr nssArenaMark;

#ifdef DEBUG
/*
 * ARENA_THREADMARK
 * 
 * Optionally, this arena implementation can be compiled with some
 * runtime checking enabled, which will catch the situation where
 * one thread "marks" the arena, another thread allocates memory,
 * and then the mark is released.  Usually this is a surprise to
 * the second thread, and this leads to weird runtime errors.
 * Define ARENA_THREADMARK to catch these cases; we define it for all
 * (internal and external) debug builds.
 */
#define ARENA_THREADMARK

/*
 * ARENA_DESTRUCTOR_LIST
 *
 * Unfortunately, our pointer-tracker facility, used in debug
 * builds to agressively fight invalid pointers, requries that
 * pointers be deregistered when objects are destroyed.  This
 * conflicts with the standard arena usage where "memory-only"
 * objects (that don't hold onto resources outside the arena)
 * can be allocated in an arena, and never destroyed other than
 * when the arena is destroyed.  Therefore we have added a
 * destructor-registratio facility to our arenas.  This was not
 * a simple decision, since we're getting ever-further away from
 * the original arena philosophy.  However, it was felt that
 * adding this in debug builds wouldn't be so bad; as it would
 * discourage them from being used for "serious" purposes.
 * This facility requires ARENA_THREADMARK to be defined.
 */
#ifdef ARENA_THREADMARK
#define ARENA_DESTRUCTOR_LIST
#endif /* ARENA_THREADMARK */

#endif /* DEBUG */

typedef struct nssListStr nssList;
typedef struct nssListIteratorStr nssListIterator;
typedef PRBool (* nssListCompareFunc)(void *a, void *b);
typedef PRIntn (* nssListSortFunc)(void *a, void *b);
typedef void (* nssListElementDestructorFunc)(void *el);

typedef struct nssHashStr nssHash;
typedef void (PR_CALLBACK *nssHashIterator)(const void *key, 
                                            void *value, 
                                            void *arg);

/*
 * nssPointerTracker
 *
 * This type is used in debug builds (both external and internal) to
 * track our object pointers.  Objects of this type must be statically
 * allocated, which means the structure size must be available to the
 * compiler.  Therefore we must expose the contents of this structure.
 * But please don't access elements directly; use the accessors.
 */

#ifdef DEBUG
struct nssPointerTrackerStr {
  PRCallOnceType once;
  PZLock *lock;
  PLHashTable *table;
};
typedef struct nssPointerTrackerStr nssPointerTracker;
#endif /* DEBUG */

/*
 * nssStringType
 *
 * There are several types of strings in the real world.  We try to
 * use only UTF8 and avoid the rest, but that's not always possible.
 * So we have a couple converter routines to go to and from the other
 * string types.  We have to be able to specify those string types,
 * so we have this enumeration.
 */

enum nssStringTypeEnum {
  nssStringType_DirectoryString,
  nssStringType_TeletexString, /* Not "teletext" with trailing 't' */
  nssStringType_PrintableString,
  nssStringType_UniversalString,
  nssStringType_BMPString,
  nssStringType_UTF8String,
  nssStringType_PHGString,
  nssStringType_GeneralString,

  nssStringType_Unknown = -1
};
typedef enum nssStringTypeEnum nssStringType;

PR_END_EXTERN_C

#endif /* BASET_H */
