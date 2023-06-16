/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(this, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});

async function setAndEmitFakeRemoteSettingsData(
  data,
  expectClientInitialized = true
) {
  const { AMRemoteSettings } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs"
  );
  let client;
  if (expectClientInitialized) {
    ok(AMRemoteSettings.client, "Got a remote settings client");
    ok(AMRemoteSettings.onSync, "Got a remote settings 'sync' event handler");
    client = AMRemoteSettings.client;
  } else {
    // No client is expected to exist, and so we create one to inject the expected data
    // into the RemoteSettings db.
    client = new RemoteSettings(AMRemoteSettings.RS_COLLECTION);
  }

  await client.db.clear();
  if (data.length) {
    await client.db.importChanges({}, Date.now(), data);
  }
  await client.emit("sync", { data: {} });
}
