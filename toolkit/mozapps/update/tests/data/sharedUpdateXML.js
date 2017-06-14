/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Helper functions for creating xml strings used by application update tests.
 *
 * !IMPORTANT - This file contains everything needed (along with dependencies)
 * by the updates.sjs file used by the mochitest-chrome tests. Since xpcshell
 * used by the http server is launched with -v 170 this file must not use
 * features greater than JavaScript 1.7.
 */

/* eslint-disable no-undef */

const FILE_SIMPLE_MAR = "simple.mar";
const SIZE_SIMPLE_MAR = "1031";
const MD5_HASH_SIMPLE_MAR    = "1f8c038577bb6845d94ccec4999113ee";
const SHA1_HASH_SIMPLE_MAR   = "5d49a672c87f10f31d7e326349564a11272a028b";
const SHA256_HASH_SIMPLE_MAR = "1aabbed5b1dd6e16e139afc5b43d479e254e0c26" +
                               "3c8fb9249c0a1bb93071c5fb";
const SHA384_HASH_SIMPLE_MAR = "26615014ea034af32ef5651492d5f493f5a7a1a48522e" +
                               "d24c366442a5ec21d5ef02e23fb58d79729b8ca2f9541" +
                               "99dd53";
const SHA512_HASH_SIMPLE_MAR = "922e5ae22081795f6e8d65a3c508715c9a314054179a8" +
                               "bbfe5f50dc23919ad89888291bc0a07586ab17dd0304a" +
                               "b5347473601127571c66f61f5080348e05c36b";

const STATE_NONE            = "null";
const STATE_DOWNLOADING     = "downloading";
const STATE_PENDING         = "pending";
const STATE_PENDING_SVC     = "pending-service";
const STATE_APPLYING        = "applying";
const STATE_APPLIED         = "applied";
const STATE_APPLIED_SVC     = "applied-service";
const STATE_SUCCEEDED       = "succeeded";
const STATE_DOWNLOAD_FAILED = "download-failed";
const STATE_FAILED          = "failed";

const LOADSOURCE_ERROR_WRONG_SIZE              = 2;
const CRC_ERROR                                = 4;
const READ_ERROR                               = 6;
const WRITE_ERROR                              = 7;
const MAR_CHANNEL_MISMATCH_ERROR               = 22;
const VERSION_DOWNGRADE_ERROR                  = 23;
const SERVICE_COULD_NOT_COPY_UPDATER           = 49;
const SERVICE_INVALID_APPLYTO_DIR_STAGED_ERROR = 52;
const SERVICE_INVALID_APPLYTO_DIR_ERROR        = 54;
const SERVICE_INVALID_INSTALL_DIR_PATH_ERROR   = 55;
const SERVICE_INVALID_WORKING_DIR_PATH_ERROR   = 56;
const INVALID_APPLYTO_DIR_STAGED_ERROR         = 72;
const INVALID_APPLYTO_DIR_ERROR                = 74;
const INVALID_INSTALL_DIR_PATH_ERROR           = 75;
const INVALID_WORKING_DIR_PATH_ERROR           = 76;
const INVALID_CALLBACK_PATH_ERROR              = 77;
const INVALID_CALLBACK_DIR_ERROR               = 78;

const STATE_FAILED_DELIMETER = ": ";

const STATE_FAILED_LOADSOURCE_ERROR_WRONG_SIZE =
  STATE_FAILED + STATE_FAILED_DELIMETER + LOADSOURCE_ERROR_WRONG_SIZE;
const STATE_FAILED_CRC_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + CRC_ERROR;
const STATE_FAILED_READ_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + READ_ERROR;
const STATE_FAILED_WRITE_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + WRITE_ERROR;
const STATE_FAILED_MAR_CHANNEL_MISMATCH_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + MAR_CHANNEL_MISMATCH_ERROR;
const STATE_FAILED_VERSION_DOWNGRADE_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + VERSION_DOWNGRADE_ERROR;
const STATE_FAILED_SERVICE_COULD_NOT_COPY_UPDATER =
  STATE_FAILED + STATE_FAILED_DELIMETER + SERVICE_COULD_NOT_COPY_UPDATER
