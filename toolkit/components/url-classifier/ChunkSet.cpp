//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Url Classifier code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
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

#include "ChunkSet.h"

namespace mozilla {
namespace safebrowsing {

nsresult
ChunkSet::Serialize(nsACString& aChunkStr)
{
  aChunkStr.Truncate();

  PRUint32 i = 0;
  while (i < mChunks.Length()) {
    if (i != 0) {
      aChunkStr.Append(',');
    }
    aChunkStr.AppendInt((PRInt32)mChunks[i]);

    PRUint32 first = i;
    PRUint32 last = first;
    i++;
    while (i < mChunks.Length() && (mChunks[i] == mChunks[i - 1] + 1 || mChunks[i] == mChunks[i - 1])) {
      last = i++;
    }

    if (last != first) {
      aChunkStr.Append('-');
      aChunkStr.AppendInt((PRInt32)mChunks[last]);
    }
  }

  return NS_OK;
}

nsresult
ChunkSet::Set(PRUint32 aChunk)
{
  PRUint32 idx = mChunks.BinaryIndexOf(aChunk);
  if (idx == nsTArray<uint32>::NoIndex) {
    mChunks.InsertElementSorted(aChunk);
  }
  return NS_OK;
}

nsresult
ChunkSet::Unset(PRUint32 aChunk)
{
  mChunks.RemoveElementSorted(aChunk);

  return NS_OK;
}

bool
ChunkSet::Has(PRUint32 aChunk) const
{
  return mChunks.BinaryIndexOf(aChunk) != nsTArray<uint32>::NoIndex;
}

nsresult
ChunkSet::Merge(const ChunkSet& aOther)
{
  const uint32 *dupIter = aOther.mChunks.Elements();
  const uint32 *end = aOther.mChunks.Elements() + aOther.mChunks.Length();

  for (const uint32 *iter = dupIter; iter != end; iter++) {
    nsresult rv = Set(*iter);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
ChunkSet::Remove(const ChunkSet& aOther)
{
  uint32 *addIter = mChunks.Elements();
  uint32 *end = mChunks.Elements() + mChunks.Length();

  for (uint32 *iter = addIter; iter != end; iter++) {
    if (!aOther.Has(*iter)) {
      *addIter = *iter;
      addIter++;
    }
  }

  mChunks.SetLength(addIter - mChunks.Elements());

  return NS_OK;
}

void
ChunkSet::Clear()
{
  mChunks.Clear();
}

}
}
