// SPDX-License-Identifier: MPL-2.0

export interface Deferred<T> {
  promise: Promise<T>;
  resolve(value: T): void;
  reject(reason?: unknown): void;
}
