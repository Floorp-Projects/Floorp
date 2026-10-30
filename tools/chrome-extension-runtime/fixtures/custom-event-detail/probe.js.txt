// SPDX-License-Identifier: MPL-2.0
// The exact same authored content program is loaded by Chrome and Floorp.
(() => {
  const id = globalThis.chrome?.runtime?.id;
  if (typeof id !== "string") return;
  const root = document.documentElement;
  const token = root.getAttribute("data-owned-contract-token");
  const caught = (read) => {
    try {
      return { ok: true, value: read() };
    } catch (error) {
      return {
        ok: false,
        error: {
          name: String(error?.name).slice(0, 200),
          message: String(error?.message).slice(0, 500),
        },
      };
    }
  };
  globalThis.__floorpOwnedContractSecret = 71;
  window.__floorpOwnedContractWindowSecret = 72;
  const payloads = [];
  for (const kind of ["data", "accessor", "functions"]) {
    const shared = { count: 1 };
    let getterCalls = 0;
    let setterCalls = 0;
    let throwCalls = 0;
    let setterArgument;
    const getterReceivers = [];
    const setterReceivers = [];
    const calls = [];
    const detail = {
      token,
      kind,
      shared,
      alias: shared,
      array: [shared],
      self: null,
      deleteByPage: "present",
      deleteByContent: "present",
      method(argument) {
        calls.push({
          thisIsDetail: this === detail,
          argumentIsPageAdded: argument === detail.pageAdded,
          argumentTag: caught(() => argument.tag),
        });
        return shared;
      },
      returnGlobal() {
        // Intentional, explicit export. The page records only owned marker/API
        // shapes, never an API call or an object in its serialized report.
        return globalThis;
      },
      throwShared() {
        throwCalls++;
        throw shared;
      },
      get accessor() {
        getterCalls++;
        getterReceivers.push({
          isDetail: this === detail,
          tag: caught(() => this?.tag ?? null),
        });
        return shared;
      },
      set accessor(argument) {
        setterCalls++;
        setterArgument = argument;
        setterReceivers.push({
          isDetail: this === detail,
          tag: caught(() => this?.tag ?? null),
        });
      },
    };
    detail.self = detail;
    if (kind !== "functions") {
      delete detail.method;
      delete detail.returnGlobal;
      delete detail.throwShared;
    }
    if (kind === "data") delete detail.accessor;
    const sourceEvents = [];
    const ownListener = (event) => {
      if (sourceEvents.length >= 2) return;
      const value = event.detail;
      sourceEvents.push({
        sameOriginalDetail: value === detail,
        repeatedGetterIdentity: value === event.detail,
        originalAlias: caught(() =>
          value.shared === shared && value.alias === shared
        ),
        originalCycle: caught(() => value.self === detail),
      });
    };
    document.addEventListener("floorp-owned-detail-contract", ownListener);
    const snapshots = [];
    try {
      for (const phase of [1, 2]) {
        if (phase === 2) {
          shared.count = 11;
          detail.contentAdded = { tag: "content-added" };
          delete detail.deleteByContent;
        }
        document.dispatchEvent(
          new CustomEvent("floorp-owned-detail-contract", {
            detail,
          }),
        );
        snapshots.push({
          phase,
          sharedCount: shared.count,
          pageAddedTag: caught(() => detail.pageAdded?.tag ?? null),
          deletedByPage: !Object.hasOwn(detail, "deleteByPage"),
          setterArgumentMatchesPageAdded: setterCalls === 0
            ? null
            : setterArgument === detail.pageAdded,
          setterArgumentTag: caught(() => setterArgument?.tag ?? null),
          getterCalls,
          setterCalls,
          throwCalls,
          getterReceivers: getterReceivers.map((value) => ({ ...value })),
          setterReceivers: setterReceivers.map((value) => ({ ...value })),
          calls: calls.map((value) => ({ ...value })),
        });
      }
    } finally {
      document.removeEventListener("floorp-owned-detail-contract", ownListener);
    }
    payloads.push({ kind, sourceEvents, snapshots });
  }
  root.setAttribute(
    "data-owned-content-contract",
    JSON.stringify({
      token,
      extensionId: id,
      isTop: top === window,
      windowEqualsGlobalThis: window === globalThis,
      pageGlobalMarkerType: typeof window.__floorpOwnedContractPageMarker,
      payloads,
    }),
  );
})();
