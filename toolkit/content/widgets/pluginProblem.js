/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is a UA Widget. It runs in per-origin UA widget scope,
// to be loaded by UAWidgetsChild.jsm.

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
        <!ENTITY % pluginproblemDTD SYSTEM "chrome://pluginproblem/locale/pluginproblem.dtd">
        <!ENTITY % globalDTD SYSTEM "chrome://global/locale/global.dtd">
        <!ENTITY % brandDTD SYSTEM "chrome://branding/locale/brand.dtd" >
        %pluginproblemDTD;
        %globalDTD;
        %brandDTD;
      ]>
      <div xmlns="http://www.w3.org/1999/xhtml" class="mainBox" id="main" chromedir="&locale.dir;">
        <link rel="stylesheet" type="text/css" href="chrome://pluginproblem/content/pluginProblemContent.css" />
        <link rel="stylesheet" type="text/css" href="chrome://global/skin/plugins/pluginProblem.css" />
        <div class="hoverBox">
          <label>
            <button class="icon" id="icon"/>
            <div class="msg msgVulnerabilityStatus" id="vulnerabilityStatus"><!-- set at runtime --></div>
            <div class="msg msgTapToPlay">&tapToPlayPlugin;</div>
            <div class="msg msgClickToPlay" id="clickToPlay">&clickToActivatePlugin;</div>
          </label>

          <div class="msg msgBlocked">&blockedPlugin.label;</div>
          <div class="msg msgCrashed">
            <div class="msgCrashedText" id="crashedText"><!-- set at runtime --></div>
            <!-- link href set at runtime -->
            <div class="msgReload">&reloadPlugin.pre;<a class="reloadLink" id="reloadLink" href="">&reloadPlugin.middle;</a>&reloadPlugin.post;</div>
          </div>

          <div class="msg msgManagePlugins"><a class="action-link" id="managePluginsLink" href="">&managePlugins;</a></div>
          <div class="submitStatus" id="submitStatus">
            <div class="msg msgPleaseSubmit" id="pleaseSubmit">
              <textarea class="submitComment"
                        id="submitComment"
                        placeholder="&report.comment;"/>
              <div class="submitURLOptInBox">
                <label><input class="submitURLOptIn" id="submitURLOptIn" type="checkbox"/> &report.pageURL;</label>
              </div>
              <div class="submitButtonBox">
                <span class="helpIcon" id="helpIcon" role="link"/>
                <input class="submitButton" type="button"
                       id="submitButton"
                       value="&report.please;"/>
              </div>
            </div>
            <div class="msg msgSubmitting">&report.submitting;<span class="throbber"> </span></div>
            <div class="msg msgSubmitted">&report.submitted;</div>
            <div class="msg msgNotSubmitted">&report.disabled;</div>
            <div class="msg msgSubmitFailed">&report.failed;</div>
            <div class="msg msgNoCrashReport">&report.unavailable;</div>
          </div>
          <div class="msg msgCheckForUpdates"><a class="action-link" id="checkForUpdatesLink" href="">&checkForUpdates;</a></div>
      </div>
      <button class="closeIcon" id="closeIcon" title="&hidePluginBtn.label;"/>
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
