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
 * FolderSpec.java
 *
 * 1/3/97 - atotic created the class
 */

package netscape.softupdate ;
import netscape.softupdate.*;
import netscape.security.Target;

/**
 * FolderSpec hides directory path from the user of the
 * SoftwareUpdate class
 * It is not visible to classes outside of the softupdate package,
 * because it is expected to change
 *
 */

final class FolderSpec	{
/* This class must stay final
 * If not, an attacker could subclass it, and override GetCapTarget method */

	private String urlPath;  		// Full path to the directory. Used to cache results from GetDirectoryPath
	private String folderID; 		// Unique string specifying a folder
	private String versionRegistryPath;  // Version registry path of the package
	private String userPackageName;  	// Name of the package presented to the user

    /* Error codes */
	final static int INVALID_PATH_ERR = -100;
	final static int USER_CANCELLED_ERR = -101;

	/* Constructor
	 */
	FolderSpec( String inFolderID , String inVRPath, String inPackageName)
	{
		urlPath = null;
		folderID = inFolderID;
		versionRegistryPath = inVRPath;
		userPackageName = inPackageName;
	}

	/*
	 * GetDirectoryPath
	 * returns full path to the directory in the standard URL form
	 */
	String
	GetDirectoryPath() throws SoftUpdateException
	{
		if (urlPath == null)
        {
            if (folderID.compareTo("User Pick") == 0)  // Default package folder
    		{
    			// Try the version registry default path first
    			// urlPath = VersionRegistry.getDefaultDirectory( versionRegistryPath );
    			// if (urlPath == null)
    			urlPath = PickDefaultDirectory();
    		}
            else if (folderID.compareTo("Installed") == 0) 
    		{
    			// Then use the Version Registry path
    			urlPath = versionRegistryPath;
    		}
    		else
    		// Built-in folder
    		{
    			int err = NativeGetDirectoryPath();
    			if (err != 0)
    				throw( new SoftUpdateException( folderID, err));
    		}
    	}
		return urlPath;
   }

    /**
	 * Returns full path to a file. Makes sure that the full path is bellow
	 * this directory (security measure
     * @param relativePath      relative path
     * @return                  full path to a file
	 */
	String MakeFullPath(String relativePath) throws SoftUpdateException
    {
        String fullPath = GetDirectoryPath() + GetNativePath ( relativePath );
        return fullPath;
    }

	/* NativeGetDirectoryPath
	 * gets a platform-specific directory path
	 * stores it in urlPath
	 */
	private native int
	NativeGetDirectoryPath();


	/* GetNativePath
	 * returns a native equivalent of a XP directory path
	 */
	private native String
	GetNativePath ( String Path );


	/* PickDefaultDirectory
	 * asks the user for the default directory for the package
	 * stores the choice
	 */
	private String
	PickDefaultDirectory() throws SoftUpdateException
	{
		urlPath = NativePickDefaultDirectory();

		if (urlPath == null)
			throw( new SoftUpdateException(folderID, INVALID_PATH_ERR));

        return urlPath;
   }

	/*
	 * NativePickDefaultDirectory
	 * Platform-specific implementation of GetDirectoryPath
	 */
	private native String
	NativePickDefaultDirectory() throws SoftUpdateException;

   public String toString()
   {
      String path;
      try 
      {
         path = GetDirectoryPath();
      }
      catch ( Exception e )
      {
         path = null;
      }
      return path;
   }
}
