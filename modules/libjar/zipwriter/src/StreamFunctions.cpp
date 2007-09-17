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
 * The Original Code is Zip Writer Component.
 *
 * The Initial Developer of the Original Code is
 * Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Mook <mook.moz+random.code@gmail.com>
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
 * ***** END LICENSE BLOCK *****
 */

#include "nscore.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"

/*
 * Fully reads the required amount of data. Keeps reading until all the
 * data is retrieved or an error is hit.
 */
NS_HIDDEN_(nsresult) ZW_ReadData(nsIInputStream *aStream, char *aBuffer, PRUint32 aCount)
{
    while (aCount > 0) {
        PRUint32 read;
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
 * Fully writes the required amount of data. Keeps writing untill all the
 * data is written or an error is hit.
 */
NS_HIDDEN_(nsresult) ZW_WriteData(nsIOutputStream *aStream, const char *aBuffer,
                                  PRUint32 aCount)
{
    while (aCount > 0) {
        PRUint32 written;
        nsresult rv = aStream->Write(aBuffer, aCount, &written);
        NS_ENSURE_SUCCESS(rv, rv);
        if (written <= 0)
            return NS_ERROR_FAILURE;
        aCount -= written;
        aBuffer += written;
    }

    return NS_OK;
}
