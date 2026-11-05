import { N as NoraComponentBase, n as noraComponent } from './base.js';
import { e as createRootHMR, a as _render, d as createElement, s as setProp, i as insert } from '../../core.js';
import { d as createEffect, l as onCleanup, u as untrack, i as createSignal } from './external/solid-js.js';
import { p as panelSidebarConfig } from './data.js';
import './external/es-toolkit.js';
import './external/@jsr.js';
import './external/io-ts.js';
import './external/fp-ts.js';
import './external/i18next.js';

const hoverStyle = "/* SPDX-License-Identifier: MPL-2.0 */\n\n/* Leave the collapsed, relative launcher in normal flex flow. Only the\n * absolute hover launcher needs the space occupied by Floorp's panels. */\n:root[sidebar-expand-on-hover] #sidebar-container:is([sidebar-launcher-expanded], [sidebar-ongoing-animations]) {\n  &:not([sidebar-positionend]) {\n    inset-inline-start: var(--floorp-panel-start-width, 0px);\n  }\n  &[sidebar-positionend] {\n    inset-inline-end: var(--floorp-panel-end-width, 0px);\n  }\n}\n";

(function(gFlexOrder2) {
  const floorpSidebarId = "panel-sidebar-box";
  const floorpSidebarSplitterId = "panel-sidebar-splitter";
  const floorpSidebarSelectBoxId = "panel-sidebar-select-box";
  let hoverOffsetFrame;
  const [orders, setOrders] = createRootHMR(() => createSignal({
    floorpSidebarSplitter: -1,
    floorpSidebar: -1,
    floorpSidebarSelectBox: -1
  }), void 0);
  function init() {
    renderOrderStyle();
    observePanelWidths();
    createEffect(() => {
      const floorpSidebarPositionPref = panelSidebarConfig().position_start;
      applyFlexOrder(floorpSidebarPositionPref);
    });
  }
  gFlexOrder2.init = init;
  function applyFlexOrder(floorpSidebarPositionPref) {
    if (floorpSidebarPositionPref) {
      setOrders({
        floorpSidebarSplitter: 1e3,
        floorpSidebar: 1001,
        floorpSidebarSelectBox: 1002
      });
    } else {
      setOrders({
        floorpSidebarSelectBox: -3,
        floorpSidebar: -2,
        floorpSidebarSplitter: -1
      });
    }
    updateHoverOffset();
    scheduleHoverOffsetUpdate();
  }
  gFlexOrder2.applyFlexOrder = applyFlexOrder;
  function scheduleHoverOffsetUpdate() {
    if (hoverOffsetFrame !== void 0) {
      cancelAnimationFrame(hoverOffsetFrame);
    }
    hoverOffsetFrame = requestAnimationFrame(() => {
      hoverOffsetFrame = void 0;
      updateHoverOffset();
    });
  }
  function updateHoverOffset() {
    const browser = document?.getElementById("browser");
    if (!browser) return;
    browser.style.setProperty("--floorp-panel-sidebar-rail-width", `${document.getElementById(floorpSidebarSelectBoxId)?.getBoundingClientRect().width ?? 0}px`);
    const launcher = document.getElementById("sidebar-container");
    if (document.documentElement.hasAttribute("sidebar-expand-on-hover") && (launcher?.hasAttribute("sidebar-launcher-expanded") || launcher?.hasAttribute("sidebar-ongoing-animations"))) {
      return;
    }
    const atEnd = untrack(orders).floorpSidebar > 0;
    const browserRect = browser.getBoundingClientRect();
    let occupiedEdge = atEnd ? browserRect.right : browserRect.left;
    let hasVisiblePanel = false;
    for (const id of [
      floorpSidebarSelectBoxId,
      floorpSidebarId,
      floorpSidebarSplitterId
    ]) {
      const element = document.getElementById(id);
      if (!element) continue;
      const style = getComputedStyle(element);
      if (!style || style.display === "none" || style.position === "absolute" || style.position === "fixed") continue;
      const rect = element.getBoundingClientRect();
      occupiedEdge = atEnd ? Math.min(occupiedEdge, rect.left) : Math.max(occupiedEdge, rect.right);
      hasVisiblePanel = true;
    }
    const width = hasVisiblePanel ? Math.max(0, atEnd ? browserRect.right - occupiedEdge : occupiedEdge - browserRect.left) : 0;
    browser.style.setProperty("--floorp-panel-start-width", `${atEnd ? 0 : width}px`);
    browser.style.setProperty("--floorp-panel-end-width", `${atEnd ? width : 0}px`);
  }
  function observePanelWidths() {
    const browser = document?.getElementById("browser");
    if (!browser) return;
    const resizeObserver = new ResizeObserver(updateHoverOffset);
    const panelObserver = new MutationObserver(updateHoverOffset);
    const launcherObserver = new MutationObserver(() => {
      const launcher = document.getElementById("sidebar-container");
      if (!launcher?.hasAttribute("sidebar-launcher-expanded") && !launcher?.hasAttribute("sidebar-ongoing-animations")) {
        scheduleHoverOffsetUpdate();
      }
    });
    const observe = () => {
      resizeObserver.disconnect();
      panelObserver.disconnect();
      launcherObserver.disconnect();
      for (const id of [
        floorpSidebarSelectBoxId,
        floorpSidebarId,
        floorpSidebarSplitterId
      ]) {
        const element = document.getElementById(id);
        if (element) {
          resizeObserver.observe(element);
          panelObserver.observe(element, {
            attributes: true,
            attributeFilter: [
              "data-floating",
              "data-overlay",
              "hidden"
            ]
          });
        }
      }
      const launcher = document.getElementById("sidebar-container");
      if (launcher) {
        launcherObserver.observe(launcher, {
          attributes: true,
          attributeFilter: [
            "sidebar-positionend",
            "sidebar-launcher-expanded",
            "sidebar-ongoing-animations"
          ]
        });
      }
      updateHoverOffset();
      scheduleHoverOffsetUpdate();
    };
    const childrenObserver = new MutationObserver(observe);
    childrenObserver.observe(browser, {
      childList: true
    });
    observe();
    onCleanup(() => {
      resizeObserver.disconnect();
      panelObserver.disconnect();
      launcherObserver.disconnect();
      childrenObserver.disconnect();
      if (hoverOffsetFrame !== void 0) {
        cancelAnimationFrame(hoverOffsetFrame);
        hoverOffsetFrame = void 0;
      }
      browser.style.removeProperty("--floorp-panel-start-width");
      browser.style.removeProperty("--floorp-panel-end-width");
      browser.style.removeProperty("--floorp-panel-sidebar-rail-width");
    });
  }
  function renderOrderStyle() {
    _render(() => (() => {
      var _el$ = createElement("style");
      setProp(_el$, "id", "floorp-flex-order-style");
      setProp(_el$, "jsx", true);
      insert(_el$, () => `
      #${floorpSidebarId} {
        order: ${orders().floorpSidebar} !important;
      }
      #${floorpSidebarSelectBoxId} {
        order: ${orders().floorpSidebarSelectBox} !important;
      }
      #${floorpSidebarSplitterId} {
        order: ${orders().floorpSidebarSplitter} !important;
      }
      ${hoverStyle}
    `);
      return _el$;
    })(), document?.head);
  }
})(gFlexOrder || (gFlexOrder = {}));
var gFlexOrder;

