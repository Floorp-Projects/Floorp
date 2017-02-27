/* eslint-env mozilla/frame-script */
// assert is available to chrome scripts loaded via SpecialPowers.loadChromeScript.
/* global assert */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/FormHistory.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/ContentTaskUtils.jsm");

var gAutocompletePopup = Services.ww.activeWindow.
                                   document.
                                   getElementById("PopupAutoComplete");
assert.ok(gAutocompletePopup, "Got autocomplete popup");

var ParentUtils = {
  getMenuEntries() {
    let entries = [];
    let numRows = gAutocompletePopup.view.matchCount;
    for (let i = 0; i < numRows; i++) {
      entries.push(gAutocompletePopup.view.getValueAt(i));
    }
    return entries;
  },

  cleanUpFormHist() {
    FormHistory.update({ op: "remove" });
  },

  updateFormHistory(changes) {
    let handler = {
      handleError(error) {
        assert.ok(false, error);
        sendAsyncMessage("formHistoryUpdated", { ok: false });
      },
      handleCompletion(reason) {
        if (!reason)
          sendAsyncMessage("formHistoryUpdated", { ok: true });
      },
    };
    FormHistory.update(changes, handler);
  },

  popupshownListener() {
    let results = this.getMenuEntries();
    sendAsyncMessage("onpopupshown", { results });
  },

  countEntries(name, value) {
    let obj = {};
    if (name)
      obj.fieldname = name;
    if (value)
      obj.value = value;

    let count = 0;
    let listener = {
      handleResult(result) { count = result },
      handleError(error) {
        assert.ok(false, error);
        sendAsyncMessage("entriesCounted", { ok: false });
      },
      handleCompletion(reason) {
        if (!reason) {
          sendAsyncMessage("entriesCounted", { ok: true, count });
        }
      }
    };

    FormHistory.count(obj, listener);
  },

  checkRowCount(expectedCount, expectedFirstValue = null) {
    ContentTaskUtils.waitForCondition(() => {
      // This may be called before gAutocompletePopup has initialised
      // which causes it to throw
      try {
        return gAutocompletePopup.view.matchCount === expectedCount &&
          (!expectedFirstValue ||
           expectedCount <= 1 ||
           gAutocompletePopup.view.getValueAt(0) === expectedFirstValue);
      } catch (e) {
        return false;
      }
    }, "Waiting for row count change: " + expectedCount + " First value: " + expectedFirstValue).then(() => {
      let results = this.getMenuEntries();
      sendAsyncMessage("gotMenuChange", { results });
    });
  },

  checkSelectedIndex(expectedIndex) {
    ContentTaskUtils.waitForCondition(() => {
      return gAutocompletePopup.popupOpen &&
             gAutocompletePopup.selectedIndex === expectedIndex;
    }, "Checking selected index").then(() => {
      sendAsyncMessage("gotSelectedIndex");
    });
  },

  getPopupState() {
    sendAsyncMessage("gotPopupState", {
      open: gAutocompletePopup.popupOpen,
      selectedIndex: gAutocompletePopup.selectedIndex,
      direction: gAutocompletePopup.style.direction,
    });
  },

  observe(subject, topic, data) {
    assert.ok(topic === "satchel-storage-changed");
    sendAsyncMessage("satchel-storage-changed", { subject: null, topic, data });
  },

  cleanup() {
    gAutocompletePopup.removeEventListener("popupshown", this._popupshownListener);
    this.cleanUpFormHist();
  }
};

ParentUtils._popupshownListener =
  ParentUtils.popupshownListener.bind(ParentUtils);
gAutocompletePopup.addEventListener("popupshown", ParentUtils._popupshownListener);
ParentUtils.cleanUpFormHist();

addMessageListener("updateFormHistory", (msg) => {
  ParentUtils.updateFormHistory(msg.changes);
});

addMessageListener("countEntries", ({ name, value }) => {
  ParentUtils.countEntries(name, value);
});

addMessageListener("waitForMenuChange", ({ expectedCount, expectedFirstValue }) => {
  ParentUtils.checkRowCount(expectedCount, expectedFirstValue);
});

addMessageListener("waitForSelectedIndex", ({ expectedIndex }) => {
  ParentUtils.checkSelectedIndex(expectedIndex);
});

addMessageListener("getPopupState", () => {
  ParentUtils.getPopupState();
});

addMessageListener("addObserver", () => {
  Services.obs.addObserver(ParentUtils, "satchel-storage-changed", false);
});
addMessageListener("removeObserver", () => {
  Services.obs.removeObserver(ParentUtils, "satchel-storage-changed");
});

addMessageListener("cleanup", () => {
  ParentUtils.cleanup();
});
