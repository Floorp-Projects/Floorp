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
    return(false);
  }

  return(true);
}

// this function deletes a file if it exists
function deleteThisFile(dirKey, file)
{
  var fFileToDelete;

  fFileToDelete = getFolder(dirKey, file);
  logComment(file + " file: " + fFileToDelete);
  if(File.exists(fFileToDelete))
  {
    fileDelete(fFileToDelete);
    return(true);
  }
  else
    return(false);
}

// this function deletes a folder (recursively) if it exists
function deleteThisFolder(dirKey, folder)
{
  var fToDelete;

  fToDelete = getFolder(dirKey, folder);
  logComment(folder + " folder: " + fToDelete);
  if(File.exists(fToDelete))
  {
    File.dirRemove(fToDelete, true);
    return(true);
  }
  else
    return(false);
}

