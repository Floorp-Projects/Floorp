"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionData",
                                  "resource://gre/modules/Extension.jsm");

var ExtensionPermissions = {
  // id -> object containing update details (see applyUpdate() )
  updates: new Map(),

  // Prepare the strings needed for a permission notification.
  _prepareStrings(info) {
    let appName = Strings.brand.GetStringFromName("brandShortName");
    let info2 = Object.assign({appName}, info);
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
        let stringInfo = Object.assign({addonName: info.addon.name}, info);
        let details = this._prepareStrings(stringInfo);
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
        let info = subject.wrappedJSObject;
        let {addon, resolve, reject} = info;
        let stringInfo = Object.assign({
          type: "update",
          addonName: addon.name,
        }, info);

        let details = this._prepareStrings(stringInfo);

        // If there are no promptable permissions, just apply the update
        if (details.message.length == 0) {
          resolve();
          return;
        }

        // Store all the details about the update until the user chooses to
        // look at update, at which point we will pick up in this.applyUpdate()
        details.icon = this._prepareIcon(addon.iconURL || "dummy.svg");

        let first = (this.updates.size == 0);
        this.updates.set(addon.id, {details, resolve, reject});

        if (first) {
          EventDispatcher.instance.sendRequest({
            type: "Extension:ShowUpdateIcon",
            value: true,
          });
        }
        break;

      case "webextension-optional-permission-prompt": {
        let info = subject.wrappedJSObject;
        let {name, resolve} = info;
        let stringInfo = Object.assign({
          type: "optional",
          addonName: name,
        }, info);

        let details = this._prepareStrings(stringInfo);

        // If there are no promptable permissions, just apply the update
        if (details.message.length == 0) {
          resolve(true);
          return;
        }

        // Store all the details about the update until the user chooses to
        // look at update, at which point we will pick up in this.applyUpdate()
        details.icon = this._prepareIcon(info.icon || "dummy.svg");

        details.type = "Extension:PermissionPrompt";
        let accepted = await EventDispatcher.instance.sendRequestForResult(details);
        resolve(accepted);
      }
    }
  },

  async applyUpdate(id) {
    if (!this.updates.has(id)) {
      return;
    }

    let update = this.updates.get(id);
    this.updates.delete(id);
    if (this.updates.size == 0) {
      EventDispatcher.instance.sendRequest({
        type: "Extension:ShowUpdateIcon",
        value: false,
      });
    }

    let {details} = update;
    details.type = "Extension:PermissionPrompt";

    let accepted = await EventDispatcher.instance.sendRequestForResult(details);
    if (accepted) {
      update.resolve();
    } else {
      update.reject();
    }
  },
};
