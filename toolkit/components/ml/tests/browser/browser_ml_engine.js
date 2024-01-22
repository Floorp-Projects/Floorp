/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_ml_engine_exists() {
  const response = await fetch("chrome://global/content/ml/MLEngine.html");
  ok(response.ok, "The ml engine can be fetched.");
});

add_task(async function test_summarization() {
  const { removeMocks, remoteClients } = await createAndMockMLRemoteSettings({
    autoDownloadFromRemoteSettings: true,
  });

  const summarizePromise = SummarizerModel.summarize("This gets cut in half.");

  await remoteClients.models.resolvePendingDownloads(1);
  await remoteClients.wasm.resolvePendingDownloads(1);

  is(
    await summarizePromise,
    "This gets c",
    "The text gets cut in half simulating summarizing"
  );

  removeMocks();
});
