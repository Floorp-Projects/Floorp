/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/stringbundle.js");
Cu.import("resource://services-sync/util.js");

function run_test() {
    let fn = Utils.lazyStrings("sync");
    do_check_eq(typeof fn, "function");
    let bundle = fn();
    do_check_true(bundle instanceof StringBundle);
    let url = bundle.url;
    do_check_eq(url, "chrome://weave/locale/services/sync.properties");
}
