function updateWinReg4Ren8dot3() 
{
  var fProgram      = getFolder("Program");
  var fTemp         = getFolder("Temporary");

  //Notes:
  // can't use a double backslash before subkey - Windows already puts it in.			
  // subkeys have to exist before values can be put in.
  var subkey;  // the name of the subkey you are poking around in
  var valname; // the name of the value you want to look at
  var value;   // the data in the value you want to look at.
  var winreg = getWinRegistry() ;

  if(winreg != null) 
  {
    // Here, we get the current version.
    winreg.setRootKey(winreg.HKEY_CURRENT_USER) ;  // CURRENT_USER
    subkey  = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce" ;

    winreg.createKey(subkey,"");
    valname = "ren8dot3";
    value   = fProgram + "ren8dot3.exe " + fTemp + "ren8dot3.ini";
    err     = winreg.setValueString(subkey, valname, value);
  }
}

function prepareRen8dot3(listLongFilePaths)
{
  var fTemp                 = getFolder("Temporary");
  var fProgram              = getFolder("Program");
  var fRen8dot3Ini          = getWinProfile(fTemp, "ren8dot3.ini");
  var bIniCreated           = false;
  var fLongFilePath;
  var sShortFilePath;

  if(fRen8dot3Ini != null)
  {
    for(i = 0; i < listLongFilePaths.length; i++)
    {
      fLongFilePath   = getFolder(fProgram, listLongFilePaths[i]);
      sShortFilePath  = File.windowsGetShortName(fLongFilePath);
      if(sShortFilePath)
      {
        fRen8dot3Ini.writeString("rename", sShortFilePath, fLongFilePath);
        bIniCreated = true;
      }
    }

    if(bIniCreated)
      updateWinReg4Ren8dot3() ;
  }

  return(0);
}

