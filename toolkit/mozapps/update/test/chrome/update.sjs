#include ../shared.js

const URL_HOST = "http://example.com/";
const URL_PATH = "chrome/toolkit/mozapps/update/test/chrome/";
const URL_UPDATE = URL_HOST + URL_PATH + "update.sjs";
const SHA512_HASH = "1d2307e309587ddd04299423b34762639ce6af3ee17cfdaa8fdd4e6" +
                    "6b5a61bfb6555b6e40a82604908d6d68d3e42f318f82e22b6f5e111" +
                    "8b4222e3417a2fa2d0";
const SERVICE_URL = URL_HOST + URL_PATH + "empty.mar";

const SLOW_MAR_DOWNLOAD_INTERVAL = 100;

function handleRequest(aRequest, aResponse) {
  var params = { };
  if (aRequest.queryString)
    params = parseQueryString(aRequest.queryString);

  var statusCode = params.statusCode ? parseInt(params.statusCode) : 200;
  var statusReason = params.statusReason ? params.statusReason : "OK";
  aResponse.setStatusLine(aRequest.httpVersion, statusCode, statusReason);
  aResponse.setHeader("Cache-Control", "no-cache", false);
  
  if (params.addonID) {
    aResponse.write(getUpdateRDF(params));
    return;
  }

  // When a mar download is started by the update service it can finish
  // downloading before the ui has loaded. By specifying a serviceURL for the
  // update patch that points to this file and has a slowDownloadMar param the
  // mar will be downloaded asynchronously which will allow the ui to load
  // before the download completes.
  if (params.slowDownloadMar) {
    aResponse.processAsync();
    aResponse.setHeader("Content-Type", "binary/octet-stream");
    aResponse.setHeader("Content-Length", "775");
    var marFile = AUS_Cc["@mozilla.org/file/directory_service;1"].
                  getService(AUS_Ci.nsIProperties).
                  get("CurWorkD", AUS_Ci.nsILocalFile);
    var path = URL_PATH + "empty.mar";
    var pathParts = path.split("/");
    for(var i = 0; i < pathParts.length; ++i)
      marFile.append(pathParts[i]);
    var contents = readFileBytes(marFile);
    var timer = AUS_Cc["@mozilla.org/timer;1"].
                createInstance(AUS_Ci.nsITimer);
    timer.initWithCallback(function(aTimer) {
      aResponse.write(contents);
      aResponse.finish();
    }, SLOW_MAR_DOWNLOAD_INTERVAL, AUS_Ci.nsITimer.TYPE_ONE_SHOT);
    return;
  }

  if (params.uiURL) {
    var remoteType = "";
    if (!params.remoteNoTypeAttr &&
        (params.uiURL == "BILLBOARD" || params.uiURL == "LICENSE"))
      remoteType = " " + params.uiURL.toLowerCase() + "=\"1\"";
    aResponse.write("<html><head><meta http-equiv=\"content-type\" content=" +
                    "\"text/html; charset=utf-8\"></head><body" +
                    remoteType + ">" + params.uiURL +
                    "<br><br>this is a test mar that will not affect your " +
                    "build.</body></html>");
    return;
  }

  if (params.xmlMalformed) {
    aResponse.write("xml error");
    return;
  }

  if (params.noUpdates) {
    aResponse.write(getRemoteUpdatesXMLString(""));
    return;
  }

  var hash;
  var patches = "";
  if (!params.partialPatchOnly) {
    hash = SHA512_HASH + (params.invalidCompleteHash ? "e" : "");
    patches += getRemotePatchString("complete", SERVICE_URL, "SHA512",
                                    hash, "775");
  }

  if (!params.completePatchOnly) {
    hash = SHA512_HASH + (params.invalidPartialHash ? "e" : "");
    patches += getRemotePatchString("partial", SERVICE_URL, "SHA512",
                                    hash, "775");
  }

  var type = params.type ? params.type : "major";
  var name = params.name ? params.name : "App Update Test";
  var appVersion = params.appVersion ? params.appVersion : "99.9";
  var displayVersion = params.displayVersion ? params.displayVersion
                                             : "version " + appVersion;
  var platformVersion = params.platformVersion ? params.platformVersion : "99.8";
  var buildID = params.buildID ? params.buildID : "01234567890123";
  // XXXrstrong - not specifying a detailsURL will cause a leak due to bug 470244
//  var detailsURL = params.showDetails ? URL_UPDATE + "?uiURL=DETAILS" : null;
  var detailsURL = URL_UPDATE + "?uiURL=DETAILS";
  var billboardURL = params.showBillboard ? URL_UPDATE + "?uiURL=BILLBOARD" : null;
  if (billboardURL && params.remoteNoTypeAttr)
    billboardURL += "&amp;remoteNoTypeAttr=1";
  if (params.billboard404)
    billboardURL = URL_HOST + URL_PATH + "missing.html";
  var licenseURL = params.showLicense ? URL_UPDATE + "?uiURL=LICENSE" : null;
  if (licenseURL && params.remoteNoTypeAttr)
    licenseURL += "&amp;remoteNoTypeAttr=1";
  if (params.license404)
    licenseURL = URL_HOST + URL_PATH + "missing.html";
  var showPrompt = params.showPrompt ? "true" : null;
  var showNever = params.showNever ? "true" : null;
  var showSurvey = params.showSurvey ? "true" : null;
  var extra1 = params.extra1 ? params.extra1 : null;
  var updates = getRemoteUpdateString(patches, type, "App Update Test",
                                      displayVersion, appVersion,
                                      platformVersion, buildID,
                                      detailsURL, billboardURL,
                                      licenseURL, showPrompt,
                                      showNever, showSurvey, "test extra1");

  aResponse.write(getRemoteUpdatesXMLString(updates));
}

