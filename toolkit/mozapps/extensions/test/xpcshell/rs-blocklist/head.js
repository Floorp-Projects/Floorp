// Appease eslint.
/* import-globals-from ../head_addons.js */

const { ComponentUtils } = ChromeUtils.import(
  "resource://gre/modules/ComponentUtils.jsm"
);

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
  Assert.ok(
    !!Blocklist.ExtensionBlocklist._updateEntries,
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
  // An alternative way to obtain ExtensionBlocklistMLBF is by importing the
  // global of Blocklist.jsm and reading ExtensionBlocklistMLBF off it, but
  // to avoid using the deprecated ChromeUtils.import(.., null), bug 1524027
  // needs to be fixed first. So let's use Blocklist.ExtensionBlocklist.
  const ExtensionBlocklistMLBF = Blocklist.ExtensionBlocklist;
  Assert.ok(
    Services.prefs.getBoolPref("extensions.blocklist.useMLBF", false),
    "blocklist.useMLBF should be true"
  );
  return ExtensionBlocklistMLBF;
}
