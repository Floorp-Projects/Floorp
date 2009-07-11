/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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


#include "nscore.h"
#include "plstr.h"
#include "prlink.h"

#include "nsSound.h"
#include "nsString.h"

#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"

#include <Pt.h>

NS_IMPL_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver)

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
  mInited = PR_FALSE;
}

nsSound::~nsSound()
{
}

nsresult nsSound::Init()
{
  if (mInited) return NS_OK;

  mInited = PR_TRUE;
  return NS_OK;
}

NS_METHOD nsSound::Beep()
{
  ::PtBeep();
  return NS_OK;
}

NS_METHOD nsSound::Play(nsIURL *aURL)
{
  NS_NOTYETIMPLEMENTED("nsSound::Play");

#ifdef DEBUG
printf( "\n\n\nnsSound::Play\n\n" );
#endif

  return NS_OK;
}

NS_IMETHODIMP nsSound::OnStreamComplete(nsIStreamLoader *aLoader,
                                        nsISupports *context,
                                        nsresult aStatus,
                                        PRUint32 stringLen,
                                        const PRUint8 *stringData)
{
  nsresult rv = NS_ERROR_FAILURE;

#ifdef DEBUG
printf( "\n\n\nnsSound::OnStreamComplete stringData=%s\n\n", stringData );
#endif

  if (NS_FAILED(aStatus))
    return NS_ERROR_FAILURE;

  return rv;
}

static void child_exit( void *data, int status ) { }

NS_IMETHODIMP nsSound::PlaySystemSound(const nsAString &aSoundAlias)
{
  NS_ConvertUTF16toUTF8 utf8SoundAlias(aSoundAlias);

#ifdef DEBUG
printf( "\n\n\nnsSound::PlaySystemSound aSoundAlias=%s\n\n",
        utf8SoundAlias.get() );
#endif

  const char *soundfile;

  if( NS_IsMozAliasSound(aSoundAlias) ) {
    NS_WARNING("nsISound::playSystemSound is called with \"_moz_\" events, they are obsolete, use nsISound::playEventSound instead");
    if ( aSoundAlias.Equals(NS_SYSSOUND_MAIL_BEEP) )
      soundfile = "/usr/share/mozilla/gotmail.wav";
    else
      return NS_OK;
  } else {
    /* the aSoundAlias is the fullpath to the soundfile */
    if( !access( utf8SoundAlias.get(), F_OK ) )
      soundfile = utf8SoundAlias.get();
    else
      soundfile = "/usr/share/mozilla/rest.wav";
  }

  const char* argv[] = { "/opt/Mozilla/mozilla/wave", soundfile, NULL };
  PtSpawn( "/opt/Mozilla/mozilla/wave", ( const char ** ) argv,
           NULL, NULL, child_exit, NULL, NULL );

  return NS_OK;
}

NS_IMETHODIMP nsSound::PlayEventSound(PRUint32 aEventId)
{
  if (aEventId != EVENT_NEW_MAIL_RECEIVED) {
    return NS_OK;
  }

  soundfile = "/usr/share/mozilla/gotmail.wav";
  const char* argv[] = { "/opt/Mozilla/mozilla/wave",
                         "/usr/share/mozilla/gotmail.wav", NULL };
  PtSpawn( "/opt/Mozilla/mozilla/wave", ( const char ** ) argv,
           NULL, NULL, child_exit, NULL, NULL );

  return NS_OK;
}
