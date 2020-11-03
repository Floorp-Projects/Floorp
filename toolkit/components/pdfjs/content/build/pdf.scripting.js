/**
 * @licstart The following is the entire license notice for the
 * Javascript code in this page
 *
 * Copyright 2020 Mozilla Foundation
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
 * Javascript code in this page
 */

(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory();
	else if(typeof define === 'function' && define.amd)
		define("pdfjs-dist/build/pdf.scripting", [], factory);
	else if(typeof exports === 'object')
		exports["pdfjs-dist/build/pdf.scripting"] = factory();
	else
		root["pdfjs-dist/build/pdf.scripting"] = root.pdfjsScripting = factory();
})(this, function() {
return /******/ (() => { // webpackBootstrap
/******/ 	"use strict";
/******/ 	var __webpack_modules__ = ([
/* 0 */
/***/ ((__unused_webpack_module, exports, __w_pdfjs_require__) => {



Object.defineProperty(exports, "__esModule", ({
  value: true
}));
Object.defineProperty(exports, "initSandbox", ({
  enumerable: true,
  get: function () {
    return _initialization.initSandbox;
  }
}));

var _initialization = __w_pdfjs_require__(1);

const pdfjsVersion = '2.7.232';
const pdfjsBuild = '3e52098e2';

/***/ }),
/* 1 */
/***/ ((__unused_webpack_module, exports, __w_pdfjs_require__) => {



Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.initSandbox = initSandbox;

var _aform = __w_pdfjs_require__(2);

var _app = __w_pdfjs_require__(3);

var _console = __w_pdfjs_require__(7);

var _doc = __w_pdfjs_require__(8);

var _field = __w_pdfjs_require__(9);

var _proxy = __w_pdfjs_require__(10);

var _util = __w_pdfjs_require__(11);

function initSandbox(data, extra, out) {
  const proxyHandler = new _proxy.ProxyHandler(data.dispatchEventName);
  const {
    send,
    crackURL
  } = extra;
  const doc = new _doc.Doc({
    send
  });
  const _document = {
    obj: doc,
    wrapped: new Proxy(doc, proxyHandler)
  };
  const app = new _app.App({
    send,
    _document,
    calculationOrder: data.calculationOrder
  });
  const util = new _util.Util({
    crackURL
  });
  const aform = new _aform.AForm(doc, app, util);

  for (const [name, objs] of Object.entries(data.objects)) {
    const obj = objs[0];
    obj.send = send;
    obj.doc = _document.wrapped;
    const field = new _field.Field(obj);
    const wrapped = doc._fields[name] = new Proxy(field, proxyHandler);
    app._objects[obj.id] = {
      obj: field,
      wrapped
    };
  }

  out.global = Object.create(null);
  out.app = new Proxy(app, proxyHandler);
  out.console = new Proxy(new _console.Console({
    send
  }), proxyHandler);
  out.util = new Proxy(util, proxyHandler);

  for (const name of Object.getOwnPropertyNames(_aform.AForm.prototype)) {
    if (name.startsWith("AF")) {
      out[name] = aform[name].bind(aform);
    }
  }
}

/***/ }),
/* 2 */
/***/ ((__unused_webpack_module, exports) => {



Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.AForm = void 0;

class AForm {
  constructor(document, app, util) {
    this._document = document;
    this._app = app;
    this._util = util;
  }

  AFNumber_Format(nDec, sepStyle, negStyle, currStyle, strCurrency, bCurrencyPrepend) {
    const event = this._document._event;

    if (!event.value) {
      return;
    }

    nDec = Math.abs(nDec);
    const value = event.value.trim().replace(",", ".");
    let number = Number.parseFloat(value);

    if (isNaN(number) || !isFinite(number)) {
      number = 0;
    }

    event.value = number.toFixed(nDec);
  }

}

exports.AForm = AForm;

/***/ }),
/* 3 */
/***/ ((__unused_webpack_module, exports, __w_pdfjs_require__) => {



Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.App = void 0;

var _event = __w_pdfjs_require__(4);

var _error = __w_pdfjs_require__(5);

var _pdf_object = __w_pdfjs_require__(6);

class App extends _pdf_object.PDFObject {
  constructor(data) {
    super(data);
    this._document = data._document;
    this._objects = Object.create(null);
    this._eventDispatcher = new _event.EventDispatcher(this._document, data.calculationOrder, this._objects);
    this._isApp = true;
  }

  _dispatchEvent(pdfEvent) {
    this._eventDispatcher.dispatch(pdfEvent);
  }

  get activeDocs() {
    return [this._document.wrapped];
  }

  set activeDocs(_) {
    throw new _error.NotSupportedError("app.activeDocs");
  }

  alert(cMsg, nIcon = 0, nType = 0, cTitle = "PDF.js", oDoc = null, oCheckbox = null) {
    this._send({
      command: "alert",
      value: cMsg
    });
  }

}

exports.App = App;

/***/ }),
/* 4 */
/***/ ((__unused_webpack_module, exports) => {



Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.EventDispatcher = exports.Event = void 0;

class Event {
  constructor(data) {
    this.change = data.change || "";
    this.changeEx = data.changeEx || null;
    this.commitKey = data.commitKey || 0;
    this.fieldFull = data.fieldFull || false;
    this.keyDown = data.keyDown || false;
    this.modifier = data.modifier || false;
    this.name = data.name;
    this.rc = true;
    this.richChange = data.richChange || [];
    this.richChangeEx = data.richChangeEx || [];
    this.richValue = data.richValue || [];
    this.selEnd = data.selEnd || 0;
    this.selStart = data.selStart || 0;
    this.shift = data.shift || false;
    this.source = data.source || null;
    this.target = data.target || null;
    this.targetName = data.targetName || "";
    this.type = "Field";
    this.value = data.value || null;
    this.willCommit = data.willCommit || false;
  }

}

exports.Event = Event;

class EventDispatcher {
  constructor(document, calculationOrder, objects) {
    this._document = document;
    this._calculationOrder = calculationOrder;
    this._objects = objects;
    this._document.obj._eventDispatcher = this;
  }

  dispatch(baseEvent) {
    const id = baseEvent.id;

    if (!(id in this._objects)) {
      return;
    }

    const name = baseEvent.name.replace(" ", "");
    const source = this._objects[id];
    const event = this._document.obj._event = new Event(baseEvent);
    const oldValue = source.obj.value;
    this.runActions(source, source, event, name);

    if (event.rc && oldValue !== event.value) {
      source.wrapped.value = event.value;
    }
  }

  runActions(source, target, event, eventName) {
    event.source = source.wrapped;
    event.target = target.wrapped;
    event.name = eventName;
    event.rc = true;

    if (!target.obj._runActions(event)) {
      return true;
    }

    return event.rc;
  }

}

exports.EventDispatcher = EventDispatcher;

/***/ }),
/* 5 */
/***/ ((__unused_webpack_module, exports) => {



Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.NotSupportedError = void 0;

class NotSupportedError extends Error {
  constructor(name) {
    super(`${name} isn't supported in PDF.js`);
    this.name = "NotSupportedError";
  }

}

exports.NotSupportedError = NotSupportedError;

/***/ }),
/* 6 */
/***/ ((__unused_webpack_module, exports) => {



Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.PDFObject = void 0;

class PDFObject {
  constructor(data) {
    this._expandos = Object.create(null);
    this._send = data.send || null;
    this._id = data.id || null;
  }

}

exports.PDFObject = PDFObject;

/***/ }),
/* 7 */
/***/ ((__unused_webpack_module, exports, __w_pdfjs_require__) => {



Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.Console = void 0;

var _pdf_object = __w_pdfjs_require__(6);

class Console extends _pdf_object.PDFObject {
  clear() {
    this._send({
      id: "clear"
    });
  }

  hide() {}

  println(msg) {
    if (typeof msg === "string") {
      this._send({
        command: "println",
        value: "PDF.js Console:: " + msg
      });
    }
  }

  show() {}

}

exports.Console = Console;

/***/ }),
/* 8 */
/***/ ((__unused_webpack_module, exports, __w_pdfjs_require__) => {



Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.Doc = void 0;

var _pdf_object = __w_pdfjs_require__(6);

class Doc extends _pdf_object.PDFObject {
  constructor(data) {
    super(data);
    this._printParams = null;
    this._fields = Object.create(null);
    this._event = null;
  }

  calculateNow() {
    this._eventDispatcher.calculateNow();
  }

  getField(cName) {
    if (typeof cName !== "string") {
      throw new TypeError("Invalid field name: must be a string");
    }

    if (cName in this._fields) {
      return this._fields[cName];
    }

    for (const [name, field] of Object.entries(this._fields)) {
      if (name.includes(cName)) {
        return field;
      }
    }

    return undefined;
  }

}

exports.Doc = Doc;

/***/ }),
/* 9 */
/***/ ((__unused_webpack_module, exports, __w_pdfjs_require__) => {



Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.Field = void 0;

var _pdf_object = __w_pdfjs_require__(6);

class Field extends _pdf_object.PDFObject {
  constructor(data) {
    super(data);
    this.alignment = data.alignment || "left";
    this.borderStyle = data.borderStyle || "";
    this.buttonAlignX = data.buttonAlignX || 50;
    this.buttonAlignY = data.buttonAlignY || 50;
    this.buttonFitBounds = data.buttonFitBounds;
    this.buttonPosition = data.buttonPosition;
    this.buttonScaleHow = data.buttonScaleHow;
    this.ButtonScaleWhen = data.buttonScaleWhen;
    this.calcOrderIndex = data.calcOrderIndex;
    this.charLimit = data.charLimit;
    this.comb = data.comb;
    this.commitOnSelChange = data.commitOnSelChange;
    this.currentValueIndices = data.currentValueIndices;
    this.defaultStyle = data.defaultStyle;
    this.defaultValue = data.defaultValue;
    this.doNotScroll = data.doNotScroll;
    this.doNotSpellCheck = data.doNotSpellCheck;
    this.delay = data.delay;
    this.display = data.display;
    this.doc = data.doc;
    this.editable = data.editable;
    this.exportValues = data.exportValues;
    this.fileSelect = data.fileSelect;
    this.fillColor = data.fillColor;
    this.hidden = data.hidden;
    this.highlight = data.highlight;
    this.lineWidth = data.lineWidth;
    this.multiline = data.multiline;
    this.multipleSelection = data.multipleSelection;
    this.name = data.name;
    this.numItems = data.numItems;
    this.page = data.page;
    this.password = data.password;
    this.print = data.print;
    this.radiosInUnison = data.radiosInUnison;
    this.readonly = data.readonly;
    this.rect = data.rect;
    this.required = data.required;
    this.richText = data.richText;
    this.richValue = data.richValue;
    this.rotation = data.rotation;
    this.strokeColor = data.strokeColor;
    this.style = data.style;
    this.submitName = data.submitName;
    this.textColor = data.textColor;
    this.textFont = data.textFont;
    this.textSize = data.textSize;
    this.type = data.type;
    this.userName = data.userName;
    this.value = data.value || "";
    this.valueAsString = data.valueAsString;
    this._actions = Object.create(null);
    const doc = this._document = data.doc;

    for (const [eventType, actions] of Object.entries(data.actions)) {
      this._actions[eventType] = actions.map(action => Function("event", `with (this) {${action}}`).bind(doc));
    }
  }

  setAction(cTrigger, cScript) {
    if (typeof cTrigger !== "string" || typeof cScript !== "string") {
      return;
    }

    if (!(cTrigger in this._actions)) {
      this._actions[cTrigger] = [];
    }

    this._actions[cTrigger].push(cScript);
  }

  setFocus() {
    this._send({
      id: this._id,
      focus: true
    });
  }

  _runActions(event) {
    const eventName = event.name;

    if (!(eventName in this._actions)) {
      return false;
    }

    const actions = this._actions[eventName];

    try {
      for (const action of actions) {
        action(event);
      }
    } catch (error) {
      event.rc = false;
      const value = `"${error.toString()}" for event ` + `"${eventName}" in object ${this._id}.` + `\n${error.stack}`;

      this._send({
        command: "error",
        value
      });
    }

    return true;
  }

}

exports.Field = Field;

/***/ }),
/* 10 */
/***/ ((__unused_webpack_module, exports) => {



Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.ProxyHandler = void 0;

class ProxyHandler {
  constructor(dispatchEventName) {
    this.dispatchEventName = dispatchEventName;
  }

  get(obj, prop) {
    if (obj._isApp && prop === this.dispatchEventName) {
      return obj._dispatchEvent.bind(obj);
    }

    if (prop in obj._expandos) {
      const val = obj._expandos[prop];

      if (typeof val === "function") {
        return val.bind(obj);
      }

      return val;
    }

    if (typeof prop === "string" && !prop.startsWith("_") && prop in obj) {
      const val = obj[prop];

      if (typeof val === "function") {
        return val.bind(obj);
      }

      return val;
    }

    return undefined;
  }

  set(obj, prop, value) {
    if (typeof prop === "string" && !prop.startsWith("_") && prop in obj) {
      const old = obj[prop];
      obj[prop] = value;

      if (obj._send && obj._id !== null && typeof old !== "function") {
        const data = {
          id: obj._id
        };
        data[prop] = value;

        obj._send(data);
      }
    } else {
      obj._expandos[prop] = value;
    }

    return true;
  }

  has(obj, prop) {
    return prop in obj._expandos || typeof prop === "string" && !prop.startsWith("_") && prop in obj;
  }

  getPrototypeOf(obj) {
    return null;
  }

  setPrototypeOf(obj, proto) {
    return false;
  }

  isExtensible(obj) {
    return true;
  }

  preventExtensions(obj) {
    return false;
  }

  getOwnPropertyDescriptor(obj, prop) {
    if (prop in obj._expandos) {
      return {
        configurable: true,
        enumerable: true,
        value: obj._expandos[prop]
      };
    }

    if (typeof prop === "string" && !prop.startsWith("_") && prop in obj) {
      return {
        configurable: true,
        enumerable: true,
        value: obj[prop]
      };
    }

    return undefined;
  }

  defineProperty(obj, key, descriptor) {
    Object.defineProperty(obj._expandos, key, descriptor);
    return true;
  }

  deleteProperty(obj, prop) {
    if (prop in obj._expandos) {
      delete obj._expandos[prop];
    }
  }

  ownKeys(obj) {
    const fromExpandos = Reflect.ownKeys(obj._expandos);
    const fromObj = Reflect.ownKeys(obj).filter(k => !k.startsWith("_"));
    return fromExpandos.concat(fromObj);
  }

}

exports.ProxyHandler = ProxyHandler;

/***/ }),
/* 11 */
/***/ ((__unused_webpack_module, exports, __w_pdfjs_require__) => {



Object.defineProperty(exports, "__esModule", ({
  value: true
}));
exports.Util = void 0;

var _pdf_object = __w_pdfjs_require__(6);

class Util extends _pdf_object.PDFObject {
  constructor(data) {
    super(data);
    this._crackURL = data.crackURL;
  }

  crackURL(cURL) {
    return this._crackURL(cURL);
  }

}

exports.Util = Util;

/***/ })
/******/ 	]);
/************************************************************************/
/******/ 	// The module cache
/******/ 	var __webpack_module_cache__ = {};
/******/ 	
/******/ 	// The require function
/******/ 	function __w_pdfjs_require__(moduleId) {
/******/ 		// Check if module is in cache
/******/ 		if(__webpack_module_cache__[moduleId]) {
/******/ 			return __webpack_module_cache__[moduleId].exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = __webpack_module_cache__[moduleId] = {
/******/ 			// no module.id needed
/******/ 			// no module.loaded needed
/******/ 			exports: {}
/******/ 		};
/******/ 	
/******/ 		// Execute the module function
/******/ 		__webpack_modules__[moduleId](module, module.exports, __w_pdfjs_require__);
/******/ 	
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/ 	
/************************************************************************/
/******/ 	// module exports must be returned from runtime so entry inlining is disabled
/******/ 	// startup
/******/ 	// Load entry module and return exports
/******/ 	return __w_pdfjs_require__(0);
/******/ })()
;
});