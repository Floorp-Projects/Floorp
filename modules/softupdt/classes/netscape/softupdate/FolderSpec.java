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
