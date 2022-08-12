/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests several things.
//
// 1.  We verify that we can forcefully opt-in to (particular branches of)
//     experiments, that the resulting Firefox Messaging Experiment applies, and
//     that the Firefox Messaging System respects lifetime frequency caps.
// 2.  We verify that Nimbus randomization works with specific Normandy
//     randomization IDs.
// 3.  We verify that relevant opt-out prefs disable the Nimbus and Firefox
//     Messaging System experience.

const { ASRouterTargeting } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTargeting.jsm"
);

setupProfileService();

let taskProfile;

// Arrange a dummy Remote Settings server so that no non-local network
// connections are opened.
// And arrange dummy task profile.
add_setup(() => {
  info("Setting up profile service");
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );

  let taskProfD = do_get_profile();
  taskProfD.append("test_backgroundtask_experiments_task");
  taskProfile = profileService.createUniqueProfile(
    taskProfD,
    "test_backgroundtask_experiments_task"
  );

  registerCleanupFunction(() => {
    taskProfile.remove(true);
  });
});

function resetProfile(profile) {
  profile.rootDir.remove(true);
  profile.rootDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o700);
  info(`Reset profile '${profile.rootDir.path}'`);
}

// Run the "message" background task with some default configuration.  Return
// "info" items output from the task as an array and as a map.
async function doMessage({ extraArgs = [], extraEnv = {} } = {}) {
  let sentinel = Services.uuid.generateUUID().toString();
  sentinel = sentinel.substring(1, sentinel.length - 1);

  let infoArray = [];
  let exitCode = await do_backgroundtask("message", {
    extraArgs: [
      "--sentinel",
      sentinel,
      // Use a test-specific non-ephemeral profile, not the system-wide shared
      // task-specific profile.
      "--profile",
      taskProfile.rootDir.path,
      // Don't actually show a toast notification.
      "--disable-alerts-service",
      // Don't contact Remote Settings server.  Can be overridden by subsequent
      // `--experiments ...`.
      "--no-experiments",
      ...extraArgs,
    ],
    extraEnv: {
      MOZ_LOG: "Dump:5,BackgroundTasks:5",
      ...extraEnv,
    },
    onStdoutLine: line => {
      if (line.includes(sentinel)) {
        let info = JSON.parse(line.split(sentinel)[1]);
        infoArray.push(info);
      }
    },
  });

  Assert.equal(
    0,
    exitCode,
    "The message background task exited with exit code 0"
  );

  // Turn [{x:...}, {y:...}] into {x:..., y:...}.
  let infoMap = Object.assign({}, ...infoArray);

  return { infoArray, infoMap };
}

// Opt-in to an experiment.  Verify that the experiment state is non-ephemeral,
// i.e., persisted.  Verify that messages are shown until we hit the lifetime
// frequency caps.
//
// It's awkward to inspect the `ASRouter.jsm` internal state directly in this
// manner, but this is the pattern for testing such things at the time of
// writing.
add_task(async function test_backgroundtask_caps() {
  let experimentFile = do_get_file("experiment.json");
  let experimentFileURI = Services.io.newFileURI(experimentFile);

  let { infoMap } = await doMessage({
    extraArgs: [
      // Opt-in to an experiment from a file.
      "--url",
      `${experimentFileURI.spec}?optin_branch=treatment-a`,
    ],
  });

  // Verify that the correct experiment and branch generated an impression.
  let impressions = infoMap.ASRouterState.messageImpressions;
  Assert.deepEqual(Object.keys(impressions), ["test-experiment:treatment-a"]);
  Assert.equal(impressions["test-experiment:treatment-a"].length, 1);

  // Verify that the correct toast notification was shown.
  let alert = infoMap.showAlert.args[0];
  Assert.equal(alert.title, "Treatment A");
  Assert.equal(alert.text, "Body A");

  // Now, do it again.  No need to opt-in to the experiment this time.
  ({ infoMap } = await doMessage({}));

  // Verify that only the correct experiment and branch generated an impression.
  impressions = infoMap.ASRouterState.messageImpressions;
  Assert.deepEqual(Object.keys(impressions), ["test-experiment:treatment-a"]);
  Assert.equal(impressions["test-experiment:treatment-a"].length, 2);

  // Verify that the correct toast notification was shown.
  alert = infoMap.showAlert.args[0];
  Assert.equal(alert.title, "Treatment A");
  Assert.equal(alert.text, "Body A");

  // A third time.  We'll hit the lifetime frequency cap (which is 2).
  ({ infoMap } = await doMessage({}));

  // Verify that the correct experiment and branch impressions are untouched.
  impressions = infoMap.ASRouterState.messageImpressions;
  Assert.deepEqual(Object.keys(impressions), ["test-experiment:treatment-a"]);
  Assert.equal(impressions["test-experiment:treatment-a"].length, 2);

  // Verify that no toast notication was shown.
  Assert.ok(!("showAlert" in infoMap), "No alert shown");
});

