// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
var ContextCommands = {
  copy: function cc_copy() {
    let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
    clipboard.copyString(ContextHelper.popupState.string);

    let target = ContextHelper.popupState.target;
    if (target)
      target.focus();
  },

#ifdef ANDROID
  selectInput: function cc_selectInput() {
    let imePicker = Cc["@mozilla.org/imepicker;1"].getService(Ci.nsIIMEPicker);
    imePicker.show();
  },
#endif

  paste: function cc_paste() {
    let target = ContextHelper.popupState.target;
    if (target.localName == "browser") {
      let x = ContextHelper.popupState.x;
      let y = ContextHelper.popupState.y;
      let json = {x: x, y: y, command: "paste" };
      messageManager.sendAsyncMessage("Browser:ContextCommand", json);
    } else {
      target.editor.paste(Ci.nsIClipboard.kGlobalClipboard);
      target.focus();
    }
  },

  selectAll: function cc_selectAll() {
    let target = ContextHelper.popupState.target;
    if (target.localName == "browser") {
      let x = ContextHelper.popupState.x;
      let y = ContextHelper.popupState.y;
      let json = {x: x, y: y, command: "select-all" };
      messageManager.sendAsyncMessage("Browser:ContextCommand", json);
    } else {
      target.editor.selectAll();
      target.focus();
    }
  },

  openInNewTab: function cc_openInNewTab() {
    Browser.addTab(ContextHelper.popupState.linkURL, false, Browser.selectedTab);
  },

  saveLink: function cc_saveLink() {
    let browser = ContextHelper.popupState.target;
    ContentAreaUtils.saveURL(ContextHelper.popupState.linkURL, null, "SaveLinkTitle", false, true, browser.documentURI);
  },

  saveImage: function cc_saveImage() {
    let browser = ContextHelper.popupState.target;
    ContentAreaUtils.saveImageURL(ContextHelper.popupState.mediaURL, null, "SaveImageTitle", false, true, browser.documentURI);
  },

  shareLink: function cc_shareLink() {
    let state = ContextHelper.popupState;
    SharingUI.show(state.linkURL, state.linkTitle);
  },

  shareMedia: function cc_shareMedia() {
    SharingUI.show(ContextHelper.popupState.mediaURL, null);
  },

  sendCommand: function cc_playVideo(aCommand) {
    let browser = ContextHelper.popupState.target;
    browser.messageManager.sendAsyncMessage("Browser:ContextCommand", { command: aCommand });
  },

  editBookmark: function cc_editBookmark() {
    let target = ContextHelper.popupState.target;
    target.startEditing();
  },

  removeBookmark: function cc_removeBookmark() {
    let target = ContextHelper.popupState.target;
    target.remove();
  }
};
