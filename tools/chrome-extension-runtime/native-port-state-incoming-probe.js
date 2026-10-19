(() => {
  const realm = typeof document === 'undefined' ? 'worker' : 'document';
  const cases = new Map();
  const getCase = name => {
    if (!cases.has(name)) cases.set(name, { name, realm, callbacks: [], disconnects: [], thenGetterReads: 0 });
    return cases.get(name);
  };
  const post = (port, message) => {
    try { port.postMessage(message); return null; } catch (error) { return { name: error.name, message: error.message }; }
  };
  for (const index of [1, 2]) {
    chrome.runtime.onConnect.addListener(port => {
      if (!port.name.startsWith('floorp-owned-incoming-close-')) return;
      const item = getCase(port.name);
      let callbackActive = true;
      item.callbacks.push({ index, startedAt: Date.now(), initialPostThrow: post(port, { phase: 'entered', index }) });
      if (index === 1) {
        const start = performance.now();
        while (performance.now() - start < 160) chrome.runtime.getURL('incoming-stress');
      }
      port.onDisconnect.addListener(actual => {
        const error = chrome.runtime.lastError;
        item.disconnects.push({ index, callbackActive, samePort: actual === port, error: error?.message ?? null, postThrow: post(port, { phase: 'after-disconnect' }) });
      });
      item.callbacks[item.callbacks.length - 1].afterWorkPostThrow = post(port, { phase: 'after-work', index });
      item.callbacks[item.callbacks.length - 1].finishedAt = Date.now();
      callbackActive = false;
      return { get then() { item.thenGetterReads++; throw new Error('Port event return then getter must not be read'); } };
    });
  }
  globalThis.startIncomingCloseStress = name => new Promise(resolve => {
    const port = chrome.runtime.connect({ name });
    const result = { realm, runtimeId: chrome.runtime.id, messages: [], localDisconnectAt: null };
    const timer = setTimeout(() => { result.timeout = true; port.disconnect(); resolve(result); }, 3000);
    port.onMessage.addListener(message => {
      result.messages.push(message);
      if (message.phase === 'entered' && message.index === 1) {
        result.localDisconnectAt = Date.now();
        port.disconnect();
        clearTimeout(timer); resolve(result);
      }
    });
  });
  globalThis.getIncomingCloseStress = name => cases.get(name) ?? null;
})();
