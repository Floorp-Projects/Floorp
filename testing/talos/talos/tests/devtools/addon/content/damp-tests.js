/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const isWindows = Services.appinfo.OS === "WINNT";

// DAMP is split in sub-suites to run the tests faster on continuous integration.
// See the initial patches in Bug 1749928 if we need to add more suites.
const TEST_SUITES = {
  INSPECTOR: "inspector",
  WEBCONSOLE: "webconsole",
  OTHER: "other",
};

/**
 * This is the registry for all DAMP tests. The registry is an object containing
 * one property for each DAMP sub-suite used in continuous integration. And each
 * property contains the array of tests which correspond to this suite.
 * Tests will be run in the order specified by the array.
 *
 * A test is defined with the following properties:
 * - {String} name: the name of the test (should match the path when possible)
 * - {String} path: the path to the test file under
 *   testing/talos/talos/tests/devtools/addon/content/tests/
 * - {String} description: Test description
 * - {Boolean} disabled: set to true to skip the test
 * - {Boolean} cold: set to true to run the test only during the first run of the browser
 */

module.exports = {
  [TEST_SUITES.INSPECTOR]: [
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
      name: "accessibility.cold-open",
      path: "accessibility/cold-open.js",
      description: "Measure first open toolbox on accessibility panel",
      cold: true,
    },
    // Run all tests against "simple" document
    {
      name: "simple.inspector",
      path: "inspector/simple.js",
      description:
        "Measure open/close toolbox on inspector panel against simple document",
    },
    {
      name: "simple.styleeditor",
      path: "styleeditor/simple.js",
      description:
        "Measure open/close toolbox on style editor panel against simple document",
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
      name: "complicated.inspector",
      path: "inspector/complicated.js",
      description:
        "Measure open/close toolbox on inspector panel against complicated document",
    },
    {
      name: "complicated.styleeditor",
      path: "styleeditor/complicated.js",
      description:
        "Measure open/close toolbox on style editor panel against complicated document",
    },
    {
      name: "custom.inspector",
      path: "inspector/custom.js",
    },
    {
      name: "custom.styleeditor",
      path: "styleeditor/custom.js",
    },
    // Run individual tests covering a very precise tool feature.
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
  ],
  [TEST_SUITES.WEBCONSOLE]: [
    {
      name: "webconsole.cold-open",
      path: "webconsole/cold-open.js",
      description: "Measure first open toolbox on webconsole panel",
      cold: true,
    },
    {
      name: "simple.webconsole",
      path: "webconsole/simple.js",
      description:
        "Measure open/close toolbox on webconsole panel against simple document",
    },
    {
      name: "complicated.webconsole",
      path: "webconsole/complicated.js",
      description:
        "Measure open/close toolbox on webconsole panel against complicated document",
    },
    {
      name: "custom.webconsole",
      path: "webconsole/custom.js",
    },
    {
      name: "console.bulklog",
      path: "webconsole/bulklog.js",
      description:
        "Measure time for a bunch of sync console.log statements to appear",
    },
    {
      name: "console.log-in-loop-content-process",
      path: "webconsole/log-in-loop-content-process.js",
      description:
        "Measure time for a bunch of sync console.log statements to be handled on the content process",
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
      description:
        "Measure time to expand a large object and close the console",
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
      description:
        "Measure time it takes to type something in the console input",
    },
  ],
  [TEST_SUITES.OTHER]: [
    {
      name: "debugger.cold-open",
      path: "debugger/cold-open.js",
      description: "Measure first open toolbox on debugger panel",
      cold: true,
    },
    {
      name: "netmonitor.cold-open",
      path: "netmonitor/cold-open.js",
      description: "Measure first open toolbox on netmonitor panel",
      cold: true,
    },
    {
      name: "simple.debugger",
      path: "debugger/simple.js",
      description:
        "Measure open/close toolbox on debugger panel against simple document",
    },
    {
      name: "simple.netmonitor",
      path: "netmonitor/simple.js",
      description:
        "Measure open/close toolbox on network monitor panel against simple document",
    },
    {
      name: "complicated.debugger",
      path: "debugger/complicated.js",
      description:
        "Measure open/close toolbox on debugger panel against complicated document",
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
      name: "custom.debugger",
      path: "debugger/custom.js",
    },
    {
      name: "custom.netmonitor",
      path: "netmonitor/custom.js",
      description:
        "Measure open/reload/close toolbox on network monitor panel against a custom test document",
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
      name: "browser-toolbox",
      path: "toolbox/browser-toolbox.js",
    },
    {
      name: "server.protocoljs",
      path: "server/protocol.js",
      description: "Measure RDP/protocol.js performance",
    },
    // ⚠  Adding new individual tests slows down DAMP execution ⚠
    // ⚠  Consider contributing to custom.${tool} rather than adding isolated tests ⚠
    // ⚠  See https://firefox-source-docs.mozilla.org/devtools/tests/writing-perf-tests.html ⚠
  ],
};
