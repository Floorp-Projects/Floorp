"use strict";
/**
 * Copyright 2017 Google Inc. All rights reserved.
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
var __classPrivateFieldSet = (this && this.__classPrivateFieldSet) || function (receiver, state, value, kind, f) {
    if (kind === "m") throw new TypeError("Private method is not writable");
    if (kind === "a" && !f) throw new TypeError("Private accessor was defined without a setter");
    if (typeof state === "function" ? receiver !== state || !f : !state.has(receiver)) throw new TypeError("Cannot write private member to an object whose class did not declare it");
    return (kind === "a" ? f.call(receiver, value) : f ? f.value = value : state.set(receiver, value)), value;
};
var __classPrivateFieldGet = (this && this.__classPrivateFieldGet) || function (receiver, state, kind, f) {
    if (kind === "a" && !f) throw new TypeError("Private accessor was defined without a getter");
    if (typeof state === "function" ? receiver !== state || !f : !state.has(receiver)) throw new TypeError("Cannot read private member from an object whose class did not declare it");
    return kind === "m" ? f : kind === "a" ? f.call(receiver) : f ? f.value : state.get(receiver);
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
var _TestServer_dirPath, _TestServer_server, _TestServer_wsServer, _TestServer_startTime, _TestServer_cachedPathPrefix, _TestServer_connections, _TestServer_routes, _TestServer_auths, _TestServer_csp, _TestServer_gzipRoutes, _TestServer_requestSubscribers, _TestServer_onServerConnection, _TestServer_onRequest, _TestServer_onWebSocketConnection;
Object.defineProperty(exports, "__esModule", { value: true });
exports.TestServer = void 0;
const assert_1 = __importDefault(require("assert"));
const fs_1 = require("fs");
const http_1 = require("http");
const https_1 = require("https");
const mime_1 = require("mime");
const path_1 = require("path");
const ws_1 = require("ws");
const zlib_1 = require("zlib");
class TestServer {
    constructor(dirPath, sslOptions) {
        _TestServer_dirPath.set(this, void 0);
        _TestServer_server.set(this, void 0);
        _TestServer_wsServer.set(this, void 0);
        _TestServer_startTime.set(this, new Date());
        _TestServer_cachedPathPrefix.set(this, void 0);
        _TestServer_connections.set(this, new Set());
        _TestServer_routes.set(this, new Map());
        _TestServer_auths.set(this, new Map());
        _TestServer_csp.set(this, new Map());
        _TestServer_gzipRoutes.set(this, new Set());
        _TestServer_requestSubscribers.set(this, new Map());
        _TestServer_onServerConnection.set(this, (connection) => {
            __classPrivateFieldGet(this, _TestServer_connections, "f").add(connection);
            // ECONNRESET is a legit error given
            // that tab closing simply kills process.
            connection.on('error', error => {
                if (error.code !== 'ECONNRESET') {
                    throw error;
                }
            });
            connection.once('close', () => {
                return __classPrivateFieldGet(this, _TestServer_connections, "f").delete(connection);
            });
        });
        _TestServer_onRequest.set(this, (request, response) => {
            request.on('error', (error) => {
                if (error.code === 'ECONNRESET') {
                    response.end();
                }
                else {
                    throw error;
                }
            });
            request.postBody = new Promise(resolve => {
                let body = '';
                request.on('data', (chunk) => {
                    return (body += chunk);
                });
                request.on('end', () => {
                    return resolve(body);
                });
            });
            (0, assert_1.default)(request.url);
            const url = new URL(request.url, `https://${request.headers.host}`);
            const path = url.pathname + url.search;
            const auth = __classPrivateFieldGet(this, _TestServer_auths, "f").get(path);
            if (auth) {
                const credentials = Buffer.from((request.headers.authorization || '').split(' ')[1] || '', 'base64').toString();
                if (credentials !== `${auth.username}:${auth.password}`) {
                    response.writeHead(401, {
                        'WWW-Authenticate': 'Basic realm="Secure Area"',
                    });
                    response.end('HTTP Error 401 Unauthorized: Access is denied');
                    return;
                }
            }
            const subscriber = __classPrivateFieldGet(this, _TestServer_requestSubscribers, "f").get(path);
            if (subscriber) {
                subscriber.resolve.call(undefined, request);
                __classPrivateFieldGet(this, _TestServer_requestSubscribers, "f").delete(path);
            }
            const handler = __classPrivateFieldGet(this, _TestServer_routes, "f").get(path);
            if (handler) {
                handler.call(undefined, request, response);
            }
            else {
                this.serveFile(request, response, path);
            }
        });
        _TestServer_onWebSocketConnection.set(this, (socket) => {
            socket.send('opened');
        });
        __classPrivateFieldSet(this, _TestServer_dirPath, dirPath, "f");
        if (sslOptions) {
            __classPrivateFieldSet(this, _TestServer_server, (0, https_1.createServer)(sslOptions, __classPrivateFieldGet(this, _TestServer_onRequest, "f")), "f");
        }
        else {
            __classPrivateFieldSet(this, _TestServer_server, (0, http_1.createServer)(__classPrivateFieldGet(this, _TestServer_onRequest, "f")), "f");
        }
        __classPrivateFieldGet(this, _TestServer_server, "f").on('connection', __classPrivateFieldGet(this, _TestServer_onServerConnection, "f"));
        __classPrivateFieldSet(this, _TestServer_wsServer, new ws_1.Server({ server: __classPrivateFieldGet(this, _TestServer_server, "f") }), "f");
        __classPrivateFieldGet(this, _TestServer_wsServer, "f").on('connection', __classPrivateFieldGet(this, _TestServer_onWebSocketConnection, "f"));
    }
    static async create(dirPath) {
        let res;
        const promise = new Promise(resolve => {
            res = resolve;
        });
        const server = new TestServer(dirPath);
        __classPrivateFieldGet(server, _TestServer_server, "f").once('listening', res);
        __classPrivateFieldGet(server, _TestServer_server, "f").listen(0);
        await promise;
        return server;
    }
    static async createHTTPS(dirPath) {
        let res;
        const promise = new Promise(resolve => {
            res = resolve;
        });
        const server = new TestServer(dirPath, {
            key: (0, fs_1.readFileSync)((0, path_1.join)(__dirname, '..', 'key.pem')),
            cert: (0, fs_1.readFileSync)((0, path_1.join)(__dirname, '..', 'cert.pem')),
            passphrase: 'aaaa',
        });
        __classPrivateFieldGet(server, _TestServer_server, "f").once('listening', res);
        __classPrivateFieldGet(server, _TestServer_server, "f").listen(0);
        await promise;
        return server;
    }
    get port() {
        return __classPrivateFieldGet(this, _TestServer_server, "f").address().port;
    }
    enableHTTPCache(pathPrefix) {
        __classPrivateFieldSet(this, _TestServer_cachedPathPrefix, pathPrefix, "f");
    }
    setAuth(path, username, password) {
        __classPrivateFieldGet(this, _TestServer_auths, "f").set(path, { username, password });
    }
    enableGzip(path) {
        __classPrivateFieldGet(this, _TestServer_gzipRoutes, "f").add(path);
    }
    setCSP(path, csp) {
        __classPrivateFieldGet(this, _TestServer_csp, "f").set(path, csp);
    }
    async stop() {
        this.reset();
        for (const socket of __classPrivateFieldGet(this, _TestServer_connections, "f")) {
            socket.destroy();
        }
        __classPrivateFieldGet(this, _TestServer_connections, "f").clear();
        await new Promise(x => {
            return __classPrivateFieldGet(this, _TestServer_server, "f").close(x);
        });
    }
    setRoute(path, handler) {
        __classPrivateFieldGet(this, _TestServer_routes, "f").set(path, handler);
    }
    setRedirect(from, to) {
        this.setRoute(from, (_, res) => {
            res.writeHead(302, { location: to });
            res.end();
        });
    }
    waitForRequest(path) {
        const subscriber = __classPrivateFieldGet(this, _TestServer_requestSubscribers, "f").get(path);
        if (subscriber) {
            return subscriber.promise;
        }
        let resolve;
        let reject;
        const promise = new Promise((res, rej) => {
            resolve = res;
            reject = rej;
        });
        __classPrivateFieldGet(this, _TestServer_requestSubscribers, "f").set(path, { resolve, reject, promise });
        return promise;
    }
    reset() {
        __classPrivateFieldGet(this, _TestServer_routes, "f").clear();
        __classPrivateFieldGet(this, _TestServer_auths, "f").clear();
        __classPrivateFieldGet(this, _TestServer_csp, "f").clear();
        __classPrivateFieldGet(this, _TestServer_gzipRoutes, "f").clear();
        const error = new Error('Static Server has been reset');
        for (const subscriber of __classPrivateFieldGet(this, _TestServer_requestSubscribers, "f").values()) {
            subscriber.reject.call(undefined, error);
        }
        __classPrivateFieldGet(this, _TestServer_requestSubscribers, "f").clear();
    }
    serveFile(request, response, pathName) {
        if (pathName === '/') {
            pathName = '/index.html';
        }
        const filePath = (0, path_1.join)(__classPrivateFieldGet(this, _TestServer_dirPath, "f"), pathName.substring(1));
        if (__classPrivateFieldGet(this, _TestServer_cachedPathPrefix, "f") && filePath.startsWith(__classPrivateFieldGet(this, _TestServer_cachedPathPrefix, "f"))) {
            if (request.headers['if-modified-since']) {
                response.statusCode = 304; // not modified
                response.end();
                return;
            }
            response.setHeader('Cache-Control', 'public, max-age=31536000');
            response.setHeader('Last-Modified', __classPrivateFieldGet(this, _TestServer_startTime, "f").toISOString());
        }
        else {
            response.setHeader('Cache-Control', 'no-cache, no-store');
        }
        const csp = __classPrivateFieldGet(this, _TestServer_csp, "f").get(pathName);
        if (csp) {
            response.setHeader('Content-Security-Policy', csp);
        }
        (0, fs_1.readFile)(filePath, (err, data) => {
            if (err) {
                response.statusCode = 404;
                response.end(`File not found: ${filePath}`);
                return;
            }
            const mimeType = (0, mime_1.getType)(filePath);
            if (mimeType) {
                const isTextEncoding = /^text\/|^application\/(javascript|json)/.test(mimeType);
                const contentType = isTextEncoding
                    ? `${mimeType}; charset=utf-8`
                    : mimeType;
                response.setHeader('Content-Type', contentType);
            }
            if (__classPrivateFieldGet(this, _TestServer_gzipRoutes, "f").has(pathName)) {
                response.setHeader('Content-Encoding', 'gzip');
                (0, zlib_1.gzip)(data, (_, result) => {
                    response.end(result);
                });
            }
            else {
                response.end(data);
            }
        });
    }
}
exports.TestServer = TestServer;
_TestServer_dirPath = new WeakMap(), _TestServer_server = new WeakMap(), _TestServer_wsServer = new WeakMap(), _TestServer_startTime = new WeakMap(), _TestServer_cachedPathPrefix = new WeakMap(), _TestServer_connections = new WeakMap(), _TestServer_routes = new WeakMap(), _TestServer_auths = new WeakMap(), _TestServer_csp = new WeakMap(), _TestServer_gzipRoutes = new WeakMap(), _TestServer_requestSubscribers = new WeakMap(), _TestServer_onServerConnection = new WeakMap(), _TestServer_onRequest = new WeakMap(), _TestServer_onWebSocketConnection = new WeakMap();
//# sourceMappingURL=index.js.map