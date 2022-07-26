/* eslint-env mozilla/chrome-script */

var permChangedObs = {
  observe(subject, topic, data) {
    if (topic == "perm-changed") {
      var permission = subject.QueryInterface(Ci.nsIPermission);
      var msg = { op: data, type: permission.type };
      sendAsyncMessage("perm-changed", msg);
    }
  },
};

Services.obs.addObserver(permChangedObs, "perm-changed");
