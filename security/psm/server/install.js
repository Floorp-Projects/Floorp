function checkError(errorValue)
{
  if((errorValue != SUCCESS) && (errorValue != REBOOT_NEEDED))
  {
    abortInstall(errorValue);
    return(true);
  }

  return(false);
} // end checkError()

function updateWinReg4Ren8dot3() 
{
  fTemp = getFolder("Temporary");

  //Notes:
  // can't use a double backslash before subkey - Windows already puts it in.			
  // subkeys have to exist before values can be put in.
  var winreg = getWinRegistry() ;
  var subkey;  //the name of the subkey you are poking around in
  var valname; // the name of the value you want to look at
  var value;   //the data in the value you want to look at.

  if(winreg != null) 
  {
    // Here, we get the current version.
    winreg.setRootKey(winreg.HKEY_CURRENT_USER) ;  // CURRENT_USER
    subkey  = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce" ;
    winreg.createKey(subkey,"");
    valname = "ren8dot3";
    value   = fPSM + "ren8dot3.exe " + fTemp + "ren8dot3.ini";
    err     = winreg.setValueString(subkey, valname, value);
  }
}

function updatePrivateProfile()
{
    fTemp        = getFolder("Temporary");
    fProgram     = getFolder("Program");
    fRen8dot3Ini = getWinProfile(fTemp, "ren8dot3.ini");

    err = fRen8dot3Ini.writeString("rename", fPSM + "nsbrk3~1.dll", fPSM + "nsbrk3231.dll");
    err = fRen8dot3Ini.writeString("rename", fPSM + "nscnv3~1.dll", fPSM + "nscnv3231.dll");
    err = fRen8dot3Ini.writeString("rename", fPSM + "nscol3~1.dll", fPSM + "nscol3231.dll");
    err = fRen8dot3Ini.writeString("rename", fPSM + "nsfmt3~1.dll", fPSM + "nsfmt3231.dll");
    err = fRen8dot3Ini.writeString("rename", fPSM + "nsres3~1.dll", fPSM + "nsres3231.dll");
    err = fRen8dot3Ini.writeString("rename", fPSM + "nsuni3~1.dll", fPSM + "nsuni3231.dll");

    return(0);
}

function updateWindowsRegistry(psmPath)
{
  var winReg = getWinRegistry();
  if(winReg != null)
  {
    winReg.setRootKey(winReg.HKEY_LOCAL_MACHINE);
    subKey    = "SOFTWARE\\Netscape\\Personal Security Manager\\Main";
    valueName = "Install Directory";
    err       = winReg.createKey(subKey, "");
    err       = winReg.setValueString(subKey, valueName, psmPath);
  }
  else
  {
    logComment("getWinRegsitry() failed: " + winReg);
  }
}


// main
var err = startInstall("Netscape Personal Security Manager", "/Netscape/Personal Security Manager", "1.1.0.00058"); 
logComment("startInstall() returned: " + err);

fPSM     = getFolder("Communicator","psm");

setPackageFolder(fPSM);
err  = addDirectory("/Netscape/Personal Security Manager/Program",
                    "1.2.0.00001",
                    "psm",                 // dir name in jar to extract 
                    fPSM,                  // Where to put this file (Returned from GetFolder) 
                    "",                    // subdir name to create relative to communicatorFolder
                    true );                // Force Flag 
logComment("addDirectory() returned: " + err);

// check return value
if(!checkError(err))
{
    updateWindowsRegistry(fPSM);
    updatePrivateProfile();
    updateWinReg4Ren8dot3();
    err = finalizeInstall(); 
    logComment("finalizeInstall() returned: " + err);
}

// end main
