/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C) 1999
 * Christopher Blizzard.  All Rights Reserved.
 */

#ifndef nsIUnixToolkitService_h__
#define nsIUnixToolkitService_h__

#include "nsISupports.h"
#include "nsString.h"

// Interface id for the UnixWindow service
// { 7EA38EF1-44D5-11d3-B21C-000064657374 }
#define NS_UNIX_TOOLKIT_SERVICE_IID        \
{ 0x7ea38ef1, 0x44d5, 0x11d3, \
  { 0xb2, 0x1c, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

// Class ID for our implementation
// { 7EA38EF2-44D5-11d3-B21C-000064657374 }
#define NS_UNIX_TOOLKIT_SERVICE_CID \
{ 0x7ea38ef2, 0x44d5, 0x11d3, \
  { 0xb2, 0x1c, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

#define NS_UNIX_TOOLKIT_SERVICE_PROGID "component://netscape/widget/unix_services/toolkit_service"

/**
 * This is an interface for getting access to many toolkit 
 * support stuff needed to embed and other fancy stuff.
 * @created Wed Jul 28 1999
 * @author  Ramiro Estrugo <ramiro@netscape.com>
 */

class nsITimer;

class nsIUnixToolkitService : public nsISupports
{
 public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_UNIX_TOOLKIT_SERVICE_IID);


  /**
   * Set the name of the toolkit to use.  It is usually not required to 
   * ever call this method, since the toolkit name will be fetched 
   * automatically from the users environment.  The environment variable
   * MOZ_TOOLKIT is checked.  If this variable is not set, then a default
   * toolkit is picked.  Currently the default toolkit is gtk.
   *
   * @param [IN] aToolkitName The name of the toolkit to use.
   *        
   */
  NS_IMETHOD SetToolkitName(const nsString & aToolkitName) = 0;

  
  /**
   * Check whether a toolkit name is valid.  Currently, the following are
   * valid toolkits:
   *
   * gtk, motif, xlib, qt
   *
   * @param [IN] aToolkitName The name of the toolkit to to check.
   * @param [OUT] aIsValidOut PRBool value that is true if aToolkitName
   * is valid.
   *        
   */
  NS_IMETHOD IsValidToolkit(const nsString & aToolkitName,
                            PRBool *         aResultOut) = 0;

  /**
   * Get the name of the toolkit currently being used.  The toolkit name
   * will be one of: {gtk,motif,xlib}
   *
   * @param [OUT] aToolkitNameOut On return it holds the toolkit name.
   *
   */
  NS_IMETHOD GetToolkitName(nsString & aToolkitNameOut) = 0;

  /**
   * Get the name of the widget dll.  The widget dll will be something like:
   * libwidget_{gtk,motif,xlib,qt}.so
   *
   * @param [OUT] aWidgetDllNameOut On return it holds the widget dll name.
   *        
   */
  NS_IMETHOD GetWidgetDllName(nsString & aWidgetDllNameOut) = 0;

  /**
   * Get the name of the gfx dll.  The gfx dll will be something like: 
   * libgfx_{gtk,motif,xlib,qt}.so
   *
   * @param [OUT] aGfxDllNameOut On return it holds the gfx dll name.
   *        
   * The gfx dll will be something like: libgfx_{gtk,motif,xlib,qt}.so
   */
  NS_IMETHOD GetGfxDllName(nsString & aGfxDllNameOut) = 0;

  /**
   * Get the CID of the timer class associated with the toolkit in use.
   * The CID can then be used to create instances of a timer that will
   * work with the toolkit in use.
   *
   * @param [OUT] aTimerCIDOut On return it holds a pointer to the CID.
   *        
   */
  NS_IMETHOD GetTimerCID(nsCID ** aTimerCIDOut) = 0;
};

#endif /* nsIUnixToolkitService_h__ */
