/*
 * This is a mock file pointing to PlacesUtils.jsm for add-ons backwards
 * compatibility.
 *
 * If you are importing this file, you should instead import PlacesUtils.jsm.
 * If you need to support different versions of toolkit, then you can use the
 * following code to import PlacesUtils in your scope.
 *
 * Try {
 *   Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
 * } catch (ex) {
 *   Components.utils.import("resource://gre/modules/utils.js");
 * }
 *
 */

var EXPORTED_SYMBOLS = ["PlacesUtils"];

Components.utils.reportError('An add-on is importing utils.js module, this file is deprecated, PlacesUtils.jsm should be used instead. Please notify add-on author of this problem.');

Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
