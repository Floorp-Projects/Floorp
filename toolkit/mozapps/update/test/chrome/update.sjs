#include ../shared.js

const URL_HOST = "http://example.com/";
const URL_PATH = "chrome/toolkit/mozapps/update/test/chrome/";
const URL_UPDATE = URL_HOST + URL_PATH + "update.sjs";
const SHA512_HASH = "1d2307e309587ddd04299423b34762639ce6af3ee17cfdaa8fdd4e6" +
                    "6b5a61bfb6555b6e40a82604908d6d68d3e42f318f82e22b6f5e111" +
                    "8b4222e3417a2fa2d0";
const SERVICE_URL = URL_HOST + URL_PATH + "empty.mar";


function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Cache-Control", "no-cache", false);
  if (request.queryString.match(/^uiURL=/)) {
    var uiURL = decodeURIComponent(request.queryString.substring(6));
    response.write("<html><head><meta http-equiv=\"content-type\" content=" +
                   "\"text/html; charset=utf-8\"></head><body>" + uiURL +
                   "<br><br>this is a test mar that will not affect your " +
                   "build.</body></html>");
    return;
  }

  var params = { };
  if (request.queryString)
    params = parseQueryString(request.queryString);

  if (params.xmlMalformed) {
    response.write("xml error");
    return;
  }

  if (params.noUpdates) {
    response.write(getRemoteUpdatesXMLString(""));
    return;
  }

  var patches = getRemotePatchString("complete", SERVICE_URL, "SHA512",
                                     SHA512_HASH, "775");

  if (!params.completePatchOnly)
    patches += getRemotePatchString("partial", SERVICE_URL, "SHA512",
                                    SHA512_HASH, "775");
  var type = params.type ? params.type : "major";
  var name = params.name ? params.name : "App Update Test";
  var appVersion = params.appVersion ? params.appVersion : "99.9";
  var displayVersion = params.displayVersion ? params.displayVersion : "version " + appVersion;
  var platformVersion = params.platformVersion ? params.platformVersion : "99.8";
  var buildID = params.buildID ? params.buildID : "01234567890123";
  // XXXrstrong - not specifying a detailsURL will cause a leak due to bug 470244
//  var detailsURL = params.showDetails ? URL_UPDATE + "?uiURL=DETAILS" : null;
  var detailsURL = URL_UPDATE + "?uiURL=DETAILS";
  var billboardURL = params.showBillboard ? URL_UPDATE + "?uiURL=BILLBOARD" : null;
  var licenseURL = params.showLicense ? URL_UPDATE + "?uiURL=LICENSE" : null;
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

  response.write(getRemoteUpdatesXMLString(updates));
}

function parseQueryString(str) {
  var paramArray = str.split("&");
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
