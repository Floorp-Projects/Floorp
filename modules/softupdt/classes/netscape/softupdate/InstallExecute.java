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

import java.lang.*;
import netscape.security.PrivilegeManager;
import netscape.security.Target;
import netscape.security.AppletSecurity;

/* InstallFile
 * extracts the file to the temporary directory
 * on Complete(), copies it to the final location
 */

class InstallExecute extends InstallObject {
	private String jarLocation; // Location in the JAR
	private	String tempFile;    // temporary file location
    private String args;        // command line arguments
    private String cmdline;     // constructed command-line

	/*	Constructor
		inJarLocation	- location inside the JAR file
		inZigPtr        - pointer to the ZIG *
	 */

	InstallExecute(SoftwareUpdate inSoftUpdate, String inJarLocation, String inArgs)
	{
	    super(inSoftUpdate);
        jarLocation = inJarLocation;
        args = inArgs;

		/* Request impersonation privileges */
		netscape.security.PrivilegeManager privMgr;
		Target impersonation, target;

		privMgr	= AppletSecurity.getPrivilegeManager();
		impersonation = Target.findTarget( SoftwareUpdate.IMPERSONATOR );
		privMgr.enablePrivilege( impersonation );

	    /* check the security permissions */
		target = Target.findTarget( SoftwareUpdate.INSTALL_PRIV );

	   /* XXX: we need a way to indicate that a dialog box should appear.*/
		privMgr.enablePrivilege( target, softUpdate.GetPrincipal() );
    }

	/* Prepare
	 * Extracts	file out of	the	JAR	archive	into the temp directory
	 */
	protected void Prepare() throws SoftUpdateException
	{
	    netscape.security.PrivilegeManager privMgr;
	    Target impersonation;
	    Target execTarget;
		privMgr = AppletSecurity.getPrivilegeManager();
		impersonation = Target.findTarget( SoftwareUpdate.IMPERSONATOR );
	    execTarget = Target.findTarget( SoftwareUpdate.INSTALL_PRIV );
	    privMgr.enablePrivilege( impersonation );
    	privMgr.enablePrivilege( execTarget, softUpdate.GetPrincipal() );
        tempFile = softUpdate.ExtractJARFile( jarLocation, null );

        if ( args == null || System.getProperty("os.name").startsWith("Mac") )
            cmdline = tempFile;
        else
            cmdline = tempFile+" "+args;
	}

	/* Complete
	 * Completes the install by executing the file
	 * Security hazard: make sure we request the right permissions
	 */
	protected void	Complete() throws SoftUpdateException
	{
		int	err;
		Target execTarget;
		netscape.security.PrivilegeManager privMgr;
		Target impersonation;

		privMgr = AppletSecurity.getPrivilegeManager();
		impersonation = Target.findTarget( SoftwareUpdate.IMPERSONATOR );
	    privMgr.enablePrivilege( impersonation );
		execTarget = Target.findTarget( SoftwareUpdate.INSTALL_PRIV );
		privMgr.enablePrivilege( execTarget, softUpdate.GetPrincipal() );
		NativeComplete();
		privMgr.revertPrivilege( execTarget );
		// 
		// System.out.println("File executed: " + tempFile);
	}

	protected void Abort()
	{
	    NativeAbort();
    }

	private native void NativeComplete() throws SoftUpdateException;
	private native void NativeAbort();
        
    public String toString()
    {
        return Strings.details_ExecuteProgress() + tempFile; // Needs I10n
    }

}
