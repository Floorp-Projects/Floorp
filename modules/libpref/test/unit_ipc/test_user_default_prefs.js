const Ci = Components.interfaces;
const Cc = Components.classes;

let pb = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

// This pref is chosen somewhat arbitrarily --- we just need one
// that's guaranteed to have a default value.
const kPrefName = 'intl.accept_languages'; // of type char, which we
                                           // assume below
let initialValue = null;

function check_child_pref_info_eq(continuation) {
    sendCommand(
        'var pb = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);\n'+
        // Returns concatenation "[value],[isUser]"
        'pb.getCharPref("'+ kPrefName +'")+ "," +'+
        'pb.prefHasUserValue("'+ kPrefName +'");',
        function (info) {
            let [ value, isUser ] = info.split(',');
            do_check_eq(pb.getCharPref(kPrefName), value);
            do_check_eq(pb.prefHasUserValue(kPrefName), isUser == "true");
            continuation();
        });
}

function run_test() {
    // We finish in clean_up()
    do_test_pending();

    try {
        if (pb.getCharPref('dom.ipc.processPrelaunch.enabled')) {
            dump('WARNING: Content process may already have launched, so this test may not be meaningful.');
        }
    } catch(e) { }

    initialValue = pb.getCharPref(kPrefName);

    test_user_setting();
}

function test_user_setting() {
    // We rely on setting this before the content process starts up.
    // When it starts up, it should recognize this as a user pref, not
    // a default pref.
    pb.setCharPref(kPrefName, 'i-imaginarylanguage');
    // NB: processing of the value-change notification in the child
    // process triggered by the above set happens-before the remaining
    // code here
    check_child_pref_info_eq(function () {
            do_check_eq(pb.prefHasUserValue(kPrefName), true);

            test_cleared_is_default();
        });
}

function test_cleared_is_default() {
    pb.clearUserPref(kPrefName);
    // NB: processing of the value-change notification in the child
    // process triggered by the above set happens-before the remaining
    // code here
    check_child_pref_info_eq(function () {
            do_check_eq(pb.prefHasUserValue(kPrefName), false);

            clean_up();
        });
}

function clean_up() {
    pb.setCharPref(kPrefName, initialValue);
    // NB: processing of the value-change notification in the child
    // process triggered by the above set happens-before the remaining
    // code here
    check_child_pref_info_eq(function () {
            do_test_finished();
        });
}