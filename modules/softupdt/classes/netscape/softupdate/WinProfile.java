/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
//    static final String INI_TARGET = "WindowsIniFile";

//    /* create the target */
//    static	{
//        target = new ParameterizedTarget(
//                    INI_TARGET,
//                    PrivilegeManager.getSystemPrincipal(),
//                    UserDialogHelper.targetRiskMedium(),
//                    UserDialogHelper.targetRiskColorMedium(),
//                    Strings.targetDesc_WinIni(),
//                    Strings.targetUrl_WinIni() );
//        target = (ParameterizedTarget)target.registerTarget();
//    };

    WinProfile( SoftwareUpdate suObj, FolderSpec folder, String file )
        throws SoftUpdateException
    {
        filename = folder.MakeFullPath( file );
        su = suObj;
        principal = suObj.GetPrincipal();
        privMgr = PrivilegeManager.getPrivilegeManager();
        impersonation = Target.findTarget( SoftwareUpdate.IMPERSONATOR );
        target = (UserTarget)Target.findTarget( SoftwareUpdate.INSTALL_PRIV );
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
