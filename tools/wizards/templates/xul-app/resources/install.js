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

var srDest = ${install_size_kilobytes};

var err = initInstall("${app_name_long}", "${app_name_short}",
                      "${app_version}"); 

logComment("initInstall: " + err);

if (verifyDiskSpace(getFolder("Program"), srDest))
{
    addFile("${app_name_long}",
            "bin/chrome/${jar_file_name}", // jar source folder 
            getFolder("Chrome"),        // target folder
            "");                        // target subdir 

    registerChrome(PACKAGE | DELAYED_CHROME, 
                   getFolder("Chrome","${jar_file_name}"),
                   "${content_reg_dir}");
    registerChrome(LOCALE | DELAYED_CHROME, getFolder("Chrome",
                                                      "${jar_file_name}"),
                   "${locale_reg_dir}");
    registerChrome(SKIN | DELAYED_CHROME, getFolder("Chrome",
                                                    "${jar_file_name}"),
                   "${skin_reg_dir}");

    if (err==SUCCESS)
        performInstall(); 
    else
        cancelInstall(err);
}
else
    cancelInstall(INSUFFICIENT_DISK_SPACE);
