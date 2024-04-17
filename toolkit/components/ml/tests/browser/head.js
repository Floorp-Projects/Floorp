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
  const remoteClients = {
    wasm: await createMLWasmRemoteClient(autoDownloadFromRemoteSettings),
  };

  MLEngineParent.mockRemoteSettings(remoteClients.wasm.client);

  return {
    async removeMocks() {
      await remoteClients.wasm.client.attachments.deleteAll();
      await remoteClients.wasm.client.db.clear();

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
