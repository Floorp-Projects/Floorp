/**
 * @licstart The following is the entire license notice for the
 * JavaScript code in this page
 *
 * Copyright 2023 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @licend The above is the entire license notice for the
 * JavaScript code in this page
 */

/******/ // The require scope
/******/ var __webpack_require__ = {};
/******/ 
/************************************************************************/
/******/ /* webpack/runtime/define property getters */
/******/ (() => {
/******/ 	// define getter functions for harmony exports
/******/ 	__webpack_require__.d = (exports, definition) => {
/******/ 		for(var key in definition) {
/******/ 			if(__webpack_require__.o(definition, key) && !__webpack_require__.o(exports, key)) {
/******/ 				Object.defineProperty(exports, key, { enumerable: true, get: definition[key] });
/******/ 			}
/******/ 		}
/******/ 	};
/******/ })();
/******/ 
/******/ /* webpack/runtime/hasOwnProperty shorthand */
/******/ (() => {
/******/ 	__webpack_require__.o = (obj, prop) => (Object.prototype.hasOwnProperty.call(obj, prop))
/******/ })();
/******/ 
/************************************************************************/
var __webpack_exports__ = {};

// EXPORTS
__webpack_require__.d(__webpack_exports__, {
  PDFViewerApplication: () => (/* reexport */ PDFViewerApplication),
  PDFViewerApplicationConstants: () => (/* binding */ AppConstants),
  PDFViewerApplicationOptions: () => (/* reexport */ AppOptions)
});

