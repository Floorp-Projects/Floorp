/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  HttpServer: "resource://testing-common/httpd.sys.mjs",
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
});

const PingServer = {
  _httpServer: null,
  _started: false,
  _defers: [Promise.withResolvers()],
  _currentDeferred: 0,

  get port() {
    return this._httpServer.identity.primaryPort;
  },

  get host() {
    return this._httpServer.identity.primaryHost;
  },

  get started() {
    return this._started;
  },

  registerPingHandler(handler) {
    this._httpServer.registerPrefixHandler("/submit/", handler);
  },

  resetPingHandler() {
    this.registerPingHandler(request => {
      let r = request;
      console.trace(
        `defaultPingHandler() - ${r.method} ${r.scheme}://${r.host}:${r.port}${r.path}`
      );
      let deferred = this._defers[this._defers.length - 1];
      this._defers.push(Promise.withResolvers());
      deferred.resolve(request);
    });
  },

  start() {
    this._httpServer = new HttpServer();
    this._httpServer.start(-1);
    this._started = true;
    this.clearRequests();
    this.resetPingHandler();
  },

  stop() {
    return new Promise(resolve => {
      this._httpServer.stop(resolve);
      this._started = false;
    });
  },

  clearRequests() {
    this._defers = [Promise.withResolvers()];
    this._currentDeferred = 0;
  },

  promiseNextRequest() {
    const deferred = this._defers[this._currentDeferred++];
    // Send the ping to the consumer on the next tick, so that the completion gets
    // signaled to Telemetry.
    return new Promise(r =>
      Services.tm.dispatchToMainThread(() => r(deferred.promise))
    );
  },

  promiseNextPing() {
    return this.promiseNextRequest().then(request =>
      decodeRequestPayload(request)
    );
  },

  async promiseNextRequests(count) {
    let results = [];
    for (let i = 0; i < count; ++i) {
      results.push(await this.promiseNextRequest());
    }

    return results;
  },

  promiseNextPings(count) {
    return this.promiseNextRequests(count).then(requests => {
      return Array.from(requests, decodeRequestPayload);
    });
  },
};

/**
 * Decode the payload of an HTTP request into a ping.
 *
 * @param {object} request The data representing an HTTP request (nsIHttpRequest).
 * @returns {object} The decoded ping payload.
 */
function decodeRequestPayload(request) {
  let s = request.bodyInputStream;
  let payload = null;

  if (
    request.hasHeader("content-encoding") &&
    request.getHeader("content-encoding") == "gzip"
  ) {
    let observer = {
      buffer: "",
      onStreamComplete(loader, context, status, length, result) {
        // String.fromCharCode can only deal with 500,000 characters
        // at a time, so chunk the result into parts of that size.
        const chunkSize = 500000;
        for (let offset = 0; offset < result.length; offset += chunkSize) {
          this.buffer += String.fromCharCode.apply(
            String,
            result.slice(offset, offset + chunkSize)
          );
        }
      },
    };

    let scs = Cc["@mozilla.org/streamConverters;1"].getService(
      Ci.nsIStreamConverterService
    );
    let listener = Cc["@mozilla.org/network/stream-loader;1"].createInstance(
      Ci.nsIStreamLoader
    );
    listener.init(observer);
    let converter = scs.asyncConvertData(
      "gzip",
      "uncompressed",
      listener,
      null
    );
    converter.onStartRequest(null, null);
    converter.onDataAvailable(null, s, 0, s.available());
    converter.onStopRequest(null, null, null);
    // TODO: nsIScriptableUnicodeConverter is deprecated
    // But I can't figure out how else to ungzip bodyInputStream.
    let unicodeConverter = Cc[
      "@mozilla.org/intl/scriptableunicodeconverter"
    ].createInstance(Ci.nsIScriptableUnicodeConverter);
    unicodeConverter.charset = "UTF-8";
    let utf8string = unicodeConverter.ConvertToUnicode(observer.buffer);
    utf8string += unicodeConverter.Finish();
    payload = JSON.parse(utf8string);
  } else {
    let bytes = NetUtil.readInputStream(s, s.available());
    payload = JSON.parse(new TextDecoder().decode(bytes));
  }

  return payload;
}
