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
package	netscape.softupdate	;

import netscape.softupdate.*;
import netscape.util.*;
import java.lang.*;
import netscape.security.PrivilegeManager;
import netscape.security.Target;
import netscape.security.AppletSecurity;

/* InstallFile
 * extracts	the	file to	the	temporary directory
 * on Complete(), copies it	to the final location
 */

final class	InstallFile	extends	InstallObject {

	private	String vrName;		// Name	of the component
	private	VersionInfo	versionInfo;	// Version
	private	String jarLocation;	// Location	in the JAR
	private	String tempFile;		// temporary file location
	private	String finalFile;		// final file destination
    private String regPackageName;	// Name	of the package we are installing
	private String userPackageName;	// User-readable package name
	private Target target;          // security	target
    private boolean force;      // whether install is forced
    private boolean replace = false;      // whether file exists
    private boolean bChild = false;      // whether file is a child
    private boolean bUpgrade = false;    // whether file is an upgrade

	/*	Constructor
		inSoftUpdate	- softUpdate object	we belong to
		inComponentName	- full path	of the registry	component
		inVInfo			- full version info
		inJarLocation	- location inside the JAR file
		inFinalFileSpec	- final	location on	disk
	 */

	InstallFile(SoftwareUpdate inSoftUpdate,
				String inVRName,
				VersionInfo	inVInfo,
				String inJarLocation,
				FolderSpec folderSpec,
				String inPartialPath,
                boolean forceInstall) throws SoftUpdateException
	{
       	super(inSoftUpdate);
	    tempFile = null;
		vrName = inVRName;
		versionInfo	= inVInfo;
		jarLocation	= inJarLocation;
        force = forceInstall;
		finalFile =	folderSpec.MakeFullPath( inPartialPath );

		/* Request impersonation privileges */
		netscape.security.PrivilegeManager privMgr;
		Target impersonation;

		privMgr	= AppletSecurity.getPrivilegeManager();
		impersonation = Target.findTarget( SoftwareUpdate.IMPERSONATOR );
		privMgr.enablePrivilege( impersonation );

	    /* check the security permissions */
		target = Target.findTarget( SoftwareUpdate.INSTALL_PRIV );

	   /* XXX: we need a way to indicate that a dialog box should appear. */
		privMgr.enablePrivilege( target, softUpdate.GetPrincipal() );
        
        userPackageName = inSoftUpdate.GetUserPackageName();
        regPackageName = inSoftUpdate.GetRegPackageName();

        // determine Child status
        if ( regPackageName.length() == 0 ) {
            // in the "current communicator package" absolute pathnames (start
            // with slash) indicate shared files -- all others are children
            bChild = ( vrName.charAt(0) != '/' );
        }
        else {
            bChild = vrName.startsWith(regPackageName);
        }

        replace = NativeDoesFileExist();

	}

	/* Prepare
	 * Extracts	file out of	the	JAR	archive	into the temp directory
	 */
	protected void Prepare() throws SoftUpdateException
	{
		netscape.security.PrivilegeManager privMgr;
		Target impersonation;

		privMgr = AppletSecurity.getPrivilegeManager();
		impersonation = Target.findTarget( SoftwareUpdate.IMPERSONATOR );
	    privMgr.enablePrivilege( impersonation );
	    privMgr.enablePrivilege( target, softUpdate.GetPrincipal() );
		tempFile = softUpdate.ExtractJARFile( jarLocation, finalFile );
	}

	/* Complete
	 * Completes the install:
	 * - move the downloaded file to the final location
	 * - updates the registry
	 */
	protected void	Complete() throws SoftUpdateException
	{
		int	err;
        Integer refCount;
        int rc;

		/* Check the security for our target */
		netscape.security.PrivilegeManager privMgr;
		Target impersonation;

		privMgr = AppletSecurity.getPrivilegeManager();
		impersonation = Target.findTarget( SoftwareUpdate.IMPERSONATOR );
		privMgr.enablePrivilege( impersonation );
		privMgr.enablePrivilege( target, softUpdate.GetPrincipal() );
		err	= NativeComplete();
		privMgr.revertPrivilege(target);

		if ( 0 == err || SoftwareUpdate.REBOOT_NEEDED == err )
		{
            // we ignore all registry errors because they're not
            // important enough to abort an otherwise OK install.
            if (!bChild)
            {
                int found;
                found = VersionRegistry.uninstallFileExists(regPackageName, vrName);
                if (found != VersionRegistry.REGERR_OK)
		            bUpgrade = false;
                else
                    bUpgrade = true;
            }
            else if ( VersionRegistry.REGERR_OK == VersionRegistry.inRegistry(vrName))
            {
                bUpgrade = true;
            }
            else
            {
                bUpgrade = false;
            }
            
            refCount = VersionRegistry.getRefCount(vrName);
            if (!bUpgrade)
            {
		        if (refCount != null) 
                {
                    rc = 1 + refCount.intValue();
                    VersionRegistry.installComponent(vrName, finalFile, versionInfo, rc );
                
                }
                else
                {
                    if (replace)
                        VersionRegistry.installComponent(vrName, finalFile, versionInfo, 2);
                    else
                        VersionRegistry.installComponent(vrName, finalFile, versionInfo, 1);
                }
            }
            else if (bUpgrade)
            {
                if (refCount == null)
                {
                    VersionRegistry.installComponent(vrName, finalFile, versionInfo, 1);
                }
                else 
                {
                    VersionRegistry.installComponent(vrName, finalFile, versionInfo );
                }
            }

            if ( !bChild && !bUpgrade ) {
                VersionRegistry.uninstallAddFile(regPackageName, vrName);
            }
        }

        if ( err == SoftwareUpdate.REBOOT_NEEDED ) {
            throw( new SoftUpdateException(finalFile, err));    
        }
		else if ( err != 0 ) {
			throw( new SoftUpdateException(Strings.error_InstallFileUnexpected()  +  finalFile, err));
        }

	}
	
	protected void Abort()
	{
	    NativeAbort();
    }
    
    private native void NativeAbort();
	private	native int NativeComplete();
    private	native boolean NativeDoesFileExist();

	public String toString()
	{
        if ( replace )
            return Strings.details_ReplaceFile() + finalFile;
        else
            return Strings.details_InstallFile() + finalFile;
	}
}
