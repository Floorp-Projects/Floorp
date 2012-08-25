const Cc = Components.classes;
const Ci = Components.interfaces;

var etld = Cc["@mozilla.org/network/effective-tld-service;1"]
             .getService(Ci.nsIEffectiveTLDService);

function run_test()
{
  var file = do_get_file("data/test_psl.txt");
  var ios = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);
  var uri = ios.newFileURI(file);
  var scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                       .getService(Ci.mozIJSSubScriptLoader);
  var srvScope = {};
  scriptLoader.loadSubScript(uri.spec, srvScope);
}

function checkPublicSuffix(host, expectedSuffix)
{
  var actualSuffix = null;
  try {
    actualSuffix = etld.getBaseDomainFromHost(host);
  } catch (e if e.name == "NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS" ||
                e.name == "NS_ERROR_ILLEGAL_VALUE") {
  }
  do_check_eq(actualSuffix, expectedSuffix);
}
