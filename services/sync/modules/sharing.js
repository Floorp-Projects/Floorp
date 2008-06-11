EXPORTED_SYMBOLS = ["Sharing"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

function Api(dav) {
  this._dav = dav;
}

Api.prototype = {
  shareWithUsers: function Api_shareWithUsers(path, users, onComplete) {
    this._shareGenerator.async(this,
                               onComplete,
                               path,
                               users);
  },

  _shareGenerator: function Api__shareGenerator(path, users) {
    let self = yield;

    this._dav.defaultPrefix = "";

    let cmd = {"version" : 1,
               "directory" : path,
               "share_to_users" : users};
    let jsonSvc = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
    let json = jsonSvc.encode(cmd);

    this._dav.POST("share/", "cmd=" + escape(json), self.cb);
    let xhr = yield;

    let retval;

    if (xhr.status == 200) {
      if (xhr.responseText == "OK") {
        retval = {wasSuccessful: true};
      } else {
        retval = {wasSuccessful: false,
                  errorText: xhr.responseText};
      }
    } else {
      retval = {wasSuccessful: false,
                errorText: "Server returned status " + xhr.status};
    }

    self.done(retval);
  }
};

Sharing = {
  Api: Api
};