;// CONCATENATED MODULE: ./web/ui_utils.js
const DEFAULT_SCALE_VALUE = "auto";
const DEFAULT_SCALE = 1.0;
const DEFAULT_SCALE_DELTA = 1.1;
const MIN_SCALE = 0.1;
const MAX_SCALE = 10.0;
const UNKNOWN_SCALE = 0;
const MAX_AUTO_SCALE = 1.25;
const SCROLLBAR_PADDING = 40;
const VERTICAL_PADDING = 5;
const RenderingStates = {
  INITIAL: 0,
  RUNNING: 1,
  PAUSED: 2,
  FINISHED: 3
};
const PresentationModeState = {
  UNKNOWN: 0,
  NORMAL: 1,
  CHANGING: 2,
  FULLSCREEN: 3
};
const SidebarView = {
  UNKNOWN: -1,
  NONE: 0,
  THUMBS: 1,
  OUTLINE: 2,
  ATTACHMENTS: 3,
  LAYERS: 4
};
const TextLayerMode = {
  DISABLE: 0,
  ENABLE: 1,
  ENABLE_PERMISSIONS: 2
};
const ScrollMode = {
  UNKNOWN: -1,
  VERTICAL: 0,
  HORIZONTAL: 1,
  WRAPPED: 2,
  PAGE: 3
};
const SpreadMode = {
  UNKNOWN: -1,
  NONE: 0,
  ODD: 1,
  EVEN: 2
};
const CursorTool = {
  SELECT: 0,
  HAND: 1,
  ZOOM: 2
};
const AutoPrintRegExp = /\bprint\s*\(/;
class OutputScale {
  constructor() {
    const pixelRatio = window.devicePixelRatio || 1;
    this.sx = pixelRatio;
    this.sy = pixelRatio;
  }
  get scaled() {
    return this.sx !== 1 || this.sy !== 1;
  }
}
function scrollIntoView(element, spot, scrollMatches = false) {
  let parent = element.offsetParent;
  if (!parent) {
    console.error("offsetParent is not set -- cannot scroll");
    return;
  }
  let offsetY = element.offsetTop + element.clientTop;
  let offsetX = element.offsetLeft + element.clientLeft;
  while (parent.clientHeight === parent.scrollHeight && parent.clientWidth === parent.scrollWidth || scrollMatches && (parent.classList.contains("markedContent") || getComputedStyle(parent).overflow === "hidden")) {
    offsetY += parent.offsetTop;
    offsetX += parent.offsetLeft;
    parent = parent.offsetParent;
    if (!parent) {
      return;
    }
  }
  if (spot) {
    if (spot.top !== undefined) {
      offsetY += spot.top;
    }
    if (spot.left !== undefined) {
      offsetX += spot.left;
      parent.scrollLeft = offsetX;
    }
  }
  parent.scrollTop = offsetY;
}
function watchScroll(viewAreaElement, callback) {
  const debounceScroll = function (evt) {
    if (rAF) {
      return;
    }
    rAF = window.requestAnimationFrame(function viewAreaElementScrolled() {
      rAF = null;
      const currentX = viewAreaElement.scrollLeft;
      const lastX = state.lastX;
      if (currentX !== lastX) {
        state.right = currentX > lastX;
      }
      state.lastX = currentX;
      const currentY = viewAreaElement.scrollTop;
      const lastY = state.lastY;
      if (currentY !== lastY) {
        state.down = currentY > lastY;
      }
      state.lastY = currentY;
      callback(state);
    });
  };
  const state = {
    right: true,
    down: true,
    lastX: viewAreaElement.scrollLeft,
    lastY: viewAreaElement.scrollTop,
    _eventHandler: debounceScroll
  };
  let rAF = null;
  viewAreaElement.addEventListener("scroll", debounceScroll, true);
  return state;
}
function parseQueryString(query) {
  const params = new Map();
  for (const [key, value] of new URLSearchParams(query)) {
    params.set(key.toLowerCase(), value);
  }
  return params;
}
const InvisibleCharactersRegExp = /[\x00-\x1F]/g;
function removeNullCharacters(str, replaceInvisible = false) {
  if (!InvisibleCharactersRegExp.test(str)) {
    return str;
  }
  if (replaceInvisible) {
    return str.replaceAll(InvisibleCharactersRegExp, m => {
      return m === "\x00" ? "" : " ";
    });
  }
  return str.replaceAll("\x00", "");
}
function binarySearchFirstItem(items, condition, start = 0) {
  let minIndex = start;
  let maxIndex = items.length - 1;
  if (maxIndex < 0 || !condition(items[maxIndex])) {
    return items.length;
  }
  if (condition(items[minIndex])) {
    return minIndex;
  }
  while (minIndex < maxIndex) {
    const currentIndex = minIndex + maxIndex >> 1;
    const currentItem = items[currentIndex];
    if (condition(currentItem)) {
      maxIndex = currentIndex;
    } else {
      minIndex = currentIndex + 1;
    }
  }
  return minIndex;
}
function approximateFraction(x) {
  if (Math.floor(x) === x) {
    return [x, 1];
  }
  const xinv = 1 / x;
  const limit = 8;
  if (xinv > limit) {
    return [1, limit];
  } else if (Math.floor(xinv) === xinv) {
    return [1, xinv];
  }
  const x_ = x > 1 ? xinv : x;
  let a = 0,
    b = 1,
    c = 1,
    d = 1;
  while (true) {
    const p = a + c,
      q = b + d;
    if (q > limit) {
      break;
    }
    if (x_ <= p / q) {
      c = p;
      d = q;
    } else {
      a = p;
      b = q;
    }
  }
  let result;
  if (x_ - a / b < c / d - x_) {
    result = x_ === x ? [a, b] : [b, a];
  } else {
    result = x_ === x ? [c, d] : [d, c];
  }
  return result;
}
function roundToDivide(x, div) {
  const r = x % div;
  return r === 0 ? x : Math.round(x - r + div);
}
function getPageSizeInches({
  view,
  userUnit,
  rotate
}) {
  const [x1, y1, x2, y2] = view;
  const changeOrientation = rotate % 180 !== 0;
  const width = (x2 - x1) / 72 * userUnit;
  const height = (y2 - y1) / 72 * userUnit;
  return {
    width: changeOrientation ? height : width,
    height: changeOrientation ? width : height
  };
}
function backtrackBeforeAllVisibleElements(index, views, top) {
  if (index < 2) {
    return index;
  }
  let elt = views[index].div;
  let pageTop = elt.offsetTop + elt.clientTop;
  if (pageTop >= top) {
    elt = views[index - 1].div;
    pageTop = elt.offsetTop + elt.clientTop;
  }
  for (let i = index - 2; i >= 0; --i) {
    elt = views[i].div;
    if (elt.offsetTop + elt.clientTop + elt.clientHeight <= pageTop) {
      break;
    }
    index = i;
  }
  return index;
}
function getVisibleElements({
  scrollEl,
  views,
  sortByVisibility = false,
  horizontal = false,
  rtl = false
}) {
  const top = scrollEl.scrollTop,
    bottom = top + scrollEl.clientHeight;
  const left = scrollEl.scrollLeft,
    right = left + scrollEl.clientWidth;
  function isElementBottomAfterViewTop(view) {
    const element = view.div;
    const elementBottom = element.offsetTop + element.clientTop + element.clientHeight;
    return elementBottom > top;
  }
  function isElementNextAfterViewHorizontally(view) {
    const element = view.div;
    const elementLeft = element.offsetLeft + element.clientLeft;
    const elementRight = elementLeft + element.clientWidth;
    return rtl ? elementLeft < right : elementRight > left;
  }
  const visible = [],
    ids = new Set(),
    numViews = views.length;
  let firstVisibleElementInd = binarySearchFirstItem(views, horizontal ? isElementNextAfterViewHorizontally : isElementBottomAfterViewTop);
  if (firstVisibleElementInd > 0 && firstVisibleElementInd < numViews && !horizontal) {
    firstVisibleElementInd = backtrackBeforeAllVisibleElements(firstVisibleElementInd, views, top);
  }
  let lastEdge = horizontal ? right : -1;
  for (let i = firstVisibleElementInd; i < numViews; i++) {
    const view = views[i],
      element = view.div;
    const currentWidth = element.offsetLeft + element.clientLeft;
    const currentHeight = element.offsetTop + element.clientTop;
    const viewWidth = element.clientWidth,
      viewHeight = element.clientHeight;
    const viewRight = currentWidth + viewWidth;
    const viewBottom = currentHeight + viewHeight;
    if (lastEdge === -1) {
      if (viewBottom >= bottom) {
        lastEdge = viewBottom;
      }
    } else if ((horizontal ? currentWidth : currentHeight) > lastEdge) {
      break;
    }
    if (viewBottom <= top || currentHeight >= bottom || viewRight <= left || currentWidth >= right) {
      continue;
    }
    const hiddenHeight = Math.max(0, top - currentHeight) + Math.max(0, viewBottom - bottom);
    const hiddenWidth = Math.max(0, left - currentWidth) + Math.max(0, viewRight - right);
    const fractionHeight = (viewHeight - hiddenHeight) / viewHeight,
      fractionWidth = (viewWidth - hiddenWidth) / viewWidth;
    const percent = fractionHeight * fractionWidth * 100 | 0;
    visible.push({
      id: view.id,
      x: currentWidth,
      y: currentHeight,
      view,
      percent,
      widthPercent: fractionWidth * 100 | 0
    });
    ids.add(view.id);
  }
  const first = visible[0],
    last = visible.at(-1);
  if (sortByVisibility) {
    visible.sort(function (a, b) {
      const pc = a.percent - b.percent;
      if (Math.abs(pc) > 0.001) {
        return -pc;
      }
      return a.id - b.id;
    });
  }
  return {
    first,
    last,
    views: visible,
    ids
  };
}
function normalizeWheelEventDirection(evt) {
  let delta = Math.hypot(evt.deltaX, evt.deltaY);
  const angle = Math.atan2(evt.deltaY, evt.deltaX);
  if (-0.25 * Math.PI < angle && angle < 0.75 * Math.PI) {
    delta = -delta;
  }
  return delta;
}
function normalizeWheelEventDelta(evt) {
  const deltaMode = evt.deltaMode;
  let delta = normalizeWheelEventDirection(evt);
  const MOUSE_PIXELS_PER_LINE = 30;
  const MOUSE_LINES_PER_PAGE = 30;
  if (deltaMode === WheelEvent.DOM_DELTA_PIXEL) {
    delta /= MOUSE_PIXELS_PER_LINE * MOUSE_LINES_PER_PAGE;
  } else if (deltaMode === WheelEvent.DOM_DELTA_LINE) {
    delta /= MOUSE_LINES_PER_PAGE;
  }
  return delta;
}
function isValidRotation(angle) {
  return Number.isInteger(angle) && angle % 90 === 0;
}
function isValidScrollMode(mode) {
  return Number.isInteger(mode) && Object.values(ScrollMode).includes(mode) && mode !== ScrollMode.UNKNOWN;
}
function isValidSpreadMode(mode) {
  return Number.isInteger(mode) && Object.values(SpreadMode).includes(mode) && mode !== SpreadMode.UNKNOWN;
}
function isPortraitOrientation(size) {
  return size.width <= size.height;
}
const animationStarted = new Promise(function (resolve) {
  window.requestAnimationFrame(resolve);
});
const docStyle = document.documentElement.style;
function clamp(v, min, max) {
  return Math.min(Math.max(v, min), max);
}
class ProgressBar {
  #classList = null;
  #disableAutoFetchTimeout = null;
  #percent = 0;
  #style = null;
  #visible = true;
  constructor(bar) {
    this.#classList = bar.classList;
    this.#style = bar.style;
  }
  get percent() {
    return this.#percent;
  }
  set percent(val) {
    this.#percent = clamp(val, 0, 100);
    if (isNaN(val)) {
      this.#classList.add("indeterminate");
      return;
    }
    this.#classList.remove("indeterminate");
    this.#style.setProperty("--progressBar-percent", `${this.#percent}%`);
  }
  setWidth(viewer) {
    if (!viewer) {
      return;
    }
    const container = viewer.parentNode;
    const scrollbarWidth = container.offsetWidth - viewer.offsetWidth;
    if (scrollbarWidth > 0) {
      this.#style.setProperty("--progressBar-end-offset", `${scrollbarWidth}px`);
    }
  }
  setDisableAutoFetch(delay = 5000) {
    if (isNaN(this.#percent)) {
      return;
    }
    if (this.#disableAutoFetchTimeout) {
      clearTimeout(this.#disableAutoFetchTimeout);
    }
    this.show();
    this.#disableAutoFetchTimeout = setTimeout(() => {
      this.#disableAutoFetchTimeout = null;
      this.hide();
    }, delay);
  }
  hide() {
    if (!this.#visible) {
      return;
    }
    this.#visible = false;
    this.#classList.add("hidden");
  }
  show() {
    if (this.#visible) {
      return;
    }
    this.#visible = true;
    this.#classList.remove("hidden");
  }
}
function getActiveOrFocusedElement() {
  let curRoot = document;
  let curActiveOrFocused = curRoot.activeElement || curRoot.querySelector(":focus");
  while (curActiveOrFocused?.shadowRoot) {
    curRoot = curActiveOrFocused.shadowRoot;
    curActiveOrFocused = curRoot.activeElement || curRoot.querySelector(":focus");
  }
  return curActiveOrFocused;
}
function apiPageLayoutToViewerModes(layout) {
  let scrollMode = ScrollMode.VERTICAL,
    spreadMode = SpreadMode.NONE;
  switch (layout) {
    case "SinglePage":
      scrollMode = ScrollMode.PAGE;
      break;
    case "OneColumn":
      break;
    case "TwoPageLeft":
      scrollMode = ScrollMode.PAGE;
    case "TwoColumnLeft":
      spreadMode = SpreadMode.ODD;
      break;
    case "TwoPageRight":
      scrollMode = ScrollMode.PAGE;
    case "TwoColumnRight":
      spreadMode = SpreadMode.EVEN;
      break;
  }
  return {
    scrollMode,
    spreadMode
  };
}
function apiPageModeToSidebarView(mode) {
  switch (mode) {
    case "UseNone":
      return SidebarView.NONE;
    case "UseThumbs":
      return SidebarView.THUMBS;
    case "UseOutlines":
      return SidebarView.OUTLINE;
    case "UseAttachments":
      return SidebarView.ATTACHMENTS;
    case "UseOC":
      return SidebarView.LAYERS;
  }
  return SidebarView.NONE;
}
function toggleCheckedBtn(button, toggle, view = null) {
  button.classList.toggle("toggled", toggle);
  button.setAttribute("aria-checked", toggle);
  view?.classList.toggle("hidden", !toggle);
}
function toggleExpandedBtn(button, toggle, view = null) {
  button.classList.toggle("toggled", toggle);
  button.setAttribute("aria-expanded", toggle);
  view?.classList.toggle("hidden", !toggle);
}

;// CONCATENATED MODULE: ./web/pdfjs.js
;
const {
  AbortException,
  AnnotationEditorLayer,
  AnnotationEditorParamsType,
  AnnotationEditorType,
  AnnotationEditorUIManager,
  AnnotationLayer,
  AnnotationMode,
  build,
  CMapCompressionType,
  createValidAbsoluteUrl,
  DOMSVGFactory,
  FeatureTest,
  fetchData,
  getDocument,
  getFilenameFromUrl,
  getPdfFilenameFromUrl,
  getXfaPageViewport,
  GlobalWorkerOptions,
  ImageKind,
  InvalidPDFException,
  isDataScheme,
  isPdfFile,
  MissingPDFException,
  noContextMenu,
  normalizeUnicode,
  OPS,
  PasswordResponses,
  PDFDataRangeTransport,
  PDFDateString,
  PDFWorker,
  PermissionFlag,
  PixelsPerInch,
  PromiseCapability,
  RenderingCancelledException,
  renderTextLayer,
  setLayerDimensions,
  shadow,
  UnexpectedResponseException,
  updateTextLayer,
  Util,
  VerbosityLevel,
  version,
  XfaLayer
} = globalThis.pdfjsLib;

;// CONCATENATED MODULE: ./web/app_options.js
const compatibilityParams = Object.create(null);
;
const OptionKind = {
  BROWSER: 0x01,
  VIEWER: 0x02,
  API: 0x04,
  WORKER: 0x08,
  PREFERENCE: 0x80
};
const defaultOptions = {
  canvasMaxAreaInBytes: {
    value: -1,
    kind: OptionKind.BROWSER + OptionKind.API
  },
  isInAutomation: {
    value: false,
    kind: OptionKind.BROWSER
  },
  supportsDocumentFonts: {
    value: true,
    kind: OptionKind.BROWSER
  },
  supportsIntegratedFind: {
    value: false,
    kind: OptionKind.BROWSER
  },
  supportsMouseWheelZoomCtrlKey: {
    value: true,
    kind: OptionKind.BROWSER
  },
  supportsMouseWheelZoomMetaKey: {
    value: true,
    kind: OptionKind.BROWSER
  },
  supportsPinchToZoom: {
    value: true,
    kind: OptionKind.BROWSER
  },
  annotationEditorMode: {
    value: 0,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  annotationMode: {
    value: 2,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  cursorToolOnLoad: {
    value: 0,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  defaultZoomDelay: {
    value: 400,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  defaultZoomValue: {
    value: "",
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  disableHistory: {
    value: false,
    kind: OptionKind.VIEWER
  },
  disablePageLabels: {
    value: false,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  enablePermissions: {
    value: false,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  enablePrintAutoRotate: {
    value: true,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  enableScripting: {
    value: true,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  externalLinkRel: {
    value: "noopener noreferrer nofollow",
    kind: OptionKind.VIEWER
  },
  externalLinkTarget: {
    value: 0,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  historyUpdateUrl: {
    value: false,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  ignoreDestinationZoom: {
    value: false,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  imageResourcesPath: {
    value: "resource://pdf.js/web/images/",
    kind: OptionKind.VIEWER
  },
  maxCanvasPixels: {
    value: 16777216,
    kind: OptionKind.VIEWER
  },
  forcePageColors: {
    value: false,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  pageColorsBackground: {
    value: "Canvas",
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  pageColorsForeground: {
    value: "CanvasText",
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  pdfBugEnabled: {
    value: false,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  printResolution: {
    value: 150,
    kind: OptionKind.VIEWER
  },
  sidebarViewOnLoad: {
    value: -1,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  scrollModeOnLoad: {
    value: -1,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  spreadModeOnLoad: {
    value: -1,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  textLayerMode: {
    value: 1,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  viewOnLoad: {
    value: 0,
    kind: OptionKind.VIEWER + OptionKind.PREFERENCE
  },
  cMapPacked: {
    value: true,
    kind: OptionKind.API
  },
  cMapUrl: {
    value: "resource://pdf.js/web/cmaps/",
    kind: OptionKind.API
  },
  disableAutoFetch: {
    value: false,
    kind: OptionKind.API + OptionKind.PREFERENCE
  },
  disableFontFace: {
    value: false,
    kind: OptionKind.API + OptionKind.PREFERENCE
  },
  disableRange: {
    value: false,
    kind: OptionKind.API + OptionKind.PREFERENCE
  },
  disableStream: {
    value: false,
    kind: OptionKind.API + OptionKind.PREFERENCE
  },
  docBaseUrl: {
    value: "",
    kind: OptionKind.API
  },
  enableXfa: {
    value: true,
    kind: OptionKind.API + OptionKind.PREFERENCE
  },
  fontExtraProperties: {
    value: false,
    kind: OptionKind.API
  },
  isEvalSupported: {
    value: true,
    kind: OptionKind.API
  },
  isOffscreenCanvasSupported: {
    value: true,
    kind: OptionKind.API
  },
  maxImageSize: {
    value: -1,
    kind: OptionKind.API
  },
  pdfBug: {
    value: false,
    kind: OptionKind.API
  },
  standardFontDataUrl: {
    value: "resource://pdf.js/web/standard_fonts/",
    kind: OptionKind.API
  },
  verbosity: {
    value: 1,
    kind: OptionKind.API
  },
  workerPort: {
    value: null,
    kind: OptionKind.WORKER
  },
  workerSrc: {
    value: "resource://pdf.js/build/pdf.worker.mjs",
    kind: OptionKind.WORKER
  }
};
;
const userOptions = Object.create(null);
class AppOptions {
  constructor() {
    throw new Error("Cannot initialize AppOptions.");
  }
  static get(name) {
    const userOption = userOptions[name];
    if (userOption !== undefined) {
      return userOption;
    }
    const defaultOption = defaultOptions[name];
    if (defaultOption !== undefined) {
      return compatibilityParams[name] ?? defaultOption.value;
    }
    return undefined;
  }
  static getAll(kind = null) {
    const options = Object.create(null);
    for (const name in defaultOptions) {
      const defaultOption = defaultOptions[name];
      if (kind) {
        if (!(kind & defaultOption.kind)) {
          continue;
        }
      }
      const userOption = userOptions[name];
      options[name] = userOption !== undefined ? userOption : compatibilityParams[name] ?? defaultOption.value;
    }
    return options;
  }
  static set(name, value) {
    userOptions[name] = value;
  }
  static setAll(options, init = false) {
    for (const name in options) {
      userOptions[name] = options[name];
    }
  }
  static remove(name) {
    delete userOptions[name];
  }
}

;// CONCATENATED MODULE: ./web/event_utils.js
const WaitOnType = {
  EVENT: "event",
  TIMEOUT: "timeout"
};
function waitOnEventOrTimeout({
  target,
  name,
  delay = 0
}) {
  return new Promise(function (resolve, reject) {
    if (typeof target !== "object" || !(name && typeof name === "string") || !(Number.isInteger(delay) && delay >= 0)) {
      throw new Error("waitOnEventOrTimeout - invalid parameters.");
    }
    function handler(type) {
      if (target instanceof EventBus) {
        target._off(name, eventHandler);
      } else {
        target.removeEventListener(name, eventHandler);
      }
      if (timeout) {
        clearTimeout(timeout);
      }
      resolve(type);
    }
    const eventHandler = handler.bind(null, WaitOnType.EVENT);
    if (target instanceof EventBus) {
      target._on(name, eventHandler);
    } else {
      target.addEventListener(name, eventHandler);
    }
    const timeoutHandler = handler.bind(null, WaitOnType.TIMEOUT);
    const timeout = setTimeout(timeoutHandler, delay);
  });
}
class EventBus {
  #listeners = Object.create(null);
  on(eventName, listener, options = null) {
    this._on(eventName, listener, {
      external: true,
      once: options?.once
    });
  }
  off(eventName, listener, options = null) {
    this._off(eventName, listener, {
      external: true,
      once: options?.once
    });
  }
  dispatch(eventName, data) {
    const eventListeners = this.#listeners[eventName];
    if (!eventListeners || eventListeners.length === 0) {
      return;
    }
    let externalListeners;
    for (const {
      listener,
      external,
      once
    } of eventListeners.slice(0)) {
      if (once) {
        this._off(eventName, listener);
      }
      if (external) {
        (externalListeners ||= []).push(listener);
        continue;
      }
      listener(data);
    }
    if (externalListeners) {
      for (const listener of externalListeners) {
        listener(data);
      }
      externalListeners = null;
    }
  }
  _on(eventName, listener, options = null) {
    const eventListeners = this.#listeners[eventName] ||= [];
    eventListeners.push({
      listener,
      external: options?.external === true,
      once: options?.once === true
    });
  }
  _off(eventName, listener, options = null) {
    const eventListeners = this.#listeners[eventName];
    if (!eventListeners) {
      return;
    }
    for (let i = 0, ii = eventListeners.length; i < ii; i++) {
      if (eventListeners[i].listener === listener) {
        eventListeners.splice(i, 1);
        return;
      }
    }
  }
}
class AutomationEventBus extends EventBus {
  dispatch(eventName, data) {
    super.dispatch(eventName, data);
    const detail = Object.create(null);
    if (data) {
      for (const key in data) {
        const value = data[key];
        if (key === "source") {
          if (value === window || value === document) {
            return;
          }
          continue;
        }
        detail[key] = value;
      }
    }
    const event = new CustomEvent(eventName, {
      bubbles: true,
      cancelable: true,
      detail
    });
    document.dispatchEvent(event);
  }
}

;// CONCATENATED MODULE: ./web/pdf_link_service.js

const DEFAULT_LINK_REL = "noopener noreferrer nofollow";
const LinkTarget = {
  NONE: 0,
  SELF: 1,
  BLANK: 2,
  PARENT: 3,
  TOP: 4
};
function addLinkAttributes(link, {
  url,
  target,
  rel,
  enabled = true
} = {}) {
  if (!url || typeof url !== "string") {
    throw new Error('A valid "url" parameter must provided.');
  }
  if (enabled) {
    link.href = link.title = url;
  } else {
    link.href = "";
    link.title = `Disabled: ${url}`;
    link.onclick = () => {
      return false;
    };
  }
  let targetStr = "";
  switch (target) {
    case LinkTarget.NONE:
      break;
    case LinkTarget.SELF:
      targetStr = "_self";
      break;
    case LinkTarget.BLANK:
      targetStr = "_blank";
      break;
    case LinkTarget.PARENT:
      targetStr = "_parent";
      break;
    case LinkTarget.TOP:
      targetStr = "_top";
      break;
  }
  link.target = targetStr;
  link.rel = typeof rel === "string" ? rel : DEFAULT_LINK_REL;
}
class PDFLinkService {
  #pagesRefCache = new Map();
  constructor({
    eventBus,
    externalLinkTarget = null,
    externalLinkRel = null,
    ignoreDestinationZoom = false
  } = {}) {
    this.eventBus = eventBus;
    this.externalLinkTarget = externalLinkTarget;
    this.externalLinkRel = externalLinkRel;
    this.externalLinkEnabled = true;
    this._ignoreDestinationZoom = ignoreDestinationZoom;
    this.baseUrl = null;
    this.pdfDocument = null;
    this.pdfViewer = null;
    this.pdfHistory = null;
  }
  setDocument(pdfDocument, baseUrl = null) {
    this.baseUrl = baseUrl;
    this.pdfDocument = pdfDocument;
    this.#pagesRefCache.clear();
  }
  setViewer(pdfViewer) {
    this.pdfViewer = pdfViewer;
  }
  setHistory(pdfHistory) {
    this.pdfHistory = pdfHistory;
  }
  get pagesCount() {
    return this.pdfDocument ? this.pdfDocument.numPages : 0;
  }
  get page() {
    return this.pdfViewer.currentPageNumber;
  }
  set page(value) {
    this.pdfViewer.currentPageNumber = value;
  }
  get rotation() {
    return this.pdfViewer.pagesRotation;
  }
  set rotation(value) {
    this.pdfViewer.pagesRotation = value;
  }
  get isInPresentationMode() {
    return this.pdfViewer.isInPresentationMode;
  }
  #goToDestinationHelper(rawDest, namedDest = null, explicitDest) {
    const destRef = explicitDest[0];
    let pageNumber;
    if (typeof destRef === "object" && destRef !== null) {
      pageNumber = this._cachedPageNumber(destRef);
      if (!pageNumber) {
        this.pdfDocument.getPageIndex(destRef).then(pageIndex => {
          this.cachePageRef(pageIndex + 1, destRef);
          this.#goToDestinationHelper(rawDest, namedDest, explicitDest);
        }).catch(() => {
          console.error(`PDFLinkService.#goToDestinationHelper: "${destRef}" is not ` + `a valid page reference, for dest="${rawDest}".`);
        });
        return;
      }
    } else if (Number.isInteger(destRef)) {
      pageNumber = destRef + 1;
    } else {
      console.error(`PDFLinkService.#goToDestinationHelper: "${destRef}" is not ` + `a valid destination reference, for dest="${rawDest}".`);
      return;
    }
    if (!pageNumber || pageNumber < 1 || pageNumber > this.pagesCount) {
      console.error(`PDFLinkService.#goToDestinationHelper: "${pageNumber}" is not ` + `a valid page number, for dest="${rawDest}".`);
      return;
    }
    if (this.pdfHistory) {
      this.pdfHistory.pushCurrentPosition();
      this.pdfHistory.push({
        namedDest,
        explicitDest,
        pageNumber
      });
    }
    this.pdfViewer.scrollPageIntoView({
      pageNumber,
      destArray: explicitDest,
      ignoreDestinationZoom: this._ignoreDestinationZoom
    });
  }
  async goToDestination(dest) {
    if (!this.pdfDocument) {
      return;
    }
    let namedDest, explicitDest;
    if (typeof dest === "string") {
      namedDest = dest;
      explicitDest = await this.pdfDocument.getDestination(dest);
    } else {
      namedDest = null;
      explicitDest = await dest;
    }
    if (!Array.isArray(explicitDest)) {
      console.error(`PDFLinkService.goToDestination: "${explicitDest}" is not ` + `a valid destination array, for dest="${dest}".`);
      return;
    }
    this.#goToDestinationHelper(dest, namedDest, explicitDest);
  }
  goToPage(val) {
    if (!this.pdfDocument) {
      return;
    }
    const pageNumber = typeof val === "string" && this.pdfViewer.pageLabelToPageNumber(val) || val | 0;
    if (!(Number.isInteger(pageNumber) && pageNumber > 0 && pageNumber <= this.pagesCount)) {
      console.error(`PDFLinkService.goToPage: "${val}" is not a valid page.`);
      return;
    }
    if (this.pdfHistory) {
      this.pdfHistory.pushCurrentPosition();
      this.pdfHistory.pushPage(pageNumber);
    }
    this.pdfViewer.scrollPageIntoView({
      pageNumber
    });
  }
  addLinkAttributes(link, url, newWindow = false) {
    addLinkAttributes(link, {
      url,
      target: newWindow ? LinkTarget.BLANK : this.externalLinkTarget,
      rel: this.externalLinkRel,
      enabled: this.externalLinkEnabled
    });
  }
  getDestinationHash(dest) {
    if (typeof dest === "string") {
      if (dest.length > 0) {
        return this.getAnchorUrl("#" + escape(dest));
      }
    } else if (Array.isArray(dest)) {
      const str = JSON.stringify(dest);
      if (str.length > 0) {
        return this.getAnchorUrl("#" + escape(str));
      }
    }
    return this.getAnchorUrl("");
  }
  getAnchorUrl(anchor) {
    return this.baseUrl ? this.baseUrl + anchor : anchor;
  }
  setHash(hash) {
    if (!this.pdfDocument) {
      return;
    }
    let pageNumber, dest;
    if (hash.includes("=")) {
      const params = parseQueryString(hash);
      if (params.has("search")) {
        const query = params.get("search").replaceAll('"', ""),
          phrase = params.get("phrase") === "true";
        this.eventBus.dispatch("findfromurlhash", {
          source: this,
          query: phrase ? query : query.match(/\S+/g)
        });
      }
      if (params.has("page")) {
        pageNumber = params.get("page") | 0 || 1;
      }
      if (params.has("zoom")) {
        const zoomArgs = params.get("zoom").split(",");
        const zoomArg = zoomArgs[0];
        const zoomArgNumber = parseFloat(zoomArg);
        if (!zoomArg.includes("Fit")) {
          dest = [null, {
            name: "XYZ"
          }, zoomArgs.length > 1 ? zoomArgs[1] | 0 : null, zoomArgs.length > 2 ? zoomArgs[2] | 0 : null, zoomArgNumber ? zoomArgNumber / 100 : zoomArg];
        } else if (zoomArg === "Fit" || zoomArg === "FitB") {
          dest = [null, {
            name: zoomArg
          }];
        } else if (zoomArg === "FitH" || zoomArg === "FitBH" || zoomArg === "FitV" || zoomArg === "FitBV") {
          dest = [null, {
            name: zoomArg
          }, zoomArgs.length > 1 ? zoomArgs[1] | 0 : null];
        } else if (zoomArg === "FitR") {
          if (zoomArgs.length !== 5) {
            console.error('PDFLinkService.setHash: Not enough parameters for "FitR".');
          } else {
            dest = [null, {
              name: zoomArg
            }, zoomArgs[1] | 0, zoomArgs[2] | 0, zoomArgs[3] | 0, zoomArgs[4] | 0];
          }
        } else {
          console.error(`PDFLinkService.setHash: "${zoomArg}" is not a valid zoom value.`);
        }
      }
      if (dest) {
        this.pdfViewer.scrollPageIntoView({
          pageNumber: pageNumber || this.page,
          destArray: dest,
          allowNegativeOffset: true
        });
      } else if (pageNumber) {
        this.page = pageNumber;
      }
      if (params.has("pagemode")) {
        this.eventBus.dispatch("pagemode", {
          source: this,
          mode: params.get("pagemode")
        });
      }
      if (params.has("nameddest")) {
        this.goToDestination(params.get("nameddest"));
      }
    } else {
      dest = unescape(hash);
      try {
        dest = JSON.parse(dest);
        if (!Array.isArray(dest)) {
          dest = dest.toString();
        }
      } catch {}
      if (typeof dest === "string" || PDFLinkService.#isValidExplicitDestination(dest)) {
        this.goToDestination(dest);
        return;
      }
      console.error(`PDFLinkService.setHash: "${unescape(hash)}" is not a valid destination.`);
    }
  }
  executeNamedAction(action) {
    switch (action) {
      case "GoBack":
        this.pdfHistory?.back();
        break;
      case "GoForward":
        this.pdfHistory?.forward();
        break;
      case "NextPage":
        this.pdfViewer.nextPage();
        break;
      case "PrevPage":
        this.pdfViewer.previousPage();
        break;
      case "LastPage":
        this.page = this.pagesCount;
        break;
      case "FirstPage":
        this.page = 1;
        break;
      default:
        break;
    }
    this.eventBus.dispatch("namedaction", {
      source: this,
      action
    });
  }
  async executeSetOCGState(action) {
    const pdfDocument = this.pdfDocument;
    const optionalContentConfig = await this.pdfViewer.optionalContentConfigPromise;
    if (pdfDocument !== this.pdfDocument) {
      return;
    }
    let operator;
    for (const elem of action.state) {
      switch (elem) {
        case "ON":
        case "OFF":
        case "Toggle":
          operator = elem;
          continue;
      }
      switch (operator) {
        case "ON":
          optionalContentConfig.setVisibility(elem, true);
          break;
        case "OFF":
          optionalContentConfig.setVisibility(elem, false);
          break;
        case "Toggle":
          const group = optionalContentConfig.getGroup(elem);
          if (group) {
            optionalContentConfig.setVisibility(elem, !group.visible);
          }
          break;
      }
    }
    this.pdfViewer.optionalContentConfigPromise = Promise.resolve(optionalContentConfig);
  }
  cachePageRef(pageNum, pageRef) {
    if (!pageRef) {
      return;
    }
    const refStr = pageRef.gen === 0 ? `${pageRef.num}R` : `${pageRef.num}R${pageRef.gen}`;
    this.#pagesRefCache.set(refStr, pageNum);
  }
  _cachedPageNumber(pageRef) {
    if (!pageRef) {
      return null;
    }
    const refStr = pageRef.gen === 0 ? `${pageRef.num}R` : `${pageRef.num}R${pageRef.gen}`;
    return this.#pagesRefCache.get(refStr) || null;
  }
  static #isValidExplicitDestination(dest) {
    if (!Array.isArray(dest)) {
      return false;
    }
    const destLength = dest.length;
    if (destLength < 2) {
      return false;
    }
    const page = dest[0];
    if (!(typeof page === "object" && Number.isInteger(page.num) && Number.isInteger(page.gen)) && !(Number.isInteger(page) && page >= 0)) {
      return false;
    }
    const zoom = dest[1];
    if (!(typeof zoom === "object" && typeof zoom.name === "string")) {
      return false;
    }
    let allowNull = true;
    switch (zoom.name) {
      case "XYZ":
        if (destLength !== 5) {
          return false;
        }
        break;
      case "Fit":
      case "FitB":
        return destLength === 2;
      case "FitH":
      case "FitBH":
      case "FitV":
      case "FitBV":
        if (destLength !== 3) {
          return false;
        }
        break;
      case "FitR":
        if (destLength !== 6) {
          return false;
        }
        allowNull = false;
        break;
      default:
        return false;
    }
    for (let i = 2; i < destLength; i++) {
      const param = dest[i];
      if (!(typeof param === "number" || allowNull && param === null)) {
        return false;
      }
    }
    return true;
  }
}
class SimpleLinkService {
  constructor() {
    this.externalLinkEnabled = true;
  }
  get pagesCount() {
    return 0;
  }
  get page() {
    return 0;
  }
  set page(value) {}
  get rotation() {
    return 0;
  }
  set rotation(value) {}
  get isInPresentationMode() {
    return false;
  }
  async goToDestination(dest) {}
  goToPage(val) {}
  addLinkAttributes(link, url, newWindow = false) {
    addLinkAttributes(link, {
      url,
      enabled: this.externalLinkEnabled
    });
  }
  getDestinationHash(dest) {
    return "#";
  }
  getAnchorUrl(hash) {
    return "#";
  }
  setHash(hash) {}
  executeNamedAction(action) {}
  executeSetOCGState(action) {}
  cachePageRef(pageNum, pageRef) {}
}

;// CONCATENATED MODULE: ./web/stubs-geckoview.js
const AltTextManager = null;
const AnnotationEditorParams = null;
const PDFAttachmentViewer = null;
const PDFCursorTools = null;
const PDFDocumentProperties = null;
const PDFFindBar = null;
const PDFLayerViewer = null;
const PDFOutlineViewer = null;
const PDFPresentationMode = null;
const PDFSidebar = null;
const PDFThumbnailViewer = null;
const SecondaryToolbar = null;

;// CONCATENATED MODULE: ./web/overlay_manager.js
class OverlayManager {
  #overlays = new WeakMap();
  #active = null;
  get active() {
    return this.#active;
  }
  async register(dialog, canForceClose = false) {
    if (typeof dialog !== "object") {
      throw new Error("Not enough parameters.");
    } else if (this.#overlays.has(dialog)) {
      throw new Error("The overlay is already registered.");
    }
    this.#overlays.set(dialog, {
      canForceClose
    });
    dialog.addEventListener("cancel", evt => {
      this.#active = null;
    });
  }
  async open(dialog) {
    if (!this.#overlays.has(dialog)) {
      throw new Error("The overlay does not exist.");
    } else if (this.#active) {
      if (this.#active === dialog) {
        throw new Error("The overlay is already active.");
      } else if (this.#overlays.get(dialog).canForceClose) {
        await this.close();
      } else {
        throw new Error("Another overlay is currently active.");
      }
    }
    this.#active = dialog;
    dialog.showModal();
  }
  async close(dialog = this.#active) {
    if (!this.#overlays.has(dialog)) {
      throw new Error("The overlay does not exist.");
    } else if (!this.#active) {
      throw new Error("The overlay is currently not active.");
    } else if (this.#active !== dialog) {
      throw new Error("Another overlay is currently active.");
    }
    dialog.close();
    this.#active = null;
  }
}

;// CONCATENATED MODULE: ./web/password_prompt.js

class PasswordPrompt {
  #activeCapability = null;
  #updateCallback = null;
  #reason = null;
  constructor(options, overlayManager, isViewerEmbedded = false) {
    this.dialog = options.dialog;
    this.label = options.label;
    this.input = options.input;
    this.submitButton = options.submitButton;
    this.cancelButton = options.cancelButton;
    this.overlayManager = overlayManager;
    this._isViewerEmbedded = isViewerEmbedded;
    this.submitButton.addEventListener("click", this.#verify.bind(this));
    this.cancelButton.addEventListener("click", this.close.bind(this));
    this.input.addEventListener("keydown", e => {
      if (e.keyCode === 13) {
        this.#verify();
      }
    });
    this.overlayManager.register(this.dialog, true);
    this.dialog.addEventListener("close", this.#cancel.bind(this));
  }
  async open() {
    if (this.#activeCapability) {
      await this.#activeCapability.promise;
    }
    this.#activeCapability = new PromiseCapability();
    try {
      await this.overlayManager.open(this.dialog);
    } catch (ex) {
      this.#activeCapability.resolve();
      throw ex;
    }
    const passwordIncorrect = this.#reason === PasswordResponses.INCORRECT_PASSWORD;
    if (!this._isViewerEmbedded || passwordIncorrect) {
      this.input.focus();
    }
    this.label.setAttribute("data-l10n-id", `pdfjs-password-${passwordIncorrect ? "invalid" : "label"}`);
  }
  async close() {
    if (this.overlayManager.active === this.dialog) {
      this.overlayManager.close(this.dialog);
    }
  }
  #verify() {
    const password = this.input.value;
    if (password?.length > 0) {
      this.#invokeCallback(password);
    }
  }
  #cancel() {
    this.#invokeCallback(new Error("PasswordPrompt cancelled."));
    this.#activeCapability.resolve();
  }
  #invokeCallback(password) {
    if (!this.#updateCallback) {
      return;
    }
    this.close();
    this.input.value = "";
    this.#updateCallback(password);
    this.#updateCallback = null;
  }
  async setUpdateCallback(updateCallback, reason) {
    if (this.#activeCapability) {
      await this.#activeCapability.promise;
    }
    this.#updateCallback = updateCallback;
    this.#reason = reason;
  }
}

;// CONCATENATED MODULE: ./web/pdf_find_utils.js
const CharacterType = {
  SPACE: 0,
  ALPHA_LETTER: 1,
  PUNCT: 2,
  HAN_LETTER: 3,
  KATAKANA_LETTER: 4,
  HIRAGANA_LETTER: 5,
  HALFWIDTH_KATAKANA_LETTER: 6,
  THAI_LETTER: 7
};
function isAlphabeticalScript(charCode) {
  return charCode < 0x2e80;
}
function isAscii(charCode) {
  return (charCode & 0xff80) === 0;
}
function isAsciiAlpha(charCode) {
  return charCode >= 0x61 && charCode <= 0x7a || charCode >= 0x41 && charCode <= 0x5a;
}
function isAsciiDigit(charCode) {
  return charCode >= 0x30 && charCode <= 0x39;
}
function isAsciiSpace(charCode) {
  return charCode === 0x20 || charCode === 0x09 || charCode === 0x0d || charCode === 0x0a;
}
function isHan(charCode) {
  return charCode >= 0x3400 && charCode <= 0x9fff || charCode >= 0xf900 && charCode <= 0xfaff;
}
function isKatakana(charCode) {
  return charCode >= 0x30a0 && charCode <= 0x30ff;
}
function isHiragana(charCode) {
  return charCode >= 0x3040 && charCode <= 0x309f;
}
function isHalfwidthKatakana(charCode) {
  return charCode >= 0xff60 && charCode <= 0xff9f;
}
function isThai(charCode) {
  return (charCode & 0xff80) === 0x0e00;
}
function getCharacterType(charCode) {
  if (isAlphabeticalScript(charCode)) {
    if (isAscii(charCode)) {
      if (isAsciiSpace(charCode)) {
        return CharacterType.SPACE;
      } else if (isAsciiAlpha(charCode) || isAsciiDigit(charCode) || charCode === 0x5f) {
        return CharacterType.ALPHA_LETTER;
      }
      return CharacterType.PUNCT;
    } else if (isThai(charCode)) {
      return CharacterType.THAI_LETTER;
    } else if (charCode === 0xa0) {
      return CharacterType.SPACE;
    }
    return CharacterType.ALPHA_LETTER;
  }
  if (isHan(charCode)) {
    return CharacterType.HAN_LETTER;
  } else if (isKatakana(charCode)) {
    return CharacterType.KATAKANA_LETTER;
  } else if (isHiragana(charCode)) {
    return CharacterType.HIRAGANA_LETTER;
  } else if (isHalfwidthKatakana(charCode)) {
    return CharacterType.HALFWIDTH_KATAKANA_LETTER;
  }
  return CharacterType.ALPHA_LETTER;
}
let NormalizeWithNFKC;
function getNormalizeWithNFKC() {
  NormalizeWithNFKC ||= ` ¨ª¯²-µ¸-º¼-¾Ĳ-ĳĿ-ŀŉſǄ-ǌǱ-ǳʰ-ʸ˘-˝ˠ-ˤʹͺ;΄-΅·ϐ-ϖϰ-ϲϴ-ϵϹևٵ-ٸक़-य़ড়-ঢ়য়ਲ਼ਸ਼ਖ਼-ਜ਼ਫ਼ଡ଼-ଢ଼ำຳໜ-ໝ༌གྷཌྷདྷབྷཛྷཀྵჼᴬ-ᴮᴰ-ᴺᴼ-ᵍᵏ-ᵪᵸᶛ-ᶿẚ-ẛάέήίόύώΆ᾽-῁ΈΉ῍-῏ΐΊ῝-῟ΰΎ῭-`ΌΏ´-῾ - ‑‗․-… ″-‴‶-‷‼‾⁇-⁉⁗ ⁰-ⁱ⁴-₎ₐ-ₜ₨℀-℃℅-ℇ℉-ℓℕ-№ℙ-ℝ℠-™ℤΩℨK-ℭℯ-ℱℳ-ℹ℻-⅀ⅅ-ⅉ⅐-ⅿ↉∬-∭∯-∰〈-〉①-⓪⨌⩴-⩶⫝̸ⱼ-ⱽⵯ⺟⻳⼀-⿕　〶〸-〺゛-゜ゟヿㄱ-ㆎ㆒-㆟㈀-㈞㈠-㉇㉐-㉾㊀-㏿ꚜ-ꚝꝰꟲ-ꟴꟸ-ꟹꭜ-ꭟꭩ豈-嗀塚晴凞-羽蘒諸逸-都飯-舘並-龎ﬀ-ﬆﬓ-ﬗיִײַ-זּטּ-לּמּנּ-סּףּ-פּצּ-ﮱﯓ-ﴽﵐ-ﶏﶒ-ﷇﷰ-﷼︐-︙︰-﹄﹇-﹒﹔-﹦﹨-﹫ﹰ-ﹲﹴﹶ-ﻼ！-ﾾￂ-ￇￊ-ￏￒ-ￗￚ-ￜ￠-￦`;
  return NormalizeWithNFKC;
}

;// CONCATENATED MODULE: ./web/pdf_find_controller.js



const FindState = {
  FOUND: 0,
  NOT_FOUND: 1,
  WRAPPED: 2,
  PENDING: 3
};
const FIND_TIMEOUT = 250;
const MATCH_SCROLL_OFFSET_TOP = -50;
const MATCH_SCROLL_OFFSET_LEFT = -400;
const CHARACTERS_TO_NORMALIZE = {
  "\u2010": "-",
  "\u2018": "'",
  "\u2019": "'",
  "\u201A": "'",
  "\u201B": "'",
  "\u201C": '"',
  "\u201D": '"',
  "\u201E": '"',
  "\u201F": '"',
  "\u00BC": "1/4",
  "\u00BD": "1/2",
  "\u00BE": "3/4"
};
const DIACRITICS_EXCEPTION = new Set([0x3099, 0x309a, 0x094d, 0x09cd, 0x0a4d, 0x0acd, 0x0b4d, 0x0bcd, 0x0c4d, 0x0ccd, 0x0d3b, 0x0d3c, 0x0d4d, 0x0dca, 0x0e3a, 0x0eba, 0x0f84, 0x1039, 0x103a, 0x1714, 0x1734, 0x17d2, 0x1a60, 0x1b44, 0x1baa, 0x1bab, 0x1bf2, 0x1bf3, 0x2d7f, 0xa806, 0xa82c, 0xa8c4, 0xa953, 0xa9c0, 0xaaf6, 0xabed, 0x0c56, 0x0f71, 0x0f72, 0x0f7a, 0x0f7b, 0x0f7c, 0x0f7d, 0x0f80, 0x0f74]);
let DIACRITICS_EXCEPTION_STR;
const DIACRITICS_REG_EXP = /\p{M}+/gu;
const SPECIAL_CHARS_REG_EXP = /([.*+?^${}()|[\]\\])|(\p{P})|(\s+)|(\p{M})|(\p{L})/gu;
const NOT_DIACRITIC_FROM_END_REG_EXP = /([^\p{M}])\p{M}*$/u;
const NOT_DIACRITIC_FROM_START_REG_EXP = /^\p{M}*([^\p{M}])/u;
const SYLLABLES_REG_EXP = /[\uAC00-\uD7AF\uFA6C\uFACF-\uFAD1\uFAD5-\uFAD7]+/g;
const SYLLABLES_LENGTHS = new Map();
const FIRST_CHAR_SYLLABLES_REG_EXP = "[\\u1100-\\u1112\\ud7a4-\\ud7af\\ud84a\\ud84c\\ud850\\ud854\\ud857\\ud85f]";
const NFKC_CHARS_TO_NORMALIZE = new Map();
let noSyllablesRegExp = null;
let withSyllablesRegExp = null;
function normalize(text) {
  const syllablePositions = [];
  let m;
  while ((m = SYLLABLES_REG_EXP.exec(text)) !== null) {
    let {
      index
    } = m;
    for (const char of m[0]) {
      let len = SYLLABLES_LENGTHS.get(char);
      if (!len) {
        len = char.normalize("NFD").length;
        SYLLABLES_LENGTHS.set(char, len);
      }
      syllablePositions.push([len, index++]);
    }
  }
  let normalizationRegex;
  if (syllablePositions.length === 0 && noSyllablesRegExp) {
    normalizationRegex = noSyllablesRegExp;
  } else if (syllablePositions.length > 0 && withSyllablesRegExp) {
    normalizationRegex = withSyllablesRegExp;
  } else {
    const replace = Object.keys(CHARACTERS_TO_NORMALIZE).join("");
    const toNormalizeWithNFKC = getNormalizeWithNFKC();
    const CJK = "(?:\\p{Ideographic}|[\u3040-\u30FF])";
    const HKDiacritics = "(?:\u3099|\u309A)";
    const regexp = `([${replace}])|([${toNormalizeWithNFKC}])|(${HKDiacritics}\\n)|(\\p{M}+(?:-\\n)?)|(\\S-\\n)|(${CJK}\\n)|(\\n)`;
    if (syllablePositions.length === 0) {
      normalizationRegex = noSyllablesRegExp = new RegExp(regexp + "|(\\u0000)", "gum");
    } else {
      normalizationRegex = withSyllablesRegExp = new RegExp(regexp + `|(${FIRST_CHAR_SYLLABLES_REG_EXP})`, "gum");
    }
  }
  const rawDiacriticsPositions = [];
  while ((m = DIACRITICS_REG_EXP.exec(text)) !== null) {
    rawDiacriticsPositions.push([m[0].length, m.index]);
  }
  let normalized = text.normalize("NFD");
  const positions = [[0, 0]];
  let rawDiacriticsIndex = 0;
  let syllableIndex = 0;
  let shift = 0;
  let shiftOrigin = 0;
  let eol = 0;
  let hasDiacritics = false;
  normalized = normalized.replace(normalizationRegex, (match, p1, p2, p3, p4, p5, p6, p7, p8, i) => {
    i -= shiftOrigin;
    if (p1) {
      const replacement = CHARACTERS_TO_NORMALIZE[p1];
      const jj = replacement.length;
      for (let j = 1; j < jj; j++) {
        positions.push([i - shift + j, shift - j]);
      }
      shift -= jj - 1;
      return replacement;
    }
    if (p2) {
      let replacement = NFKC_CHARS_TO_NORMALIZE.get(p2);
      if (!replacement) {
        replacement = p2.normalize("NFKC");
        NFKC_CHARS_TO_NORMALIZE.set(p2, replacement);
      }
      const jj = replacement.length;
      for (let j = 1; j < jj; j++) {
        positions.push([i - shift + j, shift - j]);
      }
      shift -= jj - 1;
      return replacement;
    }
    if (p3) {
      hasDiacritics = true;
      if (i + eol === rawDiacriticsPositions[rawDiacriticsIndex]?.[1]) {
        ++rawDiacriticsIndex;
      } else {
        positions.push([i - 1 - shift + 1, shift - 1]);
        shift -= 1;
        shiftOrigin += 1;
      }
      positions.push([i - shift + 1, shift]);
      shiftOrigin += 1;
      eol += 1;
      return p3.charAt(0);
    }
    if (p4) {
      const hasTrailingDashEOL = p4.endsWith("\n");
      const len = hasTrailingDashEOL ? p4.length - 2 : p4.length;
      hasDiacritics = true;
      let jj = len;
      if (i + eol === rawDiacriticsPositions[rawDiacriticsIndex]?.[1]) {
        jj -= rawDiacriticsPositions[rawDiacriticsIndex][0];
        ++rawDiacriticsIndex;
      }
      for (let j = 1; j <= jj; j++) {
        positions.push([i - 1 - shift + j, shift - j]);
      }
      shift -= jj;
      shiftOrigin += jj;
      if (hasTrailingDashEOL) {
        i += len - 1;
        positions.push([i - shift + 1, 1 + shift]);
        shift += 1;
        shiftOrigin += 1;
        eol += 1;
        return p4.slice(0, len);
      }
      return p4;
    }
    if (p5) {
      const len = p5.length - 2;
      positions.push([i - shift + len, 1 + shift]);
      shift += 1;
      shiftOrigin += 1;
      eol += 1;
      return p5.slice(0, -2);
    }
    if (p6) {
      const len = p6.length - 1;
      positions.push([i - shift + len, shift]);
      shiftOrigin += 1;
      eol += 1;
      return p6.slice(0, -1);
    }
    if (p7) {
      positions.push([i - shift + 1, shift - 1]);
      shift -= 1;
      shiftOrigin += 1;
      eol += 1;
      return " ";
    }
    if (i + eol === syllablePositions[syllableIndex]?.[1]) {
      const newCharLen = syllablePositions[syllableIndex][0] - 1;
      ++syllableIndex;
      for (let j = 1; j <= newCharLen; j++) {
        positions.push([i - (shift - j), shift - j]);
      }
      shift -= newCharLen;
      shiftOrigin += newCharLen;
    }
    return p8;
  });
  positions.push([normalized.length, shift]);
  return [normalized, positions, hasDiacritics];
}
function getOriginalIndex(diffs, pos, len) {
  if (!diffs) {
    return [pos, len];
  }
  const start = pos;
  const end = pos + len - 1;
  let i = binarySearchFirstItem(diffs, x => x[0] >= start);
  if (diffs[i][0] > start) {
    --i;
  }
  let j = binarySearchFirstItem(diffs, x => x[0] >= end, i);
  if (diffs[j][0] > end) {
    --j;
  }
  const oldStart = start + diffs[i][1];
  const oldEnd = end + diffs[j][1];
  const oldLen = oldEnd + 1 - oldStart;
  return [oldStart, oldLen];
}
class PDFFindController {
  #state = null;
  #updateMatchesCountOnProgress = true;
  #visitedPagesCount = 0;
  constructor({
    linkService,
    eventBus,
    updateMatchesCountOnProgress = true
  }) {
    this._linkService = linkService;
    this._eventBus = eventBus;
    this.#updateMatchesCountOnProgress = updateMatchesCountOnProgress;
    this.onIsPageVisible = null;
    this.#reset();
    eventBus._on("find", this.#onFind.bind(this));
    eventBus._on("findbarclose", this.#onFindBarClose.bind(this));
  }
  get highlightMatches() {
    return this._highlightMatches;
  }
  get pageMatches() {
    return this._pageMatches;
  }
  get pageMatchesLength() {
    return this._pageMatchesLength;
  }
  get selected() {
    return this._selected;
  }
  get state() {
    return this.#state;
  }
  setDocument(pdfDocument) {
    if (this._pdfDocument) {
      this.#reset();
    }
    if (!pdfDocument) {
      return;
    }
    this._pdfDocument = pdfDocument;
    this._firstPageCapability.resolve();
  }
  #onFind(state) {
    if (!state) {
      return;
    }
    const pdfDocument = this._pdfDocument;
    const {
      type
    } = state;
    if (this.#state === null || this.#shouldDirtyMatch(state)) {
      this._dirtyMatch = true;
    }
    this.#state = state;
    if (type !== "highlightallchange") {
      this.#updateUIState(FindState.PENDING);
    }
    this._firstPageCapability.promise.then(() => {
      if (!this._pdfDocument || pdfDocument && this._pdfDocument !== pdfDocument) {
        return;
      }
      this.#extractText();
      const findbarClosed = !this._highlightMatches;
      const pendingTimeout = !!this._findTimeout;
      if (this._findTimeout) {
        clearTimeout(this._findTimeout);
        this._findTimeout = null;
      }
      if (!type) {
        this._findTimeout = setTimeout(() => {
          this.#nextMatch();
          this._findTimeout = null;
        }, FIND_TIMEOUT);
      } else if (this._dirtyMatch) {
        this.#nextMatch();
      } else if (type === "again") {
        this.#nextMatch();
        if (findbarClosed && this.#state.highlightAll) {
          this.#updateAllPages();
        }
      } else if (type === "highlightallchange") {
        if (pendingTimeout) {
          this.#nextMatch();
        } else {
          this._highlightMatches = true;
        }
        this.#updateAllPages();
      } else {
        this.#nextMatch();
      }
    });
  }
  scrollMatchIntoView({
    element = null,
    selectedLeft = 0,
    pageIndex = -1,
    matchIndex = -1
  }) {
    if (!this._scrollMatches || !element) {
      return;
    } else if (matchIndex === -1 || matchIndex !== this._selected.matchIdx) {
      return;
    } else if (pageIndex === -1 || pageIndex !== this._selected.pageIdx) {
      return;
    }
    this._scrollMatches = false;
    const spot = {
      top: MATCH_SCROLL_OFFSET_TOP,
      left: selectedLeft + MATCH_SCROLL_OFFSET_LEFT
    };
    scrollIntoView(element, spot, true);
  }
  #reset() {
    this._highlightMatches = false;
    this._scrollMatches = false;
    this._pdfDocument = null;
    this._pageMatches = [];
    this._pageMatchesLength = [];
    this.#visitedPagesCount = 0;
    this.#state = null;
    this._selected = {
      pageIdx: -1,
      matchIdx: -1
    };
    this._offset = {
      pageIdx: null,
      matchIdx: null,
      wrapped: false
    };
    this._extractTextPromises = [];
    this._pageContents = [];
    this._pageDiffs = [];
    this._hasDiacritics = [];
    this._matchesCountTotal = 0;
    this._pagesToSearch = null;
    this._pendingFindMatches = new Set();
    this._resumePageIdx = null;
    this._dirtyMatch = false;
    clearTimeout(this._findTimeout);
    this._findTimeout = null;
    this._firstPageCapability = new PromiseCapability();
  }
  get #query() {
    const {
      query
    } = this.#state;
    if (typeof query === "string") {
      if (query !== this._rawQuery) {
        this._rawQuery = query;
        [this._normalizedQuery] = normalize(query);
      }
      return this._normalizedQuery;
    }
    return (query || []).filter(q => !!q).map(q => normalize(q)[0]);
  }
  #shouldDirtyMatch(state) {
    const newQuery = state.query,
      prevQuery = this.#state.query;
    const newType = typeof newQuery,
      prevType = typeof prevQuery;
    if (newType !== prevType) {
      return true;
    }
    if (newType === "string") {
      if (newQuery !== prevQuery) {
        return true;
      }
    } else if (JSON.stringify(newQuery) !== JSON.stringify(prevQuery)) {
      return true;
    }
    switch (state.type) {
      case "again":
        const pageNumber = this._selected.pageIdx + 1;
        const linkService = this._linkService;
        return pageNumber >= 1 && pageNumber <= linkService.pagesCount && pageNumber !== linkService.page && !(this.onIsPageVisible?.(pageNumber) ?? true);
      case "highlightallchange":
        return false;
    }
    return true;
  }
  #isEntireWord(content, startIdx, length) {
    let match = content.slice(0, startIdx).match(NOT_DIACRITIC_FROM_END_REG_EXP);
    if (match) {
      const first = content.charCodeAt(startIdx);
      const limit = match[1].charCodeAt(0);
      if (getCharacterType(first) === getCharacterType(limit)) {
        return false;
      }
    }
    match = content.slice(startIdx + length).match(NOT_DIACRITIC_FROM_START_REG_EXP);
    if (match) {
      const last = content.charCodeAt(startIdx + length - 1);
      const limit = match[1].charCodeAt(0);
      if (getCharacterType(last) === getCharacterType(limit)) {
        return false;
      }
    }
    return true;
  }
  #calculateRegExpMatch(query, entireWord, pageIndex, pageContent) {
    const matches = this._pageMatches[pageIndex] = [];
    const matchesLength = this._pageMatchesLength[pageIndex] = [];
    if (!query) {
      return;
    }
    const diffs = this._pageDiffs[pageIndex];
    let match;
    while ((match = query.exec(pageContent)) !== null) {
      if (entireWord && !this.#isEntireWord(pageContent, match.index, match[0].length)) {
        continue;
      }
      const [matchPos, matchLen] = getOriginalIndex(diffs, match.index, match[0].length);
      if (matchLen) {
        matches.push(matchPos);
        matchesLength.push(matchLen);
      }
    }
  }
  #convertToRegExpString(query, hasDiacritics) {
    const {
      matchDiacritics
    } = this.#state;
    let isUnicode = false;
    query = query.replaceAll(SPECIAL_CHARS_REG_EXP, (match, p1, p2, p3, p4, p5) => {
      if (p1) {
        return `[ ]*\\${p1}[ ]*`;
      }
      if (p2) {
        return `[ ]*${p2}[ ]*`;
      }
      if (p3) {
        return "[ ]+";
      }
      if (matchDiacritics) {
        return p4 || p5;
      }
      if (p4) {
        return DIACRITICS_EXCEPTION.has(p4.charCodeAt(0)) ? p4 : "";
      }
      if (hasDiacritics) {
        isUnicode = true;
        return `${p5}\\p{M}*`;
      }
      return p5;
    });
    const trailingSpaces = "[ ]*";
    if (query.endsWith(trailingSpaces)) {
      query = query.slice(0, query.length - trailingSpaces.length);
    }
    if (matchDiacritics) {
      if (hasDiacritics) {
        DIACRITICS_EXCEPTION_STR ||= String.fromCharCode(...DIACRITICS_EXCEPTION);
        isUnicode = true;
        query = `${query}(?=[${DIACRITICS_EXCEPTION_STR}]|[^\\p{M}]|$)`;
      }
    }
    return [isUnicode, query];
  }
  #calculateMatch(pageIndex) {
    let query = this.#query;
    if (query.length === 0) {
      return;
    }
    const {
      caseSensitive,
      entireWord
    } = this.#state;
    const pageContent = this._pageContents[pageIndex];
    const hasDiacritics = this._hasDiacritics[pageIndex];
    let isUnicode = false;
    if (typeof query === "string") {
      [isUnicode, query] = this.#convertToRegExpString(query, hasDiacritics);
    } else {
      query = query.sort().reverse().map(q => {
        const [isUnicodePart, queryPart] = this.#convertToRegExpString(q, hasDiacritics);
        isUnicode ||= isUnicodePart;
        return `(${queryPart})`;
      }).join("|");
    }
    const flags = `g${isUnicode ? "u" : ""}${caseSensitive ? "" : "i"}`;
    query = query ? new RegExp(query, flags) : null;
    this.#calculateRegExpMatch(query, entireWord, pageIndex, pageContent);
    if (this.#state.highlightAll) {
      this.#updatePage(pageIndex);
    }
    if (this._resumePageIdx === pageIndex) {
      this._resumePageIdx = null;
      this.#nextPageMatch();
    }
    const pageMatchesCount = this._pageMatches[pageIndex].length;
    this._matchesCountTotal += pageMatchesCount;
    if (this.#updateMatchesCountOnProgress) {
      if (pageMatchesCount > 0) {
        this.#updateUIResultsCount();
      }
    } else if (++this.#visitedPagesCount === this._linkService.pagesCount) {
      this.#updateUIResultsCount();
    }
  }
  #extractText() {
    if (this._extractTextPromises.length > 0) {
      return;
    }
    let promise = Promise.resolve();
    const textOptions = {
      disableNormalization: true
    };
    for (let i = 0, ii = this._linkService.pagesCount; i < ii; i++) {
      const extractTextCapability = new PromiseCapability();
      this._extractTextPromises[i] = extractTextCapability.promise;
      promise = promise.then(() => {
        return this._pdfDocument.getPage(i + 1).then(pdfPage => {
          return pdfPage.getTextContent(textOptions);
        }).then(textContent => {
          const strBuf = [];
          for (const textItem of textContent.items) {
            strBuf.push(textItem.str);
            if (textItem.hasEOL) {
              strBuf.push("\n");
            }
          }
          [this._pageContents[i], this._pageDiffs[i], this._hasDiacritics[i]] = normalize(strBuf.join(""));
          extractTextCapability.resolve();
        }, reason => {
          console.error(`Unable to get text content for page ${i + 1}`, reason);
          this._pageContents[i] = "";
          this._pageDiffs[i] = null;
          this._hasDiacritics[i] = false;
          extractTextCapability.resolve();
        });
      });
    }
  }
  #updatePage(index) {
    if (this._scrollMatches && this._selected.pageIdx === index) {
      this._linkService.page = index + 1;
    }
    this._eventBus.dispatch("updatetextlayermatches", {
      source: this,
      pageIndex: index
    });
  }
  #updateAllPages() {
    this._eventBus.dispatch("updatetextlayermatches", {
      source: this,
      pageIndex: -1
    });
  }
  #nextMatch() {
    const previous = this.#state.findPrevious;
    const currentPageIndex = this._linkService.page - 1;
    const numPages = this._linkService.pagesCount;
    this._highlightMatches = true;
    if (this._dirtyMatch) {
      this._dirtyMatch = false;
      this._selected.pageIdx = this._selected.matchIdx = -1;
      this._offset.pageIdx = currentPageIndex;
      this._offset.matchIdx = null;
      this._offset.wrapped = false;
      this._resumePageIdx = null;
      this._pageMatches.length = 0;
      this._pageMatchesLength.length = 0;
      this.#visitedPagesCount = 0;
      this._matchesCountTotal = 0;
      this.#updateAllPages();
      for (let i = 0; i < numPages; i++) {
        if (this._pendingFindMatches.has(i)) {
          continue;
        }
        this._pendingFindMatches.add(i);
        this._extractTextPromises[i].then(() => {
          this._pendingFindMatches.delete(i);
          this.#calculateMatch(i);
        });
      }
    }
    const query = this.#query;
    if (query.length === 0) {
      this.#updateUIState(FindState.FOUND);
      return;
    }
    if (this._resumePageIdx) {
      return;
    }
    const offset = this._offset;
    this._pagesToSearch = numPages;
    if (offset.matchIdx !== null) {
      const numPageMatches = this._pageMatches[offset.pageIdx].length;
      if (!previous && offset.matchIdx + 1 < numPageMatches || previous && offset.matchIdx > 0) {
        offset.matchIdx = previous ? offset.matchIdx - 1 : offset.matchIdx + 1;
        this.#updateMatch(true);
        return;
      }
      this.#advanceOffsetPage(previous);
    }
    this.#nextPageMatch();
  }
  #matchesReady(matches) {
    const offset = this._offset;
    const numMatches = matches.length;
    const previous = this.#state.findPrevious;
    if (numMatches) {
      offset.matchIdx = previous ? numMatches - 1 : 0;
      this.#updateMatch(true);
      return true;
    }
    this.#advanceOffsetPage(previous);
    if (offset.wrapped) {
      offset.matchIdx = null;
      if (this._pagesToSearch < 0) {
        this.#updateMatch(false);
        return true;
      }
    }
    return false;
  }
  #nextPageMatch() {
    if (this._resumePageIdx !== null) {
      console.error("There can only be one pending page.");
    }
    let matches = null;
    do {
      const pageIdx = this._offset.pageIdx;
      matches = this._pageMatches[pageIdx];
      if (!matches) {
        this._resumePageIdx = pageIdx;
        break;
      }
    } while (!this.#matchesReady(matches));
  }
  #advanceOffsetPage(previous) {
    const offset = this._offset;
    const numPages = this._linkService.pagesCount;
    offset.pageIdx = previous ? offset.pageIdx - 1 : offset.pageIdx + 1;
    offset.matchIdx = null;
    this._pagesToSearch--;
    if (offset.pageIdx >= numPages || offset.pageIdx < 0) {
      offset.pageIdx = previous ? numPages - 1 : 0;
      offset.wrapped = true;
    }
  }
  #updateMatch(found = false) {
    let state = FindState.NOT_FOUND;
    const wrapped = this._offset.wrapped;
    this._offset.wrapped = false;
    if (found) {
      const previousPage = this._selected.pageIdx;
      this._selected.pageIdx = this._offset.pageIdx;
      this._selected.matchIdx = this._offset.matchIdx;
      state = wrapped ? FindState.WRAPPED : FindState.FOUND;
      if (previousPage !== -1 && previousPage !== this._selected.pageIdx) {
        this.#updatePage(previousPage);
      }
    }
    this.#updateUIState(state, this.#state.findPrevious);
    if (this._selected.pageIdx !== -1) {
      this._scrollMatches = true;
      this.#updatePage(this._selected.pageIdx);
    }
  }
  #onFindBarClose(evt) {
    const pdfDocument = this._pdfDocument;
    this._firstPageCapability.promise.then(() => {
      if (!this._pdfDocument || pdfDocument && this._pdfDocument !== pdfDocument) {
        return;
      }
      if (this._findTimeout) {
        clearTimeout(this._findTimeout);
        this._findTimeout = null;
      }
      if (this._resumePageIdx) {
        this._resumePageIdx = null;
        this._dirtyMatch = true;
      }
      this.#updateUIState(FindState.FOUND);
      this._highlightMatches = false;
      this.#updateAllPages();
    });
  }
  #requestMatchesCount() {
    const {
      pageIdx,
      matchIdx
    } = this._selected;
    let current = 0,
      total = this._matchesCountTotal;
    if (matchIdx !== -1) {
      for (let i = 0; i < pageIdx; i++) {
        current += this._pageMatches[i]?.length || 0;
      }
      current += matchIdx + 1;
    }
    if (current < 1 || current > total) {
      current = total = 0;
    }
    return {
      current,
      total
    };
  }
  #updateUIResultsCount() {
    this._eventBus.dispatch("updatefindmatchescount", {
      source: this,
      matchesCount: this.#requestMatchesCount()
    });
  }
  #updateUIState(state, previous = false) {
    if (!this.#updateMatchesCountOnProgress && (this.#visitedPagesCount !== this._linkService.pagesCount || state === FindState.PENDING)) {
      return;
    }
    this._eventBus.dispatch("updatefindcontrolstate", {
      source: this,
      state,
      previous,
      matchesCount: this.#requestMatchesCount(),
      rawQuery: this.#state?.query ?? null
    });
  }
}

;// CONCATENATED MODULE: ./web/pdf_history.js


const HASH_CHANGE_TIMEOUT = 1000;
const POSITION_UPDATED_THRESHOLD = 50;
const UPDATE_VIEWAREA_TIMEOUT = 1000;
function getCurrentHash() {
  return document.location.hash;
}
class PDFHistory {
  constructor({
    linkService,
    eventBus
  }) {
    this.linkService = linkService;
    this.eventBus = eventBus;
    this._initialized = false;
    this._fingerprint = "";
    this.reset();
    this._boundEvents = null;
    this.eventBus._on("pagesinit", () => {
      this._isPagesLoaded = false;
      this.eventBus._on("pagesloaded", evt => {
        this._isPagesLoaded = !!evt.pagesCount;
      }, {
        once: true
      });
    });
  }
  initialize({
    fingerprint,
    resetHistory = false,
    updateUrl = false
  }) {
    if (!fingerprint || typeof fingerprint !== "string") {
      console.error('PDFHistory.initialize: The "fingerprint" must be a non-empty string.');
      return;
    }
    if (this._initialized) {
      this.reset();
    }
    const reInitialized = this._fingerprint !== "" && this._fingerprint !== fingerprint;
    this._fingerprint = fingerprint;
    this._updateUrl = updateUrl === true;
    this._initialized = true;
    this._bindEvents();
    const state = window.history.state;
    this._popStateInProgress = false;
    this._blockHashChange = 0;
    this._currentHash = getCurrentHash();
    this._numPositionUpdates = 0;
    this._uid = this._maxUid = 0;
    this._destination = null;
    this._position = null;
    if (!this._isValidState(state, true) || resetHistory) {
      const {
        hash,
        page,
        rotation
      } = this._parseCurrentHash(true);
      if (!hash || reInitialized || resetHistory) {
        this._pushOrReplaceState(null, true);
        return;
      }
      this._pushOrReplaceState({
        hash,
        page,
        rotation
      }, true);
      return;
    }
    const destination = state.destination;
    this._updateInternalState(destination, state.uid, true);
    if (destination.rotation !== undefined) {
      this._initialRotation = destination.rotation;
    }
    if (destination.dest) {
      this._initialBookmark = JSON.stringify(destination.dest);
      this._destination.page = null;
    } else if (destination.hash) {
      this._initialBookmark = destination.hash;
    } else if (destination.page) {
      this._initialBookmark = `page=${destination.page}`;
    }
  }
  reset() {
    if (this._initialized) {
      this._pageHide();
      this._initialized = false;
      this._unbindEvents();
    }
    if (this._updateViewareaTimeout) {
      clearTimeout(this._updateViewareaTimeout);
      this._updateViewareaTimeout = null;
    }
    this._initialBookmark = null;
    this._initialRotation = null;
  }
  push({
    namedDest = null,
    explicitDest,
    pageNumber
  }) {
    if (!this._initialized) {
      return;
    }
    if (namedDest && typeof namedDest !== "string") {
      console.error("PDFHistory.push: " + `"${namedDest}" is not a valid namedDest parameter.`);
      return;
    } else if (!Array.isArray(explicitDest)) {
      console.error("PDFHistory.push: " + `"${explicitDest}" is not a valid explicitDest parameter.`);
      return;
    } else if (!this._isValidPage(pageNumber)) {
      if (pageNumber !== null || this._destination) {
        console.error("PDFHistory.push: " + `"${pageNumber}" is not a valid pageNumber parameter.`);
        return;
      }
    }
    const hash = namedDest || JSON.stringify(explicitDest);
    if (!hash) {
      return;
    }
    let forceReplace = false;
    if (this._destination && (isDestHashesEqual(this._destination.hash, hash) || isDestArraysEqual(this._destination.dest, explicitDest))) {
      if (this._destination.page) {
        return;
      }
      forceReplace = true;
    }
    if (this._popStateInProgress && !forceReplace) {
      return;
    }
    this._pushOrReplaceState({
      dest: explicitDest,
      hash,
      page: pageNumber,
      rotation: this.linkService.rotation
    }, forceReplace);
    if (!this._popStateInProgress) {
      this._popStateInProgress = true;
      Promise.resolve().then(() => {
        this._popStateInProgress = false;
      });
    }
  }
  pushPage(pageNumber) {
    if (!this._initialized) {
      return;
    }
    if (!this._isValidPage(pageNumber)) {
      console.error(`PDFHistory.pushPage: "${pageNumber}" is not a valid page number.`);
      return;
    }
    if (this._destination?.page === pageNumber) {
      return;
    }
    if (this._popStateInProgress) {
      return;
    }
    this._pushOrReplaceState({
      dest: null,
      hash: `page=${pageNumber}`,
      page: pageNumber,
      rotation: this.linkService.rotation
    });
    if (!this._popStateInProgress) {
      this._popStateInProgress = true;
      Promise.resolve().then(() => {
        this._popStateInProgress = false;
      });
    }
  }
  pushCurrentPosition() {
    if (!this._initialized || this._popStateInProgress) {
      return;
    }
    this._tryPushCurrentPosition();
  }
  back() {
    if (!this._initialized || this._popStateInProgress) {
      return;
    }
    const state = window.history.state;
    if (this._isValidState(state) && state.uid > 0) {
      window.history.back();
    }
  }
  forward() {
    if (!this._initialized || this._popStateInProgress) {
      return;
    }
    const state = window.history.state;
    if (this._isValidState(state) && state.uid < this._maxUid) {
      window.history.forward();
    }
  }
  get popStateInProgress() {
    return this._initialized && (this._popStateInProgress || this._blockHashChange > 0);
  }
  get initialBookmark() {
    return this._initialized ? this._initialBookmark : null;
  }
  get initialRotation() {
    return this._initialized ? this._initialRotation : null;
  }
  _pushOrReplaceState(destination, forceReplace = false) {
    const shouldReplace = forceReplace || !this._destination;
    const newState = {
      fingerprint: this._fingerprint,
      uid: shouldReplace ? this._uid : this._uid + 1,
      destination
    };
    this._updateInternalState(destination, newState.uid);
    let newUrl;
    if (this._updateUrl && destination?.hash) {
      const baseUrl = document.location.href.split("#")[0];
      if (!baseUrl.startsWith("file://")) {
        newUrl = `${baseUrl}#${destination.hash}`;
      }
    }
    if (shouldReplace) {
      window.history.replaceState(newState, "", newUrl);
    } else {
      window.history.pushState(newState, "", newUrl);
    }
  }
  _tryPushCurrentPosition(temporary = false) {
    if (!this._position) {
      return;
    }
    let position = this._position;
    if (temporary) {
      position = Object.assign(Object.create(null), this._position);
      position.temporary = true;
    }
    if (!this._destination) {
      this._pushOrReplaceState(position);
      return;
    }
    if (this._destination.temporary) {
      this._pushOrReplaceState(position, true);
      return;
    }
    if (this._destination.hash === position.hash) {
      return;
    }
    if (!this._destination.page && (POSITION_UPDATED_THRESHOLD <= 0 || this._numPositionUpdates <= POSITION_UPDATED_THRESHOLD)) {
      return;
    }
    let forceReplace = false;
    if (this._destination.page >= position.first && this._destination.page <= position.page) {
      if (this._destination.dest !== undefined || !this._destination.first) {
        return;
      }
      forceReplace = true;
    }
    this._pushOrReplaceState(position, forceReplace);
  }
  _isValidPage(val) {
    return Number.isInteger(val) && val > 0 && val <= this.linkService.pagesCount;
  }
  _isValidState(state, checkReload = false) {
    if (!state) {
      return false;
    }
    if (state.fingerprint !== this._fingerprint) {
      if (checkReload) {
        if (typeof state.fingerprint !== "string" || state.fingerprint.length !== this._fingerprint.length) {
          return false;
        }
        const [perfEntry] = performance.getEntriesByType("navigation");
        if (perfEntry?.type !== "reload") {
          return false;
        }
      } else {
        return false;
      }
    }
    if (!Number.isInteger(state.uid) || state.uid < 0) {
      return false;
    }
    if (state.destination === null || typeof state.destination !== "object") {
      return false;
    }
    return true;
  }
  _updateInternalState(destination, uid, removeTemporary = false) {
    if (this._updateViewareaTimeout) {
      clearTimeout(this._updateViewareaTimeout);
      this._updateViewareaTimeout = null;
    }
    if (removeTemporary && destination?.temporary) {
      delete destination.temporary;
    }
    this._destination = destination;
    this._uid = uid;
    this._maxUid = Math.max(this._maxUid, uid);
    this._numPositionUpdates = 0;
  }
  _parseCurrentHash(checkNameddest = false) {
    const hash = unescape(getCurrentHash()).substring(1);
    const params = parseQueryString(hash);
    const nameddest = params.get("nameddest") || "";
    let page = params.get("page") | 0;
    if (!this._isValidPage(page) || checkNameddest && nameddest.length > 0) {
      page = null;
    }
    return {
      hash,
      page,
      rotation: this.linkService.rotation
    };
  }
  _updateViewarea({
    location
  }) {
    if (this._updateViewareaTimeout) {
      clearTimeout(this._updateViewareaTimeout);
      this._updateViewareaTimeout = null;
    }
    this._position = {
      hash: location.pdfOpenParams.substring(1),
      page: this.linkService.page,
      first: location.pageNumber,
      rotation: location.rotation
    };
    if (this._popStateInProgress) {
      return;
    }
    if (POSITION_UPDATED_THRESHOLD > 0 && this._isPagesLoaded && this._destination && !this._destination.page) {
      this._numPositionUpdates++;
    }
    if (UPDATE_VIEWAREA_TIMEOUT > 0) {
      this._updateViewareaTimeout = setTimeout(() => {
        if (!this._popStateInProgress) {
          this._tryPushCurrentPosition(true);
        }
        this._updateViewareaTimeout = null;
      }, UPDATE_VIEWAREA_TIMEOUT);
    }
  }
  _popState({
    state
  }) {
    const newHash = getCurrentHash(),
      hashChanged = this._currentHash !== newHash;
    this._currentHash = newHash;
    if (!state) {
      this._uid++;
      const {
        hash,
        page,
        rotation
      } = this._parseCurrentHash();
      this._pushOrReplaceState({
        hash,
        page,
        rotation
      }, true);
      return;
    }
    if (!this._isValidState(state)) {
      return;
    }
    this._popStateInProgress = true;
    if (hashChanged) {
      this._blockHashChange++;
      waitOnEventOrTimeout({
        target: window,
        name: "hashchange",
        delay: HASH_CHANGE_TIMEOUT
      }).then(() => {
        this._blockHashChange--;
      });
    }
    const destination = state.destination;
    this._updateInternalState(destination, state.uid, true);
    if (isValidRotation(destination.rotation)) {
      this.linkService.rotation = destination.rotation;
    }
    if (destination.dest) {
      this.linkService.goToDestination(destination.dest);
    } else if (destination.hash) {
      this.linkService.setHash(destination.hash);
    } else if (destination.page) {
      this.linkService.page = destination.page;
    }
    Promise.resolve().then(() => {
      this._popStateInProgress = false;
    });
  }
  _pageHide() {
    if (!this._destination || this._destination.temporary) {
      this._tryPushCurrentPosition();
    }
  }
  _bindEvents() {
    if (this._boundEvents) {
      return;
    }
    this._boundEvents = {
      updateViewarea: this._updateViewarea.bind(this),
      popState: this._popState.bind(this),
      pageHide: this._pageHide.bind(this)
    };
    this.eventBus._on("updateviewarea", this._boundEvents.updateViewarea);
    window.addEventListener("popstate", this._boundEvents.popState);
    window.addEventListener("pagehide", this._boundEvents.pageHide);
  }
  _unbindEvents() {
    if (!this._boundEvents) {
      return;
    }
    this.eventBus._off("updateviewarea", this._boundEvents.updateViewarea);
    window.removeEventListener("popstate", this._boundEvents.popState);
    window.removeEventListener("pagehide", this._boundEvents.pageHide);
    this._boundEvents = null;
  }
}
function isDestHashesEqual(destHash, pushHash) {
  if (typeof destHash !== "string" || typeof pushHash !== "string") {
    return false;
  }
  if (destHash === pushHash) {
    return true;
  }
  const nameddest = parseQueryString(destHash).get("nameddest");
  if (nameddest === pushHash) {
    return true;
  }
  return false;
}
function isDestArraysEqual(firstDest, secondDest) {
  function isEntryEqual(first, second) {
    if (typeof first !== typeof second) {
      return false;
    }
    if (Array.isArray(first) || Array.isArray(second)) {
      return false;
    }
    if (first !== null && typeof first === "object" && second !== null) {
      if (Object.keys(first).length !== Object.keys(second).length) {
        return false;
      }
      for (const key in first) {
        if (!isEntryEqual(first[key], second[key])) {
          return false;
        }
      }
      return true;
    }
    return first === second || Number.isNaN(first) && Number.isNaN(second);
  }
  if (!(Array.isArray(firstDest) && Array.isArray(secondDest))) {
    return false;
  }
  if (firstDest.length !== secondDest.length) {
    return false;
  }
  for (let i = 0, ii = firstDest.length; i < ii; i++) {
    if (!isEntryEqual(firstDest[i], secondDest[i])) {
      return false;
    }
  }
  return true;
}

;// CONCATENATED MODULE: ./web/pdf_rendering_queue.js


const CLEANUP_TIMEOUT = 30000;
class PDFRenderingQueue {
  constructor() {
    this.pdfViewer = null;
    this.pdfThumbnailViewer = null;
    this.onIdle = null;
    this.highestPriorityPage = null;
    this.idleTimeout = null;
    this.printing = false;
    this.isThumbnailViewEnabled = false;
  }
  setViewer(pdfViewer) {
    this.pdfViewer = pdfViewer;
  }
  setThumbnailViewer(pdfThumbnailViewer) {
    this.pdfThumbnailViewer = pdfThumbnailViewer;
  }
  isHighestPriority(view) {
    return this.highestPriorityPage === view.renderingId;
  }
  renderHighestPriority(currentlyVisiblePages) {
    if (this.idleTimeout) {
      clearTimeout(this.idleTimeout);
      this.idleTimeout = null;
    }
    if (this.pdfViewer.forceRendering(currentlyVisiblePages)) {
      return;
    }
    if (this.isThumbnailViewEnabled && this.pdfThumbnailViewer?.forceRendering()) {
      return;
    }
    if (this.printing) {
      return;
    }
    if (this.onIdle) {
      this.idleTimeout = setTimeout(this.onIdle.bind(this), CLEANUP_TIMEOUT);
    }
  }
  getHighestPriority(visible, views, scrolledDown, preRenderExtra = false) {
    const visibleViews = visible.views,
      numVisible = visibleViews.length;
    if (numVisible === 0) {
      return null;
    }
    for (let i = 0; i < numVisible; i++) {
      const view = visibleViews[i].view;
      if (!this.isViewFinished(view)) {
        return view;
      }
    }
    const firstId = visible.first.id,
      lastId = visible.last.id;
    if (lastId - firstId + 1 > numVisible) {
      const visibleIds = visible.ids;
      for (let i = 1, ii = lastId - firstId; i < ii; i++) {
        const holeId = scrolledDown ? firstId + i : lastId - i;
        if (visibleIds.has(holeId)) {
          continue;
        }
        const holeView = views[holeId - 1];
        if (!this.isViewFinished(holeView)) {
          return holeView;
        }
      }
    }
    let preRenderIndex = scrolledDown ? lastId : firstId - 2;
    let preRenderView = views[preRenderIndex];
    if (preRenderView && !this.isViewFinished(preRenderView)) {
      return preRenderView;
    }
    if (preRenderExtra) {
      preRenderIndex += scrolledDown ? 1 : -1;
      preRenderView = views[preRenderIndex];
      if (preRenderView && !this.isViewFinished(preRenderView)) {
        return preRenderView;
      }
    }
    return null;
  }
  isViewFinished(view) {
    return view.renderingState === RenderingStates.FINISHED;
  }
  renderView(view) {
    switch (view.renderingState) {
      case RenderingStates.FINISHED:
        return false;
      case RenderingStates.PAUSED:
        this.highestPriorityPage = view.renderingId;
        view.resume();
        break;
      case RenderingStates.RUNNING:
        this.highestPriorityPage = view.renderingId;
        break;
      case RenderingStates.INITIAL:
        this.highestPriorityPage = view.renderingId;
        view.draw().finally(() => {
          this.renderHighestPriority();
        }).catch(reason => {
          if (reason instanceof RenderingCancelledException) {
            return;
          }
          console.error(`renderView: "${reason}"`);
        });
        break;
    }
    return true;
  }
}

;// CONCATENATED MODULE: ./web/pdf_scripting_manager.js


class PDFScriptingManager {
  #closeCapability = null;
  #destroyCapability = null;
  #docProperties = null;
  #eventBus = null;
  #externalServices = null;
  #pdfDocument = null;
  #pdfViewer = null;
  #ready = false;
  #sandboxBundleSrc = null;
  #scripting = null;
  #willPrintCapability = null;
  constructor({
    eventBus,
    sandboxBundleSrc = null,
    externalServices = null,
    docProperties = null
  }) {
    this.#eventBus = eventBus;
    this.#externalServices = externalServices;
    this.#docProperties = docProperties;
  }
  setViewer(pdfViewer) {
    this.#pdfViewer = pdfViewer;
  }
  async setDocument(pdfDocument) {
    if (this.#pdfDocument) {
      await this.#destroyScripting();
    }
    this.#pdfDocument = pdfDocument;
    if (!pdfDocument) {
      return;
    }
    const [objects, calculationOrder, docActions] = await Promise.all([pdfDocument.getFieldObjects(), pdfDocument.getCalculationOrderIds(), pdfDocument.getJSActions()]);
    if (!objects && !docActions) {
      await this.#destroyScripting();
      return;
    }
    if (pdfDocument !== this.#pdfDocument) {
      return;
    }
    try {
      this.#scripting = this.#initScripting();
    } catch (error) {
      console.error(`setDocument: "${error.message}".`);
      await this.#destroyScripting();
      return;
    }
    this._internalEvents.set("updatefromsandbox", event => {
      if (event?.source === window) {
        this.#updateFromSandbox(event.detail);
      }
    });
    this._internalEvents.set("dispatcheventinsandbox", event => {
      this.#scripting?.dispatchEventInSandbox(event.detail);
    });
    this._internalEvents.set("pagechanging", ({
      pageNumber,
      previous
    }) => {
      if (pageNumber === previous) {
        return;
      }
      this.#dispatchPageClose(previous);
      this.#dispatchPageOpen(pageNumber);
    });
    this._internalEvents.set("pagerendered", ({
      pageNumber
    }) => {
      if (!this._pageOpenPending.has(pageNumber)) {
        return;
      }
      if (pageNumber !== this.#pdfViewer.currentPageNumber) {
        return;
      }
      this.#dispatchPageOpen(pageNumber);
    });
    this._internalEvents.set("pagesdestroy", async () => {
      await this.#dispatchPageClose(this.#pdfViewer.currentPageNumber);
      await this.#scripting?.dispatchEventInSandbox({
        id: "doc",
        name: "WillClose"
      });
      this.#closeCapability?.resolve();
    });
    for (const [name, listener] of this._internalEvents) {
      this.#eventBus._on(name, listener);
    }
    try {
      const docProperties = await this.#docProperties(pdfDocument);
      if (pdfDocument !== this.#pdfDocument) {
        return;
      }
      await this.#scripting.createSandbox({
        objects,
        calculationOrder,
        appInfo: {
          platform: navigator.platform,
          language: navigator.language
        },
        docInfo: {
          ...docProperties,
          actions: docActions
        }
      });
      this.#eventBus.dispatch("sandboxcreated", {
        source: this
      });
    } catch (error) {
      console.error(`setDocument: "${error.message}".`);
      await this.#destroyScripting();
      return;
    }
    await this.#scripting?.dispatchEventInSandbox({
      id: "doc",
      name: "Open"
    });
    await this.#dispatchPageOpen(this.#pdfViewer.currentPageNumber, true);
    Promise.resolve().then(() => {
      if (pdfDocument === this.#pdfDocument) {
        this.#ready = true;
      }
    });
  }
  async dispatchWillSave() {
    return this.#scripting?.dispatchEventInSandbox({
      id: "doc",
      name: "WillSave"
    });
  }
  async dispatchDidSave() {
    return this.#scripting?.dispatchEventInSandbox({
      id: "doc",
      name: "DidSave"
    });
  }
  async dispatchWillPrint() {
    if (!this.#scripting) {
      return;
    }
    await this.#willPrintCapability?.promise;
    this.#willPrintCapability = new PromiseCapability();
    try {
      await this.#scripting.dispatchEventInSandbox({
        id: "doc",
        name: "WillPrint"
      });
    } catch (ex) {
      this.#willPrintCapability.resolve();
      this.#willPrintCapability = null;
      throw ex;
    }
    await this.#willPrintCapability.promise;
  }
  async dispatchDidPrint() {
    return this.#scripting?.dispatchEventInSandbox({
      id: "doc",
      name: "DidPrint"
    });
  }
  get destroyPromise() {
    return this.#destroyCapability?.promise || null;
  }
  get ready() {
    return this.#ready;
  }
  get _internalEvents() {
    return shadow(this, "_internalEvents", new Map());
  }
  get _pageOpenPending() {
    return shadow(this, "_pageOpenPending", new Set());
  }
  get _visitedPages() {
    return shadow(this, "_visitedPages", new Map());
  }
  async #updateFromSandbox(detail) {
    const pdfViewer = this.#pdfViewer;
    const isInPresentationMode = pdfViewer.isInPresentationMode || pdfViewer.isChangingPresentationMode;
    const {
      id,
      siblings,
      command,
      value
    } = detail;
    if (!id) {
      switch (command) {
        case "clear":
          console.clear();
          break;
        case "error":
          console.error(value);
          break;
        case "layout":
          if (!isInPresentationMode) {
            const modes = apiPageLayoutToViewerModes(value);
            pdfViewer.spreadMode = modes.spreadMode;
          }
          break;
        case "page-num":
          pdfViewer.currentPageNumber = value + 1;
          break;
        case "print":
          await pdfViewer.pagesPromise;
          this.#eventBus.dispatch("print", {
            source: this
          });
          break;
        case "println":
          console.log(value);
          break;
        case "zoom":
          if (!isInPresentationMode) {
            pdfViewer.currentScaleValue = value;
          }
          break;
        case "SaveAs":
          this.#eventBus.dispatch("download", {
            source: this
          });
          break;
        case "FirstPage":
          pdfViewer.currentPageNumber = 1;
          break;
        case "LastPage":
          pdfViewer.currentPageNumber = pdfViewer.pagesCount;
          break;
        case "NextPage":
          pdfViewer.nextPage();
          break;
        case "PrevPage":
          pdfViewer.previousPage();
          break;
        case "ZoomViewIn":
          if (!isInPresentationMode) {
            pdfViewer.increaseScale();
          }
          break;
        case "ZoomViewOut":
          if (!isInPresentationMode) {
            pdfViewer.decreaseScale();
          }
          break;
        case "WillPrintFinished":
          this.#willPrintCapability?.resolve();
          this.#willPrintCapability = null;
          break;
      }
      return;
    }
    if (isInPresentationMode && detail.focus) {
      return;
    }
    delete detail.id;
    delete detail.siblings;
    const ids = siblings ? [id, ...siblings] : [id];
    for (const elementId of ids) {
      const element = document.querySelector(`[data-element-id="${elementId}"]`);
      if (element) {
        element.dispatchEvent(new CustomEvent("updatefromsandbox", {
          detail
        }));
      } else {
        this.#pdfDocument?.annotationStorage.setValue(elementId, detail);
      }
    }
  }
  async #dispatchPageOpen(pageNumber, initialize = false) {
    const pdfDocument = this.#pdfDocument,
      visitedPages = this._visitedPages;
    if (initialize) {
      this.#closeCapability = new PromiseCapability();
    }
    if (!this.#closeCapability) {
      return;
    }
    const pageView = this.#pdfViewer.getPageView(pageNumber - 1);
    if (pageView?.renderingState !== RenderingStates.FINISHED) {
      this._pageOpenPending.add(pageNumber);
      return;
    }
    this._pageOpenPending.delete(pageNumber);
    const actionsPromise = (async () => {
      const actions = await (!visitedPages.has(pageNumber) ? pageView.pdfPage?.getJSActions() : null);
      if (pdfDocument !== this.#pdfDocument) {
        return;
      }
      await this.#scripting?.dispatchEventInSandbox({
        id: "page",
        name: "PageOpen",
        pageNumber,
        actions
      });
    })();
    visitedPages.set(pageNumber, actionsPromise);
  }
  async #dispatchPageClose(pageNumber) {
    const pdfDocument = this.#pdfDocument,
      visitedPages = this._visitedPages;
    if (!this.#closeCapability) {
      return;
    }
    if (this._pageOpenPending.has(pageNumber)) {
      return;
    }
    const actionsPromise = visitedPages.get(pageNumber);
    if (!actionsPromise) {
      return;
    }
    visitedPages.set(pageNumber, null);
    await actionsPromise;
    if (pdfDocument !== this.#pdfDocument) {
      return;
    }
    await this.#scripting?.dispatchEventInSandbox({
      id: "page",
      name: "PageClose",
      pageNumber
    });
  }
  #initScripting() {
    this.#destroyCapability = new PromiseCapability();
    if (this.#scripting) {
      throw new Error("#initScripting: Scripting already exists.");
    }
    return this.#externalServices.createScripting({
      sandboxBundleSrc: this.#sandboxBundleSrc
    });
  }
  async #destroyScripting() {
    if (!this.#scripting) {
      this.#pdfDocument = null;
      this.#destroyCapability?.resolve();
      return;
    }
    if (this.#closeCapability) {
      await Promise.race([this.#closeCapability.promise, new Promise(resolve => {
        setTimeout(resolve, 1000);
      })]).catch(() => {});
      this.#closeCapability = null;
    }
    this.#pdfDocument = null;
    try {
      await this.#scripting.destroySandbox();
    } catch {}
    this.#willPrintCapability?.reject(new Error("Scripting destroyed."));
    this.#willPrintCapability = null;
    for (const [name, listener] of this._internalEvents) {
      this.#eventBus._off(name, listener);
    }
    this._internalEvents.clear();
    this._pageOpenPending.clear();
    this._visitedPages.clear();
    this.#scripting = null;
    this.#ready = false;
    this.#destroyCapability?.resolve();
  }
}

