// Number of bits in the canvas that we randomize when canvas randomization is
// enabled.
const NUM_RANDOMIZED_CANVAS_BITS = 256;

/**
 * Compares two Uint8Arrays and returns the number of bits that are different.
 *
 * @param {Uint8ClampedArray} arr1 - The first Uint8ClampedArray to compare.
 * @param {Uint8ClampedArray} arr2 - The second Uint8ClampedArray to compare.
 * @returns {number} - The number of bits that are different between the two
 *                     arrays.
 */
function countDifferencesInUint8Arrays(arr1, arr2) {
  let count = 0;
  for (let i = 0; i < arr1.length; i++) {
    let diff = arr1[i] ^ arr2[i];
    while (diff > 0) {
      count += diff & 1;
      diff >>= 1;
    }
  }
  return count;
}

/**
 * Compares two ArrayBuffer objects to see if they are different.
 *
 * @param {ArrayBuffer} buffer1 - The first buffer to compare.
 * @param {ArrayBuffer} buffer2 - The second buffer to compare.
 * @returns {boolean} True if the buffers are different, false if they are the
 *                    same.
 */
function countDifferencesInArrayBuffers(buffer1, buffer2) {
  // compare the byte lengths of the two buffers
  if (buffer1.byteLength !== buffer2.byteLength) {
    return true;
  }

  // create typed arrays to represent the two buffers
  const view1 = new DataView(buffer1);
  const view2 = new DataView(buffer2);

  let differences = 0;
  // compare the byte values of the two typed arrays
  for (let i = 0; i < buffer1.byteLength; i++) {
    if (view1.getUint8(i) !== view2.getUint8(i)) {
      differences += 1;
    }
  }

  // the two buffers are the same
  return differences;
}

function promiseObserver(topic) {
  return new Promise(resolve => {
    let obs = (aSubject, aTopic, aData) => {
      Services.obs.removeObserver(obs, aTopic);
      resolve(aSubject);
    };
    Services.obs.addObserver(obs, topic);
  });
}

/**
 *
 * Spawns a worker in the given browser and sends a callback function to it.
 * The result of the callback function is returned as a Promise.
 *
 * @param {object} browser - The browser context to spawn the worker in.
 * @param {Function} fn - The callback function to send to the worker.
 * @returns {Promise} A Promise that resolves to the result of the callback function.
 */
function runFunctionInWorker(browser, fn) {
  return SpecialPowers.spawn(browser, [fn.toString()], async callback => {
    // Create a worker.
    let worker = new content.Worker("worker.js");

    // Send the callback to the worker.
    return new content.Promise(resolve => {
      worker.onmessage = e => {
        resolve(e.data.result);
      };

      worker.postMessage({
        callback,
      });
    });
  });
}

const TEST_FIRST_PARTY_CONTEXT_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.net"
  ) + "testPage.html";
const TEST_THIRD_PARTY_CONTEXT_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "scriptExecPage.html";

/**
 * Executes provided callbacks in both first and third party contexts.
 *
 * @param {string} name - Name of the test.
 * @param {Function} firstPartyCallback - The callback to be executed in the first-party context.
 * @param {Function} thirdPartyCallback - The callback to be executed in the third-party context.
 * @param {Function} [cleanupFunction] - A cleanup function to be called after the tests.
 * @param {Array} [extraPrefs] - Optional. An array of preferences to be set before running the test.
 */
function runTestInFirstAndThirdPartyContexts(
  name,
  firstPartyCallback,
  thirdPartyCallback,
  cleanupFunction,
  extraPrefs
) {
  add_task(async _ => {
    info("Starting test `" + name + "' in first and third party contexts");

    await SpecialPowers.flushPrefEnv();

    if (extraPrefs && Array.isArray(extraPrefs) && extraPrefs.length) {
      await SpecialPowers.pushPrefEnv({ set: extraPrefs });
    }

    info("Creating a new tab");
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_FIRST_PARTY_CONTEXT_PAGE
    );

    info("Creating a 3rd party content and a 1st party popup");
    let firstPartyPopupPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      TEST_THIRD_PARTY_CONTEXT_PAGE,
      true
    );

    let thirdPartyBC = await SpecialPowers.spawn(
      tab.linkedBrowser,
      [TEST_THIRD_PARTY_CONTEXT_PAGE],
      async url => {
        let ifr = content.document.createElement("iframe");

        await new content.Promise(resolve => {
          ifr.onload = resolve;

          content.document.body.appendChild(ifr);
          ifr.src = url;
        });

        content.open(url);

        return ifr.browsingContext;
      }
    );
    let firstPartyTab = await firstPartyPopupPromise;
    let firstPartyBrowser = firstPartyTab.linkedBrowser;

    info("Sending code to the 3rd party content and the 1st party popup");
    let runningCallbackTask = async obj => {
      return new content.Promise(resolve => {
        content.postMessage({ callback: obj.callback }, "*");

        content.addEventListener("message", function msg(event) {
          // This is the event from above postMessage.
          if (event.data.callback) {
            return;
          }

          if (event.data.type == "finish") {
            content.removeEventListener("message", msg);
            resolve();
            return;
          }

          if (event.data.type == "ok") {
            ok(event.data.what, event.data.msg);
            return;
          }

          if (event.data.type == "info") {
            info(event.data.msg);
            return;
          }

          ok(false, "Unknown message");
        });
      });
    };

    let firstPartyTask = SpecialPowers.spawn(
      firstPartyBrowser,
      [
        {
          callback: firstPartyCallback.toString(),
        },
      ],
      runningCallbackTask
    );

    let thirdPartyTask = SpecialPowers.spawn(
      thirdPartyBC,
      [
        {
          callback: thirdPartyCallback.toString(),
        },
      ],
      runningCallbackTask
    );

    await Promise.all([firstPartyTask, thirdPartyTask]);

    info("Removing the tab");
    BrowserTestUtils.removeTab(tab);
    BrowserTestUtils.removeTab(firstPartyTab);
  });

  add_task(async _ => {
    info("Cleaning up.");
    if (cleanupFunction) {
      await cleanupFunction();
    }
  });
}
