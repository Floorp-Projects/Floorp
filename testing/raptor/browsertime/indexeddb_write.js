/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

module.exports = async function (context, commands) {
  context.log.info("Starting a indexedDB write");
  const post_startup_delay = context.options.browsertime.post_startup_delay;
  console.log("context options", context.options);

  const test_url = context.options.browsertime.url;
  const chunk_size = context.options.browsertime.chunk_size;
  const iterations = context.options.browsertime.iterations;
  const buffer_type = context.options.browsertime.buffer_type;
  const atomic_value = context.options.browsertime.atomic;
  if (atomic_value * atomic_value != atomic_value) {
    throw Error("Value of atomic shall be 0 for falsehood, 1 for truth.");
  }
  const atomic = 0 != atomic_value;

  const accepted_buffers = ["Array", "ArrayBuffer", "Blob"];
  if (!accepted_buffers.includes(buffer_type)) {
    throw Error("Buffer type " + buffer_type + " is unknown.");
  }

  context.log.info("IndexedDB write URL = " + test_url);
  context.log.info("IndexedDB write chunk size = " + chunk_size);
  context.log.info("IndexedDB write iterations = " + iterations);
  context.log.info(
    "IndexedDB writes " +
      (atomic ? "all in one big transaction" : "in separate transactions")
  );
  context.log.info("IndexedDB write data format " + buffer_type);

  context.log.info(
    "Waiting for %d ms (post_startup_delay)",
    post_startup_delay
  );

  await commands.navigate(test_url);

  const seleniumDriver = context.selenium.driver;

  await commands.wait.byTime(post_startup_delay);

  await commands.measure.start();
  const time_duration = await seleniumDriver.executeAsyncScript(`
        const notifyDone = arguments[arguments.length - 1];

        const iterations = ${iterations};
        const sizeInBytes = ${chunk_size};
        const bufferType = "${buffer_type}";
        const atomic = ${atomic};

        const makeData = (() => {
          if (bufferType === "ArrayBuffer") {
            return () => {
              const valueBuffer = new ArrayBuffer(sizeInBytes);

              const charCodeView = new Uint16Array(valueBuffer);
              const sizeInUint16 = sizeInBytes / 2;
              for (let i=0; i < sizeInUint16; ++i) {
                charCodeView[i] = "qwerty".charCodeAt(i % 6);
              }

              return valueBuffer;
            };
          }

          if (bufferType === "Array") {
            return () => {
              return Array.from({length: sizeInBytes}, (_, i) => "qwerty"[i % 6]);
            };
          }

          if (bufferType !== "Blob") {
            throw Error("Unknown buffer type " + bufferType);
          }

          return () => {
            return new Blob([Array.from({length: sizeInBytes}, (_, i) => "qwerty"[i % 6])]);
          };
        })();

        function addData(txSource, txProvider, i) {
          try {
            const keyName = "doc_" + i;
            const valueData = makeData();
            const record = { key: keyName, property: valueData };

            const rq = txProvider(txSource).add(record);

            return new Promise((res_ad, rej_ad) => {
              rq.onsuccess = () => { res_ad(); };
              rq.onerror = e => { rej_ad(e); };
            });
          } catch (e) {
            return new Promise((_, rej_ad) => rej_ad(e));
          }
        }

        function waitForData(txSource) {
          try {
            if (!atomic) {
              const txProvider = src => src.transaction("store", "readwrite").objectStore("store");
              return Promise.all(
                Array.from({ length: iterations }, (_, i) => {
                  return addData(txSource, txProvider, i);
                }));
            }

            const currentTx = txSource.transaction("store", "readwrite").objectStore("store");
            return Promise.all(
              Array.from({ length: iterations }, (_, i) => {
                return addData(currentTx, src => src, i);
              }));
          } catch (e) {
            return new Promise((_, rej_tx) => rej_tx(e));
          }
        }

        function upgradePromise() {
          try {
            const open_db = indexedDB.open("rootsdb");
            return new Promise((res_upgrade, rej_upgrade) => {
              open_db.onupgradeneeded = e => {
                e.target.result.createObjectStore("store", { keyPath: "key" });
              };
              open_db.onsuccess = e => { res_upgrade(e.target.result); };
              open_db.onerror = e => { rej_upgrade(e); };
            });
          } catch (e) {
            return new Promise((_, rej_upgrade) => rej_upgrade(e));
          }
        }

        const startTime = performance.now();
        upgradePromise().then(waitForData).then(() => {
          notifyDone(performance.now() - startTime);
        });
      `);
  await commands.measure.stop();

  console.log("Time duration was ", time_duration);

  await commands.measure.addObject({
    custom_data: { time_duration },
  });

  context.log.info("IndexedDB write ended.");
  return true;
};
