/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://mozmill/driver/mozmill.js");
let controller = getBrowserController();

let win = findElement.ID(controller.window.document, 'main-window');
pep.performAction("open_window", function() {
  win.keypress("n", {"ctrlKey":true});
});

pep.getWindow().close();
