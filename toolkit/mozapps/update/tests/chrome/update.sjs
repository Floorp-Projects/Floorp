/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

/**
 * Server side http server script for application update tests.
 */

// ChromeUtils isn't available in sjs files so disable the eslint rule for it.
/* eslint-disable mozilla/use-chromeutils-import */

// Definitions from files used by this file
/* global getState */

Cu.import("resource://gre/modules/Services.jsm");

function getTestDataFile(aFilename) {
  let file = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  let pathParts = REL_PATH_DATA.split("/");
  for (let i = 0; i < pathParts.length; ++i) {
    file.append(pathParts[i]);
  }
  if (aFilename) {
    file.append(aFilename);
  }
  return file;
}

function loadHelperScript(aScriptFile) {
  let scriptSpec = Services.io.newFileURI(aScriptFile).spec;
  Services.scriptloader.loadSubScript(scriptSpec, this);
}

var scriptFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
scriptFile.initWithPath(getState("__LOCATION__"));
scriptFile = scriptFile.parent;
/* import-globals-from testConstants.js */
scriptFile.append("testConstants.js");
loadHelperScript(scriptFile);

/* import-globals-from ../data/sharedUpdateXML.js */
scriptFile = getTestDataFile("sharedUpdateXML.js");
loadHelperScript(scriptFile);

const SERVICE_URL = URL_HOST + "/" + REL_PATH_DATA + FILE_SIMPLE_MAR;
const BAD_SERVICE_URL = URL_HOST + "/" + REL_PATH_DATA + "not_here.mar";

const SLOW_MAR_DOWNLOAD_INTERVAL = 100;
var gTimer;

function handleRequest(aRequest, aResponse) {
  let params = { };
  if (aRequest.queryString) {
    params = parseQueryString(aRequest.queryString);
  }

  let statusCode = params.statusCode ? parseInt(params.statusCode) : 200;
  let statusReason = params.statusReason ? params.statusReason : "OK";
  aResponse.setStatusLine(aRequest.httpVersion, statusCode, statusReason);
  aResponse.setHeader("Cache-Control", "no-cache", false);

  // When a mar download is started by the update service it can finish
  // downloading before the ui has loaded. By specifying a serviceURL for the
  // update patch that points to this file and has a slowDownloadMar param the
  // mar will be downloaded asynchronously which will allow the ui to load
  // before the download completes.
  if (params.slowDownloadMar) {
    aResponse.processAsync();
    aResponse.setHeader("Content-Type", "binary/octet-stream");
    aResponse.setHeader("Content-Length", SIZE_SIMPLE_MAR);
    var continueFile = getTestDataFile("continue");
    var contents = readFileBytes(getTestDataFile(FILE_SIMPLE_MAR));
    gTimer = Cc["@mozilla.org/timer;1"].
             createInstance(Ci.nsITimer);
    gTimer.initWithCallback(function(aTimer) {
      if (continueFile.exists()) {
        gTimer.cancel();
        aResponse.write(contents);
        aResponse.finish();
      }
    }, SLOW_MAR_DOWNLOAD_INTERVAL, Ci.nsITimer.TYPE_REPEATING_SLACK);
    return;
  }

  if (params.uiURL) {
    let remoteType = "";
    if (!params.remoteNoTypeAttr && params.uiURL == "BILLBOARD") {
      remoteType = " " + params.uiURL.toLowerCase() + "=\"1\"";
    }
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

  if (params.unsupported) {
    aResponse.write(getRemoteUpdatesXMLString("  <update type=\"major\" " +
                                              "unsupported=\"true\" " +
                                              "detailsURL=\"" + URL_HOST +
                                              "\"></update>\n"));
    return;
  }

  let size;
  let patches = "";
  let url = params.badURL ? BAD_SERVICE_URL : SERVICE_URL;
  if (!params.partialPatchOnly) {
    size = SIZE_SIMPLE_MAR + (params.invalidCompleteSize ? "1" : "");
    let patchProps = {type: "complete",
                      url,
                      size};
    patches += getRemotePatchString(patchProps);
  }

  if (!params.completePatchOnly) {
    size = SIZE_SIMPLE_MAR + (params.invalidPartialSize ? "1" : "");
    let patchProps = {type: "partial",
                      url,
                      size};
    patches += getRemotePatchString(patchProps);
  }

  let updateProps = {};
  if (params.type) {
    updateProps.type = params.type;
  }

  if (params.name) {
    updateProps.name = params.name;
  }

  if (params.appVersion) {
    updateProps.appVersion = params.appVersion;
  }

  if (params.displayVersion) {
    updateProps.displayVersion = params.displayVersion;
  }

  if (params.buildID) {
    updateProps.buildID = params.buildID;
  }

  if (params.promptWaitTime) {
    updateProps.promptWaitTime = params.promptWaitTime;
  }

  let updates = getRemoteUpdateString(updateProps, patches);
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
  let paramArray = aQueryString.split("&");
  let regex = /^([^=]+)=(.*)$/;
  let params = {};
  for (let i = 0, sz = paramArray.length; i < sz; i++) {
    let match = regex.exec(paramArray[i]);
    if (!match) {
      throw Components.Exception(
        "Bad parameter in queryString! '" + paramArray[i] + "'",
        Cr.NS_ERROR_ILLEGAL_VALUE);
    }
    params[decodeURIComponent(match[1])] = decodeURIComponent(match[2]);
  }

  return params;
}
