/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var keygenThread;

function onLoad()
{
  keygenThread = window.arguments[0].QueryInterface(Components.interfaces.nsIKeygenThread);
  
  if (!keygenThread) {
    window.close();
    return;
  }
  
  setCursor("wait");

  var obs = {
    observe : function keygenListenerObserve(subject, topic, data) {
      if (topic == "keygen-finished")
        window.close();
    }
  };
  
  keygenThread.startKeyGeneration(obs);
}

function onClose()
{
  setCursor("default");

  var alreadyClosed = {};
  keygenThread.userCanceled(alreadyClosed);
}