;// CONCATENATED MODULE: ./web/stubs.js
const NullL10n = null;

;// CONCATENATED MODULE: ./web/annotation_editor_layer_builder.js


class AnnotationEditorLayerBuilder {
  #annotationLayer = null;
  #uiManager;
  constructor(options) {
    this.pageDiv = options.pageDiv;
    this.pdfPage = options.pdfPage;
    this.accessibilityManager = options.accessibilityManager;
    this.l10n = options.l10n || NullL10n;
    this.annotationEditorLayer = null;
    this.div = null;
    this._cancelled = false;
    this.#uiManager = options.uiManager;
    this.#annotationLayer = options.annotationLayer || null;
  }
  async render(viewport, intent = "display") {
    if (intent !== "display") {
      return;
    }
    if (this._cancelled) {
      return;
    }
    const clonedViewport = viewport.clone({
      dontFlip: true
    });
    if (this.div) {
      this.annotationEditorLayer.update({
        viewport: clonedViewport
      });
      this.show();
      return;
    }
    const div = this.div = document.createElement("div");
    div.className = "annotationEditorLayer";
    div.tabIndex = 0;
    div.hidden = true;
    div.dir = this.#uiManager.direction;
    this.pageDiv.append(div);
    this.annotationEditorLayer = new AnnotationEditorLayer({
      uiManager: this.#uiManager,
      div,
      accessibilityManager: this.accessibilityManager,
      pageIndex: this.pdfPage.pageNumber - 1,
      l10n: this.l10n,
      viewport: clonedViewport,
      annotationLayer: this.#annotationLayer
    });
    const parameters = {
      viewport: clonedViewport,
      div,
      annotations: null,
      intent
    };
    this.annotationEditorLayer.render(parameters);
    this.show();
  }
  cancel() {
    this._cancelled = true;
    if (!this.div) {
      return;
    }
    this.pageDiv = null;
    this.annotationEditorLayer.destroy();
    this.div.remove();
  }
  hide() {
    if (!this.div) {
      return;
    }
    this.div.hidden = true;
  }
  show() {
    if (!this.div || this.annotationEditorLayer.isEmpty) {
      return;
    }
    this.div.hidden = false;
  }
}

;// CONCATENATED MODULE: ./web/annotation_layer_builder.js


class AnnotationLayerBuilder {
  #onPresentationModeChanged = null;
  constructor({
    pageDiv,
    pdfPage,
    linkService,
    downloadManager,
    annotationStorage = null,
    imageResourcesPath = "",
    renderForms = true,
    enableScripting = false,
    hasJSActionsPromise = null,
    fieldObjectsPromise = null,
    annotationCanvasMap = null,
    accessibilityManager = null
  }) {
    this.pageDiv = pageDiv;
    this.pdfPage = pdfPage;
    this.linkService = linkService;
    this.downloadManager = downloadManager;
    this.imageResourcesPath = imageResourcesPath;
    this.renderForms = renderForms;
    this.annotationStorage = annotationStorage;
    this.enableScripting = enableScripting;
    this._hasJSActionsPromise = hasJSActionsPromise || Promise.resolve(false);
    this._fieldObjectsPromise = fieldObjectsPromise || Promise.resolve(null);
    this._annotationCanvasMap = annotationCanvasMap;
    this._accessibilityManager = accessibilityManager;
    this.annotationLayer = null;
    this.div = null;
    this._cancelled = false;
    this._eventBus = linkService.eventBus;
  }
  async render(viewport, intent = "display") {
    if (this.div) {
      if (this._cancelled || !this.annotationLayer) {
        return;
      }
      this.annotationLayer.update({
        viewport: viewport.clone({
          dontFlip: true
        })
      });
      return;
    }
    const [annotations, hasJSActions, fieldObjects] = await Promise.all([this.pdfPage.getAnnotations({
      intent
    }), this._hasJSActionsPromise, this._fieldObjectsPromise]);
    if (this._cancelled) {
      return;
    }
    const div = this.div = document.createElement("div");
    div.className = "annotationLayer";
    this.pageDiv.append(div);
    if (annotations.length === 0) {
      this.hide();
      return;
    }
    this.annotationLayer = new AnnotationLayer({
      div,
      accessibilityManager: this._accessibilityManager,
      annotationCanvasMap: this._annotationCanvasMap,
      page: this.pdfPage,
      viewport: viewport.clone({
        dontFlip: true
      })
    });
    await this.annotationLayer.render({
      annotations,
      imageResourcesPath: this.imageResourcesPath,
      renderForms: this.renderForms,
      linkService: this.linkService,
      downloadManager: this.downloadManager,
      annotationStorage: this.annotationStorage,
      enableScripting: this.enableScripting,
      hasJSActions,
      fieldObjects
    });
    if (this.linkService.isInPresentationMode) {
      this.#updatePresentationModeState(PresentationModeState.FULLSCREEN);
    }
    if (!this.#onPresentationModeChanged) {
      this.#onPresentationModeChanged = evt => {
        this.#updatePresentationModeState(evt.state);
      };
      this._eventBus?._on("presentationmodechanged", this.#onPresentationModeChanged);
    }
  }
  cancel() {
    this._cancelled = true;
    if (this.#onPresentationModeChanged) {
      this._eventBus?._off("presentationmodechanged", this.#onPresentationModeChanged);
      this.#onPresentationModeChanged = null;
    }
  }
  hide() {
    if (!this.div) {
      return;
    }
    this.div.hidden = true;
  }
  #updatePresentationModeState(state) {
    if (!this.div) {
      return;
    }
    let disableFormElements = false;
    switch (state) {
      case PresentationModeState.FULLSCREEN:
        disableFormElements = true;
        break;
      case PresentationModeState.NORMAL:
        break;
      default:
        return;
    }
    for (const section of this.div.childNodes) {
      if (section.hasAttribute("data-internal-link")) {
        continue;
      }
      section.inert = disableFormElements;
    }
  }
}

;// CONCATENATED MODULE: ./web/struct_tree_layer_builder.js

const PDF_ROLE_TO_HTML_ROLE = {
  Document: null,
  DocumentFragment: null,
  Part: "group",
  Sect: "group",
  Div: "group",
  Aside: "note",
  NonStruct: "none",
  P: null,
  H: "heading",
  Title: null,
  FENote: "note",
  Sub: "group",
  Lbl: null,
  Span: null,
  Em: null,
  Strong: null,
  Link: "link",
  Annot: "note",
  Form: "form",
  Ruby: null,
  RB: null,
  RT: null,
  RP: null,
  Warichu: null,
  WT: null,
  WP: null,
  L: "list",
  LI: "listitem",
  LBody: null,
  Table: "table",
  TR: "row",
  TH: "columnheader",
  TD: "cell",
  THead: "columnheader",
  TBody: null,
  TFoot: null,
  Caption: null,
  Figure: "figure",
  Formula: null,
  Artifact: null
};
const HEADING_PATTERN = /^H(\d+)$/;
class StructTreeLayerBuilder {
  #treeDom = undefined;
  get renderingDone() {
    return this.#treeDom !== undefined;
  }
  render(structTree) {
    if (this.#treeDom !== undefined) {
      return this.#treeDom;
    }
    const treeDom = this.#walk(structTree);
    treeDom?.classList.add("structTree");
    return this.#treeDom = treeDom;
  }
  hide() {
    if (this.#treeDom && !this.#treeDom.hidden) {
      this.#treeDom.hidden = true;
    }
  }
  show() {
    if (this.#treeDom?.hidden) {
      this.#treeDom.hidden = false;
    }
  }
  #setAttributes(structElement, htmlElement) {
    const {
      alt,
      id,
      lang
    } = structElement;
    if (alt !== undefined) {
      htmlElement.setAttribute("aria-label", removeNullCharacters(alt));
    }
    if (id !== undefined) {
      htmlElement.setAttribute("aria-owns", id);
    }
    if (lang !== undefined) {
      htmlElement.setAttribute("lang", removeNullCharacters(lang, true));
    }
  }
  #walk(node) {
    if (!node) {
      return null;
    }
    const element = document.createElement("span");
    if ("role" in node) {
      const {
        role
      } = node;
      const match = role.match(HEADING_PATTERN);
      if (match) {
        element.setAttribute("role", "heading");
        element.setAttribute("aria-level", match[1]);
      } else if (PDF_ROLE_TO_HTML_ROLE[role]) {
        element.setAttribute("role", PDF_ROLE_TO_HTML_ROLE[role]);
      }
    }
    this.#setAttributes(node, element);
    if (node.children) {
      if (node.children.length === 1 && "id" in node.children[0]) {
        this.#setAttributes(node.children[0], element);
      } else {
        for (const kid of node.children) {
          element.append(this.#walk(kid));
        }
      }
    }
    return element;
  }
}

;// CONCATENATED MODULE: ./web/text_accessibility.js

