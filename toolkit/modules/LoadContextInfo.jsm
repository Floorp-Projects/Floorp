/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This jsm is here only for compatibility.  Extension developers may use it
 * to build nsILoadContextInfo to pass down to HTTP cache APIs.  Originally
 * it was possible to implement nsILoadContextInfo in JS.  But now it turned
 * out to be a built-in class only, so we need a component (service) as
 * a factory to build nsILoadContextInfo in a JS code.
 */

this.EXPORTED_SYMBOLS = ["LoadContextInfo"];
this.LoadContextInfo = Components.classes["@mozilla.org/load-context-info-factory;1"]
                                 .getService(Components.interfaces.nsILoadContextInfoFactory);
