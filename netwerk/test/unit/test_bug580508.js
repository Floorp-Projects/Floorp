var ioService = Cc["@mozilla.org/network/io-service;1"]
                .getService(Ci.nsIIOService);
var resProt = ioService.getProtocolHandler("resource")
              .QueryInterface(Ci.nsIResProtocolHandler);

function run_test() {
    // Define a resource:// alias that points to another resource:// URI.
    let greModulesURI = ioService.newURI("resource://gre/modules/");
    resProt.setSubstitution("my-gre-modules", greModulesURI);

    // When we ask for the alias, we should not get the resource://
    // URI that we registered it for but the original file URI.
    let greFileSpec = ioService.newURI("modules/", null,
                                       resProt.getSubstitution("gre")).spec;
    let aliasURI = resProt.getSubstitution("my-gre-modules");
    Assert.equal(aliasURI.spec, greFileSpec);

    // Resolving URIs using the original resource path and the alias
    // should yield the same result.
    let greNetUtilURI = ioService.newURI("resource://gre/modules/NetUtil.jsm");
    let myNetUtilURI = ioService.newURI("resource://my-gre-modules/NetUtil.jsm");
    Assert.equal(resProt.resolveURI(greNetUtilURI),
                 resProt.resolveURI(myNetUtilURI));
}
