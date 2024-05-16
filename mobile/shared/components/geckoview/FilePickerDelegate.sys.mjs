/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewUtils } from "resource://gre/modules/GeckoViewUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  GeckoViewPrompter: "resource://gre/modules/GeckoViewPrompter.sys.mjs",
});

const { debug, warn } = GeckoViewUtils.initLogging("FilePickerDelegate");

export class FilePickerDelegate {
  /* ----------  nsIFilePicker  ---------- */
  init(aBrowsingContext, aTitle, aMode) {
    if (
      aMode === Ci.nsIFilePicker.modeGetFolder ||
      aMode === Ci.nsIFilePicker.modeSave
    ) {
      throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
    }
    this._prompt = new lazy.GeckoViewPrompter(aBrowsingContext);
    this._msg = {
      type: "file",
      title: aTitle,
      mode: aMode === Ci.nsIFilePicker.modeOpenMultiple ? "multiple" : "single",
    };
    this._mode = aMode;
    this._mimeTypes = [];
    this._capture = 0;
  }

  get mode() {
    return this._mode;
  }

  appendRawFilter(aFilter) {
    this._mimeTypes.push(aFilter);
  }

  show() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  open(aFilePickerShownCallback) {
    this._msg.mimeTypes = this._mimeTypes;
    this._msg.capture = this._capture;
    this._prompt.asyncShowPrompt(this._msg, result => {
      // OK: result
      // Cancel: !result
      if (!result || !result.files || !result.files.length) {
        aFilePickerShownCallback.done(Ci.nsIFilePicker.returnCancel);
      } else {
        this._resolveFiles(result.files, aFilePickerShownCallback);
      }
    });
  }

  async _resolveFiles(aFiles, aCallback) {
    const fileData = [];

    try {
      for (const file of aFiles) {
        const domFile = await this._getDOMFile(file);
        fileData.push({
          file,
          domFile,
        });
      }
    } catch (ex) {
      warn`Error resolving files from file picker: ${ex}`;
      aCallback.done(Ci.nsIFilePicker.returnCancel);
      return;
    }

    this._fileData = fileData;
    aCallback.done(Ci.nsIFilePicker.returnOK);
  }

  get file() {
    if (!this._fileData) {
      throw Components.Exception("", Cr.NS_ERROR_NOT_AVAILABLE);
    }
    const fileData = this._fileData[0];
    if (!fileData) {
      return null;
    }
    return new lazy.FileUtils.File(fileData.file);
  }

  get fileURL() {
    return Services.io.newFileURI(this.file);
  }

  *_getEnumerator(aDOMFile) {
    if (!this._fileData) {
      throw Components.Exception("", Cr.NS_ERROR_NOT_AVAILABLE);
    }

    for (const fileData of this._fileData) {
      if (aDOMFile) {
        yield fileData.domFile;
      }
      yield new lazy.FileUtils.File(fileData.file);
    }
  }

  get files() {
    return this._getEnumerator(/* aDOMFile */ false);
  }

  _getDOMFile(aPath) {
    if (this._prompt.domWin) {
      return this._prompt.domWin.File.createFromFileName(aPath);
    }
    return File.createFromFileName(aPath);
  }

  get domFileOrDirectory() {
    if (!this._fileData) {
      throw Components.Exception("", Cr.NS_ERROR_NOT_AVAILABLE);
    }
    return this._fileData[0] ? this._fileData[0].domFile : null;
  }

  get domFileOrDirectoryEnumerator() {
    return this._getEnumerator(/* aDOMFile */ true);
  }

  get defaultString() {
    return "";
  }

  set defaultString(aValue) {}

  get defaultExtension() {
    return "";
  }

  set defaultExtension(aValue) {}

  get filterIndex() {
    return 0;
  }

  set filterIndex(aValue) {}

  get displayDirectory() {
    return null;
  }

  set displayDirectory(aValue) {}

  get displaySpecialDirectory() {
    return "";
  }

  set displaySpecialDirectory(aValue) {}

  get addToRecentDocs() {
    return false;
  }

  set addToRecentDocs(aValue) {}

  get okButtonLabel() {
    return "";
  }

  set okButtonLabel(aValue) {}

  get capture() {
    return this._capture;
  }

  set capture(aValue) {
    this._capture = aValue;
  }
}

FilePickerDelegate.prototype.classID = Components.ID(
  "{e4565e36-f101-4bf5-950b-4be0887785a9}"
);
FilePickerDelegate.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIFilePicker",
]);
