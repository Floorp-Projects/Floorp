function run_test() {
  var testBundle = do_get_file("data/test_bug446708");

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
  zipW.open(tmpFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
  AddToZip(zipW, "", bundle); 
  zipW.close();
}

