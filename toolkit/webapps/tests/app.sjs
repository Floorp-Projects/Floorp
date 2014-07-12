var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

function getQuery(request) {
  let query = {};

  request.queryString.split('&').forEach(function(val) {
    let [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  return query;
}

function getTestFile(aName) {
  var file = Services.dirsvc.get("CurWorkD", Ci.nsIFile);

  var path = "chrome/toolkit/webapps/tests/data/app/" + aName;

  path.split("/").forEach(function(component) {
    file.append(component);
  });

  return file;
}

function readFile(aFile) {
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  fstream.init(aFile, -1, 0, 0);
  var data = NetUtil.readInputStreamToString(fstream, fstream.available());
  fstream.close();
  return data;
}

function getHostedManifest(aVersion) {
  return readFile(getTestFile("hosted_manifest.webapp")).
           replace(/VERSION_TOKEN/g, aVersion);
}

function getManifest(aVersion) {
  return readFile(getTestFile("manifest.webapp")).
           replace(/VERSION_TOKEN/g, aVersion);
}

function buildAppPackage(aVersion) {
  const PR_RDWR        = 0x04;
  const PR_CREATE_FILE = 0x08;
  const PR_TRUNCATE    = 0x20;

  let zipFile = Services.dirsvc.get("TmpD", Ci.nsIFile);
  zipFile.append("application.zip");

  let zipWriter = Cc["@mozilla.org/zipwriter;1"].createInstance(Ci.nsIZipWriter);
  zipWriter.open(zipFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

  // Add index.html file to the zip file
  zipWriter.addEntryFile("index.html",
                         Ci.nsIZipWriter.COMPRESSION_NONE,
                         getTestFile("index.html"),
                         false);

  // Add manifest to the zip file
  var manStream = Cc["@mozilla.org/io/string-input-stream;1"].
                  createInstance(Ci.nsIStringInputStream);
  var manifest = getManifest(aVersion);
  manStream.setData(manifest, manifest.length);
  zipWriter.addEntryStream("manifest.webapp", Date.now(),
                           Ci.nsIZipWriter.COMPRESSION_NONE,
                           manStream, false);

  zipWriter.close();

  return readFile(zipFile);
}

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  let query = getQuery(request);

  if ("appreq" in query) {
    response.setHeader("Content-Type", "text/plain", false);
    response.write("Hello world!");

    setState("appreq", "done");

    return;
  }

  if ("testreq" in query) {
    response.setHeader("Content-Type", "text/plain", false);

    response.write(getState("appreq"));

    return;
  }

  if ("setVersion" in query) {
    setState("version", query.setVersion);
    response.write("OK");
    return;
  }
  var version = Number(getState("version"));

  if ("getPackage" in query) {
    response.setHeader("Content-Type", "application/zip", false);
    response.write(buildAppPackage(version));

    var getPackageQueries = Number(getState("getPackageQueries"));
    setState("getPackageQueries", String(++getPackageQueries));

    return;
  }

  if ("getPackageQueries" in query) {
    response.setHeader("Content-Type", "text/plain", false);
    response.write(String(Number(getState("getPackageQueries"))));
    return;
  }

  if ("getManifest" in query) {
    response.setHeader("Content-Type", "application/x-web-app-manifest+json", false);
    response.write(getManifest(version));

    var getManifestQueries = Number(getState("getManifestQueries"));
    setState("getManifestQueries", String(++getManifestQueries));

    return;
  }

  if ("getHostedManifest" in query) {
    response.setHeader("Content-Type", "application/x-web-app-manifest+json", false);
    response.write(getHostedManifest(version));

    var getManifestQueries = Number(getState("getManifestQueries"));
    setState("getManifestQueries", String(++getManifestQueries));

    return;
  }

  if ("getManifestQueries" in query) {
    response.setHeader("Content-Type", "text/plain", false);
    response.write(String(Number(getState("getManifestQueries"))));
    return;
  }
}
