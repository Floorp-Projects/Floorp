/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is a UA Widget. It runs in per-origin UA widget scope,
// to be loaded by UAWidgetsChild.jsm.
// This widget results in a hidden element that occupies room where the plugin
// would be if we still supported plugins.
this.PluginProblemWidget = class {
  constructor(shadowRoot) {
    this.shadowRoot = shadowRoot;
    this.element = shadowRoot.host;
    // ownerGlobal is chrome-only, not accessible to UA Widget script here.
    this.window = this.element.ownerDocument.defaultView; // eslint-disable-line mozilla/use-ownerGlobal
  }

  onsetup() {
    const parser = new this.window.DOMParser();
    parser.forceEnableDTD();
    let parserDoc = parser.parseFromString(
      `
      <!DOCTYPE bindings [
        <!ENTITY % globalDTD SYSTEM "chrome://global/locale/global.dtd">
        <!ENTITY % brandDTD SYSTEM "chrome://branding/locale/brand.dtd" >
        %globalDTD;
        %brandDTD;
      ]>
      <div xmlns="http://www.w3.org/1999/xhtml" class="mainBox" id="main" chromedir="&locale.dir;">
        <link rel="stylesheet" type="text/css" href="chrome://pluginproblem/content/pluginProblemContent.css" />
      </div>
    `,
      "application/xml"
    );
    this.shadowRoot.importNodeAndAppendChildAt(
      this.shadowRoot,
      parserDoc.documentElement,
      true
    );

    // Notify browser-plugins.js that we were attached, on a delay because
    // this binding doesn't complete layout until the constructor
    // completes.
    this.element.dispatchEvent(
      new this.window.CustomEvent("PluginBindingAttached")
    );
  }
};
