function run_test() {
  try {
    var sts = Components.classes["@mozilla.org/network/socket-transport-service;1"]
                        .getService(Components.interfaces.nsISocketTransportService);
  } catch(e) {}

  Assert.ok(!!sts);
}
