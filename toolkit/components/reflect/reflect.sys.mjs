/* -*-  indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This is the js module for Reflect. Import it like so:
 *   const { Reflect } = ChromeUtils.importESModule(
 *     "resource://gre/modules/reflect.sys.mjs"
 *   );
 *
 * This will create a 'Reflect' object, which provides an interface to the
 * SpiderMonkey parser API.
 */

// Initialize the Reflect object. You do not need to do this yourself.
const init = Cc["@mozilla.org/jsreflect;1"].createInstance();
init();

// Reflect is a standard built-in defined on the shared global.
export const Reflect = globalThis.Reflect;
