/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests basics of loading SQLite extension.

const VALID_EXTENSION_NAME = "fts5";

add_setup(async function () {
  cleanup();
});

add_task(async function test_valid_call() {
  info("Testing valid call");
  let conn = getOpenedUnsharedDatabase();

  await new Promise((resolve, reject) => {
    conn.loadExtension(VALID_EXTENSION_NAME, status => {
      if (Components.isSuccessCode(status)) {
        resolve();
      } else {
        reject(status);
      }
    });
  });

  cleanup();
});

add_task(async function test_invalid_calls() {
  info("Testing invalid calls");
  let conn = getOpenedUnsharedDatabase();

  await Assert.rejects(
    new Promise((resolve, reject) => {
      conn.loadExtension("unknown", status => {
        if (Components.isSuccessCode(status)) {
          resolve();
        } else {
          reject(status);
        }
      });
    }),
    /NS_ERROR_ILLEGAL_VALUE/,
    "Should fail loading unknown extension"
  );

  cleanup();

  await Assert.rejects(
    new Promise((resolve, reject) => {
      conn.loadExtension(VALID_EXTENSION_NAME, status => {
        if (Components.isSuccessCode(status)) {
          resolve();
        } else {
          reject(status);
        }
      });
    }),
    /NS_ERROR_NOT_INITIALIZED/,
    "Should fail loading extension on a closed connection"
  );
});

add_task(async function test_more_invalid_calls() {
  let conn = getOpenedUnsharedDatabase();
  let promiseClosed = asyncClose(conn);

  await Assert.rejects(
    new Promise((resolve, reject) => {
      conn.loadExtension(VALID_EXTENSION_NAME, status => {
        if (Components.isSuccessCode(status)) {
          resolve();
        } else {
          reject(status);
        }
      });
    }),
    /NS_ERROR_NOT_INITIALIZED/,
    "Should fail loading extension on a closing connection"
  );

  await promiseClosed;
});
