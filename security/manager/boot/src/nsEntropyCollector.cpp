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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
 *   Kai Engert <kaie@netscape.com>
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

#include "prlog.h"
#include "nsEntropyCollector.h"
#include "nsMemory.h"

nsEntropyCollector::nsEntropyCollector()
:mBytesCollected(0), mWritePointer(mEntropyCache)
{
  NS_INIT_ISUPPORTS();
}

nsEntropyCollector::~nsEntropyCollector()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsEntropyCollector,
                              nsIEntropyCollector,
                              nsIBufEntropyCollector)

NS_IMETHODIMP
nsEntropyCollector::RandomUpdate(void *new_entropy, PRInt32 bufLen)
{
  if (bufLen > 0) {
    if (mForwardTarget) {
      return mForwardTarget->RandomUpdate(new_entropy, bufLen);
    }
    else {
      const unsigned char *InputPointer = (const unsigned char *)new_entropy;
      const unsigned char *PastEndPointer = mEntropyCache + entropy_buffer_size;

      // if the input is large, we only take as much as we can store
      PRInt32 bytes_wanted = PR_MIN(bufLen, entropy_buffer_size);

      // remember the number of bytes we will have after storing new_entropy
      mBytesCollected = PR_MIN(entropy_buffer_size, mBytesCollected + bytes_wanted);

      // as the above statements limit bytes_wanted to the entropy_buffer_size,
      // this loop will iterate at most twice.
      while (bytes_wanted > 0) {

        // how many bytes to end of cyclic buffer?
        const PRInt32 space_to_end = PastEndPointer - mWritePointer;

        // how many bytes can we copy, not reaching the end of the buffer?
        const PRInt32 this_time = PR_MIN(space_to_end, bytes_wanted);

        // copy at most to the end of the cyclic buffer
        for (PRInt32 i = 0; i < this_time; ++i) {

          // accept the fact that we use our buffer's random uninitialized content
          unsigned int old = *mWritePointer;

          // combine new and old value already stored in buffer
          // this logic comes from PSM 1
          *mWritePointer++ = ((old << 1) | (old >> 7)) ^ *InputPointer++;
        }

        PR_ASSERT(mWritePointer <= PastEndPointer);
        PR_ASSERT(mWritePointer >= mEntropyCache);

        // have we arrived at the end of the buffer?
        if (PastEndPointer == mWritePointer) {
          // reset write pointer back to begining of our buffer
          mWritePointer = mEntropyCache;
        }

        // subtract the number of bytes we have already copied
        bytes_wanted -= this_time;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEntropyCollector::ForwardTo(nsIEntropyCollector *aCollector)
{
  NS_PRECONDITION(!mForwardTarget, "|ForwardTo| should only be called once.");

  mForwardTarget = aCollector;
  mForwardTarget->RandomUpdate(mEntropyCache, mBytesCollected);
  mBytesCollected = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsEntropyCollector::DontForward()
{
  mForwardTarget = nsnull;
  return NS_OK;
}
