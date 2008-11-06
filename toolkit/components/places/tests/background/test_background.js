/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

Components.utils.import("resource://gre/modules/PlacesBackground.jsm");

function test_service_exists()
{
  print("begin test_service_exists");
  do_check_neq(PlacesBackground, null);
  print("end test_service_exists");
}

function test_isOnCurrentThread()
{
  print("begin test_isOnCurrentThread");
  do_check_false(PlacesBackground.isOnCurrentThread());

  let event = {
    run: function()
    {
      do_check_true(PlacesBackground.isOnCurrentThread());
    }
  };
  PlacesBackground.dispatch(event, Ci.nsIEventTarget.DISPATCH_SYNC);
  print("end test_isOnCurrentThread");
}

function test_two_events_same_thread()
{
   print("begin test_two_events_same_thread");
  // This test imports PlacesBackground.jsm onto two different objects to
  // ensure that the thread is the same for both.
  let event = {
    run: function()
    {
      let tm = Cc["@mozilla.org/thread-manager;1"].
               getService(Ci.nsIThreadManager);

      if (!this.thread1)
        this.thread1 = tm.currentThread;
      else
        this.thread2 = tm.currentThread;
    }
  };

  let obj1 = { };
  Components.utils.import("resource://gre/modules/PlacesBackground.jsm", obj1);
  obj1.PlacesBackground.dispatch(event, Ci.nsIEventTarget.DISPATCH_SYNC);
  let obj2 = { };
  Components.utils.import("resource://gre/modules/PlacesBackground.jsm", obj2);
  obj2.PlacesBackground.dispatch(event, Ci.nsIEventTarget.DISPATCH_SYNC);
  do_check_eq(event.thread1, event.thread2);
  print("end test_two_events_same_thread");
}

function test_places_background_shutdown_topic()
{
  print("begin test_places_background_shutdown_topic");
  // Ensures that the places shutdown topic is dispatched before the thread is
  // shutdown.
  let os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.addObserver({
    observe: function(aSubject, aTopic, aData)
    {
      // We should still be able to dispatch an event without throwing now!
      PlacesBackground.dispatch({
        run: function()
        {
          do_test_finished();
        }
      }, Ci.nsIEventTarget.DISPATCH_NORMAL);
    }
  }, "places-background-shutdown", false);
  do_test_pending();
  print("end test_places_background_shutdown_topic");
}

let tests = [
  test_service_exists,
  test_isOnCurrentThread,
  test_two_events_same_thread,
  test_places_background_shutdown_topic,
];

function run_test()
{
  print("begin run_test");
  for (let i = 0; i < tests.length; i++)
    tests[i]();

  print("dispatch shutdown");
  // xpcshell doesn't dispatch shutdown-application
  let os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.notifyObservers(null, "quit-application", null);
  print("end run_test");
}
