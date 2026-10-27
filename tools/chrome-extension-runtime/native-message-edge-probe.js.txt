(() => {
  const realm = typeof document === 'undefined' ? 'worker' : 'document';
  const receiverRecords = [];
  const errors = error => ({ name: error?.name, message: error?.message ?? String(error) });
  const invoke = (call, record, name) => { try { record[name] = { returned: typeof call() }; } catch (error) { record[name] = { error: errors(error) }; } };
  chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (request?.ownedControl !== 'response-hook-oracle') return undefined;
    const record = { id: request.id, realm, runtimeId: chrome.runtime.id, senderId: sender.id, kind: request.kind, restored: true, calls: [], hookCalls: 0, thenReads: 0 };
    receiverRecords.push(record);
    if (request.kind.startsWith('reentrant-')) {
      const [, hook, outcome] = request.kind.split('-');
      const runHook = () => {
        record.hookCalls++;
        invoke(() => sendResponse('inner'), record, 'inner');
        if (outcome === 'throw') throw new RangeError('owned outer hook error');
        if (outcome === 'undefined') return undefined;
        if (outcome === 'bigint') return 12n;
        if (outcome === 'cycle') { const cycle = {}; cycle.self = cycle; return cycle; }
        return 'outer';
      };
      const value = hook === 'tojson' ? { toJSON: runHook } : { get selected() { return runHook(); } };
      invoke(() => sendResponse(value), record, 'outer');
      invoke(() => sendResponse('late'), record, 'late');
      return undefined;
    }
    const payload = { ownedPayload: true };
    const property = request.kind.includes('tojson') ? 'toJSON' : 'then';
    const previous = Object.getOwnPropertyDescriptor(Object.prototype, property);
    record.restored = false;
    const restore = () => { if (previous) Object.defineProperty(Object.prototype, property, previous); else delete Object.prototype[property]; record.restored = true; };
    if (property === 'toJSON') {
      Object.defineProperty(Object.prototype, 'toJSON', { configurable: true, value: function(key) { record.hookCalls++; record.calls.push({ original: this === payload, key }); return { transformed: true }; } });
    } else {
      Object.defineProperty(Object.prototype, 'then', { configurable: true, get() { record.thenReads++; record.calls.push({ original: this === payload }); return undefined; } });
    }
    if (request.kind.endsWith('promise')) {
      setTimeout(restore, 0);
      return Promise.resolve(payload);
    }
    try { invoke(() => sendResponse(payload), record, 'outer'); } finally { restore(); }
    return undefined;
  });
  globalThis.responseHookKinds = [
    ...['tojson', 'getter'].flatMap(hook => ['valid', 'throw', 'undefined', 'bigint', 'cycle'].map(outcome => 'reentrant-' + hook + '-' + outcome)),
    'prototype-tojson-sendResponse', 'prototype-tojson-promise', 'prototype-then-sendResponse', 'prototype-then-promise',
  ];
  globalThis.runResponseHook = async (id, kind) => {
    const record = { id, kind, realm, runtimeId: chrome.runtime.id, syncThrow: null, settled: null };
    let promise;
    try { promise = chrome.runtime.sendMessage({ ownedControl: 'response-hook-oracle', id, kind }); } catch (error) { record.syncThrow = errors(error); }
    if (promise) await promise.then(value => { record.settled = { status: 'fulfilled', value }; }, error => { record.settled = { status: 'rejected', error: errors(error) }; });
    return record;
  };
  globalThis.getResponseHook = id => { const record = receiverRecords.find(item => item.id === id); return record?.restored ? record : null; };
  globalThis.runOptionHook = async name => {
    const record = { name, realm, runtimeId: chrome.runtime.id, payloadCalls: 0, optionsReads: 0, order: [], syncThrow: null, settled: null };
    const payload = { toJSON() { record.payloadCalls++; record.order.push('payload-toJSON'); return { ownedPayload: true }; } };
    let options;
    const getter = () => { record.optionsReads++; record.order.push('options-getter'); return false; };
    if (name === 'inherited-unknown-enumerable') options = Object.create({ unknownOwnedOption: true });
    if (name === 'inherited-tls-enumerable') { const prototype = {}; Object.defineProperty(prototype, 'includeTlsChannelId', { enumerable: true, get: getter }); options = Object.create(prototype); }
    if (name === 'own-unknown-nonenumerable') { options = {}; Object.defineProperty(options, 'unknownOwnedOption', { value: true }); }
    if (name === 'own-tls-nonenumerable') { options = {}; Object.defineProperty(options, 'includeTlsChannelId', { get: getter }); }
    if (name === 'own-tls-getter-enumerable') { options = {}; Object.defineProperty(options, 'includeTlsChannelId', { enumerable: true, get: getter }); }
    if (name === 'tls-getter-before-unknown') { options = {}; Object.defineProperty(options, 'includeTlsChannelId', { enumerable: true, get: getter }); options.unknownOwnedOption = true; }
    if (name === 'unknown-before-tls-getter') { options = { unknownOwnedOption: true }; Object.defineProperty(options, 'includeTlsChannelId', { enumerable: true, get: getter }); }
    if (name === 'tls-null') options = { includeTlsChannelId: null };
    if (name === 'tls-undefined') options = { includeTlsChannelId: undefined };
    let promise;
    try { promise = chrome.runtime.sendMessage(chrome.runtime.id, payload, options); } catch (error) { record.syncThrow = errors(error); }
    if (promise) await promise.then(value => { record.settled = { status: 'fulfilled', value }; }, error => { record.settled = { status: 'rejected', error: errors(error) }; });
    return record;
  };
  const portRecords = new Map();
  chrome.runtime.onConnect.addListener(port => {
    if (!port.name.startsWith('owned-port-hook-')) return;
    const record = { realm, name: port.name, runtimeId: chrome.runtime.id, messages: [] };
    portRecords.set(port.name, record);
    port.onMessage.addListener(value => { record.messages.push(value); port.postMessage('owned-ack'); });
  });
  globalThis.getPortHook = name => portRecords.get(name) ?? null;
  globalThis.runPortHook = async (name, kind) => {
    const record = { name, kind, realm, runtimeId: chrome.runtime.id, hookCalls: 0, received: [] };
    const port = chrome.runtime.connect({ name });
    port.onMessage.addListener(value => record.received.push(value));
    port.onDisconnect.addListener(() => { record.disconnectError = chrome.runtime.lastError?.message ?? null; });
    const value = { toJSON() {
      record.hookCalls++;
      if (kind.startsWith('disconnect')) port.disconnect();
      else invoke(() => port.postMessage('inner'), record, 'nested');
      if (kind.endsWith('throw')) throw new RangeError('owned Port hook error');
      return { outer: true };
    } };
    invoke(() => port.postMessage(value), record, 'post');
    await new Promise(resolve => setTimeout(resolve, 180));
    port.disconnect();
    return record;
  };
  // Reply to the options tests without introducing payload/response hooks.
  chrome.runtime.onMessage.addListener((request, sender, respond) => { if (request?.ownedPayload) respond('options-ack'); });
})();
