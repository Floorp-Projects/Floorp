/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef _nsZipDataStream_h_
#define _nsZipDataStream_h_

#include "nsZipWriter.h"
#include "nsIOutputStream.h"
#include "nsIStreamListener.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

class nsZipDataStream final : public nsIStreamListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsZipDataStream() {}

  nsresult Init(nsZipWriter* aWriter, nsIOutputStream* aStream,
                nsZipHeader* aHeader, int32_t aCompression);

  nsresult ReadStream(nsIInputStream* aStream);

 private:
  ~nsZipDataStream() {}

  nsCOMPtr<nsIStreamListener> mOutput;
  nsCOMPtr<nsIOutputStream> mStream;
  RefPtr<nsZipWriter> mWriter;
  RefPtr<nsZipHeader> mHeader;

  nsresult CompleteEntry();
  nsresult ProcessData(nsIRequest* aRequest, nsISupports* aContext,
                       char* aBuffer, uint64_t aOffset, uint32_t aCount);
};

#endif
