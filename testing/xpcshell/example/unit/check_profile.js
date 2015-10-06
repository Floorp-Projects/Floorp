/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var {classes: Cc, interfaces: Ci} = Components;

function check_profile_dir(profd)
{
  Assert.ok(profd.exists());
  Assert.ok(profd.isDirectory());
  let dirSvc = Cc["@mozilla.org/file/directory_service;1"]
                 .getService(Ci.nsIProperties);
  let profd2 = dirSvc.get("ProfD", Ci.nsILocalFile);
  Assert.ok(profd2.exists());
  Assert.ok(profd2.isDirectory());
  // make sure we got the same thing back...
  Assert.ok(profd.equals(profd2));
}

function check_do_get_profile(fireProfileAfterChange)
{
  const observedTopics = new Map([
    ["profile-do-change", 0],
    ["profile-after-change", 0],
  ]);
  const expectedTopics = new Map(observedTopics);

  const obs = Cc["@mozilla.org/observer-service;1"]
                   .getService(Ci.nsIObserverService);
  for (let [topic,] of observedTopics) {
    obs.addObserver(() => {
      let val = observedTopics.get(topic) + 1;
      observedTopics.set(topic, val);
    }, topic, false);
  }

  // Trigger profile creation.
  let profd = do_get_profile();
  check_profile_dir(profd);

  // Check the observed topics
  expectedTopics.set("profile-do-change", 1);
  if (fireProfileAfterChange) {
    expectedTopics.set("profile-after-change", 1);
  }
  Assert.deepEqual(observedTopics, expectedTopics);

  // A second do_get_profile() should not trigger more notifications.
  profd = do_get_profile();
  check_profile_dir(profd);
  Assert.deepEqual(observedTopics, expectedTopics);
}
