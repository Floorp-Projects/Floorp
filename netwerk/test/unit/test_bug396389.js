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
  Assert.ok(uri1 instanceof Ci.nsIFileURL);

  var uri2 = uri1.clone();
  Assert.ok(uri2 instanceof Ci.nsIFileURL);
  Assert.ok(uri1.equals(uri2));

  var uri3 = round_trip(uri1);
  Assert.ok(uri3 instanceof Ci.nsIFileURL);
  Assert.ok(uri1.equals(uri3));

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
    Assert.equal(uri4.asciiHost, "xn--jos-dma.example.net.ch");
    Assert.equal(uri4.displayHost, "jos\u00e9.example.net.ch");
    
    var uri5 = round_trip(uri4);
    Assert.ok(uri4.equals(uri5));
    Assert.equal(uri4.displayHost, uri5.displayHost);
    Assert.equal(uri4.asciiHost, uri5.asciiHost);
  } finally {
    for (var pref of prefData) {
      if (prefs.prefHasUserValue(pref.name))
        prefs.clearUserPref(pref.name);
    }
  }
}
