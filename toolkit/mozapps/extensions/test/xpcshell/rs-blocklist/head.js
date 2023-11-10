// Appease eslint.
/* import-globals-from ../head_addons.js */

var { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const IS_ANDROID_BUILD = AppConstants.platform === "android";

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
