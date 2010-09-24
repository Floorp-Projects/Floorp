Cu.import("resource://services-sync/util.js");

function run_test() {
  let str = "Umlaute: \u00FC \u00E4\n"; // Umlaute: ü ä
  let encoded = Utils.encodeUTF8(str);
  let decoded = Utils.decodeUTF8(encoded);
  do_check_eq(decoded, str);
}
