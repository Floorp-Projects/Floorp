/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let EXPORTED_SYMBOLS = [ "Reflect" ];

/*
 * This is the js module for Reflect. Import it like so:
 *   Components.utils.import("resource://gre/modules/reflect.jsm");
 *
 * This will create a 'Reflect' object, which provides an interface to the
 * SpiderMonkey parser API.
 *
 * For documentation on the API, see:
 * https://developer.mozilla.org/en/SpiderMonkey/Parser_API
 *
 */


// Initialize the ctypes object. You do not need to do this yourself.
const init = Components.classes["@mozilla.org/jsreflect;1"].createInstance();
init();

