/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_set_default_pdf_handler_no_data() {
  const sandbox = sinon.createSandbox();
  const stub = sandbox.stub();

  await SMATestUtils.executeAndValidateAction(
    { type: "SET_DEFAULT_PDF_HANDLER" },
    {
      ownerGlobal: {
        getShellService: () => ({
          setAsDefaultPDFHandler: stub,
        }),
      },
    }
  );

  Assert.equal(
    stub.callCount,
    1,
    "setAsDefaultPDFHandler was called by the action"
  );
  Assert.ok(
    stub.calledWithExactly(false),
    "setAsDefaultPDFHandler called with onlyIfKnownBrowser = false"
  );
});

add_task(async function test_set_default_pdf_handler_data_false() {
  const sandbox = sinon.createSandbox();
  const stub = sandbox.stub();

  await SMATestUtils.executeAndValidateAction(
    {
      type: "SET_DEFAULT_PDF_HANDLER",
      data: {
        onlyIfKnownBrowser: false,
      },
    },
    {
      ownerGlobal: {
        getShellService: () => ({
          setAsDefaultPDFHandler: stub,
        }),
      },
    }
  );

  Assert.equal(
    stub.callCount,
    1,
    "setAsDefaultPDFHandler was called by the action"
  );
  Assert.ok(
    stub.calledWithExactly(false),
    "setAsDefaultPDFHandler called with onlyIfKnownBrowser = false"
  );
});

add_task(async function test_set_default_pdf_handler_data_true() {
  const sandbox = sinon.createSandbox();
  const stub = sandbox.stub();

  await SMATestUtils.executeAndValidateAction(
    {
      type: "SET_DEFAULT_PDF_HANDLER",
      data: {
        onlyIfKnownBrowser: true,
      },
    },
    {
      ownerGlobal: {
        getShellService: () => ({
          setAsDefaultPDFHandler: stub,
        }),
      },
    }
  );

  Assert.equal(
    stub.callCount,
    1,
    "setAsDefaultPDFHandler was called by the action"
  );
  Assert.ok(
    stub.calledWithExactly(true),
    "setAsDefaultPDFHandler called with onlyIfKnownBrowser = true"
  );
});

add_task(async function test_decline_default_pdf_handler() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(
      "browser.shell.checkDefaultPDF.silencedByUser"
    );
  });

  await SMATestUtils.executeAndValidateAction({
    type: "DECLINE_DEFAULT_PDF_HANDLER",
  });

  Assert.equal(
    Services.prefs.getBoolPref("browser.shell.checkDefaultPDF.silencedByUser"),
    true,
    "DECLINE_DEFAULT_PDF_HANDLER ought to set pref properly."
  );
});
