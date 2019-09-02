// We've run once, and hopefully bypassed any network connections that add-ons
// might have tried to make after first install. Now let's uninstall ourselves
// so that subsequent starts run in online mode.

/* globals browser */
browser.management.uninstallSelf({});
