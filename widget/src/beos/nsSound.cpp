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
#include <stdio.h>
#include "nsIURL.h"
#include "nsString.h"
#include "nsIFileURL.h"
#include "nsSound.h"
#include "nsNetUtil.h"

#include "nsDirectoryServiceDefs.h"

#include "nsNativeCharsetUtils.h"

#include <OS.h>
#include <SimpleGameSound.h>
#include <Beep.h>
#include <unistd.h>



NS_IMPL_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver)

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
 : mSound(0)
{
}

nsSound::~nsSound()
{
	Init();
}

NS_IMETHODIMP nsSound::OnStreamComplete(nsIStreamLoader *aLoader,
                                        nsISupports *context,
                                        nsresult aStatus,
                                        PRUint32 dataLen,
                                        const PRUint8 *data)
{
	// print a load error on bad status
	if (NS_FAILED(aStatus))
	{
#ifdef DEBUG
		printf("Failed load sound file");
#endif
		return aStatus;
	}
	// In theory, BeOS can play any sound format supported by MediaKit,
	// we can also play it from data, but it needs parsing format and 
	// providing it to sound player, so let MediaKit to do it by self
	static const char kSoundTmpFileName[] = "mozsound";
	nsCOMPtr<nsIFile> soundTmp;
	nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(soundTmp));
	NS_ENSURE_SUCCESS(rv, rv);
	rv = soundTmp->AppendNative(nsDependentCString(kSoundTmpFileName));
	NS_ENSURE_SUCCESS(rv, rv);
	nsCAutoString soundFilename;
	(void) soundTmp->GetNativePath(soundFilename);
	FILE *fp = fopen(soundFilename.get(), "wb+");
#ifdef DEBUG
	printf("Playing sound file%s\n",soundFilename.get());
#endif
	if (fp) 
	{
		fwrite(data, 1, dataLen, fp);
		fflush(fp);
		fclose(fp);
		Init();
		mSound = new BSimpleGameSound(soundFilename.get());
		if (mSound != NULL && mSound->InitCheck() == B_OK)
		{
			mSound->SetIsLooping(false);
			mSound->StartPlaying();
			rv = NS_OK;
		}
		else
		{
			rv = NS_ERROR_FAILURE;
		}
		unlink(soundFilename.get());
	}
	else
	{
		return Beep();
	}
	return rv;
}

NS_IMETHODIMP nsSound::Init(void)
{
	if (mSound)
	{
		mSound->StopPlaying();
		delete mSound;
		mSound = NULL;
	}
	return NS_OK;
}

NS_METHOD nsSound::Beep()
{
	::beep();
	return NS_OK;
}

NS_METHOD nsSound::Play(nsIURL *aURL)
{
	nsresult rv;
	nsCOMPtr<nsIStreamLoader> loader;
	rv = NS_NewStreamLoader(getter_AddRefs(loader), aURL, this);
	return rv;
}

NS_IMETHODIMP nsSound::PlaySystemSound(const nsAString &aSoundAlias)
{
	nsresult rv = NS_ERROR_FAILURE;
	if (NS_IsMozAliasSound(aSoundAlias)) {
		NS_WARNING("nsISound::playSystemSound is called with \"_moz_\" events, they are obsolete, use nsISound::playEventSound instead");
		if (aSoundAlias.Equals(NS_SYSSOUND_MAIL_BEEP))
			return Beep();
		return NS_OK;
	}
	nsCOMPtr <nsIURI> fileURI;
	// create a nsILocalFile and then a nsIFileURL from that
	nsCOMPtr <nsILocalFile> soundFile;
	rv = NS_NewLocalFile(aSoundAlias, PR_TRUE, 
    					getter_AddRefs(soundFile));
	NS_ENSURE_SUCCESS(rv, rv);
	rv = NS_NewFileURI(getter_AddRefs(fileURI), soundFile);
	NS_ENSURE_SUCCESS(rv, rv);
	nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(fileURI, &rv);
	NS_ENSURE_SUCCESS(rv, rv);
	rv = Play(fileURL);
	return rv;
}

NS_IMETHODIMP nsSound::PlayEventSound(PRUint32 aEventId)
{
  return aEventId == EVENT_NEW_MAIL_RECEIVED ? Beep() : NS_OK;
}
