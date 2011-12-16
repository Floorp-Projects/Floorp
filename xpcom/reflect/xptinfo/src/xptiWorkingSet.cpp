/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike McCabe <mccabe@netscape.com>
 *   John Bandhauer <jband@netscape.com>
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

/* Implementation of xptiWorkingSet. */

#include "xptiprivate.h"
#include "nsString.h"
#include "nsIMemoryReporter.h"

using namespace mozilla;

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(XPTMallocSizeOf, "xpti-working-set")

static PRInt64 GetXPTArenaSize()
{
  return XPT_SizeOfArena(gXPTIStructArena, XPTMallocSizeOf);
}

NS_MEMORY_REPORTER_IMPLEMENT(xptiWorkingSet,
                             "explicit/xpti-working-set",
                             KIND_HEAP,
                             UNITS_BYTES,
                             GetXPTArenaSize,
                             "Memory used by the XPCOM typelib system.")

#define XPTI_STRUCT_ARENA_BLOCK_SIZE    (1024 * 1)
#define XPTI_HASHTABLE_SIZE             2048

xptiWorkingSet::xptiWorkingSet()
    : mTableReentrantMonitor("xptiWorkingSet::mTableReentrantMonitor")
{
    MOZ_COUNT_CTOR(xptiWorkingSet);

    mIIDTable.Init(XPTI_HASHTABLE_SIZE);
    mNameTable.Init(XPTI_HASHTABLE_SIZE);

    gXPTIStructArena = XPT_NewArena(XPTI_STRUCT_ARENA_BLOCK_SIZE, sizeof(double),
                                    "xptiWorkingSet structs");

    NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(xptiWorkingSet));
}        

static PLDHashOperator
xpti_Invalidator(const char* keyname, xptiInterfaceEntry* entry, void* arg)
{
    entry->LockedInvalidateInterfaceInfo();
    return PL_DHASH_NEXT;
}

void 
xptiWorkingSet::InvalidateInterfaceInfos()
{
    ReentrantMonitorAutoEnter monitor(mTableReentrantMonitor);
    mNameTable.EnumerateRead(xpti_Invalidator, NULL);
}        

xptiWorkingSet::~xptiWorkingSet()
{
    MOZ_COUNT_DTOR(xptiWorkingSet);

    // Only destroy the arena if we're doing leak stats. Why waste shutdown
    // time touching pages if we don't have to?
#ifdef NS_FREE_PERMANENT_DATA
    XPT_DestroyArena(gXPTIStructArena);
#endif
}

XPTArena* gXPTIStructArena;
