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

import netscape.softupdate.*;
import netscape.util.*;
import java.lang.*;
import netscape.security.PrivilegeManager;
import netscape.security.Target;
import netscape.security.AppletSecurity;

/* InstallFile
 * extracts the file to the temporary directory
 * on Complete(), copies it to the final location
 */

final class InstallPatch extends InstallObject {

    private String vrName;              // Registry name of the component
    private VersionInfo versionInfo;    // Version
    private String jarLocation;         // Location in the JAR
    private String patchURL;            // extracted location of diff (xpURL)
    private String targetfile;          // source and final file (native)
    private String patchedfile;         // temp name of patched file

    /*  Constructor
        inSoftUpdate    - softUpdate object we belong to
        inVRName        - full path of the registry component
        inVInfo         - full version info
        inJarLocation   - location inside the JAR file
        folderspec      - FolderSpec of target file
        inPartialPath   - target file on disk relative to folderspec
     */

    InstallPatch(SoftwareUpdate inSoftUpdate,
                    String inVRName,
                    VersionInfo inVInfo,
                    String inJarLocation) throws SoftUpdateException
    {
        super( inSoftUpdate );

        vrName      = inVRName;
        versionInfo = inVInfo;
        jarLocation = inJarLocation;

        targetfile = VersionRegistry.componentPath( vrName );
        if ( targetfile == null )
            throw new SoftUpdateException("", SoftwareUpdate.NO_SUCH_COMPONENT);

        checkPrivileges();
    }


    InstallPatch(SoftwareUpdate inSoftUpdate,
                    String inVRName,
                    VersionInfo inVInfo,
                    String inJarLocation,
                    FolderSpec folderSpec,
                    String inPartialPath) throws SoftUpdateException
    {
        super( inSoftUpdate );

        vrName      = inVRName;
        versionInfo = inVInfo;
        jarLocation = inJarLocation;

        targetfile = folderSpec.MakeFullPath( inPartialPath );

        checkPrivileges();
    }


    private void checkPrivileges()
    {
        // This won't actually give us these privileges because
        // we lose them again as soon as the function returns. But we
        // don't really need the privs, we're just checking security

        PrivilegeManager privMgr = AppletSecurity.getPrivilegeManager();

        Target impersonation = 
            Target.findTarget( SoftwareUpdate.IMPERSONATOR );

        Target priv = Target.findTarget( 
            SoftwareUpdate.targetNames[ SoftwareUpdate.FULL_INSTALL ] );

        privMgr.enablePrivilege( impersonation );
        privMgr.enablePrivilege( priv, softUpdate.GetPrincipal() );
    }



    protected void ApplyPatch() throws SoftUpdateException
    {
        String srcname;
        boolean deleteOldSrc;

        checkPrivileges();

        patchURL = softUpdate.ExtractJARFile( jarLocation, targetfile );

        if ( softUpdate.patchList.containsKey( targetfile ) )  {
            srcname      = (String)softUpdate.patchList.get( targetfile );
            deleteOldSrc = true;
        } 
        else {
            srcname      = targetfile;
            deleteOldSrc = false;
        }

        patchedfile = NativePatch( srcname, patchURL );

        if ( patchedfile != null ) {
            softUpdate.patchList.put( targetfile, patchedfile );
        }
        else {
            throw new SoftUpdateException( targetfile + " not patched",
                SoftwareUpdate.UNEXPECTED_ERROR );
        }

        if ( deleteOldSrc ) {
            NativeDeleteFile( srcname );
        }

    }





    /* Complete
     * Completes the install:
     * - move the patched file to the final location
     * - updates the registry
     */
    protected void Complete() throws SoftUpdateException
    {
        int err;

        checkPrivileges();

        String tmp = (String)softUpdate.patchList.get( targetfile );
        if ( tmp.compareTo( patchedfile ) == 0 ) 
        {
            // the patch has not been superceded--do final replacement
            
            err = NativeReplace( targetfile, patchedfile );

            if ( 0 == err || SoftwareUpdate.REBOOT_NEEDED == err )
            {
                VersionRegistry.installComponent( 
                    vrName, targetfile, versionInfo);

                if ( err != 0 )
                    throw new SoftUpdateException(targetfile, err);
            }
            else
            {
                throw new SoftUpdateException( 
                    Strings.error_InstallFileUnexpected() + targetfile, err);
            }
        }
        else 
        {
            // nothing -- old intermediate patched file was
            // deleted by superceding patch
        }
    }

    
    protected void Abort()
    {
        String tmp = (String)softUpdate.patchList.get( targetfile );
        if ( tmp.compareTo( patchedfile ) == 0 )
            NativeDeleteFile( patchedfile );
    }


    private native String NativePatch( String srcfile, String diffURL );
    private native int    NativeReplace( String target, String tmpfile );
    private native void   NativeDeleteFile( String file );


    public String toString()
    {
        return "Patch " + targetfile + " (" + versionInfo + ")";
    }
}
