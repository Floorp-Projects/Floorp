/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../../common/tests/unit/head_helpers.js */
/* import-globals-from ../../../common/tests/unit/head_http.js */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { SCOPE_OLD_SYNC, LEGACY_SCOPE_WEBEXT_SYNC } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsCommon.js"
);

// Some mock key data, in both scoped-key and legacy field formats.
const MOCK_ACCOUNT_KEYS = {
  scopedKeys: {
    [SCOPE_OLD_SYNC]: {
      kid: "1234567890123-u7u7u7u7u7u7u7u7u7u7uw",
      k:
        "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqg",
      kty: "oct",
    },
    [LEGACY_SCOPE_WEBEXT_SYNC]: {
      kid: "1234567890123-3d3d3d3d3d3d3d3d3d3d3d3d3d3d3d3d3d3d3d3d3d0",
      k:
        "zMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzA",
      kty: "oct",
    },
  },
  kSync:
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  kXCS: "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
  kExtSync:
    "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
  kExtKbHash:
    "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd ",
};

(function initFxAccountsTestingInfrastructure() {
  do_get_profile();

  let { initTestLogging } = ChromeUtils.import(
    "resource://testing-common/services/common/logging.js"
  );

  initTestLogging("Trace");
}.call(this));
