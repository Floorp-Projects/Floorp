"use strict";

/* exported assert_background_serviceworker_pref_enabled,
 *          getBackgroundServiceWorkerRegistration,
 *          getServiceWorkerInfo, getServiceWorkerState,
 *          waitForServiceWorkerRegistrationsRemoved, waitForServiceWorkerTerminated
 */

async function assert_background_serviceworker_pref_enabled() {
  is(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    true,
    "Expect extensions.backgroundServiceWorker.enabled to be true"
  );
}

// Return the name of the enum corresponding to the worker's state (ex: "STATE_ACTIVATED")
// because nsIServiceWorkerInfo doesn't currently provide a comparable string-returning getter.
function getServiceWorkerState(workerInfo) {
  const map = Object.keys(workerInfo)
    .filter(k => k.startsWith("STATE_"))
    .reduce((map, name) => {
      map.set(workerInfo[name], name);
      return map;
    }, new Map());
  return map.has(workerInfo.state)
    ? map.get(workerInfo.state)
    : "state: ${workerInfo.state}";
}

function getServiceWorkerInfo(swRegInfo) {
  const {
    evaluatingWorker,
    installingWorker,
    waitingWorker,
    activeWorker,
  } = swRegInfo;
  return evaluatingWorker || installingWorker || waitingWorker || activeWorker;
}

async function waitForServiceWorkerTerminated(swRegInfo) {
  info(`Wait all ${swRegInfo.scope} workers to be terminated`);

  try {
    await BrowserTestUtils.waitForCondition(
      () => !getServiceWorkerInfo(swRegInfo)
    );
  } catch (err) {
    const workerInfo = getServiceWorkerInfo(swRegInfo);
    if (workerInfo) {
      ok(
        false,
        `Error while waiting for workers for scope ${swRegInfo.scope} to be terminated. ` +
          `Found a worker in state: ${getServiceWorkerState(workerInfo)}`
      );
      return;
    }

    throw err;
  }
}

function getBackgroundServiceWorkerRegistration(extension) {
  const policy = WebExtensionPolicy.getByHostname(extension.uuid);
  const expectedSWScope = policy.getURL("/");
  const expectedScriptURL = policy.extension.backgroundWorkerScript || "";

  ok(
    expectedScriptURL.startsWith(expectedSWScope),
    `Extension does include a valid background.service_worker: ${expectedScriptURL}`
  );

  const swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
    Ci.nsIServiceWorkerManager
  );

  let swReg;
  let regs = swm.getAllRegistrations();

  for (let i = 0; i < regs.length; i++) {
    let reg = regs.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
    if (reg.scriptSpec === expectedScriptURL) {
      swReg = reg;
      break;
    }
  }

  ok(swReg, `Found service worker registration for ${expectedScriptURL}`);

  is(
    swReg.scope,
    expectedSWScope,
    "The extension background worker registration has the expected scope URL"
  );

  return swReg;
}

async function waitForServiceWorkerRegistrationsRemoved(extension) {
  info(`Wait ${extension.id} service worker registration to be deleted`);
  const swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
    Ci.nsIServiceWorkerManager
  );
  let baseURI = Services.io.newURI(`moz-extension://${extension.uuid}/`);
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    baseURI,
    {}
  );

  await BrowserTestUtils.waitForCondition(() => {
    let regs = swm.getAllRegistrations();

    for (let i = 0; i < regs.length; i++) {
      let reg = regs.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
      if (principal.equals(reg.principal)) {
        return false;
      }
    }

    info(`All ${extension.id} service worker registrations are gone`);
    return true;
  }, `All ${extension.id} service worker registrations should be deleted`);
}
