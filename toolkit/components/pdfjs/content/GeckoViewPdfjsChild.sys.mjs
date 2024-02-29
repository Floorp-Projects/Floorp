/* Copyright 2022 Mozilla Foundation
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

import { GeckoViewActorChild } from "resource://gre/modules/GeckoViewActorChild.sys.mjs";

export class GeckoViewPdfjsChild extends GeckoViewActorChild {
  init(aSupportsFind) {
    this.sendAsyncMessage("PDFJS:Parent:addEventListener", { aSupportsFind });
  }

  dispatchEvent(aType, aDetail) {
    const forward = this.contentWindow.document.createEvent("CustomEvent");
    forward.initCustomEvent(aType, true, true, aDetail);
    this.contentWindow.dispatchEvent(forward);
  }

  receiveMessage(aMsg) {
    debug`receiveMessage: name=${aMsg.name}, data=${aMsg.data}`;

    switch (aMsg.name) {
      case "PDFJS:Child:handleEvent": {
        const detail = Cu.cloneInto(aMsg.data.detail, this.contentWindow);
        this.dispatchEvent(aMsg.data.type, detail);
        break;
      }
      case "PDFJS:Child:getNimbus":
        Services.obs.notifyObservers(aMsg.data, "pdfjs-getNimbus");
        break;
    }
  }
}

const { debug } = GeckoViewPdfjsChild.initLogging("GeckoViewPdfjsChild");
