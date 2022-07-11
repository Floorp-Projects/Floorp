/* Copyright 2020 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

const { SandboxSupportBase } = ChromeUtils.import(
  "resource://pdf.js/build/pdf.sandbox.external.js"
);

const EXPORTED_SYMBOLS = ["PdfSandbox"];

class SandboxSupport extends SandboxSupportBase {
  constructor(win, sandbox) {
    super(win);
    this.sandbox = sandbox;
  }
  exportValueToSandbox(val) {
    return Cu.cloneInto(val, this.sandbox);
  }

  importValueFromSandbox(val) {
    return Cu.cloneInto(val, this.win);
  }

  createErrorForSandbox(errorMessage) {
    return new this.sandbox.Error(errorMessage);
  }
}

class PdfSandbox {
  constructor(window, data) {
    this.window = window;
    const sandbox = Cu.Sandbox(null, {
      sandboxName: "PDF.js scripting sandbox",
      sameZoneAs: window,
      wantXrays: true,
      wantGlobalProperties: [],
      wantComponents: false,
      wantExportHelpers: false,
    });
    this.sandbox = sandbox;
    this.support = new SandboxSupport(window, sandbox);

    const sandboxInit = Cu.createObjectIn(sandbox);
    sandboxInit.data = Cu.cloneInto(data, sandbox);

    sandbox.callExternalFunction = Cu.exportFunction(
      this.support.createSandboxExternals(),
      sandbox
    );

    try {
      // Run the code in the bundle
      Services.scriptloader.loadSubScript(
        "resource://pdf.js/build/pdf.scripting.js",
        sandbox
      );

      this.support.commFun = sandbox.pdfjsScripting.wrappedJSObject.initSandbox(
        sandboxInit
      );
    } catch (error) {
      // error belongs to the sandbox so create a new one
      const msg = error.message;
      this.destroy();
      throw new Error(msg);
    }
  }

  dispatchEvent(event) {
    this.support.callSandboxFunction("dispatchEvent", event);
  }

  destroy() {
    if (this.sandbox) {
      this.support.destroy();
      this.support = null;
      Cu.nukeSandbox(this.sandbox);
      this.sandbox = null;
    }
  }
}
