/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "nsSound.h"
#include "nsObjCExceptions.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsIURL.h"
#include "nsString.h"

#import <Cocoa/Cocoa.h>

NS_IMPL_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver)

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
                          PRUint32 dataLen,
                          const PRUint8 *data)
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

  NSString *name = [NSString stringWithCharacters:aSoundAlias.BeginReading()
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
nsSound::PlayEventSound(PRUint32 aEventId)
{
  // Mac doesn't have system sound settings for each user actions.
  return NS_OK;
}
