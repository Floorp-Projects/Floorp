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

import java.lang.*;
import netscape.security.PrivilegeManager;
import netscape.security.Target;
import netscape.security.AppletSecurity;

/* InstallDelete
 * deletes the specified file from the disk
 */

class InstallDelete extends InstallObject {
	private	String finalFile = null;		// final file to be deleted
	private	String registryName;		// final file to be deleted

	int deleteStatus;
	static final int DELETE_FILE = 1;
    static final int DELETE_COMPONENT = 2;



	/*	Constructor
		inFolder	- a folder object representing the directory that contains the file
		inRelativeFileName  - a relative path and file name
	 */

	InstallDelete(SoftwareUpdate inSoftUpdate, FolderSpec inFolder,
		String inRelativeFileName) throws SoftUpdateException
	{
	    super(inSoftUpdate);
		deleteStatus = DELETE_FILE;
       	finalFile =	inFolder.MakeFullPath(inRelativeFileName);
		processInstallDelete();
	}

	/*	Constructor
		inRegistryName	- name of the component in the registry
	 */
	InstallDelete(SoftwareUpdate inSoftUpdate, String inRegistryName)
		throws SoftUpdateException
	{
	    super(inSoftUpdate);
		deleteStatus = DELETE_COMPONENT;
   		registryName = inRegistryName;
		processInstallDelete();
    }

	private void processInstallDelete() throws SoftUpdateException
	{
		int err;

		/* Request impersonation privileges */
		netscape.security.PrivilegeManager privMgr;
		Target impersonation, target;

		privMgr	= AppletSecurity.getPrivilegeManager();
		impersonation = Target.findTarget( SoftwareUpdate.IMPERSONATOR );
		privMgr.enablePrivilege( impersonation );

	    /* check the security permissions */
		target = Target.findTarget( SoftwareUpdate.INSTALL_PRIV );

		privMgr.enablePrivilege( target, softUpdate.GetPrincipal() );

		if (deleteStatus == DELETE_COMPONENT)
		{
			/* Check if the component is in the registry */
			err = VersionRegistry.inRegistry(registryName);
			if (err != VersionRegistry.REGERR_OK)
			{
				throw( new SoftUpdateException(Strings.error_NotInRegistry()  +  registryName, err));
			}
			else
			{
				finalFile = VersionRegistry.componentPath(registryName);
			}
		}

		/* Check if the file exists and is not read only */
		if (finalFile != null)
		{
			err = NativeCheckFileStatus();
			if (err == 0)
			{
				/* System.out.println("File exists and is not read only" + finalFile);*/
			}
			else if (err == SoftwareUpdate.FILE_DOES_NOT_EXIST)
			{
				/*throw( new SoftUpdateException(Strings.error_FileDoesNotExist()  +  finalFile, err));*/
			}
			else if (err == SoftwareUpdate.FILE_READ_ONLY)
			{
				throw( new SoftUpdateException(Strings.error_FileReadOnly()  +  finalFile, err));
			}
			else if (err == SoftwareUpdate.FILE_IS_DIRECTORY)
			{
				throw( new SoftUpdateException(Strings.error_FileIsDirectory()  +  finalFile, err));
			}
			else
			{
				throw( new SoftUpdateException(Strings.error_Unexpected()  +  finalFile, err));
			}
		}
	}

    // no set-up necessary
    protected void Prepare() throws SoftUpdateException {}
	
	/* Complete
	 * Completes the install by deleting the file
	 * Security hazard: make sure we request the right permissions
	 */
	protected void Complete() throws SoftUpdateException
	{
		int	err = -1;
		Target execTarget;
		netscape.security.PrivilegeManager privMgr;
		Target impersonation;
		privMgr = AppletSecurity.getPrivilegeManager();
		impersonation = Target.findTarget( SoftwareUpdate.IMPERSONATOR );
	    privMgr.enablePrivilege( impersonation );
		execTarget = Target.findTarget( SoftwareUpdate.INSTALL_PRIV );
		privMgr.enablePrivilege( execTarget, softUpdate.GetPrincipal() );
		if (deleteStatus == DELETE_COMPONENT) 
		{
			err = VersionRegistry.deleteComponent(registryName);
		}
		if ((deleteStatus == DELETE_FILE) || (err == VersionRegistry.REGERR_OK))
		{
			if (finalFile != null)
			{
				err = NativeComplete();
				if ((err != 0) && (err != SoftwareUpdate.FILE_DOES_NOT_EXIST))
				{
					privMgr.revertPrivilege( execTarget );
					throw( new SoftUpdateException(Strings.error_Unexpected()  +  finalFile, err));
				}
			}
		}
		else
		{
			throw( new SoftUpdateException(Strings.error_Unexpected()  +  finalFile, err));
		}
		return;
	}


	protected void Abort()
	{
		;
    }

	private native int NativeComplete() throws SoftUpdateException;
	private native int NativeCheckFileStatus() throws SoftUpdateException;
        
	public String toString()
	{
		if (deleteStatus == DELETE_FILE)
		{
			return Strings.details_DeleteFile() + finalFile;
		}
		else
		{
			return Strings.details_DeleteComponent() + registryName;
		}
	}

}
