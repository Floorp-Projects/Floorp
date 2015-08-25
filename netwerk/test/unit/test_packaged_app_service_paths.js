Cu.import('resource://gre/modules/LoadContextInfo.jsm');
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");

var gRequestNo = 0;
function packagedAppContentHandler(metadata, response)
{
  response.setHeader("Content-Type", 'application/package');
  var body = testData.getData();
  response.bodyOutputStream.write(body, body.length);
  gRequestNo++;
}

function getPrincipal(url) {
  let ssm = Cc["@mozilla.org/scriptsecuritymanager;1"]
              .getService(Ci.nsIScriptSecurityManager);
  let uri = createURI(url);
  return ssm.createCodebasePrincipal(uri, {});
}

var subresourcePaths = [
  [ "/index.html", "index.html" ],
  [ "index.html",  "index.html" ],
  [ "/../../index.html", "index.html" ],
  [ "../../index.html", "index.html" ],
  [ "/hello/./.././index.html", "index.html" ],
  [ "hello/./.././index.html", "index.html" ],
  [ "../hello/index.html", "hello/index.html" ],
  [ "/../hello/index.html", "hello/index.html" ],
  [ "/./././index.html", "index.html" ],
]

var content = "<html><body> Test </body></html>";

// The package content
// getData formats it as described at http://www.w3.org/TR/web-packaging/#streamable-package-format
var testData = {
  token : "gc0pJq0M:08jU534c0p",
  getData: function() {
    var str = "";

    str += "--" + this.token + "\r\n";
    str += "Content-Location: " + subresourcePaths[gRequestNo][0] + "\r\n";
    str += "Content-Type: text/html" + "\r\n";
    str += "\r\n";

    str += content + "\r\n";
    str += "--" + this.token + "--";

    return str;
  }
}

XPCOMUtils.defineLazyGetter(this, "uri", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

// The active http server initialized in run_test
var httpserver = null;
// The packaged app service initialized in run_test
var paservice = null;
// This variable is set before getResource is called. The listener uses this variable
// to check the correct resource path for the returned entry
var packagePath = null;

function run_test()
{
  // setup test
  httpserver = new HttpServer();
  httpserver.registerPrefixHandler("/package/", packagedAppContentHandler);
  httpserver.start(-1);

  paservice = Cc["@mozilla.org/network/packaged-app-service;1"]
                     .getService(Ci.nsIPackagedAppService);

  var testuri = createURI("http://localhost/");

  add_test(continueTest);
  // run tests
  run_next_test();
}

// A listener we use to check the proper cache entry is returned by the service
function packagedResourceListener(path, content) {
  this.path = path;
  this.content = content;
}

packagedResourceListener.prototype = {
  QueryInterface: function (iid) {
    if (iid.equals(Ci.nsICacheEntryOpenCallback) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  onCacheEntryCheck: function() { return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED; },
  onCacheEntryAvailable: function (entry, isnew, appcache, status) {
    equal(status, Cr.NS_OK, "status is NS_OK");
    ok(!!entry, "Needs to have an entry");
    equal(entry.key, uri + packagePath + "!//" + this.path, "Check entry has correct name");
    var inputStream = entry.openInputStream(0);
    pumpReadStream(inputStream, (read) => {
        inputStream.close();
        equal(read, this.content); // not using do_check_eq since logger will fail for the 1/4MB string
        continueTest();
    });
  }
};

var gGenerator = test_paths();
function continueTest() {
  try {
    gGenerator.next();
  } catch (e if e instanceof StopIteration) {
    run_next_test();
  }
}
function test_paths() {
  for (var i in subresourcePaths) {
    packagePath = "/package/" + i;
    dump("Iteration " + i + "\n");
    paservice.getResource(getPrincipal(uri + packagePath + "!//" + subresourcePaths[i][1]), 0,
      LoadContextInfo.default,
      new packagedResourceListener(subresourcePaths[i][1], content));
    yield undefined;
  }
}
