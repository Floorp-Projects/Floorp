/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#include "nsWidgetDefs.h"
#include "nsSound.h"

NS_IMPL_ISUPPORTS(nsSound,nsISound::GetIID())

nsSound::nsSound()
{}

nsSound::~nsSound()
{}

nsresult nsSound::Beep()
{
   WinAlarm( HWND_DESKTOP, WA_WARNING);
   return NS_OK;
}

// This function should NOT exist -- how do people expect to componentize
// widget & gfx if they keep introducing static functions.

#if 0
nsresult NS_NewSound( nsISound** aSound)
{
   if( !aSound)
      return NS_ERROR_NULL_POINTER;

   nsSound *pSound = new nsSound;
   return pSound->QueryInterface( nsISound::GetIID(), (void**) aSound);
}
#endif
