"use strict";

function run_test() {
  var f = do_get_file("test_bug336501.js");

  var fis = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  fis.init(f, -1, -1, 0);

  var bis = Cc["@mozilla.org/network/buffered-input-stream;1"].createInstance(
    Ci.nsIBufferedInputStream
  );
  bis.init(fis, 32);

  var sis = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
    Ci.nsIScriptableInputStream
  );
  sis.init(bis);

  sis.read(45);
  sis.close();

  var data = sis.read(45);
  Assert.equal(data.length, 0);
}
