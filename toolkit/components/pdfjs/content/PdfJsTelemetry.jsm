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

"use strict";

var EXPORTED_SYMBOLS = ["PdfJsTelemetry"];

var PdfJsTelemetry = {
  onViewerIsUsed(isAttachment) {
    Services.telemetry.scalarAdd("pdf.viewer.used", 1);
    if (isAttachment) {
      Services.telemetry.scalarAdd("pdf.viewer.is_attachment", 1);
    }
  },
  onFallbackError(featureId) {
    let histogram = Services.telemetry.getHistogramById(
      "PDF_VIEWER_FALLBACK_ERROR"
    );
    histogram.add(featureId ?? "unknown");
  },
  onDocumentSize(size) {
    let histogram = Services.telemetry.getHistogramById(
      "PDF_VIEWER_DOCUMENT_SIZE_KB"
    );
    histogram.add(size / 1024);
  },
  onDocumentVersion(versionId) {
    let histogram = Services.telemetry.getHistogramById(
      "PDF_VIEWER_DOCUMENT_VERSION_2"
    );
    histogram.add(versionId);
  },
  onDocumentGenerator(generatorId) {
    let histogram = Services.telemetry.getHistogramById(
      "PDF_VIEWER_DOCUMENT_GENERATOR_2"
    );
    histogram.add(generatorId);
  },
  onEmbed(isObject) {
    let histogram = Services.telemetry.getHistogramById("PDF_VIEWER_EMBED_2");
    histogram.add(isObject ? "object_embed" : "iframe");
  },
  onFontType(fontTypeId) {
    let histogram = Services.telemetry.getHistogramById(
      "PDF_VIEWER_FONT_TYPES_2"
    );
    histogram.add(fontTypeId);
  },
  onForm(formType) {
    let histogram = Services.telemetry.getHistogramById("PDF_VIEWER_FORM_2");
    histogram.add(formType);
  },
  onPrint() {
    Services.telemetry.scalarAdd("pdf.viewer.print", 1);
  },
  onStreamType(streamTypeId) {
    let histogram = Services.telemetry.getHistogramById(
      "PDF_VIEWER_STREAM_TYPES_2"
    );
    histogram.add(streamTypeId);
  },
  onTimeToView(ms) {
    let histogram = Services.telemetry.getHistogramById(
      "PDF_VIEWER_TIME_TO_VIEW_MS"
    );
    histogram.add(ms);
  },
  onTagged(tagged) {
    let histogram = Services.telemetry.getHistogramById("PDF_VIEWER_TAGGED");
    histogram.add(tagged);
  },
  onEditing(type) {
    if (["ink", "freetext", "print", "save"].includes(type)) {
      Glean.pdfjs.editing[type].add(1);
    }
  },
  onButtons(id) {
    Glean.pdfjs.buttons[id].add(1);
  },
};
