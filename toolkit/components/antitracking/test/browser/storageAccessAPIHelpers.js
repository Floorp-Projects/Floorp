async function hasStorageAccessInitially() {
  let hasAccess = await document.hasStorageAccess();
  ok(hasAccess, "Has storage access");
}

async function noStorageAccessInitially() {
  let hasAccess = await document.hasStorageAccess();
  ok(!hasAccess, "Doesn't yet have storage access");
}

async function callRequestStorageAccess(callback) {
  let dwu = SpecialPowers.getDOMWindowUtils(window);
  let helper = dwu.setHandlingUserInput(true);

  let p;
  let threw = false;
  try {
    p = document.requestStorageAccess();
  } catch (e) {
    threw = true;
  } finally {
    helper.destruct();
  }
  let rejected = false;
  try {
    if (callback) {
      await p.then(_ => callback(dwu));
    } else {
      await p;
    }
  } catch (e) {
    rejected = true;
  }

  let success = !threw && !rejected;
  let hasAccess = await document.hasStorageAccess();
  is(hasAccess, success,
     "Should " + (success ? "" : "not ") + "have storage access now");

  return [threw, rejected];
}

