

// Registry constant paths
// These will be used when the Win32 Registry Keys are written

var HKEY_LOCAL_MACHINE = "HKEY_LOCAL_MACHINE";
var HKEY_CURRENT_USER  = "HKEY_CURRENT_USER";
var REG_MOZ_PATH = "SOFTWARE\\MozillaPlugins";


// My Own Error Code in case secondary installation fails
var noSecondaryInstall = 1;


// error return codes need some memory
var err;

// error return codes when we try and install to the current browser

var errBlock1;

// error return codes when we try and do a secondary installation

var errBlock2 = 0;

// global variable containing our secondary install location

var secondaryFolder;

//Special error values used by the Cycore developers (www.cycore.com) who helped make this install script

var exceptionOccuredError       = -4711;
var winRegIsNullError           = -4712;
var invalidRootkeyError         = -4713;
var registrykeyNotWritableError = -4714;


//initInstall block
//the installation is initialized here -- if we fail here, cancel the installation

// initInstall is quite an overloaded method, but I have invoked it here with three strings
// which are globally defined

err = initInstall(SOFTWARE_NAME, PLID, VERSION);

if (err != 0)
{
	logComment("Install failed at initInstall level with " + err);
	cancelInstall(err);
}

//addFiles to current browser block

var pluginsFolder = getFolder("Plugins");

//Verify Disk Space

if(verifyDiskSpace(pluginsFolder, PLUGIN_SIZE+COMPONENT_SIZE))
{
	// start installing plugin shared library
	resetError();
	
	// install the plugin shared library to the current browser's Plugin directory

	errBlock1 = addFile (PLID, VERSION, PLUGIN_FILE, pluginsFolder, null);
	if (errBlock1!=0)
	{
		logComment("Could NOT add " + PLUGIN_FILE + " to " + pluginsFolder + ":" + errBlock1);
		cancelInstall(errBlock1);
	} 
	
	// start installing xpt file if this is a scriptable plugin
	// install to the plugins directory -- this works well in Mozilla 1.0 clients
	// in Mozilla 1.0 clients, the Components directory can be avoided for XPT files

	errBlock1 = addFile (PLID, VERSION, COMPONENT_FILE, pluginsFolder, null);
	if (errBlock1!=0)
	{
		logComment("Could NOT add " + COMPONENT_FILE + " to " + pluginsFolder + ":" + errBlock1);
		cancelInstall(errBlock1);
	} 

}
else
	{
		logComment("Cancelling current browser install due to lack of space...");
		cancellInstall();
	}


// Secondary install block, which sets up plugins and xpt in another location in addition to the current browser

errBlock2 = createSecondaryInstall();


// performInstall block, in which error conditions from previous blocks are checked.
// This block also invokes a function to write registry keys (PLID) and checks return from key writing
// This block invokes refreshPlugins() to ensure that plugin and xpt are available for use immediately 

if (errBlock1 == SUCCESS)
{

	// installation to the current browser was a success - this is the most important job of this script!

	if(errBlock2 == SUCCESS)
	{
		// Now take care of writing PLIDs to the Win32 Registry

		err = writePLIDSolution();
		if(err!=SUCCESS)
		{
			logComment("Could NOT write Win32 Keys as specified: " + err);
		}
		else
		{
			logComment("PLID entries are present in the Win32 Registry");
		}
	}
	
	resetError();
	err = performInstall();
	if (err == SUCCESS)	
		refreshPlugins(true);  

// call refreshPlugins(true) if you'd like the web page which invoked the plugin to 
// reload.  You can also simply call refreshPlugins()
}

else
	cancelInstall(errBlock1);

// PLID solution -- write keys to the registry 

/**
 * Function for secondary installation of plugin (FirstInstall).
 * You should not stop the install process because the function failed,
 * you still have a chance to install the plugin for the already
 * installed gecko browsers.
 *
 * @param empty param list
 **/

