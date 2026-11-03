// Authored public-API probe shared unchanged between Chrome and Floorp fixtures.
globalThis.ownedQuietNativeProbe = (() => {
  let controlPort = null;
  const state = {
    started: false,
    controlReady: null,
    controlDisconnected: false,
    controlDisconnectError: null,
    controlMessages: [],
    initial: null,
    startDocumentTime: null,
    quietStartDocumentTime: null,
  };
  const snapshot = () => JSON.parse(JSON.stringify({...state, documentTime: performance.now()}));
  return {
    async start({nativeHost = null} = {}) {
      if (state.started) throw Error('Owned quiet probe already started');
      state.started = true;
      state.startDocumentTime = performance.now();
      await new Promise((resolve, reject) => {
        const timeout = setTimeout(() => reject(Error('Quiet control Port did not become ready')), 4000);
        controlPort = chrome.runtime.connect({name: 'owned-quiet-worker-control'});
        controlPort.onDisconnect.addListener(() => {
          state.controlDisconnected = true;
          state.controlDisconnectError = chrome.runtime.lastError?.message ?? null;
          clearTimeout(timeout);
          if (!state.controlReady) reject(Error(state.controlDisconnectError ?? 'Quiet control Port disconnected before ready'));
        });
        controlPort.onMessage.addListener(message => {
          state.controlMessages.push(message);
          if (message.kind === 'quiet-control-ready') {
            state.controlReady = message;
            clearTimeout(timeout);
            resolve();
          }
        });
      });
      if (nativeHost) {
        state.initial = await chrome.runtime.sendMessage({kind: 'owned-quiet-native-start', host: nativeHost});
        if (state.initial?.error) throw Error(state.initial.error);
        if (state.initial?.workerGeneration !== state.controlReady.workerGeneration) throw Error('Native host and control Port started in different Worker generations');
      }
      state.quietStartDocumentTime = performance.now();
      return snapshot();
    },
    snapshot,
    async probe() {
      const passiveBefore = snapshot();
      const response = await chrome.runtime.sendMessage({kind: 'owned-quiet-native-probe'});
      return {passiveBefore, response, passiveAfter: snapshot()};
    },
    async stop() {
      if (state.initial && !state.controlDisconnected) await chrome.runtime.sendMessage({kind: 'owned-quiet-native-stop'});
      controlPort?.disconnect();
      controlPort = null;
      return snapshot();
    },
  };
})();
