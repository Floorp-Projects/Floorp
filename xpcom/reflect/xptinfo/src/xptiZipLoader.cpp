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

/* Implementation of xptiZipLoader. */

#include "xptiprivate.h"

XPTHeader*
xptiZipLoader::ReadXPTFileFromInputStream(nsIInputStream *stream,
                                          xptiWorkingSet* aWorkingSet)
{
    XPTCursor cursor;
    PRUint32 totalRead = 0;
    XPTState *state = nsnull;
    XPTHeader *header = nsnull;

    PRUint32 flen;
    stream->Available(&flen);
    
    char *whole = new char[flen];
    if (!whole)
    {
        return nsnull;
    }

    // all exits from on here should be via 'goto out' 

    while(flen - totalRead)
    {
        PRUint32 avail;
        PRUint32 read;

        if(NS_FAILED(stream->Available(&avail)))
        {
            goto out;
        }

        if(avail > flen)
        {
            goto out;
        }

        if(NS_FAILED(stream->Read(whole+totalRead, avail, &read)))
        {
            goto out;
        }

        totalRead += read;
    }
    
    // Go ahead and close the stream now.
    stream = nsnull;

    if(!(state = XPT_NewXDRState(XPT_DECODE, whole, flen)))
    {
        goto out;
    }
    
    if(!XPT_MakeCursor(state, XPT_HEADER, 0, &cursor))
    {
        goto out;
    }
    
    if (!XPT_DoHeader(aWorkingSet->GetStructArena(), &cursor, &header))
    {
        header = nsnull;
        goto out;
    }

 out:
    if(state)
        XPT_DestroyXDRState(state);
    if(whole)
        delete [] whole;
    return header;
}

