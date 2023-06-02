// Appease eslint.
/* import-globals-from ../head_addons.js */

var { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

// Initializing and asserting the expected telemetry is currently conditioned
// on this const.
// TODO(Bug 1752139) remove this along with initializing and asserting the expected
// telemetry also for android build, once `Services.fog.testResetFOG()` is implemented
// for Android builds.
const IS_ANDROID_BUILD = AppConstants.platform === "android";
const IS_FOG_RESET_SUPPORTED = !IS_ANDROID_BUILD;
const DUMMY_TIME = 939420000000; // new Date(1999, 9, 9)
const DUMMY_STRING = "GleanDummyString";
let gleanEventCount = 0;

function _resetMetric(gleanMetric) {
  let value = gleanMetric.testGetValue();
  if (value === undefined) {
    return; // Never initialized, nothing to reset.
  }
  if (gleanMetric instanceof Ci.nsIGleanDatetime) {
    gleanMetric.set(DUMMY_TIME * 1000);
  } else if (gleanMetric instanceof Ci.nsIGleanString) {
    gleanMetric.set(DUMMY_STRING);
  } else if (gleanMetric instanceof Ci.nsIGleanEvent) {
    // NOTE: this doesn't work when there is more than one event;
    // for now we assume that there is only one: addonBlockChange.
    // Cannot overwrite, so just store the current list length.
    gleanEventCount = value.length;
  } else {
    throw new Error("Unsupported Glean metric type - cannot reset");
  }
}

function testGetValue(gleanMetric) {
  let value = gleanMetric.testGetValue();
  if (value === undefined || IS_FOG_RESET_SUPPORTED) {
    return value;
  }
  if (gleanMetric instanceof Ci.nsIGleanDatetime) {
    return value.getTime() === DUMMY_TIME ? undefined : value;
  }
  if (gleanMetric instanceof Ci.nsIGleanString) {
    return value === DUMMY_STRING ? undefined : value;
  }
  if (gleanMetric instanceof Ci.nsIGleanEvent) {
    // NOTE: this doesn't work when there is more than one event;
    // for now we assume that there is only one: addonBlockChange.
    value = value.slice(gleanEventCount);
    return value.length ? value : undefined;
  }
  throw new Error("Unsupported Glean metric type");
}

function resetBlocklistTelemetry() {
  if (IS_FOG_RESET_SUPPORTED) {
    Services.fog.testResetFOG();
    return;
  }
  // TODO bug 1752139: fix testResetFOG and remove workarounds.
  _resetMetric(Glean.blocklist.addonBlockChange);
  _resetMetric(Glean.blocklist.lastModifiedRsAddonsMblf);
  _resetMetric(Glean.blocklist.mlbfSource);
  _resetMetric(Glean.blocklist.mlbfGenerationTime);
  _resetMetric(Glean.blocklist.mlbfStashTimeOldest);
  _resetMetric(Glean.blocklist.mlbfStashTimeNewest);
}

const MLBF_RECORD = {
  id: "A blocklist entry that refers to a MLBF file",
  // Higher than any last_modified in addons-bloomfilters.json:
  last_modified: Date.now(),
  attachment: {
    size: 32,
    hash: "6af648a5d6ce6dbee99b0aab1780d24d204977a6606ad670d5372ef22fac1052",
    filename: "does-not-matter.bin",
  },
  attachment_type: "bloomfilter-base",
  generation_time: 1577833200000,
};

function enable_blocklist_v2_instead_of_useMLBF() {
  Blocklist.allowDeprecatedBlocklistV2 = true;
  Services.prefs.setBoolPref("extensions.blocklist.useMLBF", false);
  // Sanity check: blocklist v2 has been enabled.
  const { BlocklistPrivate } = ChromeUtils.importESModule(
    "resource://gre/modules/Blocklist.sys.mjs"
  );
  Assert.equal(
    Blocklist.ExtensionBlocklist,
    BlocklistPrivate.ExtensionBlocklistRS,
    "ExtensionBlocklistRS should have been enabled"
  );
}

async function load_mlbf_record_as_blob() {
  const url = Services.io.newFileURI(
    do_get_file("../data/mlbf-blocked1-unblocked2.bin")
  ).spec;
  Cu.importGlobalProperties(["fetch"]);
  return (await fetch(url)).blob();
}

function getExtensionBlocklistMLBF() {
  // ExtensionBlocklist.Blocklist is an ExtensionBlocklistMLBF if the useMLBF
  // pref is set to true.
  const {
    BlocklistPrivate: { ExtensionBlocklistMLBF },
  } = ChromeUtils.importESModule("resource://gre/modules/Blocklist.sys.mjs");
  if (Blocklist.allowDeprecatedBlocklistV2) {
    Assert.ok(
      Services.prefs.getBoolPref("extensions.blocklist.useMLBF", false),
      "blocklist.useMLBF should be true"
    );
  }
  return ExtensionBlocklistMLBF;
}
