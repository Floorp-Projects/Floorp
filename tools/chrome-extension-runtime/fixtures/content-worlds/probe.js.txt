// SPDX-License-Identifier: MPL-2.0
// Authored observation only. No page/API replacement or Xray waiver.
(() => {
  const names = [
    "wrappedJSObject",
    "window",
    "globalThis",
    "self",
    "top",
    "parent",
    "unsafeWindow",
    "cloneInto",
    "exportFunction",
    "browser",
    "chrome",
    "__floorpOwnedPageMarker",
  ];
  const limit = (value) => String(value).slice(0, 300);
  const caught = (fn) => {
    try {
      return { value: fn() };
    } catch (error) {
      return {
        error: { name: limit(error?.name), message: limit(error?.message) },
      };
    }
  };
  const descriptor = (object, name) => {
    const d = Object.getOwnPropertyDescriptor(object, name);
    return d
      ? {
        configurable: d.configurable,
        enumerable: d.enumerable,
        accessor: "get" in d || "set" in d,
        hasGetter: typeof d.get === "function",
        hasSetter: typeof d.set === "function",
        writable: "writable" in d ? d.writable : null,
        valueType: "value" in d ? typeof d.value : null,
      }
      : null;
  };
  const reference = (object) => ({
    sameWindow: object === window,
    sameGlobalThis: object === globalThis,
    properties: Object.fromEntries(names.map((name) => [name, {
      has: caught(() => name in object),
      own: caught(() => Object.hasOwn(object, name)),
      descriptor: caught(() => descriptor(object, name)),
      // This explicitly observes public/native property getters. Their results
      // are reduced to scalars; no referent is retained in the report.
      read: caught(() => {
        const value = Reflect.get(object, name);
        return {
          type: typeof value,
          sameWindow: value === window,
          sameGlobalThis: value === globalThis,
          primitive: value === null ||
              ["string", "number", "boolean"].includes(typeof value)
            ? value
            : null,
        };
      }),
    }])),
    ownKeys: caught(() => {
      const keys = Reflect.ownKeys(object);
      return {
        count: keys.length,
        selected: names.filter((name) => keys.includes(name)),
      };
    }),
  });
  const extensionId = caught(() =>
    Reflect.get(globalThis, "chrome")?.runtime?.id
  );
  const isolated = typeof extensionId.value === "string";
  const privateIsolation = isolated
    ? {
      globalWrite: Reflect.set(
        globalThis,
        "__floorpOwnedIsolatedGlobalMarker",
        71,
      ),
      windowWrite: Reflect.set(window, "__floorpOwnedIsolatedWindowMarker", 72),
      pageOwnMarkerType: typeof Reflect.get(window, "__floorpOwnedPageMarker"),
    }
    : null;
  const result = {
    version: 1,
    href: location.href,
    extensionId: extensionId.value ?? null,
    isTop: top === window,
    globalThisEqualsWindow: globalThis === window,
    references: {
      globalThis: reference(globalThis),
      window: reference(window),
      top: caught(() => reference(top)),
      parent: caught(() => reference(parent)),
      iframe: caught(() => {
        const frame = document.querySelector("iframe");
        return frame?.contentWindow ? reference(frame.contentWindow) : null;
      }),
    },
    publicDomPageMarker: document.documentElement.getAttribute(
      "data-public-page-marker",
    ),
    ownDocumentUrl: document.documentURI,
    privateIsolation,
  };
  document.documentElement.setAttribute(
    isolated
      ? "data-isolated-global-observation"
      : "data-page-global-observation",
    JSON.stringify(result),
  );
  if (isolated) {
    // Genuine content-script constructor/options/data realm, with no actor or
    // privileged cloning. The page listener records native accessibility.
    document.dispatchEvent(
      new CustomEvent("floorp-owned-world-detail", {
        detail: { prefs: { marker: "authored-content-value" } },
      }),
    );
  }
})();