// Test that background tasks are enrolled into branches based on the Normandy
// randomization ID as expected.  Run the message task with a hard-coded list of
// a single experiment and known randomization IDs, and verify that the enrolled
// branches are as expected.
add_task(async function test_backgroundtask_randomization() {
  let experimentFile = do_get_file("experiment.json");

  // These randomization IDs were extracted by hand from Firefox instances.
  // Randomization is sufficiently stable to hard-code these IDs rather than
  // generating new ones at test time.
  let BRANCH_MAP = {
    "treatment-a": {
      randomizationId: "d0e95fc3-fb15-4bc4-8151-a89582a56e29",
      title: "Treatment A",
      text: "Body A",
    },
    "treatment-b": {
      randomizationId: "90a60347-66cc-4716-9fef-cf49dd992d51",
      title: "Treatment B",
      text: "Body B",
    },
  };

  for (let [branchSlug, branchDetails] of Object.entries(BRANCH_MAP)) {
    // Start fresh each time.
    resetProfile(taskProfile);

    // Invoke twice; verify the branch is consistent each time.
    for (let count = 1; count <= 2; count++) {
      let { infoMap } = await doMessage({
        extraArgs: [
          // Read experiments from a file.
          "--experiments",
          experimentFile.path,
          // Fixed randomization ID yields a deterministic enrollment branch assignment.
          "--randomizationId",
          branchDetails.randomizationId,
        ],
      });

      // Verify that only the correct experiment and branch generated an impression.
      let impressions = infoMap.ASRouterState.messageImpressions;
      Assert.deepEqual(Object.keys(impressions), [
        `test-experiment:${branchSlug}`,
      ]);
      Assert.equal(impressions[`test-experiment:${branchSlug}`].length, count);

      // Verify that the correct toast notification was shown.
      let alert = infoMap.showAlert.args[0];
      Assert.equal(alert.title, branchDetails.title, "Title is correct");
      Assert.equal(alert.text, branchDetails.text, "Text is correct");
    }
  }
});

// Test that background tasks respect the datareporting and studies opt-out
// preferences.
add_task(async function test_backgroundtask_optout_preferences() {
  let experimentFile = do_get_file("experiment.json");

  let OPTION_MAP = {
    "--no-datareporting": {
      "datareporting.healthreport.uploadEnabled": false,
      "app.shield.optoutstudies.enabled": true,
    },
    "--no-optoutstudies": {
      "datareporting.healthreport.uploadEnabled": true,
      "app.shield.optoutstudies.enabled": false,
    },
  };

  for (let [option, expectedPrefs] of Object.entries(OPTION_MAP)) {
    // Start fresh each time.
    resetProfile(taskProfile);

    let { infoMap } = await doMessage({
      extraArgs: [
        option,
        // Read experiments from a file.  Opting in to an experiment with
        // `--url` does not consult relevant preferences.
        "--experiments",
        experimentFile.path,
      ],
    });

    Assert.deepEqual(infoMap.taskProfilePrefs, expectedPrefs);

    // Verify that no experiment generated an impression.
    let impressions = infoMap.ASRouterState.messageImpressions;
    Assert.deepEqual(
      impressions,
      [],
      `No impressions generated with ${option}`
    );

    // Verify that no toast notication was shown.
    Assert.ok(!("showAlert" in infoMap), `No alert shown with ${option}`);
  }
});

