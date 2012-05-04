/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Most of the other example tests use Mozmill to perform various
// automation. Note that this is not necessary.
//
// This test will check responsiveness while resizing the window

let window = pep.getWindow();
let width = window.outerWidth;
let height = window.outerHeight;

pep.performAction('resize_by', function() {
  window.resizeBy(100, 100);
});

pep.performAction('resize_to', function() {
  window.resizeTo(800, 600);
});

// Tests should clean up after themselves
window.resizeTo(width, height);