function createSecondaryInstall()
{
	// Use getFolder in such a way that it creates C:\WINNT\System32\MyPlugin

	secondaryFolder = getFolder("Win System", COMPANY_NAME);
	
	// if secondaryFolder is NULL, then there has been an error

	if(!secondaryFolder)
		return noSecondaryInstall;
	else
	{	
		// we have admin privileges to write to the Win System directory
		// so we will set up DLL and XPT in their new home
		errBlock2 = addFile (PLID, VERSION, PLUGIN_FILE, secondaryFolder, null);

		//  Something went wrong if errBlock2 is NOT 0

		if (errBlock2!=0)
		{
		logComment("Could NOT add " + PLUGIN_FILE + " to " + secondaryFolder + ":" + errBlock2);
		return errBlock2;
		} 
	
		// start installing xpt file if this is a scriptable plugin
		errBlock2 = addFile (PLID, VERSION, COMPONENT_FILE, secondaryFolder, null);
		if (errBlock2!=0)
		{
		logComment("Could NOT add " + COMPONENT_FILE + " to " + secondaryFolder + ":" + errBlock2);
		return errBlock2;
		} 

		
	} 
	return 0; // 0 means everything went well with the secondary install
}

function writePLIDSolution()
{

//Concatenate the secondary install path with the filename to make a fully qualified pathname

var qualifiedSecondaryFolderDLL = secondaryFolder + PLUGIN_FILE;
var qualifiedSecondaryFolderXPT = secondaryFolder + COMPONENT_FILE;	

// write PLID keys (mozilla.org/projects/plugins/first-install-problem.html)
// write PLID keys to HKLM

var HKLM_status =  registerPLID(HKEY_LOCAL_MACHINE, REG_MOZ_PATH,
									PLID,
					 		   		qualifiedSecondaryFolderDLL, qualifiedSecondaryFolderXPT,
					 		   		PLUGIN_DESCRIPTION, COMPANY_NAME, SOFTWARE_NAME, VERSION,
					 		   		MIMETYPE, SUFFIX, SUFFIX_DESCRIPTION);

	logComment("Moz First Install Installation: registerPLID("+HKEY_LOCAL_MACHINE+") returned, status "+HKLM_status);

	if (HKLM_status == false)
	{
// write PLID keys (mozilla.org/projects/plugins/first-install-problem.html)
// write PLID keys to HKCU

		var HKCU_status =  registerPLID(HKEY_CURRENT_USER, REG_MOZ_PATH,
									PLID,
					 		   		qualifiedSecondaryFolderDLL, qualifiedSecondaryFolderXPT,
					 		   		PLUGIN_DESCRIPTION, COMPANY_NAME, SOFTWARE_NAME, VERSION,
					 		   		MIMETYPE, SUFFIX, SUFFIX_DESCRIPTION);


		logComment("First Install Installation: registerPLID("+HKEY_CURRENT_USER+") returned, status "+HKLM_status);

		if (HKCU_status != 0)
		{
        	logComment("Could not write to the registry. Errorcode="+HKCU_status);

        		return HKCU_status;
		}
	}

	return 0;


}
/**
 * Function for preinstallation of plugin (FirstInstall).
 * You should not stop the install process because the function failed,
 * you still have a chance to install the plugin for the already
 * installed gecko browsers.
 *
 * @param dirPath   directory path from getFolder
 * @param spaceRequired    required space in kilobytes
 * 
 **/