const STATE_FAILED_SERVICE_INVALID_APPLYTO_DIR_STAGED_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + SERVICE_INVALID_APPLYTO_DIR_STAGED_ERROR;
const STATE_FAILED_SERVICE_INVALID_APPLYTO_DIR_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + SERVICE_INVALID_APPLYTO_DIR_ERROR;
const STATE_FAILED_SERVICE_INVALID_INSTALL_DIR_PATH_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + SERVICE_INVALID_INSTALL_DIR_PATH_ERROR;
const STATE_FAILED_SERVICE_INVALID_WORKING_DIR_PATH_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + SERVICE_INVALID_WORKING_DIR_PATH_ERROR;
const STATE_FAILED_INVALID_APPLYTO_DIR_STAGED_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + INVALID_APPLYTO_DIR_STAGED_ERROR;
const STATE_FAILED_INVALID_APPLYTO_DIR_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + INVALID_APPLYTO_DIR_ERROR;
const STATE_FAILED_INVALID_INSTALL_DIR_PATH_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + INVALID_INSTALL_DIR_PATH_ERROR;
const STATE_FAILED_INVALID_WORKING_DIR_PATH_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + INVALID_WORKING_DIR_PATH_ERROR;
const STATE_FAILED_INVALID_CALLBACK_PATH_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + INVALID_CALLBACK_PATH_ERROR;
const STATE_FAILED_INVALID_CALLBACK_DIR_ERROR =
  STATE_FAILED + STATE_FAILED_DELIMETER + INVALID_CALLBACK_DIR_ERROR;

const DEFAULT_UPDATE_VERSION = "999999.0";

/**
 * Constructs a string representing a remote update xml file.
 *
 * @param  aUpdates
 *         The string representing the update elements.
 * @return The string representing a remote update xml file.
 */
function getRemoteUpdatesXMLString(aUpdates) {
  return "<?xml version=\"1.0\"?>" +
         "<updates>" +
         aUpdates +
         "</updates>";
}

/**
 * Constructs a string representing an update element for a remote update xml
 * file. See getUpdateString for parameter information not provided below.
 *
 * @param  aUpdateProps
 *         An object containing non default test values for an nsIUpdate.
 *         See updateProps names below for possible object names.
 * @param  aPatches
 *         String representing the application update patches.
 * @return The string representing an update element for an update xml file.
 */
function getRemoteUpdateString(aUpdateProps, aPatches) {
  const updateProps = {
    type: "major",
    name: "App Update Test",
    displayVersion: null,
    appVersion: DEFAULT_UPDATE_VERSION,
    buildID: "20080811053724",
    detailsURL: URL_HTTP_UPDATE_SJS + "?uiURL=DETAILS",
    promptWaitTime: null,
    backgroundInterval: null,
    custom1: null,
    custom2: null
  };

  for (let name in aUpdateProps) {
    updateProps[name] = aUpdateProps[name];
  }

  return getUpdateString(updateProps) + ">" +
         aPatches +
         "</update>";
}

/**
 * Constructs a string representing a patch element for a remote update xml
 * file. See getPatchString for parameter information not provided below.
 *
 * @param  aPatchProps (optional)
 *         An object containing non default test values for an nsIUpdatePatch.
 *         See patchProps below for possible object names.
 * @return The string representing a patch element for a remote update xml file.
 */
function getRemotePatchString(aPatchProps) {
  const patchProps = {
    type: "complete",
    _url: null,
    get url() {
      if (this._url) {
        return this._url;
      }
      if (gURLData) {
        return gURLData + FILE_SIMPLE_MAR;
      }
      return null;
    },
    set url(val) {
      this._url = val;
    },
    hashFunction: "MD5",
    hashValue: MD5_HASH_SIMPLE_MAR,
    size: SIZE_SIMPLE_MAR
  };

  for (let name in aPatchProps) {
    patchProps[name] = aPatchProps[name];
  }

  return getPatchString(patchProps) + "/>";
}

/**
 * Constructs a string representing a local update xml file.
 *
 * @param  aUpdates
 *         The string representing the update elements.
 * @return The string representing a local update xml file.
 */
function getLocalUpdatesXMLString(aUpdates) {
  if (!aUpdates || aUpdates == "") {
    return "<updates xmlns=\"http://www.mozilla.org/2005/app-update\"/>";
  }
  return ("<updates xmlns=\"http://www.mozilla.org/2005/app-update\">" +
          aUpdates +
          "</updates>");
}

/**
 * Constructs a string representing an update element for a local update xml
 * file. See getUpdateString for parameter information not provided below.
 *
 * @param  aUpdateProps
 *         An object containing non default test values for an nsIUpdate.
 *         See updateProps names below for possible object names.
 * @param  aPatches
 *         String representing the application update patches.
 * @return The string representing an update element for an update xml file.
 */
