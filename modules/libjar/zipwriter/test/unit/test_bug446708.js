function run_test() {
  var testBundle = do_get_file("modules/libjar/zipwriter/test/unit/data/test_bug446708");

  RecursivelyZipDirectory(testBundle);
}

// Add |file| to the zip. |path| is the current path for the file.
function AddToZip(zipWriter, path, file)
{
  var currentPath = path + file.leafName;
  
  if (file.isDirectory()) {
    currentPath += "/";
  }
  
  // THIS IS WHERE THE ERROR OCCURS, FOR THE FILE "st14-1.tiff" IN "test_bug446708"
  zipWriter.addEntryFile(currentPath, Ci.nsIZipWriter.COMPRESSION_DEFAULT, file, false);
  
  // if it's a dir, continue adding its contents recursively...
  if (file.isDirectory()) {
    var entries = file.QueryInterface(Components.interfaces.nsIFile).directoryEntries;
    while (entries.hasMoreElements()) {
      var entry = entries.getNext().QueryInterface(Components.interfaces.nsIFile);
      AddToZip(zipWriter, currentPath, entry);
    }
  }
  
  // ...otherwise, we're done
}

function RecursivelyZipDirectory(bundle)
{
  // create directory service
  var dirUtils = Components.classes["@mozilla.org/file/directory_service;1"]
      .createInstance(Components.interfaces.nsIProperties);
  
  // get the temp dir, where our temporary zip attachments can be stored
  var tempFile = dirUtils.get("TmpD", Components.interfaces.nsIFile).clone();

  // create unique file there
  tempFile.append(bundle.leafName + ".zip");
  tempFile.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 
                        0600);
  
  zipW.open(tempFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
  
  AddToZip(zipW, "", bundle); 
  
  // we're done.
  zipW.close();
}