function verifyDiskSpace(dirPath, spaceRequired)
{
  var spaceAvailable;

  // Get the available disk space on the given path
  spaceAvailable = fileGetDiskSpaceAvailable(dirPath);

  // Convert the available disk space into kilobytes
  spaceAvailable = parseInt(spaceAvailable / 1024);

  // do the verification
  if(spaceAvailable < spaceRequired)
  {
    logComment("Insufficient disk space: " + dirPath);
    logComment("  required : " + spaceRequired + " K");
    logComment("  available: " + spaceAvailable + " K");
    return(false);
  }

  return(true);
}
/**
 * Function for writing keys to the Win32 System Registry.
 * You should not stop the install process because the function failed.
 * You still have a chance to install the plugin for the
 * current Gecko browser.
 *
 * @param rootKey   must be one of these two string values HKEY_LOCAL_MACHINE or HKEY_CURRENT_USER
 * @param plidID    PLID for Plugins
 * @param dllAbsolutePath  the fully qualified path to the DLL
 * @param xptAbsolutePath  the fully qualified path to the XPT
 * @param pluginDescription a String describing the plugin
 * @param vendor            a String describing the vendor
 * @param productName       the name of this software
 * @param pluginVersion     version string of the plugin
 * @param mimeType[]        MIME type handled by the plugin
				    Plugins with more than one MIME type
				    might use an array
 * @param suffix[]            Suffix handled by plugin (e.g. .my files)
 * @param suffixDescription[] String describing suffix -- each description matches the corresponding suffix
 **/
