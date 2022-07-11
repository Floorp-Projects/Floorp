/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { modal } = ChromeUtils.import(
  "chrome://remote/content/marionette/modal.js"
);

const chromeWindow = {};

const mockModalDialog = {
  docShell: {
    chromeEventHandler: null,
  },
  opener: {
    ownerGlobal: chromeWindow,
  },
  Dialog: {
    args: {
      modalType: Services.prompt.MODAL_TYPE_WINDOW,
    },
  },
};

const mockCurBrowser = {
  window: chromeWindow,
};

add_test(function test_addCallback() {
  let observer = new modal.DialogObserver(() => mockCurBrowser);
  let cb1 = () => true;
  let cb2 = () => false;

  equal(observer.callbacks.size, 0);
  observer.add(cb1);
  equal(observer.callbacks.size, 1);
  observer.add(cb1);
  equal(observer.callbacks.size, 1);
  observer.add(cb2);
  equal(observer.callbacks.size, 2);

  run_next_test();
});

add_test(function test_removeCallback() {
  let observer = new modal.DialogObserver(() => mockCurBrowser);
  let cb1 = () => true;
  let cb2 = () => false;

  equal(observer.callbacks.size, 0);
  observer.add(cb1);
  observer.add(cb2);

  equal(observer.callbacks.size, 2);
  observer.remove(cb1);
  equal(observer.callbacks.size, 1);
  observer.remove(cb1);
  equal(observer.callbacks.size, 1);
  observer.remove(cb2);
  equal(observer.callbacks.size, 0);

  run_next_test();
});

add_test(function test_registerDialogClosedEventHandler() {
  let observer = new modal.DialogObserver(() => mockCurBrowser);
  let mockChromeWindow = {
    addEventListener(event, cb) {
      equal(
        event,
        "DOMModalDialogClosed",
        "registered event for closing modal"
      );
      equal(cb, observer, "set itself as handler");
      run_next_test();
    },
  };

  observer.observe(mockChromeWindow, "domwindowopened");
});

add_test(function test_handleCallbackOpenModalDialog() {
  let observer = new modal.DialogObserver(() => mockCurBrowser);

  observer.add((action, dialog) => {
    equal(action, modal.ACTION_OPENED, "'opened' action has been passed");
    equal(dialog, mockModalDialog, "dialog has been passed");
    run_next_test();
  });
  observer.observe(mockModalDialog, "common-dialog-loaded");
});

add_test(function test_handleCallbackCloseModalDialog() {
  let observer = new modal.DialogObserver(() => mockCurBrowser);

  observer.add((action, dialog) => {
    equal(action, modal.ACTION_CLOSED, "'closed' action has been passed");
    equal(dialog, mockModalDialog, "dialog has been passed");
    run_next_test();
  });
  observer.handleEvent({
    type: "DOMModalDialogClosed",
    target: mockModalDialog,
  });
});

add_test(function test_dialogClosed() {
  let observer = new modal.DialogObserver(() => mockCurBrowser);

  observer.dialogClosed().then(() => {
    run_next_test();
  });
  observer.handleEvent({
    type: "DOMModalDialogClosed",
    target: mockModalDialog,
  });
});
