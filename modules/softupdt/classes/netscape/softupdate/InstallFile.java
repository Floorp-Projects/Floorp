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