const TARGETING_LIST = [
  // Target based on background task details.
  ["isBackgroundTaskMode", 1],
  ["backgroundTaskName == 'message'", 1],
  ["backgroundTaskName == 'unrecognized'", 0],
  // Filter based on `defaultProfile` targeting snapshot.
  ["(currentDate|date - defaultProfile.currentDate|date) > 0", 1],
  ["(currentDate|date - defaultProfile.currentDate|date) > 999999", 0],
];

// Test that background tasks targeting works for Nimbus experiments.
add_task(async function test_backgroundtask_Nimbus_targeting() {
  let experimentFile = do_get_file("experiment.json");
  let experimentData = await IOUtils.readJSON(experimentFile.path);

  // We can't take a full environment snapshot under `xpcshell`.  Select a few
  // items that do work.
  let target = {
    currentDate: ASRouterTargeting.Environment.currentDate,
    firefoxVersion: ASRouterTargeting.Environment.firefoxVersion,
  };
  let targetSnapshot = await ASRouterTargeting.getEnvironmentSnapshot(target);

  for (let [targeting, expectedLength] of TARGETING_LIST) {
    // Start fresh each time.
    resetProfile(taskProfile);

    let snapshotFile = taskProfile.rootDir.clone();
    snapshotFile.append("targeting.snapshot.json");
    await IOUtils.writeJSON(snapshotFile.path, targetSnapshot);

    // Write updated experiment data.
    experimentData.data.targeting = targeting;
    let targetingExperimentFile = taskProfile.rootDir.clone();
    targetingExperimentFile.append("targeting.experiment.json");
    await IOUtils.writeJSON(targetingExperimentFile.path, experimentData);

    let { infoMap } = await doMessage({
      extraArgs: [
        "--experiments",
        targetingExperimentFile.path,
        "--targeting-snapshot",
        snapshotFile.path,
      ],
    });

    // Verify that the given targeting generated the expected number of impressions.
    let impressions = infoMap.ASRouterState.messageImpressions;
    Assert.equal(
      Object.keys(impressions).length,
      expectedLength,
      `${expectedLength} impressions generated with targeting '${targeting}'`
    );
  }
});

// Test that background tasks targeting works for Firefox Messaging System branches.
add_task(async function test_backgroundtask_Messaging_targeting() {
  // Don't target the Nimbus experiment at all.  Use a consistent
  // randomization ID to always enroll in the first branch.  Target
  // the first branch of the Firefox Messaging Experiment to the given
  // targeting.  Therefore, we either get the first branch if the
  // targeting matches, or nothing at all.

  // This randomization ID was extracted by hand from a Firefox instance.
  let treatmentARandomizationId = "d0e95fc3-fb15-4bc4-8151-a89582a56e29";

  let experimentFile = do_get_file("experiment.json");
  let experimentData = await IOUtils.readJSON(experimentFile.path);

  // We can't take a full environment snapshot under `xpcshell`.  Select a few
  // items that do work.
  let target = {
    currentDate: ASRouterTargeting.Environment.currentDate,
    firefoxVersion: ASRouterTargeting.Environment.firefoxVersion,
  };
  let targetSnapshot = await ASRouterTargeting.getEnvironmentSnapshot(target);

  for (let [targeting, expectedLength] of TARGETING_LIST) {
    // Start fresh each time.
    resetProfile(taskProfile);

    let snapshotFile = taskProfile.rootDir.clone();
    snapshotFile.append("targeting.snapshot.json");
    await IOUtils.writeJSON(snapshotFile.path, targetSnapshot);

    // Write updated experiment data.
    experimentData.data.targeting = "true";
    experimentData.data.branches[0].features[0].value.targeting = targeting;

    let targetingExperimentFile = taskProfile.rootDir.clone();
    targetingExperimentFile.append("targeting.experiment.json");
    await IOUtils.writeJSON(targetingExperimentFile.path, experimentData);

    let { infoMap } = await doMessage({
      extraArgs: [
        "--experiments",
        targetingExperimentFile.path,
        "--targeting-snapshot",
        snapshotFile.path,
        "--randomizationId",
        treatmentARandomizationId,
      ],
    });

    // Verify that the given targeting generated the expected number of impressions.
    let impressions = infoMap.ASRouterState.messageImpressions;
    Assert.equal(
      Object.keys(impressions).length,
      expectedLength,
      `${expectedLength} impressions generated with targeting '${targeting}'`
    );
  }
});
