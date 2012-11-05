/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["StringBundle"];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

/**
 * A string bundle.
 *
 * This object presents two APIs: a deprecated one that is equivalent to the API
 * for the stringbundle XBL binding, to make it easy to switch from that binding
 * to this module, and a new one that is simpler and easier to use.
 *
 * The benefit of this module over the XBL binding is that it can also be used
 * in JavaScript modules and components, not only in chrome JS.
 *
 * To use this module, import it, create a new instance of StringBundle,
 * and then use the instance's |get| and |getAll| methods to retrieve strings
 * (you can get both plain and formatted strings with |get|):
 *
 *   let strings =
 *     new StringBundle("chrome://example/locale/strings.properties");
 *   let foo = strings.get("foo");
 *   let barFormatted = strings.get("bar", [arg1, arg2]);
 *   for each (let string in strings.getAll())
 *     dump (string.key + " = " + string.value + "\n");
 *
 * @param url {String}
 *        the URL of the string bundle
 */
this.StringBundle = function StringBundle(url) {
  this.url = url;
}

StringBundle.prototype = {
  /**
   * the locale associated with the application
   * @type nsILocale
   * @private
   */
  get _appLocale() {
    try {
      return Cc["@mozilla.org/intl/nslocaleservice;1"].
             getService(Ci.nsILocaleService).
             getApplicationLocale();
    }
    catch(ex) {
      return null;
    }
  },

  /**
   * the wrapped nsIStringBundle
   * @type nsIStringBundle
   * @private
   */
  get _stringBundle() {
    let stringBundle = Cc["@mozilla.org/intl/stringbundle;1"].
                       getService(Ci.nsIStringBundleService).
                       createBundle(this.url, this._appLocale);
    this.__defineGetter__("_stringBundle", function() stringBundle);
    return this._stringBundle;
  },


  // the new API

  /**
   * the URL of the string bundle
   * @type String
   */
  _url: null,
  get url() {
    return this._url;
  },
  set url(newVal) {
    this._url = newVal;
    delete this._stringBundle;
  },

  /**
   * Get a string from the bundle.
   *
   * @param key {String}
   *        the identifier of the string to get
   * @param args {array} [optional]
   *        an array of arguments that replace occurrences of %S in the string
   *
   * @returns {String} the value of the string
   */
  get: function(key, args) {
    if (args)
      return this.stringBundle.formatStringFromName(key, args, args.length);
    else
      return this.stringBundle.GetStringFromName(key);
  },

  /**
   * Get all the strings in the bundle.
   *
   * @returns {Array}
   *          an array of objects with key and value properties
   */
  getAll: function() {
    let strings = [];

    // FIXME: for performance, return an enumerable array that wraps the string
    // bundle's nsISimpleEnumerator (does JavaScript already support this?).

    let enumerator = this.stringBundle.getSimpleEnumeration();

    while (enumerator.hasMoreElements()) {
      // We could simply return the nsIPropertyElement objects, but I think
      // it's better to return standard JS objects that behave as consumers
      // expect JS objects to behave (f.e. you can modify them dynamically).
      let string = enumerator.getNext().QueryInterface(Ci.nsIPropertyElement);
      strings.push({ key: string.key, value: string.value });
    }

    return strings;
  },


  // the deprecated XBL binding-compatible API

  /**
   * the URL of the string bundle
   * @deprecated because its name doesn't make sense outside of an XBL binding
   * @type String
   */
  get src() {
    return this.url;
  },
  set src(newVal) {
    this.url = newVal;
  },

  /**
   * the locale associated with the application
   * @deprecated because it has never been used outside the XBL binding itself,
   * and consumers should obtain it directly from the locale service anyway.
   * @type nsILocale
   */
  get appLocale() {
    return this._appLocale;
  },

  /**
   * the wrapped nsIStringBundle
   * @deprecated because this module should provide all necessary functionality
   * @type nsIStringBundle
   *
   * If you do ever need to use this, let the authors of this module know why
   * so they can surface functionality for your use case in the module itself
   * and you don't have to access this underlying XPCOM component.
   */
  get stringBundle() {
    return this._stringBundle;
  },

  /**
   * Get a string from the bundle.
   * @deprecated use |get| instead
   *
   * @param key {String}
   *        the identifier of the string to get
   *
   * @returns {String}
   *          the value of the string
   */
  getString: function(key) {
    return this.get(key);
  },

  /**
   * Get a formatted string from the bundle.
   * @deprecated use |get| instead
   *
   * @param key {string}
   *        the identifier of the string to get
   * @param args {array}
   *        an array of arguments that replace occurrences of %S in the string
   *
   * @returns {String}
   *          the formatted value of the string
   */
  getFormattedString: function(key, args) {
    return this.get(key, args);
  },

  /**
   * Get an enumeration of the strings in the bundle.
   * @deprecated use |getAll| instead
   *
   * @returns {nsISimpleEnumerator}
   *          a enumeration of the strings in the bundle
   */
  get strings() {
    return this.stringBundle.getSimpleEnumeration();
  }
}
