/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
package netscape.softupdate ;

import netscape.softupdate.*;
import netscape.util.*;
import java.lang.*;

/**
 * WinProfileItem
 * Writes a single item into a Windows .INI file.
 */

final class WinProfileItem extends InstallObject {

    private WinProfile profile; // initiating profile object
    private String section;     // Name of section
    private String key;         // Name of key
    private String value;       // data to write


    WinProfileItem(WinProfile profileObj,
                String sectionName,
                String keyName,
                String val) throws SoftUpdateException
    {
        super(profileObj.softUpdate());
        profile = profileObj;
        section = sectionName;
        key     = keyName;
        value   = val;
    }


    /**
     * Completes the install:
     * - writes the data into the .INI file
     */
    protected void Complete() throws SoftUpdateException
    {
        profile.finalWriteString( section, key, value );
    }

    float GetInstallOrder()
    {
        return 4;
    }

    public String toString()
    {
        return "Write "+profile.filename()+": ["+section+"] "+key+"="+value;
    }

    // no need for special clean-up
    protected void Abort()  {}
}
