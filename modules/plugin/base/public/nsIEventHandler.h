/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

////////////////////////////////////////////////////////////////////////////////
/**
 * <B>INTERFACE TO NETSCAPE COMMUNICATOR PLUGINS (NEW C++ API).</B>
 *
 * <P>This superscedes the old plugin API (npapi.h, npupp.h), and 
 * eliminates the need for glue files: npunix.c, npwin.cpp and npmac.cpp that
 * get linked with the plugin. You will however need to link with the "backward
 * adapter" (badapter.cpp) in order to allow your plugin to run in pre-5.0
 * browsers. 
 *
 * <P>See nsplugin.h for an overview of how this interface fits with the 
 * overall plugin architecture.
 */
////////////////////////////////////////////////////////////////////////////////

#ifndef nsIEventHandler_h___
#define nsIEventHandler_h___

#include "nsplugindefs.h"

////////////////////////////////////////////////////////////////////////////////
// Plugin Instance Interface

/**
 * The nsIEventHandler interface provides the callback to handle events.
 * It is implemented by the plugin, and is the superclass for nsIPluginInstance.
 */
class nsIEventHandler : public nsISupports {
public:

    /**
     * Handles an event. An nsIEventHandler can also get registered with with
     * nsIPluginManager2::RegisterWindow and will be called whenever an event
     * comes in for that window.
     *
     * Note that for Unix and Mac the nsPluginEvent structure is different
     * from the old NPEvent structure -- it's no longer the native event
     * record, but is instead a struct. This was done for future extensibility,
     * and so that the Mac could receive the window argument too. For Windows
     * and OS2, it's always been a struct, so there's no change for them.
     *
     * (Corresponds to NPP_HandleEvent.)
     *
     * @param event - the event to be handled
     * @param handled - set to PR_TRUE if event was handled
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    HandleEvent(nsPluginEvent* event, PRBool* handled) = 0;

};

#define NS_IEVENTHANDLER_IID                         \
{ /* a447ddf0-1a99-11d2-815f-006008119d7a */         \
    0xa447ddf0,                                      \
    0x1a99,                                          \
    0x11d2,                                          \
    {0x81, 0x5f, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIEventHandler_h___ */
