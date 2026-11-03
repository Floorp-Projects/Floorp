(() => {
  const realm = typeof document === 'undefined' ? 'worker' : 'document';
  const normalName = 'floorp-state-stress-owned-remote-close';
  chrome.runtime.onConnect.addListener(port => {
    if (port.name === normalName) port.disconnect();
  });
  const errorRecord = error => error ? { name: error.name, message: error.message } : null;
  const tryPost = (port, value) => {
    try { port.postMessage(value); return null; } catch (error) { return errorRecord(error); }
  };
  const burnSameTask = (milliseconds = 45) => {
    const start = performance.now();
    let iterations = 0;
    while (performance.now() - start < milliseconds) {
      iterations++;
      if (iterations % 128 === 0) chrome.runtime.getURL('state-stress-witness');
    }
    return { milliseconds: performance.now() - start, iterations };
  };
  const connect = (kind, hosts) => kind === 'normal'
    ? chrome.runtime.connect({ name: normalName })
    : chrome.runtime.connectNative(hosts[kind]);
  const sleep = milliseconds => new Promise(resolve => setTimeout(resolve, milliseconds));
  const withListeners = (kind, hosts, serialization) => new Promise(resolve => {
    let initialTask = true;
    const result = { kind, variant: serialization ? 'serialization' : 'late-listeners', listeners: [], postThrow: null, timeout: false };
    const port = connect(kind, hosts);
    const timer = setTimeout(() => { result.timeout = true; port.disconnect(); resolve(result); }, 2500);
    result.workload = burnSameTask();
    port.onMessage.addListener(() => { result.unexpectedMessage = true; });
    port.onDisconnect.addListener(actual => {
      const lastError = chrome.runtime.lastError;
      result.listeners.push({ index: 1, initialTask, samePort: actual === port, error: lastError?.message ?? null, postThrow: tryPost(port, { afterDisconnect: true }) });
    });
    port.onDisconnect.addListener(actual => {
      const lastError = chrome.runtime.lastError;
      result.listeners.push({ index: 2, initialTask, samePort: actual === port, error: lastError?.message ?? null, postThrow: tryPost(port, { afterDisconnect: true }) });
      setTimeout(() => { clearTimeout(timer); resolve(result); }, 0);
    });
    if (serialization) {
      const cycle = {}; cycle.self = cycle;
      result.cyclicThrow = tryPost(port, cycle);
      result.bigIntThrow = tryPost(port, { value: 1n });
    }
    result.postThrow = tryPost(port, { stateStress: true, kind });
    initialTask = false;
  });
  const noPublicListener = async (kind, hosts) => {
    const port = connect(kind, hosts);
    const result = { kind, variant: 'no-public-disconnect-listener', workload: burnSameTask(), initialPostThrow: null, eventualPostThrow: null, attempts: 0 };
    result.initialPostThrow = tryPost(port, { initial: true });
    const deadline = performance.now() + 2500;
    while (!result.eventualPostThrow && performance.now() < deadline) {
      await sleep(10);
      result.attempts++;
      result.eventualPostThrow = tryPost(port, { later: true });
    }
    port.disconnect();
    return result;
  };
  const explicitDisconnect = (kind, hosts) => {
    const port = connect(kind, hosts);
    port.disconnect();
    return { kind, variant: 'explicit-local-disconnect', postThrow: tryPost(port, { afterLocalDisconnect: true }) };
  };
  const mutationAtPost = host => new Promise(resolve => {
    const port = chrome.runtime.connectNative(host);
    const result = { kind: 'allowed', variant: 'mutation-at-post', echo: null, postThrow: null };
    const timer = setTimeout(() => { result.timeout = true; port.disconnect(); resolve(result); }, 2500);
    port.onMessage.addListener(message => {
      result.echo = message;
      clearTimeout(timer);
      port.disconnect();
      resolve(result);
    });
    port.onDisconnect.addListener(() => {
      const error = chrome.runtime.lastError;
      if (error) result.disconnectError = error.message;
    });
    const payload = { command: 'echo', value: 'at-call', list: ['original'] };
    result.postThrow = tryPost(port, payload);
    payload.value = 'mutated'; payload.list.push('later');
  });
  const establishedLaterTask = host => new Promise(resolve => {
    const port = chrome.runtime.connectNative(host);
    const result = { kind: 'allowed', variant: 'established-later-task-close', postThrow: null, initialTask: null, hostCloseRequestedAt: null, postAt: null };
    let callbackActive = false;
    const timer = setTimeout(() => { result.timeout = true; port.disconnect(); resolve(result); }, 3000);
    port.onMessage.addListener(message => {
      if (message.armed !== true) return;
      callbackActive = true;
      result.hostCloseRequestedAt = message.closeRequestedAt;
      result.workload = burnSameTask(160);
      result.postAt = Date.now();
      result.postThrow = tryPost(port, { afterHostExitBeforeDisconnectDelivery: true });
      callbackActive = false;
    });
    port.onDisconnect.addListener(actual => {
      const lastError = chrome.runtime.lastError;
      result.disconnect = { samePort: actual === port, callbackActive, error: lastError?.message ?? null, postThrow: tryPost(port, { afterDisconnect: true }) };
      clearTimeout(timer); resolve(result);
    });
    result.initialPostThrow = tryPost(port, { command: 'armed-close', delay: 0.035 });
  });
  globalThis.probePortStateStress = async hosts => {
    const cases = [];
    for (const kind of ['normal', 'missing', 'forbidden']) {
      cases.push(await withListeners(kind, hosts, false));
      cases.push(await withListeners(kind, hosts, true));
      cases.push(await noPublicListener(kind, hosts));
      cases.push(explicitDisconnect(kind, hosts));
    }
    cases.push(await mutationAtPost(hosts.allowed));
    cases.push(await establishedLaterTask(hosts.allowed));
    return { realm, runtimeId: chrome.runtime.id, cases };
  };
})();
