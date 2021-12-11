/*
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
"use strict";

/*
 * This file is generated from kinto-http.js - do not modify directly.
 */

const global = this;
const globalThis = this;

var EXPORTED_SYMBOLS = ["KintoHttpClient"];

const { setTimeout, clearTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");
const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyGlobalGetters(global, ["fetch"]);

/*
 * Version 5.3.0 - 284d97d
 */

(function (global, factory) {
    typeof exports === 'object' && typeof module !== 'undefined' ? module.exports = factory() :
    typeof define === 'function' && define.amd ? define(factory) :
    (global = typeof globalThis !== 'undefined' ? globalThis : global || self, global.KintoHttpClient = factory());
}(this, (function () { 'use strict';

    /*! *****************************************************************************
    Copyright (c) Microsoft Corporation.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
    REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
    AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
    INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
    LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
    OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
    PERFORMANCE OF THIS SOFTWARE.
    ***************************************************************************** */
    function __decorate(decorators, target, key, desc) {
        var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
        if (typeof Reflect === "object" && typeof Reflect.decorate === "function")
            r = Reflect.decorate(decorators, target, key, desc);
        else
            for (var i = decorators.length - 1; i >= 0; i--)
                if (d = decorators[i])
                    r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
        return c > 3 && r && Object.defineProperty(target, key, r), r;
    }

    /**
     * Chunks an array into n pieces.
     *
     * @private
     * @param  {Array}  array
     * @param  {Number} n
     * @return {Array}
     */
    function partition(array, n) {
        if (n <= 0) {
            return [array];
        }
        return array.reduce((acc, x, i) => {
            if (i === 0 || i % n === 0) {
                acc.push([x]);
            }
            else {
                acc[acc.length - 1].push(x);
            }
            return acc;
        }, []);
    }
    /**
     * Returns a Promise always resolving after the specified amount in milliseconds.
     *
     * @return Promise<void>
     */
    function delay(ms) {
        return new Promise((resolve) => setTimeout(resolve, ms));
    }
    /**
     * Always returns a resource data object from the provided argument.
     *
     * @private
     * @param  {Object|String} resource
     * @return {Object}
     */
    function toDataBody(resource) {
        if (isObject(resource)) {
            return resource;
        }
        if (typeof resource === "string") {
            return { id: resource };
        }
        throw new Error("Invalid argument.");
    }
    /**
     * Transforms an object into an URL query string, stripping out any undefined
     * values.
     *
     * @param  {Object} obj
     * @return {String}
     */
    function qsify(obj) {
        const encode = (v) => encodeURIComponent(typeof v === "boolean" ? String(v) : v);
        const stripped = cleanUndefinedProperties(obj);
        return Object.keys(stripped)
            .map((k) => {
            const ks = encode(k) + "=";
            if (Array.isArray(stripped[k])) {
                return ks + stripped[k].map((v) => encode(v)).join(",");
            }
            else {
                return ks + encode(stripped[k]);
            }
        })
            .join("&");
    }
    /**
     * Checks if a version is within the provided range.
     *
     * @param  {String} version    The version to check.
     * @param  {String} minVersion The minimum supported version (inclusive).
     * @param  {String} maxVersion The minimum supported version (exclusive).
     * @throws {Error} If the version is outside of the provided range.
     */
    function checkVersion(version, minVersion, maxVersion) {
        const extract = (str) => str.split(".").map((x) => parseInt(x, 10));
        const [verMajor, verMinor] = extract(version);
        const [minMajor, minMinor] = extract(minVersion);
        const [maxMajor, maxMinor] = extract(maxVersion);
        const checks = [
            verMajor < minMajor,
            verMajor === minMajor && verMinor < minMinor,
            verMajor > maxMajor,
            verMajor === maxMajor && verMinor >= maxMinor,
        ];
        if (checks.some((x) => x)) {
            throw new Error(`Version ${version} doesn't satisfy ${minVersion} <= x < ${maxVersion}`);
        }
    }
    /**
     * Generates a decorator function ensuring a version check is performed against
     * the provided requirements before executing it.
     *
     * @param  {String} min The required min version (inclusive).
     * @param  {String} max The required max version (inclusive).
     * @return {Function}
     */
    function support(min, max) {
        return function (
        // @ts-ignore
        target, key, descriptor) {
            const fn = descriptor.value;
            return {
                configurable: true,
                get() {
                    const wrappedMethod = (...args) => {
                        // "this" is the current instance which its method is decorated.
                        const client = this.client ? this.client : this;
                        return client
                            .fetchHTTPApiVersion()
                            .then((version) => checkVersion(version, min, max))
                            .then(() => fn.apply(this, args));
                    };
                    Object.defineProperty(this, key, {
                        value: wrappedMethod,
                        configurable: true,
                        writable: true,
                    });
                    return wrappedMethod;
                },
            };
        };
    }
    /**
     * Generates a decorator function ensuring that the specified capabilities are
     * available on the server before executing it.
     *
     * @param  {Array<String>} capabilities The required capabilities.
     * @return {Function}
     */
    function capable(capabilities) {
        return function (
        // @ts-ignore
        target, key, descriptor) {
            const fn = descriptor.value;
            return {
                configurable: true,
                get() {
                    const wrappedMethod = (...args) => {
                        // "this" is the current instance which its method is decorated.
                        const client = this.client ? this.client : this;
                        return client
                            .fetchServerCapabilities()
                            .then((available) => {
                            const missing = capabilities.filter((c) => !(c in available));
                            if (missing.length > 0) {
                                const missingStr = missing.join(", ");
                                throw new Error(`Required capabilities ${missingStr} not present on server`);
                            }
                        })
                            .then(() => fn.apply(this, args));
                    };
                    Object.defineProperty(this, key, {
                        value: wrappedMethod,
                        configurable: true,
                        writable: true,
                    });
                    return wrappedMethod;
                },
            };
        };
    }
    /**
     * Generates a decorator function ensuring an operation is not performed from
     * within a batch request.
     *
     * @param  {String} message The error message to throw.
     * @return {Function}
     */
    function nobatch(message) {
        return function (
        // @ts-ignore
        target, key, descriptor) {
            const fn = descriptor.value;
            return {
                configurable: true,
                get() {
                    const wrappedMethod = (...args) => {
                        // "this" is the current instance which its method is decorated.
                        if (this._isBatch) {
                            throw new Error(message);
                        }
                        return fn.apply(this, args);
                    };
                    Object.defineProperty(this, key, {
                        value: wrappedMethod,
                        configurable: true,
                        writable: true,
                    });
                    return wrappedMethod;
                },
            };
        };
    }
    /**
     * Returns true if the specified value is an object (i.e. not an array nor null).
     * @param  {Object} thing The value to inspect.
     * @return {bool}
     */
    function isObject(thing) {
        return typeof thing === "object" && thing !== null && !Array.isArray(thing);
    }
    /**
     * Parses a data url.
     * @param  {String} dataURL The data url.
     * @return {Object}
     */
    function parseDataURL(dataURL) {
        const regex = /^data:(.*);base64,(.*)/;
        const match = dataURL.match(regex);
        if (!match) {
            throw new Error(`Invalid data-url: ${String(dataURL).substr(0, 32)}...`);
        }
        const props = match[1];
        const base64 = match[2];
        const [type, ...rawParams] = props.split(";");
        const params = rawParams.reduce((acc, param) => {
            const [key, value] = param.split("=");
            return Object.assign(Object.assign({}, acc), { [key]: value });
        }, {});
        return Object.assign(Object.assign({}, params), { type, base64 });
    }
    /**
     * Extracts file information from a data url.
     * @param  {String} dataURL The data url.
     * @return {Object}
     */
    function extractFileInfo(dataURL) {
        const { name, type, base64 } = parseDataURL(dataURL);
        const binary = atob(base64);
        const array = [];
        for (let i = 0; i < binary.length; i++) {
            array.push(binary.charCodeAt(i));
        }
        const blob = new Blob([new Uint8Array(array)], { type });
        return { blob, name };
    }
    /**
     * Creates a FormData instance from a data url and an existing JSON response
     * body.
     * @param  {String} dataURL            The data url.
     * @param  {Object} body               The response body.
     * @param  {Object} [options={}]       The options object.
     * @param  {Object} [options.filename] Force attachment file name.
     * @return {FormData}
     */
    function createFormData(dataURL, body, options = {}) {
        const { filename = "untitled" } = options;
        const { blob, name } = extractFileInfo(dataURL);
        const formData = new FormData();
        formData.append("attachment", blob, name || filename);
        for (const property in body) {
            if (typeof body[property] !== "undefined") {
                formData.append(property, JSON.stringify(body[property]));
            }
        }
        return formData;
    }
    /**
     * Clones an object with all its undefined keys removed.
     * @private
     */
    function cleanUndefinedProperties(obj) {
        const result = {};
        for (const key in obj) {
            if (typeof obj[key] !== "undefined") {
                result[key] = obj[key];
            }
        }
        return result;
    }
    /**
     * Handle common query parameters for Kinto requests.
     *
     * @param  {String}  [path]  The endpoint base path.
     * @param  {Array}   [options.fields]    Fields to limit the
     *   request to.
     * @param  {Object}  [options.query={}]  Additional query arguments.
     */
    function addEndpointOptions(path, options = {}) {
        const query = Object.assign({}, options.query);
        if (options.fields) {
            query._fields = options.fields;
        }
        const queryString = qsify(query);
        if (queryString) {
            return path + "?" + queryString;
        }
        return path;
    }
    /**
     * Replace authorization header with an obscured version
     */
    function obscureAuthorizationHeader(headers) {
        const h = new Headers(headers);
        if (h.has("authorization")) {
            h.set("authorization", "**** (suppressed)");
        }
        const obscuredHeaders = {};
        for (const [header, value] of h.entries()) {
            obscuredHeaders[header] = value;
        }
        return obscuredHeaders;
    }

    /**
     * Kinto server error code descriptors.
     */
    const ERROR_CODES = {
        104: "Missing Authorization Token",
        105: "Invalid Authorization Token",
        106: "Request body was not valid JSON",
        107: "Invalid request parameter",
        108: "Missing request parameter",
        109: "Invalid posted data",
        110: "Invalid Token / id",
        111: "Missing Token / id",
        112: "Content-Length header was not provided",
        113: "Request body too large",
        114: "Resource was created, updated or deleted meanwhile",
        115: "Method not allowed on this end point (hint: server may be readonly)",
        116: "Requested version not available on this server",
        117: "Client has sent too many requests",
        121: "Resource access is forbidden for this user",
        122: "Another resource violates constraint",
        201: "Service Temporary unavailable due to high load",
        202: "Service deprecated",
        999: "Internal Server Error",
    };
    class NetworkTimeoutError extends Error {
        constructor(url, options) {
            super(`Timeout while trying to access ${url} with ${JSON.stringify(options)}`);
            if (Error.captureStackTrace) {
                Error.captureStackTrace(this, NetworkTimeoutError);
            }
            this.url = url;
            this.options = options;
        }
    }
    class UnparseableResponseError extends Error {
        constructor(response, body, error) {
            const { status } = response;
            super(`Response from server unparseable (HTTP ${status || 0}; ${error}): ${body}`);
            if (Error.captureStackTrace) {
                Error.captureStackTrace(this, UnparseableResponseError);
            }
            this.status = status;
            this.response = response;
            this.stack = error.stack;
            this.error = error;
        }
    }
    /**
     * "Error" subclass representing a >=400 response from the server.
     *
     * Whether or not this is an error depends on your application.
     *
     * The `json` field can be undefined if the server responded with an
     * empty response body. This shouldn't generally happen. Most "bad"
     * responses come with a JSON error description, or (if they're
     * fronted by a CDN or nginx or something) occasionally non-JSON
     * responses (which become UnparseableResponseErrors, above).
     */
    class ServerResponse extends Error {
        constructor(response, json) {
            const { status } = response;
            let { statusText } = response;
            let errnoMsg;
            if (json) {
                // Try to fill in information from the JSON error.
                statusText = json.error || statusText;
                // Take errnoMsg from either ERROR_CODES or json.message.
                if (json.errno && json.errno in ERROR_CODES) {
                    errnoMsg = ERROR_CODES[json.errno];
                }
                else if (json.message) {
                    errnoMsg = json.message;
                }
                // If we had both ERROR_CODES and json.message, and they differ,
                // combine them.
                if (errnoMsg && json.message && json.message !== errnoMsg) {
                    errnoMsg += ` (${json.message})`;
                }
            }
            let message = `HTTP ${status} ${statusText}`;
            if (errnoMsg) {
                message += `: ${errnoMsg}`;
            }
            super(message.trim());
            if (Error.captureStackTrace) {
                Error.captureStackTrace(this, ServerResponse);
            }
            this.response = response;
            this.data = json;
        }
    }

    var errors = /*#__PURE__*/Object.freeze({
        __proto__: null,
        'default': ERROR_CODES,
        NetworkTimeoutError: NetworkTimeoutError,
        ServerResponse: ServerResponse,
        UnparseableResponseError: UnparseableResponseError
    });

    /**
     * Enhanced HTTP client for the Kinto protocol.
     * @private
     */
    class HTTP {
        /**
         * Constructor.
         *
         * @param {EventEmitter} events                       The event handler.
         * @param {Object}       [options={}}                 The options object.
         * @param {Number}       [options.timeout=null]       The request timeout in ms, if any (default: `null`).
         * @param {String}       [options.requestMode="cors"] The HTTP request mode (default: `"cors"`).
         */
        constructor(events, options = {}) {
            // public properties
            /**
             * The event emitter instance.
             * @type {EventEmitter}
             */
            this.events = events;
            /**
             * The request mode.
             * @see  https://fetch.spec.whatwg.org/#requestmode
             * @type {String}
             */
            this.requestMode = options.requestMode || HTTP.defaultOptions.requestMode;
            /**
             * The request timeout.
             * @type {Number}
             */
            this.timeout = options.timeout || HTTP.defaultOptions.timeout;
            /**
             * The fetch() function.
             * @type {Function}
             */
            this.fetchFunc = options.fetchFunc || globalThis.fetch.bind(globalThis);
        }
        /**
         * Default HTTP request headers applied to each outgoing request.
         *
         * @type {Object}
         */
        static get DEFAULT_REQUEST_HEADERS() {
            return {
                Accept: "application/json",
                "Content-Type": "application/json",
            };
        }
        /**
         * Default options.
         *
         * @type {Object}
         */
        static get defaultOptions() {
            return { timeout: null, requestMode: "cors" };
        }
        /**
         * @private
         */
        timedFetch(url, options) {
            let hasTimedout = false;
            return new Promise((resolve, reject) => {
                // Detect if a request has timed out.
                let _timeoutId;
                if (this.timeout) {
                    _timeoutId = setTimeout(() => {
                        hasTimedout = true;
                        if (options && options.headers) {
                            options = Object.assign(Object.assign({}, options), { headers: obscureAuthorizationHeader(options.headers) });
                        }
                        reject(new NetworkTimeoutError(url, options));
                    }, this.timeout);
                }
                function proceedWithHandler(fn) {
                    return (arg) => {
                        if (!hasTimedout) {
                            if (_timeoutId) {
                                clearTimeout(_timeoutId);
                            }
                            fn(arg);
                        }
                    };
                }
                this.fetchFunc(url, options)
                    .then(proceedWithHandler(resolve))
                    .catch(proceedWithHandler(reject));
            });
        }
        /**
         * @private
         */
        async processResponse(response) {
            const { status, headers } = response;
            const text = await response.text();
            // Check if we have a body; if so parse it as JSON.
            let json;
            if (text.length !== 0) {
                try {
                    json = JSON.parse(text);
                }
                catch (err) {
                    throw new UnparseableResponseError(response, text, err);
                }
            }
            if (status >= 400) {
                throw new ServerResponse(response, json);
            }
            return { status, json: json, headers };
        }
        /**
         * @private
         */
        async retry(url, retryAfter, request, options) {
            await delay(retryAfter);
            return this.request(url, request, Object.assign(Object.assign({}, options), { retry: options.retry - 1 }));
        }
        /**
         * Performs an HTTP request to the Kinto server.
         *
         * Resolves with an objet containing the following HTTP response properties:
         * - `{Number}  status`  The HTTP status code.
         * - `{Object}  json`    The JSON response body.
         * - `{Headers} headers` The response headers object; see the ES6 fetch() spec.
         *
         * @param  {String} url               The URL.
         * @param  {Object} [request={}]      The request object, passed to
         *     fetch() as its options object.
         * @param  {Object} [request.headers] The request headers object (default: {})
         * @param  {Object} [options={}]      Options for making the
         *     request
         * @param  {Number} [options.retry]   Number of retries (default: 0)
         * @return {Promise}
         */
        async request(url, request = { headers: {} }, options = { retry: 0 }) {
            // Ensure default request headers are always set
            request.headers = Object.assign(Object.assign({}, HTTP.DEFAULT_REQUEST_HEADERS), request.headers);
            // If a multipart body is provided, remove any custom Content-Type header as
            // the fetch() implementation will add the correct one for us.
            if (request.body && request.body instanceof FormData) {
                if (request.headers instanceof Headers) {
                    request.headers.delete("Content-Type");
                }
                else if (!Array.isArray(request.headers)) {
                    delete request.headers["Content-Type"];
                }
            }
            request.mode = this.requestMode;
            const response = await this.timedFetch(url, request);
            const { headers } = response;
            this._checkForDeprecationHeader(headers);
            this._checkForBackoffHeader(headers);
            // Check if the server summons the client to retry after a while.
            const retryAfter = this._checkForRetryAfterHeader(headers);
            // If number of allowed of retries is not exhausted, retry the same request.
            if (retryAfter && options.retry > 0) {
                return this.retry(url, retryAfter, request, options);
            }
            else {
                return this.processResponse(response);
            }
        }
        _checkForDeprecationHeader(headers) {
            const alertHeader = headers.get("Alert");
            if (!alertHeader) {
                return;
            }
            let alert;
            try {
                alert = JSON.parse(alertHeader);
            }
            catch (err) {
                console.warn("Unable to parse Alert header message", alertHeader);
                return;
            }
            console.warn(alert.message, alert.url);
            if (this.events) {
                this.events.emit("deprecated", alert);
            }
        }
        _checkForBackoffHeader(headers) {
            let backoffMs;
            const backoffHeader = headers.get("Backoff");
            const backoffSeconds = backoffHeader ? parseInt(backoffHeader, 10) : 0;
            if (backoffSeconds > 0) {
                backoffMs = new Date().getTime() + backoffSeconds * 1000;
            }
            else {
                backoffMs = 0;
            }
            if (this.events) {
                this.events.emit("backoff", backoffMs);
            }
        }
        _checkForRetryAfterHeader(headers) {
            const retryAfter = headers.get("Retry-After");
            if (!retryAfter) {
                return;
            }
            const delay = parseInt(retryAfter, 10) * 1000;
            const tryAgainAfter = new Date().getTime() + delay;
            if (this.events) {
                this.events.emit("retry-after", tryAgainAfter);
            }
            return delay;
        }
    }

    /**
     * Endpoints templates.
     * @type {Object}
     */
    const ENDPOINTS = {
        root: () => "/",
        batch: () => "/batch",
        permissions: () => "/permissions",
        bucket: (bucket) => "/buckets" + (bucket ? `/${bucket}` : ""),
        history: (bucket) => `${ENDPOINTS.bucket(bucket)}/history`,
        collection: (bucket, coll) => `${ENDPOINTS.bucket(bucket)}/collections` + (coll ? `/${coll}` : ""),
        group: (bucket, group) => `${ENDPOINTS.bucket(bucket)}/groups` + (group ? `/${group}` : ""),
        record: (bucket, coll, id) => `${ENDPOINTS.collection(bucket, coll)}/records` + (id ? `/${id}` : ""),
        attachment: (bucket, coll, id) => `${ENDPOINTS.record(bucket, coll, id)}/attachment`,
    };

    const requestDefaults = {
        safe: false,
        // check if we should set default content type here
        headers: {},
        patch: false,
    };
    /**
     * @private
     */
    function safeHeader(safe, last_modified) {
        if (!safe) {
            return {};
        }
        if (last_modified) {
            return { "If-Match": `"${last_modified}"` };
        }
        return { "If-None-Match": "*" };
    }
    /**
     * @private
     */
    function createRequest(path, { data, permissions }, options = {}) {
        const { headers, safe } = Object.assign(Object.assign({}, requestDefaults), options);
        const method = options.method || (data && data.id) ? "PUT" : "POST";
        return {
            method,
            path,
            headers: Object.assign(Object.assign({}, headers), safeHeader(safe)),
            body: { data, permissions },
        };
    }
    /**
     * @private
     */
    function updateRequest(path, { data, permissions }, options = {}) {
        const { headers, safe, patch } = Object.assign(Object.assign({}, requestDefaults), options);
        const { last_modified } = Object.assign(Object.assign({}, data), options);
        const hasNoData = data &&
            Object.keys(data).filter((k) => k !== "id" && k !== "last_modified")
                .length === 0;
        if (hasNoData) {
            data = undefined;
        }
        return {
            method: patch ? "PATCH" : "PUT",
            path,
            headers: Object.assign(Object.assign({}, headers), safeHeader(safe, last_modified)),
            body: { data, permissions },
        };
    }
    /**
     * @private
     */
    function jsonPatchPermissionsRequest(path, permissions, opType, options = {}) {
        const { headers, safe, last_modified } = Object.assign(Object.assign({}, requestDefaults), options);
        const ops = [];
        for (const [type, principals] of Object.entries(permissions)) {
            if (principals) {
                for (const principal of principals) {
                    ops.push({
                        op: opType,
                        path: `/permissions/${type}/${principal}`,
                    });
                }
            }
        }
        return {
            method: "PATCH",
            path,
            headers: Object.assign(Object.assign(Object.assign({}, headers), safeHeader(safe, last_modified)), { "Content-Type": "application/json-patch+json" }),
            body: ops,
        };
    }
    /**
     * @private
     */
    function deleteRequest(path, options = {}) {
        const { headers, safe, last_modified } = Object.assign(Object.assign({}, requestDefaults), options);
        if (safe && !last_modified) {
            throw new Error("Safe concurrency check requires a last_modified value.");
        }
        return {
            method: "DELETE",
            path,
            headers: Object.assign(Object.assign({}, headers), safeHeader(safe, last_modified)),
        };
    }
    /**
     * @private
     */
    function addAttachmentRequest(path, dataURI, { data, permissions } = {}, options = {}) {
        const { headers, safe, gzipped } = Object.assign(Object.assign({}, requestDefaults), options);
        const { last_modified } = Object.assign(Object.assign({}, data), options);
        const body = { data, permissions };
        const formData = createFormData(dataURI, body, options);
        const customPath = `${path}${gzipped !== null ? "?gzipped=" + (gzipped ? "true" : "false") : ""}`;
        return {
            method: "POST",
            path: customPath,
            headers: Object.assign(Object.assign({}, headers), safeHeader(safe, last_modified)),
            body: formData,
        };
    }

    /**
     * Exports batch responses as a result object.
     *
     * @private
     * @param  {Array} responses The batch subrequest responses.
     * @param  {Array} requests  The initial issued requests.
     * @return {Object}
     */
    function aggregate(responses = [], requests = []) {
        if (responses.length !== requests.length) {
            throw new Error("Responses length should match requests one.");
        }
        const results = {
            errors: [],
            published: [],
            conflicts: [],
            skipped: [],
        };
        return responses.reduce((acc, response, index) => {
            const { status } = response;
            const request = requests[index];
            if (status >= 200 && status < 400) {
                acc.published.push(response.body);
            }
            else if (status === 404) {
                // Extract the id manually from request path while waiting for Kinto/kinto#818
                const regex = /(buckets|groups|collections|records)\/([^/]+)$/;
                const extracts = request.path.match(regex);
                const id = extracts && extracts.length === 3 ? extracts[2] : undefined;
                acc.skipped.push({
                    id,
                    path: request.path,
                    error: response.body,
                });
            }
            else if (status === 412) {
                acc.conflicts.push({
                    // XXX: specifying the type is probably superfluous
                    type: "outgoing",
                    local: request.body,
                    remote: (response.body.details && response.body.details.existing) || null,
                });
            }
            else {
                acc.errors.push({
                    path: request.path,
                    sent: request,
                    error: response.body,
                });
            }
            return acc;
        }, results);
    }

    // Unique ID creation requires a high quality random # generator. In the browser we therefore
    // require the crypto API and do not support built-in fallback to lower quality random number
    // generators (like Math.random()).
    var getRandomValues;
    var rnds8 = new Uint8Array(16);
    function rng() {
        // lazy load so that environments that need to polyfill have a chance to do so
        if (!getRandomValues) {
            // getRandomValues needs to be invoked in a context where "this" is a Crypto implementation. Also,
            // find the complete implementation of crypto (msCrypto) on IE11.
            getRandomValues = typeof crypto !== 'undefined' && crypto.getRandomValues && crypto.getRandomValues.bind(crypto) || typeof msCrypto !== 'undefined' && typeof msCrypto.getRandomValues === 'function' && msCrypto.getRandomValues.bind(msCrypto);
            if (!getRandomValues) {
                throw new Error('crypto.getRandomValues() not supported. See https://github.com/uuidjs/uuid#getrandomvalues-not-supported');
            }
        }
        return getRandomValues(rnds8);
    }

    var REGEX = /^(?:[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}|00000000-0000-0000-0000-000000000000)$/i;

    function validate(uuid) {
        return typeof uuid === 'string' && REGEX.test(uuid);
    }

    /**
     * Convert array of 16 byte values to UUID string format of the form:
     * XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
     */
    var byteToHex = [];
    for (var i = 0; i < 256; ++i) {
        byteToHex.push((i + 0x100).toString(16).substr(1));
    }
    function stringify(arr) {
        var offset = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0;
        // Note: Be careful editing this code!  It's been tuned for performance
        // and works in ways you may not expect. See https://github.com/uuidjs/uuid/pull/434
        var uuid = (byteToHex[arr[offset + 0]] + byteToHex[arr[offset + 1]] + byteToHex[arr[offset + 2]] + byteToHex[arr[offset + 3]] + '-' + byteToHex[arr[offset + 4]] + byteToHex[arr[offset + 5]] + '-' + byteToHex[arr[offset + 6]] + byteToHex[arr[offset + 7]] + '-' + byteToHex[arr[offset + 8]] + byteToHex[arr[offset + 9]] + '-' + byteToHex[arr[offset + 10]] + byteToHex[arr[offset + 11]] + byteToHex[arr[offset + 12]] + byteToHex[arr[offset + 13]] + byteToHex[arr[offset + 14]] + byteToHex[arr[offset + 15]]).toLowerCase(); // Consistency check for valid UUID.  If this throws, it's likely due to one
        // of the following:
        // - One or more input array values don't map to a hex octet (leading to
        // "undefined" in the uuid)
        // - Invalid input values for the RFC `version` or `variant` fields
        if (!validate(uuid)) {
            throw TypeError('Stringified UUID is invalid');
        }
        return uuid;
    }

    function v4(options, buf, offset) {
        options = options || {};
        var rnds = options.random || (options.rng || rng)(); // Per 4.4, set bits for version and `clock_seq_hi_and_reserved`
        rnds[6] = rnds[6] & 0x0f | 0x40;
        rnds[8] = rnds[8] & 0x3f | 0x80; // Copy bytes to buffer, if provided
        if (buf) {
            offset = offset || 0;
            for (var i = 0; i < 16; ++i) {
                buf[offset + i] = rnds[i];
            }
            return buf;
        }
        return stringify(rnds);
    }

    /**
     * Abstract representation of a selected collection.
     *
     */
    class Collection {
        /**
         * Constructor.
         *
         * @param  {KintoClient}  client            The client instance.
         * @param  {Bucket}       bucket            The bucket instance.
         * @param  {String}       name              The collection name.
         * @param  {Object}       [options={}]      The options object.
         * @param  {Object}       [options.headers] The headers object option.
         * @param  {Boolean}      [options.safe]    The safe option.
         * @param  {Number}       [options.retry]   The retry option.
         * @param  {Boolean}      [options.batch]   (Private) Whether this
         *     Collection is operating as part of a batch.
         */
        constructor(client, bucket, name, options = {}) {
            /**
             * @ignore
             */
            this.client = client;
            /**
             * @ignore
             */
            this.bucket = bucket;
            /**
             * The collection name.
             * @type {String}
             */
            this.name = name;
            this._endpoints = client.endpoints;
            /**
             * @ignore
             */
            this._retry = options.retry || 0;
            this._safe = !!options.safe;
            // FIXME: This is kind of ugly; shouldn't the bucket be responsible
            // for doing the merge?
            this._headers = Object.assign(Object.assign({}, this.bucket.headers), options.headers);
        }
        get execute() {
            return this.client.execute.bind(this.client);
        }
        /**
         * Get the value of "headers" for a given request, merging the
         * per-request headers with our own "default" headers.
         *
         * @private
         */
        _getHeaders(options) {
            return Object.assign(Object.assign({}, this._headers), options.headers);
        }
        /**
         * Get the value of "safe" for a given request, using the
         * per-request option if present or falling back to our default
         * otherwise.
         *
         * @private
         * @param {Object} options The options for a request.
         * @returns {Boolean}
         */
        _getSafe(options) {
            return Object.assign({ safe: this._safe }, options).safe;
        }
        /**
         * As _getSafe, but for "retry".
         *
         * @private
         */
        _getRetry(options) {
            return Object.assign({ retry: this._retry }, options).retry;
        }
        /**
         * Retrieves the total number of records in this collection.
         *
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.headers] The headers object option.
         * @param  {Number} [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Number, Error>}
         */
        async getTotalRecords(options = {}) {
            const path = this._endpoints.record(this.bucket.name, this.name);
            const request = {
                headers: this._getHeaders(options),
                path,
                method: "HEAD",
            };
            const { headers } = await this.client.execute(request, {
                raw: true,
                retry: this._getRetry(options),
            });
            return parseInt(headers.get("Total-Records"), 10);
        }
        /**
         * Retrieves the ETag of the records list, for use with the `since` filtering option.
         *
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.headers] The headers object option.
         * @param  {Number} [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<String, Error>}
         */
        async getRecordsTimestamp(options = {}) {
            const path = this._endpoints.record(this.bucket.name, this.name);
            const request = {
                headers: this._getHeaders(options),
                path,
                method: "HEAD",
            };
            const { headers } = (await this.client.execute(request, {
                raw: true,
                retry: this._getRetry(options),
            }));
            return headers.get("ETag");
        }
        /**
         * Retrieves collection data.
         *
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.headers] The headers object option.
         * @param  {Object} [options.query]   Query parameters to pass in
         *     the request. This might be useful for features that aren't
         *     yet supported by this library.
         * @param  {Array}  [options.fields]  Limit response to
         *     just some fields.
         * @param  {Number} [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Object, Error>}
         */
        async getData(options = {}) {
            const path = this._endpoints.collection(this.bucket.name, this.name);
            const request = { headers: this._getHeaders(options), path };
            const { data } = (await this.client.execute(request, {
                retry: this._getRetry(options),
                query: options.query,
                fields: options.fields,
            }));
            return data;
        }
        /**
         * Set collection data.
         * @param  {Object}   data                    The collection data object.
         * @param  {Object}   [options={}]            The options object.
         * @param  {Object}   [options.headers]       The headers object option.
         * @param  {Number}   [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Boolean}  [options.safe]          The safe option.
         * @param  {Boolean}  [options.patch]         The patch option.
         * @param  {Number}   [options.last_modified] The last_modified option.
         * @return {Promise<Object, Error>}
         */
        async setData(data, options = {}) {
            if (!isObject(data)) {
                throw new Error("A collection object is required.");
            }
            const { patch, permissions } = options;
            const { last_modified } = Object.assign(Object.assign({}, data), options);
            const path = this._endpoints.collection(this.bucket.name, this.name);
            const request = updateRequest(path, { data, permissions }, {
                last_modified,
                patch,
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Retrieves the list of permissions for this collection.
         *
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.headers] The headers object option.
         * @param  {Number} [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Object, Error>}
         */
        async getPermissions(options = {}) {
            const path = this._endpoints.collection(this.bucket.name, this.name);
            const request = { headers: this._getHeaders(options), path };
            const { permissions } = (await this.client.execute(request, {
                retry: this._getRetry(options),
            }));
            return permissions;
        }
        /**
         * Replaces all existing collection permissions with the ones provided.
         *
         * @param  {Object}   permissions             The permissions object.
         * @param  {Object}   [options={}]            The options object
         * @param  {Object}   [options.headers]       The headers object option.
         * @param  {Number}   [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Boolean}  [options.safe]          The safe option.
         * @param  {Number}   [options.last_modified] The last_modified option.
         * @return {Promise<Object, Error>}
         */
        async setPermissions(permissions, options = {}) {
            if (!isObject(permissions)) {
                throw new Error("A permissions object is required.");
            }
            const path = this._endpoints.collection(this.bucket.name, this.name);
            const data = { last_modified: options.last_modified };
            const request = updateRequest(path, { data, permissions }, {
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Append principals to the collection permissions.
         *
         * @param  {Object}  permissions             The permissions object.
         * @param  {Object}  [options={}]            The options object
         * @param  {Boolean} [options.safe]          The safe option.
         * @param  {Object}  [options.headers]       The headers object option.
         * @param  {Number}  [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Object}  [options.last_modified] The last_modified option.
         * @return {Promise<Object, Error>}
         */
        async addPermissions(permissions, options = {}) {
            if (!isObject(permissions)) {
                throw new Error("A permissions object is required.");
            }
            const path = this._endpoints.collection(this.bucket.name, this.name);
            const { last_modified } = options;
            const request = jsonPatchPermissionsRequest(path, permissions, "add", {
                last_modified,
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Remove principals from the collection permissions.
         *
         * @param  {Object}  permissions             The permissions object.
         * @param  {Object}  [options={}]            The options object
         * @param  {Boolean} [options.safe]          The safe option.
         * @param  {Object}  [options.headers]       The headers object option.
         * @param  {Number}  [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Object}  [options.last_modified] The last_modified option.
         * @return {Promise<Object, Error>}
         */
        async removePermissions(permissions, options = {}) {
            if (!isObject(permissions)) {
                throw new Error("A permissions object is required.");
            }
            const path = this._endpoints.collection(this.bucket.name, this.name);
            const { last_modified } = options;
            const request = jsonPatchPermissionsRequest(path, permissions, "remove", {
                last_modified,
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Creates a record in current collection.
         *
         * @param  {Object}  record                The record to create.
         * @param  {Object}  [options={}]          The options object.
         * @param  {Object}  [options.headers]     The headers object option.
         * @param  {Number}  [options.retry=0]     Number of retries to make
         *     when faced with transient errors.
         * @param  {Boolean} [options.safe]        The safe option.
         * @param  {Object}  [options.permissions] The permissions option.
         * @return {Promise<Object, Error>}
         */
        async createRecord(record, options = {}) {
            const { permissions } = options;
            const path = this._endpoints.record(this.bucket.name, this.name, record.id);
            const request = createRequest(path, { data: record, permissions }, {
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Adds an attachment to a record, creating the record when it doesn't exist.
         *
         * @param  {String}  dataURL                 The data url.
         * @param  {Object}  [record={}]             The record data.
         * @param  {Object}  [options={}]            The options object.
         * @param  {Object}  [options.headers]       The headers object option.
         * @param  {Number}  [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Boolean} [options.safe]          The safe option.
         * @param  {Number}  [options.last_modified] The last_modified option.
         * @param  {Object}  [options.permissions]   The permissions option.
         * @param  {String}  [options.filename]      Force the attachment filename.
         * @param  {String}  [options.gzipped]       Force the attachment to be gzipped or not.
         * @return {Promise<Object, Error>}
         */
        async addAttachment(dataURI, record = {}, options = {}) {
            const { permissions } = options;
            const id = record.id || v4();
            const path = this._endpoints.attachment(this.bucket.name, this.name, id);
            const { last_modified } = Object.assign(Object.assign({}, record), options);
            const addAttachmentRequest$1 = addAttachmentRequest(path, dataURI, { data: record, permissions }, {
                last_modified,
                filename: options.filename,
                gzipped: options.gzipped,
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            await this.client.execute(addAttachmentRequest$1, {
                stringify: false,
                retry: this._getRetry(options),
            });
            return this.getRecord(id);
        }
        /**
         * Removes an attachment from a given record.
         *
         * @param  {Object}  recordId                The record id.
         * @param  {Object}  [options={}]            The options object.
         * @param  {Object}  [options.headers]       The headers object option.
         * @param  {Number}  [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Boolean} [options.safe]          The safe option.
         * @param  {Number}  [options.last_modified] The last_modified option.
         */
        async removeAttachment(recordId, options = {}) {
            const { last_modified } = options;
            const path = this._endpoints.attachment(this.bucket.name, this.name, recordId);
            const request = deleteRequest(path, {
                last_modified,
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Updates a record in current collection.
         *
         * @param  {Object}  record                  The record to update.
         * @param  {Object}  [options={}]            The options object.
         * @param  {Object}  [options.headers]       The headers object option.
         * @param  {Number}  [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Boolean} [options.safe]          The safe option.
         * @param  {Number}  [options.last_modified] The last_modified option.
         * @param  {Object}  [options.permissions]   The permissions option.
         * @return {Promise<Object, Error>}
         */
        async updateRecord(record, options = {}) {
            if (!isObject(record)) {
                throw new Error("A record object is required.");
            }
            if (!record.id) {
                throw new Error("A record id is required.");
            }
            const { permissions } = options;
            const { last_modified } = Object.assign(Object.assign({}, record), options);
            const path = this._endpoints.record(this.bucket.name, this.name, record.id);
            const request = updateRequest(path, { data: record, permissions }, {
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
                last_modified,
                patch: !!options.patch,
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Deletes a record from the current collection.
         *
         * @param  {Object|String} record                  The record to delete.
         * @param  {Object}        [options={}]            The options object.
         * @param  {Object}        [options.headers]       The headers object option.
         * @param  {Number}        [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Boolean}       [options.safe]          The safe option.
         * @param  {Number}        [options.last_modified] The last_modified option.
         * @return {Promise<Object, Error>}
         */
        async deleteRecord(record, options = {}) {
            const recordObj = toDataBody(record);
            if (!recordObj.id) {
                throw new Error("A record id is required.");
            }
            const { id } = recordObj;
            const { last_modified } = Object.assign(Object.assign({}, recordObj), options);
            const path = this._endpoints.record(this.bucket.name, this.name, id);
            const request = deleteRequest(path, {
                last_modified,
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Deletes records from the current collection.
         *
         * Sorting is done by passing a `sort` string option:
         *
         * - The field to order the results by, prefixed with `-` for descending.
         * Default: `-last_modified`.
         *
         * @see http://kinto.readthedocs.io/en/stable/api/1.x/sorting.html
         *
         * Filtering is done by passing a `filters` option object:
         *
         * - `{fieldname: "value"}`
         * - `{min_fieldname: 4000}`
         * - `{in_fieldname: "1,2,3"}`
         * - `{not_fieldname: 0}`
         * - `{exclude_fieldname: "0,1"}`
         *
         * @see http://kinto.readthedocs.io/en/stable/api/1.x/filtering.html
         *
         * @param  {Object}   [options={}]                    The options object.
         * @param  {Object}   [options.headers]               The headers object option.
         * @param  {Number}   [options.retry=0]               Number of retries to make
         *     when faced with transient errors.
         * @param  {Object}   [options.filters={}]            The filters object.
         * @param  {String}   [options.sort="-last_modified"] The sort field.
         * @param  {String}   [options.at]                    The timestamp to get a snapshot at.
         * @param  {String}   [options.limit=null]            The limit field.
         * @param  {String}   [options.pages=1]               The number of result pages to aggregate.
         * @param  {Number}   [options.since=null]            Only retrieve records modified since the provided timestamp.
         * @param  {Array}    [options.fields]                Limit response to just some fields.
         * @return {Promise<Object, Error>}
         */
        async deleteRecords(options = {}) {
            const path = this._endpoints.record(this.bucket.name, this.name);
            return this.client.paginatedDelete(path, options, {
                headers: this._getHeaders(options),
                retry: this._getRetry(options),
            });
        }
        /**
         * Retrieves a record from the current collection.
         *
         * @param  {String} id                The record id to retrieve.
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.headers] The headers object option.
         * @param  {Object} [options.query]   Query parameters to pass in
         *     the request. This might be useful for features that aren't
         *     yet supported by this library.
         * @param  {Array}  [options.fields]  Limit response to
         *     just some fields.
         * @param  {Number} [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Object, Error>}
         */
        async getRecord(id, options = {}) {
            const path = this._endpoints.record(this.bucket.name, this.name, id);
            const request = { headers: this._getHeaders(options), path };
            return this.client.execute(request, {
                retry: this._getRetry(options),
                query: options.query,
                fields: options.fields,
            });
        }
        /**
         * Lists records from the current collection.
         *
         * Sorting is done by passing a `sort` string option:
         *
         * - The field to order the results by, prefixed with `-` for descending.
         * Default: `-last_modified`.
         *
         * @see http://kinto.readthedocs.io/en/stable/api/1.x/sorting.html
         *
         * Filtering is done by passing a `filters` option object:
         *
         * - `{fieldname: "value"}`
         * - `{min_fieldname: 4000}`
         * - `{in_fieldname: "1,2,3"}`
         * - `{not_fieldname: 0}`
         * - `{exclude_fieldname: "0,1"}`
         *
         * @see http://kinto.readthedocs.io/en/stable/api/1.x/filtering.html
         *
         * Paginating is done by passing a `limit` option, then calling the `next()`
         * method from the resolved result object to fetch the next page, if any.
         *
         * @param  {Object}   [options={}]                    The options object.
         * @param  {Object}   [options.headers]               The headers object option.
         * @param  {Number}   [options.retry=0]               Number of retries to make
         *     when faced with transient errors.
         * @param  {Object}   [options.filters={}]            The filters object.
         * @param  {String}   [options.sort="-last_modified"] The sort field.
         * @param  {String}   [options.at]                    The timestamp to get a snapshot at.
         * @param  {String}   [options.limit=null]            The limit field.
         * @param  {String}   [options.pages=1]               The number of result pages to aggregate.
         * @param  {Number}   [options.since=null]            Only retrieve records modified since the provided timestamp.
         * @param  {Array}    [options.fields]                Limit response to just some fields.
         * @return {Promise<Object, Error>}
         */
        async listRecords(options = {}) {
            const path = this._endpoints.record(this.bucket.name, this.name);
            if (options.at) {
                return this.getSnapshot(options.at);
            }
            else {
                return this.client.paginatedList(path, options, {
                    headers: this._getHeaders(options),
                    retry: this._getRetry(options),
                });
            }
        }
        /**
         * @private
         */
        async isHistoryComplete() {
            // We consider that if we have the collection creation event part of the
            // history, then all records change events have been tracked.
            const { data: [oldestHistoryEntry], } = await this.bucket.listHistory({
                limit: 1,
                filters: {
                    action: "create",
                    resource_name: "collection",
                    collection_id: this.name,
                },
            });
            return !!oldestHistoryEntry;
        }
        /**
         * @private
         */
        async getSnapshot(at) {
            if (!at || !Number.isInteger(at) || at <= 0) {
                throw new Error("Invalid argument, expected a positive integer.");
            }
            // Retrieve history and check it covers the required time range.
            // Ensure we have enough history data to retrieve the complete list of
            // changes.
            if (!(await this.isHistoryComplete())) {
                throw new Error("Computing a snapshot is only possible when the full history for a " +
                    "collection is available. Here, the history plugin seems to have " +
                    "been enabled after the creation of the collection.");
            }
            // Because of https://github.com/Kinto/kinto-http.js/issues/963
            // we cannot simply rely on the history endpoint.
            // Our strategy here is to clean-up the history entries from the
            // records that were deleted via the plural endpoint.
            // We will detect them by comparing the current state of the collection
            // and the full history of the collection since its genesis.
            // List full history of collection.
            const { data: fullHistory } = await this.bucket.listHistory({
                pages: Infinity,
                sort: "last_modified",
                filters: {
                    resource_name: "record",
                    collection_id: this.name,
                },
            });
            // Keep latest entry ever, and latest within snapshot window.
            // (history is sorted chronologically)
            const latestEver = new Map();
            const latestInSnapshot = new Map();
            for (const entry of fullHistory) {
                if (entry.target.data.last_modified <= at) {
                    // Snapshot includes changes right on timestamp.
                    latestInSnapshot.set(entry.record_id, entry);
                }
                latestEver.set(entry.record_id, entry);
            }
            // Current records ids in the collection.
            const { data: current } = await this.listRecords({
                pages: Infinity,
                fields: ["id"], // we don't need attributes.
            });
            const currentIds = new Set(current.map((record) => record.id));
            // If a record is not in the current collection, and its
            // latest history entry isn't a delete then this means that
            // it was deleted via the plural endpoint (and that we lost track
            // of this deletion because of bug #963)
            const deletedViaPlural = new Set();
            for (const entry of latestEver.values()) {
                if (entry.action != "delete" && !currentIds.has(entry.record_id)) {
                    deletedViaPlural.add(entry.record_id);
                }
            }
            // Now reconstruct the collection based on latest version in snapshot
            // filtering all deleted records.
            const reconstructed = [];
            for (const entry of latestInSnapshot.values()) {
                if (entry.action != "delete" && !deletedViaPlural.has(entry.record_id)) {
                    reconstructed.push(entry.target.data);
                }
            }
            return {
                last_modified: String(at),
                data: Array.from(reconstructed).sort((a, b) => b.last_modified - a.last_modified),
                next: () => {
                    throw new Error("Snapshots don't support pagination");
                },
                hasNextPage: false,
                totalRecords: reconstructed.length,
            };
        }
        /**
         * Performs batch operations at the current collection level.
         *
         * @param  {Function} fn                   The batch operation function.
         * @param  {Object}   [options={}]         The options object.
         * @param  {Object}   [options.headers]    The headers object option.
         * @param  {Boolean}  [options.safe]       The safe option.
         * @param  {Number}   [options.retry]      The retry option.
         * @param  {Boolean}  [options.aggregate]  Produces a grouped result object.
         * @return {Promise<Object, Error>}
         */
        async batch(fn, options = {}) {
            return this.client.batch(fn, {
                bucket: this.bucket.name,
                collection: this.name,
                headers: this._getHeaders(options),
                retry: this._getRetry(options),
                safe: this._getSafe(options),
                aggregate: !!options.aggregate,
            });
        }
    }
    __decorate([
        capable(["attachments"])
    ], Collection.prototype, "addAttachment", null);
    __decorate([
        capable(["attachments"])
    ], Collection.prototype, "removeAttachment", null);
    __decorate([
        capable(["history"])
    ], Collection.prototype, "getSnapshot", null);

    /**
     * Abstract representation of a selected bucket.
     *
     */
    class Bucket {
        /**
         * Constructor.
         *
         * @param  {KintoClient} client            The client instance.
         * @param  {String}      name              The bucket name.
         * @param  {Object}      [options={}]      The headers object option.
         * @param  {Object}      [options.headers] The headers object option.
         * @param  {Boolean}     [options.safe]    The safe option.
         * @param  {Number}      [options.retry]   The retry option.
         */
        constructor(client, name, options = {}) {
            /**
             * @ignore
             */
            this.client = client;
            /**
             * The bucket name.
             * @type {String}
             */
            this.name = name;
            this._endpoints = client.endpoints;
            /**
             * @ignore
             */
            this._headers = options.headers || {};
            this._retry = options.retry || 0;
            this._safe = !!options.safe;
        }
        get execute() {
            return this.client.execute.bind(this.client);
        }
        get headers() {
            return this._headers;
        }
        /**
         * Get the value of "headers" for a given request, merging the
         * per-request headers with our own "default" headers.
         *
         * @private
         */
        _getHeaders(options) {
            return Object.assign(Object.assign({}, this._headers), options.headers);
        }
        /**
         * Get the value of "safe" for a given request, using the
         * per-request option if present or falling back to our default
         * otherwise.
         *
         * @private
         * @param {Object} options The options for a request.
         * @returns {Boolean}
         */
        _getSafe(options) {
            return Object.assign({ safe: this._safe }, options).safe;
        }
        /**
         * As _getSafe, but for "retry".
         *
         * @private
         */
        _getRetry(options) {
            return Object.assign({ retry: this._retry }, options).retry;
        }
        /**
         * Selects a collection.
         *
         * @param  {String}  name              The collection name.
         * @param  {Object}  [options={}]      The options object.
         * @param  {Object}  [options.headers] The headers object option.
         * @param  {Boolean} [options.safe]    The safe option.
         * @return {Collection}
         */
        collection(name, options = {}) {
            return new Collection(this.client, this, name, {
                headers: this._getHeaders(options),
                retry: this._getRetry(options),
                safe: this._getSafe(options),
            });
        }
        /**
         * Retrieves the ETag of the collection list, for use with the `since` filtering option.
         *
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.headers] The headers object option.
         * @param  {Number} [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<String, Error>}
         */
        async getCollectionsTimestamp(options = {}) {
            const path = this._endpoints.collection(this.name);
            const request = {
                headers: this._getHeaders(options),
                path,
                method: "HEAD",
            };
            const { headers } = (await this.client.execute(request, {
                raw: true,
                retry: this._getRetry(options),
            }));
            return headers.get("ETag");
        }
        /**
         * Retrieves the ETag of the group list, for use with the `since` filtering option.
         *
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.headers] The headers object option.
         * @param  {Number} [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<String, Error>}
         */
        async getGroupsTimestamp(options = {}) {
            const path = this._endpoints.group(this.name);
            const request = {
                headers: this._getHeaders(options),
                path,
                method: "HEAD",
            };
            const { headers } = (await this.client.execute(request, {
                raw: true,
                retry: this._getRetry(options),
            }));
            return headers.get("ETag");
        }
        /**
         * Retrieves bucket data.
         *
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.headers] The headers object option.
         * @param  {Object} [options.query]   Query parameters to pass in
         *     the request. This might be useful for features that aren't
         *     yet supported by this library.
         * @param  {Array}  [options.fields]  Limit response to
         *     just some fields.
         * @param  {Number} [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Object, Error>}
         */
        async getData(options = {}) {
            const path = this._endpoints.bucket(this.name);
            const request = {
                headers: this._getHeaders(options),
                path,
            };
            const { data } = (await this.client.execute(request, {
                retry: this._getRetry(options),
                query: options.query,
                fields: options.fields,
            }));
            return data;
        }
        /**
         * Set bucket data.
         * @param  {Object}  data                    The bucket data object.
         * @param  {Object}  [options={}]            The options object.
         * @param  {Object}  [options.headers={}]    The headers object option.
         * @param  {Boolean} [options.safe]          The safe option.
         * @param  {Number}  [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Boolean} [options.patch]         The patch option.
         * @param  {Number}  [options.last_modified] The last_modified option.
         * @return {Promise<Object, Error>}
         */
        async setData(data, options = {}) {
            if (!isObject(data)) {
                throw new Error("A bucket object is required.");
            }
            const bucket = Object.assign(Object.assign({}, data), { id: this.name });
            // For default bucket, we need to drop the id from the data object.
            // Bug in Kinto < 3.1.1
            const bucketId = bucket.id;
            if (bucket.id === "default") {
                delete bucket.id;
            }
            const path = this._endpoints.bucket(bucketId);
            const { patch, permissions } = options;
            const { last_modified } = Object.assign(Object.assign({}, data), options);
            const request = updateRequest(path, { data: bucket, permissions }, {
                last_modified,
                patch,
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Retrieves the list of history entries in the current bucket.
         *
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.headers] The headers object option.
         * @param  {Number} [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Array<Object>, Error>}
         */
        async listHistory(options = {}) {
            const path = this._endpoints.history(this.name);
            return this.client.paginatedList(path, options, {
                headers: this._getHeaders(options),
                retry: this._getRetry(options),
            });
        }
        /**
         * Retrieves the list of collections in the current bucket.
         *
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.filters={}] The filters object.
         * @param  {Object} [options.headers] The headers object option.
         * @param  {Number} [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @param  {Array}  [options.fields]  Limit response to
         *     just some fields.
         * @return {Promise<Array<Object>, Error>}
         */
        async listCollections(options = {}) {
            const path = this._endpoints.collection(this.name);
            return this.client.paginatedList(path, options, {
                headers: this._getHeaders(options),
                retry: this._getRetry(options),
            });
        }
        /**
         * Creates a new collection in current bucket.
         *
         * @param  {String|undefined}  id          The collection id.
         * @param  {Object}  [options={}]          The options object.
         * @param  {Boolean} [options.safe]        The safe option.
         * @param  {Object}  [options.headers]     The headers object option.
         * @param  {Number}  [options.retry=0]     Number of retries to make
         *     when faced with transient errors.
         * @param  {Object}  [options.permissions] The permissions object.
         * @param  {Object}  [options.data]        The data object.
         * @return {Promise<Object, Error>}
         */
        async createCollection(id, options = {}) {
            const { permissions, data = {} } = options;
            data.id = id;
            const path = this._endpoints.collection(this.name, id);
            const request = createRequest(path, { data, permissions }, {
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Deletes a collection from the current bucket.
         *
         * @param  {Object|String} collection              The collection to delete.
         * @param  {Object}        [options={}]            The options object.
         * @param  {Object}        [options.headers]       The headers object option.
         * @param  {Number}        [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Boolean}       [options.safe]          The safe option.
         * @param  {Number}        [options.last_modified] The last_modified option.
         * @return {Promise<Object, Error>}
         */
        async deleteCollection(collection, options = {}) {
            const collectionObj = toDataBody(collection);
            if (!collectionObj.id) {
                throw new Error("A collection id is required.");
            }
            const { id } = collectionObj;
            const { last_modified } = Object.assign(Object.assign({}, collectionObj), options);
            const path = this._endpoints.collection(this.name, id);
            const request = deleteRequest(path, {
                last_modified,
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Deletes collections from the current bucket.
         *
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.filters={}] The filters object.
         * @param  {Object} [options.headers] The headers object option.
         * @param  {Number} [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @param  {Array}  [options.fields]  Limit response to
         *     just some fields.
         * @return {Promise<Array<Object>, Error>}
         */
        async deleteCollections(options = {}) {
            const path = this._endpoints.collection(this.name);
            return this.client.paginatedDelete(path, options, {
                headers: this._getHeaders(options),
                retry: this._getRetry(options),
            });
        }
        /**
         * Retrieves the list of groups in the current bucket.
         *
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.filters={}] The filters object.
         * @param  {Object} [options.headers] The headers object option.
         * @param  {Number} [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @param  {Array}  [options.fields]  Limit response to
         *     just some fields.
         * @return {Promise<Array<Object>, Error>}
         */
        async listGroups(options = {}) {
            const path = this._endpoints.group(this.name);
            return this.client.paginatedList(path, options, {
                headers: this._getHeaders(options),
                retry: this._getRetry(options),
            });
        }
        /**
         * Fetches a group in current bucket.
         *
         * @param  {String} id                The group id.
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.headers] The headers object option.
         * @param  {Number} [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @param  {Object} [options.query]   Query parameters to pass in
         *     the request. This might be useful for features that aren't
         *     yet supported by this library.
         * @param  {Array}  [options.fields]  Limit response to
         *     just some fields.
         * @return {Promise<Object, Error>}
         */
        async getGroup(id, options = {}) {
            const path = this._endpoints.group(this.name, id);
            const request = {
                headers: this._getHeaders(options),
                path,
            };
            return this.client.execute(request, {
                retry: this._getRetry(options),
                query: options.query,
                fields: options.fields,
            });
        }
        /**
         * Creates a new group in current bucket.
         *
         * @param  {String|undefined}  id                    The group id.
         * @param  {Array<String>}     [members=[]]          The list of principals.
         * @param  {Object}            [options={}]          The options object.
         * @param  {Object}            [options.data]        The data object.
         * @param  {Object}            [options.permissions] The permissions object.
         * @param  {Boolean}           [options.safe]        The safe option.
         * @param  {Object}            [options.headers]     The headers object option.
         * @param  {Number}            [options.retry=0]     Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Object, Error>}
         */
        async createGroup(id, members = [], options = {}) {
            const data = Object.assign(Object.assign({}, options.data), { id,
                members });
            const path = this._endpoints.group(this.name, id);
            const { permissions } = options;
            const request = createRequest(path, { data, permissions }, {
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Updates an existing group in current bucket.
         *
         * @param  {Object}  group                   The group object.
         * @param  {Object}  [options={}]            The options object.
         * @param  {Object}  [options.data]          The data object.
         * @param  {Object}  [options.permissions]   The permissions object.
         * @param  {Boolean} [options.safe]          The safe option.
         * @param  {Object}  [options.headers]       The headers object option.
         * @param  {Number}  [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Number}  [options.last_modified] The last_modified option.
         * @return {Promise<Object, Error>}
         */
        async updateGroup(group, options = {}) {
            if (!isObject(group)) {
                throw new Error("A group object is required.");
            }
            if (!group.id) {
                throw new Error("A group id is required.");
            }
            const data = Object.assign(Object.assign({}, options.data), group);
            const path = this._endpoints.group(this.name, group.id);
            const { patch, permissions } = options;
            const { last_modified } = Object.assign(Object.assign({}, data), options);
            const request = updateRequest(path, { data, permissions }, {
                last_modified,
                patch,
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Deletes a group from the current bucket.
         *
         * @param  {Object|String} group                   The group to delete.
         * @param  {Object}        [options={}]            The options object.
         * @param  {Object}        [options.headers]       The headers object option.
         * @param  {Number}        [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Boolean}       [options.safe]          The safe option.
         * @param  {Number}        [options.last_modified] The last_modified option.
         * @return {Promise<Object, Error>}
         */
        async deleteGroup(group, options = {}) {
            const groupObj = toDataBody(group);
            const { id } = groupObj;
            const { last_modified } = Object.assign(Object.assign({}, groupObj), options);
            const path = this._endpoints.group(this.name, id);
            const request = deleteRequest(path, {
                last_modified,
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Deletes groups from the current bucket.
         *
         * @param  {Object} [options={}]          The options object.
         * @param  {Object} [options.filters={}]  The filters object.
         * @param  {Object} [options.headers]     The headers object option.
         * @param  {Number} [options.retry=0]     Number of retries to make
         *     when faced with transient errors.
         * @param  {Array}  [options.fields]      Limit response to
         *     just some fields.
         * @return {Promise<Array<Object>, Error>}
         */
        async deleteGroups(options = {}) {
            const path = this._endpoints.group(this.name);
            return this.client.paginatedDelete(path, options, {
                headers: this._getHeaders(options),
                retry: this._getRetry(options),
            });
        }
        /**
         * Retrieves the list of permissions for this bucket.
         *
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.headers] The headers object option.
         * @param  {Number} [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Object, Error>}
         */
        async getPermissions(options = {}) {
            const request = {
                headers: this._getHeaders(options),
                path: this._endpoints.bucket(this.name),
            };
            const { permissions } = (await this.client.execute(request, {
                retry: this._getRetry(options),
            }));
            return permissions;
        }
        /**
         * Replaces all existing bucket permissions with the ones provided.
         *
         * @param  {Object}  permissions             The permissions object.
         * @param  {Object}  [options={}]            The options object
         * @param  {Boolean} [options.safe]          The safe option.
         * @param  {Object}  [options.headers={}]    The headers object option.
         * @param  {Number}  [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Object}  [options.last_modified] The last_modified option.
         * @return {Promise<Object, Error>}
         */
        async setPermissions(permissions, options = {}) {
            if (!isObject(permissions)) {
                throw new Error("A permissions object is required.");
            }
            const path = this._endpoints.bucket(this.name);
            const { last_modified } = options;
            const data = { last_modified };
            const request = updateRequest(path, { data, permissions }, {
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Append principals to the bucket permissions.
         *
         * @param  {Object}  permissions             The permissions object.
         * @param  {Object}  [options={}]            The options object
         * @param  {Boolean} [options.safe]          The safe option.
         * @param  {Object}  [options.headers]       The headers object option.
         * @param  {Number}  [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Object}  [options.last_modified] The last_modified option.
         * @return {Promise<Object, Error>}
         */
        async addPermissions(permissions, options = {}) {
            if (!isObject(permissions)) {
                throw new Error("A permissions object is required.");
            }
            const path = this._endpoints.bucket(this.name);
            const { last_modified } = options;
            const request = jsonPatchPermissionsRequest(path, permissions, "add", {
                last_modified,
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Remove principals from the bucket permissions.
         *
         * @param  {Object}  permissions             The permissions object.
         * @param  {Object}  [options={}]            The options object
         * @param  {Boolean} [options.safe]          The safe option.
         * @param  {Object}  [options.headers]       The headers object option.
         * @param  {Number}  [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Object}  [options.last_modified] The last_modified option.
         * @return {Promise<Object, Error>}
         */
        async removePermissions(permissions, options = {}) {
            if (!isObject(permissions)) {
                throw new Error("A permissions object is required.");
            }
            const path = this._endpoints.bucket(this.name);
            const { last_modified } = options;
            const request = jsonPatchPermissionsRequest(path, permissions, "remove", {
                last_modified,
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            });
            return this.client.execute(request, {
                retry: this._getRetry(options),
            });
        }
        /**
         * Performs batch operations at the current bucket level.
         *
         * @param  {Function} fn                   The batch operation function.
         * @param  {Object}   [options={}]         The options object.
         * @param  {Object}   [options.headers]    The headers object option.
         * @param  {Boolean}  [options.safe]       The safe option.
         * @param  {Number}   [options.retry=0]    The retry option.
         * @param  {Boolean}  [options.aggregate]  Produces a grouped result object.
         * @return {Promise<Object, Error>}
         */
        async batch(fn, options = {}) {
            return this.client.batch(fn, {
                bucket: this.name,
                headers: this._getHeaders(options),
                retry: this._getRetry(options),
                safe: this._getSafe(options),
                aggregate: !!options.aggregate,
            });
        }
    }
    __decorate([
        capable(["history"])
    ], Bucket.prototype, "listHistory", null);

    /**
     * Currently supported protocol version.
     * @type {String}
     */
    const SUPPORTED_PROTOCOL_VERSION = "v1";
    /**
     * High level HTTP client for the Kinto API.
     *
     * @example
     * const client = new KintoClient("https://kinto.dev.mozaws.net/v1");
     * client.bucket("default")
     *    .collection("my-blog")
     *    .createRecord({title: "First article"})
     *   .then(console.log.bind(console))
     *   .catch(console.error.bind(console));
     */
    class KintoClientBase {
        /**
         * Constructor.
         *
         * @param  {String}       remote  The remote URL.
         * @param  {Object}       [options={}]                  The options object.
         * @param  {Boolean}      [options.safe=true]           Adds concurrency headers to every requests.
         * @param  {EventEmitter} [options.events=EventEmitter] The events handler instance.
         * @param  {Object}       [options.headers={}]          The key-value headers to pass to each request.
         * @param  {Object}       [options.retry=0]             Number of retries when request fails (default: 0)
         * @param  {String}       [options.bucket="default"]    The default bucket to use.
         * @param  {String}       [options.requestMode="cors"]  The HTTP request mode (from ES6 fetch spec).
         * @param  {Number}       [options.timeout=null]        The request timeout in ms, if any.
         * @param  {Function}     [options.fetchFunc=fetch]     The function to be used to execute HTTP requests.
         */
        constructor(remote, options) {
            if (typeof remote !== "string" || !remote.length) {
                throw new Error("Invalid remote URL: " + remote);
            }
            if (remote[remote.length - 1] === "/") {
                remote = remote.slice(0, -1);
            }
            this._backoffReleaseTime = null;
            this._requests = [];
            this._isBatch = !!options.batch;
            this._retry = options.retry || 0;
            this._safe = !!options.safe;
            this._headers = options.headers || {};
            // public properties
            /**
             * The remote server base URL.
             * @type {String}
             */
            this.remote = remote;
            /**
             * Current server information.
             * @ignore
             * @type {Object|null}
             */
            this.serverInfo = null;
            /**
             * The event emitter instance. Should comply with the `EventEmitter`
             * interface.
             * @ignore
             * @type {Class}
             */
            this.events = options.events;
            this.endpoints = ENDPOINTS;
            const { fetchFunc, requestMode, timeout } = options;
            /**
             * The HTTP instance.
             * @ignore
             * @type {HTTP}
             */
            this.http = new HTTP(this.events, { fetchFunc, requestMode, timeout });
            this._registerHTTPEvents();
        }
        /**
         * The remote endpoint base URL. Setting the value will also extract and
         * validate the version.
         * @type {String}
         */
        get remote() {
            return this._remote;
        }
        /**
         * @ignore
         */
        set remote(url) {
            let version;
            try {
                version = url.match(/\/(v\d+)\/?$/)[1];
            }
            catch (err) {
                throw new Error("The remote URL must contain the version: " + url);
            }
            if (version !== SUPPORTED_PROTOCOL_VERSION) {
                throw new Error(`Unsupported protocol version: ${version}`);
            }
            this._remote = url;
            this._version = version;
        }
        /**
         * The current server protocol version, eg. `v1`.
         * @type {String}
         */
        get version() {
            return this._version;
        }
        /**
         * Backoff remaining time, in milliseconds. Defaults to zero if no backoff is
         * ongoing.
         *
         * @type {Number}
         */
        get backoff() {
            const currentTime = new Date().getTime();
            if (this._backoffReleaseTime && currentTime < this._backoffReleaseTime) {
                return this._backoffReleaseTime - currentTime;
            }
            return 0;
        }
        /**
         * Registers HTTP events.
         * @private
         */
        _registerHTTPEvents() {
            // Prevent registering event from a batch client instance
            if (!this._isBatch && this.events) {
                this.events.on("backoff", (backoffMs) => {
                    this._backoffReleaseTime = backoffMs;
                });
            }
        }
        /**
         * Retrieve a bucket object to perform operations on it.
         *
         * @param  {String}  name              The bucket name.
         * @param  {Object}  [options={}]      The request options.
         * @param  {Boolean} [options.safe]    The resulting safe option.
         * @param  {Number}  [options.retry]   The resulting retry option.
         * @param  {Object}  [options.headers] The extended headers object option.
         * @return {Bucket}
         */
        bucket(name, options = {}) {
            return new Bucket(this, name, {
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
                retry: this._getRetry(options),
            });
        }
        /**
         * Set client "headers" for every request, updating previous headers (if any).
         *
         * @param {Object} headers The headers to merge with existing ones.
         */
        setHeaders(headers) {
            this._headers = Object.assign(Object.assign({}, this._headers), headers);
            this.serverInfo = null;
        }
        /**
         * Get the value of "headers" for a given request, merging the
         * per-request headers with our own "default" headers.
         *
         * Note that unlike other options, headers aren't overridden, but
         * merged instead.
         *
         * @private
         * @param {Object} options The options for a request.
         * @returns {Object}
         */
        _getHeaders(options) {
            return Object.assign(Object.assign({}, this._headers), options.headers);
        }
        /**
         * Get the value of "safe" for a given request, using the
         * per-request option if present or falling back to our default
         * otherwise.
         *
         * @private
         * @param {Object} options The options for a request.
         * @returns {Boolean}
         */
        _getSafe(options) {
            return Object.assign({ safe: this._safe }, options).safe;
        }
        /**
         * As _getSafe, but for "retry".
         *
         * @private
         */
        _getRetry(options) {
            return Object.assign({ retry: this._retry }, options).retry;
        }
        /**
         * Retrieves the server's "hello" endpoint. This endpoint reveals
         * server capabilities and settings as well as telling the client
         * "who they are" according to their given authorization headers.
         *
         * @private
         * @param  {Object}  [options={}] The request options.
         * @param  {Object}  [options.headers={}] Headers to use when making
         *     this request.
         * @param  {Number}  [options.retry=0]    Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Object, Error>}
         */
        async _getHello(options = {}) {
            const path = this.remote + ENDPOINTS.root();
            const { json } = await this.http.request(path, { headers: this._getHeaders(options) }, { retry: this._getRetry(options) });
            return json;
        }
        /**
         * Retrieves server information and persist them locally. This operation is
         * usually performed a single time during the instance lifecycle.
         *
         * @param  {Object}  [options={}] The request options.
         * @param  {Number}  [options.retry=0]    Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Object, Error>}
         */
        async fetchServerInfo(options = {}) {
            if (this.serverInfo) {
                return this.serverInfo;
            }
            this.serverInfo = await this._getHello({ retry: this._getRetry(options) });
            return this.serverInfo;
        }
        /**
         * Retrieves Kinto server settings.
         *
         * @param  {Object}  [options={}] The request options.
         * @param  {Number}  [options.retry=0]    Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Object, Error>}
         */
        async fetchServerSettings(options = {}) {
            const { settings } = await this.fetchServerInfo(options);
            return settings;
        }
        /**
         * Retrieve server capabilities information.
         *
         * @param  {Object}  [options={}] The request options.
         * @param  {Number}  [options.retry=0]    Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Object, Error>}
         */
        async fetchServerCapabilities(options = {}) {
            const { capabilities } = await this.fetchServerInfo(options);
            return capabilities;
        }
        /**
         * Retrieve authenticated user information.
         *
         * @param  {Object}  [options={}] The request options.
         * @param  {Object}  [options.headers={}] Headers to use when making
         *     this request.
         * @param  {Number}  [options.retry=0]    Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Object, Error>}
         */
        async fetchUser(options = {}) {
            const { user } = await this._getHello(options);
            return user;
        }
        /**
         * Retrieve authenticated user information.
         *
         * @param  {Object}  [options={}] The request options.
         * @param  {Number}  [options.retry=0]    Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Object, Error>}
         */
        async fetchHTTPApiVersion(options = {}) {
            const { http_api_version } = await this.fetchServerInfo(options);
            return http_api_version;
        }
        /**
         * Process batch requests, chunking them according to the batch_max_requests
         * server setting when needed.
         *
         * @param  {Array}  requests     The list of batch subrequests to perform.
         * @param  {Object} [options={}] The options object.
         * @return {Promise<Object, Error>}
         */
        async _batchRequests(requests, options = {}) {
            const headers = this._getHeaders(options);
            if (!requests.length) {
                return [];
            }
            const serverSettings = await this.fetchServerSettings({
                retry: this._getRetry(options),
            });
            const maxRequests = serverSettings["batch_max_requests"];
            if (maxRequests && requests.length > maxRequests) {
                const chunks = partition(requests, maxRequests);
                const results = [];
                for (const chunk of chunks) {
                    const result = await this._batchRequests(chunk, options);
                    results.push(...result);
                }
                return results;
            }
            const { responses } = (await this.execute({
                // FIXME: is this really necessary, since it's also present in
                // the "defaults"?
                headers,
                path: ENDPOINTS.batch(),
                method: "POST",
                body: {
                    defaults: { headers },
                    requests,
                },
            }, { retry: this._getRetry(options) }));
            return responses;
        }
        /**
         * Sends batch requests to the remote server.
         *
         * Note: Reserved for internal use only.
         *
         * @ignore
         * @param  {Function} fn                        The function to use for describing batch ops.
         * @param  {Object}   [options={}]              The options object.
         * @param  {Boolean}  [options.safe]            The safe option.
         * @param  {Number}   [options.retry]           The retry option.
         * @param  {String}   [options.bucket]          The bucket name option.
         * @param  {String}   [options.collection]      The collection name option.
         * @param  {Object}   [options.headers]         The headers object option.
         * @param  {Boolean}  [options.aggregate=false] Produces an aggregated result object.
         * @return {Promise<Object, Error>}
         */
        async batch(fn, options = {}) {
            const rootBatch = new KintoClientBase(this.remote, {
                events: this.events,
                batch: true,
                safe: this._getSafe(options),
                retry: this._getRetry(options),
            });
            if (options.bucket && options.collection) {
                fn(rootBatch.bucket(options.bucket).collection(options.collection));
            }
            else if (options.bucket) {
                fn(rootBatch.bucket(options.bucket));
            }
            else {
                fn(rootBatch);
            }
            const responses = await this._batchRequests(rootBatch._requests, options);
            if (options.aggregate) {
                return aggregate(responses, rootBatch._requests);
            }
            else {
                return responses;
            }
        }
        /**
         * Executes an atomic HTTP request.
         *
         * @param  {Object}  request             The request object.
         * @param  {String}  request.path        The path to fetch, relative
         *     to the Kinto server root.
         * @param  {String}  [request.method="GET"] The method to use in the
         *     request.
         * @param  {Body}    [request.body]      The request body.
         * @param  {Object}  [request.headers={}] The request headers.
         * @param  {Object}  [options={}]        The options object.
         * @param  {Boolean} [options.raw=false] If true, resolve with full response
         * @param  {Boolean} [options.stringify=true] If true, serialize body data to
         * @param  {Number}  [options.retry=0]   The number of times to
         *     retry a request if the server responds with Retry-After.
         * JSON.
         * @return {Promise<Object, Error>}
         */
        async execute(request, options = {}) {
            const { raw = false, stringify = true } = options;
            // If we're within a batch, add the request to the stack to send at once.
            if (this._isBatch) {
                this._requests.push(request);
                // Resolve with a message in case people attempt at consuming the result
                // from within a batch operation.
                const msg = ("This result is generated from within a batch " +
                    "operation and should not be consumed.");
                return raw
                    ? { status: 0, json: msg, headers: new Headers() }
                    : msg;
            }
            const uri = this.remote + addEndpointOptions(request.path, options);
            const result = await this.http.request(uri, cleanUndefinedProperties({
                // Limit requests to only those parts that would be allowed in
                // a batch request -- don't pass through other fancy fetch()
                // options like integrity, redirect, mode because they will
                // break on a batch request.  A batch request only allows
                // headers, method, path (above), and body.
                method: request.method,
                headers: request.headers,
                body: stringify ? JSON.stringify(request.body) : request.body,
            }), { retry: this._getRetry(options) });
            return raw ? result : result.json;
        }
        /**
         * Perform an operation with a given HTTP method on some pages from
         * a paginated list, following the `next-page` header automatically
         * until we have processed the requested number of pages. Return a
         * response with a `.next()` method that can be called to perform
         * the requested HTTP method on more results.
         *
         * @private
         * @param  {String}  path
         *     The path to make the request to.
         * @param  {Object}  params
         *     The parameters to use when making the request.
         * @param  {String}  [params.sort="-last_modified"]
         *     The sorting order to use when doing operation on pages.
         * @param  {Object}  [params.filters={}]
         *     The filters to send in the request.
         * @param  {Number}  [params.limit=undefined]
         *     The limit to send in the request. Undefined means no limit.
         * @param  {Number}  [params.pages=undefined]
         *     The number of pages to operate on. Undefined means one page. Pass
         *     Infinity to operate on everything.
         * @param  {String}  [params.since=undefined]
         *     The ETag from which to start doing operation on pages.
         * @param  {Array}   [params.fields]
         *     Limit response to just some fields.
         * @param  {Object}  [options={}]
         *     Additional request-level parameters to use in all requests.
         * @param  {Object}  [options.headers={}]
         *     Headers to use during all requests.
         * @param  {Number}  [options.retry=0]
         *     Number of times to retry each request if the server responds
         *     with Retry-After.
         * @param  {String}  [options.method="GET"]
         *     The method to use in the request.
         */
        async paginatedOperation(path, params = {}, options = {}) {
            // FIXME: this is called even in batch requests, which doesn't
            // make any sense (since all batch requests get a "dummy"
            // response; see execute() above).
            const { sort, filters, limit, pages, since, fields } = Object.assign({ sort: "-last_modified" }, params);
            // Safety/Consistency check on ETag value.
            if (since && typeof since !== "string") {
                throw new Error(`Invalid value for since (${since}), should be ETag value.`);
            }
            const query = Object.assign(Object.assign({}, filters), { _sort: sort, _limit: limit, _since: since });
            if (fields) {
                query._fields = fields;
            }
            const querystring = qsify(query);
            let results = [], current = 0;
            const next = async function (nextPage) {
                if (!nextPage) {
                    throw new Error("Pagination exhausted.");
                }
                return processNextPage(nextPage);
            };
            const processNextPage = async (nextPage) => {
                const { headers } = options;
                return handleResponse(await this.http.request(nextPage, { headers }));
            };
            const pageResults = (results, nextPage, etag) => {
                // ETag string is supposed to be opaque and stored «as-is».
                // ETag header values are quoted (because of * and W/"foo").
                return {
                    last_modified: etag ? etag.replace(/"/g, "") : etag,
                    data: results,
                    next: next.bind(null, nextPage),
                    hasNextPage: !!nextPage,
                    totalRecords: -1,
                };
            };
            const handleResponse = async function ({ headers = new Headers(), json = {}, }) {
                const nextPage = headers.get("Next-Page");
                const etag = headers.get("ETag");
                if (!pages) {
                    return pageResults(json.data, nextPage, etag);
                }
                // Aggregate new results with previous ones
                results = results.concat(json.data);
                current += 1;
                if (current >= pages || !nextPage) {
                    // Pagination exhausted
                    return pageResults(results, nextPage, etag);
                }
                // Follow next page
                return processNextPage(nextPage);
            };
            return handleResponse((await this.execute(
            // N.B.: This doesn't use _getHeaders, because all calls to
            // `paginatedList` are assumed to come from calls that already
            // have headers merged at e.g. the bucket or collection level.
            {
                headers: options.headers ? options.headers : {},
                path: path + "?" + querystring,
                method: options.method,
            }, 
            // N.B. This doesn't use _getRetry, because all calls to
            // `paginatedList` are assumed to come from calls that already
            // used `_getRetry` at e.g. the bucket or collection level.
            { raw: true, retry: options.retry || 0 })));
        }
        /**
         * Fetch some pages from a paginated list, following the `next-page`
         * header automatically until we have fetched the requested number
         * of pages. Return a response with a `.next()` method that can be
         * called to fetch more results.
         *
         * @private
         * @param  {String}  path
         *     The path to make the request to.
         * @param  {Object}  params
         *     The parameters to use when making the request.
         * @param  {String}  [params.sort="-last_modified"]
         *     The sorting order to use when fetching.
         * @param  {Object}  [params.filters={}]
         *     The filters to send in the request.
         * @param  {Number}  [params.limit=undefined]
         *     The limit to send in the request. Undefined means no limit.
         * @param  {Number}  [params.pages=undefined]
         *     The number of pages to fetch. Undefined means one page. Pass
         *     Infinity to fetch everything.
         * @param  {String}  [params.since=undefined]
         *     The ETag from which to start fetching.
         * @param  {Array}   [params.fields]
         *     Limit response to just some fields.
         * @param  {Object}  [options={}]
         *     Additional request-level parameters to use in all requests.
         * @param  {Object}  [options.headers={}]
         *     Headers to use during all requests.
         * @param  {Number}  [options.retry=0]
         *     Number of times to retry each request if the server responds
         *     with Retry-After.
         */
        async paginatedList(path, params = {}, options = {}) {
            return this.paginatedOperation(path, params, options);
        }
        /**
         * Delete multiple objects, following the pagination if the number of
         * objects exceeds the page limit until we have deleted the requested
         * number of pages. Return a response with a `.next()` method that can
         * be called to delete more results.
         *
         * @private
         * @param  {String}  path
         *     The path to make the request to.
         * @param  {Object}  params
         *     The parameters to use when making the request.
         * @param  {String}  [params.sort="-last_modified"]
         *     The sorting order to use when deleting.
         * @param  {Object}  [params.filters={}]
         *     The filters to send in the request.
         * @param  {Number}  [params.limit=undefined]
         *     The limit to send in the request. Undefined means no limit.
         * @param  {Number}  [params.pages=undefined]
         *     The number of pages to delete. Undefined means one page. Pass
         *     Infinity to delete everything.
         * @param  {String}  [params.since=undefined]
         *     The ETag from which to start deleting.
         * @param  {Array}   [params.fields]
         *     Limit response to just some fields.
         * @param  {Object}  [options={}]
         *     Additional request-level parameters to use in all requests.
         * @param  {Object}  [options.headers={}]
         *     Headers to use during all requests.
         * @param  {Number}  [options.retry=0]
         *     Number of times to retry each request if the server responds
         *     with Retry-After.
         */
        paginatedDelete(path, params = {}, options = {}) {
            const { headers, safe, last_modified } = options;
            const deleteRequest$1 = deleteRequest(path, {
                headers,
                safe: safe ? safe : false,
                last_modified,
            });
            return this.paginatedOperation(path, params, Object.assign(Object.assign({}, options), { headers: deleteRequest$1.headers, method: "DELETE" }));
        }
        /**
         * Lists all permissions.
         *
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.headers={}] Headers to use when making
         *     this request.
         * @param  {Number} [options.retry=0]    Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Object[], Error>}
         */
        async listPermissions(options = {}) {
            const path = ENDPOINTS.permissions();
            // Ensure the default sort parameter is something that exists in permissions
            // entries, as `last_modified` doesn't; here, we pick "id".
            const paginationOptions = Object.assign({ sort: "id" }, options);
            return this.paginatedList(path, paginationOptions, {
                headers: this._getHeaders(options),
                retry: this._getRetry(options),
            });
        }
        /**
         * Retrieves the list of buckets.
         *
         * @param  {Object} [options={}]      The options object.
         * @param  {Object} [options.headers={}] Headers to use when making
         *     this request.
         * @param  {Number} [options.retry=0]    Number of retries to make
         *     when faced with transient errors.
         * @param  {Object} [options.filters={}] The filters object.
         * @param  {Array}  [options.fields]     Limit response to
         *     just some fields.
         * @return {Promise<Object[], Error>}
         */
        async listBuckets(options = {}) {
            const path = ENDPOINTS.bucket();
            return this.paginatedList(path, options, {
                headers: this._getHeaders(options),
                retry: this._getRetry(options),
            });
        }
        /**
         * Creates a new bucket on the server.
         *
         * @param  {String|null}  id                The bucket name (optional).
         * @param  {Object}       [options={}]      The options object.
         * @param  {Boolean}      [options.data]    The bucket data option.
         * @param  {Boolean}      [options.safe]    The safe option.
         * @param  {Object}       [options.headers] The headers object option.
         * @param  {Number}       [options.retry=0] Number of retries to make
         *     when faced with transient errors.
         * @return {Promise<Object, Error>}
         */
        async createBucket(id, options = {}) {
            const { data, permissions } = options;
            const _data = Object.assign(Object.assign({}, data), { id: id ? id : undefined });
            const path = _data.id ? ENDPOINTS.bucket(_data.id) : ENDPOINTS.bucket();
            return this.execute(createRequest(path, { data: _data, permissions }, {
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            }), { retry: this._getRetry(options) });
        }
        /**
         * Deletes a bucket from the server.
         *
         * @ignore
         * @param  {Object|String} bucket                  The bucket to delete.
         * @param  {Object}        [options={}]            The options object.
         * @param  {Boolean}       [options.safe]          The safe option.
         * @param  {Object}        [options.headers]       The headers object option.
         * @param  {Number}        [options.retry=0]       Number of retries to make
         *     when faced with transient errors.
         * @param  {Number}        [options.last_modified] The last_modified option.
         * @return {Promise<Object, Error>}
         */
        async deleteBucket(bucket, options = {}) {
            const bucketObj = toDataBody(bucket);
            if (!bucketObj.id) {
                throw new Error("A bucket id is required.");
            }
            const path = ENDPOINTS.bucket(bucketObj.id);
            const { last_modified } = Object.assign(Object.assign({}, bucketObj), options);
            return this.execute(deleteRequest(path, {
                last_modified,
                headers: this._getHeaders(options),
                safe: this._getSafe(options),
            }), { retry: this._getRetry(options) });
        }
        /**
         * Deletes buckets.
         *
         * @param  {Object} [options={}]             The options object.
         * @param  {Boolean} [options.safe]          The safe option.
         * @param  {Object} [options.headers={}]     Headers to use when making
         *     this request.
         * @param  {Number} [options.retry=0]        Number of retries to make
         *     when faced with transient errors.
         * @param  {Object} [options.filters={}]     The filters object.
         * @param  {Array}  [options.fields]         Limit response to
         *     just some fields.
         * @param  {Number}  [options.last_modified] The last_modified option.
         * @return {Promise<Object[], Error>}
         */
        async deleteBuckets(options = {}) {
            const path = ENDPOINTS.bucket();
            return this.paginatedDelete(path, options, {
                headers: this._getHeaders(options),
                retry: this._getRetry(options),
                safe: options.safe,
                last_modified: options.last_modified,
            });
        }
        async createAccount(username, password) {
            return this.execute(createRequest(`/accounts/${username}`, { data: { password } }, { method: "PUT" }));
        }
    }
    __decorate([
        nobatch("This operation is not supported within a batch operation.")
    ], KintoClientBase.prototype, "fetchServerSettings", null);
    __decorate([
        nobatch("This operation is not supported within a batch operation.")
    ], KintoClientBase.prototype, "fetchServerCapabilities", null);
    __decorate([
        nobatch("This operation is not supported within a batch operation.")
    ], KintoClientBase.prototype, "fetchUser", null);
    __decorate([
        nobatch("This operation is not supported within a batch operation.")
    ], KintoClientBase.prototype, "fetchHTTPApiVersion", null);
    __decorate([
        nobatch("Can't use batch within a batch!")
    ], KintoClientBase.prototype, "batch", null);
    __decorate([
        capable(["permissions_endpoint"])
    ], KintoClientBase.prototype, "listPermissions", null);
    __decorate([
        support("1.4", "2.0")
    ], KintoClientBase.prototype, "deleteBuckets", null);
    __decorate([
        capable(["accounts"])
    ], KintoClientBase.prototype, "createAccount", null);

    /*
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    const { EventEmitter } = ChromeUtils.import("resource://gre/modules/EventEmitter.jsm");
    class KintoHttpClient extends KintoClientBase {
        constructor(remote, options = {}) {
            const events = {};
            EventEmitter.decorate(events);
            super(remote, Object.assign({ events: events }, options));
        }
    }
    KintoHttpClient.errors = errors;

    return KintoHttpClient;

})));
