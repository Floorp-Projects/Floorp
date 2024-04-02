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

export class PdfjsChild extends JSWindowActorChild {
  init(aSupportsFind) {
    if (aSupportsFind) {
      this.sendAsyncMessage("PDFJS:Parent:addEventListener");
    }
  }

  dispatchEvent(aType, aDetail) {
    aDetail &&= Cu.cloneInto(aDetail, this.contentWindow);
    const contentWindow = this.contentWindow;
    const forward = contentWindow.document.createEvent("CustomEvent");
    forward.initCustomEvent(aType, true, true, aDetail);
    contentWindow.dispatchEvent(forward);
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "PDFJS:Child:handleEvent":
        this.dispatchEvent(msg.data.type, msg.data.detail);
        break;
      case "PDFJS:Editing":
        this.dispatchEvent("editingaction", msg.data);
        break;
      case "PDFJS:ZoomIn":
      case "PDFJS:ZoomOut":
      case "PDFJS:ZoomReset":
      case "PDFJS:Save": {
        const type = msg.name.split("PDFJS:")[1].toLowerCase();
        this.dispatchEvent(type, null);
        break;
      }
    }
  }
}
