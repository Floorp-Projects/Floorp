# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import sys

from marionette_driver import Wait
from six import reraise


class WindowManagerMixin(object):
    def setUp(self):
        super(WindowManagerMixin, self).setUp()

        self.start_window = self.marionette.current_chrome_window_handle
        self.start_windows = self.marionette.chrome_window_handles

        self.start_tab = self.marionette.current_window_handle
        self.start_tabs = self.marionette.window_handles

    def tearDown(self):
        if len(self.marionette.chrome_window_handles) > len(self.start_windows):
            raise Exception("Not all windows as opened by the test have been closed")

        if len(self.marionette.window_handles) > len(self.start_tabs):
            raise Exception("Not all tabs as opened by the test have been closed")

        super(WindowManagerMixin, self).tearDown()

    def close_all_tabs(self):
        current_window_handles = self.marionette.window_handles

        # If the start tab is not present anymore, use the next one of the list
        if self.start_tab not in current_window_handles:
            self.start_tab = current_window_handles[0]

        current_window_handles.remove(self.start_tab)
        for handle in current_window_handles:
            self.marionette.switch_to_window(handle)
            self.marionette.close()

        self.marionette.switch_to_window(self.start_tab)

    def close_all_windows(self):
        current_chrome_window_handles = self.marionette.chrome_window_handles

        # If the start window is not present anymore, use the next one of the list
        if self.start_window not in current_chrome_window_handles:
            self.start_window = current_chrome_window_handles[0]
        current_chrome_window_handles.remove(self.start_window)

        for handle in current_chrome_window_handles:
            self.marionette.switch_to_window(handle)
            self.marionette.close_chrome_window()

        self.marionette.switch_to_window(self.start_window)

    def open_tab(self, callback=None, focus=False):
        current_tabs = self.marionette.window_handles

        try:
            if callable(callback):
                callback()
            else:
                result = self.marionette.open(type="tab", focus=focus)
                if result["type"] != "tab":
                    raise Exception(
                        "Newly opened browsing context is of type {} and not tab.".format(
                            result["type"]
                        )
                    )
        except Exception:
            exc_cls, exc, tb = sys.exc_info()
            reraise(
                exc_cls,
                exc_cls("Failed to trigger opening a new tab: {}".format(exc)),
                tb,
            )
        else:
            Wait(self.marionette).until(
                lambda mn: len(mn.window_handles) == len(current_tabs) + 1,
                message="No new tab has been opened",
            )

            [new_tab] = list(set(self.marionette.window_handles) - set(current_tabs))

            return new_tab

    def open_window(self, callback=None, focus=False, private=False):
        current_windows = self.marionette.chrome_window_handles
        current_tabs = self.marionette.window_handles

        def loaded(handle):
            with self.marionette.using_context("chrome"):
                return self.marionette.execute_script(
                    """
                  const { windowManager } = ChromeUtils.import(
                    "chrome://remote/content/shared/WindowManager.jsm"
                  );
                  const win = windowManager.findWindowByHandle(arguments[0]).win;
                  return win.document.readyState == "complete";
                """,
                    script_args=[handle],
                )

        try:
            if callable(callback):
                callback(focus)
            else:
                result = self.marionette.open(
                    type="window", focus=focus, private=private
                )
                if result["type"] != "window":
                    raise Exception(
                        "Newly opened browsing context is of type {} and not window.".format(
                            result["type"]
                        )
                    )
        except Exception:
            exc_cls, exc, tb = sys.exc_info()
            reraise(
                exc_cls,
                exc_cls("Failed to trigger opening a new window: {}".format(exc)),
                tb,
            )
        else:
            Wait(self.marionette).until(
                lambda mn: len(mn.chrome_window_handles) == len(current_windows) + 1,
                message="No new window has been opened",
            )

            [new_window] = list(
                set(self.marionette.chrome_window_handles) - set(current_windows)
            )

            # Before continuing ensure the window has been completed loading
            Wait(self.marionette).until(
                lambda _: loaded(new_window),
                message="Window with handle '{}'' did not finish loading".format(
                    new_window
                ),
            )

            # Bug 1507771 - Return the correct handle based on the currently selected context
            # as long as "WebDriver:NewWindow" is not handled separtely in chrome context
            context = self.marionette._send_message(
                "Marionette:GetContext", key="value"
            )
            if context == "chrome":
                return new_window
            elif context == "content":
                [new_tab] = list(
                    set(self.marionette.window_handles) - set(current_tabs)
                )
                return new_tab

    def open_chrome_window(self, url, focus=False):
        """Open a new chrome window with the specified chrome URL.

        Can be replaced with "WebDriver:NewWindow" once the command
        supports opening generic chrome windows beside browsers (bug 1507771).
        """

        def open_with_js(focus):
            with self.marionette.using_context("chrome"):
                self.marionette.execute_async_script(
                    """
                  let [url, focus, resolve] = arguments;

                  function waitForEvent(target, type, args) {
                    return new Promise(resolve => {
                      let params = Object.assign({once: true}, args);
                      target.addEventListener(type, event => {
                        dump(`** Received DOM event ${event.type} for ${event.target}\n`);
                        resolve();
                      }, params);
                    });
                  }

                  function waitForFocus(win) {
                    return Promise.all([
                      waitForEvent(win, "activate"),
                      waitForEvent(win, "focus", {capture: true}),
                    ]);
                  }

                  (async function() {
                    // Open a window, wait for it to receive focus
                    let win = window.openDialog(url, null, "chrome,centerscreen");
                    let focused = waitForFocus(win);

                    win.focus();
                    await focused;

                    // The new window shouldn't get focused. As such set the
                    // focus back to the opening window.
                    if (!focus && Services.focus.activeWindow != window) {
                      let focused = waitForFocus(window);
                      window.focus();
                      await focused;
                    }

                    resolve(win.docShell.browsingContext.id);
                  })();
                """,
                    script_args=(url, focus),
                )

        with self.marionette.using_context("chrome"):
            return self.open_window(callback=open_with_js, focus=focus)