function registerPLID(rootKey, plidPath,
					  plidID,
					  dllAbsolutePath, xptAbsolutePath,
					  pluginDescription, vendor, productName, pluginVersion,
					  mimeType, suffix, suffixDescription)
{
	var myRegStatus = 0;
	var coMimetypePath;

	winreg = getWinRegistry();

	if (winreg == null)
	{
		logComment("Moz registerPLID: winreg == null");
		return winregIsNullError;
	}

	// Which root to start from HKLM, HKCU
	if (rootKey == HKEY_LOCAL_MACHINE)
	{
		logComment("Moz registerPLID: rootKey=="+HKEY_LOCAL_MACHINE);
		winreg.setRootKey(winreg.HKEY_LOCAL_MACHINE);
	}
	else if (rootKey == HKEY_CURRENT_USER)
	{
		logComment("Moz registerPLID: rootKey=="+HKEY_CURRENT_USER);
		winreg.setRootKey(winreg.HKEY_CURRENT_USER);
	}
	else
	{
		logComment("Moz registerPlid: invalid rootkey, "+rootKey);
		return invalidRootkeyError;
	}

	if (!winreg.isKeyWritable(plidPath))
	{
		logComment("Moz registerPLID: registry key not writable");
		return registryKeyNotWritableError;
	}

	// If we can't find the plidPath create the key
	if (!winreg.keyExists(plidPath))
	{
		logComment("Moz registerPLID: creating missing key "+plidPath+".");
		myRegStatus = winreg.createKey(plidPath, "");
		if (myRegStatus != 0)
		{
			logComment("Moz registerPLID: Could not create the key, "+plidPath+"as expected. Errorcode="+myRegStatus);
			return myRegStatus;
		}
	}

	// OK were done, let's write info about our plugin, path, productname, etc



	myMozillaPluginPath = plidPath+"\\"+plidID; // For instance. SOFTWARE\MozillaPlugins\@Cycore.com/Cult3DViewer
	myRegStatus = winreg.createKey(plidPath+"\\"+plidID, "");
	if (myRegStatus != 0)
	{
		logComment("Moz registerPLID: could not create the subkey "+plidID+" to "+plidPath+". Errorcode="+myRegStatus);
		return myRegStatus;
	}


	// Write path to DLL
	myRegStatus = winreg.setValueString(myMozillaPluginPath, "Path", dllAbsolutePath);
	if (myRegStatus != 0)
	{
		logComment("Moz registerPLID: Could not write the DLL path value. RegPath="+myMozillaPluginPath+", DLLPath="+dllAbsolutePath+". ErrorCode="+myRegStatus);
		return myRegStatus;
	}

	// Write XPTPath
	myRegStatus = winreg.setValueString(myMozillaPluginPath, "XPTPath", xptAbsolutePath);
	if (myRegStatus != 0)
	{
		logComment("Moz registerPLID: Could not write the XPT path value. RegPath="+myMozillaPluginPath+"XPTPath="+xptAbsolutePath+", as expected. Errorcode="+myRegStatus);
		return myRegStatus;
	}

	// Write the productName
	myRegStatus = winreg.setValueString(myMozillaPluginPath, "ProductName", productName);
	if (myRegStatus != 0)
	{
		logComment("Moz registerPLID: Could not write productName. RegPath="+myMozillaPluginPath+", productName="+productName+", as expected. Errorcode="+myRegStatus);
		return myRegStatus;
	}

	// Write the vendor
	myRegStatus = winreg.setValueString(myMozillaPluginPath, "Vendor", vendor);
	if (myRegStatus != 0)
	{
		logComment("Moz registerPLID: Could not write vendorName. RegPath="+myMozillaPluginPath+", vendorName="+vendor+", as expected. Errorcode="+myRegStatus);
		return myRegStatus;
	}

	// Write the plugin description
	myRegStatus = winreg.setValueString(myMozillaPluginPath, "Description", pluginDescription);
	if (myRegStatus != 0)
	{
		logComment("Moz registerPLID: Could not write plugin description. RegPath="+myMozillaPluginPath+", description="+pluginDescription+", as expected. Errorcode="+myRegStatus);
		return myRegStatus;
	}

	// Write the version number of the plug-in
	myRegStatus = winreg.setValueString(myMozillaPluginPath, "Version", pluginVersion);
	if (myRegStatus != 0)
	{
		logComment("Moz registerPLID: Could not write plugin version. RegPath="+myMozillaPluginPath+", pluginVersion="+pluginVersion+", as expected. Errorcode="+myRegStatus);
		return myRegStatus;
	}

	/////////////////////////////////
	// Write the subkey MimeTypes //
	///////////////////////////////
	var myMimetypePath = myMozillaPluginPath+"\\MimeTypes";
	myRegStatus = winreg.createKey(myMimetypePath, "");
	if (myRegStatus != 0)
	{
		logComment("Moz registerPLID: Could not create the subkey MimeTypes, "+myMimetypePath+" as expected. Errorcode="+myRegStatus);
		return myRegStatus;
	}

//Write as many MIME types under the mimetypes key as you have...

for(i=0; i<mimeType.length; i++)
{
	coMimetypePath = myMimetypePath+"\\"+mimeType[i];
	myRegStatus = winreg.createKey(coMimetypePath, "");
	if (myRegStatus != 0)
	{
		logComment("Moz registerPLID: Could not create the subkey "+coMimetypePath+", as expected. Errorcode="+myRegStatus);
		return myRegStatus;
	}

	// Write the description of the co mimetype
	myRegStatus = winreg.setValueString(coMimetypePath, "Description", suffixDescription[i]);
	if (myRegStatus != 0)
	{
		logComment("Moz registerPLID: Could not create the suffix description value, "+suffixDescription[i]+", as expected. Errorcode="+myRegStatus);
		return myRegStatus;
	}

	
	myRegStatus = winreg.setValueString(coMimetypePath, "Suffixes", suffix[i])
	if (myRegStatus != 0)
	{
		logComment("Moz registerPLID: Could not create the suffixes value, "+suffix[i]+", as expected. Errorcode="+myRegStatus);
		return myRegStatus;
	}
}
	/////////////////////////////////
	// Write the subkey Suffixes  //
	///////////////////////////////

	var suffixPath = myMozillaPluginPath+"\\Suffixes";

	// Write the suffix of the mimetype
	myRegStatus = winreg.createKey(suffixPath, "");
	if (myRegStatus != 0)
	{
		logComment("Moz registerPLID: Could not create the suffix key, "+suffixPath+", as expected. ErrorCode="+myRegStatus);
		return myRegStatus;
	}

for(i=0; i<suffix.length; i++)
{
	// Write the suffix (extension), (one value-key with no value)
	myRegStatus = winreg.setValueString(suffixPath, suffix[i], "\0");
	if (myRegStatus != 0)
	{
		logComment("Moz registerPLID: Could not create the suffix value. RegPath="+suffixPath+", value="+suffix+", as expected. Errorcode="+myRegStatus);
		return myRegStatus;
	}
}

	logComment("Moz registerPLID: Registry keys seems to be written successfully");

	return 0;
}

