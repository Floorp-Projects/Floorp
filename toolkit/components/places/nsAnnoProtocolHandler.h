//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAnnoProtocolHandler_h___
#define nsAnnoProtocolHandler_h___

#include "nsCOMPtr.h"
#include "nsIAnnotationService.h"
#include "nsIProtocolHandler.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "mozilla/Attributes.h"

// {e8b8bdb7-c96c-4d82-9c6f-2b3c585ec7ea}
#define NS_ANNOPROTOCOLHANDLER_CID \
{ 0xe8b8bdb7, 0xc96c, 0x4d82, { 0x9c, 0x6f, 0x2b, 0x3c, 0x58, 0x5e, 0xc7, 0xea } }

class nsAnnoProtocolHandler MOZ_FINAL : public nsIProtocolHandler, public nsSupportsWeakReference
{
public:
  nsAnnoProtocolHandler() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER

private:
  ~nsAnnoProtocolHandler() {}

protected:
  nsresult ParseAnnoURI(nsIURI* aURI, nsIURI** aResultURI, nsCString& aName);

  /**
   * Obtains a new channel to be used to get a favicon from the database.  This
   * method is asynchronous.
   *
   * @param aURI
   *        The URI the channel will be created for.  This is the URI that is
   *        set as the original URI on the channel.
   * @param aAnnotationURI
   *        The URI that holds the data needed to get the favicon from the
   *        database.
   * @returns (via _channel) the channel that will obtain the favicon data.
   */
  nsresult NewFaviconChannel(nsIURI *aURI,
                             nsIURI *aAnnotationURI,
                             nsIChannel **_channel);
};

#endif /* nsAnnoProtocolHandler_h___ */
