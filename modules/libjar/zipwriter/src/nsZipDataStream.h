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

class nsZipDataStream : public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    nsZipDataStream()
    {
    }

    nsresult Init(nsZipWriter *aWriter, nsIOutputStream *aStream,
                  nsZipHeader *aHeader, PRInt32 aCompression);

    nsresult ReadStream(nsIInputStream *aStream);

private:

    nsCOMPtr<nsIStreamListener> mOutput;
    nsCOMPtr<nsIOutputStream> mStream;
    nsRefPtr<nsZipWriter> mWriter;
    nsRefPtr<nsZipHeader> mHeader;

    nsresult CompleteEntry();
    nsresult ProcessData(nsIRequest *aRequest, nsISupports *aContext,
                         char *aBuffer, PRUint32 aOffset, PRUint32 aCount);
};

#endif
