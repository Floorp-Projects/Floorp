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

