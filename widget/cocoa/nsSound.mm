/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSound.h"
#include "nsObjCExceptions.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsIURL.h"
#include "nsString.h"

#import <Cocoa/Cocoa.h>

NS_IMPL_ISUPPORTS(nsSound, nsISound, nsIStreamLoaderObserver)

nsSound::nsSound()
{
}

nsSound::~nsSound()
{
}

NS_IMETHODIMP
nsSound::Beep()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSBeep();
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsSound::OnStreamComplete(nsIStreamLoader *aLoader,
                          nsISupports *context,
                          nsresult aStatus,
                          uint32_t dataLen,
                          const uint8_t *data)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSData *value = [NSData dataWithBytes:data length:dataLen];

  NSSound *sound = [[NSSound alloc] initWithData:value];

  [sound play];

  [sound autorelease];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsSound::Play(nsIURL *aURL)
{
  nsCOMPtr<nsIURI> uri(do_QueryInterface(aURL));
  nsCOMPtr<nsIStreamLoader> loader;
  return NS_NewStreamLoader(getter_AddRefs(loader), uri, this);
}

NS_IMETHODIMP
nsSound::Init()
{
  return NS_OK;
}

NS_IMETHODIMP
nsSound::PlaySystemSound(const nsAString &aSoundAlias)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (NS_IsMozAliasSound(aSoundAlias)) {
    NS_WARNING("nsISound::playSystemSound is called with \"_moz_\" events, they are obsolete, use nsISound::playEventSound instead");
    // Mac doesn't have system sound settings for each user actions.
    return NS_OK;
  }

  NSString *name = [NSString stringWithCharacters:reinterpret_cast<const unichar*>(aSoundAlias.BeginReading())
                                           length:aSoundAlias.Length()];
  NSSound *sound = [NSSound soundNamed:name];
  if (sound) {
    [sound stop];
    [sound play];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsSound::PlayEventSound(uint32_t aEventId)
{
  // Mac doesn't have system sound settings for each user actions.
  return NS_OK;
}
