// this function verifies disk space in kilobytes
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
    abortInstall(INSUFFICIENT_DISK_SPACE);
    return(false);
  }

  return(true);
}

function checkError(errorValue)
{
  if((errorValue != SUCCESS) && (errorValue != REBOOT_NEEDED))
  {
    abortInstall(errorValue);
    return(true);
  }

  return(false);
} // end checkError()

