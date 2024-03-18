/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/// <reference path="../../../../../toolkit/components/translations/tests/browser/shared-head.js" />

"use strict";

/**
 * @type {import("../../content/SummarizerModel.sys.mjs")}
 */
const { SummarizerModel } = ChromeUtils.importESModule(
  "chrome://global/content/ml/SummarizerModel.sys.mjs"
);

/**
 * @type {import("../../actors/MLEngineParent.sys.mjs")}
 */
const { MLEngineParent } = ChromeUtils.importESModule(
  "resource://gre/actors/MLEngineParent.sys.mjs"
);

const { ModelHub } = ChromeUtils.importESModule(
  "chrome://global/content/ml/ModelHub.sys.mjs"
);

const { IndexedDBCache } = ChromeUtils.importESModule(
  "chrome://global/content/ml/ModelHub.sys.mjs"
);
// This test suite shares some utility functions with translations as they work in a very
// similar fashion. Eventually, the plan is to unify these two components.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/translations/tests/browser/shared-head.js",
  this
);

function getDefaultModelRecords() {
  return [
    {
      name: "summarizer-model",
      version: SummarizerModel.MODEL_MAJOR_VERSION + ".0",
    },
  ];
}

function getDefaultWasmRecords() {
  return [
    {
      name: "inference-engine",
      version: MLEngineParent.WASM_MAJOR_VERSION + ".0",
    },
  ];
}

/**
 * Creates a local RemoteSettingsClient for use within tests.
 *
 * @param {boolean} autoDownloadFromRemoteSettings
 * @param {object[]} models
 * @returns {AttachmentMock}
 */
async function createMLModelsRemoteClient(
  autoDownloadFromRemoteSettings,
  models = getDefaultModelRecords()
) {
  const { RemoteSettings } = ChromeUtils.importESModule(
    "resource://services-settings/remote-settings.sys.mjs"
  );
  const mockedCollectionName = "test-ml-models";
  const client = RemoteSettings(
    `${mockedCollectionName}-${_remoteSettingsMockId++}`
  );
  const metadata = {};
  await client.db.clear();
  await client.db.importChanges(
    metadata,
    Date.now(),
    models.map(({ name, version }) => ({
      id: crypto.randomUUID(),
      name,
      version,
      last_modified: Date.now(),
      schema: Date.now(),
      attachment: {
        hash: `${crypto.randomUUID()}`,
        size: `123`,
        filename: name,
        location: `main-workspace/ml-models/${crypto.randomUUID()}.bin`,
        mimetype: "application/octet-stream",
      },
    }))
  );

  return createAttachmentMock(
    client,
    mockedCollectionName,
    autoDownloadFromRemoteSettings
  );
}

async function createAndMockMLRemoteSettings({
  models = getDefaultModelRecords(),
  autoDownloadFromRemoteSettings = false,
} = {}) {
  const remoteClients = {
    models: await createMLModelsRemoteClient(
      autoDownloadFromRemoteSettings,
      models
    ),
    wasm: await createMLWasmRemoteClient(autoDownloadFromRemoteSettings),
  };

  MLEngineParent.mockRemoteSettings(remoteClients.wasm.client);
  SummarizerModel.mockRemoteSettings(remoteClients.models.client);

  return {
    async removeMocks() {
      await remoteClients.models.client.attachments.deleteAll();
      await remoteClients.models.client.db.clear();
      await remoteClients.wasm.client.attachments.deleteAll();
      await remoteClients.wasm.client.db.clear();

      MLEngineParent.removeMocks();
      SummarizerModel.removeMocks();
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
