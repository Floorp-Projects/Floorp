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

import netscape.security.PrivilegeManager;
import netscape.security.Principal;
import netscape.security.Target;
import netscape.security.UserTarget;
import netscape.security.UserDialogHelper;

final class WinReg {
    /* not static because class is not public--users couldn't get to them */
    public final int  HKEY_CLASSES_ROOT           = 0x80000000;
    public final int  HKEY_CURRENT_USER           = 0x80000001;
    public final int  HKEY_LOCAL_MACHINE          = 0x80000002;
    public final int  HKEY_USERS                  = 0x80000003;

    protected final int CREATE          = 1;
    protected final int DELETE          = 2;
    protected final int DELETE_VAL      = 3;
    protected final int SET_VAL_STRING  = 4;
    protected final int SET_VAL         = 5;


    private int       rootkey = HKEY_CLASSES_ROOT;
    private Principal principal;
    private PrivilegeManager privMgr;
    private Target impersonation;
    private SoftwareUpdate su;

    private UserTarget target;
//    static final String INI_TARGET = "FullWindowsRegistryAccess";

//    static	{
//        /* create the target */
//        target = new UserTarget(
//                    INI_TARGET,
//                    PrivilegeManager.getSystemPrincipal(),
//                    UserDialogHelper.targetRiskHigh(),
//                    UserDialogHelper.targetRiskColorHigh(),
//                    Strings.targetDesc_WinReg(),
//                    Strings.targetUrl_WinReg() );
//        target = (UserTarget)target.registerTarget();
//    };


    WinReg(SoftwareUpdate suObj) {
        su = suObj;
        principal = suObj.GetPrincipal();
        privMgr = PrivilegeManager.getPrivilegeManager();
        impersonation = Target.findTarget( SoftwareUpdate.IMPERSONATOR );
        target = (UserTarget)Target.findTarget( SoftwareUpdate.INSTALL_PRIV );
    }


    public void setRootKey(int key) {
        rootkey = key;
    }


    public int createKey(String subkey, String classname) {
        int result;
        try {
	        privMgr.enablePrivilege( impersonation );
	        privMgr.enablePrivilege( target, principal );

            WinRegItem wi = new WinRegItem( this, rootkey, CREATE,
                subkey, classname, null );
            su.ScheduleForInstall( wi );
            result = 0;
        }
        catch ( Exception e ) {
            e.printStackTrace( System.out );
            result = -1;
        }
        return result;
    }


    public int deleteKey(String subkey) {
        int result;
        try {
	        privMgr.enablePrivilege( impersonation );
	        privMgr.enablePrivilege( target, principal );

            WinRegItem wi = new WinRegItem( this, rootkey, DELETE,
                subkey, null, null );
            su.ScheduleForInstall( wi );
            result = 0;
        }
        catch ( Exception e ) {
            e.printStackTrace( System.out );
            result = -1;
        }
        return result;
    }


    public int deleteValue(String subkey, String valname) {
        int result;
        try {
	        privMgr.enablePrivilege( impersonation );
	        privMgr.enablePrivilege( target, principal );

            WinRegItem wi = new WinRegItem( this, rootkey, DELETE_VAL,
                subkey, valname, null );
            su.ScheduleForInstall( wi );
            result = 0;
        }
        catch ( Exception e ) {
            e.printStackTrace( System.out );
            result = -1;
        }
        return result;
    }

    public int    setValueString(String subkey, String valname, String value) {
        int result;
        try {
	        privMgr.enablePrivilege( impersonation );
	        privMgr.enablePrivilege( target, principal );

            WinRegItem wi = new WinRegItem( this, rootkey, SET_VAL_STRING,
                subkey, valname, (Object)value );
            su.ScheduleForInstall( wi );
            result = 0;
        }
        catch ( Exception e ) {
            e.printStackTrace( System.out );
            result = -1;
        }
        return result;
    }

    public String getValueString(String subkey, String valname) {
        String result;
        try {
	        privMgr.enablePrivilege( impersonation );
	        privMgr.enablePrivilege( target, principal );

            result = nativeGetValueString( subkey, valname );
        }
        catch ( Exception e ) {
            e.printStackTrace( System.out );
            result = null;
        }
        return result;
    }

    public int setValue(String subkey, String valname, WinRegValue value) {
        int result;
        try {
	        privMgr.enablePrivilege( impersonation );
	        privMgr.enablePrivilege( target, principal );

            WinRegItem wi = new WinRegItem( this, rootkey, SET_VAL,
                subkey, valname, (Object)value );
            su.ScheduleForInstall( wi );
            result = 0;
        }
        catch ( Exception e ) {
            e.printStackTrace( System.out );
            result = -1;
        }
        return result;    }

    public WinRegValue getValue(String subkey, String valname) {
        WinRegValue result;
        try {
	        privMgr.enablePrivilege( impersonation );
	        privMgr.enablePrivilege( target, principal );

            result = nativeGetValue( subkey, valname );
        }
        catch ( Exception e ) {
            e.printStackTrace( System.out );
            result = null;
        }
        return result;
    }

    protected SoftwareUpdate softUpdate() { return su; }

    protected int finalCreateKey(int root, String subkey, String classname) {
        setRootKey(root);
        return nativeCreateKey(subkey, classname);
    }

    protected int finalDeleteKey(int root, String subkey) {
        setRootKey(root);
        return nativeDeleteKey(subkey);
    }

    protected int finalDeleteValue(int root, String subkey, String valname) {
        setRootKey(root);
        return nativeDeleteValue(subkey, valname);
    }

    protected int finalSetValueString(int root, String subkey, String valname, String value) {
        setRootKey(root);
        return nativeSetValueString(subkey, valname, value);
    }

    protected int finalSetValue(int root, String subkey, String valname, WinRegValue value) {
        setRootKey(root);
        return nativeSetValue(subkey, valname, value);
    }

    private native int nativeCreateKey(String subkey, String classname);
    private native int nativeDeleteKey(String subkey);
    private native int nativeDeleteValue(String subkey, String valname);

    private native int    nativeSetValueString(String subkey, String valname, String value);
    private native String nativeGetValueString(String subkey, String valname);

    private native int nativeSetValue(String subkey, String valname, WinRegValue value);
    private native WinRegValue nativeGetValue(String subkey, String valname);
}
