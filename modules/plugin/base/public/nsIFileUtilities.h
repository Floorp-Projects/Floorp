/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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

#ifndef nsIFileUtilities_h___
#define nsIFileUtilities_h___

#include "nsplugindefs.h"

////////////////////////////////////////////////////////////////////////////////
// File Utilities Interface
// This interface reflects operations only available in Communicator 5.0.

class nsIFileUtilities : public nsISupports {
public:

    // QueryInterface on nsIPluginManager to get this.
    
    NS_IMETHOD_(const char*)
    GetProgramPath(void) = 0;

    NS_IMETHOD_(const char*)
    GetTempDirPath(void) = 0;

    enum FileNameType { SIGNED_APPLET_DBNAME, TEMP_FILENAME };

    NS_IMETHOD_(nsresult)
    GetFileName(const char* fn, FileNameType type,
                char* resultBuf, PRUint32 bufLen) = 0;

    NS_IMETHOD_(nsresult)
    NewTempFileName(const char* prefix, char* resultBuf, PRUint32 bufLen) = 0;

};

#define NS_IFILEUTILITIES_IID                        \
{ /* 89a31ce0-019a-11d2-815b-006008119d7a */         \
    0x89a31ce0,                                      \
    0x019a,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIFileUtilities_h___ */
