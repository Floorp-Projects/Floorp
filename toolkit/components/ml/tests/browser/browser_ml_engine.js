/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/// <reference path="head.js" />

async function setup({ disabled = false, prefs = [] } = {}) {
  const { removeMocks, remoteClients } = await createAndMockMLRemoteSettings({
    autoDownloadFromRemoteSettings: false,
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabled by default.
      ["browser.ml.enable", !disabled],
      ["browser.ml.logLevel", "All"],
      ...prefs,
    ],
  });

  return {
    remoteClients,
    async cleanup() {
      await removeMocks();
      await waitForCondition(
        () => EngineProcess.areAllEnginesTerminated(),
        "Waiting for all of the engines to be terminated.",
        100,
        200
      );
    },
  };
}

add_task(async function test_ml_engine_basics() {
  const { cleanup, remoteClients } = await setup();

  info("Get the engine process");
  const mlEngineParent = await EngineProcess.getMLEngineParent();

  info("Get summarizer");
  const summarizer = mlEngineParent.getEngine(
    "summarizer",
    SummarizerModel.getModel
  );

  info("Run the summarizer");
  const summarizePromise = summarizer.run("This gets cut in half.");

  info("Wait for the pending downloads.");
  await remoteClients.models.resolvePendingDownloads(1);
  await remoteClients.wasm.resolvePendingDownloads(1);

  is(
    await summarizePromise,
    "This gets c",
    "The text gets cut in half simulating summarizing"
  );

  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  await EngineProcess.destroyMLEngine();

  await cleanup();
});

add_task(async function test_ml_engine_model_rejection() {
  const { cleanup, remoteClients } = await setup();

  info("Get the engine process");
  const mlEngineParent = await EngineProcess.getMLEngineParent();

  info("Get summarizer");
  const summarizer = mlEngineParent.getEngine(
    "summarizer",
    SummarizerModel.getModel
  );

  info("Run the summarizer");
  const summarizePromise = summarizer.run("This gets cut in half.");

  info("Wait for the pending downloads.");
  await remoteClients.wasm.resolvePendingDownloads(1);
  await remoteClients.models.rejectPendingDownloads(1);

  let error;
  try {
    await summarizePromise;
  } catch (e) {
    error = e;
  }
  is(
    error?.message,
    "Intentionally rejecting downloads.",
    "The error is correctly surfaced."
  );

  await cleanup();
});

add_task(async function test_ml_engine_wasm_rejection() {
  const { cleanup, remoteClients } = await setup();

  info("Get the engine process");
  const mlEngineParent = await EngineProcess.getMLEngineParent();

  info("Get summarizer");
  const summarizer = mlEngineParent.getEngine(
    "summarizer",
    SummarizerModel.getModel
  );

  info("Run the summarizer");
  const summarizePromise = summarizer.run("This gets cut in half.");

  info("Wait for the pending downloads.");
  await remoteClients.wasm.rejectPendingDownloads(1);
  await remoteClients.models.resolvePendingDownloads(1);

  let error;
  try {
    await summarizePromise;
  } catch (e) {
    error = e;
  }
  is(
    error?.message,
    "Intentionally rejecting downloads.",
    "The error is correctly surfaced."
  );

  await cleanup();
});

/**
 * Tests that the SummarizerModel's internal errors are correctly surfaced.
 */
add_task(async function test_ml_engine_model_error() {
  const { cleanup, remoteClients } = await setup();

  info("Get the engine process");
  const mlEngineParent = await EngineProcess.getMLEngineParent();

  info("Get summarizer");
  const summarizer = mlEngineParent.getEngine(
    "summarizer",
    SummarizerModel.getModel
  );

  info("Run the summarizer with a throwing example.");
  const summarizePromise = summarizer.run("throw");

  info("Wait for the pending downloads.");
  await remoteClients.wasm.resolvePendingDownloads(1);
  await remoteClients.models.resolvePendingDownloads(1);

  let error;
  try {
    await summarizePromise;
  } catch (e) {
    error = e;
  }
  is(
    error?.message,
    'Error: Received the message "throw", so intentionally throwing an error.',
    "The error is correctly surfaced."
  );

  summarizer.terminate();

  await cleanup();
});

/**
 * This test is really similar to the "basic" test, but tests manually destroying
 * the summarizer.
 */
add_task(async function test_ml_engine_destruction() {
  const { cleanup, remoteClients } = await setup();

  info("Get the engine process");
  const mlEngineParent = await EngineProcess.getMLEngineParent();

  info("Get summarizer");
  const summarizer = mlEngineParent.getEngine(
    "summarizer",
    SummarizerModel.getModel
  );

  info("Run the summarizer");
  const summarizePromise = summarizer.run("This gets cut in half.");

  info("Wait for the pending downloads.");
  await remoteClients.models.resolvePendingDownloads(1);
  await remoteClients.wasm.resolvePendingDownloads(1);

  is(
    await summarizePromise,
    "This gets c",
    "The text gets cut in half simulating summarizing"
  );

  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  summarizer.terminate();

  info(
    "The summarizer is manually destroyed. The cleanup function should wait for the engine process to be destroyed."
  );

  await cleanup();
});
