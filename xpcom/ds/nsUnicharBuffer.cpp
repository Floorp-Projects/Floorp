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

#include "nsUnicharBuffer.h"
#include "nsCRT.h"

#define MIN_BUFFER_SIZE 32

UnicharBufferImpl::UnicharBufferImpl()
  : mBuffer(NULL), mSpace(0), mLength(0)
{
}

NS_METHOD
UnicharBufferImpl::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  UnicharBufferImpl* it = new UnicharBufferImpl();
  if (it == nsnull) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(it);
  nsresult rv = it->QueryInterface(aIID, aResult);
  NS_RELEASE(it);
  return rv;
}

NS_IMETHODIMP
UnicharBufferImpl::Init(PRUint32 aBufferSize)
{
  if (aBufferSize < MIN_BUFFER_SIZE) {
    aBufferSize = MIN_BUFFER_SIZE;
  }
  mSpace = aBufferSize;
  mLength = 0;
  mBuffer = new PRUnichar[aBufferSize];
  return mBuffer ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMPL_ISUPPORTS1(UnicharBufferImpl, nsIUnicharBuffer)

UnicharBufferImpl::~UnicharBufferImpl()
{
  if (nsnull != mBuffer) {
    delete[] mBuffer;
    mBuffer = nsnull;
  }
  mLength = 0;
}

NS_IMETHODIMP_(PRInt32)
UnicharBufferImpl::GetLength() const
{
  return mLength;
}

NS_IMETHODIMP_(PRInt32)
UnicharBufferImpl::GetBufferSize() const
{
  return mSpace;
}

NS_IMETHODIMP_(PRUnichar*)
UnicharBufferImpl::GetBuffer() const
{
  return mBuffer;
}

NS_IMETHODIMP_(PRBool)
UnicharBufferImpl::Grow(PRInt32 aNewSize)
{
  if (PRUint32(aNewSize) < MIN_BUFFER_SIZE) {
    aNewSize = MIN_BUFFER_SIZE;
  }
  PRUnichar* newbuf = new PRUnichar[aNewSize];
  if (nsnull != newbuf) {
    if (0 != mLength) {
      memcpy(newbuf, mBuffer, mLength * sizeof(PRUnichar));
    }
    delete[] mBuffer;
    mBuffer = newbuf;
    return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult
NS_NewUnicharBuffer(nsIUnicharBuffer** aInstancePtrResult,
                    nsISupports* aOuter,
                    PRUint32 aBufferSize)
{
  nsresult rv;
  nsIUnicharBuffer* buf;
  rv = UnicharBufferImpl::Create(aOuter, NS_GET_IID(nsIUnicharBuffer), 
                                 (void**)&buf);
  if (NS_FAILED(rv)) return rv;
  rv = buf->Init(aBufferSize);
  if (NS_FAILED(rv)) {
    NS_RELEASE(buf);
    return rv;
  }
  *aInstancePtrResult = buf;
  return rv;
}
