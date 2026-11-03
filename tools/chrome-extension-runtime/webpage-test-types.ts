// SPDX-License-Identifier: MPL-2.0
// Public native objects observed by the ordinary HTTP messaging fixture.
export interface WebPagePort {
  onMessage: { addListener(callback: (value: unknown) => void): void };
  onDisconnect: { addListener(callback: () => void): void };
  error?: { message?: string };
  disconnect(): void;
  postMessage(value: object): void;
}

export interface WebPageRuntime {
  id?: string;
  lastError?: { message?: string };
  sendNativeMessage?: unknown;
  onMessage?: unknown;
  sendMessage(target: string, message: object): Promise<unknown>;
  sendMessage(
    target: string,
    message: object,
    callback: (value: unknown) => void,
  ): void;
  connect(target: string, options: { name: string }): WebPagePort;
}

export interface WebPageChrome {
  runtime?: WebPageRuntime;
  storage?: unknown;
}

export interface WebPageFixtureRequest {
  floorpWebPageFixture?: boolean;
  requestId: number;
  operation: string;
  target: string;
}

export interface WebPageFixtureWindow {
  document: { title: string } | null;
  chrome?: WebPageChrome;
  addEventListener(
    name: "message",
    listener: (event: { data: unknown }) => void,
  ): void;
  addEventListener(
    name: "pageshow",
    listener: (event: { persisted: boolean }) => void,
  ): void;
}
