"use strict";
// Globals

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
ChromeUtils.defineESModuleGetters(this, {
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
  ExperimentTestUtils: "resource://testing-common/NimbusTestUtils.sys.mjs",
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
});

// Sinon does not support Set or Map in spy.calledWith()
function onFinalizeCalled(spyOrCallArgs, ...expectedArgs) {
  function mapToObject(map) {
    return Object.assign(
      {},
      ...Array.from(map.entries()).map(([k, v]) => ({ [k]: v }))
    );
  }

  function toPlainObjects(args) {
    return [
      args[0],
      {
        ...args[1],
        invalidBranches: mapToObject(args[1].invalidBranches),
        invalidFeatures: mapToObject(args[1].invalidFeatures),
        missingLocale: Array.from(args[1].missingLocale),
        missingL10nIds: mapToObject(args[1].missingL10nIds),
      },
    ];
  }

  const plainExpected = toPlainObjects(expectedArgs);

  if (Array.isArray(spyOrCallArgs)) {
    return ObjectUtils.deepEqual(toPlainObjects(spyOrCallArgs), plainExpected);
  }

  for (const args of spyOrCallArgs.args) {
    if (ObjectUtils.deepEqual(toPlainObjects(args), plainExpected)) {
      return true;
    }
  }

  return false;
}

/**
 * Assert the store has no active experiments or rollouts.
 */
async function assertEmptyStore(store, { cleanup = false } = {}) {
  Assert.deepEqual(
    store
      .getAll()
      .filter(e => e.active)
      .map(e => e.slug),
    [],
    "Store should have no active enrollments"
  );

  Assert.deepEqual(
    store
      .getAll()
      .filter(e => e.inactive)
      .map(e => e.slug),
    [],
    "Store should have no inactive enrollments"
  );

  if (cleanup) {
    // We need to call finalize first to ensure that any pending saves from
    // JSONFile.saveSoon overwrite files on disk.
    store._store.saveSoon();
    await store._store.finalize();
    await IOUtils.remove(store._store.path);
  }
}
