var Cc = Components.classes;
var Ci = Components.interfaces;

function run_test() {
  var f =
      Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties).get("ComRegF", Ci.nsIFile);

  var fis =
      Cc["@mozilla.org/network/file-input-stream;1"].
      createInstance(Ci.nsIFileInputStream);
  fis.init(f, -1, -1, 0); 

  var bis =
      Cc["@mozilla.org/network/buffered-input-stream;1"].
      createInstance(Ci.nsIBufferedInputStream);
  bis.init(fis, 32);

  var sis =
      Cc["@mozilla.org/scriptableinputstream;1"].
      createInstance(Ci.nsIScriptableInputStream);
  sis.init(bis);

  sis.read(45);
  sis.close();

  var data = sis.read(45);
  do_check_eq(data.length, 0);
}
