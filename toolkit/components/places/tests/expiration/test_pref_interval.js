/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
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
 * The Original Code is Places Unit Tests.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@bonardo.net> (Original Author)
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

/**
 * What this is aimed to test:
 *
 * Expiration relies on an interval, that is user-preffable setting
 * "places.history.expiration.interval_seconds".
 * On pref change it will stop current interval timer and fire a new one,
 * that will obey the new value.
 * If the pref is set to a number <= 0 we will use the default value.
 */

const TOPIC_EXPIRATION_FINISHED = "places-expiration-finished";
const MAX_WAIT_SECONDS = 4;
const INTERVAL_CUSHION = 2;

let os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);

let gTests = [

  // This test should be the first, so the interval won't be influenced by
  // status of history.
  { desc: "Set interval to 1s.",
    interval: 1,
    expectedNotification: true,
  },

  { desc: "Set interval to a negative value.",
    interval: -1,
    expectedNotification: false, // Will ignore.
  },

  { desc: "Set interval to 0.",
    interval: 0,
    expectedNotification: false, // Will ignore.
  },

  { desc: "Set interval to a large value.",
    interval: 100,
    expectedNotification: false, // Won't wait so long.
  },
];

let gCurrentTest;

function run_test() {
  // The pref should not exist by default.
  try {
    getInterval();
    do_throw("interval pref should not exist by default");
  }
  catch (ex) {}

  // Force the component, so it will start observing preferences.
  force_expiration_start();

  run_next_test();
  do_test_pending();
}

function run_next_test() {
  if (gTests.length) {
    gCurrentTest = gTests.shift();
    print(gCurrentTest.desc);
    gCurrentTest.receivedNotification = false;
    gCurrentTest.observer = {
      observe: function(aSubject, aTopic, aData) {
        gCurrentTest.receivedNotification = true;
      }
    };
    os.addObserver(gCurrentTest.observer, TOPIC_EXPIRATION_FINISHED, false);
    setInterval(gCurrentTest.interval);
    let waitSeconds = Math.min(MAX_WAIT_SECONDS,
                               gCurrentTest.interval + INTERVAL_CUSHION);
    do_timeout(waitSeconds * 1000, check_result);
  }
  else {
    clearInterval();
    do_test_finished();
  }
}

function check_result() {
  os.removeObserver(gCurrentTest.observer, TOPIC_EXPIRATION_FINISHED);

  do_check_eq(gCurrentTest.receivedNotification,
              gCurrentTest.expectedNotification);

  run_next_test();
}
