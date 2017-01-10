function round_trip(uri) {
  var objectOutStream = Cc["@mozilla.org/binaryoutputstream;1"].
                       createInstance(Ci.nsIObjectOutputStream);
  var pipe = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);
  pipe.init(false, false, 0, 0xffffffff, null);
  objectOutStream.setOutputStream(pipe.outputStream);
  objectOutStream.writeCompoundObject(uri, Ci.nsISupports, true);
  objectOutStream.close();

  var objectInStream = Cc["@mozilla.org/binaryinputstream;1"].
                       createInstance(Ci.nsIObjectInputStream);
  objectInStream.setInputStream(pipe.inputStream);
  return objectInStream.readObject(true).QueryInterface(Ci.nsIURI);
}

var prefData =
  [
    {
      name: "network.IDN_show_punycode",
      newVal: false
    },
    {
      name: "network.IDN.whitelist.ch",
      newVal: true
    }
  ];

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  var uri1 = ios.newURI("file:///");
  do_check_true(uri1 instanceof Ci.nsIFileURL);

  var uri2 = uri1.clone();
  do_check_true(uri2 instanceof Ci.nsIFileURL);
  do_check_true(uri1.equals(uri2));

  var uri3 = round_trip(uri1);
  do_check_true(uri3 instanceof Ci.nsIFileURL);
  do_check_true(uri1.equals(uri3));

  // Make sure our prefs are set such that this test actually means something
  var prefs = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);
  for (var pref of prefData) {
    try {
      pref.oldVal = prefs.getBoolPref(pref.name);
    } catch(e) {
    }
    prefs.setBoolPref(pref.name, pref.newVal);
  }
  
  try {
    // URI stolen from
    // http://lists.w3.org/Archives/Public/public-iri/2004Mar/0012.html
    var uri4 = ios.newURI("http://xn--jos-dma.example.net.ch/");
    do_check_eq(uri4.asciiHost, "xn--jos-dma.example.net.ch");
    do_check_eq(uri4.host, "jos\u00e9.example.net.ch");
    
    var uri5 = round_trip(uri4);
    do_check_true(uri4.equals(uri5));
    do_check_eq(uri4.host, uri5.host);
    do_check_eq(uri4.asciiHost, uri5.asciiHost);
  } finally {
    for (var pref of prefData) {
      if (prefs.prefHasUserValue(pref.name))
        prefs.clearUserPref(pref.name);
    }
  }
}
