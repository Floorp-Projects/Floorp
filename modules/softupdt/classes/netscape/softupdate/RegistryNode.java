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
/*
 * RegistryNode.java
 */
package netscape.softupdate ;

import netscape.softupdate.Registry;
import java.util.Enumeration;
import netscape.security.PrivilegeManager;

/**
 * The RegistryNode class represents a node in an open registry
 */
public final class RegistryNode implements RegistryErrors {

    /*
     * Private data members
     */
    private Registry    reg;
    private int         key;
    private String      target;

    protected int        getKey()       { return key; }
    protected String     getTarget()    { return target; }

    /**
     * The constructor is private so this class can only be created
     * by the native method Registry::nGetKey()
     */
    private RegistryNode(Registry parent, int rootKey, String targ)
    {
        reg = parent;
        key = rootKey;
        target = targ;
    }


    public Enumeration properties()
    {
        return new RegEntryEnumerator(reg, key, target);
    }


    public void
    deleteProperty(String name) throws RegistryException
    {
        PrivilegeManager.checkPrivilegeEnabled( target );
        int status = nDeleteEntry(name);
        if (REGERR_OK != status)
            throw new RegistryException(status);
    }


    public int
    getPropertyType(String name) throws RegistryException
    {
        PrivilegeManager.checkPrivilegeEnabled( target );
        int status = nGetEntryType( name );

        if ( status < 0 )
            throw new RegistryException(-status);

        return status;
    }


    public Object
    getProperty(String name) throws RegistryException
    {
        PrivilegeManager.checkPrivilegeEnabled( target );
        return nGetEntry( name );
    }


    /* the following ugliness is due to a javah bug that can't
     * entirely handle overloaded methods.  (The implementations
     * are unique, but the native stub redirection isn't.)
     */
    public void
    setProperty(String name, String value)  throws RegistryException
    {
        PrivilegeManager.checkPrivilegeEnabled( target );
        int status = setEntryS(name, value);
        if (status != REGERR_OK)
            throw new RegistryException(status);
    }

    public void
    setProperty(String name, int[] value) throws RegistryException
    {
        PrivilegeManager.checkPrivilegeEnabled( target );
        int status = setEntryI(name, value);
        if (status != REGERR_OK)
            throw new RegistryException(status);
    }

    public void
    setProperty(String name, byte[] value) throws RegistryException
    {
        PrivilegeManager.checkPrivilegeEnabled( target );
        int status = setEntryB(name, value);
        if (status != REGERR_OK)
            throw new RegistryException(status);
    }


    private native int       nDeleteEntry(String name);
    private native int       nGetEntryType(String name);
    private native Object    nGetEntry(String name);
    private native int       setEntryS(String name, String value);
    private native int       setEntryI(String name, int[] value);
    private native int       setEntryB(String name, byte[] value);
}


