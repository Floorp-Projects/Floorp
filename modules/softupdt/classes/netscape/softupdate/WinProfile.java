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
package netscape.softupdate;

import netscape.security.UserTarget;
import netscape.security.PrivilegeManager;
import netscape.security.Principal;
import netscape.security.Target;
import netscape.security.UserDialogHelper;


final class WinProfile {
    private String filename;
    private SoftwareUpdate su;
    private Principal principal;
    private PrivilegeManager privMgr;
    private Target impersonation;

    private UserTarget target;

    WinProfile( SoftwareUpdate suObj, FolderSpec folder, String file )
        throws SoftUpdateException
    {
        filename = folder.MakeFullPath( file );
        su = suObj;
        principal = suObj.GetPrincipal();
        privMgr = PrivilegeManager.getPrivilegeManager();
        impersonation = Target.findTarget( SoftwareUpdate.IMPERSONATOR );
        target = (UserTarget)Target.findTarget( 
            SoftwareUpdate.targetNames[SoftwareUpdate.FULL_INSTALL] );
    }

    /**
     * Schedules a write into a windows "ini" file.  "Value" can be
     * null to delete the value, but we don't support deleting an entire
     * section via a null "key".  The actual write takes place during
     * SoftwareUpdate.FinalizeInstall();
     *
     * @return  false for failure, true for success
     */
    public boolean writeString( String section, String key, String value ) {
        boolean result;
        WinProfileItem item;
        try {
	        privMgr.enablePrivilege( impersonation );
	        privMgr.enablePrivilege( target, principal );

            item = new WinProfileItem( this, section, key, value );
            su.ScheduleForInstall( item );
            result = true;
        }
        catch ( Exception e ) {
            e.printStackTrace( System.out );
            result = false;
        }
        return result;
    }


    /**
     * Reads a value from a windows "ini" file.  We don't support using
     * a null "key" to return a list of keys--you have to know what you want
     *
     * @return  String value from INI, "" if not found, null if error
     */
    public String getString( String section, String key ) {
        String result;
        try {
	        privMgr.enablePrivilege( impersonation );
	        privMgr.enablePrivilege( target, principal );

            return nativeGetString( section, key );
        }
        catch ( Exception e ) {
            e.printStackTrace( System.out );
            result = null;
        }
        return result;
    }

    protected String filename() { return filename; }
    protected SoftwareUpdate softUpdate() { return su; }

    protected int finalWriteString( String section, String key, String value ) {
        // do we need another security check here?
        return nativeWriteString( section, key, value );
    }

    private native int nativeWriteString( String section, String key, String value );
    private native String nativeGetString( String section, String key );
}
