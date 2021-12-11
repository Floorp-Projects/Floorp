/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

let getExtension = () => {
  return ExtensionTestUtils.loadExtension({
    background: async function() {
      const runningListener = isRunning => {
        if (isRunning) {
          browser.test.sendMessage("started");
        } else {
          browser.test.sendMessage("stopped");
        }
      };

      browser.test.onMessage.addListener(async (message, data) => {
        let result;
        switch (message) {
          case "start":
            result = await browser.geckoProfiler.start({
              bufferSize: 10000,
              windowLength: 20,
              interval: 0.5,
              features: ["js"],
              threads: ["GeckoMain"],
            });
            browser.test.assertEq(undefined, result, "start returns nothing.");
            break;
          case "stop":
            result = await browser.geckoProfiler.stop();
            browser.test.assertEq(undefined, result, "stop returns nothing.");
            break;
          case "pause":
            result = await browser.geckoProfiler.pause();
            browser.test.assertEq(undefined, result, "pause returns nothing.");
            browser.test.sendMessage("paused");
            break;
          case "resume":
            result = await browser.geckoProfiler.resume();
            browser.test.assertEq(undefined, result, "resume returns nothing.");
            browser.test.sendMessage("resumed");
            break;
          case "test profile":
            result = await browser.geckoProfiler.getProfile();
            browser.test.assertTrue(
              "libs" in result,
              "The profile contains libs."
            );
            browser.test.assertTrue(
              "meta" in result,
              "The profile contains meta."
            );
            browser.test.assertTrue(
              "threads" in result,
              "The profile contains threads."
            );
            browser.test.assertTrue(
              result.threads.some(t => t.name == "GeckoMain"),
              "The profile contains a GeckoMain thread."
            );
            browser.test.sendMessage("tested profile");
            break;
          case "test dump to file":
            try {
              await browser.geckoProfiler.dumpProfileToFile(data.fileName);
              browser.test.sendMessage("tested dump to file", {});
            } catch (e) {
              browser.test.sendMessage("tested dump to file", {
                error: e.message,
              });
            }
            break;
          case "test profile as array buffer":
            let arrayBuffer = await browser.geckoProfiler.getProfileAsArrayBuffer();
            browser.test.assertTrue(
              arrayBuffer.byteLength >= 2,
              "The profile array buffer contains data."
            );
            let textDecoder = new TextDecoder();
            let profile = JSON.parse(textDecoder.decode(arrayBuffer));
            browser.test.assertTrue(
              "libs" in profile,
              "The profile contains libs."
            );
            browser.test.assertTrue(
              "meta" in profile,
              "The profile contains meta."
            );
            browser.test.assertTrue(
              "threads" in profile,
              "The profile contains threads."
            );
            browser.test.assertTrue(
              profile.threads.some(t => t.name == "GeckoMain"),
              "The profile contains a GeckoMain thread."
            );
            browser.test.sendMessage("tested profile as array buffer");
            break;
          case "remove runningListener":
            browser.geckoProfiler.onRunning.removeListener(runningListener);
            browser.test.sendMessage("removed runningListener");
            break;
        }
      });

      browser.test.sendMessage("ready");

      browser.geckoProfiler.onRunning.addListener(runningListener);
    },

    manifest: {
      permissions: ["geckoProfiler"],
      applications: {
        gecko: {
          id: "profilertest@mozilla.com",
        },
      },
    },
  });
};

let verifyProfileData = bytes => {
  let textDecoder = new TextDecoder();
  let profile = JSON.parse(textDecoder.decode(bytes));
  ok("libs" in profile, "The profile contains libs.");
  ok("meta" in profile, "The profile contains meta.");
  ok("threads" in profile, "The profile contains threads.");
  ok(
    profile.threads.some(t => t.name == "GeckoMain"),
    "The profile contains a GeckoMain thread."
  );
};

add_task(async function testProfilerControl() {
  const acceptedExtensionIdsPref =
    "extensions.geckoProfiler.acceptedExtensionIds";
  Services.prefs.setCharPref(
    acceptedExtensionIdsPref,
    "profilertest@mozilla.com"
  );

  let extension = getExtension();
  await extension.startup();
  await extension.awaitMessage("ready");
  await extension.awaitMessage("stopped");

  extension.sendMessage("start");
  await extension.awaitMessage("started");

  extension.sendMessage("test profile");
  await extension.awaitMessage("tested profile");

  const profilerPath = OS.Path.join(OS.Constants.Path.profileDir, "profiler");
  let data, fileName, targetPath;

  // test with file name only
  fileName = "bar.profile";
  targetPath = OS.Path.join(profilerPath, fileName);
  extension.sendMessage("test dump to file", { fileName });
  data = await extension.awaitMessage("tested dump to file");
  equal(data.error, undefined, "No error thrown");
  ok(await OS.File.exists(targetPath), "Saved gecko profile exists.");
  verifyProfileData(await OS.File.read(targetPath));

  // test overwriting the formerly created file
  extension.sendMessage("test dump to file", { fileName });
  data = await extension.awaitMessage("tested dump to file");
  equal(data.error, undefined, "No error thrown");
  ok(await OS.File.exists(targetPath), "Saved gecko profile exists.");
  verifyProfileData(await OS.File.read(targetPath));

  // test with a POSIX path, which is not allowed
  fileName = "foo/bar.profile";
  targetPath = OS.Path.join(profilerPath, ...fileName.split("/"));
  extension.sendMessage("test dump to file", { fileName });
  data = await extension.awaitMessage("tested dump to file");
  equal(data.error, "Path cannot contain a subdirectory.");
  ok(!(await OS.File.exists(targetPath)), "Gecko profile hasn't been saved.");

  // test with a non POSIX path which is not allowed
  fileName = "foo\\bar.profile";
  targetPath = OS.Path.join(profilerPath, ...fileName.split("\\"));
  extension.sendMessage("test dump to file", { fileName });
  data = await extension.awaitMessage("tested dump to file");
  equal(data.error, "Path cannot contain a subdirectory.");
  ok(!(await OS.File.exists(targetPath)), "Gecko profile hasn't been saved.");

  extension.sendMessage("test profile as array buffer");
  await extension.awaitMessage("tested profile as array buffer");

  extension.sendMessage("pause");
  await extension.awaitMessage("paused");

  extension.sendMessage("resume");
  await extension.awaitMessage("resumed");

  extension.sendMessage("stop");
  await extension.awaitMessage("stopped");

  extension.sendMessage("remove runningListener");
  await extension.awaitMessage("removed runningListener");

  await extension.unload();

  Services.prefs.clearUserPref(acceptedExtensionIdsPref);
});
