/* vim:set ts=2 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function onButtonClick() {
  var textbox = document.getElementById("textbox");

  var contractid = (textbox.value % 2 == 0) ?
      "@test.mozilla.org/simple-test;1?impl=js" :
      "@test.mozilla.org/simple-test;1?impl=c++";

  var test = Components.classes[contractid].
      createInstance(Components.interfaces.nsISimpleTest);

  textbox.value = test.add(textbox.value, 1);
}
