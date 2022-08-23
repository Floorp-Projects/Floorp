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
/// <reference types="node" />
/// <reference types="node" />
import { IncomingMessage, ServerResponse } from 'http';
import { ServerOptions as HttpsServerOptions } from 'https';
declare type TestIncomingMessage = IncomingMessage & {
    postBody?: Promise<string>;
};
export declare class TestServer {
    #private;
    PORT: number;
    PREFIX: string;
    CROSS_PROCESS_PREFIX: string;
    EMPTY_PAGE: string;
    static create(dirPath: string): Promise<TestServer>;
    static createHTTPS(dirPath: string): Promise<TestServer>;
    constructor(dirPath: string, sslOptions?: HttpsServerOptions);
    get port(): number;
    enableHTTPCache(pathPrefix: string): void;
    setAuth(path: string, username: string, password: string): void;
    enableGzip(path: string): void;
    setCSP(path: string, csp: string): void;
    stop(): Promise<void>;
    setRoute(path: string, handler: (req: IncomingMessage, res: ServerResponse) => void): void;
    setRedirect(from: string, to: string): void;
    waitForRequest(path: string): Promise<TestIncomingMessage>;
    reset(): void;
    serveFile(request: IncomingMessage, response: ServerResponse, pathName: string): void;
}
export {};
//# sourceMappingURL=index.d.ts.map