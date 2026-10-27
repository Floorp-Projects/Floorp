(() => {
  const realm = typeof document === 'undefined' ? 'worker' : 'document';
  const receiver = (message, sender, respond) => {
    respond({ receiver: realm, receiverId: chrome.runtime.id, senderId: sender.id, payloadType: typeof message, payloadUndefined: message === undefined, payload: message });
  };
  chrome.runtime.onMessage.addListener(receiver);
  globalThis.setOverloadReceiverEnabled = enabled => { if (enabled) chrome.runtime.onMessage.addListener(receiver); else chrome.runtime.onMessage.removeListener(receiver); };
  const payload = () => ({ overloadPayload: 'owned-object', value: 7 });
  const text = 'owned-string-payload';
  const own = () => chrome.runtime.id;
  const cases = [
    ['object-payload', () => [payload()]],
    ['string-payload', () => [text]],
    ['own-id-object-payload', () => [own(), payload()]],
    ['null-id-object-payload', () => [null, payload()]],
    ['undefined-id-object-payload', () => [undefined, payload()]],
    ['string-payload-empty-options', () => [text, {}]],
    ['string-payload-tls-options', () => [text, { includeTlsChannelId: false }]],
    ['object-payload-empty-options', () => [payload(), {}]],
    ['own-id-object-payload-empty-options', () => [own(), payload(), {}]],
    ['own-id-string-payload-empty-options', () => [own(), text, {}]],
    ['own-id-object-payload-tls-options', () => [own(), payload(), { includeTlsChannelId: false }]],
    ['null-id-object-payload-tls-options', () => [null, payload(), { includeTlsChannelId: false }]],
    ['string-payload-null-second', () => [text, null]],
    ['string-payload-undefined-second', () => [text, undefined]],
    ['own-id-null-second', () => [own(), null]],
    ['own-id-undefined-second', () => [own(), undefined]],
    ['empty-string-id-object-payload', () => ['', payload()]],
    ['own-id-as-string-with-empty-options', () => [own(), {}]],
    ['own-id-as-string-with-tls-options', () => [own(), { includeTlsChannelId: false }]],
    ['object-payload-null-options', () => [payload(), null]],
    ['object-payload-undefined-options', () => [payload(), undefined]],
    ['own-id-object-payload-null-options', () => [own(), payload(), null]],
    ['own-id-object-payload-undefined-options', () => [own(), payload(), undefined]],
    ['null-id-object-payload-empty-options', () => [null, payload(), {}]],
    ['undefined-id-object-payload-empty-options', () => [undefined, payload(), {}]],
    ['null-id-string-payload', () => [null, text]],
    ['undefined-id-string-payload', () => [undefined, text]],
    ['null-first-empty-object-second', () => [null, {}]],
    ['undefined-first-empty-object-second', () => [undefined, {}]],
    ['null-first-tls-object-second', () => [null, { includeTlsChannelId: false }]],
    ['undefined-first-tls-object-second', () => [undefined, { includeTlsChannelId: false }]],
    ['object-options-undefined-callback-slot', () => [payload(), {}, undefined]],
    ['object-options-null-callback-slot', () => [payload(), {}, null]],
    ['object-undefined-options-undefined-callback-slot', () => [payload(), undefined, undefined]],
    ['object-null-options-null-callback-slot', () => [payload(), null, null]],
    ['own-id-object-options-undefined-callback-slot', () => [own(), payload(), {}, undefined]],
    ['own-id-object-options-null-callback-slot', () => [own(), payload(), {}, null]],
    ['string-options-undefined-callback-slot', () => [text, {}, undefined]],
    ['string-options-null-callback-slot', () => [text, {}, null]],
    ['null-id-object-options-null-callback-slot', () => [null, payload(), {}, null]],
    ['empty-arguments', () => []],
    ['undefined-only', () => [undefined]],
    ['null-only', () => [null]],
    ['number-only', () => [41]],
    ['object-payload-unknown-options', () => [payload(), { unknownOwnedOption: true }]],
    ['string-payload-unknown-options', () => [text, { unknownOwnedOption: true }]],
    ['own-id-object-payload-unknown-options', () => [own(), payload(), { unknownOwnedOption: true }]],
    ['object-payload-number-options', () => [payload(), 42]],
    ['own-id-object-payload-number-options', () => [own(), payload(), 42]],
    ['invalid-id-object-payload', () => ['!', payload()]],
    ['invalid-id-object-payload-empty-options', () => ['!', payload(), {}]],
    ['invalid-id-string-payload-empty-options', () => ['!', text, {}]],
    ['too-many-arguments', () => [own(), payload(), {}, 'extra-owned-argument']],
    ['tls-option-wrong-type', () => [payload(), { includeTlsChannelId: 'wrong' }]],
    ['own-id-tls-option-wrong-type', () => [own(), payload(), { includeTlsChannelId: 'wrong' }]],
  ];
  const errors = error => ({ name: error?.name ?? null, message: error?.message ?? String(error) });
  const snapshotLastError = () => {
    const value = chrome.runtime.lastError;
    return value === undefined ? null : { name: value.name ?? null, message: value.message ?? null, keys: Object.keys(value) };
  };
  const describe = value => value === undefined ? { type: 'undefined' } : value === null ? { type: 'null' } : { type: typeof value, value };
  globalThis.listOverloadCases = () => cases.map(([name, make]) => ({ name, arguments: make().map(describe) }));
  globalThis.runOverloadCase = async (name, mode) => {
    const entry = cases.find(value => value[0] === name);
    if (!entry) throw Error('Unknown authored overload case');
    const args = entry[1]();
    const result = { name, mode, realm, runtimeId: chrome.runtime.id, arguments: args.map(describe), beforeLastError: snapshotLastError(), syncThrow: null, returnedKind: null, callback: null, promise: null };
    let completeCallback;
    const callbackComplete = new Promise(resolve => { completeCallback = resolve; });
    const callback = response => {
      result.callback = { response, responseUndefined: response === undefined, lastError: snapshotLastError() };
      completeCallback();
    };
    let returned;
    try {
      returned = chrome.runtime.sendMessage(...args, ...(mode === 'callback' ? [callback] : []));
      result.returnedKind = returned === undefined ? 'undefined' : returned instanceof Promise ? 'Promise' : typeof returned;
    } catch (error) { result.syncThrow = errors(error); }
    if (!result.syncThrow) {
      const settlement = returned && typeof returned.then === 'function'
        ? Promise.resolve(returned).then(value => { result.promise = { status: 'fulfilled', response: value, responseUndefined: value === undefined }; }, error => { result.promise = { status: 'rejected', error: errors(error) }; })
        : null;
      if (mode === 'callback') {
        let timer;
        const timeout = new Promise(resolve => { timer = setTimeout(() => { result.callbackTimeout = true; resolve(); }, 1200); });
        await Promise.race([callbackComplete, timeout]);
        clearTimeout(timer);
        if (settlement) await settlement;
      } else if (settlement) await settlement;
    }
    result.afterLastError = snapshotLastError();
    await new Promise(resolve => setTimeout(resolve, 0));
    result.nextTurnLastError = snapshotLastError();
    return result;
  };
})();
