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
package netscape.softupdate ;

import netscape.softupdate.*;
import netscape.util.*;
import java.lang.*;

/**
 * WinProfileItem
 * Writes a single item into a Windows .INI file.
 */

final class WinRegItem extends InstallObject
{
    private WinReg reg;         // initiating WinReg object
    private int    rootkey;
    private int    command;
    private String subkey;     // Name of section
    private String name;         // Name of key
    private Object value;       // data to write


    WinRegItem(WinReg regObj,
                int    root,
                int    action,
                String sub,
                String valname,
                Object val) throws SoftUpdateException
    {
        super(regObj.softUpdate());
        reg = regObj;
        command = action;
        rootkey = root;
        subkey = sub;
        name = valname;
        value = val;
    }


    /**
     * Completes the install:
     * - writes the data into the .INI file
     */
    protected void Complete() throws SoftUpdateException
    {
        switch (command) {
            case reg.CREATE:
                reg.finalCreateKey(rootkey, subkey, name);
                break;
            case reg.DELETE:
                reg.finalDeleteKey(rootkey, subkey);
                break;
            case reg.DELETE_VAL:
                reg.finalDeleteValue(rootkey, subkey, name );
                break;
            case reg.SET_VAL_STRING:
                reg.finalSetValueString(rootkey, subkey, name, (String)value);
                break;
            case reg.SET_VAL:
                reg.finalSetValue(rootkey, subkey, name, (WinRegValue)value);
                break;
            default:
    			throw new SoftUpdateException("WinRegItem", SoftwareUpdate.INVALID_ARGUMENTS);
        }
    }

    float GetInstallOrder()
    {
        return 3;
    }

    public String toString()
    {
        switch (command) {
            case reg.CREATE:
                return "Create Registry Key "+keystr(rootkey,subkey,null);
            case reg.DELETE:
                return "Delete Registry key "+keystr(rootkey,subkey,null);
            case reg.DELETE_VAL:
                return "Delete Registry value "+keystr(rootkey,subkey,name);
            case reg.SET_VAL_STRING:
                return "Store Registry value "+keystr(rootkey,subkey,name);
            case reg.SET_VAL:
                return "Store Registry value "+keystr(rootkey,subkey,name);
            default:
                return "Unknown "+keystr(rootkey,subkey,name);
        }
    }

    private String keystr(int root, String subkey, String name)
    {
        String rootstr;
        switch (root) {
            case reg.HKEY_CLASSES_ROOT:
                rootstr = "\\HKEY_CLASSES_ROOT\\";
                break;
            case reg.HKEY_CURRENT_USER:
                rootstr = "\\HKEY_CURRENT_USER\\";
                break;
            case reg.HKEY_LOCAL_MACHINE:
                rootstr = "\\HKEY_LOCAL_MACHINE\\";
                break;
            case reg.HKEY_USERS:
                rootstr = "\\HKEY_USERS\\";
                break;
            default:
                rootstr = "\\#"+root+"\\";
                break;
        }

        if ( name == null )
            return rootstr+subkey;
        else
            return rootstr+subkey+" ["+name+"]";
    }

    // no need for special clean-up
    protected void Abort()  {}

    // no need for set-up
    protected void Prepare() throws SoftUpdateException {}
}