/**
 * Helper function to create a JS object representing the url parameters from
 * the request's queryString.
 *
 * @param  aQueryString
 *         The request's query string.
 * @return A JS object representing the url parameters from the request's
 *         queryString.
 */
function parseQueryString(aQueryString) {
  var paramArray = aQueryString.split("&");
  var regex = /^([^=]+)=(.*)$/;
  var params = {};
  for (var i = 0, sz = paramArray.length; i < sz; i++) {
    var match = regex.exec(paramArray[i]);
    if (!match)
      throw "Bad parameter in queryString!  '" + paramArray[i] + "'";
    params[decodeURIComponent(match[1])] = decodeURIComponent(match[2]);
  }

  return params;
}

/**
 * Helper function to gets the string representation of the contents of the
 * add-on's update manifest file.
 *
 * @param  aParams
 *         A JS object representing the url parameters from the request's
 *         queryString.
 * @return A string representation of the contents of the add-on's update
 *         manifest file.
 */
function getUpdateRDF(aParams) {
  var addonVersion;
  var addonID = aParams.addonID;
  var addonUpdateType = addonID.split("_")[0];
  var maxVersion = aParams.platformVersion;

  switch (addonUpdateType) {
    case "updatecompatibility":
      // Use "1.0" for the add-on version for the compatibility update case since
      // the tests create all add-ons with "1.0" for the version.
      addonVersion = "1.0";
      break;
    case "updateversion":
      // Use "2.0" for the add-on version for the version update case since the
      // tests create all add-ons with "1.0" for the version.
      addonVersion = "2.0";
      break;
    default:
      return "<?xml version=\"1.0\"?>\n" +
             "<RDF:RDF xmlns:RDF=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" " +
             "         xmlns:em=\"http://www.mozilla.org/2004/em-rdf#\">\n" +
             "</RDF:RDF>\n";
  }

  return "<?xml version=\"1.0\"?>\n" +
         "<RDF:RDF xmlns:RDF=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" " +
         "         xmlns:em=\"http://www.mozilla.org/2004/em-rdf#\">\n" +
         "  <RDF:Description about=\"urn:mozilla:extension:" + addonID + "\">\n" +
         "    <em:updates>\n" +
         "      <RDF:Seq>\n" +
         "        <RDF:li resource=\"urn:mozilla:extension:" + addonID + ":" + addonVersion + "\"/>\n" +
         "      </RDF:Seq>\n" +
         "    </em:updates>\n" +
         "  </RDF:Description>\n" +
         "  <RDF:Description about=\"urn:mozilla:extension:" + addonID + ":" + addonVersion + "\">\n" +
         "    <em:version>" + addonVersion + "</em:version>\n" +
         "    <em:targetApplication>\n" +
         "      <RDF:Description>\n" +
         "        <em:id>toolkit@mozilla.org</em:id>\n" +
         "        <em:minVersion>0</em:minVersion>\n" +
         "        <em:maxVersion>" + maxVersion + "</em:maxVersion>\n" +
         "        <em:updateLink>" + URL_HOST + URL_PATH + "</em:updateLink>\n" +
         "        <em:updateHash>sha256:0</em:updateHash>\n" + 
         "      </RDF:Description>\n" +
         "    </em:targetApplication>\n" +
         "  </RDF:Description>\n" +
         "</RDF:RDF>\n";
}
