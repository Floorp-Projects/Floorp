/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the download manager backend

do_get_profile();

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
var {
  HTTP_400,
  HTTP_401,
  HTTP_402,
  HTTP_403,
  HTTP_404,
  HTTP_405,
  HTTP_406,
  HTTP_407,
  HTTP_408,
  HTTP_409,
  HTTP_410,
  HTTP_411,
  HTTP_412,
  HTTP_413,
  HTTP_414,
  HTTP_415,
  HTTP_417,
  HTTP_500,
  HTTP_501,
  HTTP_502,
  HTTP_503,
  HTTP_504,
  HTTP_505,
  HttpError,
  HttpServer,
} = ChromeUtils.import("resource://testing-common/httpd.js");

// List types, this should sync with |enum LIST_TYPES| defined in PendingLookup.
var ALLOW_LIST = 0;
var BLOCK_LIST = 1;
var NO_LIST = 2;

// Allow or block reason, this should sync with |enum Reason| in ApplicationReputation.cpp
var NotSet = 0;
var LocalWhitelist = 1;
var LocalBlocklist = 2;
var NonBinaryFile = 3;
var VerdictSafe = 4;
var VerdictUnknown = 5;
var VerdictDangerous = 6;
var VerdictDangerousHost = 7;
var VerdictUnwanted = 8;
var VerdictUncommon = 9;
var VerdictUnrecognized = 10;
var DangerousPrefOff = 11;
var DangerousHostPrefOff = 12;
var UnwantedPrefOff = 13;
var UncommonPrefOff = 14;
var NetworkError = 15;
var RemoteLookupDisabled = 16;
var InternalError = 17;
var DPDisabled = 18;
var MAX_REASON = 19;

function createURI(aObj) {
  return aObj instanceof Ci.nsIFile
    ? Services.io.newFileURI(aObj)
    : Services.io.newURI(aObj);
}

function add_telemetry_count(telemetry, index, count) {
  let val = telemetry[index] || 0;
  telemetry[index] = val + count;
}

function check_telemetry(aExpectedTelemetry) {
  // SHOULD_BLOCK = true
  let shouldBlock = Services.telemetry
    .getHistogramById("APPLICATION_REPUTATION_SHOULD_BLOCK")
    .snapshot();
  let expectedShouldBlock = aExpectedTelemetry.shouldBlock;
  Assert.equal(shouldBlock.values[1] || 0, expectedShouldBlock);

  let local = Services.telemetry
    .getHistogramById("APPLICATION_REPUTATION_LOCAL")
    .snapshot();

  let expectedLocal = aExpectedTelemetry.local;
  Assert.equal(
    local.values[ALLOW_LIST] || 0,
    expectedLocal[ALLOW_LIST] || 0,
    "Allow list.values don't match"
  );
  Assert.equal(
    local.values[BLOCK_LIST] || 0,
    expectedLocal[BLOCK_LIST] || 0,
    "Block list.values don't match"
  );
  Assert.equal(
    local.values[NO_LIST] || 0,
    expectedLocal[NO_LIST] || 0,
    "No list.values don't match"
  );

  let reason = Services.telemetry
    .getHistogramById("APPLICATION_REPUTATION_REASON")
    .snapshot();
  let expectedReason = aExpectedTelemetry.reason;

  for (let i = 0; i < MAX_REASON; i++) {
    if ((reason.values[i] || 0) != (expectedReason[i] || 0)) {
      Assert.ok(false, "Allow or Block reason(" + i + ") doesn't match");
    }
  }
}

function get_telemetry_snapshot() {
  let local = Services.telemetry
    .getHistogramById("APPLICATION_REPUTATION_LOCAL")
    .snapshot();
  let shouldBlock = Services.telemetry
    .getHistogramById("APPLICATION_REPUTATION_SHOULD_BLOCK")
    .snapshot();
  let reason = Services.telemetry
    .getHistogramById("APPLICATION_REPUTATION_REASON")
    .snapshot();
  return {
    shouldBlock: shouldBlock.values[1] || 0,
    local: local.values,
    reason: reason.values,
  };
}
