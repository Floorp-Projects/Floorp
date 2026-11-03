// SPDX-License-Identifier: MPL-2.0
// This public extension program is shared byte-for-byte by native and Chrome runs.
// Exact expected bytes of the existing authored corpus-content.html HTTP fixture.
export const ownedWebRequestExpectedBody =
  "<!doctype html><meta charset=utf-8><title>Authored content transformation</title><style>html,body{background:rgb(255,255,255);color:rgb(0,0,0)}body{margin:30px;font:20px sans-serif}#probe{padding:30px;background:rgb(210,220,230)}</style><h1>Authored extension content operation</h1><p id=probe>Verify the real installed extension changes this authored page.</p><a href=/second>Navigate and verify persistence</a>";
export const ownedWebRequestProbeSource = `
async function runOwnedWebRequestProbe(config) {
  const storage = (globalThis.browser ?? chrome).storage.local;
  const records = [];
  const listeners = [];
  const observation = {
    phase: 'starting', kind: config.kind, token: config.token,
    runtimeId: chrome.runtime.id,
    isWorker: typeof ServiceWorkerGlobalScope === 'function' && self instanceof ServiceWorkerGlobalScope,
    hasDocument: typeof document !== 'undefined',
    importWitness: globalThis.ownedWebRequestImportWitness,
    url: config.url, records, cleanupErrors: [], invalidAdds: [], registrations: [], removals: [],
  };
  const pause = milliseconds => new Promise(resolve => setTimeout(resolve, milliseconds));
  const publish = async phase => {
    observation.phase = phase;
    await storage.set({ [config.resultKey]: observation });
  };
  try {
    const invalidListener = () => {};
    for (const test of [
      { kind: 'missing-filter', args: [] },
      { kind: 'non-array-urls', args: [{ urls: 'invalid-owned-filter' }] },
    ]) {
      let error = null;
      try { chrome.webRequest.onBeforeRequest.addListener(invalidListener, ...test.args); }
      catch (failure) { error = String(failure); }
      const registered = chrome.webRequest.onBeforeRequest.hasListener(invalidListener);
      observation.invalidAdds.push({ kind: test.kind, error, registered });
      if (registered) {
        try { chrome.webRequest.onBeforeRequest.removeListener(invalidListener); }
        catch (failure) { observation.cleanupErrors.push({ name: 'invalid-' + test.kind, error: String(failure) }); }
      }
    }
    for (const name of ['onBeforeRequest', 'onBeforeSendHeaders', 'onResponseStarted', 'onCompleted', 'onErrorOccurred']) {
      const listener = details => {
        if (details.url !== config.url) return;
        records.push({ name, url: details.url, requestId: details.requestId,
          method: details.method, type: details.type, timeStamp: details.timeStamp,
          observedAt: Date.now(), statusCode: details.statusCode ?? null,
          error: details.error ?? null });
      };
      chrome.webRequest[name].addListener(listener, { urls: ['http://127.0.0.1/*'] });
      listeners.push({ name, listener });
      observation.registrations.push({ name, registered: chrome.webRequest[name].hasListener(listener) });
    }
    await publish('ready');
    const gateDeadline = Date.now() + 15000;
    let released = false;
    while (Date.now() < gateDeadline) {
      const state = await storage.get(config.gateKey);
      if (state[config.gateKey] === config.token) { released = true; break; }
      await pause(30);
    }
    if (!released) throw Error('Owned HTTP request gate was not released');
    observation.startedAt = Date.now();
    const abort = new AbortController();
    const timeout = setTimeout(() => abort.abort(), 8000);
    try {
      const response = await fetch(config.url, { cache: 'no-store', credentials: 'omit', signal: abort.signal });
      observation.response = { status: response.status, url: response.url,
        redirected: response.redirected, body: await response.text() };
    } finally { clearTimeout(timeout); }
    const eventDeadline = Date.now() + 5000;
    while (Date.now() < eventDeadline && !records.some(record => record.name === 'onCompleted' || record.name === 'onErrorOccurred')) await pause(30);
    // Allow a subsequent ordinary batch to arrive before checking duplicates/order.
    await pause(300);
    observation.finishedAt = Date.now();
  } catch (error) {
    observation.error = String(error);
  } finally {
    for (const { name, listener } of listeners) {
      let before = null, after = null;
      try { before = chrome.webRequest[name].hasListener(listener); }
      catch (error) { observation.cleanupErrors.push({ name, error: 'hasListener before removal: ' + String(error) }); }
      try { chrome.webRequest[name].removeListener(listener); }
      catch (error) { observation.cleanupErrors.push({ name, error: String(error) }); }
      try { after = chrome.webRequest[name].hasListener(listener); }
      catch (error) { observation.cleanupErrors.push({ name, error: 'hasListener after removal: ' + String(error) }); }
      observation.removals.push({ name, before, after });
    }
  }
  await publish('complete');
}
`;

export const ownedWebRequestControllerSource = `
const ownedWebRequestStorage = (globalThis.browser ?? chrome).storage.local;
window.addEventListener('message', async event => {
  if ((event.source !== window && event.source !== null) || event.data?.target !== 'owned-webrequest-release' || event.data?.token !== ownedWebRequestConfig.token) return;
  try { await ownedWebRequestStorage.set({ [ownedWebRequestConfig.gateKey]: ownedWebRequestConfig.token }); }
  catch (error) { document.title = JSON.stringify({ phase: 'controller-error', error: String(error) }); }
});
(async () => {
  try {
    for (;;) {
      const state = await ownedWebRequestStorage.get(ownedWebRequestConfig.resultKey);
      const value = state[ownedWebRequestConfig.resultKey];
      if (value) {
        document.title = JSON.stringify(value);
        if (value.phase === 'complete') break;
      }
      await new Promise(resolve => setTimeout(resolve, 30));
    }
  } catch (error) { document.title = JSON.stringify({ phase: 'controller-error', error: String(error) }); }
})();
`;
