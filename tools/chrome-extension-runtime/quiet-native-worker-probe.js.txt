// Authored public-API probe shared unchanged between Chrome and Floorp fixtures.
const quietWorkerGeneration = crypto.randomUUID();
let quietNativePort = null;
let quietControlPort = null;
let quietNativeDisconnect = null;
const quietPendingNative = new Map();

function quietHostRequest(command) {
  if (!quietNativePort) {
    return Promise.resolve({workerGeneration: quietWorkerGeneration, nativePortMissing: true, nativeDisconnect: quietNativeDisconnect});
  }
  const requestId = crypto.randomUUID();
  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => {
      quietPendingNative.delete(requestId);
      reject(Error('Owned quiet native host response timed out'));
    }, 4000);
    quietPendingNative.set(requestId, {
      resolve(value) { clearTimeout(timeout); resolve(value); },
      reject(error) { clearTimeout(timeout); reject(error); },
    });
    quietNativePort.postMessage({command, requestId, workerGeneration: quietWorkerGeneration});
  });
}

chrome.runtime.onConnect.addListener(port => {
  if (port.name !== 'owned-quiet-worker-control') return;
  quietControlPort = port;
  port.onDisconnect.addListener(() => {
    const error = chrome.runtime.lastError;
    if (quietControlPort === port) quietControlPort = null;
    if (error) globalThis.quietControlDisconnect = error.message;
  });
  port.postMessage({kind: 'quiet-control-ready', workerGeneration: quietWorkerGeneration});
});

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (message?.kind === 'owned-quiet-native-start') {
    if (quietNativePort) {
      sendResponse({error: 'Owned native Port already exists', workerGeneration: quietWorkerGeneration});
      return false;
    }
    quietNativePort = chrome.runtime.connectNative(message.host);
    quietNativePort.onMessage.addListener(reply => {
      const pending = quietPendingNative.get(reply.message?.requestId);
      if (!pending) return;
      quietPendingNative.delete(reply.message.requestId);
      pending.resolve({workerGeneration: quietWorkerGeneration, host: reply});
    });
    quietNativePort.onDisconnect.addListener(() => {
      quietNativeDisconnect = chrome.runtime.lastError?.message ?? null;
      quietNativePort = null;
      for (const pending of quietPendingNative.values()) pending.reject(Error(quietNativeDisconnect ?? 'Owned native Port disconnected'));
      quietPendingNative.clear();
    });
    quietHostRequest('begin').then(sendResponse, error => sendResponse({error: error.message, workerGeneration: quietWorkerGeneration}));
    return true;
  }
  if (message?.kind === 'owned-quiet-native-probe') {
    quietHostRequest('probe').then(sendResponse, error => sendResponse({error: error.message, workerGeneration: quietWorkerGeneration}));
    return true;
  }
  if (message?.kind === 'owned-quiet-native-stop') {
    quietNativePort?.disconnect();
    quietNativePort = null;
    sendResponse({workerGeneration: quietWorkerGeneration, stopped: true});
    return false;
  }
  return false;
});
