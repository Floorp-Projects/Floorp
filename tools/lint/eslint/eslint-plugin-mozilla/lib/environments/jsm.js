/**
 * @fileoverview Defines the environment for jsm files.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  "globals": {
    // Intl is standard JS availability.
    "Intl": false,
    // These globals are hard-coded and available in .jsm scopes.
    // https://searchfox.org/mozilla-central/rev/ed212c79cfe86357e9a5740082b9364e7f6e526f/js/xpconnect/loader/mozJSComponentLoader.cpp#134-140
    "atob": false,
    "btoa": false,
    "debug": false,
    "dump": false,
    // These globals are made available via WebIDL files, see ResolveSystemBinding in:
    // https://searchfox.org/mozilla-central/source/__GENERATED__/dom/bindings/ResolveSystemBinding.cpp
    "AbortController": false,
    "AbortSignal": false,
    "AddonManagerPermissions": false,
    "ChannelWrapper": false,
    "ChromeUtils": false,
    "ChromeWorker": false,
    "DOMError": false,
    "DOMException": false,
    "DOMRequest": false,
    "DOMStringList": false,
    "DominatorTree": false,
    "ErrorEvent": false,
    "Event": false,
    "EventTarget": false,
    "FileReader": false,
    "HeapSnapshot": false,
    "IDBCursor": false,
    "IDBCursorWithValue": false,
    "IDBDatabase": false,
    "IDBFactory": false,
    "IDBFileHandle": false,
    "IDBFileRequest": false,
    "IDBIndex": false,
    "IDBKeyRange": false,
    "IDBLocaleAwareKeyRange": false,
    "IDBMutableFile": false,
    "IDBObjectStore": false,
    "IDBOpenDBRequest": false,
    "IDBRequest": false,
    "IDBTransaction": false,
    "IDBVersionChangeEvent": false,
    "IdleDeadline": false,
    "MatchGlob": false,
    "MatchPattern": false,
    "MatchPatternSet": false,
    "MessageEvent": false,
    "MessagePort": false,
    "PrecompiledScript": false,
    "PromiseDebugging": false,
    "StreamFilter": false,
    "StreamFilterDataEvent": false,
    "StructuredCloneHolder": false,
    "TCPServerSocket": false,
    "TCPServerSocketEvent": false,
    "TCPSocket": false,
    "TCPSocketErrorEvent": false,
    "TCPSocketEvent": false,
    "TextDecoder": false,
    "TextEncoder": false,
    "URLSearchParams": false,
    "WebExtensionContentScript": false,
    "WebExtensionPolicy": false,
    "Worker": false,
    // Items not defined as System specifically, but available globally.
    "File": false,
    "Headers": false,
    "XMLHttpRequest": false,
    "URL": false
  }
};
