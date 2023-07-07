/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const modules = {
  root: {},
  "windowglobal-in-root": {},
  windowglobal: {},
};

// eslint-disable-next-line mozilla/lazy-getter-object-name
ChromeUtils.defineESModuleGetters(modules.root, {
  browser:
    "chrome://remote/content/webdriver-bidi/modules/root/browser.sys.mjs",
  browsingContext:
    "chrome://remote/content/webdriver-bidi/modules/root/browsingContext.sys.mjs",
  input: "chrome://remote/content/webdriver-bidi/modules/root/input.sys.mjs",
  log: "chrome://remote/content/webdriver-bidi/modules/root/log.sys.mjs",
  network:
    "chrome://remote/content/webdriver-bidi/modules/root/network.sys.mjs",
  script: "chrome://remote/content/webdriver-bidi/modules/root/script.sys.mjs",
  session:
    "chrome://remote/content/webdriver-bidi/modules/root/session.sys.mjs",
});

// eslint-disable-next-line mozilla/lazy-getter-object-name
ChromeUtils.defineESModuleGetters(modules["windowglobal-in-root"], {
  browsingContext:
    "chrome://remote/content/webdriver-bidi/modules/windowglobal-in-root/browsingContext.sys.mjs",
  log: "chrome://remote/content/webdriver-bidi/modules/windowglobal-in-root/log.sys.mjs",
  script:
    "chrome://remote/content/webdriver-bidi/modules/windowglobal-in-root/script.sys.mjs",
});

// eslint-disable-next-line mozilla/lazy-getter-object-name
ChromeUtils.defineESModuleGetters(modules.windowglobal, {
  browsingContext:
    "chrome://remote/content/webdriver-bidi/modules/windowglobal/browsingContext.sys.mjs",
  input:
    "chrome://remote/content/webdriver-bidi/modules/windowglobal/input.sys.mjs",
  log: "chrome://remote/content/webdriver-bidi/modules/windowglobal/log.sys.mjs",
  script:
    "chrome://remote/content/webdriver-bidi/modules/windowglobal/script.sys.mjs",
});
