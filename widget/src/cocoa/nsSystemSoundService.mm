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
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#include "nsSystemSoundService.h"
#include "nsObjCExceptions.h"

#import <Cocoa/Cocoa.h>

/*****************************************************************************
 *  nsSystemSoundService implementation
 *****************************************************************************/

NS_IMPL_ISUPPORTS1(nsSystemSoundService, nsISystemSoundService)

NS_IMPL_ISYSTEMSOUNDSERVICE_GETINSTANCE(nsSystemSoundService)

nsSystemSoundService::nsSystemSoundService() :
  nsSystemSoundServiceBase()
{
}

nsSystemSoundService::~nsSystemSoundService()
{
}

NS_IMETHODIMP
nsSystemSoundService::Beep()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult rv = nsSystemSoundServiceBase::Beep();
  NS_ENSURE_SUCCESS(rv, rv);

  NSBeep();
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsSystemSoundService::PlayAlias(const nsAString &aSoundAlias)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult rv = nsSystemSoundServiceBase::PlayAlias(aSoundAlias);
  NS_ENSURE_SUCCESS(rv, rv);

  NSString *name = [NSString stringWithCharacters:aSoundAlias.BeginReading()
                                           length:aSoundAlias.Length()];
  NSSound *sound = [NSSound soundNamed:name];
  if (sound) {
    StopSoundPlayer();
    [sound stop];
    [sound play];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsSystemSoundService::PlayEventSound(PRUint32 aEventID)
{
  nsresult rv = nsSystemSoundServiceBase::PlayEventSound(aEventID);
  NS_ENSURE_SUCCESS(rv, rv);

  // Mac doesn't have system sound settings for each user actions.
  return NS_OK;
}