class TextAccessibilityManager {
  #enabled = false;
  #textChildren = null;
  #textNodes = new Map();
  #waitingElements = new Map();
  setTextMapping(textDivs) {
    this.#textChildren = textDivs;
  }
  static #compareElementPositions(e1, e2) {
    const rect1 = e1.getBoundingClientRect();
    const rect2 = e2.getBoundingClientRect();
    if (rect1.width === 0 && rect1.height === 0) {
      return +1;
    }
    if (rect2.width === 0 && rect2.height === 0) {
      return -1;
    }
    const top1 = rect1.y;
    const bot1 = rect1.y + rect1.height;
    const mid1 = rect1.y + rect1.height / 2;
    const top2 = rect2.y;
    const bot2 = rect2.y + rect2.height;
    const mid2 = rect2.y + rect2.height / 2;
    if (mid1 <= top2 && mid2 >= bot1) {
      return -1;
    }
    if (mid2 <= top1 && mid1 >= bot2) {
      return +1;
    }
    const centerX1 = rect1.x + rect1.width / 2;
    const centerX2 = rect2.x + rect2.width / 2;
    return centerX1 - centerX2;
  }
  enable() {
    if (this.#enabled) {
      throw new Error("TextAccessibilityManager is already enabled.");
    }
    if (!this.#textChildren) {
      throw new Error("Text divs and strings have not been set.");
    }
    this.#enabled = true;
    this.#textChildren = this.#textChildren.slice();
    this.#textChildren.sort(TextAccessibilityManager.#compareElementPositions);
    if (this.#textNodes.size > 0) {
      const textChildren = this.#textChildren;
      for (const [id, nodeIndex] of this.#textNodes) {
        const element = document.getElementById(id);
        if (!element) {
          this.#textNodes.delete(id);
          continue;
        }
        this.#addIdToAriaOwns(id, textChildren[nodeIndex]);
      }
    }
    for (const [element, isRemovable] of this.#waitingElements) {
      this.addPointerInTextLayer(element, isRemovable);
    }
    this.#waitingElements.clear();
  }
  disable() {
    if (!this.#enabled) {
      return;
    }
    this.#waitingElements.clear();
    this.#textChildren = null;
    this.#enabled = false;
  }
  removePointerInTextLayer(element) {
    if (!this.#enabled) {
      this.#waitingElements.delete(element);
      return;
    }
    const children = this.#textChildren;
    if (!children || children.length === 0) {
      return;
    }
    const {
      id
    } = element;
    const nodeIndex = this.#textNodes.get(id);
    if (nodeIndex === undefined) {
      return;
    }
    const node = children[nodeIndex];
    this.#textNodes.delete(id);
    let owns = node.getAttribute("aria-owns");
    if (owns?.includes(id)) {
      owns = owns.split(" ").filter(x => x !== id).join(" ");
      if (owns) {
        node.setAttribute("aria-owns", owns);
      } else {
        node.removeAttribute("aria-owns");
        node.setAttribute("role", "presentation");
      }
    }
  }
  #addIdToAriaOwns(id, node) {
    const owns = node.getAttribute("aria-owns");
    if (!owns?.includes(id)) {
      node.setAttribute("aria-owns", owns ? `${owns} ${id}` : id);
    }
    node.removeAttribute("role");
  }
  addPointerInTextLayer(element, isRemovable) {
    const {
      id
    } = element;
    if (!id) {
      return null;
    }
    if (!this.#enabled) {
      this.#waitingElements.set(element, isRemovable);
      return null;
    }
    if (isRemovable) {
      this.removePointerInTextLayer(element);
    }
    const children = this.#textChildren;
    if (!children || children.length === 0) {
      return null;
    }
    const index = binarySearchFirstItem(children, node => TextAccessibilityManager.#compareElementPositions(element, node) < 0);
    const nodeIndex = Math.max(0, index - 1);
    const child = children[nodeIndex];
    this.#addIdToAriaOwns(id, child);
    this.#textNodes.set(id, nodeIndex);
    const parent = child.parentNode;
    return parent?.classList.contains("markedContent") ? parent.id : null;
  }
  moveElementInDOM(container, element, contentElement, isRemovable) {
    const id = this.addPointerInTextLayer(contentElement, isRemovable);
    if (!container.hasChildNodes()) {
      container.append(element);
      return id;
    }
    const children = Array.from(container.childNodes).filter(node => node !== element);
    if (children.length === 0) {
      return id;
    }
    const elementToCompare = contentElement || element;
    const index = binarySearchFirstItem(children, node => TextAccessibilityManager.#compareElementPositions(elementToCompare, node) < 0);
    if (index === 0) {
      children[0].before(element);
    } else {
      children[index - 1].after(element);
    }
    return id;
  }
}

;// CONCATENATED MODULE: ./web/text_highlighter.js
class TextHighlighter {
  constructor({
    findController,
    eventBus,
    pageIndex
  }) {
    this.findController = findController;
    this.matches = [];
    this.eventBus = eventBus;
    this.pageIdx = pageIndex;
    this._onUpdateTextLayerMatches = null;
    this.textDivs = null;
    this.textContentItemsStr = null;
    this.enabled = false;
  }
  setTextMapping(divs, texts) {
    this.textDivs = divs;
    this.textContentItemsStr = texts;
  }
  enable() {
    if (!this.textDivs || !this.textContentItemsStr) {
      throw new Error("Text divs and strings have not been set.");
    }
    if (this.enabled) {
      throw new Error("TextHighlighter is already enabled.");
    }
    this.enabled = true;
    if (!this._onUpdateTextLayerMatches) {
      this._onUpdateTextLayerMatches = evt => {
        if (evt.pageIndex === this.pageIdx || evt.pageIndex === -1) {
          this._updateMatches();
        }
      };
      this.eventBus._on("updatetextlayermatches", this._onUpdateTextLayerMatches);
    }
    this._updateMatches();
  }
  disable() {
    if (!this.enabled) {
      return;
    }
    this.enabled = false;
    if (this._onUpdateTextLayerMatches) {
      this.eventBus._off("updatetextlayermatches", this._onUpdateTextLayerMatches);
      this._onUpdateTextLayerMatches = null;
    }
    this._updateMatches(true);
  }
  _convertMatches(matches, matchesLength) {
    if (!matches) {
      return [];
    }
    const {
      textContentItemsStr
    } = this;
    let i = 0,
      iIndex = 0;
    const end = textContentItemsStr.length - 1;
    const result = [];
    for (let m = 0, mm = matches.length; m < mm; m++) {
      let matchIdx = matches[m];
      while (i !== end && matchIdx >= iIndex + textContentItemsStr[i].length) {
        iIndex += textContentItemsStr[i].length;
        i++;
      }
      if (i === textContentItemsStr.length) {
        console.error("Could not find a matching mapping");
      }
      const match = {
        begin: {
          divIdx: i,
          offset: matchIdx - iIndex
        }
      };
      matchIdx += matchesLength[m];
      while (i !== end && matchIdx > iIndex + textContentItemsStr[i].length) {
        iIndex += textContentItemsStr[i].length;
        i++;
      }
      match.end = {
        divIdx: i,
        offset: matchIdx - iIndex
      };
      result.push(match);
    }
    return result;
  }
  _renderMatches(matches) {
    if (matches.length === 0) {
      return;
    }
    const {
      findController,
      pageIdx
    } = this;
    const {
      textContentItemsStr,
      textDivs
    } = this;
    const isSelectedPage = pageIdx === findController.selected.pageIdx;
    const selectedMatchIdx = findController.selected.matchIdx;
    const highlightAll = findController.state.highlightAll;
    let prevEnd = null;
    const infinity = {
      divIdx: -1,
      offset: undefined
    };
    function beginText(begin, className) {
      const divIdx = begin.divIdx;
      textDivs[divIdx].textContent = "";
      return appendTextToDiv(divIdx, 0, begin.offset, className);
    }
    function appendTextToDiv(divIdx, fromOffset, toOffset, className) {
      let div = textDivs[divIdx];
      if (div.nodeType === Node.TEXT_NODE) {
        const span = document.createElement("span");
        div.before(span);
        span.append(div);
        textDivs[divIdx] = span;
        div = span;
      }
      const content = textContentItemsStr[divIdx].substring(fromOffset, toOffset);
      const node = document.createTextNode(content);
      if (className) {
        const span = document.createElement("span");
        span.className = `${className} appended`;
        span.append(node);
        div.append(span);
        return className.includes("selected") ? span.offsetLeft : 0;
      }
      div.append(node);
      return 0;
    }
    let i0 = selectedMatchIdx,
      i1 = i0 + 1;
    if (highlightAll) {
      i0 = 0;
      i1 = matches.length;
    } else if (!isSelectedPage) {
      return;
    }
    let lastDivIdx = -1;
    let lastOffset = -1;
    for (let i = i0; i < i1; i++) {
      const match = matches[i];
      const begin = match.begin;
      if (begin.divIdx === lastDivIdx && begin.offset === lastOffset) {
        continue;
      }
      lastDivIdx = begin.divIdx;
      lastOffset = begin.offset;
      const end = match.end;
      const isSelected = isSelectedPage && i === selectedMatchIdx;
      const highlightSuffix = isSelected ? " selected" : "";
      let selectedLeft = 0;
      if (!prevEnd || begin.divIdx !== prevEnd.divIdx) {
        if (prevEnd !== null) {
          appendTextToDiv(prevEnd.divIdx, prevEnd.offset, infinity.offset);
        }
        beginText(begin);
      } else {
        appendTextToDiv(prevEnd.divIdx, prevEnd.offset, begin.offset);
      }
      if (begin.divIdx === end.divIdx) {
        selectedLeft = appendTextToDiv(begin.divIdx, begin.offset, end.offset, "highlight" + highlightSuffix);
      } else {
        selectedLeft = appendTextToDiv(begin.divIdx, begin.offset, infinity.offset, "highlight begin" + highlightSuffix);
        for (let n0 = begin.divIdx + 1, n1 = end.divIdx; n0 < n1; n0++) {
          textDivs[n0].className = "highlight middle" + highlightSuffix;
        }
        beginText(end, "highlight end" + highlightSuffix);
      }
      prevEnd = end;
      if (isSelected) {
        findController.scrollMatchIntoView({
          element: textDivs[begin.divIdx],
          selectedLeft,
          pageIndex: pageIdx,
          matchIndex: selectedMatchIdx
        });
      }
    }
    if (prevEnd) {
      appendTextToDiv(prevEnd.divIdx, prevEnd.offset, infinity.offset);
    }
  }
  _updateMatches(reset = false) {
    if (!this.enabled && !reset) {
      return;
    }
    const {
      findController,
      matches,
      pageIdx
    } = this;
    const {
      textContentItemsStr,
      textDivs
    } = this;
    let clearedUntilDivIdx = -1;
    for (const match of matches) {
      const begin = Math.max(clearedUntilDivIdx, match.begin.divIdx);
      for (let n = begin, end = match.end.divIdx; n <= end; n++) {
        const div = textDivs[n];
        div.textContent = textContentItemsStr[n];
        div.className = "";
      }
      clearedUntilDivIdx = match.end.divIdx + 1;
    }
    if (!findController?.highlightMatches || reset) {
      return;
    }
    const pageMatches = findController.pageMatches[pageIdx] || null;
    const pageMatchesLength = findController.pageMatchesLength[pageIdx] || null;
    this.matches = this._convertMatches(pageMatches, pageMatchesLength);
    this._renderMatches(this.matches);
  }
}

;// CONCATENATED MODULE: ./web/text_layer_builder.js


class TextLayerBuilder {
  #enablePermissions = false;
  #rotation = 0;
  #scale = 0;
  #textContentSource = null;
  constructor({
    highlighter = null,
    accessibilityManager = null,
    isOffscreenCanvasSupported = true,
    enablePermissions = false
  }) {
    this.textContentItemsStr = [];
    this.renderingDone = false;
    this.textDivs = [];
    this.textDivProperties = new WeakMap();
    this.textLayerRenderTask = null;
    this.highlighter = highlighter;
    this.accessibilityManager = accessibilityManager;
    this.isOffscreenCanvasSupported = isOffscreenCanvasSupported;
    this.#enablePermissions = enablePermissions === true;
    this.onAppend = null;
    this.div = document.createElement("div");
    this.div.className = "textLayer";
  }
  #finishRendering() {
    this.renderingDone = true;
    const endOfContent = document.createElement("div");
    endOfContent.className = "endOfContent";
    this.div.append(endOfContent);
    this.#bindMouse();
  }
  get numTextDivs() {
    return this.textDivs.length;
  }
  async render(viewport) {
    if (!this.#textContentSource) {
      throw new Error('No "textContentSource" parameter specified.');
    }
    const scale = viewport.scale * (globalThis.devicePixelRatio || 1);
    const {
      rotation
    } = viewport;
    if (this.renderingDone) {
      const mustRotate = rotation !== this.#rotation;
      const mustRescale = scale !== this.#scale;
      if (mustRotate || mustRescale) {
        this.hide();
        updateTextLayer({
          container: this.div,
          viewport,
          textDivs: this.textDivs,
          textDivProperties: this.textDivProperties,
          isOffscreenCanvasSupported: this.isOffscreenCanvasSupported,
          mustRescale,
          mustRotate
        });
        this.#scale = scale;
        this.#rotation = rotation;
      }
      this.show();
      return;
    }
    this.cancel();
    this.highlighter?.setTextMapping(this.textDivs, this.textContentItemsStr);
    this.accessibilityManager?.setTextMapping(this.textDivs);
    this.textLayerRenderTask = renderTextLayer({
      textContentSource: this.#textContentSource,
      container: this.div,
      viewport,
      textDivs: this.textDivs,
      textDivProperties: this.textDivProperties,
      textContentItemsStr: this.textContentItemsStr,
      isOffscreenCanvasSupported: this.isOffscreenCanvasSupported
    });
    await this.textLayerRenderTask.promise;
    this.#finishRendering();
    this.#scale = scale;
    this.#rotation = rotation;
    this.onAppend(this.div);
    this.highlighter?.enable();
    this.accessibilityManager?.enable();
  }
  hide() {
    if (!this.div.hidden && this.renderingDone) {
      this.highlighter?.disable();
      this.div.hidden = true;
    }
  }
  show() {
    if (this.div.hidden && this.renderingDone) {
      this.div.hidden = false;
      this.highlighter?.enable();
    }
  }
  cancel() {
    if (this.textLayerRenderTask) {
      this.textLayerRenderTask.cancel();
      this.textLayerRenderTask = null;
    }
    this.highlighter?.disable();
    this.accessibilityManager?.disable();
    this.textContentItemsStr.length = 0;
    this.textDivs.length = 0;
    this.textDivProperties = new WeakMap();
  }
  setTextContentSource(source) {
    this.cancel();
    this.#textContentSource = source;
  }
  #bindMouse() {
    const {
      div
    } = this;
    div.addEventListener("mousedown", evt => {
      const end = div.querySelector(".endOfContent");
      if (!end) {
        return;
      }
      end.classList.add("active");
    });
    div.addEventListener("mouseup", () => {
      const end = div.querySelector(".endOfContent");
      if (!end) {
        return;
      }
      end.classList.remove("active");
    });
    div.addEventListener("copy", event => {
      if (!this.#enablePermissions) {
        const selection = document.getSelection();
        event.clipboardData.setData("text/plain", removeNullCharacters(normalizeUnicode(selection.toString())));
      }
      event.preventDefault();
      event.stopPropagation();
    });
  }
}

;// CONCATENATED MODULE: ./web/xfa_layer_builder.js

class XfaLayerBuilder {
  constructor({
    pdfPage,
    annotationStorage = null,
    linkService,
    xfaHtml = null
  }) {
    this.pdfPage = pdfPage;
    this.annotationStorage = annotationStorage;
    this.linkService = linkService;
    this.xfaHtml = xfaHtml;
    this.div = null;
    this._cancelled = false;
  }
  async render(viewport, intent = "display") {
    if (intent === "print") {
      const parameters = {
        viewport: viewport.clone({
          dontFlip: true
        }),
        div: this.div,
        xfaHtml: this.xfaHtml,
        annotationStorage: this.annotationStorage,
        linkService: this.linkService,
        intent
      };
      this.div = document.createElement("div");
      parameters.div = this.div;
      return XfaLayer.render(parameters);
    }
    const xfaHtml = await this.pdfPage.getXfa();
    if (this._cancelled || !xfaHtml) {
      return {
        textDivs: []
      };
    }
    const parameters = {
      viewport: viewport.clone({
        dontFlip: true
      }),
      div: this.div,
      xfaHtml,
      annotationStorage: this.annotationStorage,
      linkService: this.linkService,
      intent
    };
    if (this.div) {
      return XfaLayer.update(parameters);
    }
    this.div = document.createElement("div");
    parameters.div = this.div;
    return XfaLayer.render(parameters);
  }
  cancel() {
    this._cancelled = true;
  }
  hide() {
    if (!this.div) {
      return;
    }
    this.div.hidden = true;
  }
}

;// CONCATENATED MODULE: ./web/pdf_page_view.js












const MAX_CANVAS_PIXELS = compatibilityParams.maxCanvasPixels || 16777216;
const DEFAULT_LAYER_PROPERTIES = null;
class PDFPageView {
  #annotationMode = AnnotationMode.ENABLE_FORMS;
  #hasRestrictedScaling = false;
  #layerProperties = null;
  #loadingId = null;
  #previousRotation = null;
  #renderError = null;
  #renderingState = RenderingStates.INITIAL;
  #textLayerMode = TextLayerMode.ENABLE;
  #useThumbnailCanvas = {
    directDrawing: true,
    initialOptionalContent: true,
    regularAnnotations: true
  };
  #viewportMap = new WeakMap();
  constructor(options) {
    const container = options.container;
    const defaultViewport = options.defaultViewport;
    this.id = options.id;
    this.renderingId = "page" + this.id;
    this.#layerProperties = options.layerProperties || DEFAULT_LAYER_PROPERTIES;
    this.pdfPage = null;
    this.pageLabel = null;
    this.rotation = 0;
    this.scale = options.scale || DEFAULT_SCALE;
    this.viewport = defaultViewport;
    this.pdfPageRotate = defaultViewport.rotation;
    this._optionalContentConfigPromise = options.optionalContentConfigPromise || null;
    this.#textLayerMode = options.textLayerMode ?? TextLayerMode.ENABLE;
    this.#annotationMode = options.annotationMode ?? AnnotationMode.ENABLE_FORMS;
    this.imageResourcesPath = options.imageResourcesPath || "";
    this.isOffscreenCanvasSupported = options.isOffscreenCanvasSupported ?? true;
    this.maxCanvasPixels = options.maxCanvasPixels ?? MAX_CANVAS_PIXELS;
    this.pageColors = options.pageColors || null;
    this.eventBus = options.eventBus;
    this.renderingQueue = options.renderingQueue;
    this.l10n = options.l10n || NullL10n;
    this.renderTask = null;
    this.resume = null;
    this._annotationCanvasMap = null;
    this.annotationLayer = null;
    this.annotationEditorLayer = null;
    this.textLayer = null;
    this.zoomLayer = null;
    this.xfaLayer = null;
    this.structTreeLayer = null;
    const div = document.createElement("div");
    div.className = "page";
    div.setAttribute("data-page-number", this.id);
    div.setAttribute("role", "region");
    div.setAttribute("data-l10n-id", "pdfjs-page-landmark");
    div.setAttribute("data-l10n-args", JSON.stringify({
      page: this.id
    }));
    this.div = div;
    this.#setDimensions();
    container?.append(div);
  }
  get renderingState() {
    return this.#renderingState;
  }
  set renderingState(state) {
    if (state === this.#renderingState) {
      return;
    }
    this.#renderingState = state;
    if (this.#loadingId) {
      clearTimeout(this.#loadingId);
      this.#loadingId = null;
    }
    switch (state) {
      case RenderingStates.PAUSED:
        this.div.classList.remove("loading");
        break;
      case RenderingStates.RUNNING:
        this.div.classList.add("loadingIcon");
        this.#loadingId = setTimeout(() => {
          this.div.classList.add("loading");
          this.#loadingId = null;
        }, 0);
        break;
      case RenderingStates.INITIAL:
      case RenderingStates.FINISHED:
        this.div.classList.remove("loadingIcon", "loading");
        break;
    }
  }
  #setDimensions() {
    const {
      viewport
    } = this;
    if (this.pdfPage) {
      if (this.#previousRotation === viewport.rotation) {
        return;
      }
      this.#previousRotation = viewport.rotation;
    }
    setLayerDimensions(this.div, viewport, true, false);
  }
  setPdfPage(pdfPage) {
    this.pdfPage = pdfPage;
    this.pdfPageRotate = pdfPage.rotate;
    const totalRotation = (this.rotation + this.pdfPageRotate) % 360;
    this.viewport = pdfPage.getViewport({
      scale: this.scale * PixelsPerInch.PDF_TO_CSS_UNITS,
      rotation: totalRotation
    });
    this.#setDimensions();
    this.reset();
  }
  destroy() {
    this.reset();
    this.pdfPage?.cleanup();
  }
  get _textHighlighter() {
    return shadow(this, "_textHighlighter", new TextHighlighter({
      pageIndex: this.id - 1,
      eventBus: this.eventBus,
      findController: this.#layerProperties.findController
    }));
  }
  async #renderAnnotationLayer() {
    let error = null;
    try {
      await this.annotationLayer.render(this.viewport, "display");
    } catch (ex) {
      console.error(`#renderAnnotationLayer: "${ex}".`);
      error = ex;
    } finally {
      this.eventBus.dispatch("annotationlayerrendered", {
        source: this,
        pageNumber: this.id,
        error
      });
    }
  }
  async #renderAnnotationEditorLayer() {
    let error = null;
    try {
      await this.annotationEditorLayer.render(this.viewport, "display");
    } catch (ex) {
      console.error(`#renderAnnotationEditorLayer: "${ex}".`);
      error = ex;
    } finally {
      this.eventBus.dispatch("annotationeditorlayerrendered", {
        source: this,
        pageNumber: this.id,
        error
      });
    }
  }
  async #renderXfaLayer() {
    let error = null;
    try {
      const result = await this.xfaLayer.render(this.viewport, "display");
      if (result?.textDivs && this._textHighlighter) {
        this.#buildXfaTextContentItems(result.textDivs);
      }
    } catch (ex) {
      console.error(`#renderXfaLayer: "${ex}".`);
      error = ex;
    } finally {
      if (this.xfaLayer?.div) {
        this.l10n.pause();
        this.div.append(this.xfaLayer.div);
        this.l10n.resume();
      }
      this.eventBus.dispatch("xfalayerrendered", {
        source: this,
        pageNumber: this.id,
        error
      });
    }
  }
  async #renderTextLayer() {
    const {
      pdfPage,
      textLayer,
      viewport
    } = this;
    if (!textLayer) {
      return;
    }
    let error = null;
    try {
      if (!textLayer.renderingDone) {
        const readableStream = pdfPage.streamTextContent({
          includeMarkedContent: true,
          disableNormalization: true
        });
        textLayer.setTextContentSource(readableStream);
      }
      await textLayer.render(viewport);
    } catch (ex) {
      if (ex instanceof AbortException) {
        return;
      }
      console.error(`#renderTextLayer: "${ex}".`);
      error = ex;
    }
    this.eventBus.dispatch("textlayerrendered", {
      source: this,
      pageNumber: this.id,
      numTextDivs: textLayer.numTextDivs,
      error
    });
    this.#renderStructTreeLayer();
  }
  async #renderStructTreeLayer() {
    if (!this.textLayer) {
      return;
    }
    this.structTreeLayer ||= new StructTreeLayerBuilder();
    const tree = await (!this.structTreeLayer.renderingDone ? this.pdfPage.getStructTree() : null);
    const treeDom = this.structTreeLayer?.render(tree);
    if (treeDom) {
      this.l10n.pause();
      this.canvas?.append(treeDom);
      this.l10n.resume();
    }
    this.structTreeLayer?.show();
  }
  async #buildXfaTextContentItems(textDivs) {
    const text = await this.pdfPage.getTextContent();
    const items = [];
    for (const item of text.items) {
      items.push(item.str);
    }
    this._textHighlighter.setTextMapping(textDivs, items);
    this._textHighlighter.enable();
  }
  _resetZoomLayer(removeFromDOM = false) {
    if (!this.zoomLayer) {
      return;
    }
    const zoomLayerCanvas = this.zoomLayer.firstChild;
    this.#viewportMap.delete(zoomLayerCanvas);
    zoomLayerCanvas.width = 0;
    zoomLayerCanvas.height = 0;
    if (removeFromDOM) {
      this.zoomLayer.remove();
    }
    this.zoomLayer = null;
  }
  reset({
    keepZoomLayer = false,
    keepAnnotationLayer = false,
    keepAnnotationEditorLayer = false,
    keepXfaLayer = false,
    keepTextLayer = false
  } = {}) {
    this.cancelRendering({
      keepAnnotationLayer,
      keepAnnotationEditorLayer,
      keepXfaLayer,
      keepTextLayer
    });
    this.renderingState = RenderingStates.INITIAL;
    const div = this.div;
    const childNodes = div.childNodes,
      zoomLayerNode = keepZoomLayer && this.zoomLayer || null,
      annotationLayerNode = keepAnnotationLayer && this.annotationLayer?.div || null,
      annotationEditorLayerNode = keepAnnotationEditorLayer && this.annotationEditorLayer?.div || null,
      xfaLayerNode = keepXfaLayer && this.xfaLayer?.div || null,
      textLayerNode = keepTextLayer && this.textLayer?.div || null;
    for (let i = childNodes.length - 1; i >= 0; i--) {
      const node = childNodes[i];
      switch (node) {
        case zoomLayerNode:
        case annotationLayerNode:
        case annotationEditorLayerNode:
        case xfaLayerNode:
        case textLayerNode:
          continue;
      }
      node.remove();
    }
    div.removeAttribute("data-loaded");
    if (annotationLayerNode) {
      this.annotationLayer.hide();
    }
    if (annotationEditorLayerNode) {
      this.annotationEditorLayer.hide();
    }
    if (xfaLayerNode) {
      this.xfaLayer.hide();
    }
    if (textLayerNode) {
      this.textLayer.hide();
    }
    this.structTreeLayer?.hide();
    if (!zoomLayerNode) {
      if (this.canvas) {
        this.#viewportMap.delete(this.canvas);
        this.canvas.width = 0;
        this.canvas.height = 0;
        delete this.canvas;
      }
      this._resetZoomLayer();
    }
  }
  update({
    scale = 0,
    rotation = null,
    optionalContentConfigPromise = null,
    drawingDelay = -1
  }) {
    this.scale = scale || this.scale;
    if (typeof rotation === "number") {
      this.rotation = rotation;
    }
    if (optionalContentConfigPromise instanceof Promise) {
      this._optionalContentConfigPromise = optionalContentConfigPromise;
      optionalContentConfigPromise.then(optionalContentConfig => {
        if (optionalContentConfigPromise !== this._optionalContentConfigPromise) {
          return;
        }
        this.#useThumbnailCanvas.initialOptionalContent = optionalContentConfig.hasInitialVisibility;
      });
    }
    this.#useThumbnailCanvas.directDrawing = true;
    const totalRotation = (this.rotation + this.pdfPageRotate) % 360;
    this.viewport = this.viewport.clone({
      scale: this.scale * PixelsPerInch.PDF_TO_CSS_UNITS,
      rotation: totalRotation
    });
    this.#setDimensions();
    if (this.canvas) {
      let onlyCssZoom = false;
      if (this.#hasRestrictedScaling) {
        if (this.maxCanvasPixels > 0) {
          const {
            width,
            height
          } = this.viewport;
          const {
            sx,
            sy
          } = this.outputScale;
          onlyCssZoom = (Math.floor(width) * sx | 0) * (Math.floor(height) * sy | 0) > this.maxCanvasPixels;
        }
      }
      const postponeDrawing = !onlyCssZoom && drawingDelay >= 0 && drawingDelay < 1000;
      if (postponeDrawing || onlyCssZoom) {
        if (postponeDrawing && this.renderingState !== RenderingStates.FINISHED) {
          this.cancelRendering({
            keepZoomLayer: true,
            keepAnnotationLayer: true,
            keepAnnotationEditorLayer: true,
            keepXfaLayer: true,
            keepTextLayer: true,
            cancelExtraDelay: drawingDelay
          });
          this.renderingState = RenderingStates.FINISHED;
          this.#useThumbnailCanvas.directDrawing = false;
        }
        this.cssTransform({
          target: this.canvas,
          redrawAnnotationLayer: true,
          redrawAnnotationEditorLayer: true,
          redrawXfaLayer: true,
          redrawTextLayer: !postponeDrawing,
          hideTextLayer: postponeDrawing
        });
        if (postponeDrawing) {
          return;
        }
        this.eventBus.dispatch("pagerendered", {
          source: this,
          pageNumber: this.id,
          cssTransform: true,
          timestamp: performance.now(),
          error: this.#renderError
        });
        return;
      }
      if (!this.zoomLayer && !this.canvas.hidden) {
        this.zoomLayer = this.canvas.parentNode;
        this.zoomLayer.style.position = "absolute";
      }
    }
    if (this.zoomLayer) {
      this.cssTransform({
        target: this.zoomLayer.firstChild
      });
    }
    this.reset({
      keepZoomLayer: true,
      keepAnnotationLayer: true,
      keepAnnotationEditorLayer: true,
      keepXfaLayer: true,
      keepTextLayer: true
    });
  }
  cancelRendering({
    keepAnnotationLayer = false,
    keepAnnotationEditorLayer = false,
    keepXfaLayer = false,
    keepTextLayer = false,
    cancelExtraDelay = 0
  } = {}) {
    if (this.renderTask) {
      this.renderTask.cancel(cancelExtraDelay);
      this.renderTask = null;
    }
    this.resume = null;
    if (this.textLayer && (!keepTextLayer || !this.textLayer.div)) {
      this.textLayer.cancel();
      this.textLayer = null;
    }
    if (this.structTreeLayer && !this.textLayer) {
      this.structTreeLayer = null;
    }
    if (this.annotationLayer && (!keepAnnotationLayer || !this.annotationLayer.div)) {
      this.annotationLayer.cancel();
      this.annotationLayer = null;
      this._annotationCanvasMap = null;
    }
    if (this.annotationEditorLayer && (!keepAnnotationEditorLayer || !this.annotationEditorLayer.div)) {
      this.annotationEditorLayer.cancel();
      this.annotationEditorLayer = null;
    }
    if (this.xfaLayer && (!keepXfaLayer || !this.xfaLayer.div)) {
      this.xfaLayer.cancel();
      this.xfaLayer = null;
      this._textHighlighter?.disable();
    }
  }
  cssTransform({
    target,
    redrawAnnotationLayer = false,
    redrawAnnotationEditorLayer = false,
    redrawXfaLayer = false,
    redrawTextLayer = false,
    hideTextLayer = false
  }) {
    if (!target.hasAttribute("zooming")) {
      target.setAttribute("zooming", true);
      const {
        style
      } = target;
      style.width = style.height = "";
    }
    const originalViewport = this.#viewportMap.get(target);
    if (this.viewport !== originalViewport) {
      const relativeRotation = this.viewport.rotation - originalViewport.rotation;
      const absRotation = Math.abs(relativeRotation);
      let scaleX = 1,
        scaleY = 1;
      if (absRotation === 90 || absRotation === 270) {
        const {
          width,
          height
        } = this.viewport;
        scaleX = height / width;
        scaleY = width / height;
      }
      target.style.transform = `rotate(${relativeRotation}deg) scale(${scaleX}, ${scaleY})`;
    }
    if (redrawAnnotationLayer && this.annotationLayer) {
      this.#renderAnnotationLayer();
    }
    if (redrawAnnotationEditorLayer && this.annotationEditorLayer) {
      this.#renderAnnotationEditorLayer();
    }
    if (redrawXfaLayer && this.xfaLayer) {
      this.#renderXfaLayer();
    }
    if (this.textLayer) {
      if (hideTextLayer) {
        this.textLayer.hide();
        this.structTreeLayer?.hide();
      } else if (redrawTextLayer) {
        this.#renderTextLayer();
      }
    }
  }
  get width() {
    return this.viewport.width;
  }
  get height() {
    return this.viewport.height;
  }
  getPagePoint(x, y) {
    return this.viewport.convertToPdfPoint(x, y);
  }
  async #finishRenderTask(renderTask, error = null) {
    if (renderTask === this.renderTask) {
      this.renderTask = null;
    }
    if (error instanceof RenderingCancelledException) {
      this.#renderError = null;
      return;
    }
    this.#renderError = error;
    this.renderingState = RenderingStates.FINISHED;
    this._resetZoomLayer(true);
    this.#useThumbnailCanvas.regularAnnotations = !renderTask.separateAnnots;
    this.eventBus.dispatch("pagerendered", {
      source: this,
      pageNumber: this.id,
      cssTransform: false,
      timestamp: performance.now(),
      error: this.#renderError
    });
    if (error) {
      throw error;
    }
  }
  async draw() {
    if (this.renderingState !== RenderingStates.INITIAL) {
      console.error("Must be in new state before drawing");
      this.reset();
    }
    const {
      div,
      l10n,
      pageColors,
      pdfPage,
      viewport
    } = this;
    if (!pdfPage) {
      this.renderingState = RenderingStates.FINISHED;
      throw new Error("pdfPage is not loaded");
    }
    this.renderingState = RenderingStates.RUNNING;
    const canvasWrapper = document.createElement("div");
    canvasWrapper.classList.add("canvasWrapper");
    div.append(canvasWrapper);
    if (!this.textLayer && this.#textLayerMode !== TextLayerMode.DISABLE && !pdfPage.isPureXfa) {
      this._accessibilityManager ||= new TextAccessibilityManager();
      this.textLayer = new TextLayerBuilder({
        highlighter: this._textHighlighter,
        accessibilityManager: this._accessibilityManager,
        isOffscreenCanvasSupported: this.isOffscreenCanvasSupported,
        enablePermissions: this.#textLayerMode === TextLayerMode.ENABLE_PERMISSIONS
      });
      this.textLayer.onAppend = textLayerDiv => {
        this.l10n.pause();
        this.div.append(textLayerDiv);
        this.l10n.resume();
      };
    }
    if (!this.annotationLayer && this.#annotationMode !== AnnotationMode.DISABLE) {
      const {
        annotationStorage,
        downloadManager,
        enableScripting,
        fieldObjectsPromise,
        hasJSActionsPromise,
        linkService
      } = this.#layerProperties;
      this._annotationCanvasMap ||= new Map();
      this.annotationLayer = new AnnotationLayerBuilder({
        pageDiv: div,
        pdfPage,
        annotationStorage,
        imageResourcesPath: this.imageResourcesPath,
        renderForms: this.#annotationMode === AnnotationMode.ENABLE_FORMS,
        linkService,
        downloadManager,
        enableScripting,
        hasJSActionsPromise,
        fieldObjectsPromise,
        annotationCanvasMap: this._annotationCanvasMap,
        accessibilityManager: this._accessibilityManager
      });
    }
    const renderContinueCallback = cont => {
      showCanvas?.(false);
      if (this.renderingQueue && !this.renderingQueue.isHighestPriority(this)) {
        this.renderingState = RenderingStates.PAUSED;
        this.resume = () => {
          this.renderingState = RenderingStates.RUNNING;
          cont();
        };
        return;
      }
      cont();
    };
    const {
      width,
      height
    } = viewport;
    const canvas = document.createElement("canvas");
    canvas.setAttribute("role", "presentation");
    canvas.hidden = true;
    const hasHCM = !!(pageColors?.background && pageColors?.foreground);
    let showCanvas = isLastShow => {
      if (!hasHCM || isLastShow) {
        canvas.hidden = false;
        showCanvas = null;
      }
    };
    canvasWrapper.append(canvas);
    this.canvas = canvas;
    const ctx = canvas.getContext("2d", {
      alpha: false
    });
    const outputScale = this.outputScale = new OutputScale();
    if (this.maxCanvasPixels > 0) {
      const pixelsInViewport = width * height;
      const maxScale = Math.sqrt(this.maxCanvasPixels / pixelsInViewport);
      if (outputScale.sx > maxScale || outputScale.sy > maxScale) {
        outputScale.sx = maxScale;
        outputScale.sy = maxScale;
        this.#hasRestrictedScaling = true;
      } else {
        this.#hasRestrictedScaling = false;
      }
    }
    const sfx = approximateFraction(outputScale.sx);
    const sfy = approximateFraction(outputScale.sy);
    canvas.width = roundToDivide(width * outputScale.sx, sfx[0]);
    canvas.height = roundToDivide(height * outputScale.sy, sfy[0]);
    const {
      style
    } = canvas;
    style.width = roundToDivide(width, sfx[1]) + "px";
    style.height = roundToDivide(height, sfy[1]) + "px";
    this.#viewportMap.set(canvas, viewport);
    const transform = outputScale.scaled ? [outputScale.sx, 0, 0, outputScale.sy, 0, 0] : null;
    const renderContext = {
      canvasContext: ctx,
      transform,
      viewport,
      annotationMode: this.#annotationMode,
      optionalContentConfigPromise: this._optionalContentConfigPromise,
      annotationCanvasMap: this._annotationCanvasMap,
      pageColors
    };
    const renderTask = this.renderTask = this.pdfPage.render(renderContext);
    renderTask.onContinue = renderContinueCallback;
    const resultPromise = renderTask.promise.then(async () => {
      showCanvas?.(true);
      await this.#finishRenderTask(renderTask);
      this.#renderTextLayer();
      if (this.annotationLayer) {
        await this.#renderAnnotationLayer();
      }
      if (!this.annotationEditorLayer) {
        const {
          annotationEditorUIManager
        } = this.#layerProperties;
        if (!annotationEditorUIManager) {
          return;
        }
        this.annotationEditorLayer = new AnnotationEditorLayerBuilder({
          uiManager: annotationEditorUIManager,
          pageDiv: div,
          pdfPage,
          l10n,
          accessibilityManager: this._accessibilityManager,
          annotationLayer: this.annotationLayer?.annotationLayer
        });
      }
      this.#renderAnnotationEditorLayer();
    }, error => {
      if (!(error instanceof RenderingCancelledException)) {
        showCanvas?.(true);
      }
      return this.#finishRenderTask(renderTask, error);
    });
    if (pdfPage.isPureXfa) {
      if (!this.xfaLayer) {
        const {
          annotationStorage,
          linkService
        } = this.#layerProperties;
        this.xfaLayer = new XfaLayerBuilder({
          pdfPage,
          annotationStorage,
          linkService
        });
      }
      this.#renderXfaLayer();
    }
    div.setAttribute("data-loaded", true);
    this.eventBus.dispatch("pagerender", {
      source: this,
      pageNumber: this.id
    });
    return resultPromise;
  }
  setPageLabel(label) {
    this.pageLabel = typeof label === "string" ? label : null;
    this.div.setAttribute("data-l10n-args", JSON.stringify({
      page: this.pageLabel ?? this.id
    }));
    if (this.pageLabel !== null) {
      this.div.setAttribute("data-page-label", this.pageLabel);
    } else {
      this.div.removeAttribute("data-page-label");
    }
  }
  get thumbnailCanvas() {
    const {
      directDrawing,
      initialOptionalContent,
      regularAnnotations
    } = this.#useThumbnailCanvas;
    return directDrawing && initialOptionalContent && regularAnnotations ? this.canvas : null;
  }
}