function getLocalUpdateString(aUpdateProps, aPatches) {
  const updateProps = {
    type: "major",
    name: "App Update Test",
    displayVersion: null,
    _appVersion: null,
    get appVersion() {
      if (this._appVersion) {
        return this._appVersion;
      }
      if (Services && Services.appinfo && Services.appinfo.version) {
        return Services.appinfo.version;
      }
      return DEFAULT_UPDATE_VERSION;
    },
    set appVersion(val) {
      this._appVersion = val;
    },
    buildID: "20080811053724",
    detailsURL: URL_HTTP_UPDATE_SJS + "?uiURL=DETAILS",
    promptWaitTime: null,
    backgroundInterval: null,
    custom1: null,
    custom2: null,
    serviceURL: "http://test_service/",
    installDate: "1238441400314",
    statusText: "Install Pending",
    isCompleteUpdate: "true",
    channel: gDefaultPrefBranch.getCharPref(PREF_APP_UPDATE_CHANNEL),
    foregroundDownload: "true",
    previousAppVersion: null
  };

  for (let name in aUpdateProps) {
    updateProps[name] = aUpdateProps[name];
  }

  let previousAppVersion = updateProps.previousAppVersion ?
    "previousAppVersion=\"" + updateProps.previousAppVersion + "\" " : "";
  let serviceURL = "serviceURL=\"" + updateProps.serviceURL + "\" ";
  let installDate = "installDate=\"" + updateProps.installDate + "\" ";
  let statusText = updateProps.statusText ?
    "statusText=\"" + updateProps.statusText + "\" " : "";
  let isCompleteUpdate =
    "isCompleteUpdate=\"" + updateProps.isCompleteUpdate + "\" ";
  let channel = "channel=\"" + updateProps.channel + "\" ";
  let foregroundDownload = updateProps.foregroundDownload ?
    "foregroundDownload=\"" + updateProps.foregroundDownload + "\">" : ">";

  return getUpdateString(updateProps) +
         " " +
         previousAppVersion +
         serviceURL +
         installDate +
         statusText +
         isCompleteUpdate +
         channel +
         foregroundDownload +
         aPatches +
         "</update>";
}

/**
 * Constructs a string representing a patch element for a local update xml file.
 * See getPatchString for parameter information not provided below.
 *
 * @param  aPatchProps (optional)
 *         An object containing non default test values for an nsIUpdatePatch.
 *         See patchProps below for possible object names.
 * @return The string representing a patch element for a local update xml file.
 */
function getLocalPatchString(aPatchProps) {
  const patchProps = {
    type: "complete",
    url: gURLData + FILE_SIMPLE_MAR,
    hashFunction: "MD5",
    hashValue: MD5_HASH_SIMPLE_MAR,
    size: SIZE_SIMPLE_MAR,
    selected: "true",
    state: STATE_SUCCEEDED
  };

  for (let name in aPatchProps) {
    patchProps[name] = aPatchProps[name];
  }

  let selected = "selected=\"" + patchProps.selected + "\" ";
  let state = "state=\"" + patchProps.state + "\"/>";
  return getPatchString(patchProps) + " " +
         selected +
         state;
}

/**
 * Constructs a string representing an update element for a remote update xml
 * file.
 *
 * @param  aUpdateProps (optional)
 *         An object containing non default test values for an nsIUpdate.
 *         See the aUpdateProps property names below for possible object names.
 * @return The string representing an update element for an update xml file.
 */
function getUpdateString(aUpdateProps) {
  let type = "type=\"" + aUpdateProps.type + "\" ";
  let name = "name=\"" + aUpdateProps.name + "\" ";
  let displayVersion = aUpdateProps.displayVersion ?
    "displayVersion=\"" + aUpdateProps.displayVersion + "\" " : "";
  let appVersion = "appVersion=\"" + aUpdateProps.appVersion + "\" ";
  // Not specifying a detailsURL will cause a leak due to bug 470244
  let detailsURL = "detailsURL=\"" + aUpdateProps.detailsURL + "\" ";
  let promptWaitTime = aUpdateProps.promptWaitTime ?
    "promptWaitTime=\"" + aUpdateProps.promptWaitTime + "\" " : "";
  let backgroundInterval = aUpdateProps.backgroundInterval ?
    "backgroundInterval=\"" + aUpdateProps.backgroundInterval + "\" " : "";
  let custom1 = aUpdateProps.custom1 ? aUpdateProps.custom1 + " " : "";
  let custom2 = aUpdateProps.custom2 ? aUpdateProps.custom2 + " " : "";
  let buildID = "buildID=\"" + aUpdateProps.buildID + "\"";

  return "  <update " + type +
                        name +
                        displayVersion +
                        appVersion +
                        detailsURL +
                        promptWaitTime +
                        backgroundInterval +
                        custom1 +
                        custom2 +
                        buildID;
}

/**
 * Constructs a string representing a patch element for an update xml file.
 *
 * @param  aPatchProps (optional)
 *         An object containing non default test values for an nsIUpdatePatch.
 *         See the patchProps property names below for possible object names.
 * @return The string representing a patch element for an update xml file.
 */
function getPatchString(aPatchProps) {
  let type = "type=\"" + aPatchProps.type + "\" ";
  let url = "URL=\"" + aPatchProps.url + "\" ";
  let hashFunction = "hashFunction=\"" + aPatchProps.hashFunction + "\" ";
  let hashValue = "hashValue=\"" + aPatchProps.hashValue + "\" ";
  let size = "size=\"" + aPatchProps.size + "\"";
  return "<patch " +
         type +
         url +
         hashFunction +
         hashValue +
         size;
}
