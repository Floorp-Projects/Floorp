// SPDX-License-Identifier: MPL-2.0
// The owned page publishes DOM observations through its normal title. This does
// not alter any browser API, content-script global, descriptor or vendor code.
(() => {
  if (top !== window) return;
  const extensionExpected =
    new URL(location.href).searchParams.get("extension") === "1";
  const deadline = Date.now() + 10000;
  const read = (doc, name) => {
    const text = doc?.documentElement?.getAttribute(name);
    return text ? JSON.parse(text) : null;
  };
  const timer = setInterval(() => {
    const child = document.querySelector("iframe")?.contentDocument;
    const observation = {
      topPage: read(document, "data-page-global-observation"),
      framePage: read(child, "data-page-global-observation"),
      topIsolated: read(document, "data-isolated-global-observation"),
      frameIsolated: read(child, "data-isolated-global-observation"),
      topContentEvent: read(document, "data-public-custom-event-observation"),
      frameContentEvent: read(child, "data-public-custom-event-observation"),
      privateMarkersVisibleToPage: {
        topGlobal: typeof Reflect.get(
          window,
          "__floorpOwnedIsolatedGlobalMarker",
        ),
        topWindow: typeof Reflect.get(
          window,
          "__floorpOwnedIsolatedWindowMarker",
        ),
        frameGlobal: child?.defaultView
          ? typeof Reflect.get(
            child.defaultView,
            "__floorpOwnedIsolatedGlobalMarker",
          )
          : null,
        frameWindow: child?.defaultView
          ? typeof Reflect.get(
            child.defaultView,
            "__floorpOwnedIsolatedWindowMarker",
          )
          : null,
      },
    };
    const complete = observation.topPage && observation.framePage &&
      (!extensionExpected ||
        (observation.topIsolated && observation.frameIsolated));
    if (complete || Date.now() > deadline) {
      clearInterval(timer);
      const payload = document.createElement("pre");
      payload.id = "world-observation-json";
      payload.textContent = JSON.stringify({
        complete: Boolean(complete),
        extensionExpected,
        observation,
      });
      document.body.appendChild(payload);
      document.title = complete
        ? "world-observation-complete"
        : "world-observation-incomplete";
    }
  }, 25);
  addEventListener("pagehide", () => clearInterval(timer), { once: true });
})();
