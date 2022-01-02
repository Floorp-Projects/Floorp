/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const isWindows = Services.appinfo.OS === "WINNT";
/**
 * This is the registry for all DAMP tests. Tests will be run in the order specified by
 * the DAMP_TESTS array.
 *
 * A test is defined with the following properties:
 * - {String} name: the name of the test (should match the path when possible)
 * - {String} path: the path to the test file under
 *   testing/talos/talos/tests/devtools/addon/content/tests/
 * - {String} description: Test description
 * - {Boolean} disabled: set to true to skip the test
 * - {Boolean} cold: set to true to run the test only during the first run of the browser
 */

module.exports = [
  // The first cold-open test is *colder* than the other cold-open tests, it will also
  // assess the impact of loading shared DevTools modules for the first time.
  // This test will assert the impact of base loader/Loader.jsm modules loading,
  // typically gDevtools/gDevToolsBrowser/Framework modules, while the others will mostly
  // track panel-specific modules (Browser loader, but not only).
  {
    name: "inspector.cold-open",
    path: "inspector/cold-open.js",
    description:
      "Measure first open toolbox on inspector panel (incl. shared modules)",
    cold: true,
  },
  {
    name: "debugger.cold-open",
    path: "debugger/cold-open.js",
    description: "Measure first open toolbox on debugger panel",
    cold: true,
  },
  {
    name: "webconsole.cold-open",
    path: "webconsole/cold-open.js",
    description: "Measure first open toolbox on webconsole panel",
    cold: true,
  },
  {
    name: "netmonitor.cold-open",
    path: "netmonitor/cold-open.js",
    description: "Measure first open toolbox on netmonitor panel",
    cold: true,
  },
  {
    name: "accessibility.cold-open",
    path: "accessibility/cold-open.js",
    description: "Measure first open toolbox on accessibility panel",
    cold: true,
  },
  // Run all tests against "simple" document
  {
    name: "simple.webconsole",
    path: "webconsole/simple.js",
    description:
      "Measure open/close toolbox on webconsole panel against simple document",
  },
  {
    name: "simple.inspector",
    path: "inspector/simple.js",
    description:
      "Measure open/close toolbox on inspector panel against simple document",
  },
  {
    name: "simple.debugger",
    path: "debugger/simple.js",
    description:
      "Measure open/close toolbox on debugger panel against simple document",
  },
  {
    name: "simple.styleeditor",
    path: "styleeditor/simple.js",
    description:
      "Measure open/close toolbox on style editor panel against simple document",
  },
  {
    name: "simple.netmonitor",
    path: "netmonitor/simple.js",
    description:
      "Measure open/close toolbox on network monitor panel against simple document",
  },
  {
    name: "simple.accessibility",
    path: "accessibility/simple.js",
    description:
      "Measure open/close toolbox on accessibility panel against simple document",
    // Bug 1660854 - disable on Windows due to frequent failures
    disabled: isWindows,
  },
  // Run all tests against "complicated" document
  {
    name: "complicated.webconsole",
    path: "webconsole/complicated.js",
    description:
      "Measure open/close toolbox on webconsole panel against complicated document",
  },
  {
    name: "complicated.inspector",
    path: "inspector/complicated.js",
    description:
      "Measure open/close toolbox on inspector panel against complicated document",
  },
  {
    name: "complicated.debugger",
    path: "debugger/complicated.js",
    description:
      "Measure open/close toolbox on debugger panel against complicated document",
  },
  {
    name: "complicated.styleeditor",
    path: "styleeditor/complicated.js",
    description:
      "Measure open/close toolbox on style editor panel against complicated document",
  },
  // Bug 1693975 - disable test due to frequent failures
  //  {
  //    name: "complicated.netmonitor",
  //    path: "netmonitor/complicated.js",
  //    description:
  //      "Measure open/close toolbox on network monitor panel against complicated document",
  //  },
  // Run all tests against a document specific to each tool
  {
    name: "custom.webconsole",
    path: "webconsole/custom.js",
  },
  {
    name: "custom.inspector",
    path: "inspector/custom.js",
  },
  {
    name: "custom.debugger",
    path: "debugger/custom.js",
  },
  // Run individual tests covering a very precise tool feature.
  {
    name: "console.bulklog",
    path: "webconsole/bulklog.js",
    description:
      "Measure time for a bunch of sync console.log statements to appear",
  },
  {
    name: "console.autocomplete",
    path: "webconsole/autocomplete.js",
    description: "Measure time for autocomplete popup to appear",
  },
  {
    name: "console.streamlog",
    path: "webconsole/streamlog.js",
    description:
      "Measure rAF on page during a stream of console.log statements",
  },
  {
    name: "console.objectexpand",
    path: "webconsole/objectexpand.js",
    description: "Measure time to expand a large object and close the console",
  },
  {
    name: "console.openwithcache",
    path: "webconsole/openwithcache.js",
    description:
      "Measure time to render last logged messages in console for a page with 100 logged messages",
  },
  {
    name: "console.typing",
    path: "webconsole/typing.js",
    description: "Measure time it takes to type something in the console input",
  },
  {
    name: "inspector.mutations",
    path: "inspector/mutations.js",
    description:
      "Measure the time to perform childList mutations when inspector is enabled",
  },
  {
    name: "inspector.layout",
    path: "inspector/layout.js",
    description:
      "Measure the time to open/close toolbox on inspector with layout tab against big document with grid containers",
  },
  {
    name: "panelsInBackground.reload",
    path: "toolbox/panels-in-background.js",
    description: "Measure page reload time when all panels are in background",
  },
  {
    name: "toolbox.screenshot",
    path: "toolbox/screenshot.js",
    description: "Measure the time to take a fullpage screenshot",
  },
  {
    name: "server.protocoljs",
    path: "server/protocol.js",
    description: "Measure RDP/protocol.js performance",
  },
  // ⚠  Adding new individual tests slows down DAMP execution ⚠
  // ⚠  Consider contributing to custom.${tool} rather than adding isolated tests ⚠
  // ⚠  See https://firefox-source-docs.mozilla.org/devtools/tests/writing-perf-tests.html ⚠
];
