/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include "nsXPCOM.h"
#include "nsMemory.h"
#include "nsXPCOMPrivate.h"
#include "nsDebug.h"
#include "nsISupportsUtils.h"
#include "nsCOMPtr.h"

////////////////////////////////////////////////////////////////////////////////
// nsMemory static helper routines

NS_COM_GLUE nsresult
nsMemory::HeapMinimize(bool aImmediate)
{
    nsCOMPtr<nsIMemory> mem;
    nsresult rv = NS_GetMemoryManager(getter_AddRefs(mem));
    NS_ENSURE_SUCCESS(rv, rv);

    return mem->HeapMinimize(aImmediate);
}

NS_COM_GLUE void*
nsMemory::Clone(const void* ptr, PRSize size)
{
    void* newPtr = NS_Alloc(size);
    if (newPtr)
        memcpy(newPtr, ptr, size);
    return newPtr;
}

NS_COM_GLUE nsIMemory*
nsMemory::GetGlobalMemoryService()
{
    nsIMemory* mem;
    nsresult rv = NS_GetMemoryManager(&mem);
    if (NS_FAILED(rv)) return nsnull;
   
    return mem;
}

//----------------------------------------------------------------------

