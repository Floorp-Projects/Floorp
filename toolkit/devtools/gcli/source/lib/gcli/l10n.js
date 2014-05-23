/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';

var Cc = require('chrome').Cc;
var Ci = require('chrome').Ci;
var Cu = require('chrome').Cu;

var prefSvc = Cc['@mozilla.org/preferences-service;1']
                        .getService(Ci.nsIPrefService);
var prefBranch = prefSvc.getBranch(null).QueryInterface(Ci.nsIPrefBranch2);

var Services = Cu.import('resource://gre/modules/Services.jsm', {}).Services;
var stringBundle = Services.strings.createBundle(
        'chrome://browser/locale/devtools/gclicommands.properties');

/**
 * Lookup a string in the GCLI string bundle
 */
exports.lookup = function(name) {
  try {
    return stringBundle.GetStringFromName(name);
  }
  catch (ex) {
    throw new Error('Failure in lookup(\'' + name + '\')');
  }
};

/**
 * An alternative to lookup().
 * <code>l10n.lookup('BLAH') === l10n.propertyLookup.BLAH</code>
 * This is particularly nice for templates because you can pass
 * <code>l10n:l10n.propertyLookup</code> in the template data and use it
 * like <code>${l10n.BLAH}</code>
 */
exports.propertyLookup = Proxy.create({
  get: function(rcvr, name) {
    return exports.lookup(name);
  }
});

/**
 * Lookup a string in the GCLI string bundle
 */
exports.lookupFormat = function(name, swaps) {
  try {
    return stringBundle.formatStringFromName(name, swaps, swaps.length);
  }
  catch (ex) {
    throw new Error('Failure in lookupFormat(\'' + name + '\')');
  }
};

/**
 * Allow GCLI users to be hidden by the 'devtools.chrome.enabled' pref.
 * Use it in commands like this:
 * <pre>
 *   name: "somecommand",
 *   hidden: l10n.hiddenByChromePref(),
 *   exec: function(args, context) { ... }
 * </pre>
 */
exports.hiddenByChromePref = function() {
  return !prefBranch.prefHasUserValue('devtools.chrome.enabled');
};
