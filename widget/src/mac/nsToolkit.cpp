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

#include "nsToolkit.h"

#include "nsIEventSink.h"

#include "nsWidgetSupport.h"

#include <Gestalt.h>
#include <Movies.h>



//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()
{
  ::EnterMovies();	// Required for nsSound to Work! -- fix for bug 194632
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{ 
  ::ExitMovies();	// done with Movie Toolbox -- fix for bug 194632
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsresult
nsToolkit::InitEventQueue(PRThread * aThread)
{
  return NS_OK;
}


//
// Return the OS X version as returned from Gestalt(gestaltSystemVersion, ...)
//
long
nsToolkit :: OSXVersion()
{
  static long gOSXVersion = 0x0;
  if (gOSXVersion == 0x0) {
    OSErr err = ::Gestalt(gestaltSystemVersion, &gOSXVersion);
    if (err != noErr) {
      NS_ERROR("Couldn't determine OS X version, assume 10.0");
      gOSXVersion = MAC_OS_X_VERSION_10_0_HEX;
    }
  }
  return gOSXVersion;
}


//
// GetTopWidget
//
// We've stashed the nsIWidget for the given windowPtr in the data 
// properties of the window. Fetch it.
//
void
nsToolkit::GetTopWidget ( WindowPtr aWindow, nsIWidget** outWidget )
{
  nsIWidget* topLevelWidget = nsnull;
  ::GetWindowProperty ( aWindow,
      kTopLevelWidgetPropertyCreator, kTopLevelWidgetRefPropertyTag,
      sizeof(nsIWidget*), nsnull, (void*)&topLevelWidget);
  if ( topLevelWidget ) {
    *outWidget = topLevelWidget;
    NS_ADDREF(*outWidget);
  }
}


//
// GetWindowEventSink
//
// We've stashed the nsIEventSink for the given windowPtr in the data 
// properties of the window. Fetch it.
//
void
nsToolkit::GetWindowEventSink ( WindowPtr aWindow, nsIEventSink** outSink )
{
  *outSink = nsnull;
  
  nsCOMPtr<nsIWidget> topWidget;
  GetTopWidget ( aWindow, getter_AddRefs(topWidget) );
  nsCOMPtr<nsIEventSink> sink ( do_QueryInterface(topWidget) );
  if ( sink ) {
    *outSink = sink;
    NS_ADDREF(*outSink);
  }
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsToolkitBase* NS_CreateToolkitInstance()
{
  return new nsToolkit();
}
