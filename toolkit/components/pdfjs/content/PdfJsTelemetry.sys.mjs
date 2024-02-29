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

export class PdfJsTelemetryContent {
  static onViewerIsUsed() {
    Glean.pdfjs.used.add(1);
  }
}

export class PdfJsTelemetry {
  static report(aData) {
    const { type } = aData;
    switch (type) {
      case "pageInfo":
        this.onTimeToView(aData.timestamp);
        break;
      case "editing":
        this.onEditing(aData);
        break;
      case "buttons":
      case "gv-buttons":
        {
          const id = aData.data.id.replace(
            /([A-Z])/g,
            c => `_${c.toLowerCase()}`
          );
          if (type === "buttons") {
            this.onButtons(id);
          } else {
            this.onGeckoview(id);
          }
        }
        break;
    }
  }

  static onTimeToView(ms) {
    Glean.pdfjs.timeToView.accumulateSamples([ms]);
  }

  static onEditing({ type, data }) {
    if (type !== "editing" || !data) {
      return;
    }

    switch (data.type) {
      case "freetext":
      case "ink":
        Glean.pdfjs.editing[data.type].add(1);
        return;
      case "print":
      case "save":
        {
          Glean.pdfjs.editing[data.type].add(1);
          if (!data.stats) {
            return;
          }
          const numbers = ["one", "two", "three", "four", "five"];
          Glean.pdfjsEditingHighlight[data.type].add(1);
          Glean.pdfjsEditingHighlight.numberOfColors[
            numbers[data.stats.highlight.numberOfColors - 1]
          ].add(1);
        }
        return;
      case "stamp":
        if (data.action === "added") {
          Glean.pdfjs.editing.stamp.add(1);
          return;
        }
        Glean.pdfjs.stamp[data.action].add(1);
        for (const key of [
          "alt_text_keyboard",
          "alt_text_decorative",
          "alt_text_description",
          "alt_text_edit",
        ]) {
          if (data[key]) {
            Glean.pdfjs.stamp[key].add(1);
          }
        }
        return;
      case "highlight":
      case "free_highlight":
        switch (data.action) {
          case "added":
            Glean.pdfjsEditingHighlight.kind[data.type].add(1);
            Glean.pdfjsEditingHighlight.method[data.methodOfCreation].add(1);
            Glean.pdfjsEditingHighlight.color[data.color].add(1);
            if (data.type === "free_highlight") {
              Glean.pdfjsEditingHighlight.thickness.accumulateSamples([
                data.thickness,
              ]);
            }
            break;
          case "color_changed":
            Glean.pdfjsEditingHighlight.color[data.color].add(1);
            Glean.pdfjsEditingHighlight.colorChanged.add(1);
            break;
          case "thickness_changed":
            Glean.pdfjsEditingHighlight.thickness.accumulateSamples([
              data.thickness,
            ]);
            Glean.pdfjsEditingHighlight.thicknessChanged.add(1);
            break;
          case "deleted":
            Glean.pdfjsEditingHighlight.deleted.add(1);
            break;
          case "edited":
            Glean.pdfjsEditingHighlight.edited.add(1);
            break;
          case "toggle_visibility":
            Glean.pdfjsEditingHighlight.toggleVisibility.add(1);
            break;
        }
        break;
    }
  }

  static onButtons(id) {
    Glean.pdfjs.buttons[id].add(1);
  }

  static onGeckoview(id) {
    Glean.pdfjs.geckoview[id].add(1);
  }
}
