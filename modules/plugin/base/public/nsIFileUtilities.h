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

#ifndef nsIFileUtilities_h___
#define nsIFileUtilities_h___

#include "nsplugindefs.h"
#include "nsISupports.h"

////////////////////////////////////////////////////////////////////////////////
// File Utilities Interface
// This interface reflects operations only available in Communicator 5.0.

#define NS_IFILEUTILITIES_IID                        \
{ /* 89a31ce0-019a-11d2-815b-006008119d7a */         \
    0x89a31ce0,                                      \
    0x019a,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

/**
 * The nsIFileUtilities interface provides access to random file operations.
 * To obtain: QueryInterface on nsIPluginManager.
 */
class nsIFileUtilities : public nsISupports {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFILEUTILITIES_IID)
    
    /**
     * Returns the name of the browser executable program.
     *
     * @param result - the returned path to the program
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetProgramPath(const char* *result) = 0;

    /**
     * Returns the name of the temporary directory.
     *
     * @param result - the returned path to the temp directory
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetTempDirPath(const char* *result) = 0;

    /**
     * Returns a unique temporary file name.
     *
     * @param prefix - a string to prefix to the temporary file name
     * @param bufLen - the length of the resultBuf to receive the data
     * @param resultBuf - the returned temp file name
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    NewTempFileName(const char* prefix, PRUint32 bufLen, char* resultBuf) = 0;

};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIFileUtilities_h___ */
