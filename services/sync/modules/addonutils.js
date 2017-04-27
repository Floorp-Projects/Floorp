/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["AddonUtils"];

var {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/util.js");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonRepository",
  "resource://gre/modules/addons/AddonRepository.jsm");

function AddonUtilsInternal() {
  this._log = Log.repository.getLogger("Sync.AddonUtils");
  this._log.Level = Log.Level[Svc.Prefs.get("log.logger.addonutils")];
}
AddonUtilsInternal.prototype = {
  /**
   * Obtain an AddonInstall object from an AddonSearchResult instance.
   *
   * The callback will be invoked with the result of the operation. The
   * callback receives 2 arguments, error and result. Error will be falsy
   * on success or some kind of error value otherwise. The result argument
   * will be an AddonInstall on success or null on failure. It is possible
   * for the error to be falsy but result to be null. This could happen if
   * an install was not found.
   *
   * @param addon
   *        AddonSearchResult to obtain install from.
   * @param cb
   *        Function to be called with result of operation.
   */
  getInstallFromSearchResult:
    function getInstallFromSearchResult(addon, cb) {

    this._log.debug("Obtaining install for " + addon.id);

    // We should theoretically be able to obtain (and use) addon.install if
    // it is available. However, the addon.sourceURI rewriting won't be
    // reflected in the AddonInstall, so we can't use it. If we ever get rid
    // of sourceURI rewriting, we can avoid having to reconstruct the
    // AddonInstall.
    AddonManager.getInstallForURL(
      addon.sourceURI.spec,
      function handleInstall(install) {
        cb(null, install);
      },
      "application/x-xpinstall",
      undefined,
      addon.name,
      addon.iconURL,
      addon.version
    );
  },

  /**
   * Installs an add-on from an AddonSearchResult instance.
   *
   * The options argument defines extra options to control the install.
   * Recognized keys in this map are:
   *
   *   syncGUID - Sync GUID to use for the new add-on.
   *   enabled - Boolean indicating whether the add-on should be enabled upon
   *             install.
   *
   * When complete it calls a callback with 2 arguments, error and result.
   *
   * If error is falsy, result is an object. If error is truthy, result is
   * null.
   *
   * The result object has the following keys:
   *
   *   id      ID of add-on that was installed.
   *   install AddonInstall that was installed.
   *   addon   Addon that was installed.
   *
   * @param addon
   *        AddonSearchResult to install add-on from.
   * @param options
   *        Object with additional metadata describing how to install add-on.
   * @param cb
   *        Function to be invoked with result of operation.
   */
  installAddonFromSearchResult:
    function installAddonFromSearchResult(addon, options, cb) {
    this._log.info("Trying to install add-on from search result: " + addon.id);

    this.getInstallFromSearchResult(addon, (error, install) => {
      if (error) {
        cb(error, null);
        return;
      }

      if (!install) {
        cb(new Error("AddonInstall not available: " + addon.id), null);
        return;
      }

      try {
        this._log.info("Installing " + addon.id);
        let log = this._log;

        let listener = {
          onInstallStarted: function onInstallStarted(install) {
            if (!options) {
              return;
            }

            if (options.syncGUID) {
              log.info("Setting syncGUID of " + install.name + ": " +
                       options.syncGUID);
              install.addon.syncGUID = options.syncGUID;
            }

            // We only need to change userDisabled if it is disabled because
            // enabled is the default.
            if ("enabled" in options && !options.enabled) {
              log.info("Marking add-on as disabled for install: " +
                       install.name);
              install.addon.userDisabled = true;
            }
          },
          onInstallEnded(install, addon) {
            install.removeListener(listener);

            cb(null, {id: addon.id, install, addon});
          },
          onInstallFailed(install) {
            install.removeListener(listener);

            cb(new Error("Install failed: " + install.error), null);
          },
          onDownloadFailed(install) {
            install.removeListener(listener);

            cb(new Error("Download failed: " + install.error), null);
          }
        };
        install.addListener(listener);
        install.install();
      } catch (ex) {
        this._log.error("Error installing add-on", ex);
        cb(ex, null);
      }
    });
  },

  /**
   * Uninstalls the Addon instance and invoke a callback when it is done.
   *
   * @param addon
   *        Addon instance to uninstall.
   * @param cb
   *        Function to be invoked when uninstall has finished. It receives a
   *        truthy value signifying error and the add-on which was uninstalled.
   */
  uninstallAddon: function uninstallAddon(addon, cb) {
    let listener = {
      onUninstalling(uninstalling, needsRestart) {
        if (addon.id != uninstalling.id) {
          return;
        }

        // We assume restartless add-ons will send the onUninstalled event
        // soon.
        if (!needsRestart) {
          return;
        }

        // For non-restartless add-ons, we issue the callback on uninstalling
        // because we will likely never see the uninstalled event.
        AddonManager.removeAddonListener(listener);
        cb(null, addon);
      },
      onUninstalled(uninstalled) {
        if (addon.id != uninstalled.id) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        cb(null, addon);
      }
    };
    AddonManager.addAddonListener(listener);
    addon.uninstall();
  },

  /**
   * Installs multiple add-ons specified by metadata.
   *
   * The first argument is an array of objects. Each object must have the
   * following keys:
   *
   *   id - public ID of the add-on to install.
   *   syncGUID - syncGUID for new add-on.
   *   enabled - boolean indicating whether the add-on should be enabled.
   *   requireSecureURI - Boolean indicating whether to require a secure
   *     URI when installing from a remote location. This defaults to
   *     true.
   *
   * The callback will be called when activity on all add-ons is complete. The
   * callback receives 2 arguments, error and result.
   *
   * If error is truthy, it contains a string describing the overall error.
   *
   * The 2nd argument to the callback is always an object with details on the
   * overall execution state. It contains the following keys:
   *
   *   installedIDs  Array of add-on IDs that were installed.
   *   installs      Array of AddonInstall instances that were installed.
   *   addons        Array of Addon instances that were installed.
   *   errors        Array of errors encountered. Only has elements if error is
   *                 truthy.
   *
   * @param installs
   *        Array of objects describing add-ons to install.
   * @param cb
   *        Function to be called when all actions are complete.
   */
  installAddons: function installAddons(installs, cb) {
    if (!cb) {
      throw new Error("Invalid argument: cb is not defined.");
    }

    let ids = [];
    for (let addon of installs) {
      ids.push(addon.id);
    }

    AddonRepository.getAddonsByIDs(ids, {
      searchSucceeded: (addons, addonsLength, total) => {
        this._log.info("Found " + addonsLength + "/" + ids.length +
                       " add-ons during repository search.");

        let ourResult = {
          installedIDs: [],
          installs:     [],
          addons:       [],
          skipped:      [],
          errors:       []
        };

        if (!addonsLength) {
          cb(null, ourResult);
          return;
        }

        let expectedInstallCount = 0;
        let finishedCount = 0;
        let installCallback = function installCallback(error, result) {
          finishedCount++;

          if (error) {
            ourResult.errors.push(error);
          } else {
            ourResult.installedIDs.push(result.id);
            ourResult.installs.push(result.install);
            ourResult.addons.push(result.addon);
          }

          if (finishedCount >= expectedInstallCount) {
            if (ourResult.errors.length > 0) {
              cb(new Error("1 or more add-ons failed to install"), ourResult);
            } else {
              cb(null, ourResult);
            }
          }
        };

        let toInstall = [];

        // Rewrite the "src" query string parameter of the source URI to note
        // that the add-on was installed by Sync and not something else so
        // server-side metrics aren't skewed (bug 708134). The server should
        // ideally send proper URLs, but this solution was deemed too
        // complicated at the time the functionality was implemented.
        for (let addon of addons) {
          // Find the specified options for this addon.
          let options;
          for (let install of installs) {
            if (install.id == addon.id) {
              options = install;
              break;
            }
          }
          if (!this.canInstallAddon(addon, options)) {
            ourResult.skipped.push(addon.id);
            continue;
          }

          // We can go ahead and attempt to install it.
          toInstall.push(addon);

          // We should always be able to QI the nsIURI to nsIURL. If not, we
          // still try to install the add-on, but we don't rewrite the URL,
          // potentially skewing metrics.
          try {
            addon.sourceURI.QueryInterface(Ci.nsIURL);
          } catch (ex) {
            this._log.warn("Unable to QI sourceURI to nsIURL: " +
                           addon.sourceURI.spec);
            continue;
          }

          let params = addon.sourceURI.query.split("&").map(
            function rewrite(param) {

            if (param.indexOf("src=") == 0) {
              return "src=sync";
            }
            return param;
          });

          addon.sourceURI.query = params.join("&");
        }

        expectedInstallCount = toInstall.length;

        if (!expectedInstallCount) {
          cb(null, ourResult);
          return;
        }

        // Start all the installs asynchronously. They will report back to us
        // as they finish, eventually triggering the global callback.
        for (let addon of toInstall) {
          let options = {};
          for (let install of installs) {
            if (install.id == addon.id) {
              options = install;
              break;
            }
          }

          this.installAddonFromSearchResult(addon, options, installCallback);
        }

      },

      searchFailed: function searchFailed() {
        cb(new Error("AddonRepository search failed"), null);
      },
    });
  },

  /**
   * Returns true if we are able to install the specified addon, false
   * otherwise. It is expected that this will log the reason if it returns
   * false.
   *
   * @param addon
   *        (Addon) Add-on instance to check.
   * @param options
   *        (object) The options specified for this addon. See installAddons()
   *        for the valid elements.
   */
  canInstallAddon(addon, options) {
    // sourceURI presence isn't enforced by AddonRepository. So, we skip
    // add-ons without a sourceURI.
    if (!addon.sourceURI) {
      this._log.info("Skipping install of add-on because missing " +
                     "sourceURI: " + addon.id);
      return false;
    }
    // Verify that the source URI uses TLS. We don't allow installs from
    // insecure sources for security reasons. The Addon Manager ensures
    // that cert validation etc is performed.
    // (We should also consider just dropping this entirely and calling
    // XPIProvider.isInstallAllowed, but that has additional semantics we might
    // need to think through...)
    let requireSecureURI = true;
    if (options && options.requireSecureURI !== undefined) {
      requireSecureURI = options.requireSecureURI;
    }

    if (requireSecureURI) {
      let scheme = addon.sourceURI.scheme;
      if (scheme != "https") {
        this._log.info(`Skipping install of add-on "${addon.id}" because sourceURI's scheme of "${scheme}" is not trusted`);
        return false;
      }
    }
    this._log.info(`Add-on "${addon.id}" is able to be installed`);
    return true;
  },


  /**
   * Update the user disabled flag for an add-on.
   *
   * The supplied callback will be called when the operation is
   * complete. If the new flag matches the existing or if the add-on
   * isn't currently active, the function will fire the callback
   * immediately. Else, the callback is invoked when the AddonManager
   * reports the change has taken effect or has been registered.
   *
   * The callback receives as arguments:
   *
   *   (Error) Encountered error during operation or null on success.
   *   (Addon) The add-on instance being operated on.
   *
   * @param addon
   *        (Addon) Add-on instance to operate on.
   * @param value
   *        (bool) New value for add-on's userDisabled property.
   * @param cb
   *        (function) Callback to be invoked on completion.
   */
  updateUserDisabled: function updateUserDisabled(addon, value, cb) {
    if (addon.userDisabled == value) {
      cb(null, addon);
      return;
    }

    let listener = {
      onEnabling: (wrapper, needsRestart) => {
        this._log.debug("onEnabling: " + wrapper.id);
        if (wrapper.id != addon.id) {
          return;
        }

        // We ignore the restartless case because we'll get onEnabled shortly.
        if (!needsRestart) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        cb(null, wrapper);
      },

      onEnabled: wrapper => {
        this._log.debug("onEnabled: " + wrapper.id);
        if (wrapper.id != addon.id) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        cb(null, wrapper);
      },

      onDisabling: (wrapper, needsRestart) => {
        this._log.debug("onDisabling: " + wrapper.id);
        if (wrapper.id != addon.id) {
          return;
        }

        if (!needsRestart) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        cb(null, wrapper);
      },

      onDisabled: wrapper => {
        this._log.debug("onDisabled: " + wrapper.id);
        if (wrapper.id != addon.id) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        cb(null, wrapper);
      },

      onOperationCancelled: wrapper => {
        this._log.debug("onOperationCancelled: " + wrapper.id);
        if (wrapper.id != addon.id) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        cb(new Error("Operation cancelled"), wrapper);
      }
    };

    // The add-on listeners are only fired if the add-on is active. If not, the
    // change is silently updated and made active when/if the add-on is active.

    if (!addon.appDisabled) {
      AddonManager.addAddonListener(listener);
    }

    this._log.info("Updating userDisabled flag: " + addon.id + " -> " + value);
    addon.userDisabled = !!value;

    if (!addon.appDisabled) {
      cb(null, addon);
    }
    // Else the listener will handle invoking the callback.
  },

};

XPCOMUtils.defineLazyGetter(this, "AddonUtils", function() {
  return new AddonUtilsInternal();
});
