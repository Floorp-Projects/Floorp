/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

module.exports = async function (context, commands) {
  context.log.info("Starting a indexedDB getkeyrange");

  const test_url = context.options.browsertime.url;
  let page_cycles = context.options.browsertime.page_cycles;
  let page_cycle_delay = context.options.browsertime.page_cycle_delay;
  let post_startup_delay = context.options.browsertime.post_startup_delay;

  context.log.info(
    "Waiting for %d ms (post_startup_delay)",
    post_startup_delay
  );
  await commands.wait.byTime(post_startup_delay);

  await commands.navigate(test_url);

  for (let count = 0; count < page_cycles; count++) {
    context.log.info("Cycle %d, waiting for %d ms", count, page_cycle_delay);
    await commands.wait.byTime(page_cycle_delay);

    context.log.info("Cycle %d, starting the measure", count);
    await await commands.measure.start();
    await commands.js.run(`
        const notifyDone = arguments[arguments.length - 1];
        async function resPromise() {
          return new Promise((resolve, reject) => {
            const results = {};
            const request = indexedDB.open('get-keyrange', 1);

            request.onsuccess = () => {
              const db = request.result;
              const start = Date.now();
              const keyRange = IDBKeyRange.bound(0, 99);
              const transaction = db.transaction('entries', 'readonly');
              const store = transaction.objectStore('entries');
              const index = store.index('index');
              const getAllRequest = index.getAll(keyRange);
              getAllRequest.onsuccess = () => {
                const items = getAllRequest.result;
                items.forEach((item) => {
                  results[item.key] = item;
                });
                const end = Date.now();
                db.close();
                resolve(end - start);
              };
              getAllRequest.onerror = () => {
                reject(getAllRequest.error);
              };
            };

          });
        }
        resPromise().then(() => {
          notifyDone();
        });
      `);
    await commands.measure.stop();
  }

  context.log.info("IndexedDB getkeyrange ended.");
  return true;
};
