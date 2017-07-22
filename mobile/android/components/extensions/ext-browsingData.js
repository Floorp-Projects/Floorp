/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SharedPreferences",
                                  "resource://gre/modules/SharedPreferences.jsm");

this.browsingData = class extends ExtensionAPI {
  getAPI(context) {
    return {
      browsingData: {
        settings() {
          const PREF_DOMAIN = "android.not_a_preference.privacy.clear";
          const PREF_KEY_PREFIX = "private.data.";
          // The following prefs are the only ones in Firefox that match corresponding
          // values used by Chrome when returning settings.
          const PREF_LIST = ["cache", "history", "formdata", "cookies_sessions", "downloadFiles"];

          let dataTrue = SharedPreferences.forProfile().getSetPref(PREF_DOMAIN);
          let name;

          let dataToRemove = {};
          let dataRemovalPermitted = {};

          for (let item of PREF_LIST) {
            // The property formData needs a different case than the
            // formdata preference.
            switch(item){
              case "formdata":
                name = "formData";
                break;
              case "cookies_sessions":
                name = "cookies";
                break;
              case "downloadFiles":
                name = "downloads";
                break;
              default:
                name = item;
            }
            dataToRemove[name] = dataTrue.includes(`${PREF_KEY_PREFIX}${item}`);
            // Firefox doesn't have the same concept of dataRemovalPermitted
            // as Chrome, so it will always be true.
            dataRemovalPermitted[name] = true;
          }
          // We do not provide option to delete history by time
          // so, since value is given 0, which means Everything
          return Promise.resolve({options: {since: 0}, dataToRemove, dataRemovalPermitted});
        },
      },
    };
  }
};