function checkError(errorValue)
{
  if((errorValue != SUCCESS) && (errorValue != REBOOT_NEEDED))
  {
    abortInstall(errorValue);
    return(true);
  }

  return(false);
} // end checkError()

// main
var err = startInstall("Netscape Personal Security Manager", "/Netscape/Personal Security Manager", "1.1.0.00058"); 
logComment("startInstall() returned: " + err);

fPSM     = getFolder("Communicator","psm");

setPackageFolder(fPSM);
err  = addDirectory("/Netscape/Personal Security Manager/Program",
                    "1.1.0.00058",
                    "psm",                 // dir name in jar to extract 
                    fPSM,                  // Where to put this file (Returned from GetFolder) 
                    "",                    // subdir name to create relative to communicatorFolder
                    true );                // Force Flag 
logComment("addDirectory() returned: " + err);

// check return value
if(!checkError(err))
{
    err = finalizeInstall(); 
    logComment("finalizeInstall() returned: " + err);
}

// end main
