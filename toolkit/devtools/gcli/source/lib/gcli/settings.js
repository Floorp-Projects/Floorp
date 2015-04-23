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

var imports = {};

var Cc = require('chrome').Cc;
var Ci = require('chrome').Ci;
var Cu = require('chrome').Cu;

var XPCOMUtils = Cu.import('resource://gre/modules/XPCOMUtils.jsm', {}).XPCOMUtils;
var Services = Cu.import('resource://gre/modules/Services.jsm', {}).Services;

XPCOMUtils.defineLazyGetter(imports, 'prefBranch', function() {
  var prefService = Cc['@mozilla.org/preferences-service;1']
          .getService(Ci.nsIPrefService);
  return prefService.getBranch(null).QueryInterface(Ci.nsIPrefBranch2);
});

XPCOMUtils.defineLazyGetter(imports, 'supportsString', function() {
  return Cc['@mozilla.org/supports-string;1']
          .createInstance(Ci.nsISupportsString);
});

var util = require('./util/util');

/**
 * All local settings have this prefix when used in Firefox
 */
var DEVTOOLS_PREFIX = 'devtools.gcli.';

/**
 * A manager for the registered Settings
 */
function Settings(types, settingValues) {
  this._types = types;

  if (settingValues != null) {
    throw new Error('settingValues is not supported when writing to prefs');
  }

  // Collection of preferences for sorted access
  this._settingsAll = [];

  // Collection of preferences for fast indexed access
  this._settingsMap = new Map();

  // Flag so we know if we've read the system preferences
  this._hasReadSystem = false;

  // Event for use to detect when the list of settings changes
  this.onChange = util.createEvent('Settings.onChange');
}

/**
 * Load system prefs if they've not been loaded already
 * @return true
 */
Settings.prototype._readSystem = function() {
  if (this._hasReadSystem) {
    return;
  }

  imports.prefBranch.getChildList('').forEach(function(name) {
    var setting = new Setting(this, name);
    this._settingsAll.push(setting);
    this._settingsMap.set(name, setting);
  }.bind(this));

  this._settingsAll.sort(function(s1, s2) {
    return s1.name.localeCompare(s2.name);
  }.bind(this));

  this._hasReadSystem = true;
};

/**
 * Get an array containing all known Settings filtered to match the given
 * filter (string) at any point in the name of the setting
 */
Settings.prototype.getAll = function(filter) {
  this._readSystem();

  if (filter == null) {
    return this._settingsAll;
  }

  return this._settingsAll.filter(function(setting) {
    return setting.name.indexOf(filter) !== -1;
  }.bind(this));
};

/**
 * Add a new setting
 */
Settings.prototype.add = function(prefSpec) {
  var setting = new Setting(this, prefSpec);

  if (this._settingsMap.has(setting.name)) {
    // Once exists already, we're going to need to replace it in the array
    for (var i = 0; i < this._settingsAll.length; i++) {
      if (this._settingsAll[i].name === setting.name) {
        this._settingsAll[i] = setting;
      }
    }
  }

  this._settingsMap.set(setting.name, setting);
  this.onChange({ added: setting.name });

  return setting;
};

/**
 * Getter for an existing setting. Generally use of this function should be
 * avoided. Systems that define a setting should export it if they wish it to
 * be available to the outside, or not otherwise. Use of this function breaks
 * that boundary and also hides dependencies. Acceptable uses include testing
 * and embedded uses of GCLI that pre-define all settings (e.g. Firefox)
 * @param name The name of the setting to fetch
 * @return The found Setting object, or undefined if the setting was not found
 */
Settings.prototype.get = function(name) {
  // We might be able to give the answer without needing to read all system
  // settings if this is an internal setting
  var found = this._settingsMap.get(name);
  if (!found) {
    found = this._settingsMap.get(DEVTOOLS_PREFIX + name);
  }

  if (found) {
    return found;
  }

  if (this._hasReadSystem) {
    return undefined;
  }
  else {
    this._readSystem();
    found = this._settingsMap.get(name);
    if (!found) {
      found = this._settingsMap.get(DEVTOOLS_PREFIX + name);
    }
    return found;
  }
};

/**
 * Remove a setting. A no-op in this case
 */
Settings.prototype.remove = function() {
};

exports.Settings = Settings;

/**
 * A class to wrap up the properties of a Setting.
 * @see toolkit/components/viewconfig/content/config.js
 */
function Setting(settings, prefSpec) {
  this._settings = settings;
  if (typeof prefSpec === 'string') {
    // We're coming from getAll() i.e. a full listing of prefs
    this.name = prefSpec;
    this.description = '';
  }
  else {
    // A specific addition by GCLI
    this.name = DEVTOOLS_PREFIX + prefSpec.name;

    if (prefSpec.ignoreTypeDifference !== true && prefSpec.type) {
      if (this.type.name !== prefSpec.type) {
        throw new Error('Locally declared type (' + prefSpec.type + ') != ' +
            'Mozilla declared type (' + this.type.name + ') for ' + this.name);
      }
    }

    this.description = prefSpec.description;
  }

  this.onChange = util.createEvent('Setting.onChange');
}

/**
 * Reset this setting to it's initial default value
 */
Setting.prototype.setDefault = function() {
  imports.prefBranch.clearUserPref(this.name);
  Services.prefs.savePrefFile(null);
};

/**
 * What type is this property: boolean/integer/string?
 */
Object.defineProperty(Setting.prototype, 'type', {
  get: function() {
    switch (imports.prefBranch.getPrefType(this.name)) {
      case imports.prefBranch.PREF_BOOL:
        return this._settings._types.createType('boolean');

      case imports.prefBranch.PREF_INT:
        return this._settings._types.createType('number');

      case imports.prefBranch.PREF_STRING:
        return this._settings._types.createType('string');

      default:
        throw new Error('Unknown type for ' + this.name);
    }
  },
  enumerable: true
});

/**
 * What type is this property: boolean/integer/string?
 */
Object.defineProperty(Setting.prototype, 'value', {
  get: function() {
    switch (imports.prefBranch.getPrefType(this.name)) {
      case imports.prefBranch.PREF_BOOL:
        return imports.prefBranch.getBoolPref(this.name);

      case imports.prefBranch.PREF_INT:
        return imports.prefBranch.getIntPref(this.name);

      case imports.prefBranch.PREF_STRING:
        var value = imports.prefBranch.getComplexValue(this.name,
                Ci.nsISupportsString).data;
        // In case of a localized string
        if (/^chrome:\/\/.+\/locale\/.+\.properties/.test(value)) {
          value = imports.prefBranch.getComplexValue(this.name,
                  Ci.nsIPrefLocalizedString).data;
        }
        return value;

      default:
        throw new Error('Invalid value for ' + this.name);
    }
  },

  set: function(value) {
    if (imports.prefBranch.prefIsLocked(this.name)) {
      throw new Error('Locked preference ' + this.name);
    }

    switch (imports.prefBranch.getPrefType(this.name)) {
      case imports.prefBranch.PREF_BOOL:
        imports.prefBranch.setBoolPref(this.name, value);
        break;

      case imports.prefBranch.PREF_INT:
        imports.prefBranch.setIntPref(this.name, value);
        break;

      case imports.prefBranch.PREF_STRING:
        imports.supportsString.data = value;
        imports.prefBranch.setComplexValue(this.name,
                Ci.nsISupportsString,
                imports.supportsString);
        break;

      default:
        throw new Error('Invalid value for ' + this.name);
    }

    Services.prefs.savePrefFile(null);
  },

  enumerable: true
});
