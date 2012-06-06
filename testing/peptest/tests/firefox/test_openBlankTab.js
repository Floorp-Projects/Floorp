/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Initialize mozmill
Components.utils.import('resource://mozmill/driver/mozmill.js');
let c = getBrowserController();

c.open('http://mozilla.org');
c.waitForPageLoad();

// Only put things you want to test for responsiveness inside a perfom action call
pep.performAction('open_blank_tab', function() {
  c.rootElement.keypress('t', {'accelKey': true});
});

pep.performAction('close_blank_tab', function() {
  c.rootElement.keypress('w', {'accelKey': true});
});
