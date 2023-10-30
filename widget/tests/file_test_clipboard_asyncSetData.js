/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from clipboard_helper.js */

"use strict";

clipboardTypes.forEach(function (type) {
  if (clipboard.isClipboardTypeSupported(type)) {
    clipboardTypes.forEach(function (otherType) {
      if (clipboard.isClipboardTypeSupported(otherType)) {
        [true, false].forEach(async function (async) {
          add_task(async function test_clipboard_pending_asyncSetData() {
            info(
              `Test having a pending asyncSetData request on ${type} and then make a new ${
                async ? "asyncSetData" : "setData"
              } request on ${otherType}`
            );

            // Create a pending asyncSetData request
            let priorResult;
            let priorRequest;
            let priorPromise = new Promise(resolve => {
              priorRequest = clipboard.asyncSetData(type, {
                QueryInterface: SpecialPowers.ChromeUtils.generateQI([
                  "nsIAsyncSetClipboardDataCallback",
                ]),
                onComplete(rv) {
                  priorResult = rv;
                  resolve();
                },
              });
            });

            // Create a new request
            let str = writeRandomStringToClipboard(
              "text/plain",
              otherType,
              null,
              async
            );

            if (type === otherType) {
              info(
                "The new request to the same clipboard type should cancel the prior pending request"
              );
              await priorPromise;

              is(
                priorResult,
                Cr.NS_ERROR_ABORT,
                "The pending asyncSetData request should be canceled"
              );
              try {
                priorRequest.setData(
                  generateNewTransferable("text/plain", generateRandomString())
                );
                ok(
                  false,
                  "An error should be thrown if setData is called on a canceled clipboard request"
                );
              } catch (e) {
                is(
                  e.result,
                  Cr.NS_ERROR_FAILURE,
                  "An error should be thrown if setData is called on a canceled clipboard request"
                );
              }
            } else {
              info(
                "The new request to the different clipboard type should not cancel the prior pending request"
              );
              str = generateRandomString();
              priorRequest.setData(
                generateNewTransferable("text/plain", str),
                null
              );
              await priorPromise;

              is(
                priorResult,
                Cr.NS_OK,
                "The pending asyncSetData request should success"
              );

              try {
                priorRequest.setData(
                  generateNewTransferable("text/plain", generateRandomString())
                );
                ok(
                  false,
                  "Calling setData multiple times should throw an error"
                );
              } catch (e) {
                is(
                  e.result,
                  Cr.NS_ERROR_FAILURE,
                  "Calling setData multiple times should throw an error"
                );
              }
            }

            // Test clipboard data.
            is(
              getClipboardData("text/plain", type),
              str,
              `Test clipboard data for type ${type}`
            );

            // Clean clipboard data.
            cleanupAllClipboard();
          });
        });
      }
    });

    add_task(async function test_clipboard_asyncSetData_abort() {
      info(`Test abort asyncSetData request on ${type}`);

      // Create a pending asyncSetData request
      let result;
      let request = clipboard.asyncSetData(type, rv => {
        result = rv;
      });

      // Abort with NS_OK.
      try {
        request.abort(Cr.NS_OK);
        ok(false, "Throw an error when attempting to abort with NS_OK");
      } catch (e) {
        is(
          e.result,
          Cr.NS_ERROR_FAILURE,
          "Should throw an error when attempting to abort with NS_OK"
        );
      }
      is(result, undefined, "The asyncSetData request should not be canceled");

      // Abort with NS_ERROR_ABORT.
      request.abort(Cr.NS_ERROR_ABORT);
      is(
        result,
        Cr.NS_ERROR_ABORT,
        "The asyncSetData request should be canceled"
      );
      try {
        request.abort(Cr.NS_ERROR_FAILURE);
        ok(false, "Throw an error when attempting to abort again");
      } catch (e) {
        is(
          e.result,
          Cr.NS_ERROR_FAILURE,
          "Should throw an error when attempting to abort again"
        );
      }
      is(
        result,
        Cr.NS_ERROR_ABORT,
        "The callback should not be notified again"
      );

      try {
        request.setData(
          generateNewTransferable("text/plain", generateRandomString())
        );
        ok(
          false,
          "An error should be thrown if setData is called on a canceled clipboard request"
        );
      } catch (e) {
        is(
          e.result,
          Cr.NS_ERROR_FAILURE,
          "An error should be thrown if setData is called on a canceled clipboard request"
        );
      }
    });
  }
});
