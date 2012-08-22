/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "nscore.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"

/*
 * Fully reads the required amount of data. Keeps reading until all the
 * data is retrieved or an error is hit.
 */
NS_HIDDEN_(nsresult) ZW_ReadData(nsIInputStream *aStream, char *aBuffer, uint32_t aCount)
{
    while (aCount > 0) {
        uint32_t read;
        nsresult rv = aStream->Read(aBuffer, aCount, &read);
        NS_ENSURE_SUCCESS(rv, rv);
        aCount -= read;
        aBuffer += read;
        // If we hit EOF before reading the data we need then throw.
        if (read == 0 && aCount > 0)
            return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

/*
 * Fully writes the required amount of data. Keeps writing until all the
 * data is written or an error is hit.
 */
NS_HIDDEN_(nsresult) ZW_WriteData(nsIOutputStream *aStream, const char *aBuffer,
                                  uint32_t aCount)
{
    while (aCount > 0) {
        uint32_t written;
        nsresult rv = aStream->Write(aBuffer, aCount, &written);
        NS_ENSURE_SUCCESS(rv, rv);
        if (written <= 0)
            return NS_ERROR_FAILURE;
        aCount -= written;
        aBuffer += written;
    }

    return NS_OK;
}
