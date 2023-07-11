/* Copyright 2013 Mozilla Foundation
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
/* eslint max-len: ["error", 100] */

export const PdfJsTelemetry = {
  onViewerIsUsed() {
    Glean.pdfjs.used.add(1);
  },
  onTimeToView(ms) {
    Glean.pdfjs.timeToView.accumulateSamples([ms]);
  },
  onEditing(type) {
    if (["ink", "freetext", "stamp", "print", "save"].includes(type)) {
      Glean.pdfjs.editing[type].add(1);
    }
  },
  onButtons(id) {
    Glean.pdfjs.buttons[id].add(1);
  },
  onGeckoview(id) {
    Glean.pdfjs.geckoview[id].add(1);
  },
};
