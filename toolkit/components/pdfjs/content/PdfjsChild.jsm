/* Copyright 2012 Mozilla Foundation
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

var EXPORTED_SYMBOLS = ["PdfjsChild"];

class PdfjsChild extends JSWindowActorChild {
  init(supportsFind) {
    if (supportsFind) {
      this.sendAsyncMessage("PDFJS:Parent:addEventListener");
    }
  }

  dispatchEvent(type, detail) {
    const contentWindow = this.contentWindow;
    const forward = contentWindow.document.createEvent("CustomEvent");
    forward.initCustomEvent(type, true, true, detail);
    contentWindow.dispatchEvent(forward);
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "PDFJS:Child:handleEvent": {
        let detail = Cu.cloneInto(msg.data.detail, this.contentWindow);
        this.dispatchEvent(msg.data.type, detail);
        break;
      }

      case "PDFJS:ZoomIn":
      case "PDFJS:ZoomOut":
      case "PDFJS:ZoomReset":
      case "PDFJS:Save": {
        const type = msg.name.split("PDFJS:")[1].toLowerCase();
        this.dispatchEvent(type, null);
        break;
      }

      case "PDFJS:Child:fallbackDownload":
        if (this.fallbackCallback) {
          this.fallbackCallback(msg.data.download);
        }
        break;
    }
  }
}
