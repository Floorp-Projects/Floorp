// SPDX-License-Identifier: MPL-2.0
// Normal owned page listeners. No API replacement, cloning or Xray waiver.
(() => {
  const root = document.documentElement;
  const token = root.getAttribute("data-owned-contract-token");
  const events = [];
  let firstDetail;
  let firstShared;
  let firstMethod;
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
  document.addEventListener("floorp-owned-detail-contract", (event) => {
    if (events.length >= 6) return;
    const kind =
      ["data", "accessor", "functions"][Math.floor(events.length / 2)];
    const phase = events.length % 2 + 1;
    let detail;
    const detailRead = caught(() => {
      detail = event.detail;
      return { type: typeof detail, isNull: detail === null };
    });
    const alternateReceiver = { tag: "owned-alternate-page-receiver" };
    const result = {
      kind,
      phase,
      detailRead,
      detailType: typeof detail,
      detailIsNull: detail === null,
      repeatedDetailGetterIdentity: caught(() => detail === event.detail),
      repeatedEventDetailIdentity: phase === 1 ? null : detail === firstDetail,
      token: caught(() => detail.token),
      observedKind: caught(() => detail.kind),
      aliases: caught(() => ({
        aliasIsShared: detail.alias === detail.shared,
        arrayElementIsShared: detail.array[0] === detail.shared,
        selfIsDetail: detail.self === detail,
      })),
      livePriorReference: phase === 1 ? null : caught(() => ({
        priorSharedIsCurrentShared: firstShared === detail.shared,
        priorSharedCount: firstShared.count,
        priorMethodIsCurrentMethod: firstMethod === detail.method,
      })),
      beforePageWrite: caught(() => ({
        sharedCount: detail.shared.count,
        contentAddedTag: detail.contentAdded?.tag ?? null,
        deletedByContent: !Object.hasOwn(detail, "deleteByContent"),
      })),
      ownKeys: caught(() => Reflect.ownKeys(detail).map(String)),
      prototypes: caught(() => ({
        detailHasPageObjectPrototype:
          Object.getPrototypeOf(detail) === Object.prototype,
        detailConstructorIsPageObject: detail.constructor === Object,
        methodHasPageFunctionPrototype:
          Object.getPrototypeOf(detail.method) === Function.prototype,
        methodConstructorIsPageFunction: detail.method.constructor === Function,
      })),
      objectPrototype: caught(() => ({
        hasPageObjectPrototype:
          Object.getPrototypeOf(detail) === Object.prototype,
        constructorIsPageObject: detail.constructor === Object,
      })),
      accessor: {
        descriptor: caught(() => {
          const a = Object.getOwnPropertyDescriptor(detail, "accessor");
          const b = Object.getOwnPropertyDescriptor(detail, "accessor");
          return {
            configurable: a.configurable,
            enumerable: a.enumerable,
            isData: Object.hasOwn(a, "value"),
            writable: a.writable ?? null,
            valueIsShared: Object.hasOwn(a, "value")
              ? a.value === detail.shared
              : null,
            getterType: typeof a.get,
            setterType: typeof a.set,
            stableGetter: typeof a.get === "function" ? a.get === b.get : null,
            stableSetter: typeof a.set === "function" ? a.set === b.set : null,
          };
        }),
        read: caught(() => detail.accessor === detail.shared),
        explicitGetter: caught(() =>
          Reflect.apply(
            Object.getOwnPropertyDescriptor(detail, "accessor").get,
            detail,
            [],
          ) === detail.shared
        ),
        alternateGetter: caught(() =>
          Reflect.apply(
            Object.getOwnPropertyDescriptor(detail, "accessor").get,
            alternateReceiver,
            [],
          ) === detail.shared
        ),
      },
    };
    if (phase === 1) {
      firstDetail = detail;
      result.priorReferenceCapture = {
        shared: caught(() => {
          firstShared = detail.shared;
          return typeof firstShared;
        }),
        method: caught(() => {
          firstMethod = detail.method;
          return typeof firstMethod;
        }),
      };
    }
    const pageArgument = { tag: "page-added", phase };
    result.pageMutation = {
      shared: caught(() => {
        detail.shared.count = phase === 1 ? 7 : 13;
        return detail.shared.count;
      }),
      added: caught(() => {
        detail.pageAdded = pageArgument;
        return detail.pageAdded === pageArgument;
      }),
      propertyAssignment: caught(() => {
        detail.accessor = pageArgument;
        return true;
      }),
      alternateSetter: caught(() => {
        const setter = Object.getOwnPropertyDescriptor(detail, "accessor").set;
        Reflect.apply(setter, alternateReceiver, [pageArgument]);
        return true;
      }),
      deleted: caught(() => {
        delete detail.deleteByPage;
        return !Object.hasOwn(detail, "deleteByPage");
      }),
    };
    result.call = caught(() => {
      const value = Reflect.apply(detail.method, detail, [pageArgument]);
      return {
        returnIsShared: value === detail.shared,
        returnCount: value.count,
      };
    });
    result.thrownValue = caught(() => {
      try {
        Reflect.apply(detail.throwShared, detail, []);
        return { didThrow: false };
      } catch (value) {
        return {
          didThrow: true,
          thrownIsShared: value === detail.shared,
          thrownCount: value.count,
        };
      }
    });
    result.explicitReturnedGlobal = caught(() => {
      const value = Reflect.apply(detail.returnGlobal, detail, []);
      return {
        isPageGlobal: value === globalThis,
        markerType: typeof value.__floorpOwnedContractSecret,
        markerValue: value.__floorpOwnedContractSecret ?? null,
        chromeRuntimeType: typeof value.chrome?.runtime,
        browserRuntimeType: typeof value.browser?.runtime,
      };
    });
    if (phase === 1) {
      result.functionConstructor = caught(() => {
        // A constant, side-effect-free public constructor/CSP measurement.
        // It only reads this fixture's fixed synthetic marker and API shapes;
        // it never invokes an extension API or touches external state.
        const constructor = detail.method.constructor;
        const read = constructor(
          "return {markerType:typeof globalThis.__floorpOwnedContractSecret,markerValue:globalThis.__floorpOwnedContractSecret??null,chromeRuntimeType:typeof globalThis.chrome?.runtime,browserRuntimeType:typeof globalThis.browser?.runtime}",
        );
        const value = read();
        return {
          markerType: value.markerType,
          markerValue: value.markerValue,
          chromeRuntimeType: value.chromeRuntimeType,
          browserRuntimeType: value.browserRuntimeType,
        };
      });
    }
    events.push(result);
    root.setAttribute(
      "data-owned-page-contract",
      JSON.stringify({
        token,
        isTop: top === window,
        events,
        unexposedGlobals: {
          globalSecretType: typeof globalThis.__floorpOwnedContractSecret,
          windowSecretType: typeof window.__floorpOwnedContractWindowSecret,
          chromeRuntimeType: typeof window.chrome?.runtime,
          browserRuntimeType: typeof window.browser?.runtime,
        },
      }),
    );
  });
})();
