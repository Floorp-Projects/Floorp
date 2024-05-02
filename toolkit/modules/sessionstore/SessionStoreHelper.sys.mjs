/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The external API exported by this module.
 */
export var SessionStoreHelper = Object.freeze({
  buildRestoreData(formdata, scroll) {
    return SessionStoreHelperInternal.buildRestoreData(formdata, scroll);
  },
});

/**
 * The internal API for the SessionStoreHelper module.
 */
var SessionStoreHelperInternal = {
  /**
   * Builds a single nsISessionStoreRestoreData tree for the provided |formdata|
   * and |scroll| trees.
   */
  buildRestoreData(formdata, scroll) {
    function addFormEntries(root, fields, isXpath) {
      for (let [key, value] of Object.entries(fields)) {
        switch (typeof value) {
          case "string":
            root.addTextField(isXpath, key, value);
            break;
          case "boolean":
            root.addCheckbox(isXpath, key, value);
            break;
          case "object": {
            if (value === null) {
              break;
            }
            if (
              value.hasOwnProperty("type") &&
              value.hasOwnProperty("fileList")
            ) {
              root.addFileList(isXpath, key, value.type, value.fileList);
              break;
            }
            if (
              value.hasOwnProperty("selectedIndex") &&
              value.hasOwnProperty("value")
            ) {
              root.addSingleSelect(
                isXpath,
                key,
                value.selectedIndex,
                value.value
              );
              break;
            }
            if (
              value.hasOwnProperty("value") &&
              value.hasOwnProperty("state")
            ) {
              root.addCustomElement(isXpath, key, value.value, value.state);
              break;
            }
            if (
              key === "sessionData" &&
              ["about:sessionrestore", "about:welcomeback"].includes(
                formdata.url
              )
            ) {
              root.addTextField(isXpath, key, JSON.stringify(value));
              break;
            }
            if (Array.isArray(value)) {
              root.addMultipleSelect(isXpath, key, value);
              break;
            }
          }
        }
      }
    }

    let root = SessionStoreUtils.constructSessionStoreRestoreData();
    if (scroll?.hasOwnProperty("scroll")) {
      root.scroll = scroll.scroll;
    }
    if (formdata?.hasOwnProperty("url")) {
      root.url = formdata.url;
      if (formdata.hasOwnProperty("innerHTML")) {
        // eslint-disable-next-line no-unsanitized/property
        root.innerHTML = formdata.innerHTML;
      }
      if (formdata.hasOwnProperty("xpath")) {
        addFormEntries(root, formdata.xpath, /* isXpath */ true);
      }
      if (formdata.hasOwnProperty("id")) {
        addFormEntries(root, formdata.id, /* isXpath */ false);
      }
    }
    let childrenLength = Math.max(
      scroll?.children?.length || 0,
      formdata?.children?.length || 0
    );
    for (let i = 0; i < childrenLength; i++) {
      root.addChild(
        this.buildRestoreData(formdata?.children?.[i], scroll?.children?.[i]),
        i
      );
    }
    return root;
  },
};