;// CONCATENATED MODULE: ./web/pdf_viewer.js






const DEFAULT_CACHE_SIZE = 10;
const PagesCountLimit = {
  FORCE_SCROLL_MODE_PAGE: 15000,
  FORCE_LAZY_PAGE_INIT: 7500,
  PAUSE_EAGER_PAGE_INIT: 250
};
function isValidAnnotationEditorMode(mode) {
  return Object.values(AnnotationEditorType).includes(mode) && mode !== AnnotationEditorType.DISABLE;
}
class PDFPageViewBuffer {
  #buf = new Set();
  #size = 0;
  constructor(size) {
    this.#size = size;
  }
  push(view) {
    const buf = this.#buf;
    if (buf.has(view)) {
      buf.delete(view);
    }
    buf.add(view);
    if (buf.size > this.#size) {
      this.#destroyFirstView();
    }
  }
  resize(newSize, idsToKeep = null) {
    this.#size = newSize;
    const buf = this.#buf;
    if (idsToKeep) {
      const ii = buf.size;
      let i = 1;
      for (const view of buf) {
        if (idsToKeep.has(view.id)) {
          buf.delete(view);
          buf.add(view);
        }
        if (++i > ii) {
          break;
        }
      }
    }
    while (buf.size > this.#size) {
      this.#destroyFirstView();
    }
  }
  has(view) {
    return this.#buf.has(view);
  }
  [Symbol.iterator]() {
    return this.#buf.keys();
  }
  #destroyFirstView() {
    const firstView = this.#buf.keys().next().value;
    firstView?.destroy();
    this.#buf.delete(firstView);
  }
}
class PDFViewer {
  #buffer = null;
  #altTextManager = null;
  #annotationEditorMode = AnnotationEditorType.NONE;
  #annotationEditorUIManager = null;
  #annotationMode = AnnotationMode.ENABLE_FORMS;
  #containerTopLeft = null;
  #copyCallbackBound = null;
  #enablePermissions = false;
  #getAllTextInProgress = false;
  #hiddenCopyElement = null;
  #interruptCopyCondition = false;
  #previousContainerHeight = 0;
  #resizeObserver = new ResizeObserver(this.#resizeObserverCallback.bind(this));
  #scrollModePageState = null;
  #onVisibilityChange = null;
  #scaleTimeoutId = null;
  #textLayerMode = TextLayerMode.ENABLE;
  constructor(options) {
    const viewerVersion = '4.0.240';
    if (version !== viewerVersion) {
      throw new Error(`The API version "${version}" does not match the Viewer version "${viewerVersion}".`);
    }
    this.container = options.container;
    this.viewer = options.viewer || options.container.firstElementChild;
    this.#resizeObserver.observe(this.container);
    this.eventBus = options.eventBus;
    this.linkService = options.linkService || new SimpleLinkService();
    this.downloadManager = options.downloadManager || null;
    this.findController = options.findController || null;
    this.#altTextManager = options.altTextManager || null;
    if (this.findController) {
      this.findController.onIsPageVisible = pageNumber => this._getVisiblePages().ids.has(pageNumber);
    }
    this._scriptingManager = options.scriptingManager || null;
    this.#textLayerMode = options.textLayerMode ?? TextLayerMode.ENABLE;
    this.#annotationMode = options.annotationMode ?? AnnotationMode.ENABLE_FORMS;
    this.#annotationEditorMode = options.annotationEditorMode ?? AnnotationEditorType.NONE;
    this.imageResourcesPath = options.imageResourcesPath || "";
    this.enablePrintAutoRotate = options.enablePrintAutoRotate || false;
    this.isOffscreenCanvasSupported = options.isOffscreenCanvasSupported ?? true;
    this.maxCanvasPixels = options.maxCanvasPixels;
    this.l10n = options.l10n || NullL10n;
    this.#enablePermissions = options.enablePermissions || false;
    this.pageColors = options.pageColors || null;
    this.defaultRenderingQueue = !options.renderingQueue;
    this.renderingQueue = options.renderingQueue;
    this.scroll = watchScroll(this.container, this._scrollUpdate.bind(this));
    this.presentationModeState = PresentationModeState.UNKNOWN;
    this._onBeforeDraw = this._onAfterDraw = null;
    this._resetView();
    this.#updateContainerHeightCss();
    this.eventBus._on("thumbnailrendered", ({
      pageNumber,
      pdfPage
    }) => {
      const pageView = this._pages[pageNumber - 1];
      if (!this.#buffer.has(pageView)) {
        pdfPage?.cleanup();
      }
    });
  }
  get pagesCount() {
    return this._pages.length;
  }
  getPageView(index) {
    return this._pages[index];
  }
  getCachedPageViews() {
    return new Set(this.#buffer);
  }
  get pageViewsReady() {
    return this._pagesCapability.settled && this._pages.every(pageView => pageView?.pdfPage);
  }
  get renderForms() {
    return this.#annotationMode === AnnotationMode.ENABLE_FORMS;
  }
  get enableScripting() {
    return !!this._scriptingManager;
  }
  get currentPageNumber() {
    return this._currentPageNumber;
  }
  set currentPageNumber(val) {
    if (!Number.isInteger(val)) {
      throw new Error("Invalid page number.");
    }
    if (!this.pdfDocument) {
      return;
    }
    if (!this._setCurrentPageNumber(val, true)) {
      console.error(`currentPageNumber: "${val}" is not a valid page.`);
    }
  }
  _setCurrentPageNumber(val, resetCurrentPageView = false) {
    if (this._currentPageNumber === val) {
      if (resetCurrentPageView) {
        this.#resetCurrentPageView();
      }
      return true;
    }
    if (!(0 < val && val <= this.pagesCount)) {
      return false;
    }
    const previous = this._currentPageNumber;
    this._currentPageNumber = val;
    this.eventBus.dispatch("pagechanging", {
      source: this,
      pageNumber: val,
      pageLabel: this._pageLabels?.[val - 1] ?? null,
      previous
    });
    if (resetCurrentPageView) {
      this.#resetCurrentPageView();
    }
    return true;
  }
  get currentPageLabel() {
    return this._pageLabels?.[this._currentPageNumber - 1] ?? null;
  }
  set currentPageLabel(val) {
    if (!this.pdfDocument) {
      return;
    }
    let page = val | 0;
    if (this._pageLabels) {
      const i = this._pageLabels.indexOf(val);
      if (i >= 0) {
        page = i + 1;
      }
    }
    if (!this._setCurrentPageNumber(page, true)) {
      console.error(`currentPageLabel: "${val}" is not a valid page.`);
    }
  }
  get currentScale() {
    return this._currentScale !== UNKNOWN_SCALE ? this._currentScale : DEFAULT_SCALE;
  }
  set currentScale(val) {
    if (isNaN(val)) {
      throw new Error("Invalid numeric scale.");
    }
    if (!this.pdfDocument) {
      return;
    }
    this.#setScale(val, {
      noScroll: false
    });
  }
  get currentScaleValue() {
    return this._currentScaleValue;
  }
  set currentScaleValue(val) {
    if (!this.pdfDocument) {
      return;
    }
    this.#setScale(val, {
      noScroll: false
    });
  }
  get pagesRotation() {
    return this._pagesRotation;
  }
  set pagesRotation(rotation) {
    if (!isValidRotation(rotation)) {
      throw new Error("Invalid pages rotation angle.");
    }
    if (!this.pdfDocument) {
      return;
    }
    rotation %= 360;
    if (rotation < 0) {
      rotation += 360;
    }
    if (this._pagesRotation === rotation) {
      return;
    }
    this._pagesRotation = rotation;
    const pageNumber = this._currentPageNumber;
    this.refresh(true, {
      rotation
    });
    if (this._currentScaleValue) {
      this.#setScale(this._currentScaleValue, {
        noScroll: true
      });
    }
    this.eventBus.dispatch("rotationchanging", {
      source: this,
      pagesRotation: rotation,
      pageNumber
    });
    if (this.defaultRenderingQueue) {
      this.update();
    }
  }
  get firstPagePromise() {
    return this.pdfDocument ? this._firstPageCapability.promise : null;
  }
  get onePageRendered() {
    return this.pdfDocument ? this._onePageRenderedCapability.promise : null;
  }
  get pagesPromise() {
    return this.pdfDocument ? this._pagesCapability.promise : null;
  }
  get _layerProperties() {
    const self = this;
    return shadow(this, "_layerProperties", {
      get annotationEditorUIManager() {
        return self.#annotationEditorUIManager;
      },
      get annotationStorage() {
        return self.pdfDocument?.annotationStorage;
      },
      get downloadManager() {
        return self.downloadManager;
      },
      get enableScripting() {
        return !!self._scriptingManager;
      },
      get fieldObjectsPromise() {
        return self.pdfDocument?.getFieldObjects();
      },
      get findController() {
        return self.findController;
      },
      get hasJSActionsPromise() {
        return self.pdfDocument?.hasJSActions();
      },
      get linkService() {
        return self.linkService;
      }
    });
  }
  #initializePermissions(permissions) {
    const params = {
      annotationEditorMode: this.#annotationEditorMode,
      annotationMode: this.#annotationMode,
      textLayerMode: this.#textLayerMode
    };
    if (!permissions) {
      return params;
    }
    if (!permissions.includes(PermissionFlag.COPY) && this.#textLayerMode === TextLayerMode.ENABLE) {
      params.textLayerMode = TextLayerMode.ENABLE_PERMISSIONS;
    }
    if (!permissions.includes(PermissionFlag.MODIFY_CONTENTS)) {
      params.annotationEditorMode = AnnotationEditorType.DISABLE;
    }
    if (!permissions.includes(PermissionFlag.MODIFY_ANNOTATIONS) && !permissions.includes(PermissionFlag.FILL_INTERACTIVE_FORMS) && this.#annotationMode === AnnotationMode.ENABLE_FORMS) {
      params.annotationMode = AnnotationMode.ENABLE;
    }
    return params;
  }
  #onePageRenderedOrForceFetch() {
    if (document.visibilityState === "hidden" || !this.container.offsetParent || this._getVisiblePages().views.length === 0) {
      return Promise.resolve();
    }
    const visibilityChangePromise = new Promise(resolve => {
      this.#onVisibilityChange = () => {
        if (document.visibilityState !== "hidden") {
          return;
        }
        resolve();
        document.removeEventListener("visibilitychange", this.#onVisibilityChange);
        this.#onVisibilityChange = null;
      };
      document.addEventListener("visibilitychange", this.#onVisibilityChange);
    });
    return Promise.race([this._onePageRenderedCapability.promise, visibilityChangePromise]);
  }
  async getAllText() {
    const texts = [];
    const buffer = [];
    for (let pageNum = 1, pagesCount = this.pdfDocument.numPages; pageNum <= pagesCount; ++pageNum) {
      if (this.#interruptCopyCondition) {
        return null;
      }
      buffer.length = 0;
      const page = await this.pdfDocument.getPage(pageNum);
      const {
        items
      } = await page.getTextContent();
      for (const item of items) {
        if (item.str) {
          buffer.push(item.str);
        }
        if (item.hasEOL) {
          buffer.push("\n");
        }
      }
      texts.push(removeNullCharacters(buffer.join("")));
    }
    return texts.join("\n");
  }
  #copyCallback(textLayerMode, event) {
    const selection = document.getSelection();
    const {
      focusNode,
      anchorNode
    } = selection;
    if (anchorNode && focusNode && selection.containsNode(this.#hiddenCopyElement)) {
      if (this.#getAllTextInProgress || textLayerMode === TextLayerMode.ENABLE_PERMISSIONS) {
        event.preventDefault();
        event.stopPropagation();
        return;
      }
      this.#getAllTextInProgress = true;
      const savedCursor = this.container.style.cursor;
      this.container.style.cursor = "wait";
      const interruptCopy = ev => this.#interruptCopyCondition = ev.key === "Escape";
      window.addEventListener("keydown", interruptCopy);
      this.getAllText().then(async text => {
        if (text !== null) {
          await navigator.clipboard.writeText(text);
        }
      }).catch(reason => {
        console.warn(`Something goes wrong when extracting the text: ${reason.message}`);
      }).finally(() => {
        this.#getAllTextInProgress = false;
        this.#interruptCopyCondition = false;
        window.removeEventListener("keydown", interruptCopy);
        this.container.style.cursor = savedCursor;
      });
      event.preventDefault();
      event.stopPropagation();
    }
  }
  setDocument(pdfDocument) {
    if (this.pdfDocument) {
      this.eventBus.dispatch("pagesdestroy", {
        source: this
      });
      this._cancelRendering();
      this._resetView();
      this.findController?.setDocument(null);
      this._scriptingManager?.setDocument(null);
      if (this.#annotationEditorUIManager) {
        this.#annotationEditorUIManager.destroy();
        this.#annotationEditorUIManager = null;
      }
    }
    this.pdfDocument = pdfDocument;
    if (!pdfDocument) {
      return;
    }
    const pagesCount = pdfDocument.numPages;
    const firstPagePromise = pdfDocument.getPage(1);
    const optionalContentConfigPromise = pdfDocument.getOptionalContentConfig();
    const permissionsPromise = this.#enablePermissions ? pdfDocument.getPermissions() : Promise.resolve();
    if (pagesCount > PagesCountLimit.FORCE_SCROLL_MODE_PAGE) {
      console.warn("Forcing PAGE-scrolling for performance reasons, given the length of the document.");
      const mode = this._scrollMode = ScrollMode.PAGE;
      this.eventBus.dispatch("scrollmodechanged", {
        source: this,
        mode
      });
    }
    this._pagesCapability.promise.then(() => {
      this.eventBus.dispatch("pagesloaded", {
        source: this,
        pagesCount
      });
    }, () => {});
    this._onBeforeDraw = evt => {
      const pageView = this._pages[evt.pageNumber - 1];
      if (!pageView) {
        return;
      }
      this.#buffer.push(pageView);
    };
    this.eventBus._on("pagerender", this._onBeforeDraw);
    this._onAfterDraw = evt => {
      if (evt.cssTransform || this._onePageRenderedCapability.settled) {
        return;
      }
      this._onePageRenderedCapability.resolve({
        timestamp: evt.timestamp
      });
      this.eventBus._off("pagerendered", this._onAfterDraw);
      this._onAfterDraw = null;
      if (this.#onVisibilityChange) {
        document.removeEventListener("visibilitychange", this.#onVisibilityChange);
        this.#onVisibilityChange = null;
      }
    };
    this.eventBus._on("pagerendered", this._onAfterDraw);
    Promise.all([firstPagePromise, permissionsPromise]).then(([firstPdfPage, permissions]) => {
      if (pdfDocument !== this.pdfDocument) {
        return;
      }
      this._firstPageCapability.resolve(firstPdfPage);
      this._optionalContentConfigPromise = optionalContentConfigPromise;
      const {
        annotationEditorMode,
        annotationMode,
        textLayerMode
      } = this.#initializePermissions(permissions);
      if (textLayerMode !== TextLayerMode.DISABLE) {
        const element = this.#hiddenCopyElement = document.createElement("div");
        element.id = "hiddenCopyElement";
        this.viewer.before(element);
      }
      if (annotationEditorMode !== AnnotationEditorType.DISABLE) {
        const mode = annotationEditorMode;
        if (pdfDocument.isPureXfa) {
          console.warn("Warning: XFA-editing is not implemented.");
        } else if (isValidAnnotationEditorMode(mode)) {
          this.#annotationEditorUIManager = new AnnotationEditorUIManager(this.container, this.viewer, this.#altTextManager, this.eventBus, pdfDocument, this.pageColors);
          if (mode !== AnnotationEditorType.NONE) {
            this.#annotationEditorUIManager.updateMode(mode);
          }
        } else {
          console.error(`Invalid AnnotationEditor mode: ${mode}`);
        }
      }
      const viewerElement = this._scrollMode === ScrollMode.PAGE ? null : this.viewer;
      const scale = this.currentScale;
      const viewport = firstPdfPage.getViewport({
        scale: scale * PixelsPerInch.PDF_TO_CSS_UNITS
      });
      this.viewer.style.setProperty("--scale-factor", viewport.scale);
      if (this.pageColors?.foreground === "CanvasText" || this.pageColors?.background === "Canvas") {
        this.viewer.style.setProperty("--hcm-highlight-filter", pdfDocument.filterFactory.addHighlightHCMFilter("CanvasText", "Canvas", "HighlightText", "Highlight"));
      }
      for (let pageNum = 1; pageNum <= pagesCount; ++pageNum) {
        const pageView = new PDFPageView({
          container: viewerElement,
          eventBus: this.eventBus,
          id: pageNum,
          scale,
          defaultViewport: viewport.clone(),
          optionalContentConfigPromise,
          renderingQueue: this.renderingQueue,
          textLayerMode,
          annotationMode,
          imageResourcesPath: this.imageResourcesPath,
          isOffscreenCanvasSupported: this.isOffscreenCanvasSupported,
          maxCanvasPixels: this.maxCanvasPixels,
          pageColors: this.pageColors,
          l10n: this.l10n,
          layerProperties: this._layerProperties
        });
        this._pages.push(pageView);
      }
      const firstPageView = this._pages[0];
      if (firstPageView) {
        firstPageView.setPdfPage(firstPdfPage);
        this.linkService.cachePageRef(1, firstPdfPage.ref);
      }
      if (this._scrollMode === ScrollMode.PAGE) {
        this.#ensurePageViewVisible();
      } else if (this._spreadMode !== SpreadMode.NONE) {
        this._updateSpreadMode();
      }
      this.#onePageRenderedOrForceFetch().then(async () => {
        this.findController?.setDocument(pdfDocument);
        this._scriptingManager?.setDocument(pdfDocument);
        if (this.#hiddenCopyElement) {
          this.#copyCallbackBound = this.#copyCallback.bind(this, textLayerMode);
          document.addEventListener("copy", this.#copyCallbackBound);
        }
        if (this.#annotationEditorUIManager) {
          this.eventBus.dispatch("annotationeditormodechanged", {
            source: this,
            mode: this.#annotationEditorMode
          });
        }
        if (pdfDocument.loadingParams.disableAutoFetch || pagesCount > PagesCountLimit.FORCE_LAZY_PAGE_INIT) {
          this._pagesCapability.resolve();
          return;
        }
        let getPagesLeft = pagesCount - 1;
        if (getPagesLeft <= 0) {
          this._pagesCapability.resolve();
          return;
        }
        for (let pageNum = 2; pageNum <= pagesCount; ++pageNum) {
          const promise = pdfDocument.getPage(pageNum).then(pdfPage => {
            const pageView = this._pages[pageNum - 1];
            if (!pageView.pdfPage) {
              pageView.setPdfPage(pdfPage);
            }
            this.linkService.cachePageRef(pageNum, pdfPage.ref);
            if (--getPagesLeft === 0) {
              this._pagesCapability.resolve();
            }
          }, reason => {
            console.error(`Unable to get page ${pageNum} to initialize viewer`, reason);
            if (--getPagesLeft === 0) {
              this._pagesCapability.resolve();
            }
          });
          if (pageNum % PagesCountLimit.PAUSE_EAGER_PAGE_INIT === 0) {
            await promise;
          }
        }
      });
      this.eventBus.dispatch("pagesinit", {
        source: this
      });
      pdfDocument.getMetadata().then(({
        info
      }) => {
        if (pdfDocument !== this.pdfDocument) {
          return;
        }
        if (info.Language) {
          this.viewer.lang = info.Language;
        }
      });
      if (this.defaultRenderingQueue) {
        this.update();
      }
    }).catch(reason => {
      console.error("Unable to initialize viewer", reason);
      this._pagesCapability.reject(reason);
    });
  }
  setPageLabels(labels) {
    if (!this.pdfDocument) {
      return;
    }
    if (!labels) {
      this._pageLabels = null;
    } else if (!(Array.isArray(labels) && this.pdfDocument.numPages === labels.length)) {
      this._pageLabels = null;
      console.error(`setPageLabels: Invalid page labels.`);
    } else {
      this._pageLabels = labels;
    }
    for (let i = 0, ii = this._pages.length; i < ii; i++) {
      this._pages[i].setPageLabel(this._pageLabels?.[i] ?? null);
    }
  }
  _resetView() {
    this._pages = [];
    this._currentPageNumber = 1;
    this._currentScale = UNKNOWN_SCALE;
    this._currentScaleValue = null;
    this._pageLabels = null;
    this.#buffer = new PDFPageViewBuffer(DEFAULT_CACHE_SIZE);
    this._location = null;
    this._pagesRotation = 0;
    this._optionalContentConfigPromise = null;
    this._firstPageCapability = new PromiseCapability();
    this._onePageRenderedCapability = new PromiseCapability();
    this._pagesCapability = new PromiseCapability();
    this._scrollMode = ScrollMode.VERTICAL;
    this._previousScrollMode = ScrollMode.UNKNOWN;
    this._spreadMode = SpreadMode.NONE;
    this.#scrollModePageState = {
      previousPageNumber: 1,
      scrollDown: true,
      pages: []
    };
    if (this._onBeforeDraw) {
      this.eventBus._off("pagerender", this._onBeforeDraw);
      this._onBeforeDraw = null;
    }
    if (this._onAfterDraw) {
      this.eventBus._off("pagerendered", this._onAfterDraw);
      this._onAfterDraw = null;
    }
    if (this.#onVisibilityChange) {
      document.removeEventListener("visibilitychange", this.#onVisibilityChange);
      this.#onVisibilityChange = null;
    }
    this.viewer.textContent = "";
    this._updateScrollMode();
    this.viewer.removeAttribute("lang");
    if (this.#hiddenCopyElement) {
      document.removeEventListener("copy", this.#copyCallbackBound);
      this.#copyCallbackBound = null;
      this.#hiddenCopyElement.remove();
      this.#hiddenCopyElement = null;
    }
  }
  #ensurePageViewVisible() {
    if (this._scrollMode !== ScrollMode.PAGE) {
      throw new Error("#ensurePageViewVisible: Invalid scrollMode value.");
    }
    const pageNumber = this._currentPageNumber,
      state = this.#scrollModePageState,
      viewer = this.viewer;
    viewer.textContent = "";
    state.pages.length = 0;
    if (this._spreadMode === SpreadMode.NONE && !this.isInPresentationMode) {
      const pageView = this._pages[pageNumber - 1];
      viewer.append(pageView.div);
      state.pages.push(pageView);
    } else {
      const pageIndexSet = new Set(),
        parity = this._spreadMode - 1;
      if (parity === -1) {
        pageIndexSet.add(pageNumber - 1);
      } else if (pageNumber % 2 !== parity) {
        pageIndexSet.add(pageNumber - 1);
        pageIndexSet.add(pageNumber);
      } else {
        pageIndexSet.add(pageNumber - 2);
        pageIndexSet.add(pageNumber - 1);
      }
      const spread = document.createElement("div");
      spread.className = "spread";
      if (this.isInPresentationMode) {
        const dummyPage = document.createElement("div");
        dummyPage.className = "dummyPage";
        spread.append(dummyPage);
      }
      for (const i of pageIndexSet) {
        const pageView = this._pages[i];
        if (!pageView) {
          continue;
        }
        spread.append(pageView.div);
        state.pages.push(pageView);
      }
      viewer.append(spread);
    }
    state.scrollDown = pageNumber >= state.previousPageNumber;
    state.previousPageNumber = pageNumber;
  }
  _scrollUpdate() {
    if (this.pagesCount === 0) {
      return;
    }
    this.update();
  }
  #scrollIntoView(pageView, pageSpot = null) {
    const {
      div,
      id
    } = pageView;
    if (this._currentPageNumber !== id) {
      this._setCurrentPageNumber(id);
    }
    if (this._scrollMode === ScrollMode.PAGE) {
      this.#ensurePageViewVisible();
      this.update();
    }
    if (!pageSpot && !this.isInPresentationMode) {
      const left = div.offsetLeft + div.clientLeft,
        right = left + div.clientWidth;
      const {
        scrollLeft,
        clientWidth
      } = this.container;
      if (this._scrollMode === ScrollMode.HORIZONTAL || left < scrollLeft || right > scrollLeft + clientWidth) {
        pageSpot = {
          left: 0,
          top: 0
        };
      }
    }
    scrollIntoView(div, pageSpot);
    if (!this._currentScaleValue && this._location) {
      this._location = null;
    }
  }
  #isSameScale(newScale) {
    return newScale === this._currentScale || Math.abs(newScale - this._currentScale) < 1e-15;
  }
  #setScaleUpdatePages(newScale, newValue, {
    noScroll = false,
    preset = false,
    drawingDelay = -1
  }) {
    this._currentScaleValue = newValue.toString();
    if (this.#isSameScale(newScale)) {
      if (preset) {
        this.eventBus.dispatch("scalechanging", {
          source: this,
          scale: newScale,
          presetValue: newValue
        });
      }
      return;
    }
    this.viewer.style.setProperty("--scale-factor", newScale * PixelsPerInch.PDF_TO_CSS_UNITS);
    const postponeDrawing = drawingDelay >= 0 && drawingDelay < 1000;
    this.refresh(true, {
      scale: newScale,
      drawingDelay: postponeDrawing ? drawingDelay : -1
    });
    if (postponeDrawing) {
      this.#scaleTimeoutId = setTimeout(() => {
        this.#scaleTimeoutId = null;
        this.refresh();
      }, drawingDelay);
    }
    this._currentScale = newScale;
    if (!noScroll) {
      let page = this._currentPageNumber,
        dest;
      if (this._location && !(this.isInPresentationMode || this.isChangingPresentationMode)) {
        page = this._location.pageNumber;
        dest = [null, {
          name: "XYZ"
        }, this._location.left, this._location.top, null];
      }
      this.scrollPageIntoView({
        pageNumber: page,
        destArray: dest,
        allowNegativeOffset: true
      });
    }
    this.eventBus.dispatch("scalechanging", {
      source: this,
      scale: newScale,
      presetValue: preset ? newValue : undefined
    });
    if (this.defaultRenderingQueue) {
      this.update();
    }
  }
  get #pageWidthScaleFactor() {
    if (this._spreadMode !== SpreadMode.NONE && this._scrollMode !== ScrollMode.HORIZONTAL) {
      return 2;
    }
    return 1;
  }
  #setScale(value, options) {
    let scale = parseFloat(value);
    if (scale > 0) {
      options.preset = false;
      this.#setScaleUpdatePages(scale, value, options);
    } else {
      const currentPage = this._pages[this._currentPageNumber - 1];
      if (!currentPage) {
        return;
      }
      let hPadding = SCROLLBAR_PADDING,
        vPadding = VERTICAL_PADDING;
      if (this.isInPresentationMode) {
        hPadding = vPadding = 4;
        if (this._spreadMode !== SpreadMode.NONE) {
          hPadding *= 2;
        }
      } else if (this._scrollMode === ScrollMode.HORIZONTAL) {
        [hPadding, vPadding] = [vPadding, hPadding];
      }
      const pageWidthScale = (this.container.clientWidth - hPadding) / currentPage.width * currentPage.scale / this.#pageWidthScaleFactor;
      const pageHeightScale = (this.container.clientHeight - vPadding) / currentPage.height * currentPage.scale;
      switch (value) {
        case "page-actual":
          scale = 1;
          break;
        case "page-width":
          scale = pageWidthScale;
          break;
        case "page-height":
          scale = pageHeightScale;
          break;
        case "page-fit":
          scale = Math.min(pageWidthScale, pageHeightScale);
          break;
        case "auto":
          const horizontalScale = isPortraitOrientation(currentPage) ? pageWidthScale : Math.min(pageHeightScale, pageWidthScale);
          scale = Math.min(MAX_AUTO_SCALE, horizontalScale);
          break;
        default:
          console.error(`#setScale: "${value}" is an unknown zoom value.`);
          return;
      }
      options.preset = true;
      this.#setScaleUpdatePages(scale, value, options);
    }
  }
  #resetCurrentPageView() {
    const pageView = this._pages[this._currentPageNumber - 1];
    if (this.isInPresentationMode) {
      this.#setScale(this._currentScaleValue, {
        noScroll: true
      });
    }
    this.#scrollIntoView(pageView);
  }
  pageLabelToPageNumber(label) {
    if (!this._pageLabels) {
      return null;
    }
    const i = this._pageLabels.indexOf(label);
    if (i < 0) {
      return null;
    }
    return i + 1;
  }
  scrollPageIntoView({
    pageNumber,
    destArray = null,
    allowNegativeOffset = false,
    ignoreDestinationZoom = false
  }) {
    if (!this.pdfDocument) {
      return;
    }
    const pageView = Number.isInteger(pageNumber) && this._pages[pageNumber - 1];
    if (!pageView) {
      console.error(`scrollPageIntoView: "${pageNumber}" is not a valid pageNumber parameter.`);
      return;
    }
    if (this.isInPresentationMode || !destArray) {
      this._setCurrentPageNumber(pageNumber, true);
      return;
    }
    let x = 0,
      y = 0;
    let width = 0,
      height = 0,
      widthScale,
      heightScale;
    const changeOrientation = pageView.rotation % 180 !== 0;
    const pageWidth = (changeOrientation ? pageView.height : pageView.width) / pageView.scale / PixelsPerInch.PDF_TO_CSS_UNITS;
    const pageHeight = (changeOrientation ? pageView.width : pageView.height) / pageView.scale / PixelsPerInch.PDF_TO_CSS_UNITS;
    let scale = 0;
    switch (destArray[1].name) {
      case "XYZ":
        x = destArray[2];
        y = destArray[3];
        scale = destArray[4];
        x = x !== null ? x : 0;
        y = y !== null ? y : pageHeight;
        break;
      case "Fit":
      case "FitB":
        scale = "page-fit";
        break;
      case "FitH":
      case "FitBH":
        y = destArray[2];
        scale = "page-width";
        if (y === null && this._location) {
          x = this._location.left;
          y = this._location.top;
        } else if (typeof y !== "number" || y < 0) {
          y = pageHeight;
        }
        break;
      case "FitV":
      case "FitBV":
        x = destArray[2];
        width = pageWidth;
        height = pageHeight;
        scale = "page-height";
        break;
      case "FitR":
        x = destArray[2];
        y = destArray[3];
        width = destArray[4] - x;
        height = destArray[5] - y;
        let hPadding = SCROLLBAR_PADDING,
          vPadding = VERTICAL_PADDING;
        ;
        widthScale = (this.container.clientWidth - hPadding) / width / PixelsPerInch.PDF_TO_CSS_UNITS;
        heightScale = (this.container.clientHeight - vPadding) / height / PixelsPerInch.PDF_TO_CSS_UNITS;
        scale = Math.min(Math.abs(widthScale), Math.abs(heightScale));
        break;
      default:
        console.error(`scrollPageIntoView: "${destArray[1].name}" is not a valid destination type.`);
        return;
    }
    if (!ignoreDestinationZoom) {
      if (scale && scale !== this._currentScale) {
        this.currentScaleValue = scale;
      } else if (this._currentScale === UNKNOWN_SCALE) {
        this.currentScaleValue = DEFAULT_SCALE_VALUE;
      }
    }
    if (scale === "page-fit" && !destArray[4]) {
      this.#scrollIntoView(pageView);
      return;
    }
    const boundingRect = [pageView.viewport.convertToViewportPoint(x, y), pageView.viewport.convertToViewportPoint(x + width, y + height)];
    let left = Math.min(boundingRect[0][0], boundingRect[1][0]);
    let top = Math.min(boundingRect[0][1], boundingRect[1][1]);
    if (!allowNegativeOffset) {
      left = Math.max(left, 0);
      top = Math.max(top, 0);
    }
    this.#scrollIntoView(pageView, {
      left,
      top
    });
  }
  _updateLocation(firstPage) {
    const currentScale = this._currentScale;
    const currentScaleValue = this._currentScaleValue;
    const normalizedScaleValue = parseFloat(currentScaleValue) === currentScale ? Math.round(currentScale * 10000) / 100 : currentScaleValue;
    const pageNumber = firstPage.id;
    const currentPageView = this._pages[pageNumber - 1];
    const container = this.container;
    const topLeft = currentPageView.getPagePoint(container.scrollLeft - firstPage.x, container.scrollTop - firstPage.y);
    const intLeft = Math.round(topLeft[0]);
    const intTop = Math.round(topLeft[1]);
    let pdfOpenParams = `#page=${pageNumber}`;
    if (!this.isInPresentationMode) {
      pdfOpenParams += `&zoom=${normalizedScaleValue},${intLeft},${intTop}`;
    }
    this._location = {
      pageNumber,
      scale: normalizedScaleValue,
      top: intTop,
      left: intLeft,
      rotation: this._pagesRotation,
      pdfOpenParams
    };
  }
  update() {
    const visible = this._getVisiblePages();
    const visiblePages = visible.views,
      numVisiblePages = visiblePages.length;
    if (numVisiblePages === 0) {
      return;
    }
    const newCacheSize = Math.max(DEFAULT_CACHE_SIZE, 2 * numVisiblePages + 1);
    this.#buffer.resize(newCacheSize, visible.ids);
    this.renderingQueue.renderHighestPriority(visible);
    const isSimpleLayout = this._spreadMode === SpreadMode.NONE && (this._scrollMode === ScrollMode.PAGE || this._scrollMode === ScrollMode.VERTICAL);
    const currentId = this._currentPageNumber;
    let stillFullyVisible = false;
    for (const page of visiblePages) {
      if (page.percent < 100) {
        break;
      }
      if (page.id === currentId && isSimpleLayout) {
        stillFullyVisible = true;
        break;
      }
    }
    this._setCurrentPageNumber(stillFullyVisible ? currentId : visiblePages[0].id);
    this._updateLocation(visible.first);
    this.eventBus.dispatch("updateviewarea", {
      source: this,
      location: this._location
    });
  }
  containsElement(element) {
    return this.container.contains(element);
  }
  focus() {
    this.container.focus();
  }
  get _isContainerRtl() {
    return getComputedStyle(this.container).direction === "rtl";
  }
  get isInPresentationMode() {
    return this.presentationModeState === PresentationModeState.FULLSCREEN;
  }
  get isChangingPresentationMode() {
    return this.presentationModeState === PresentationModeState.CHANGING;
  }
  get isHorizontalScrollbarEnabled() {
    return this.isInPresentationMode ? false : this.container.scrollWidth > this.container.clientWidth;
  }
  get isVerticalScrollbarEnabled() {
    return this.isInPresentationMode ? false : this.container.scrollHeight > this.container.clientHeight;
  }
  _getVisiblePages() {
    const views = this._scrollMode === ScrollMode.PAGE ? this.#scrollModePageState.pages : this._pages,
      horizontal = this._scrollMode === ScrollMode.HORIZONTAL,
      rtl = horizontal && this._isContainerRtl;
    return getVisibleElements({
      scrollEl: this.container,
      views,
      sortByVisibility: true,
      horizontal,
      rtl
    });
  }
  cleanup() {
    for (const pageView of this._pages) {
      if (pageView.renderingState !== RenderingStates.FINISHED) {
        pageView.reset();
      }
    }
  }
  _cancelRendering() {
    for (const pageView of this._pages) {
      pageView.cancelRendering();
    }
  }
  async #ensurePdfPageLoaded(pageView) {
    if (pageView.pdfPage) {
      return pageView.pdfPage;
    }
    try {
      const pdfPage = await this.pdfDocument.getPage(pageView.id);
      if (!pageView.pdfPage) {
        pageView.setPdfPage(pdfPage);
      }
      if (!this.linkService._cachedPageNumber?.(pdfPage.ref)) {
        this.linkService.cachePageRef(pageView.id, pdfPage.ref);
      }
      return pdfPage;
    } catch (reason) {
      console.error("Unable to get page for page view", reason);
      return null;
    }
  }
  #getScrollAhead(visible) {
    if (visible.first?.id === 1) {
      return true;
    } else if (visible.last?.id === this.pagesCount) {
      return false;
    }
    switch (this._scrollMode) {
      case ScrollMode.PAGE:
        return this.#scrollModePageState.scrollDown;
      case ScrollMode.HORIZONTAL:
        return this.scroll.right;
    }
    return this.scroll.down;
  }
  forceRendering(currentlyVisiblePages) {
    const visiblePages = currentlyVisiblePages || this._getVisiblePages();
    const scrollAhead = this.#getScrollAhead(visiblePages);
    const preRenderExtra = this._spreadMode !== SpreadMode.NONE && this._scrollMode !== ScrollMode.HORIZONTAL;
    const pageView = this.renderingQueue.getHighestPriority(visiblePages, this._pages, scrollAhead, preRenderExtra);
    if (pageView) {
      this.#ensurePdfPageLoaded(pageView).then(() => {
        this.renderingQueue.renderView(pageView);
      });
      return true;
    }
    return false;
  }
  get hasEqualPageSizes() {
    const firstPageView = this._pages[0];
    for (let i = 1, ii = this._pages.length; i < ii; ++i) {
      const pageView = this._pages[i];
      if (pageView.width !== firstPageView.width || pageView.height !== firstPageView.height) {
        return false;
      }
    }
    return true;
  }
  getPagesOverview() {
    let initialOrientation;
    return this._pages.map(pageView => {
      const viewport = pageView.pdfPage.getViewport({
        scale: 1
      });
      const orientation = isPortraitOrientation(viewport);
      if (initialOrientation === undefined) {
        initialOrientation = orientation;
      } else if (this.enablePrintAutoRotate && orientation !== initialOrientation) {
        return {
          width: viewport.height,
          height: viewport.width,
          rotation: (viewport.rotation - 90) % 360
        };
      }
      return {
        width: viewport.width,
        height: viewport.height,
        rotation: viewport.rotation
      };
    });
  }
  get optionalContentConfigPromise() {
    if (!this.pdfDocument) {
      return Promise.resolve(null);
    }
    if (!this._optionalContentConfigPromise) {
      console.error("optionalContentConfigPromise: Not initialized yet.");
      return this.pdfDocument.getOptionalContentConfig();
    }
    return this._optionalContentConfigPromise;
  }
  set optionalContentConfigPromise(promise) {
    if (!(promise instanceof Promise)) {
      throw new Error(`Invalid optionalContentConfigPromise: ${promise}`);
    }
    if (!this.pdfDocument) {
      return;
    }
    if (!this._optionalContentConfigPromise) {
      return;
    }
    this._optionalContentConfigPromise = promise;
    this.refresh(false, {
      optionalContentConfigPromise: promise
    });
    this.eventBus.dispatch("optionalcontentconfigchanged", {
      source: this,
      promise
    });
  }
  get scrollMode() {
    return this._scrollMode;
  }
  set scrollMode(mode) {}
  _updateScrollMode(pageNumber = null) {
    const scrollMode = this._scrollMode,
      viewer = this.viewer;
    viewer.classList.toggle("scrollHorizontal", scrollMode === ScrollMode.HORIZONTAL);
    viewer.classList.toggle("scrollWrapped", scrollMode === ScrollMode.WRAPPED);
    if (!this.pdfDocument || !pageNumber) {
      return;
    }
    if (scrollMode === ScrollMode.PAGE) {
      this.#ensurePageViewVisible();
    } else if (this._previousScrollMode === ScrollMode.PAGE) {
      this._updateSpreadMode();
    }
    if (this._currentScaleValue && isNaN(this._currentScaleValue)) {
      this.#setScale(this._currentScaleValue, {
        noScroll: true
      });
    }
    this._setCurrentPageNumber(pageNumber, true);
    this.update();
  }
  get spreadMode() {
    return this._spreadMode;
  }
  set spreadMode(mode) {}
  _updateSpreadMode(pageNumber = null) {
    if (!this.pdfDocument) {
      return;
    }
    const viewer = this.viewer,
      pages = this._pages;
    if (this._scrollMode === ScrollMode.PAGE) {
      this.#ensurePageViewVisible();
    } else {
      viewer.textContent = "";
      if (this._spreadMode === SpreadMode.NONE) {
        for (const pageView of this._pages) {
          viewer.append(pageView.div);
        }
      } else {
        const parity = this._spreadMode - 1;
        let spread = null;
        for (let i = 0, ii = pages.length; i < ii; ++i) {
          if (spread === null) {
            spread = document.createElement("div");
            spread.className = "spread";
            viewer.append(spread);
          } else if (i % 2 === parity) {
            spread = spread.cloneNode(false);
            viewer.append(spread);
          }
          spread.append(pages[i].div);
        }
      }
    }
    if (!pageNumber) {
      return;
    }
    if (this._currentScaleValue && isNaN(this._currentScaleValue)) {
      this.#setScale(this._currentScaleValue, {
        noScroll: true
      });
    }
    this._setCurrentPageNumber(pageNumber, true);
    this.update();
  }
  _getPageAdvance(currentPageNumber, previous = false) {
    switch (this._scrollMode) {
      case ScrollMode.WRAPPED:
        {
          const {
              views
            } = this._getVisiblePages(),
            pageLayout = new Map();
          for (const {
            id,
            y,
            percent,
            widthPercent
          } of views) {
            if (percent === 0 || widthPercent < 100) {
              continue;
            }
            let yArray = pageLayout.get(y);
            if (!yArray) {
              pageLayout.set(y, yArray ||= []);
            }
            yArray.push(id);
          }
          for (const yArray of pageLayout.values()) {
            const currentIndex = yArray.indexOf(currentPageNumber);
            if (currentIndex === -1) {
              continue;
            }
            const numPages = yArray.length;
            if (numPages === 1) {
              break;
            }
            if (previous) {
              for (let i = currentIndex - 1, ii = 0; i >= ii; i--) {
                const currentId = yArray[i],
                  expectedId = yArray[i + 1] - 1;
                if (currentId < expectedId) {
                  return currentPageNumber - expectedId;
                }
              }
            } else {
              for (let i = currentIndex + 1, ii = numPages; i < ii; i++) {
                const currentId = yArray[i],
                  expectedId = yArray[i - 1] + 1;
                if (currentId > expectedId) {
                  return expectedId - currentPageNumber;
                }
              }
            }
            if (previous) {
              const firstId = yArray[0];
              if (firstId < currentPageNumber) {
                return currentPageNumber - firstId + 1;
              }
            } else {
              const lastId = yArray[numPages - 1];
              if (lastId > currentPageNumber) {
                return lastId - currentPageNumber + 1;
              }
            }
            break;
          }
          break;
        }
      case ScrollMode.HORIZONTAL:
        {
          break;
        }
      case ScrollMode.PAGE:
      case ScrollMode.VERTICAL:
        {
          if (this._spreadMode === SpreadMode.NONE) {
            break;
          }
          const parity = this._spreadMode - 1;
          if (previous && currentPageNumber % 2 !== parity) {
            break;
          } else if (!previous && currentPageNumber % 2 === parity) {
            break;
          }
          const {
              views
            } = this._getVisiblePages(),
            expectedId = previous ? currentPageNumber - 1 : currentPageNumber + 1;
          for (const {
            id,
            percent,
            widthPercent
          } of views) {
            if (id !== expectedId) {
              continue;
            }
            if (percent > 0 && widthPercent === 100) {
              return 2;
            }
            break;
          }
          break;
        }
    }
    return 1;
  }
  nextPage() {
    const currentPageNumber = this._currentPageNumber,
      pagesCount = this.pagesCount;
    if (currentPageNumber >= pagesCount) {
      return false;
    }
    const advance = this._getPageAdvance(currentPageNumber, false) || 1;
    this.currentPageNumber = Math.min(currentPageNumber + advance, pagesCount);
    return true;
  }
  previousPage() {
    const currentPageNumber = this._currentPageNumber;
    if (currentPageNumber <= 1) {
      return false;
    }
    const advance = this._getPageAdvance(currentPageNumber, true) || 1;
    this.currentPageNumber = Math.max(currentPageNumber - advance, 1);
    return true;
  }
  increaseScale({
    drawingDelay,
    scaleFactor,
    steps
  } = {}) {
    if (!this.pdfDocument) {
      return;
    }
    let newScale = this._currentScale;
    if (scaleFactor > 1) {
      newScale = Math.round(newScale * scaleFactor * 100) / 100;
    } else {
      steps ??= 1;
      do {
        newScale = Math.ceil((newScale * DEFAULT_SCALE_DELTA).toFixed(2) * 10) / 10;
      } while (--steps > 0 && newScale < MAX_SCALE);
    }
    this.#setScale(Math.min(MAX_SCALE, newScale), {
      noScroll: false,
      drawingDelay
    });
  }
  decreaseScale({
    drawingDelay,
    scaleFactor,
    steps
  } = {}) {
    if (!this.pdfDocument) {
      return;
    }
    let newScale = this._currentScale;
    if (scaleFactor > 0 && scaleFactor < 1) {
      newScale = Math.round(newScale * scaleFactor * 100) / 100;
    } else {
      steps ??= 1;
      do {
        newScale = Math.floor((newScale / DEFAULT_SCALE_DELTA).toFixed(2) * 10) / 10;
      } while (--steps > 0 && newScale > MIN_SCALE);
    }
    this.#setScale(Math.max(MIN_SCALE, newScale), {
      noScroll: false,
      drawingDelay
    });
  }
  #updateContainerHeightCss(height = this.container.clientHeight) {
    if (height !== this.#previousContainerHeight) {
      this.#previousContainerHeight = height;
      docStyle.setProperty("--viewer-container-height", `${height}px`);
    }
  }
  #resizeObserverCallback(entries) {
    for (const entry of entries) {
      if (entry.target === this.container) {
        this.#updateContainerHeightCss(Math.floor(entry.borderBoxSize[0].blockSize));
        this.#containerTopLeft = null;
        break;
      }
    }
  }
  get containerTopLeft() {
    return this.#containerTopLeft ||= [this.container.offsetTop, this.container.offsetLeft];
  }
  get annotationEditorMode() {
    return this.#annotationEditorUIManager ? this.#annotationEditorMode : AnnotationEditorType.DISABLE;
  }
  set annotationEditorMode({
    mode,
    editId = null,
    isFromKeyboard = false
  }) {
    if (!this.#annotationEditorUIManager) {
      throw new Error(`The AnnotationEditor is not enabled.`);
    }
    if (this.#annotationEditorMode === mode) {
      return;
    }
    if (!isValidAnnotationEditorMode(mode)) {
      throw new Error(`Invalid AnnotationEditor mode: ${mode}`);
    }
    if (!this.pdfDocument) {
      return;
    }
    this.#annotationEditorMode = mode;
    this.eventBus.dispatch("annotationeditormodechanged", {
      source: this,
      mode
    });
    this.#annotationEditorUIManager.updateMode(mode, editId, isFromKeyboard);
  }
  set annotationEditorParams({
    type,
    value
  }) {
    if (!this.#annotationEditorUIManager) {
      throw new Error(`The AnnotationEditor is not enabled.`);
    }
    this.#annotationEditorUIManager.updateParams(type, value);
  }
  refresh(noUpdate = false, updateArgs = Object.create(null)) {
    if (!this.pdfDocument) {
      return;
    }
    for (const pageView of this._pages) {
      pageView.update(updateArgs);
    }
    if (this.#scaleTimeoutId !== null) {
      clearTimeout(this.#scaleTimeoutId);
      this.#scaleTimeoutId = null;
    }
    if (!noUpdate) {
      this.update();
    }
  }
}

