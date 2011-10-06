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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
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

#ifndef nsAboutCacheEntry_h__
#define nsAboutCacheEntry_h__

#include "nsIAboutModule.h"
#include "nsIChannel.h"
#include "nsICacheListener.h"
#include "nsICacheSession.h"
#include "nsICacheEntryDescriptor.h"
#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
#include "nsIInputStreamChannel.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsICacheEntryDescriptor;

class nsAboutCacheEntry : public nsIAboutModule
                        , public nsICacheMetaDataVisitor
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIABOUTMODULE
    NS_DECL_NSICACHEMETADATAVISITOR

    nsAboutCacheEntry()
        : mBuffer(nsnull)
    {}

    virtual ~nsAboutCacheEntry() {}

private:
    nsresult GetContentStream(nsIURI *, nsIInputStream **);
    nsresult OpenCacheEntry(nsIURI *, nsICacheEntryDescriptor **);
    nsresult WriteCacheEntryDescription(nsIOutputStream *, nsICacheEntryDescriptor *);
    nsresult WriteCacheEntryUnavailable(nsIOutputStream *);
    nsresult ParseURI(nsIURI *, nsCString &, bool &, nsCString &);

private:
    nsCString *mBuffer;
};

#define NS_ABOUT_CACHE_ENTRY_MODULE_CID              \
{ /* 7fa5237d-b0eb-438f-9e50-ca0166e63788 */         \
    0x7fa5237d,                                      \
    0xb0eb,                                          \
    0x438f,                                          \
    {0x9e, 0x50, 0xca, 0x01, 0x66, 0xe6, 0x37, 0x88} \
}

#endif // nsAboutCacheEntry_h__
