/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/// <reference path="../../../../../toolkit/components/translations/tests/browser/shared-head.js" />

"use strict";

/**
 * @type {import("../../actors/MLEngineParent.sys.mjs")}
 */
const { MLEngineParent } = ChromeUtils.importESModule(
  "resource://gre/actors/MLEngineParent.sys.mjs"
);

const { ModelHub, IndexedDBCache } = ChromeUtils.importESModule(
  "chrome://global/content/ml/ModelHub.sys.mjs"
);

const { getRuntimeWasmFilename } = ChromeUtils.importESModule(
  "chrome://global/content/ml/Utils.sys.mjs"
);

const { PipelineOptions } = ChromeUtils.importESModule(
  "chrome://global/content/ml/EngineProcess.sys.mjs"
);

// This test suite shares some utility functions with translations as they work in a very
// similar fashion. Eventually, the plan is to unify these two components.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/translations/tests/browser/shared-head.js",
  this
);

function getDefaultWasmRecords() {
  return [
    {
      name: getRuntimeWasmFilename(),
      version: MLEngineParent.WASM_MAJOR_VERSION + ".0",
    },
  ];
}

async function createAndMockMLRemoteSettings({
  autoDownloadFromRemoteSettings = false,
} = {}) {
  const runtime = await createMLWasmRemoteClient(
    autoDownloadFromRemoteSettings
  );
  const options = await createOptionsRemoteClient();

  const remoteClients = {
    "ml-onnx-runtime": runtime,
    "ml-inference-options": options,
  };

  MLEngineParent.mockRemoteSettings({
    "ml-onnx-runtime": runtime.client,
    "ml-inference-options": options,
  });

  return {
    async removeMocks() {
      await runtime.client.attachments.deleteAll();
      await runtime.client.db.clear();
      await options.db.clear();
      MLEngineParent.removeMocks();
    },
    remoteClients,
  };
}

/**
 * Creates a local RemoteSettingsClient for use within tests.
 *
 * @param {boolean} autoDownloadFromRemoteSettings
 * @returns {AttachmentMock}
 */
async function createMLWasmRemoteClient(autoDownloadFromRemoteSettings) {
  const { RemoteSettings } = ChromeUtils.importESModule(
    "resource://services-settings/remote-settings.sys.mjs"
  );
  const mockedCollectionName = "test-translation-wasm";
  const client = RemoteSettings(
    `${mockedCollectionName}-${_remoteSettingsMockId++}`
  );
  const metadata = {};
  await client.db.clear();
  await client.db.importChanges(
    metadata,
    Date.now(),
    getDefaultWasmRecords().map(({ name, version }) => ({
      id: crypto.randomUUID(),
      name,
      version,
      last_modified: Date.now(),
      schema: Date.now(),
    }))
  );

  return createAttachmentMock(
    client,
    mockedCollectionName,
    autoDownloadFromRemoteSettings
  );
}

/**
 * Creates a local RemoteSettingsClient for use within tests.
 *
 * @returns {RemoteSettings}
 */
async function createOptionsRemoteClient() {
  const { RemoteSettings } = ChromeUtils.importESModule(
    "resource://services-settings/remote-settings.sys.mjs"
  );
  const mockedCollectionName = "test-ml-inference-options";
  const client = RemoteSettings(
    `${mockedCollectionName}-${_remoteSettingsMockId++}`
  );

  const record = {
    taskName: "echo",
    modelId: "mozilla/distilvit",
    processorId: "mozilla/distilvit",
    tokenizerId: "mozilla/distilvit",
    modelRevision: "main",
    processorRevision: "main",
    tokenizerRevision: "main",
    id: "74a71cfd-1734-44e6-85c0-69cf3e874138",
  };
  await client.db.clear();
  await client.db.importChanges({}, Date.now(), [record]);
  return client;
}