;// CONCATENATED MODULE: ./web/toolbar-geckoview.js
class Toolbar {
  #buttons;
  #eventBus;
  constructor(options, eventBus, nimbusData) {
    this.#eventBus = eventBus;
    const buttons = [{
      element: options.download,
      eventName: "download",
      nimbusName: "download-button"
    }, {
      element: options.openInApp,
      eventName: "openinexternalapp",
      nimbusName: "open-in-app-button"
    }];
    if (nimbusData) {
      this.#buttons = [];
      for (const button of buttons) {
        if (nimbusData[button.nimbusName]) {
          this.#buttons.push(button);
        } else {
          button.element.remove();
        }
      }
      if (this.#buttons.length > 0) {
        options.container.classList.add("show");
      } else {
        options.container.remove();
        options.mainContainer.classList.add("noToolbar");
      }
    } else {
      options.container.classList.add("show");
      this.#buttons = buttons;
    }
    this.#bindListeners(options);
  }
  setPageNumber(pageNumber, pageLabel) {}
  setPagesCount(pagesCount, hasPageLabels) {}
  setPageScale(pageScaleValue, pageScale) {}
  reset() {}
  #bindListeners(options) {
    for (const {
      element,
      eventName,
      eventDetails
    } of this.#buttons) {
      element.addEventListener("click", evt => {
        if (eventName !== null) {
          this.#eventBus.dispatch(eventName, {
            source: this,
            ...eventDetails
          });
          this.#eventBus.dispatch("reporttelemetry", {
            source: this,
            details: {
              type: "gv-buttons",
              data: {
                id: `${element.id}_tapped`
              }
            }
          });
        }
      });
    }
  }
  updateLoadingIndicatorState(loading = false) {}
}

;// CONCATENATED MODULE: ./web/view_history.js
const DEFAULT_VIEW_HISTORY_CACHE_SIZE = 20;
class ViewHistory {
  constructor(fingerprint, cacheSize = DEFAULT_VIEW_HISTORY_CACHE_SIZE) {
    this.fingerprint = fingerprint;
    this.cacheSize = cacheSize;
    this._initializedPromise = this._readFromStorage().then(databaseStr => {
      const database = JSON.parse(databaseStr || "{}");
      let index = -1;
      if (!Array.isArray(database.files)) {
        database.files = [];
      } else {
        while (database.files.length >= this.cacheSize) {
          database.files.shift();
        }
        for (let i = 0, ii = database.files.length; i < ii; i++) {
          const branch = database.files[i];
          if (branch.fingerprint === this.fingerprint) {
            index = i;
            break;
          }
        }
      }
      if (index === -1) {
        index = database.files.push({
          fingerprint: this.fingerprint
        }) - 1;
      }
      this.file = database.files[index];
      this.database = database;
    });
  }
  async _writeToStorage() {
    const databaseStr = JSON.stringify(this.database);
    sessionStorage.setItem("pdfjs.history", databaseStr);
  }
  async _readFromStorage() {
    return sessionStorage.getItem("pdfjs.history");
  }
  async set(name, val) {
    await this._initializedPromise;
    this.file[name] = val;
    return this._writeToStorage();
  }
  async setMultiple(properties) {
    await this._initializedPromise;
    for (const name in properties) {
      this.file[name] = properties[name];
    }
    return this._writeToStorage();
  }
  async get(name, defaultValue) {
    await this._initializedPromise;
    const val = this.file[name];
    return val !== undefined ? val : defaultValue;
  }
  async getMultiple(properties) {
    await this._initializedPromise;
    const values = Object.create(null);
    for (const name in properties) {
      const val = this.file[name];
      values[name] = val !== undefined ? val : properties[name];
    }
    return values;
  }
}

;// CONCATENATED MODULE: ./web/app.js


























