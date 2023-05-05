/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const modules = {
  root: {},
  "windowglobal-in-root": {},
  windowglobal: {},
};

const BASE_FOLDER =
  "chrome://mochitests/content/browser/remote/shared/messagehandler/test/browser/resources/modules";

// eslint-disable-next-line mozilla/lazy-getter-object-name
ChromeUtils.defineESModuleGetters(modules.root, {
  command: `${BASE_FOLDER}/root/command.sys.mjs`,
  event: `${BASE_FOLDER}/root/event.sys.mjs`,
  invalid: `${BASE_FOLDER}/root/invalid.sys.mjs`,
  rootOnly: `${BASE_FOLDER}/root/rootOnly.sys.mjs`,
  windowglobaltoroot: `${BASE_FOLDER}/root/windowglobaltoroot.sys.mjs`,
});

// eslint-disable-next-line mozilla/lazy-getter-object-name
ChromeUtils.defineESModuleGetters(modules["windowglobal-in-root"], {
  command: `${BASE_FOLDER}/windowglobal-in-root/command.sys.mjs`,
  event: `${BASE_FOLDER}/windowglobal-in-root/event.sys.mjs`,
});

// eslint-disable-next-line mozilla/lazy-getter-object-name
ChromeUtils.defineESModuleGetters(modules.windowglobal, {
  command: `${BASE_FOLDER}/windowglobal/command.sys.mjs`,
  commandwindowglobalonly: `${BASE_FOLDER}/windowglobal/commandwindowglobalonly.sys.mjs`,
  event: `${BASE_FOLDER}/windowglobal/event.sys.mjs`,
  eventemitter: `${BASE_FOLDER}/windowglobal/eventemitter.sys.mjs`,
  eventnointercept: `${BASE_FOLDER}/windowglobal/eventnointercept.sys.mjs`,
  eventonprefchange: `${BASE_FOLDER}/windowglobal/eventonprefchange.sys.mjs`,
  retry: `${BASE_FOLDER}/windowglobal/retry.sys.mjs`,
  sessiondataupdate: `${BASE_FOLDER}/windowglobal/sessiondataupdate.sys.mjs`,
  windowglobaltoroot: `${BASE_FOLDER}/windowglobal/windowglobaltoroot.sys.mjs`,
});
