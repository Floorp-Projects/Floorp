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