const FORCE_PAGES_LOADED_TIMEOUT = 10000;
const WHEEL_ZOOM_DISABLED_TIMEOUT = 1000;
const ViewOnLoad = {
  UNKNOWN: -1,
  PREVIOUS: 0,
  INITIAL: 1
};
class DefaultExternalServices {
  constructor() {
    throw new Error("Cannot initialize DefaultExternalServices.");
  }
  static updateFindControlState(data) {}
  static updateFindMatchesCount(data) {}
  static initPassiveLoading(callbacks) {}
  static reportTelemetry(data) {}
  static createDownloadManager() {
    throw new Error("Not implemented: createDownloadManager");
  }
  static createPreferences() {
    throw new Error("Not implemented: createPreferences");
  }
  static async createL10n() {
    throw new Error("Not implemented: createL10n");
  }
  static createScripting(options) {
    throw new Error("Not implemented: createScripting");
  }
  static updateEditorStates(data) {
    throw new Error("Not implemented: updateEditorStates");
  }
  static getNimbusExperimentData() {
    return shadow(this, "getNimbusExperimentData", Promise.resolve(null));
  }
}
const PDFViewerApplication = {
  initialBookmark: document.location.hash.substring(1),
  _initializedCapability: new PromiseCapability(),
  appConfig: null,
  pdfDocument: null,
  pdfLoadingTask: null,
  printService: null,
  pdfViewer: null,
  pdfThumbnailViewer: null,
  pdfRenderingQueue: null,
  pdfPresentationMode: null,
  pdfDocumentProperties: null,
  pdfLinkService: null,
  pdfHistory: null,
  pdfSidebar: null,
  pdfOutlineViewer: null,
  pdfAttachmentViewer: null,
  pdfLayerViewer: null,
  pdfCursorTools: null,
  pdfScriptingManager: null,
  store: null,
  downloadManager: null,
  overlayManager: null,
  preferences: null,
  toolbar: null,
  secondaryToolbar: null,
  eventBus: null,
  l10n: null,
  annotationEditorParams: null,
  isInitialViewSet: false,
  downloadComplete: false,
  isViewerEmbedded: window.parent !== window,
  url: "",
  baseUrl: "",
  _downloadUrl: "",
  externalServices: DefaultExternalServices,
  _boundEvents: Object.create(null),
  documentInfo: null,
  metadata: null,
  _contentDispositionFilename: null,
  _contentLength: null,
  _saveInProgress: false,
  _wheelUnusedTicks: 0,
  _wheelUnusedFactor: 1,
  _touchUnusedTicks: 0,
  _touchUnusedFactor: 1,
  _PDFBug: null,
  _hasAnnotationEditors: false,
  _title: document.title,
  _printAnnotationStoragePromise: null,
  _touchInfo: null,
  _isCtrlKeyDown: false,
  _nimbusDataPromise: null,
  async initialize(appConfig) {
    let l10nPromise;
    l10nPromise = this.externalServices.createL10n();
    this.appConfig = appConfig;
    this._nimbusDataPromise = this.externalServices.getNimbusExperimentData();
    try {
      await this.preferences.initializedPromise;
    } catch (ex) {
      console.error(`initialize: "${ex.message}".`);
    }
    if (AppOptions.get("pdfBugEnabled")) {
      await this._parseHashParams();
    }
    this.l10n = await l10nPromise;
    document.getElementsByTagName("html")[0].dir = this.l10n.getDirection();
    if (this.isViewerEmbedded && AppOptions.get("externalLinkTarget") === LinkTarget.NONE) {
      AppOptions.set("externalLinkTarget", LinkTarget.TOP);
    }
    await this._initializeViewerComponents();
    this.bindEvents();
    this.bindWindowEvents();
    this._initializedCapability.resolve();
  },
  async _parseHashParams() {
    const hash = document.location.hash.substring(1);
    if (!hash) {
      return;
    }
    const {
        mainContainer,
        viewerContainer
      } = this.appConfig,
      params = parseQueryString(hash);
    if (params.get("disableworker") === "true") {
      try {
        await loadFakeWorker();
      } catch (ex) {
        console.error(`_parseHashParams: "${ex.message}".`);
      }
    }
    if (params.has("disablerange")) {
      AppOptions.set("disableRange", params.get("disablerange") === "true");
    }
    if (params.has("disablestream")) {
      AppOptions.set("disableStream", params.get("disablestream") === "true");
    }
    if (params.has("disableautofetch")) {
      AppOptions.set("disableAutoFetch", params.get("disableautofetch") === "true");
    }
    if (params.has("disablefontface")) {
      AppOptions.set("disableFontFace", params.get("disablefontface") === "true");
    }
    if (params.has("disablehistory")) {
      AppOptions.set("disableHistory", params.get("disablehistory") === "true");
    }
    if (params.has("verbosity")) {
      AppOptions.set("verbosity", params.get("verbosity") | 0);
    }
    if (params.has("textlayer")) {
      switch (params.get("textlayer")) {
        case "off":
          AppOptions.set("textLayerMode", TextLayerMode.DISABLE);
          break;
        case "visible":
        case "shadow":
        case "hover":
          viewerContainer.classList.add(`textLayer-${params.get("textlayer")}`);
          try {
            await loadPDFBug(this);
            this._PDFBug.loadCSS();
          } catch (ex) {
            console.error(`_parseHashParams: "${ex.message}".`);
          }
          break;
      }
    }
    if (params.has("pdfbug")) {
      AppOptions.set("pdfBug", true);
      AppOptions.set("fontExtraProperties", true);
      const enabled = params.get("pdfbug").split(",");
      try {
        await loadPDFBug(this);
        this._PDFBug.init(mainContainer, enabled);
      } catch (ex) {
        console.error(`_parseHashParams: "${ex.message}".`);
      }
    }
  },
  async _initializeViewerComponents() {
    const {
      appConfig,
      externalServices,
      l10n
    } = this;
    const eventBus = AppOptions.get("isInAutomation") ? new AutomationEventBus() : new EventBus();
    this.eventBus = eventBus;
    this.overlayManager = new OverlayManager();
    const pdfRenderingQueue = new PDFRenderingQueue();
    pdfRenderingQueue.onIdle = this._cleanup.bind(this);
    this.pdfRenderingQueue = pdfRenderingQueue;
    const pdfLinkService = new PDFLinkService({
      eventBus,
      externalLinkTarget: AppOptions.get("externalLinkTarget"),
      externalLinkRel: AppOptions.get("externalLinkRel"),
      ignoreDestinationZoom: AppOptions.get("ignoreDestinationZoom")
    });
    this.pdfLinkService = pdfLinkService;
    const downloadManager = externalServices.createDownloadManager();
    this.downloadManager = downloadManager;
    const findController = new PDFFindController({
      linkService: pdfLinkService,
      eventBus,
      updateMatchesCountOnProgress: false
    });
    this.findController = findController;
    const pdfScriptingManager = new PDFScriptingManager({
      eventBus,
      sandboxBundleSrc: null,
      externalServices,
      docProperties: this._scriptingDocProperties.bind(this)
    });
    this.pdfScriptingManager = pdfScriptingManager;
    const container = appConfig.mainContainer,
      viewer = appConfig.viewerContainer;
    const annotationEditorMode = AppOptions.get("annotationEditorMode");
    const isOffscreenCanvasSupported = AppOptions.get("isOffscreenCanvasSupported") && FeatureTest.isOffscreenCanvasSupported;
    const pageColors = AppOptions.get("forcePageColors") || window.matchMedia("(forced-colors: active)").matches ? {
      background: AppOptions.get("pageColorsBackground"),
      foreground: AppOptions.get("pageColorsForeground")
    } : null;
    const altTextManager = appConfig.altTextDialog ? new AltTextManager(appConfig.altTextDialog, container, this.overlayManager, eventBus) : null;
    const pdfViewer = new PDFViewer({
      container,
      viewer,
      eventBus,
      renderingQueue: pdfRenderingQueue,
      linkService: pdfLinkService,
      downloadManager,
      altTextManager,
      findController,
      scriptingManager: AppOptions.get("enableScripting") && pdfScriptingManager,
      l10n,
      textLayerMode: AppOptions.get("textLayerMode"),
      annotationMode: AppOptions.get("annotationMode"),
      annotationEditorMode,
      imageResourcesPath: AppOptions.get("imageResourcesPath"),
      enablePrintAutoRotate: AppOptions.get("enablePrintAutoRotate"),
      isOffscreenCanvasSupported,
      maxCanvasPixels: AppOptions.get("maxCanvasPixels"),
      enablePermissions: AppOptions.get("enablePermissions"),
      pageColors
    });
    this.pdfViewer = pdfViewer;
    pdfRenderingQueue.setViewer(pdfViewer);
    pdfLinkService.setViewer(pdfViewer);
    pdfScriptingManager.setViewer(pdfViewer);
    if (appConfig.sidebar?.thumbnailView) {
      this.pdfThumbnailViewer = new PDFThumbnailViewer({
        container: appConfig.sidebar.thumbnailView,
        eventBus,
        renderingQueue: pdfRenderingQueue,
        linkService: pdfLinkService,
        pageColors
      });
      pdfRenderingQueue.setThumbnailViewer(this.pdfThumbnailViewer);
    }
    if (!this.isViewerEmbedded && !AppOptions.get("disableHistory")) {
      this.pdfHistory = new PDFHistory({
        linkService: pdfLinkService,
        eventBus
      });
      pdfLinkService.setHistory(this.pdfHistory);
    }
    if (!this.supportsIntegratedFind && appConfig.findBar) {
      this.findBar = new PDFFindBar(appConfig.findBar, eventBus);
    }
    if (appConfig.annotationEditorParams) {
      if (annotationEditorMode !== AnnotationEditorType.DISABLE) {
        if (!isOffscreenCanvasSupported) {
          appConfig.toolbar?.editorStampButton?.classList.add("hidden");
        }
        this.annotationEditorParams = new AnnotationEditorParams(appConfig.annotationEditorParams, eventBus);
      } else {
        for (const id of ["editorModeButtons", "editorModeSeparator"]) {
          document.getElementById(id)?.classList.add("hidden");
        }
      }
    }
    if (appConfig.documentProperties) {
      this.pdfDocumentProperties = new PDFDocumentProperties(appConfig.documentProperties, this.overlayManager, eventBus, l10n, () => this._docFilename);
    }
    if (appConfig.secondaryToolbar?.cursorHandToolButton) {
      this.pdfCursorTools = new PDFCursorTools({
        container,
        eventBus,
        cursorToolOnLoad: AppOptions.get("cursorToolOnLoad")
      });
    }
    if (appConfig.toolbar) {
      this.toolbar = new Toolbar(appConfig.toolbar, eventBus, await this._nimbusDataPromise);
    }
    if (appConfig.secondaryToolbar) {
      this.secondaryToolbar = new SecondaryToolbar(appConfig.secondaryToolbar, eventBus);
    }
    if (this.supportsFullscreen && appConfig.secondaryToolbar?.presentationModeButton) {
      this.pdfPresentationMode = new PDFPresentationMode({
        container,
        pdfViewer,
        eventBus
      });
    }
    if (appConfig.passwordOverlay) {
      this.passwordPrompt = new PasswordPrompt(appConfig.passwordOverlay, this.overlayManager, this.isViewerEmbedded);
    }
    if (appConfig.sidebar?.outlineView) {
      this.pdfOutlineViewer = new PDFOutlineViewer({
        container: appConfig.sidebar.outlineView,
        eventBus,
        l10n,
        linkService: pdfLinkService,
        downloadManager
      });
    }
    if (appConfig.sidebar?.attachmentsView) {
      this.pdfAttachmentViewer = new PDFAttachmentViewer({
        container: appConfig.sidebar.attachmentsView,
        eventBus,
        l10n,
        downloadManager
      });
    }
    if (appConfig.sidebar?.layersView) {
      this.pdfLayerViewer = new PDFLayerViewer({
        container: appConfig.sidebar.layersView,
        eventBus,
        l10n
      });
    }
    if (appConfig.sidebar) {
      this.pdfSidebar = new PDFSidebar({
        elements: appConfig.sidebar,
        eventBus,
        l10n
      });
      this.pdfSidebar.onToggled = this.forceRendering.bind(this);
      this.pdfSidebar.onUpdateThumbnails = () => {
        for (const pageView of pdfViewer.getCachedPageViews()) {
          if (pageView.renderingState === RenderingStates.FINISHED) {
            this.pdfThumbnailViewer.getThumbnail(pageView.id - 1)?.setImage(pageView);
          }
        }
        this.pdfThumbnailViewer.scrollThumbnailIntoView(pdfViewer.currentPageNumber);
      };
    }
  },
  async run(config) {
    this.preferences = this.externalServices.createPreferences();
    await this.initialize(config);
    const {
      appConfig,
      eventBus
    } = this;
    let file;
    file = window.location.href;
    if (!AppOptions.get("supportsDocumentFonts")) {
      AppOptions.set("disableFontFace", true);
      this.l10n.get("pdfjs-web-fonts-disabled").then(msg => {
        console.warn(msg);
      });
    }
    if (!this.supportsPrinting) {
      appConfig.toolbar?.print?.classList.add("hidden");
      appConfig.secondaryToolbar?.printButton.classList.add("hidden");
    }
    if (!this.supportsFullscreen) {
      appConfig.secondaryToolbar?.presentationModeButton.classList.add("hidden");
    }
    if (this.supportsIntegratedFind) {
      appConfig.toolbar?.viewFind?.classList.add("hidden");
    }
    appConfig.mainContainer.addEventListener("transitionend", function (evt) {
      if (evt.target === this) {
        eventBus.dispatch("resize", {
          source: this
        });
      }
    }, true);
    this.initPassiveLoading(file);
  },
  get initialized() {
    return this._initializedCapability.settled;
  },
  get initializedPromise() {
    return this._initializedCapability.promise;
  },
  zoomIn(steps, scaleFactor) {
    if (this.pdfViewer.isInPresentationMode) {
      return;
    }
    this.pdfViewer.increaseScale({
      drawingDelay: AppOptions.get("defaultZoomDelay"),
      steps,
      scaleFactor
    });
  },
  zoomOut(steps, scaleFactor) {
    if (this.pdfViewer.isInPresentationMode) {
      return;
    }
    this.pdfViewer.decreaseScale({
      drawingDelay: AppOptions.get("defaultZoomDelay"),
      steps,
      scaleFactor
    });
  },
  zoomReset() {
    if (this.pdfViewer.isInPresentationMode) {
      return;
    }
    this.pdfViewer.currentScaleValue = DEFAULT_SCALE_VALUE;
  },
  get pagesCount() {
    return this.pdfDocument ? this.pdfDocument.numPages : 0;
  },
  get page() {
    return this.pdfViewer.currentPageNumber;
  },
  set page(val) {
    this.pdfViewer.currentPageNumber = val;
  },
  get supportsPrinting() {
    return PDFPrintServiceFactory.instance.supportsPrinting;
  },
  get supportsFullscreen() {
    return shadow(this, "supportsFullscreen", document.fullscreenEnabled);
  },
  get supportsPinchToZoom() {
    return shadow(this, "supportsPinchToZoom", AppOptions.get("supportsPinchToZoom"));
  },
  get supportsIntegratedFind() {
    return shadow(this, "supportsIntegratedFind", AppOptions.get("supportsIntegratedFind"));
  },
  get loadingBar() {
    const barElement = document.getElementById("loadingBar");
    const bar = barElement ? new ProgressBar(barElement) : null;
    return shadow(this, "loadingBar", bar);
  },
  get supportsMouseWheelZoomCtrlKey() {
    return shadow(this, "supportsMouseWheelZoomCtrlKey", AppOptions.get("supportsMouseWheelZoomCtrlKey"));
  },
  get supportsMouseWheelZoomMetaKey() {
    return shadow(this, "supportsMouseWheelZoomMetaKey", AppOptions.get("supportsMouseWheelZoomMetaKey"));
  },
  initPassiveLoading(file) {
    this.setTitleUsingUrl(file, file);
    this.externalServices.initPassiveLoading({
      onOpenWithTransport: range => {
        this.open({
          range
        });
      },
      onOpenWithData: (data, contentDispositionFilename) => {
        if (isPdfFile(contentDispositionFilename)) {
          this._contentDispositionFilename = contentDispositionFilename;
        }
        this.open({
          data
        });
      },
      onOpenWithURL: (url, length, originalUrl) => {
        this.open({
          url,
          length,
          originalUrl
        });
      },
      onError: err => {
        this.l10n.get("pdfjs-loading-error").then(msg => {
          this._documentError(msg, err);
        });
      },
      onProgress: (loaded, total) => {
        this.progress(loaded / total);
      }
    });
  },
  setTitleUsingUrl(url = "", downloadUrl = null) {
    this.url = url;
    this.baseUrl = url.split("#", 1)[0];
    if (downloadUrl) {
      this._downloadUrl = downloadUrl === url ? this.baseUrl : downloadUrl.split("#", 1)[0];
    }
    if (isDataScheme(url)) {
      this._hideViewBookmark();
    }
    let title = getPdfFilenameFromUrl(url, "");
    if (!title) {
      try {
        title = decodeURIComponent(getFilenameFromUrl(url)) || url;
      } catch {
        title = url;
      }
    }
    this.setTitle(title);
  },
  setTitle(title = this._title) {
    this._title = title;
    if (this.isViewerEmbedded) {
      return;
    }
    const editorIndicator = this._hasAnnotationEditors && !this.pdfRenderingQueue.printing;
    document.title = `${editorIndicator ? "* " : ""}${title}`;
  },
  get _docFilename() {
    return this._contentDispositionFilename || getPdfFilenameFromUrl(this.url);
  },
  _hideViewBookmark() {
    const {
      secondaryToolbar
    } = this.appConfig;
    secondaryToolbar?.viewBookmarkButton.classList.add("hidden");
    if (secondaryToolbar?.presentationModeButton.classList.contains("hidden")) {
      document.getElementById("viewBookmarkSeparator")?.classList.add("hidden");
    }
  },
  async close() {
    this._unblockDocumentLoadEvent();
    this._hideViewBookmark();
    if (!this.pdfLoadingTask) {
      return;
    }
    const promises = [];
    promises.push(this.pdfLoadingTask.destroy());
    this.pdfLoadingTask = null;
    if (this.pdfDocument) {
      this.pdfDocument = null;
      this.pdfThumbnailViewer?.setDocument(null);
      this.pdfViewer.setDocument(null);
      this.pdfLinkService.setDocument(null);
      this.pdfDocumentProperties?.setDocument(null);
    }
    this.pdfLinkService.externalLinkEnabled = true;
    this.store = null;
    this.isInitialViewSet = false;
    this.downloadComplete = false;
    this.url = "";
    this.baseUrl = "";
    this._downloadUrl = "";
    this.documentInfo = null;
    this.metadata = null;
    this._contentDispositionFilename = null;
    this._contentLength = null;
    this._saveInProgress = false;
    this._hasAnnotationEditors = false;
    promises.push(this.pdfScriptingManager.destroyPromise, this.passwordPrompt.close());
    this.setTitle();
    this.pdfSidebar?.reset();
    this.pdfOutlineViewer?.reset();
    this.pdfAttachmentViewer?.reset();
    this.pdfLayerViewer?.reset();
    this.pdfHistory?.reset();
    this.findBar?.reset();
    this.toolbar?.reset();
    this.secondaryToolbar?.reset();
    this._PDFBug?.cleanup();
    await Promise.all(promises);
  },
  async open(args) {
    if (this.pdfLoadingTask) {
      await this.close();
    }
    const workerParams = AppOptions.getAll(OptionKind.WORKER);
    Object.assign(GlobalWorkerOptions, workerParams);
    AppOptions.set("docBaseUrl", this.baseUrl);
    const apiParams = AppOptions.getAll(OptionKind.API);
    const loadingTask = getDocument({
      ...apiParams,
      ...args
    });
    this.pdfLoadingTask = loadingTask;
    loadingTask.onPassword = (updateCallback, reason) => {
      if (this.isViewerEmbedded) {
        this._unblockDocumentLoadEvent();
      }
      this.pdfLinkService.externalLinkEnabled = false;
      this.passwordPrompt.setUpdateCallback(updateCallback, reason);
      this.passwordPrompt.open();
    };
    loadingTask.onProgress = ({
      loaded,
      total
    }) => {
      this.progress(loaded / total);
    };
    return loadingTask.promise.then(pdfDocument => {
      this.load(pdfDocument);
    }, reason => {
      if (loadingTask !== this.pdfLoadingTask) {
        return undefined;
      }
      let key = "pdfjs-loading-error";
      if (reason instanceof InvalidPDFException) {
        key = "pdfjs-invalid-file-error";
      } else if (reason instanceof MissingPDFException) {
        key = "pdfjs-missing-file-error";
      } else if (reason instanceof UnexpectedResponseException) {
        key = "pdfjs-unexpected-response-error";
      }
      return this.l10n.get(key).then(msg => {
        this._documentError(msg, {
          message: reason?.message
        });
        throw reason;
      });
    });
  },
  _ensureDownloadComplete() {
    if (this.pdfDocument && this.downloadComplete) {
      return;
    }
    throw new Error("PDF document not downloaded.");
  },
  async download(options = {}) {
    const url = this._downloadUrl,
      filename = this._docFilename;
    try {
      this._ensureDownloadComplete();
      const data = await this.pdfDocument.getData();
      const blob = new Blob([data], {
        type: "application/pdf"
      });
      await this.downloadManager.download(blob, url, filename, options);
    } catch {
      await this.downloadManager.downloadUrl(url, filename, options);
    }
  },
  async save(options = {}) {
    if (this._saveInProgress) {
      return;
    }
    this._saveInProgress = true;
    await this.pdfScriptingManager.dispatchWillSave();
    const url = this._downloadUrl,
      filename = this._docFilename;
    try {
      this._ensureDownloadComplete();
      const data = await this.pdfDocument.saveDocument();
      const blob = new Blob([data], {
        type: "application/pdf"
      });
      await this.downloadManager.download(blob, url, filename, options);
    } catch (reason) {
      console.error(`Error when saving the document: ${reason.message}`);
      await this.download(options);
    } finally {
      await this.pdfScriptingManager.dispatchDidSave();
      this._saveInProgress = false;
    }
    if (this._hasAnnotationEditors) {
      this.externalServices.reportTelemetry({
        type: "editing",
        data: {
          type: "save"
        }
      });
    }
  },
  downloadOrSave(options = {}) {
    if (this.pdfDocument?.annotationStorage.size > 0) {
      this.save(options);
    } else {
      this.download(options);
    }
  },
  openInExternalApp() {
    this.downloadOrSave({
      openInExternalApp: true
    });
  },
  _documentError(message, moreInfo = null) {
    this._unblockDocumentLoadEvent();
    this._otherError(message, moreInfo);
    this.eventBus.dispatch("documenterror", {
      source: this,
      message,
      reason: moreInfo?.message ?? null
    });
  },
  _otherError(message, moreInfo = null) {
    const moreInfoText = [`PDF.js v${version || "?"} (build: ${build || "?"})`];
    if (moreInfo) {
      moreInfoText.push(`Message: ${moreInfo.message}`);
      if (moreInfo.stack) {
        moreInfoText.push(`Stack: ${moreInfo.stack}`);
      } else {
        if (moreInfo.filename) {
          moreInfoText.push(`File: ${moreInfo.filename}`);
        }
        if (moreInfo.lineNumber) {
          moreInfoText.push(`Line: ${moreInfo.lineNumber}`);
        }
      }
    }
    console.error(`${message}\n\n${moreInfoText.join("\n")}`);
  },
  progress(level) {
    if (!this.loadingBar || this.downloadComplete) {
      return;
    }
    const percent = Math.round(level * 100);
    if (percent <= this.loadingBar.percent) {
      return;
    }
    this.loadingBar.percent = percent;
    if (this.pdfDocument?.loadingParams.disableAutoFetch ?? AppOptions.get("disableAutoFetch")) {
      this.loadingBar.setDisableAutoFetch();
    }
  },
  load(pdfDocument) {
    this.pdfDocument = pdfDocument;
    pdfDocument.getDownloadInfo().then(({
      length
    }) => {
      this._contentLength = length;
      this.downloadComplete = true;
      this.loadingBar?.hide();
      firstPagePromise.then(() => {
        this.eventBus.dispatch("documentloaded", {
          source: this
        });
      });
    });
    const pageLayoutPromise = pdfDocument.getPageLayout().catch(() => {});
    const pageModePromise = pdfDocument.getPageMode().catch(() => {});
    const openActionPromise = pdfDocument.getOpenAction().catch(() => {});
    this.toolbar?.setPagesCount(pdfDocument.numPages, false);
    this.secondaryToolbar?.setPagesCount(pdfDocument.numPages);
    this.pdfLinkService.setDocument(pdfDocument);
    this.pdfDocumentProperties?.setDocument(pdfDocument);
    const pdfViewer = this.pdfViewer;
    pdfViewer.setDocument(pdfDocument);
    const {
      firstPagePromise,
      onePageRendered,
      pagesPromise
    } = pdfViewer;
    this.pdfThumbnailViewer?.setDocument(pdfDocument);
    const storedPromise = (this.store = new ViewHistory(pdfDocument.fingerprints[0])).getMultiple({
      page: null,
      zoom: DEFAULT_SCALE_VALUE,
      scrollLeft: "0",
      scrollTop: "0",
      rotation: null,
      sidebarView: SidebarView.UNKNOWN,
      scrollMode: ScrollMode.UNKNOWN,
      spreadMode: SpreadMode.UNKNOWN
    }).catch(() => {});
    firstPagePromise.then(pdfPage => {
      this.loadingBar?.setWidth(this.appConfig.viewerContainer);
      this._initializeAnnotationStorageCallbacks(pdfDocument);
      Promise.all([animationStarted, storedPromise, pageLayoutPromise, pageModePromise, openActionPromise]).then(async ([timeStamp, stored, pageLayout, pageMode, openAction]) => {
        const viewOnLoad = AppOptions.get("viewOnLoad");
        this._initializePdfHistory({
          fingerprint: pdfDocument.fingerprints[0],
          viewOnLoad,
          initialDest: openAction?.dest
        });
        const initialBookmark = this.initialBookmark;
        const zoom = AppOptions.get("defaultZoomValue");
        let hash = zoom ? `zoom=${zoom}` : null;
        let rotation = null;
        let sidebarView = AppOptions.get("sidebarViewOnLoad");
        let scrollMode = AppOptions.get("scrollModeOnLoad");
        let spreadMode = AppOptions.get("spreadModeOnLoad");
        if (stored?.page && viewOnLoad !== ViewOnLoad.INITIAL) {
          hash = `page=${stored.page}&zoom=${zoom || stored.zoom},` + `${stored.scrollLeft},${stored.scrollTop}`;
          rotation = parseInt(stored.rotation, 10);
          if (sidebarView === SidebarView.UNKNOWN) {
            sidebarView = stored.sidebarView | 0;
          }
          if (scrollMode === ScrollMode.UNKNOWN) {
            scrollMode = stored.scrollMode | 0;
          }
          if (spreadMode === SpreadMode.UNKNOWN) {
            spreadMode = stored.spreadMode | 0;
          }
        }
        if (pageMode && sidebarView === SidebarView.UNKNOWN) {
          sidebarView = apiPageModeToSidebarView(pageMode);
        }
        if (pageLayout && scrollMode === ScrollMode.UNKNOWN && spreadMode === SpreadMode.UNKNOWN) {
          const modes = apiPageLayoutToViewerModes(pageLayout);
          spreadMode = modes.spreadMode;
        }
        this.setInitialView(hash, {
          rotation,
          sidebarView,
          scrollMode,
          spreadMode
        });
        this.eventBus.dispatch("documentinit", {
          source: this
        });
        if (!this.isViewerEmbedded) {
          pdfViewer.focus();
        }
        await Promise.race([pagesPromise, new Promise(resolve => {
          setTimeout(resolve, FORCE_PAGES_LOADED_TIMEOUT);
        })]);
        if (!initialBookmark && !hash) {
          return;
        }
        if (pdfViewer.hasEqualPageSizes) {
          return;
        }
        this.initialBookmark = initialBookmark;
        pdfViewer.currentScaleValue = pdfViewer.currentScaleValue;
        this.setInitialView(hash);
      }).catch(() => {
        this.setInitialView();
      }).then(function () {
        pdfViewer.update();
      });
    });
    pagesPromise.then(() => {
      this._unblockDocumentLoadEvent();
      this._initializeAutoPrint(pdfDocument, openActionPromise);
    }, reason => {
      this.l10n.get("pdfjs-loading-error").then(msg => {
        this._documentError(msg, {
          message: reason?.message
        });
      });
    });
    onePageRendered.then(data => {
      this.externalServices.reportTelemetry({
        type: "pageInfo",
        timestamp: data.timestamp
      });
      if (this.pdfOutlineViewer) {
        pdfDocument.getOutline().then(outline => {
          if (pdfDocument !== this.pdfDocument) {
            return;
          }
          this.pdfOutlineViewer.render({
            outline,
            pdfDocument
          });
        });
      }
      if (this.pdfAttachmentViewer) {
        pdfDocument.getAttachments().then(attachments => {
          if (pdfDocument !== this.pdfDocument) {
            return;
          }
          this.pdfAttachmentViewer.render({
            attachments
          });
        });
      }
      if (this.pdfLayerViewer) {
        pdfViewer.optionalContentConfigPromise.then(optionalContentConfig => {
          if (pdfDocument !== this.pdfDocument) {
            return;
          }
          this.pdfLayerViewer.render({
            optionalContentConfig,
            pdfDocument
          });
        });
      }
    });
    this._initializePageLabels(pdfDocument);
    this._initializeMetadata(pdfDocument);
  },
  async _scriptingDocProperties(pdfDocument) {
    if (!this.documentInfo) {
      await new Promise(resolve => {
        this.eventBus._on("metadataloaded", resolve, {
          once: true
        });
      });
      if (pdfDocument !== this.pdfDocument) {
        return null;
      }
    }
    if (!this._contentLength) {
      await new Promise(resolve => {
        this.eventBus._on("documentloaded", resolve, {
          once: true
        });
      });
      if (pdfDocument !== this.pdfDocument) {
        return null;
      }
    }
    return {
      ...this.documentInfo,
      baseURL: this.baseUrl,
      filesize: this._contentLength,
      filename: this._docFilename,
      metadata: this.metadata?.getRaw(),
      authors: this.metadata?.get("dc:creator"),
      numPages: this.pagesCount,
      URL: this.url
    };
  },
  async _initializeAutoPrint(pdfDocument, openActionPromise) {
    const [openAction, jsActions] = await Promise.all([openActionPromise, this.pdfViewer.enableScripting ? null : pdfDocument.getJSActions()]);
    if (pdfDocument !== this.pdfDocument) {
      return;
    }
    let triggerAutoPrint = openAction?.action === "Print";
    if (jsActions) {
      console.warn("Warning: JavaScript support is not enabled");
      for (const name in jsActions) {
        if (triggerAutoPrint) {
          break;
        }
        switch (name) {
          case "WillClose":
          case "WillSave":
          case "DidSave":
          case "WillPrint":
          case "DidPrint":
            continue;
        }
        triggerAutoPrint = jsActions[name].some(js => AutoPrintRegExp.test(js));
      }
    }
    if (triggerAutoPrint) {
      this.triggerPrinting();
    }
  },
  async _initializeMetadata(pdfDocument) {
    const {
      info,
      metadata,
      contentDispositionFilename,
      contentLength
    } = await pdfDocument.getMetadata();
    if (pdfDocument !== this.pdfDocument) {
      return;
    }
    this.documentInfo = info;
    this.metadata = metadata;
    this._contentDispositionFilename ??= contentDispositionFilename;
    this._contentLength ??= contentLength;
    console.log(`PDF ${pdfDocument.fingerprints[0]} [${info.PDFFormatVersion} ` + `${(info.Producer || "-").trim()} / ${(info.Creator || "-").trim()}] ` + `(PDF.js: ${version || "?"} [${build || "?"}])`);
    let pdfTitle = info.Title;
    const metadataTitle = metadata?.get("dc:title");
    if (metadataTitle) {
      if (metadataTitle !== "Untitled" && !/[\uFFF0-\uFFFF]/g.test(metadataTitle)) {
        pdfTitle = metadataTitle;
      }
    }
    if (pdfTitle) {
      this.setTitle(`${pdfTitle} - ${this._contentDispositionFilename || this._title}`);
    } else if (this._contentDispositionFilename) {
      this.setTitle(this._contentDispositionFilename);
    }
    if (info.IsXFAPresent && !info.IsAcroFormPresent && !pdfDocument.isPureXfa) {
      if (pdfDocument.loadingParams.enableXfa) {
        console.warn("Warning: XFA Foreground documents are not supported");
      } else {
        console.warn("Warning: XFA support is not enabled");
      }
    } else if ((info.IsAcroFormPresent || info.IsXFAPresent) && !this.pdfViewer.renderForms) {
      console.warn("Warning: Interactive form support is not enabled");
    }
    if (info.IsSignaturesPresent) {
      console.warn("Warning: Digital signatures validation is not supported");
    }
    this.eventBus.dispatch("metadataloaded", {
      source: this
    });
  },
  async _initializePageLabels(pdfDocument) {},
  _initializePdfHistory({
    fingerprint,
    viewOnLoad,
    initialDest = null
  }) {
    if (!this.pdfHistory) {
      return;
    }
    this.pdfHistory.initialize({
      fingerprint,
      resetHistory: viewOnLoad === ViewOnLoad.INITIAL,
      updateUrl: AppOptions.get("historyUpdateUrl")
    });
    if (this.pdfHistory.initialBookmark) {
      this.initialBookmark = this.pdfHistory.initialBookmark;
      this.initialRotation = this.pdfHistory.initialRotation;
    }
    if (initialDest && !this.initialBookmark && viewOnLoad === ViewOnLoad.UNKNOWN) {
      this.initialBookmark = JSON.stringify(initialDest);
      this.pdfHistory.push({
        explicitDest: initialDest,
        pageNumber: null
      });
    }
  },
  _initializeAnnotationStorageCallbacks(pdfDocument) {
    if (pdfDocument !== this.pdfDocument) {
      return;
    }
    const {
      annotationStorage
    } = pdfDocument;
    annotationStorage.onSetModified = () => {
      window.addEventListener("beforeunload", beforeUnload);
    };
    annotationStorage.onResetModified = () => {
      window.removeEventListener("beforeunload", beforeUnload);
    };
    annotationStorage.onAnnotationEditor = typeStr => {
      this._hasAnnotationEditors = !!typeStr;
      this.setTitle();
      if (typeStr) {
        this.externalServices.reportTelemetry({
          type: "editing",
          data: {
            type: typeStr
          }
        });
      }
    };
  },
  setInitialView(storedHash, {
    rotation,
    sidebarView,
    scrollMode,
    spreadMode
  } = {}) {
    const setRotation = angle => {
      if (isValidRotation(angle)) {
        this.pdfViewer.pagesRotation = angle;
      }
    };
    const setViewerModes = (scroll, spread) => {
      if (isValidScrollMode(scroll)) {
        this.pdfViewer.scrollMode = scroll;
      }
      if (isValidSpreadMode(spread)) {
        this.pdfViewer.spreadMode = spread;
      }
    };
    this.isInitialViewSet = true;
    this.pdfSidebar?.setInitialView(sidebarView);
    setViewerModes(scrollMode, spreadMode);
    if (this.initialBookmark) {
      setRotation(this.initialRotation);
      delete this.initialRotation;
      this.pdfLinkService.setHash(this.initialBookmark);
      this.initialBookmark = null;
    } else if (storedHash) {
      setRotation(rotation);
      this.pdfLinkService.setHash(storedHash);
    }
    this.toolbar?.setPageNumber(this.pdfViewer.currentPageNumber, this.pdfViewer.currentPageLabel);
    this.secondaryToolbar?.setPageNumber(this.pdfViewer.currentPageNumber);
    if (!this.pdfViewer.currentScaleValue) {
      this.pdfViewer.currentScaleValue = DEFAULT_SCALE_VALUE;
    }
  },
  _cleanup() {
    if (!this.pdfDocument) {
      return;
    }
    this.pdfViewer.cleanup();
    this.pdfThumbnailViewer?.cleanup();
    this.pdfDocument.cleanup(AppOptions.get("fontExtraProperties"));
  },
  forceRendering() {
    this.pdfRenderingQueue.printing = !!this.printService;
    this.pdfRenderingQueue.isThumbnailViewEnabled = this.pdfSidebar?.visibleView === SidebarView.THUMBS;
    this.pdfRenderingQueue.renderHighestPriority();
  },
  beforePrint() {
    this._printAnnotationStoragePromise = this.pdfScriptingManager.dispatchWillPrint().catch(() => {}).then(() => {
      return this.pdfDocument?.annotationStorage.print;
    });
    if (this.printService) {
      return;
    }
    if (!this.supportsPrinting) {
      this.l10n.get("pdfjs-printing-not-supported").then(msg => {
        this._otherError(msg);
      });
      return;
    }
    if (!this.pdfViewer.pageViewsReady) {
      this.l10n.get("pdfjs-printing-not-ready").then(msg => {
        window.alert(msg);
      });
      return;
    }
    const pagesOverview = this.pdfViewer.getPagesOverview();
    const printContainer = this.appConfig.printContainer;
    const printResolution = AppOptions.get("printResolution");
    const optionalContentConfigPromise = this.pdfViewer.optionalContentConfigPromise;
    const printService = PDFPrintServiceFactory.instance.createPrintService(this.pdfDocument, pagesOverview, printContainer, printResolution, optionalContentConfigPromise, this._printAnnotationStoragePromise);
    this.printService = printService;
    this.forceRendering();
    this.setTitle();
    printService.layout();
    if (this._hasAnnotationEditors) {
      this.externalServices.reportTelemetry({
        type: "editing",
        data: {
          type: "print"
        }
      });
    }
  },
  afterPrint() {
    if (this._printAnnotationStoragePromise) {
      this._printAnnotationStoragePromise.then(() => {
        this.pdfScriptingManager.dispatchDidPrint();
      });
      this._printAnnotationStoragePromise = null;
    }
    if (this.printService) {
      this.printService.destroy();
      this.printService = null;
      this.pdfDocument?.annotationStorage.resetModified();
    }
    this.forceRendering();
    this.setTitle();
  },
  rotatePages(delta) {
    this.pdfViewer.pagesRotation += delta;
  },
  requestPresentationMode() {
    this.pdfPresentationMode?.request();
  },
  triggerPrinting() {
    if (!this.supportsPrinting) {
      return;
    }
    window.print();
  },
  bindEvents() {
    const {
      eventBus,
      _boundEvents
    } = this;
    _boundEvents.beforePrint = this.beforePrint.bind(this);
    _boundEvents.afterPrint = this.afterPrint.bind(this);
    eventBus._on("resize", webViewerResize);
    eventBus._on("hashchange", webViewerHashchange);
    eventBus._on("beforeprint", _boundEvents.beforePrint);
    eventBus._on("afterprint", _boundEvents.afterPrint);
    eventBus._on("pagerender", webViewerPageRender);
    eventBus._on("pagerendered", webViewerPageRendered);
    eventBus._on("updateviewarea", webViewerUpdateViewarea);
    eventBus._on("pagechanging", webViewerPageChanging);
    eventBus._on("scalechanging", webViewerScaleChanging);
    eventBus._on("rotationchanging", webViewerRotationChanging);
    eventBus._on("sidebarviewchanged", webViewerSidebarViewChanged);
    eventBus._on("pagemode", webViewerPageMode);
    eventBus._on("namedaction", webViewerNamedAction);
    eventBus._on("presentationmodechanged", webViewerPresentationModeChanged);
    eventBus._on("presentationmode", webViewerPresentationMode);
    eventBus._on("switchannotationeditormode", webViewerSwitchAnnotationEditorMode);
    eventBus._on("switchannotationeditorparams", webViewerSwitchAnnotationEditorParams);
    eventBus._on("print", webViewerPrint);
    eventBus._on("download", webViewerDownload);
    eventBus._on("openinexternalapp", webViewerOpenInExternalApp);
    eventBus._on("firstpage", webViewerFirstPage);
    eventBus._on("lastpage", webViewerLastPage);
    eventBus._on("nextpage", webViewerNextPage);
    eventBus._on("previouspage", webViewerPreviousPage);
    eventBus._on("zoomin", webViewerZoomIn);
    eventBus._on("zoomout", webViewerZoomOut);
    eventBus._on("zoomreset", webViewerZoomReset);
    eventBus._on("pagenumberchanged", webViewerPageNumberChanged);
    eventBus._on("scalechanged", webViewerScaleChanged);
    eventBus._on("rotatecw", webViewerRotateCw);
    eventBus._on("rotateccw", webViewerRotateCcw);
    eventBus._on("optionalcontentconfig", webViewerOptionalContentConfig);
    eventBus._on("switchscrollmode", webViewerSwitchScrollMode);
    eventBus._on("scrollmodechanged", webViewerScrollModeChanged);
    eventBus._on("switchspreadmode", webViewerSwitchSpreadMode);
    eventBus._on("spreadmodechanged", webViewerSpreadModeChanged);
    eventBus._on("documentproperties", webViewerDocumentProperties);
    eventBus._on("findfromurlhash", webViewerFindFromUrlHash);
    eventBus._on("updatefindmatchescount", webViewerUpdateFindMatchesCount);
    eventBus._on("updatefindcontrolstate", webViewerUpdateFindControlState);
    if (AppOptions.get("pdfBug")) {
      _boundEvents.reportPageStatsPDFBug = reportPageStatsPDFBug;
      eventBus._on("pagerendered", _boundEvents.reportPageStatsPDFBug);
      eventBus._on("pagechanging", _boundEvents.reportPageStatsPDFBug);
    }
    eventBus._on("annotationeditorstateschanged", webViewerAnnotationEditorStatesChanged);
    eventBus._on("reporttelemetry", webViewerReportTelemetry);
  },
  bindWindowEvents() {
    const {
      eventBus,
      _boundEvents
    } = this;
    function addWindowResolutionChange(evt = null) {
      if (evt) {
        webViewerResolutionChange(evt);
      }
      const mediaQueryList = window.matchMedia(`(resolution: ${window.devicePixelRatio || 1}dppx)`);
      mediaQueryList.addEventListener("change", addWindowResolutionChange, {
        once: true
      });
    }
    addWindowResolutionChange();
    _boundEvents.windowResize = () => {
      eventBus.dispatch("resize", {
        source: window
      });
    };
    _boundEvents.windowHashChange = () => {
      eventBus.dispatch("hashchange", {
        source: window,
        hash: document.location.hash.substring(1)
      });
    };
    _boundEvents.windowBeforePrint = () => {
      eventBus.dispatch("beforeprint", {
        source: window
      });
    };
    _boundEvents.windowAfterPrint = () => {
      eventBus.dispatch("afterprint", {
        source: window
      });
    };
    _boundEvents.windowUpdateFromSandbox = event => {
      eventBus.dispatch("updatefromsandbox", {
        source: window,
        detail: event.detail
      });
    };
    window.addEventListener("visibilitychange", webViewerVisibilityChange);
    window.addEventListener("wheel", webViewerWheel, {
      passive: false
    });
    window.addEventListener("touchstart", webViewerTouchStart, {
      passive: false
    });
    window.addEventListener("touchmove", webViewerTouchMove, {
      passive: false
    });
    window.addEventListener("touchend", webViewerTouchEnd, {
      passive: false
    });
    window.addEventListener("click", webViewerClick);
    window.addEventListener("keydown", webViewerKeyDown);
    window.addEventListener("keyup", webViewerKeyUp);
    window.addEventListener("resize", _boundEvents.windowResize);
    window.addEventListener("hashchange", _boundEvents.windowHashChange);
    window.addEventListener("beforeprint", _boundEvents.windowBeforePrint);
    window.addEventListener("afterprint", _boundEvents.windowAfterPrint);
    window.addEventListener("updatefromsandbox", _boundEvents.windowUpdateFromSandbox);
  },
  unbindEvents() {
    throw new Error("Not implemented: unbindEvents");
  },
  unbindWindowEvents() {
    throw new Error("Not implemented: unbindWindowEvents");
  },
  _accumulateTicks(ticks, prop) {
    if (this[prop] > 0 && ticks < 0 || this[prop] < 0 && ticks > 0) {
      this[prop] = 0;
    }
    this[prop] += ticks;
    const wholeTicks = Math.trunc(this[prop]);
    this[prop] -= wholeTicks;
    return wholeTicks;
  },
  _accumulateFactor(previousScale, factor, prop) {
    if (factor === 1) {
      return 1;
    }
    if (this[prop] > 1 && factor < 1 || this[prop] < 1 && factor > 1) {
      this[prop] = 1;
    }
    const newFactor = Math.floor(previousScale * factor * this[prop] * 100) / (100 * previousScale);
    this[prop] = factor / newFactor;
    return newFactor;
  },
  _centerAtPos(previousScale, x, y) {
    const {
      pdfViewer
    } = this;
    const scaleDiff = pdfViewer.currentScale / previousScale - 1;
    if (scaleDiff !== 0) {
      const [top, left] = pdfViewer.containerTopLeft;
      pdfViewer.container.scrollLeft += (x - left) * scaleDiff;
      pdfViewer.container.scrollTop += (y - top) * scaleDiff;
    }
  },
  _unblockDocumentLoadEvent() {
    document.blockUnblockOnload?.(false);
    this._unblockDocumentLoadEvent = () => {};
  },
  get scriptingReady() {
    return this.pdfScriptingManager.ready;
  }
};
;
async function loadFakeWorker() {
  GlobalWorkerOptions.workerSrc ||= AppOptions.get("workerSrc");
  await import(/* webpackIgnore: true */ PDFWorker.workerSrc);
}
async function loadPDFBug(self) {
  const {
    debuggerScriptPath
  } = self.appConfig;
  const {
    PDFBug
  } = await import(/* webpackIgnore: true */ debuggerScriptPath);
  self._PDFBug = PDFBug;
}
function reportPageStatsPDFBug({
  pageNumber
}) {
  if (!globalThis.Stats?.enabled) {
    return;
  }
  const pageView = PDFViewerApplication.pdfViewer.getPageView(pageNumber - 1);
  globalThis.Stats.add(pageNumber, pageView?.pdfPage?.stats);
}
function webViewerPageRender({
  pageNumber
}) {
  if (pageNumber === PDFViewerApplication.page) {
    PDFViewerApplication.toolbar?.updateLoadingIndicatorState(true);
  }
}
function webViewerPageRendered({
  pageNumber,
  error
}) {
  if (pageNumber === PDFViewerApplication.page) {
    PDFViewerApplication.toolbar?.updateLoadingIndicatorState(false);
  }
  if (PDFViewerApplication.pdfSidebar?.visibleView === SidebarView.THUMBS) {
    const pageView = PDFViewerApplication.pdfViewer.getPageView(pageNumber - 1);
    const thumbnailView = PDFViewerApplication.pdfThumbnailViewer?.getThumbnail(pageNumber - 1);
    if (pageView) {
      thumbnailView?.setImage(pageView);
    }
  }
  if (error) {
    PDFViewerApplication.l10n.get("pdfjs-rendering-error").then(msg => {
      PDFViewerApplication._otherError(msg, error);
    });
  }
}
function webViewerPageMode({
  mode
}) {
  let view;
  switch (mode) {
    case "thumbs":
      view = SidebarView.THUMBS;
      break;
    case "bookmarks":
    case "outline":
      view = SidebarView.OUTLINE;
      break;
    case "attachments":
      view = SidebarView.ATTACHMENTS;
      break;
    case "layers":
      view = SidebarView.LAYERS;
      break;
    case "none":
      view = SidebarView.NONE;
      break;
    default:
      console.error('Invalid "pagemode" hash parameter: ' + mode);
      return;
  }
  PDFViewerApplication.pdfSidebar?.switchView(view, true);
}
function webViewerNamedAction(evt) {
  switch (evt.action) {
    case "GoToPage":
      PDFViewerApplication.appConfig.toolbar?.pageNumber.select();
      break;
    case "Find":
      if (!PDFViewerApplication.supportsIntegratedFind) {
        PDFViewerApplication?.findBar.toggle();
      }
      break;
    case "Print":
      PDFViewerApplication.triggerPrinting();
      break;
    case "SaveAs":
      PDFViewerApplication.downloadOrSave();
      break;
  }
}
function webViewerPresentationModeChanged(evt) {
  PDFViewerApplication.pdfViewer.presentationModeState = evt.state;
}
function webViewerSidebarViewChanged({
  view
}) {
  PDFViewerApplication.pdfRenderingQueue.isThumbnailViewEnabled = view === SidebarView.THUMBS;
  if (PDFViewerApplication.isInitialViewSet) {
    PDFViewerApplication.store?.set("sidebarView", view).catch(() => {});
  }
}
function webViewerUpdateViewarea({
  location
}) {
  if (PDFViewerApplication.isInitialViewSet) {
    PDFViewerApplication.store?.setMultiple({
      page: location.pageNumber,
      zoom: location.scale,
      scrollLeft: location.left,
      scrollTop: location.top,
      rotation: location.rotation
    }).catch(() => {});
  }
  if (PDFViewerApplication.appConfig.secondaryToolbar) {
    const href = PDFViewerApplication.pdfLinkService.getAnchorUrl(location.pdfOpenParams);
    PDFViewerApplication.appConfig.secondaryToolbar.viewBookmarkButton.href = href;
  }
}
function webViewerScrollModeChanged(evt) {
  if (PDFViewerApplication.isInitialViewSet && !PDFViewerApplication.pdfViewer.isInPresentationMode) {
    PDFViewerApplication.store?.set("scrollMode", evt.mode).catch(() => {});
  }
}
function webViewerSpreadModeChanged(evt) {
  if (PDFViewerApplication.isInitialViewSet && !PDFViewerApplication.pdfViewer.isInPresentationMode) {
    PDFViewerApplication.store?.set("spreadMode", evt.mode).catch(() => {});
  }
}
function webViewerResize() {
  const {
    pdfDocument,
    pdfViewer,
    pdfRenderingQueue
  } = PDFViewerApplication;
  if (pdfRenderingQueue.printing && window.matchMedia("print").matches) {
    return;
  }
  if (!pdfDocument) {
    return;
  }
  const currentScaleValue = pdfViewer.currentScaleValue;
  if (currentScaleValue === "auto" || currentScaleValue === "page-fit" || currentScaleValue === "page-width") {
    pdfViewer.currentScaleValue = currentScaleValue;
  }
  pdfViewer.update();
}
function webViewerHashchange(evt) {
  const hash = evt.hash;
  if (!hash) {
    return;
  }
  if (!PDFViewerApplication.isInitialViewSet) {
    PDFViewerApplication.initialBookmark = hash;
  } else if (!PDFViewerApplication.pdfHistory?.popStateInProgress) {
    PDFViewerApplication.pdfLinkService.setHash(hash);
  }
}
;
function webViewerPresentationMode() {
  PDFViewerApplication.requestPresentationMode();
}
function webViewerSwitchAnnotationEditorMode(evt) {
  PDFViewerApplication.pdfViewer.annotationEditorMode = evt;
}
function webViewerSwitchAnnotationEditorParams(evt) {
  PDFViewerApplication.pdfViewer.annotationEditorParams = evt;
}
function webViewerPrint() {
  PDFViewerApplication.triggerPrinting();
}
function webViewerDownload() {
  PDFViewerApplication.downloadOrSave();
}
function webViewerOpenInExternalApp() {
  PDFViewerApplication.openInExternalApp();
}
function webViewerFirstPage() {
  PDFViewerApplication.page = 1;
}
function webViewerLastPage() {
  PDFViewerApplication.page = PDFViewerApplication.pagesCount;
}
function webViewerNextPage() {
  PDFViewerApplication.pdfViewer.nextPage();
}
function webViewerPreviousPage() {
  PDFViewerApplication.pdfViewer.previousPage();
}
function webViewerZoomIn() {
  PDFViewerApplication.zoomIn();
}
function webViewerZoomOut() {
  PDFViewerApplication.zoomOut();
}
function webViewerZoomReset() {
  PDFViewerApplication.zoomReset();
}
function webViewerPageNumberChanged(evt) {
  const pdfViewer = PDFViewerApplication.pdfViewer;
  if (evt.value !== "") {
    PDFViewerApplication.pdfLinkService.goToPage(evt.value);
  }
  if (evt.value !== pdfViewer.currentPageNumber.toString() && evt.value !== pdfViewer.currentPageLabel) {
    PDFViewerApplication.toolbar?.setPageNumber(pdfViewer.currentPageNumber, pdfViewer.currentPageLabel);
  }
}
function webViewerScaleChanged(evt) {
  PDFViewerApplication.pdfViewer.currentScaleValue = evt.value;
}
function webViewerRotateCw() {
  PDFViewerApplication.rotatePages(90);
}
function webViewerRotateCcw() {
  PDFViewerApplication.rotatePages(-90);
}
function webViewerOptionalContentConfig(evt) {
  PDFViewerApplication.pdfViewer.optionalContentConfigPromise = evt.promise;
}
function webViewerSwitchScrollMode(evt) {
  PDFViewerApplication.pdfViewer.scrollMode = evt.mode;
}
function webViewerSwitchSpreadMode(evt) {
  PDFViewerApplication.pdfViewer.spreadMode = evt.mode;
}
function webViewerDocumentProperties() {
  PDFViewerApplication.pdfDocumentProperties?.open();
}
function webViewerFindFromUrlHash(evt) {
  PDFViewerApplication.eventBus.dispatch("find", {
    source: evt.source,
    type: "",
    query: evt.query,
    caseSensitive: false,
    entireWord: false,
    highlightAll: true,
    findPrevious: false,
    matchDiacritics: true
  });
}
function webViewerUpdateFindMatchesCount({
  matchesCount
}) {
  if (PDFViewerApplication.supportsIntegratedFind) {
    PDFViewerApplication.externalServices.updateFindMatchesCount(matchesCount);
  } else {
    PDFViewerApplication.findBar.updateResultsCount(matchesCount);
  }
}
function webViewerUpdateFindControlState({
  state,
  previous,
  matchesCount,
  rawQuery
}) {
  if (PDFViewerApplication.supportsIntegratedFind) {
    PDFViewerApplication.externalServices.updateFindControlState({
      result: state,
      findPrevious: previous,
      matchesCount,
      rawQuery
    });
  } else {
    PDFViewerApplication.findBar?.updateUIState(state, previous, matchesCount);
  }
}
function webViewerScaleChanging(evt) {
  PDFViewerApplication.toolbar?.setPageScale(evt.presetValue, evt.scale);
  PDFViewerApplication.pdfViewer.update();
}
function webViewerRotationChanging(evt) {
  if (PDFViewerApplication.pdfThumbnailViewer) {
    PDFViewerApplication.pdfThumbnailViewer.pagesRotation = evt.pagesRotation;
  }
  PDFViewerApplication.forceRendering();
  PDFViewerApplication.pdfViewer.currentPageNumber = evt.pageNumber;
}
function webViewerPageChanging({
  pageNumber,
  pageLabel
}) {
  PDFViewerApplication.toolbar?.setPageNumber(pageNumber, pageLabel);
  PDFViewerApplication.secondaryToolbar?.setPageNumber(pageNumber);
  if (PDFViewerApplication.pdfSidebar?.visibleView === SidebarView.THUMBS) {
    PDFViewerApplication.pdfThumbnailViewer?.scrollThumbnailIntoView(pageNumber);
  }
  const currentPage = PDFViewerApplication.pdfViewer.getPageView(pageNumber - 1);
  PDFViewerApplication.toolbar?.updateLoadingIndicatorState(currentPage?.renderingState === RenderingStates.RUNNING);
}
function webViewerResolutionChange(evt) {
  PDFViewerApplication.pdfViewer.refresh();
}
function webViewerVisibilityChange(evt) {
  if (document.visibilityState === "visible") {
    setZoomDisabledTimeout();
  }
}
let zoomDisabledTimeout = null;
function setZoomDisabledTimeout() {
  if (zoomDisabledTimeout) {
    clearTimeout(zoomDisabledTimeout);
  }
  zoomDisabledTimeout = setTimeout(function () {
    zoomDisabledTimeout = null;
  }, WHEEL_ZOOM_DISABLED_TIMEOUT);
}
function webViewerWheel(evt) {
  const {
    pdfViewer,
    supportsMouseWheelZoomCtrlKey,
    supportsMouseWheelZoomMetaKey,
    supportsPinchToZoom
  } = PDFViewerApplication;
  if (pdfViewer.isInPresentationMode) {
    return;
  }
  const deltaMode = evt.deltaMode;
  let scaleFactor = Math.exp(-evt.deltaY / 100);
  const isBuiltInMac = FeatureTest.platform.isMac;
  const isPinchToZoom = evt.ctrlKey && !PDFViewerApplication._isCtrlKeyDown && deltaMode === WheelEvent.DOM_DELTA_PIXEL && evt.deltaX === 0 && (Math.abs(scaleFactor - 1) < 0.05 || isBuiltInMac) && evt.deltaZ === 0;
  if (isPinchToZoom || evt.ctrlKey && supportsMouseWheelZoomCtrlKey || evt.metaKey && supportsMouseWheelZoomMetaKey) {
    evt.preventDefault();
    if (zoomDisabledTimeout || document.visibilityState === "hidden" || PDFViewerApplication.overlayManager.active) {
      return;
    }
    const previousScale = pdfViewer.currentScale;
    if (isPinchToZoom && supportsPinchToZoom) {
      scaleFactor = PDFViewerApplication._accumulateFactor(previousScale, scaleFactor, "_wheelUnusedFactor");
      if (scaleFactor < 1) {
        PDFViewerApplication.zoomOut(null, scaleFactor);
      } else if (scaleFactor > 1) {
        PDFViewerApplication.zoomIn(null, scaleFactor);
      } else {
        return;
      }
    } else {
      const delta = normalizeWheelEventDirection(evt);
      let ticks = 0;
      if (deltaMode === WheelEvent.DOM_DELTA_LINE || deltaMode === WheelEvent.DOM_DELTA_PAGE) {
        if (Math.abs(delta) >= 1) {
          ticks = Math.sign(delta);
        } else {
          ticks = PDFViewerApplication._accumulateTicks(delta, "_wheelUnusedTicks");
        }
      } else {
        const PIXELS_PER_LINE_SCALE = 30;
        ticks = PDFViewerApplication._accumulateTicks(delta / PIXELS_PER_LINE_SCALE, "_wheelUnusedTicks");
      }
      if (ticks < 0) {
        PDFViewerApplication.zoomOut(-ticks);
      } else if (ticks > 0) {
        PDFViewerApplication.zoomIn(ticks);
      } else {
        return;
      }
    }
    PDFViewerApplication._centerAtPos(previousScale, evt.clientX, evt.clientY);
  } else {
    setZoomDisabledTimeout();
  }
}
function webViewerTouchStart(evt) {
  if (PDFViewerApplication.pdfViewer.isInPresentationMode || evt.touches.length < 2) {
    return;
  }
  evt.preventDefault();
  if (evt.touches.length !== 2 || PDFViewerApplication.overlayManager.active) {
    PDFViewerApplication._touchInfo = null;
    return;
  }
  let [touch0, touch1] = evt.touches;
  if (touch0.identifier > touch1.identifier) {
    [touch0, touch1] = [touch1, touch0];
  }
  PDFViewerApplication._touchInfo = {
    touch0X: touch0.pageX,
    touch0Y: touch0.pageY,
    touch1X: touch1.pageX,
    touch1Y: touch1.pageY
  };
}
function webViewerTouchMove(evt) {
  if (!PDFViewerApplication._touchInfo || evt.touches.length !== 2) {
    return;
  }
  const {
    pdfViewer,
    _touchInfo,
    supportsPinchToZoom
  } = PDFViewerApplication;
  let [touch0, touch1] = evt.touches;
  if (touch0.identifier > touch1.identifier) {
    [touch0, touch1] = [touch1, touch0];
  }
  const {
    pageX: page0X,
    pageY: page0Y
  } = touch0;
  const {
    pageX: page1X,
    pageY: page1Y
  } = touch1;
  const {
    touch0X: pTouch0X,
    touch0Y: pTouch0Y,
    touch1X: pTouch1X,
    touch1Y: pTouch1Y
  } = _touchInfo;
  if (Math.abs(pTouch0X - page0X) <= 1 && Math.abs(pTouch0Y - page0Y) <= 1 && Math.abs(pTouch1X - page1X) <= 1 && Math.abs(pTouch1Y - page1Y) <= 1) {
    return;
  }
  _touchInfo.touch0X = page0X;
  _touchInfo.touch0Y = page0Y;
  _touchInfo.touch1X = page1X;
  _touchInfo.touch1Y = page1Y;
  if (pTouch0X === page0X && pTouch0Y === page0Y) {
    const v1X = pTouch1X - page0X;
    const v1Y = pTouch1Y - page0Y;
    const v2X = page1X - page0X;
    const v2Y = page1Y - page0Y;
    const det = v1X * v2Y - v1Y * v2X;
    if (Math.abs(det) > 0.02 * Math.hypot(v1X, v1Y) * Math.hypot(v2X, v2Y)) {
      return;
    }
  } else if (pTouch1X === page1X && pTouch1Y === page1Y) {
    const v1X = pTouch0X - page1X;
    const v1Y = pTouch0Y - page1Y;
    const v2X = page0X - page1X;
    const v2Y = page0Y - page1Y;
    const det = v1X * v2Y - v1Y * v2X;
    if (Math.abs(det) > 0.02 * Math.hypot(v1X, v1Y) * Math.hypot(v2X, v2Y)) {
      return;
    }
  } else {
    const diff0X = page0X - pTouch0X;
    const diff1X = page1X - pTouch1X;
    const diff0Y = page0Y - pTouch0Y;
    const diff1Y = page1Y - pTouch1Y;
    const dotProduct = diff0X * diff1X + diff0Y * diff1Y;
    if (dotProduct >= 0) {
      return;
    }
  }
  evt.preventDefault();
  const distance = Math.hypot(page0X - page1X, page0Y - page1Y) || 1;
  const pDistance = Math.hypot(pTouch0X - pTouch1X, pTouch0Y - pTouch1Y) || 1;
  const previousScale = pdfViewer.currentScale;
  if (supportsPinchToZoom) {
    const newScaleFactor = PDFViewerApplication._accumulateFactor(previousScale, distance / pDistance, "_touchUnusedFactor");
    if (newScaleFactor < 1) {
      PDFViewerApplication.zoomOut(null, newScaleFactor);
    } else if (newScaleFactor > 1) {
      PDFViewerApplication.zoomIn(null, newScaleFactor);
    } else {
      return;
    }
  } else {
    const PIXELS_PER_LINE_SCALE = 30;
    const ticks = PDFViewerApplication._accumulateTicks((distance - pDistance) / PIXELS_PER_LINE_SCALE, "_touchUnusedTicks");
    if (ticks < 0) {
      PDFViewerApplication.zoomOut(-ticks);
    } else if (ticks > 0) {
      PDFViewerApplication.zoomIn(ticks);
    } else {
      return;
    }
  }
  PDFViewerApplication._centerAtPos(previousScale, (page0X + page1X) / 2, (page0Y + page1Y) / 2);
}
function webViewerTouchEnd(evt) {
  if (!PDFViewerApplication._touchInfo) {
    return;
  }
  evt.preventDefault();
  PDFViewerApplication._touchInfo = null;
  PDFViewerApplication._touchUnusedTicks = 0;
  PDFViewerApplication._touchUnusedFactor = 1;
}
function webViewerClick(evt) {
  if (!PDFViewerApplication.secondaryToolbar?.isOpen) {
    return;
  }
  const appConfig = PDFViewerApplication.appConfig;
  if (PDFViewerApplication.pdfViewer.containsElement(evt.target) || appConfig.toolbar?.container.contains(evt.target) && evt.target !== appConfig.secondaryToolbar?.toggleButton) {
    PDFViewerApplication.secondaryToolbar.close();
  }
}
function webViewerKeyUp(evt) {
  if (evt.key === "Control") {
    PDFViewerApplication._isCtrlKeyDown = false;
  }
}
function webViewerKeyDown(evt) {
  PDFViewerApplication._isCtrlKeyDown = evt.key === "Control";
  if (PDFViewerApplication.overlayManager.active) {
    return;
  }
  const {
    eventBus,
    pdfViewer
  } = PDFViewerApplication;
  const isViewerInPresentationMode = pdfViewer.isInPresentationMode;
  let handled = false,
    ensureViewerFocused = false;
  const cmd = (evt.ctrlKey ? 1 : 0) | (evt.altKey ? 2 : 0) | (evt.shiftKey ? 4 : 0) | (evt.metaKey ? 8 : 0);
  if (cmd === 1 || cmd === 8 || cmd === 5 || cmd === 12) {
    switch (evt.keyCode) {
      case 70:
        if (!PDFViewerApplication.supportsIntegratedFind && !evt.shiftKey) {
          PDFViewerApplication.findBar?.open();
          handled = true;
        }
        break;
      case 71:
        if (!PDFViewerApplication.supportsIntegratedFind) {
          const {
            state
          } = PDFViewerApplication.findController;
          if (state) {
            const newState = {
              source: window,
              type: "again",
              findPrevious: cmd === 5 || cmd === 12
            };
            eventBus.dispatch("find", {
              ...state,
              ...newState
            });
          }
          handled = true;
        }
        break;
      case 61:
      case 107:
      case 187:
      case 171:
        PDFViewerApplication.zoomIn();
        handled = true;
        break;
      case 173:
      case 109:
      case 189:
        PDFViewerApplication.zoomOut();
        handled = true;
        break;
      case 48:
      case 96:
        if (!isViewerInPresentationMode) {
          setTimeout(function () {
            PDFViewerApplication.zoomReset();
          });
          handled = false;
        }
        break;
      case 38:
        if (isViewerInPresentationMode || PDFViewerApplication.page > 1) {
          PDFViewerApplication.page = 1;
          handled = true;
          ensureViewerFocused = true;
        }
        break;
      case 40:
        if (isViewerInPresentationMode || PDFViewerApplication.page < PDFViewerApplication.pagesCount) {
          PDFViewerApplication.page = PDFViewerApplication.pagesCount;
          handled = true;
          ensureViewerFocused = true;
        }
        break;
    }
  }
  if (cmd === 3 || cmd === 10) {
    switch (evt.keyCode) {
      case 80:
        PDFViewerApplication.requestPresentationMode();
        handled = true;
        PDFViewerApplication.externalServices.reportTelemetry({
          type: "buttons",
          data: {
            id: "presentationModeKeyboard"
          }
        });
        break;
      case 71:
        if (PDFViewerApplication.appConfig.toolbar) {
          PDFViewerApplication.appConfig.toolbar.pageNumber.select();
          handled = true;
        }
        break;
    }
  }
  if (handled) {
    if (ensureViewerFocused && !isViewerInPresentationMode) {
      pdfViewer.focus();
    }
    evt.preventDefault();
    return;
  }
  const curElement = getActiveOrFocusedElement();
  const curElementTagName = curElement?.tagName.toUpperCase();
  if (curElementTagName === "INPUT" || curElementTagName === "TEXTAREA" || curElementTagName === "SELECT" || curElementTagName === "BUTTON" && (evt.keyCode === 13 || evt.keyCode === 32) || curElement?.isContentEditable) {
    if (evt.keyCode !== 27) {
      return;
    }
  }
  if (cmd === 0) {
    let turnPage = 0,
      turnOnlyIfPageFit = false;
    switch (evt.keyCode) {
      case 38:
      case 33:
        if (pdfViewer.isVerticalScrollbarEnabled) {
          turnOnlyIfPageFit = true;
        }
        turnPage = -1;
        break;
      case 8:
        if (!isViewerInPresentationMode) {
          turnOnlyIfPageFit = true;
        }
        turnPage = -1;
        break;
      case 37:
        if (pdfViewer.isHorizontalScrollbarEnabled) {
          turnOnlyIfPageFit = true;
        }
      case 75:
      case 80:
        turnPage = -1;
        break;
      case 27:
        if (PDFViewerApplication.secondaryToolbar?.isOpen) {
          PDFViewerApplication.secondaryToolbar.close();
          handled = true;
        }
        if (!PDFViewerApplication.supportsIntegratedFind && PDFViewerApplication.findBar?.opened) {
          PDFViewerApplication.findBar.close();
          handled = true;
        }
        break;
      case 40:
      case 34:
        if (pdfViewer.isVerticalScrollbarEnabled) {
          turnOnlyIfPageFit = true;
        }
        turnPage = 1;
        break;
      case 13:
      case 32:
        if (!isViewerInPresentationMode) {
          turnOnlyIfPageFit = true;
        }
        turnPage = 1;
        break;
      case 39:
        if (pdfViewer.isHorizontalScrollbarEnabled) {
          turnOnlyIfPageFit = true;
        }
      case 74:
      case 78:
        turnPage = 1;
        break;
      case 36:
        if (isViewerInPresentationMode || PDFViewerApplication.page > 1) {
          PDFViewerApplication.page = 1;
          handled = true;
          ensureViewerFocused = true;
        }
        break;
      case 35:
        if (isViewerInPresentationMode || PDFViewerApplication.page < PDFViewerApplication.pagesCount) {
          PDFViewerApplication.page = PDFViewerApplication.pagesCount;
          handled = true;
          ensureViewerFocused = true;
        }
        break;
      case 83:
        PDFViewerApplication.pdfCursorTools?.switchTool(CursorTool.SELECT);
        break;
      case 72:
        PDFViewerApplication.pdfCursorTools?.switchTool(CursorTool.HAND);
        break;
      case 82:
        PDFViewerApplication.rotatePages(90);
        break;
      case 115:
        PDFViewerApplication.pdfSidebar?.toggle();
        break;
    }
    if (turnPage !== 0 && (!turnOnlyIfPageFit || pdfViewer.currentScaleValue === "page-fit")) {
      if (turnPage > 0) {
        pdfViewer.nextPage();
      } else {
        pdfViewer.previousPage();
      }
      handled = true;
    }
  }
  if (cmd === 4) {
    switch (evt.keyCode) {
      case 13:
      case 32:
        if (!isViewerInPresentationMode && pdfViewer.currentScaleValue !== "page-fit") {
          break;
        }
        pdfViewer.previousPage();
        handled = true;
        break;
      case 82:
        PDFViewerApplication.rotatePages(-90);
        break;
    }
  }
  if (!handled && !isViewerInPresentationMode) {
    if (evt.keyCode >= 33 && evt.keyCode <= 40 || evt.keyCode === 32 && curElementTagName !== "BUTTON") {
      ensureViewerFocused = true;
    }
  }
  if (ensureViewerFocused && !pdfViewer.containsElement(curElement)) {
    pdfViewer.focus();
  }
  if (handled) {
    evt.preventDefault();
  }
}
function beforeUnload(evt) {
  evt.preventDefault();
  evt.returnValue = "";
  return false;
}
function webViewerAnnotationEditorStatesChanged(data) {
  PDFViewerApplication.externalServices.updateEditorStates(data);
}
function webViewerReportTelemetry({
  details
}) {
  PDFViewerApplication.externalServices.reportTelemetry(details);
}
const PDFPrintServiceFactory = {
  instance: {
    supportsPrinting: false,
    createPrintService() {
      throw new Error("Not implemented: createPrintService");
    }
  }
};

