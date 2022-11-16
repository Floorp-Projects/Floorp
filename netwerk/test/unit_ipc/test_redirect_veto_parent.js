//
// Run test script in content process instead of chrome (xpcshell's default)
//
//

let ChannelEventSink2 = {
  _classDescription: "WebRequest channel event sink",
  _classID: Components.ID("115062f8-92f1-11e5-8b7f-08001110f7ec"),
  _contractID: "@mozilla.org/webrequest/channel-event-sink;1",

  QueryInterface: ChromeUtils.generateQI(["nsIChannelEventSink", "nsIFactory"]),

  init() {
    Components.manager
      .QueryInterface(Ci.nsIComponentRegistrar)
      .registerFactory(
        this._classID,
        this._classDescription,
        this._contractID,
        this
      );
  },

  register() {
    Services.catMan.addCategoryEntry(
      "net-channel-event-sinks",
      this._contractID,
      this._contractID,
      false,
      true
    );
  },

  unregister() {
    Components.manager
      .QueryInterface(Ci.nsIComponentRegistrar)
      .unregisterFactory(this._classID, ChannelEventSink2);
    Services.catMan.deleteCategoryEntry(
      "net-channel-event-sinks",
      this._contractID,
      false
    );
  },

  // nsIChannelEventSink implementation
  asyncOnChannelRedirect(oldChannel, newChannel, flags, redirectCallback) {
    // Abort the redirection
    redirectCallback.onRedirectVerifyCallback(Cr.NS_ERROR_NO_INTERFACE);
  },

  // nsIFactory implementation
  createInstance(iid) {
    return this.QueryInterface(iid);
  },
};

add_task(async function run_test() {
  ChannelEventSink2.init();
  ChannelEventSink2.register();

  run_test_in_child("child_veto_in_parent.js");
  await do_await_remote_message("child-test-done");
  ChannelEventSink2.unregister();
});
