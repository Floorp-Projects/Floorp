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
 * Registry.java
 *
 */
package netscape.softupdate ;

import java.util.Enumeration;
import java.util.NoSuchElementException;
import netscape.security.PrivilegeManager;
import netscape.security.Principal;
import netscape.security.Target;
import netscape.security.UserTarget;
import netscape.security.ForbiddenTargetException;

/**
 * The Registry class is a Java wrapper
 * around the Navigator's built-in Cross-Platform Registry.
 *
 * @version    1.0 97/2/23
 * @author     Daniel Veditz
 */
public final class Registry implements RegistryErrors {

    /*------------------
     * constants
     *------------------
     */
    protected final static int ROOTKEY_USERS           = 1;
    protected final static int ROOTKEY_COMMON          = 2;
    protected final static int ROOTKEY_CURRENT_USER    = 3;
    protected final static int ROOTKEY_PRIVATE         = 4;
    protected final static int ROOTKEY                 = 32;
    protected final static int ROOTKEY_VERSIONS        = 33;
    protected final static int PRIVATE_KEY_COMMON      = -2;
    protected final static int PRIVATE_KEY_USER        = -3;

    /* the following statics are actually used by RegistryNode objects,
     * but that's not a public class--Registry is what people will use.
     */
    protected final static int ENUM_NORMAL    = 0;
    protected final static int ENUM_DESCEND   = 1;

    public final static int TYPE_STRING    = 0x11;
    public final static int TYPE_INT_ARRAY = 0x12;
    public final static int TYPE_BYTES     = 0x13;

    private static final String PRIVATE   = "PrivateRegistryAccess";
    private static final String STANDARD  = "StandardRegistryAccess";
    private static final String ADMIN     = "AdministratorRegistryAccess";

    /*
     * Private data members
     */
    private int       hReg = 0;
    private String    regName;
    private String    username = null;

    /*
     * Constructors
     */

    /**
     * The no-arg constructor refers to standard netscape registry.
     */
    public Registry() throws RegistryException {
        this("");
    }

    /**
     * Creates a registry object for the named registry and opens it
     * (private for now, could be exposed later)
     */
    private Registry(String name) throws RegistryException {
        regName = name;

        // ensure minimal registry privileges
        PrivilegeManager.checkPrivilegeEnabled( PRIVATE );

        int status = nOpen();
        if ( status != REGERR_OK )
            throw new RegistryException(status);
    }

    /*
     * Primary methods, wrappers for native calls plus security
     */


    /**
     * Add a node to the registry
     */
    public RegistryNode
    addNode(RegistryNode root, String key) throws RegistryException
    {
        int     rootkey;
        String  roottarg;
        if ( root == null ) {
            roottarg = ADMIN;
            rootkey  = ROOTKEY;
        }
        else {
            roottarg = root.getTarget();
            rootkey  = root.getKey();
        }

        PrivilegeManager.checkPrivilegeEnabled( roottarg );
        int status = nAddKey( rootkey, key );
        if ( status != REGERR_OK )
            throw new RegistryException(status);

        return nGetKey( rootkey, key, roottarg );
    }



    /**
     * Delete a node from the registry
     */
    public void
    deleteNode(RegistryNode root, String key)  throws RegistryException
    {
        int     rootkey;
        String  roottarg;
        if ( root == null ) {
            roottarg = ADMIN;
            rootkey  = ROOTKEY;
        }
        else {
            roottarg = root.getTarget();
            rootkey  = root.getKey();
        }

        PrivilegeManager.checkPrivilegeEnabled( roottarg );
        int status = nDeleteKey( rootkey, key );
        if ( status != REGERR_OK )
            throw new RegistryException(status);
    }



    /**
     * Get a node object for further registry manipulation
     */
    public RegistryNode
    getNode(RegistryNode root, String key) throws RegistryException
    {
        int     rootkey;
        String  roottarg;
        if ( root == null ) {
            roottarg = ADMIN;
            rootkey  = ROOTKEY;
        }
        else {
            roottarg = root.getTarget();
            rootkey  = root.getKey();
        }

        PrivilegeManager.checkPrivilegeEnabled( roottarg );

        return nGetKey( rootkey, key, roottarg );
    }


    public Enumeration
    children(RegistryNode root, String key) throws RegistryException
    {
        RegistryNode node = getNode(root, key);

        return new RegKeyEnumerator(this, node.getKey(),
            ENUM_NORMAL, node.getTarget());
    }


    public Enumeration
    subtree(RegistryNode root, String key) throws RegistryException
    {
        RegistryNode node = getNode(root, key);

        return new RegKeyEnumerator(this, node.getKey(),
            ENUM_DESCEND, node.getTarget());
    }




    /*
     * Get starting nodes of varying privileges
     */

    public RegistryNode
    getSharedNode() throws RegistryException
    {
        PrivilegeManager.checkPrivilegeEnabled( STANDARD );
        return nGetKey( ROOTKEY_COMMON, "", STANDARD );
    }


    public RegistryNode
    getSharedUserNode() throws RegistryException
    {
        PrivilegeManager.checkPrivilegeEnabled( STANDARD );
        return nGetKey( ROOTKEY_CURRENT_USER, "", STANDARD );
    }


    public RegistryNode
    getPrivateNode() throws RegistryException
    {
        PrivilegeManager.checkPrivilegeEnabled( PRIVATE );

        Principal[] p =
           PrivilegeManager.getPrivilegeManager().getClassPrincipalsFromStack(1);
        if ( p == null ) {
            System.out.println("Registry called without principals");
            throw new NullPointerException("Registry called without principals");
        }

        String key = "/"+p[0].getFingerPrint()+"/Common";

        int status = nAddKey( ROOTKEY_PRIVATE, key );
        if ( status != REGERR_OK )
            throw new RegistryException(status);

        return nGetKey( ROOTKEY_PRIVATE, key, PRIVATE );
    }


    public RegistryNode
    getPrivateUserNode() throws RegistryException
    {
        PrivilegeManager.checkPrivilegeEnabled( PRIVATE );

        Principal[] p =
           PrivilegeManager.getPrivilegeManager().getClassPrincipalsFromStack(1);
        if ( p == null ) {
            System.out.println("Registry called without principals");
            throw new NullPointerException("Registry called without principals");
        }

        String key = "/"+p[0].getFingerPrint()+"/Users/"+userName();

        int status = nAddKey( ROOTKEY_PRIVATE, key );
        if ( status != REGERR_OK )
            throw new RegistryException(status);

        return nGetKey( ROOTKEY_PRIVATE, key, PRIVATE );
    }

    /*
     * private methods
     */
    protected void finalize() throws Throwable  {
        if ( hReg != 0 )
            nClose();
    }

    private String userName()
    {
        if ( username == null )
            username = nUserName();

        return username;
    }


    /*
     * native methods
     */
    private native int          nOpen();
    private native int          nClose();
    private native int          nAddKey(int root, String key);
    private native int          nDeleteKey(int root, String key);
    private native RegistryNode nGetKey(int root, String key, String targ);
    private native String       nUserName();
}

