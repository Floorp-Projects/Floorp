/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsArena.h"
#include "nsCRT.h"

ArenaImpl::ArenaImpl(void)
  : mInitialized(PR_FALSE)
{
  NS_INIT_REFCNT();
  nsCRT::memset(&mPool, 0, sizeof(PLArenaPool));
}

NS_IMETHODIMP
ArenaImpl::Init(PRUint32 aBlockSize)
{
  if (aBlockSize < NS_MIN_ARENA_BLOCK_SIZE) {
    aBlockSize = NS_DEFAULT_ARENA_BLOCK_SIZE;
  }
  PL_INIT_ARENA_POOL(&mPool, "nsIArena", aBlockSize);
  mBlockSize = aBlockSize;
  mInitialized = PR_TRUE;
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(ArenaImpl, nsIArena)

ArenaImpl::~ArenaImpl()
{
  if (mInitialized)
    PL_FinishArenaPool(&mPool);

  mInitialized = PR_FALSE;
}

NS_IMETHODIMP_(void*)
ArenaImpl::Alloc(PRUint32 size)
{
  // Adjust size so that it's a multiple of sizeof(double)
  PRUint32 align = size & (sizeof(double) - 1);
  if (0 != align) {
    size += sizeof(double) - align;
  }

  void* p;
  PL_ARENA_ALLOCATE(p, &mPool, size);
  return p;
}

NS_METHOD
ArenaImpl::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;
  
  ArenaImpl* it = new ArenaImpl();
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(it);
  nsresult rv = it->QueryInterface(aIID, aResult);
  NS_RELEASE(it);
  return rv;
}

NS_COM nsresult NS_NewHeapArena(nsIArena** aInstancePtrResult,
                                 PRUint32 aArenaBlockSize)
{
  nsresult rv;
  nsIArena* arena;
  rv = ArenaImpl::Create(NULL, NS_GET_IID(nsIArena), (void**)&arena);
  if (NS_FAILED(rv)) return rv;
    
  rv = arena->Init(aArenaBlockSize);
  if (NS_FAILED(rv)) {
    NS_RELEASE(arena);
    return rv;
  }    
  
  *aInstancePtrResult = arena;
  return rv;
}
