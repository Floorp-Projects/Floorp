/* globals browser */

/**
 * The TPS test is a Pageloader test, meaning that the tps.manifest file
 * tells Talos to load a particular page. The loading of that page signals
 * the start of the test. It's also where results need to go, as the
 * Talos gunk augments the loaded page with a special tpRecordTime
 * function that is used to report results.
 */

let processScriptPath = "content/tabswitch-content-process.js";

browser.tps.setup({ processScriptPath });
