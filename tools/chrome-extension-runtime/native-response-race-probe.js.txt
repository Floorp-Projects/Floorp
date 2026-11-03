(() => {
  const realm = typeof document === 'undefined' ? 'worker' : 'document';
  const records = [];
  const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
  const errorShape = error => ({ name: error?.name, message: error?.message ?? String(error) });
  const kinds = ['sync-callback-return-promise', 'pending-promise-earlier-callback', 'fulfilled-promise-later-callback', 'fulfillment-tojson-callback-valid', 'fulfillment-tojson-callback-throw', 'never-promise-earlier-callback'];
  chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (request?.ownedResponseRace !== true) return undefined;
    const row = { id: request.id, kind: request.kind, realm, runtimeId: chrome.runtime.id, senderId: sender.id, calls: [], hooks: 0, events: [] };
    records.push(row);
    const respond = value => {
      const call = { value };
      row.calls.push(call);
      try { call.returnedType = typeof sendResponse(value); }
      catch (error) { call.error = errorShape(error); }
    };
    if (request.kind === 'sync-callback-return-promise') {
      respond('sync-callback');
      return Promise.resolve('returned-promise');
    }
    if (request.kind === 'pending-promise-earlier-callback') {
      setTimeout(() => { row.events.push('callback'); respond('early-callback'); }, 10);
      return new Promise(resolve => setTimeout(() => { row.events.push('promise'); resolve('late-promise'); }, 60));
    }
    if (request.kind === 'fulfilled-promise-later-callback') {
      setTimeout(() => { row.events.push('callback'); respond('late-callback'); }, 60);
      return new Promise(resolve => setTimeout(() => { row.events.push('promise'); resolve('early-promise'); }, 10));
    }
    if (request.kind === 'never-promise-earlier-callback') {
      setTimeout(() => { row.events.push('callback'); respond('only-callback'); }, 10);
      return new Promise(() => {});
    }
    const payload = { toJSON() {
      row.hooks++;
      respond('reentrant-callback');
      if (request.kind.endsWith('throw')) throw new RangeError('owned response race hook failure');
      return 'outer-promise';
    } };
    return Promise.resolve(payload);
  });
  globalThis.responseRaceKinds = kinds;
  globalThis.getResponseRace = id => records.find(row => row.id === id) ?? null;
  globalThis.runResponseRace = async (id, kind) => {
    const row = { id, kind, realm, runtimeId: chrome.runtime.id, syncThrow: null, settled: null };
    let result;
    try { result = chrome.runtime.sendMessage(chrome.runtime.id, { ownedResponseRace: true, id, kind }); }
    catch (error) { row.syncThrow = errorShape(error); }
    if (result) await Promise.race([
      result.then(value => { row.settled = { status: 'fulfilled', value }; }, error => { row.settled = { status: 'rejected', error: errorShape(error) }; }),
      pause(350).then(() => { if (!row.settled) row.settled = { status: 'timeout' }; }),
    ]);
    await pause(90);
    return row;
  };
})();