function applyDecs2203RFactory() {
  function createAddInitializerMethod(initializers, decoratorFinishedRef) {
    return function addInitializer(initializer) {
      assertNotFinished(decoratorFinishedRef, "addInitializer");
      assertCallable(initializer, "An initializer");
      initializers.push(initializer);
    };
  }
  function memberDec(dec, name, desc, initializers, kind, isStatic, isPrivate, metadata, value) {
    var kindStr;
    switch (kind) {
      case 1:
        kindStr = "accessor";
        break;
      case 2:
        kindStr = "method";
        break;
      case 3:
        kindStr = "getter";
        break;
      case 4:
        kindStr = "setter";
        break;
      default:
        kindStr = "field";
    }
    var ctx = {
      kind: kindStr,
      name: isPrivate ? "#" + name : name,
      static: isStatic,
      private: isPrivate,
      metadata
    };
    var decoratorFinishedRef = {
      v: false
    };
    ctx.addInitializer = createAddInitializerMethod(initializers, decoratorFinishedRef);
    var get, set;
    if (kind === 0) {
      if (isPrivate) {
        get = desc.get;
        set = desc.set;
      } else {
        get = function() {
          return this[name];
        };
        set = function(v) {
          this[name] = v;
        };
      }
    } else if (kind === 2) {
      get = function() {
        return desc.value;
      };
    } else {
      if (kind === 1 || kind === 3) {
        get = function() {
          return desc.get.call(this);
        };
      }
      if (kind === 1 || kind === 4) {
        set = function(v) {
          desc.set.call(this, v);
        };
      }
    }
    if (get) {
      var originalGet = get;
      get = function(target) {
        if (arguments.length === 0) {
          target = this;
        }
        return originalGet.call(target);
      };
    }
    if (set) {
      var originalSet = set;
      set = function(target, value2) {
        if (arguments.length === 1) {
          value2 = target;
          target = this;
        }
        return originalSet.call(target, value2);
      };
    }
    if (isPrivate) {
      ctx.access = get && set ? {
        get,
        set
      } : get ? {
        get
      } : {
        set
      };
    } else {
      var has = function(target) {
        return name in target;
      };
      ctx.access = get && set ? {
        has,
        get,
        set
      } : get ? {
        has,
        get
      } : {
        has,
        set
      };
    }
    var newValue = dec(value, ctx);
    decoratorFinishedRef.v = true;
    return newValue;
  }
  function assertNotFinished(decoratorFinishedRef, fnName) {
    if (decoratorFinishedRef.v) {
      throw new Error("attempted to call " + fnName + " after decoration was finished");
    }
  }
  function assertCallable(fn, hint) {
    if (typeof fn !== "function") {
      throw new TypeError(hint + " must be a function");
    }
  }
  function assertValidReturnValue(kind, value) {
    var type = typeof value;
    if (kind === 1) {
      if (type !== "object" || value === null) {
        throw new TypeError("accessor decorators must return an object with get, set, or init properties or void 0");
      }
      if (value.get !== void 0) {
        assertCallable(value.get, "accessor.get");
      }
      if (value.set !== void 0) {
        assertCallable(value.set, "accessor.set");
      }
      if (value.init !== void 0) {
        assertCallable(value.init, "accessor.init");
      }
    } else if (type !== "function") {
      var hint;
      if (kind === 0) {
        hint = "field";
      } else if (kind === 10) {
        hint = "class";
      } else {
        hint = "method";
      }
      throw new TypeError(hint + " decorators must return a function or void 0");
    }
  }
  function applyMemberDec(ret, base, decInfo, name, kind, isStatic, isPrivate, initializers, metadata) {
    var decs = decInfo[0];
    var desc, init, value;
    if (isPrivate) {
      if (kind === 0 || kind === 1) {
        desc = {
          get: decInfo[3],
          set: decInfo[4]
        };
      } else if (kind === 3) {
        desc = {
          get: decInfo[3]
        };
      } else if (kind === 4) {
        desc = {
          set: decInfo[3]
        };
      } else {
        desc = {
          value: decInfo[3]
        };
      }
    } else if (kind !== 0) {
      desc = Object.getOwnPropertyDescriptor(base, name);
    }
    if (kind === 1) {
      value = {
        get: desc.get,
        set: desc.set
      };
    } else if (kind === 2) {
      value = desc.value;
    } else if (kind === 3) {
      value = desc.get;
    } else if (kind === 4) {
      value = desc.set;
    }
    var newValue, get, set;
    if (typeof decs === "function") {
      newValue = memberDec(decs, name, desc, initializers, kind, isStatic, isPrivate, metadata, value);
      if (newValue !== void 0) {
        assertValidReturnValue(kind, newValue);
        if (kind === 0) {
          init = newValue;
        } else if (kind === 1) {
          init = newValue.init;
          get = newValue.get || value.get;
          set = newValue.set || value.set;
          value = {
            get,
            set
          };
        } else {
          value = newValue;
        }
      }
    } else {
      for (var i = decs.length - 1; i >= 0; i--) {
        var dec = decs[i];
        newValue = memberDec(dec, name, desc, initializers, kind, isStatic, isPrivate, metadata, value);
        if (newValue !== void 0) {
          assertValidReturnValue(kind, newValue);
          var newInit;
          if (kind === 0) {
            newInit = newValue;
          } else if (kind === 1) {
            newInit = newValue.init;
            get = newValue.get || value.get;
            set = newValue.set || value.set;
            value = {
              get,
              set
            };
          } else {
            value = newValue;
          }
          if (newInit !== void 0) {
            if (init === void 0) {
              init = newInit;
            } else if (typeof init === "function") {
              init = [
                init,
                newInit
              ];
            } else {
              init.push(newInit);
            }
          }
        }
      }
    }
    if (kind === 0 || kind === 1) {
      if (init === void 0) {
        init = function(instance, init2) {
          return init2;
        };
      } else if (typeof init !== "function") {
        var ownInitializers = init;
        init = function(instance, init2) {
          var value2 = init2;
          for (var i2 = 0; i2 < ownInitializers.length; i2++) {
            value2 = ownInitializers[i2].call(instance, value2);
          }
          return value2;
        };
      } else {
        var originalInitializer = init;
        init = function(instance, init2) {
          return originalInitializer.call(instance, init2);
        };
      }
      ret.push(init);
    }
    if (kind !== 0) {
      if (kind === 1) {
        desc.get = value.get;
        desc.set = value.set;
      } else if (kind === 2) {
        desc.value = value;
      } else if (kind === 3) {
        desc.get = value;
      } else if (kind === 4) {
        desc.set = value;
      }
      if (isPrivate) {
        if (kind === 1) {
          ret.push(function(instance, args) {
            return value.get.call(instance, args);
          });
          ret.push(function(instance, args) {
            return value.set.call(instance, args);
          });
        } else if (kind === 2) {
          ret.push(value);
        } else {
          ret.push(function(instance, args) {
            return value.call(instance, args);
          });
        }
      } else {
        Object.defineProperty(base, name, desc);
      }
    }
  }
  function applyMemberDecs(Class, decInfos, metadata) {
    var ret = [];
    var protoInitializers;
    var staticInitializers;
    var existingProtoNonFields = /* @__PURE__ */ new Map();
    var existingStaticNonFields = /* @__PURE__ */ new Map();
    for (var i = 0; i < decInfos.length; i++) {
      var decInfo = decInfos[i];
      if (!Array.isArray(decInfo)) continue;
      var kind = decInfo[1];
      var name = decInfo[2];
      var isPrivate = decInfo.length > 3;
      var isStatic = kind >= 5;
      var base;
      var initializers;
      if (isStatic) {
        base = Class;
        kind = kind - 5;
        staticInitializers = staticInitializers || [];
        initializers = staticInitializers;
      } else {
        base = Class.prototype;
        protoInitializers = protoInitializers || [];
        initializers = protoInitializers;
      }
      if (kind !== 0 && !isPrivate) {
        var existingNonFields = isStatic ? existingStaticNonFields : existingProtoNonFields;
        var existingKind = existingNonFields.get(name) || 0;
        if (existingKind === true || existingKind === 3 && kind !== 4 || existingKind === 4 && kind !== 3) {
          throw new Error("Attempted to decorate a public method/accessor that has the same name as a previously decorated public method/accessor. This is not currently supported by the decorators plugin. Property name was: " + name);
        } else if (!existingKind && kind > 2) {
          existingNonFields.set(name, kind);
        } else {
          existingNonFields.set(name, true);
        }
      }
      applyMemberDec(ret, base, decInfo, name, kind, isStatic, isPrivate, initializers, metadata);
    }
    pushInitializers(ret, protoInitializers);
    pushInitializers(ret, staticInitializers);
    return ret;
  }
  function pushInitializers(ret, initializers) {
    if (initializers) {
      ret.push(function(instance) {
        for (var i = 0; i < initializers.length; i++) {
          initializers[i].call(instance);
        }
        return instance;
      });
    }
  }
  function applyClassDecs(targetClass, classDecs, metadata) {
    if (classDecs.length > 0) {
      var initializers = [];
      var newClass = targetClass;
      var name = targetClass.name;
      for (var i = classDecs.length - 1; i >= 0; i--) {
        var decoratorFinishedRef = {
          v: false
        };
        var nextNewClass = classDecs[i](newClass, {
          kind: "class",
          name,
          addInitializer: createAddInitializerMethod(initializers, decoratorFinishedRef),
          metadata
        });
        decoratorFinishedRef.v = true;
        if (nextNewClass !== void 0) {
          assertValidReturnValue(10, nextNewClass);
          newClass = nextNewClass;
        }
      }
      return [
        defineMetadata(newClass, metadata),
        function() {
          for (var i2 = 0; i2 < initializers.length; i2++) {
            initializers[i2].call(newClass);
          }
        }
      ];
    }
  }
  function defineMetadata(Class, metadata) {
    return Object.defineProperty(Class, Symbol.metadata || /* @__PURE__ */ Symbol.for("Symbol.metadata"), {
      configurable: true,
      enumerable: true,
      value: metadata
    });
  }
  return function applyDecs2203R(targetClass, memberDecs, classDecs, parentClass) {
    if (parentClass !== void 0) {
      var parentMetadata = parentClass[Symbol.metadata || /* @__PURE__ */ Symbol.for("Symbol.metadata")];
    }
    var metadata = Object.create(parentMetadata === void 0 ? null : parentMetadata);
    var e = applyMemberDecs(targetClass, memberDecs, metadata);
    if (!classDecs.length) defineMetadata(targetClass, metadata);
    return {
      e,
      get c() {
        return applyClassDecs(targetClass, classDecs, metadata);
      }
    };
  };
}
function _apply_decs_2203_r(targetClass, memberDecs, classDecs, parentClass) {
  return (_apply_decs_2203_r = applyDecs2203RFactory())(targetClass, memberDecs, classDecs, parentClass);
}
var _dec, _initClass, _NoraComponentBase;
_dec = noraComponent(void 0);
let _FlexOrder;
class FlexOrder extends (_NoraComponentBase = NoraComponentBase) {
  static {
    ({ c: [_FlexOrder, _initClass] } = _apply_decs_2203_r(this, [], [
      _dec
    ], _NoraComponentBase));
  }
  init() {
    gFlexOrder.init();
  }
  static {
    _initClass();
  }
}

export { _FlexOrder as default };
//# sourceMappingURL=index10.js.map
