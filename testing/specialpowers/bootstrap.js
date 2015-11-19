/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

var spObserver;

function startup(data, reason) {
  let observer = {};
  Components.utils.import("chrome://specialpowers/content/SpecialPowersObserver.jsm", observer);

  let registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  registrar.registerFactory(
    observer.SpecialPowersObserver.prototype.classID,
    "SpecialPowersObserver",
    observer.SpecialPowersObserver.prototype.contractID,
    observer.SpecialPowersObserverFactory
  );

  spObserver = new observer.SpecialPowersObserver();
  spObserver.init();
}

function shutdown(data, reason) {
  let observer = {};
  Components.utils.import("chrome://specialpowers/content/SpecialPowersObserver.jsm", observer);

  let registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  registrar.unregisterFactory(
    observer.SpecialPowersObserver.prototype.classID,
    observer.SpecialPowersObserverFactory
  );

  spObserver.uninit();
}

function install(data, reason) {}
function uninstall(data, reason) {}
