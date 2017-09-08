"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionData",
                                  "resource://gre/modules/Extension.jsm");

var ExtensionPermissions = {
  // Prepare the strings needed for a permission notification.
  _prepareStrings(info) {
    let appName = Strings.brand.GetStringFromName("brandShortName");
    let info2 = Object.assign({appName, addonName: info.addon.name}, info);
    let strings = ExtensionData.formatPermissionStrings(info2, Strings.browser);

    // We dump the main body of the dialog into a big android
    // TextView.  Build a big string with the full contents here.
    let message = "";
    if (strings.msgs.length > 0) {
      message = [strings.listIntro, ...strings.msgs.map(s => `\u2022 ${s}`)].join("\n");
    }

    return {
      header: strings.header || strings.text,
      message,
      acceptText: strings.acceptText,
      cancelText: strings.cancelText,
    };
  },

  // Prepare an icon for a permission notification
  _prepareIcon(iconURL) {
    // We can render pngs with ResourceDrawableUtils
    if (iconURL.endsWith(".png")) {
      return iconURL;
    }

    // If we can't render an icon, show the default
    return "DEFAULT";
  },

  async observe(subject, topic, data) {
    switch (topic) {
      case "webextension-permission-prompt": {
        let {target, info} = subject.wrappedJSObject;

        let details = this._prepareStrings(info);
        details.icon = this._prepareIcon(info.icon);
        details.type = "Extension:PermissionPrompt";
        let accepted = await EventDispatcher.instance.sendRequestForResult(details);

        if (accepted) {
          info.resolve();
        } else {
          info.reject();
        }
        break;
      }

      case "webextension-update-permissions":
        // To be implemented in bug 1391579, just auto-approve until then
        subject.wrappedJSObject.resolve();
        break;

      case "webextension-optional-permission-prompt":
        // To be implemented in bug 1392176, just auto-approve until then
        subject.wrappedJSObject.resolve(true);
        break;
    }
  },
};
