/**
 * Support types for toolkit/components/extensions code.
 */

/// <reference lib="dom" />
/// <reference path="./gecko.ts" />
/// <reference path="./extensions.ts" />

// This now relies on types generated in bug 1872918, or get the built
// artifact tslib directly and put it in your src/node_modules/@types:
// https://phabricator.services.mozilla.com/D197620
/// <reference types="lib.gecko.xpidl" />

// Exports for all other external modules redirected to globals.ts.
export var AppConstants,
  GeckoViewConnection, GeckoViewWebExtension, IndexedDB, JSONFile, Log;

/**
 * This is a mock for the "class" from EventEmitter.sys.mjs. When we import
 * it in extensions code using resource://gre/modules/EventEmitter.sys.mjs,
 * the catch-all rule from tsconfig.json redirects it to this file. The export
 * of the class below fulfills the import. The mock is needed when we subclass
 * that EventEmitter, typescript gets confused because it's an old style
 * function-and-prototype-based "class", and some types don't match up.
 *
 * TODO: Convert EventEmitter.sys.mjs into a proper class.
 */
export declare class EventEmitter {
  on(event: string, listener: callback): void;
  once(event: string, listener: callback): Promise<any>;
  off(event: string, listener: callback): void;
  emit(event: string, ...args: any[]): void;
}
