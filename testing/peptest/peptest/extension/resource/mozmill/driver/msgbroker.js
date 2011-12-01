/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozmill.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 *
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Andrew Halberstadt <halbersa@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var EXPORTED_SYMBOLS = ['addListener', 'addObject',
                        'removeListener',
                        'sendMessage', 'log', 'pass', 'fail'];

var listeners = {};

// add a listener for a specific message type
function addListener(msgType, listener) {

  if (listeners[msgType] === undefined) {
    listeners[msgType] = [];
  }
  listeners[msgType].push(listener);
}

// add each method in an object as a message listener
function addObject(object) {
  for (var msgType in object) {
    addListener(msgType, object[msgType]);
  }
}

// remove a listener for all message types
function removeListener(listener) {
  for (var msgType in listeners) {
    for (let i = 0; i < listeners.length; ++i) {
      if (listeners[msgType][i] == listener) {
        listeners[msgType].splice(i, 1); // remove listener from array
      }
    }
  }
}

function sendMessage(msgType, obj) {
  if (listeners[msgType] === undefined) {
    return;
  }
  for (let i = 0; i < listeners[msgType].length; ++i) {
    listeners[msgType][i](obj);
  }
}

function log(obj) {
  sendMessage('log', obj);
}

function pass(obj) {
  sendMessage('pass', obj);
}

function fail(obj) {
  sendMessage('fail', obj);
}