;// CONCATENATED MODULE: ./web/preferences.js

class BasePreferences {
  #defaults = Object.freeze({
    "annotationEditorMode": 0,
    "annotationMode": 2,
    "cursorToolOnLoad": 0,
    "defaultZoomDelay": 400,
    "defaultZoomValue": "",
    "disablePageLabels": false,
    "enablePermissions": false,
    "enablePrintAutoRotate": true,
    "enableScripting": true,
    "externalLinkTarget": 0,
    "historyUpdateUrl": false,
    "ignoreDestinationZoom": false,
    "forcePageColors": false,
    "pageColorsBackground": "Canvas",
    "pageColorsForeground": "CanvasText",
    "pdfBugEnabled": false,
    "sidebarViewOnLoad": -1,
    "scrollModeOnLoad": -1,
    "spreadModeOnLoad": -1,
    "textLayerMode": 1,
    "viewOnLoad": 0,
    "disableAutoFetch": false,
    "disableFontFace": false,
    "disableRange": false,
    "disableStream": false,
    "enableXfa": true
  });
  #prefs = Object.create(null);
  #initializedPromise = null;
  constructor() {
    if (this.constructor === BasePreferences) {
      throw new Error("Cannot initialize BasePreferences.");
    }
    this.#initializedPromise = this._readFromStorage(this.#defaults).then(({
      browserPrefs,
      prefs
    }) => {
      const BROWSER_PREFS = {
        "canvasMaxAreaInBytes": -1,
        "isInAutomation": false,
        "supportsDocumentFonts": true,
        "supportsIntegratedFind": false,
        "supportsMouseWheelZoomCtrlKey": true,
        "supportsMouseWheelZoomMetaKey": true,
        "supportsPinchToZoom": true
      };
      const options = Object.create(null);
      for (const [name, defaultVal] of Object.entries(BROWSER_PREFS)) {
        const prefVal = browserPrefs?.[name];
        options[name] = typeof prefVal === typeof defaultVal ? prefVal : defaultVal;
      }
      for (const [name, defaultVal] of Object.entries(this.#defaults)) {
        const prefVal = prefs?.[name];
        options[name] = this.#prefs[name] = typeof prefVal === typeof defaultVal ? prefVal : defaultVal;
      }
      AppOptions.setAll(options, true);
    });
  }
  async _writeToStorage(prefObj) {
    throw new Error("Not implemented: _writeToStorage");
  }
  async _readFromStorage(prefObj) {
    throw new Error("Not implemented: _readFromStorage");
  }
  async reset() {
    throw new Error("Please use `about:config` to change preferences.");
  }
  async set(name, value) {
    throw new Error("Please use `about:config` to change preferences.");
  }
  async get(name) {
    await this.#initializedPromise;
    const defaultValue = this.#defaults[name];
    if (defaultValue === undefined) {
      throw new Error(`Get preference: "${name}" is undefined.`);
    }
    return this.#prefs[name] ?? defaultValue;
  }
  get initializedPromise() {
    return this.#initializedPromise;
  }
}

;// CONCATENATED MODULE: ./web/l10n.js
class L10n {
  #dir;
  #lang;
  #l10n;
  constructor({
    lang,
    isRTL
  }, l10n = null) {
    this.#lang = L10n.#fixupLangCode(lang);
    this.#l10n = l10n;
    this.#dir = isRTL ?? L10n.#isRTL(this.#lang) ? "rtl" : "ltr";
  }
  _setL10n(l10n) {
    this.#l10n = l10n;
  }
  getLanguage() {
    return this.#lang;
  }
  getDirection() {
    return this.#dir;
  }
  async get(ids, args = null, fallback) {
    if (Array.isArray(ids)) {
      ids = ids.map(id => ({
        id
      }));
      const messages = await this.#l10n.formatMessages(ids);
      return messages.map(message => message.value);
    }
    const messages = await this.#l10n.formatMessages([{
      id: ids,
      args
    }]);
    return messages?.[0].value || fallback;
  }
  async translate(element) {
    try {
      this.#l10n.connectRoot(element);
      await this.#l10n.translateRoots();
    } catch {}
  }
  pause() {
    this.#l10n.pauseObserving();
  }
  resume() {
    this.#l10n.resumeObserving();
  }
  static #fixupLangCode(langCode) {
    langCode = langCode?.toLowerCase() || "en-us";
    const PARTIAL_LANG_CODES = {
      en: "en-us",
      es: "es-es",
      fy: "fy-nl",
      ga: "ga-ie",
      gu: "gu-in",
      hi: "hi-in",
      hy: "hy-am",
      nb: "nb-no",
      ne: "ne-np",
      nn: "nn-no",
      pa: "pa-in",
      pt: "pt-pt",
      sv: "sv-se",
      zh: "zh-cn"
    };
    return PARTIAL_LANG_CODES[langCode] || langCode;
  }
  static #isRTL(lang) {
    const shortCode = lang.split("-", 1)[0];
    return ["ar", "he", "fa", "ps", "ur"].includes(shortCode);
  }
}

;// CONCATENATED MODULE: ./web/firefoxcom.js





;
class FirefoxCom {
  static requestSync(action, data) {
    const request = document.createTextNode("");
    document.documentElement.append(request);
    const sender = new CustomEvent("pdf.js.message", {
      bubbles: true,
      cancelable: false,
      detail: {
        action,
        data,
        sync: true
      }
    });
    request.dispatchEvent(sender);
    const response = sender.detail.response;
    request.remove();
    return response;
  }
  static requestAsync(action, data) {
    return new Promise(resolve => {
      this.request(action, data, resolve);
    });
  }
  static request(action, data, callback = null) {
    const request = document.createTextNode("");
    if (callback) {
      request.addEventListener("pdf.js.response", event => {
        const response = event.detail.response;
        event.target.remove();
        callback(response);
      }, {
        once: true
      });
    }
    document.documentElement.append(request);
    const sender = new CustomEvent("pdf.js.message", {
      bubbles: true,
      cancelable: false,
      detail: {
        action,
        data,
        sync: false,
        responseExpected: !!callback
      }
    });
    request.dispatchEvent(sender);
  }
}
class DownloadManager {
  #openBlobUrls = new WeakMap();
  downloadUrl(url, filename, options = {}) {
    FirefoxCom.request("download", {
      originalUrl: url,
      filename,
      options
    });
  }
  downloadData(data, filename, contentType) {
    const blobUrl = URL.createObjectURL(new Blob([data], {
      type: contentType
    }));
    FirefoxCom.request("download", {
      blobUrl,
      originalUrl: blobUrl,
      filename,
      isAttachment: true
    });
  }
  openOrDownloadData(data, filename, dest = null) {
    const isPdfData = isPdfFile(filename);
    const contentType = isPdfData ? "application/pdf" : "";
    if (isPdfData) {
      let blobUrl = this.#openBlobUrls.get(data);
      if (!blobUrl) {
        blobUrl = URL.createObjectURL(new Blob([data], {
          type: contentType
        }));
        this.#openBlobUrls.set(data, blobUrl);
      }
      let viewerUrl = blobUrl + "?filename=" + encodeURIComponent(filename);
      if (dest) {
        viewerUrl += `#${escape(dest)}`;
      }
      try {
        window.open(viewerUrl);
        return true;
      } catch (ex) {
        console.error(`openOrDownloadData: ${ex}`);
        URL.revokeObjectURL(blobUrl);
        this.#openBlobUrls.delete(data);
      }
    }
    this.downloadData(data, filename, contentType);
    return false;
  }
  download(blob, url, filename, options = {}) {
    const blobUrl = URL.createObjectURL(blob);
    FirefoxCom.request("download", {
      blobUrl,
      originalUrl: url,
      filename,
      options
    });
  }
}
class FirefoxPreferences extends BasePreferences {
  async _readFromStorage(prefObj) {
    return FirefoxCom.requestAsync("getPreferences", prefObj);
  }
}
(function listenFindEvents() {
  const events = ["find", "findagain", "findhighlightallchange", "findcasesensitivitychange", "findentirewordchange", "findbarclose", "finddiacriticmatchingchange"];
  const findLen = "find".length;
  const handleEvent = function ({
    type,
    detail
  }) {
    if (!PDFViewerApplication.initialized) {
      return;
    }
    if (type === "findbarclose") {
      PDFViewerApplication.eventBus.dispatch(type, {
        source: window
      });
      return;
    }
    PDFViewerApplication.eventBus.dispatch("find", {
      source: window,
      type: type.substring(findLen),
      query: detail.query,
      caseSensitive: !!detail.caseSensitive,
      entireWord: !!detail.entireWord,
      highlightAll: !!detail.highlightAll,
      findPrevious: !!detail.findPrevious,
      matchDiacritics: !!detail.matchDiacritics
    });
  };
  for (const event of events) {
    window.addEventListener(event, handleEvent);
  }
})();
(function listenZoomEvents() {
  const events = ["zoomin", "zoomout", "zoomreset"];
  const handleEvent = function ({
    type,
    detail
  }) {
    if (!PDFViewerApplication.initialized) {
      return;
    }
    if (type === "zoomreset" && PDFViewerApplication.pdfViewer.currentScaleValue === DEFAULT_SCALE_VALUE) {
      return;
    }
    PDFViewerApplication.eventBus.dispatch(type, {
      source: window
    });
  };
  for (const event of events) {
    window.addEventListener(event, handleEvent);
  }
})();
(function listenSaveEvent() {
  const handleEvent = function ({
    type,
    detail
  }) {
    if (!PDFViewerApplication.initialized) {
      return;
    }
    PDFViewerApplication.eventBus.dispatch("download", {
      source: window
    });
  };
  window.addEventListener("save", handleEvent);
})();
(function listenEditingEvent() {
  const handleEvent = function ({
    detail
  }) {
    if (!PDFViewerApplication.initialized) {
      return;
    }
    PDFViewerApplication.eventBus.dispatch("editingaction", {
      source: window,
      name: detail.name
    });
  };
  window.addEventListener("editingaction", handleEvent);
})();
{
  (function listenQueryEvents() {
    window.addEventListener("pdf.js.query", async ({
      detail: {
        queryId
      }
    }) => {
      let result = null;
      if (queryId === "canDownloadInsteadOfPrint") {
        result = false;
        const {
          pdfDocument,
          pdfViewer
        } = PDFViewerApplication;
        if (pdfDocument) {
          try {
            const hasUnchangedAnnotations = pdfDocument.annotationStorage.size === 0;
            const hasWillPrint = pdfViewer.enableScripting && !!(await pdfDocument.getJSActions())?.WillPrint;
            const hasUnchangedOptionalContent = (await pdfViewer.optionalContentConfigPromise).hasInitialVisibility;
            result = hasUnchangedAnnotations && !hasWillPrint && hasUnchangedOptionalContent;
          } catch {
            console.warn("Unable to check if the document can be downloaded.");
          }
        }
      }
      window.dispatchEvent(new CustomEvent("pdf.js.query.answer", {
        bubbles: true,
        cancelable: false,
        detail: {
          queryId,
          value: result
        }
      }));
    });
  })();
}
class FirefoxComDataRangeTransport extends PDFDataRangeTransport {
  requestDataRange(begin, end) {
    FirefoxCom.request("requestDataRange", {
      begin,
      end
    });
  }
  abort() {
    FirefoxCom.requestSync("abortLoading", null);
  }
}
class FirefoxScripting {
  static async createSandbox(data) {
    const success = await FirefoxCom.requestAsync("createSandbox", data);
    if (!success) {
      throw new Error("Cannot create sandbox.");
    }
  }
  static async dispatchEventInSandbox(event) {
    FirefoxCom.request("dispatchEventInSandbox", event);
  }
  static async destroySandbox() {
    FirefoxCom.request("destroySandbox", null);
  }
}
class FirefoxExternalServices extends DefaultExternalServices {
  static updateFindControlState(data) {
    FirefoxCom.request("updateFindControlState", data);
  }
  static updateFindMatchesCount(data) {
    FirefoxCom.request("updateFindMatchesCount", data);
  }
  static initPassiveLoading(callbacks) {
    let pdfDataRangeTransport;
    window.addEventListener("message", function windowMessage(e) {
      if (e.source !== null) {
        console.warn("Rejected untrusted message from " + e.origin);
        return;
      }
      const args = e.data;
      if (typeof args !== "object" || !("pdfjsLoadAction" in args)) {
        return;
      }
      switch (args.pdfjsLoadAction) {
        case "supportsRangedLoading":
          if (args.done && !args.data) {
            callbacks.onError();
            break;
          }
          pdfDataRangeTransport = new FirefoxComDataRangeTransport(args.length, args.data, args.done, args.filename);
          callbacks.onOpenWithTransport(pdfDataRangeTransport);
          break;
        case "range":
          pdfDataRangeTransport.onDataRange(args.begin, args.chunk);
          break;
        case "rangeProgress":
          pdfDataRangeTransport.onDataProgress(args.loaded);
          break;
        case "progressiveRead":
          pdfDataRangeTransport.onDataProgressiveRead(args.chunk);
          pdfDataRangeTransport.onDataProgress(args.loaded, args.total);
          break;
        case "progressiveDone":
          pdfDataRangeTransport?.onDataProgressiveDone();
          break;
        case "progress":
          callbacks.onProgress(args.loaded, args.total);
          break;
        case "complete":
          if (!args.data) {
            callbacks.onError(args.errorCode);
            break;
          }
          callbacks.onOpenWithData(args.data, args.filename);
          break;
      }
    });
    FirefoxCom.requestSync("initPassiveLoading", null);
  }
  static reportTelemetry(data) {
    FirefoxCom.request("reportTelemetry", JSON.stringify(data));
  }
  static createDownloadManager() {
    return new DownloadManager();
  }
  static createPreferences() {
    return new FirefoxPreferences();
  }
  static updateEditorStates(data) {
    FirefoxCom.request("updateEditorStates", data);
  }
  static async createL10n() {
    const [localeProperties] = await Promise.all([FirefoxCom.requestAsync("getLocaleProperties", null), document.l10n.ready]);
    return new L10n(localeProperties, document.l10n);
  }
  static createScripting(options) {
    return FirefoxScripting;
  }
  static async getNimbusExperimentData() {
    const nimbusData = await FirefoxCom.requestAsync("getNimbusExperimentData", null);
    return nimbusData && JSON.parse(nimbusData);
  }
}
PDFViewerApplication.externalServices = FirefoxExternalServices;

;// CONCATENATED MODULE: ./web/print_utils.js



function getXfaHtmlForPrinting(printContainer, pdfDocument) {
  const xfaHtml = pdfDocument.allXfaHtml;
  const linkService = new SimpleLinkService();
  const scale = Math.round(PixelsPerInch.PDF_TO_CSS_UNITS * 100) / 100;
  for (const xfaPage of xfaHtml.children) {
    const page = document.createElement("div");
    page.className = "xfaPrintedPage";
    printContainer.append(page);
    const builder = new XfaLayerBuilder({
      pdfPage: null,
      annotationStorage: pdfDocument.annotationStorage,
      linkService,
      xfaHtml: xfaPage
    });
    const viewport = getXfaPageViewport(xfaPage, {
      scale
    });
    builder.render(viewport, "print");
    page.append(builder.div);
  }
}

;// CONCATENATED MODULE: ./web/firefox_print_service.js



function composePage(pdfDocument, pageNumber, size, printContainer, printResolution, optionalContentConfigPromise, printAnnotationStoragePromise) {
  const canvas = document.createElement("canvas");
  const PRINT_UNITS = printResolution / PixelsPerInch.PDF;
  canvas.width = Math.floor(size.width * PRINT_UNITS);
  canvas.height = Math.floor(size.height * PRINT_UNITS);
  const canvasWrapper = document.createElement("div");
  canvasWrapper.className = "printedPage";
  canvasWrapper.append(canvas);
  printContainer.append(canvasWrapper);
  let currentRenderTask = null;
  canvas.mozPrintCallback = function (obj) {
    const ctx = obj.context;
    ctx.save();
    ctx.fillStyle = "rgb(255, 255, 255)";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.restore();
    let thisRenderTask = null;
    Promise.all([pdfDocument.getPage(pageNumber), printAnnotationStoragePromise]).then(function ([pdfPage, printAnnotationStorage]) {
      if (currentRenderTask) {
        currentRenderTask.cancel();
        currentRenderTask = null;
      }
      const renderContext = {
        canvasContext: ctx,
        transform: [PRINT_UNITS, 0, 0, PRINT_UNITS, 0, 0],
        viewport: pdfPage.getViewport({
          scale: 1,
          rotation: size.rotation
        }),
        intent: "print",
        annotationMode: AnnotationMode.ENABLE_STORAGE,
        optionalContentConfigPromise,
        printAnnotationStorage
      };
      currentRenderTask = thisRenderTask = pdfPage.render(renderContext);
      return thisRenderTask.promise;
    }).then(function () {
      if (currentRenderTask === thisRenderTask) {
        currentRenderTask = null;
      }
      obj.done();
    }, function (reason) {
      if (!(reason instanceof RenderingCancelledException)) {
        console.error(reason);
      }
      if (currentRenderTask === thisRenderTask) {
        currentRenderTask.cancel();
        currentRenderTask = null;
      }
      if ("abort" in obj) {
        obj.abort();
      } else {
        obj.done();
      }
    });
  };
}
class FirefoxPrintService {
  constructor(pdfDocument, pagesOverview, printContainer, printResolution, optionalContentConfigPromise = null, printAnnotationStoragePromise = null) {
    this.pdfDocument = pdfDocument;
    this.pagesOverview = pagesOverview;
    this.printContainer = printContainer;
    this._printResolution = printResolution || 150;
    this._optionalContentConfigPromise = optionalContentConfigPromise || pdfDocument.getOptionalContentConfig();
    this._printAnnotationStoragePromise = printAnnotationStoragePromise || Promise.resolve();
  }
  layout() {
    const {
      pdfDocument,
      pagesOverview,
      printContainer,
      _printResolution,
      _optionalContentConfigPromise,
      _printAnnotationStoragePromise
    } = this;
    const body = document.querySelector("body");
    body.setAttribute("data-pdfjsprinting", true);
    const {
      width,
      height
    } = this.pagesOverview[0];
    const hasEqualPageSizes = this.pagesOverview.every(size => size.width === width && size.height === height);
    if (!hasEqualPageSizes) {
      console.warn("Not all pages have the same size. The printed result may be incorrect!");
    }
    this.pageStyleSheet = document.createElement("style");
    this.pageStyleSheet.textContent = `@page { size: ${width}pt ${height}pt;}`;
    body.append(this.pageStyleSheet);
    if (pdfDocument.isPureXfa) {
      getXfaHtmlForPrinting(printContainer, pdfDocument);
      return;
    }
    for (let i = 0, ii = pagesOverview.length; i < ii; ++i) {
      composePage(pdfDocument, i + 1, pagesOverview[i], printContainer, _printResolution, _optionalContentConfigPromise, _printAnnotationStoragePromise);
    }
  }
  destroy() {
    this.printContainer.textContent = "";
    const body = document.querySelector("body");
    body.removeAttribute("data-pdfjsprinting");
    if (this.pageStyleSheet) {
      this.pageStyleSheet.remove();
      this.pageStyleSheet = null;
    }
  }
}
PDFPrintServiceFactory.instance = {
  get supportsPrinting() {
    const canvas = document.createElement("canvas");
    const value = ("mozPrintCallback" in canvas);
    return shadow(this, "supportsPrinting", value);
  },
  createPrintService(pdfDocument, pagesOverview, printContainer, printResolution, optionalContentConfigPromise, printAnnotationStoragePromise) {
    return new FirefoxPrintService(pdfDocument, pagesOverview, printContainer, printResolution, optionalContentConfigPromise, printAnnotationStoragePromise);
  }
};

;// CONCATENATED MODULE: ./web/viewer-geckoview.js






const pdfjsVersion = '4.0.240';
const pdfjsBuild = 'ffbfd680e';
const AppConstants = null;
window.PDFViewerApplication = PDFViewerApplication;
window.PDFViewerApplicationConstants = AppConstants;
window.PDFViewerApplicationOptions = AppOptions;
function getViewerConfiguration() {
  const mainContainer = document.getElementById("viewerContainer");
  return {
    appContainer: document.body,
    mainContainer,
    viewerContainer: document.getElementById("viewer"),
    toolbar: {
      mainContainer,
      container: document.getElementById("floatingToolbar"),
      download: document.getElementById("download"),
      openInApp: document.getElementById("openInApp")
    },
    passwordOverlay: {
      dialog: document.getElementById("passwordDialog"),
      label: document.getElementById("passwordText"),
      input: document.getElementById("password"),
      submitButton: document.getElementById("passwordSubmit"),
      cancelButton: document.getElementById("passwordCancel")
    },
    printContainer: document.getElementById("printContainer"),
    openFileInput: null
  };
}
function webViewerLoad() {
  const config = getViewerConfiguration();
  PDFViewerApplication.run(config);
}
document.blockUnblockOnload?.(true);
if (document.readyState === "interactive" || document.readyState === "complete") {
  webViewerLoad();
} else {
  document.addEventListener("DOMContentLoaded", webViewerLoad, true);
}

var __webpack_exports__PDFViewerApplication = __webpack_exports__.PDFViewerApplication;
var __webpack_exports__PDFViewerApplicationConstants = __webpack_exports__.PDFViewerApplicationConstants;
var __webpack_exports__PDFViewerApplicationOptions = __webpack_exports__.PDFViewerApplicationOptions;
export { __webpack_exports__PDFViewerApplication as PDFViewerApplication, __webpack_exports__PDFViewerApplicationConstants as PDFViewerApplicationConstants, __webpack_exports__PDFViewerApplicationOptions as PDFViewerApplicationOptions };
