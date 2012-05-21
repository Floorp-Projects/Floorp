/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let EXPORTED_SYMBOLS = [ "ctypes" ];

/*
 * This is the js module for ctypes. Import it like so:
 *   Components.utils.import("resource://gre/modules/ctypes.jsm");
 *
 * This will create a 'ctypes' object, which provides an interface to describe
 * and instantiate C types and call C functions from a dynamic library.
 *
 * For documentation on the API, see:
 * https://developer.mozilla.org/en/js-ctypes/js-ctypes_reference
 *
 */

// Initialize the ctypes object. You do not need to do this yourself.
const init = Components.classes["@mozilla.org/jsctypes;1"].createInstance();
init();

