var CDP =
/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};

/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {

/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;

/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			exports: {},
/******/ 			id: moduleId,
/******/ 			loaded: false
/******/ 		};

/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);

/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;

/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}


/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;

/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;

/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";

/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(1);
	module.exports = __webpack_require__(327);


/***/ }),
/* 1 */
/***/ (function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(global) {"use strict";

	__webpack_require__(2);

	__webpack_require__(323);

	__webpack_require__(324);

	if (global._babelPolyfill) {
	  throw new Error("only one instance of babel-polyfill is allowed");
	}
	global._babelPolyfill = true;

	var DEFINE_PROPERTY = "defineProperty";
	function define(O, key, value) {
	  O[key] || Object[DEFINE_PROPERTY](O, key, {
	    writable: true,
	    configurable: true,
	    value: value
	  });
	}

	define(String.prototype, "padLeft", "".padStart);
	define(String.prototype, "padRight", "".padEnd);

	"pop,reverse,shift,keys,values,entries,indexOf,every,some,forEach,map,filter,find,findIndex,includes,join,slice,concat,push,splice,unshift,sort,lastIndexOf,reduce,reduceRight,copyWithin,fill".split(",").forEach(function (key) {
	  [][key] && define(Array, key, Function.call.bind([][key]));
	});
	/* WEBPACK VAR INJECTION */}.call(exports, (function() { return this; }())))

/***/ }),
/* 2 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(3);
	__webpack_require__(51);
	__webpack_require__(52);
	__webpack_require__(53);
	__webpack_require__(54);
	__webpack_require__(56);
	__webpack_require__(59);
	__webpack_require__(60);
	__webpack_require__(61);
	__webpack_require__(62);
	__webpack_require__(63);
	__webpack_require__(64);
	__webpack_require__(65);
	__webpack_require__(66);
	__webpack_require__(67);
	__webpack_require__(69);
	__webpack_require__(71);
	__webpack_require__(73);
	__webpack_require__(75);
	__webpack_require__(78);
	__webpack_require__(79);
	__webpack_require__(80);
	__webpack_require__(84);
	__webpack_require__(86);
	__webpack_require__(88);
	__webpack_require__(91);
	__webpack_require__(92);
	__webpack_require__(93);
	__webpack_require__(94);
	__webpack_require__(96);
	__webpack_require__(97);
	__webpack_require__(98);
	__webpack_require__(99);
	__webpack_require__(100);
	__webpack_require__(101);
	__webpack_require__(102);
	__webpack_require__(104);
	__webpack_require__(105);
	__webpack_require__(106);
	__webpack_require__(108);
	__webpack_require__(109);
	__webpack_require__(110);
	__webpack_require__(112);
	__webpack_require__(114);
	__webpack_require__(115);
	__webpack_require__(116);
	__webpack_require__(117);
	__webpack_require__(118);
	__webpack_require__(119);
	__webpack_require__(120);
	__webpack_require__(121);
	__webpack_require__(122);
	__webpack_require__(123);
	__webpack_require__(124);
	__webpack_require__(125);
	__webpack_require__(126);
	__webpack_require__(131);
	__webpack_require__(132);
	__webpack_require__(136);
	__webpack_require__(137);
	__webpack_require__(138);
	__webpack_require__(139);
	__webpack_require__(141);
	__webpack_require__(142);
	__webpack_require__(143);
	__webpack_require__(144);
	__webpack_require__(145);
	__webpack_require__(146);
	__webpack_require__(147);
	__webpack_require__(148);
	__webpack_require__(149);
	__webpack_require__(150);
	__webpack_require__(151);
	__webpack_require__(152);
	__webpack_require__(153);
	__webpack_require__(154);
	__webpack_require__(155);
	__webpack_require__(157);
	__webpack_require__(158);
	__webpack_require__(160);
	__webpack_require__(161);
	__webpack_require__(167);
	__webpack_require__(168);
	__webpack_require__(170);
	__webpack_require__(171);
	__webpack_require__(172);
	__webpack_require__(176);
	__webpack_require__(177);
	__webpack_require__(178);
	__webpack_require__(179);
	__webpack_require__(180);
	__webpack_require__(182);
	__webpack_require__(183);
	__webpack_require__(184);
	__webpack_require__(185);
	__webpack_require__(188);
	__webpack_require__(190);
	__webpack_require__(191);
	__webpack_require__(192);
	__webpack_require__(194);
	__webpack_require__(196);
	__webpack_require__(198);
	__webpack_require__(199);
	__webpack_require__(200);
	__webpack_require__(202);
	__webpack_require__(203);
	__webpack_require__(204);
	__webpack_require__(205);
	__webpack_require__(216);
	__webpack_require__(220);
	__webpack_require__(221);
	__webpack_require__(223);
	__webpack_require__(224);
	__webpack_require__(228);
	__webpack_require__(229);
	__webpack_require__(231);
	__webpack_require__(232);
	__webpack_require__(233);
	__webpack_require__(234);
	__webpack_require__(235);
	__webpack_require__(236);
	__webpack_require__(237);
	__webpack_require__(238);
	__webpack_require__(239);
	__webpack_require__(240);
	__webpack_require__(241);
	__webpack_require__(242);
	__webpack_require__(243);
	__webpack_require__(244);
	__webpack_require__(245);
	__webpack_require__(246);
	__webpack_require__(247);
	__webpack_require__(248);
	__webpack_require__(249);
	__webpack_require__(251);
	__webpack_require__(252);
	__webpack_require__(253);
	__webpack_require__(254);
	__webpack_require__(255);
	__webpack_require__(257);
	__webpack_require__(258);
	__webpack_require__(259);
	__webpack_require__(261);
	__webpack_require__(262);
	__webpack_require__(263);
	__webpack_require__(264);
	__webpack_require__(265);
	__webpack_require__(266);
	__webpack_require__(267);
	__webpack_require__(268);
	__webpack_require__(270);
	__webpack_require__(271);
	__webpack_require__(273);
	__webpack_require__(274);
	__webpack_require__(275);
	__webpack_require__(276);
	__webpack_require__(279);
	__webpack_require__(280);
	__webpack_require__(282);
	__webpack_require__(283);
	__webpack_require__(284);
	__webpack_require__(285);
	__webpack_require__(287);
	__webpack_require__(288);
	__webpack_require__(289);
	__webpack_require__(290);
	__webpack_require__(291);
	__webpack_require__(292);
	__webpack_require__(293);
	__webpack_require__(294);
	__webpack_require__(295);
	__webpack_require__(296);
	__webpack_require__(298);
	__webpack_require__(299);
	__webpack_require__(300);
	__webpack_require__(301);
	__webpack_require__(302);
	__webpack_require__(303);
	__webpack_require__(304);
	__webpack_require__(305);
	__webpack_require__(306);
	__webpack_require__(307);
	__webpack_require__(308);
	__webpack_require__(310);
	__webpack_require__(311);
	__webpack_require__(312);
	__webpack_require__(313);
	__webpack_require__(314);
	__webpack_require__(315);
	__webpack_require__(316);
	__webpack_require__(317);
	__webpack_require__(318);
	__webpack_require__(319);
	__webpack_require__(320);
	__webpack_require__(321);
	__webpack_require__(322);
	module.exports = __webpack_require__(9);


/***/ }),
/* 3 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// ECMAScript 6 symbols shim
	var global = __webpack_require__(4);
	var has = __webpack_require__(5);
	var DESCRIPTORS = __webpack_require__(6);
	var $export = __webpack_require__(8);
	var redefine = __webpack_require__(18);
	var META = __webpack_require__(22).KEY;
	var $fails = __webpack_require__(7);
	var shared = __webpack_require__(23);
	var setToStringTag = __webpack_require__(25);
	var uid = __webpack_require__(19);
	var wks = __webpack_require__(26);
	var wksExt = __webpack_require__(27);
	var wksDefine = __webpack_require__(28);
	var enumKeys = __webpack_require__(29);
	var isArray = __webpack_require__(44);
	var anObject = __webpack_require__(12);
	var isObject = __webpack_require__(13);
	var toIObject = __webpack_require__(32);
	var toPrimitive = __webpack_require__(16);
	var createDesc = __webpack_require__(17);
	var _create = __webpack_require__(45);
	var gOPNExt = __webpack_require__(48);
	var $GOPD = __webpack_require__(50);
	var $DP = __webpack_require__(11);
	var $keys = __webpack_require__(30);
	var gOPD = $GOPD.f;
	var dP = $DP.f;
	var gOPN = gOPNExt.f;
	var $Symbol = global.Symbol;
	var $JSON = global.JSON;
	var _stringify = $JSON && $JSON.stringify;
	var PROTOTYPE = 'prototype';
	var HIDDEN = wks('_hidden');
	var TO_PRIMITIVE = wks('toPrimitive');
	var isEnum = {}.propertyIsEnumerable;
	var SymbolRegistry = shared('symbol-registry');
	var AllSymbols = shared('symbols');
	var OPSymbols = shared('op-symbols');
	var ObjectProto = Object[PROTOTYPE];
	var USE_NATIVE = typeof $Symbol == 'function';
	var QObject = global.QObject;
	// Don't use setters in Qt Script, https://github.com/zloirock/core-js/issues/173
	var setter = !QObject || !QObject[PROTOTYPE] || !QObject[PROTOTYPE].findChild;

	// fallback for old Android, https://code.google.com/p/v8/issues/detail?id=687
	var setSymbolDesc = DESCRIPTORS && $fails(function () {
	  return _create(dP({}, 'a', {
	    get: function () { return dP(this, 'a', { value: 7 }).a; }
	  })).a != 7;
	}) ? function (it, key, D) {
	  var protoDesc = gOPD(ObjectProto, key);
	  if (protoDesc) delete ObjectProto[key];
	  dP(it, key, D);
	  if (protoDesc && it !== ObjectProto) dP(ObjectProto, key, protoDesc);
	} : dP;

	var wrap = function (tag) {
	  var sym = AllSymbols[tag] = _create($Symbol[PROTOTYPE]);
	  sym._k = tag;
	  return sym;
	};

	var isSymbol = USE_NATIVE && typeof $Symbol.iterator == 'symbol' ? function (it) {
	  return typeof it == 'symbol';
	} : function (it) {
	  return it instanceof $Symbol;
	};

	var $defineProperty = function defineProperty(it, key, D) {
	  if (it === ObjectProto) $defineProperty(OPSymbols, key, D);
	  anObject(it);
	  key = toPrimitive(key, true);
	  anObject(D);
	  if (has(AllSymbols, key)) {
	    if (!D.enumerable) {
	      if (!has(it, HIDDEN)) dP(it, HIDDEN, createDesc(1, {}));
	      it[HIDDEN][key] = true;
	    } else {
	      if (has(it, HIDDEN) && it[HIDDEN][key]) it[HIDDEN][key] = false;
	      D = _create(D, { enumerable: createDesc(0, false) });
	    } return setSymbolDesc(it, key, D);
	  } return dP(it, key, D);
	};
	var $defineProperties = function defineProperties(it, P) {
	  anObject(it);
	  var keys = enumKeys(P = toIObject(P));
	  var i = 0;
	  var l = keys.length;
	  var key;
	  while (l > i) $defineProperty(it, key = keys[i++], P[key]);
	  return it;
	};
	var $create = function create(it, P) {
	  return P === undefined ? _create(it) : $defineProperties(_create(it), P);
	};
	var $propertyIsEnumerable = function propertyIsEnumerable(key) {
	  var E = isEnum.call(this, key = toPrimitive(key, true));
	  if (this === ObjectProto && has(AllSymbols, key) && !has(OPSymbols, key)) return false;
	  return E || !has(this, key) || !has(AllSymbols, key) || has(this, HIDDEN) && this[HIDDEN][key] ? E : true;
	};
	var $getOwnPropertyDescriptor = function getOwnPropertyDescriptor(it, key) {
	  it = toIObject(it);
	  key = toPrimitive(key, true);
	  if (it === ObjectProto && has(AllSymbols, key) && !has(OPSymbols, key)) return;
	  var D = gOPD(it, key);
	  if (D && has(AllSymbols, key) && !(has(it, HIDDEN) && it[HIDDEN][key])) D.enumerable = true;
	  return D;
	};
	var $getOwnPropertyNames = function getOwnPropertyNames(it) {
	  var names = gOPN(toIObject(it));
	  var result = [];
	  var i = 0;
	  var key;
	  while (names.length > i) {
	    if (!has(AllSymbols, key = names[i++]) && key != HIDDEN && key != META) result.push(key);
	  } return result;
	};
	var $getOwnPropertySymbols = function getOwnPropertySymbols(it) {
	  var IS_OP = it === ObjectProto;
	  var names = gOPN(IS_OP ? OPSymbols : toIObject(it));
	  var result = [];
	  var i = 0;
	  var key;
	  while (names.length > i) {
	    if (has(AllSymbols, key = names[i++]) && (IS_OP ? has(ObjectProto, key) : true)) result.push(AllSymbols[key]);
	  } return result;
	};

	// 19.4.1.1 Symbol([description])
	if (!USE_NATIVE) {
	  $Symbol = function Symbol() {
	    if (this instanceof $Symbol) throw TypeError('Symbol is not a constructor!');
	    var tag = uid(arguments.length > 0 ? arguments[0] : undefined);
	    var $set = function (value) {
	      if (this === ObjectProto) $set.call(OPSymbols, value);
	      if (has(this, HIDDEN) && has(this[HIDDEN], tag)) this[HIDDEN][tag] = false;
	      setSymbolDesc(this, tag, createDesc(1, value));
	    };
	    if (DESCRIPTORS && setter) setSymbolDesc(ObjectProto, tag, { configurable: true, set: $set });
	    return wrap(tag);
	  };
	  redefine($Symbol[PROTOTYPE], 'toString', function toString() {
	    return this._k;
	  });

	  $GOPD.f = $getOwnPropertyDescriptor;
	  $DP.f = $defineProperty;
	  __webpack_require__(49).f = gOPNExt.f = $getOwnPropertyNames;
	  __webpack_require__(43).f = $propertyIsEnumerable;
	  __webpack_require__(42).f = $getOwnPropertySymbols;

	  if (DESCRIPTORS && !__webpack_require__(24)) {
	    redefine(ObjectProto, 'propertyIsEnumerable', $propertyIsEnumerable, true);
	  }

	  wksExt.f = function (name) {
	    return wrap(wks(name));
	  };
	}

	$export($export.G + $export.W + $export.F * !USE_NATIVE, { Symbol: $Symbol });

	for (var es6Symbols = (
	  // 19.4.2.2, 19.4.2.3, 19.4.2.4, 19.4.2.6, 19.4.2.8, 19.4.2.9, 19.4.2.10, 19.4.2.11, 19.4.2.12, 19.4.2.13, 19.4.2.14
	  'hasInstance,isConcatSpreadable,iterator,match,replace,search,species,split,toPrimitive,toStringTag,unscopables'
	).split(','), j = 0; es6Symbols.length > j;)wks(es6Symbols[j++]);

	for (var wellKnownSymbols = $keys(wks.store), k = 0; wellKnownSymbols.length > k;) wksDefine(wellKnownSymbols[k++]);

	$export($export.S + $export.F * !USE_NATIVE, 'Symbol', {
	  // 19.4.2.1 Symbol.for(key)
	  'for': function (key) {
	    return has(SymbolRegistry, key += '')
	      ? SymbolRegistry[key]
	      : SymbolRegistry[key] = $Symbol(key);
	  },
	  // 19.4.2.5 Symbol.keyFor(sym)
	  keyFor: function keyFor(sym) {
	    if (!isSymbol(sym)) throw TypeError(sym + ' is not a symbol!');
	    for (var key in SymbolRegistry) if (SymbolRegistry[key] === sym) return key;
	  },
	  useSetter: function () { setter = true; },
	  useSimple: function () { setter = false; }
	});

	$export($export.S + $export.F * !USE_NATIVE, 'Object', {
	  // 19.1.2.2 Object.create(O [, Properties])
	  create: $create,
	  // 19.1.2.4 Object.defineProperty(O, P, Attributes)
	  defineProperty: $defineProperty,
	  // 19.1.2.3 Object.defineProperties(O, Properties)
	  defineProperties: $defineProperties,
	  // 19.1.2.6 Object.getOwnPropertyDescriptor(O, P)
	  getOwnPropertyDescriptor: $getOwnPropertyDescriptor,
	  // 19.1.2.7 Object.getOwnPropertyNames(O)
	  getOwnPropertyNames: $getOwnPropertyNames,
	  // 19.1.2.8 Object.getOwnPropertySymbols(O)
	  getOwnPropertySymbols: $getOwnPropertySymbols
	});

	// 24.3.2 JSON.stringify(value [, replacer [, space]])
	$JSON && $export($export.S + $export.F * (!USE_NATIVE || $fails(function () {
	  var S = $Symbol();
	  // MS Edge converts symbol values to JSON as {}
	  // WebKit converts symbol values to JSON as null
	  // V8 throws on boxed symbols
	  return _stringify([S]) != '[null]' || _stringify({ a: S }) != '{}' || _stringify(Object(S)) != '{}';
	})), 'JSON', {
	  stringify: function stringify(it) {
	    var args = [it];
	    var i = 1;
	    var replacer, $replacer;
	    while (arguments.length > i) args.push(arguments[i++]);
	    $replacer = replacer = args[1];
	    if (!isObject(replacer) && it === undefined || isSymbol(it)) return; // IE8 returns string on undefined
	    if (!isArray(replacer)) replacer = function (key, value) {
	      if (typeof $replacer == 'function') value = $replacer.call(this, key, value);
	      if (!isSymbol(value)) return value;
	    };
	    args[1] = replacer;
	    return _stringify.apply($JSON, args);
	  }
	});

	// 19.4.3.4 Symbol.prototype[@@toPrimitive](hint)
	$Symbol[PROTOTYPE][TO_PRIMITIVE] || __webpack_require__(10)($Symbol[PROTOTYPE], TO_PRIMITIVE, $Symbol[PROTOTYPE].valueOf);
	// 19.4.3.5 Symbol.prototype[@@toStringTag]
	setToStringTag($Symbol, 'Symbol');
	// 20.2.1.9 Math[@@toStringTag]
	setToStringTag(Math, 'Math', true);
	// 24.3.3 JSON[@@toStringTag]
	setToStringTag(global.JSON, 'JSON', true);


/***/ }),
/* 4 */
/***/ (function(module, exports) {

	// https://github.com/zloirock/core-js/issues/86#issuecomment-115759028
	var global = module.exports = typeof window != 'undefined' && window.Math == Math
	  ? window : typeof self != 'undefined' && self.Math == Math ? self
	  // eslint-disable-next-line no-new-func
	  : Function('return this')();
	if (typeof __g == 'number') __g = global; // eslint-disable-line no-undef


/***/ }),
/* 5 */
/***/ (function(module, exports) {

	var hasOwnProperty = {}.hasOwnProperty;
	module.exports = function (it, key) {
	  return hasOwnProperty.call(it, key);
	};


/***/ }),
/* 6 */
/***/ (function(module, exports, __webpack_require__) {

	// Thank's IE8 for his funny defineProperty
	module.exports = !__webpack_require__(7)(function () {
	  return Object.defineProperty({}, 'a', { get: function () { return 7; } }).a != 7;
	});


/***/ }),
/* 7 */
/***/ (function(module, exports) {

	module.exports = function (exec) {
	  try {
	    return !!exec();
	  } catch (e) {
	    return true;
	  }
	};


/***/ }),
/* 8 */
/***/ (function(module, exports, __webpack_require__) {

	var global = __webpack_require__(4);
	var core = __webpack_require__(9);
	var hide = __webpack_require__(10);
	var redefine = __webpack_require__(18);
	var ctx = __webpack_require__(20);
	var PROTOTYPE = 'prototype';

	var $export = function (type, name, source) {
	  var IS_FORCED = type & $export.F;
	  var IS_GLOBAL = type & $export.G;
	  var IS_STATIC = type & $export.S;
	  var IS_PROTO = type & $export.P;
	  var IS_BIND = type & $export.B;
	  var target = IS_GLOBAL ? global : IS_STATIC ? global[name] || (global[name] = {}) : (global[name] || {})[PROTOTYPE];
	  var exports = IS_GLOBAL ? core : core[name] || (core[name] = {});
	  var expProto = exports[PROTOTYPE] || (exports[PROTOTYPE] = {});
	  var key, own, out, exp;
	  if (IS_GLOBAL) source = name;
	  for (key in source) {
	    // contains in native
	    own = !IS_FORCED && target && target[key] !== undefined;
	    // export native or passed
	    out = (own ? target : source)[key];
	    // bind timers to global for call from export context
	    exp = IS_BIND && own ? ctx(out, global) : IS_PROTO && typeof out == 'function' ? ctx(Function.call, out) : out;
	    // extend global
	    if (target) redefine(target, key, out, type & $export.U);
	    // export
	    if (exports[key] != out) hide(exports, key, exp);
	    if (IS_PROTO && expProto[key] != out) expProto[key] = out;
	  }
	};
	global.core = core;
	// type bitmap
	$export.F = 1;   // forced
	$export.G = 2;   // global
	$export.S = 4;   // static
	$export.P = 8;   // proto
	$export.B = 16;  // bind
	$export.W = 32;  // wrap
	$export.U = 64;  // safe
	$export.R = 128; // real proto method for `library`
	module.exports = $export;


/***/ }),
/* 9 */
/***/ (function(module, exports) {

	var core = module.exports = { version: '2.5.7' };
	if (typeof __e == 'number') __e = core; // eslint-disable-line no-undef


/***/ }),
/* 10 */
/***/ (function(module, exports, __webpack_require__) {

	var dP = __webpack_require__(11);
	var createDesc = __webpack_require__(17);
	module.exports = __webpack_require__(6) ? function (object, key, value) {
	  return dP.f(object, key, createDesc(1, value));
	} : function (object, key, value) {
	  object[key] = value;
	  return object;
	};


/***/ }),
/* 11 */
/***/ (function(module, exports, __webpack_require__) {

	var anObject = __webpack_require__(12);
	var IE8_DOM_DEFINE = __webpack_require__(14);
	var toPrimitive = __webpack_require__(16);
	var dP = Object.defineProperty;

	exports.f = __webpack_require__(6) ? Object.defineProperty : function defineProperty(O, P, Attributes) {
	  anObject(O);
	  P = toPrimitive(P, true);
	  anObject(Attributes);
	  if (IE8_DOM_DEFINE) try {
	    return dP(O, P, Attributes);
	  } catch (e) { /* empty */ }
	  if ('get' in Attributes || 'set' in Attributes) throw TypeError('Accessors not supported!');
	  if ('value' in Attributes) O[P] = Attributes.value;
	  return O;
	};


/***/ }),
/* 12 */
/***/ (function(module, exports, __webpack_require__) {

	var isObject = __webpack_require__(13);
	module.exports = function (it) {
	  if (!isObject(it)) throw TypeError(it + ' is not an object!');
	  return it;
	};


/***/ }),
/* 13 */
/***/ (function(module, exports) {

	module.exports = function (it) {
	  return typeof it === 'object' ? it !== null : typeof it === 'function';
	};


/***/ }),
/* 14 */
/***/ (function(module, exports, __webpack_require__) {

	module.exports = !__webpack_require__(6) && !__webpack_require__(7)(function () {
	  return Object.defineProperty(__webpack_require__(15)('div'), 'a', { get: function () { return 7; } }).a != 7;
	});


/***/ }),
/* 15 */
/***/ (function(module, exports, __webpack_require__) {

	var isObject = __webpack_require__(13);
	var document = __webpack_require__(4).document;
	// typeof document.createElement is 'object' in old IE
	var is = isObject(document) && isObject(document.createElement);
	module.exports = function (it) {
	  return is ? document.createElement(it) : {};
	};


/***/ }),
/* 16 */
/***/ (function(module, exports, __webpack_require__) {

	// 7.1.1 ToPrimitive(input [, PreferredType])
	var isObject = __webpack_require__(13);
	// instead of the ES6 spec version, we didn't implement @@toPrimitive case
	// and the second argument - flag - preferred type is a string
	module.exports = function (it, S) {
	  if (!isObject(it)) return it;
	  var fn, val;
	  if (S && typeof (fn = it.toString) == 'function' && !isObject(val = fn.call(it))) return val;
	  if (typeof (fn = it.valueOf) == 'function' && !isObject(val = fn.call(it))) return val;
	  if (!S && typeof (fn = it.toString) == 'function' && !isObject(val = fn.call(it))) return val;
	  throw TypeError("Can't convert object to primitive value");
	};


/***/ }),
/* 17 */
/***/ (function(module, exports) {

	module.exports = function (bitmap, value) {
	  return {
	    enumerable: !(bitmap & 1),
	    configurable: !(bitmap & 2),
	    writable: !(bitmap & 4),
	    value: value
	  };
	};


/***/ }),
/* 18 */
/***/ (function(module, exports, __webpack_require__) {

	var global = __webpack_require__(4);
	var hide = __webpack_require__(10);
	var has = __webpack_require__(5);
	var SRC = __webpack_require__(19)('src');
	var TO_STRING = 'toString';
	var $toString = Function[TO_STRING];
	var TPL = ('' + $toString).split(TO_STRING);

	__webpack_require__(9).inspectSource = function (it) {
	  return $toString.call(it);
	};

	(module.exports = function (O, key, val, safe) {
	  var isFunction = typeof val == 'function';
	  if (isFunction) has(val, 'name') || hide(val, 'name', key);
	  if (O[key] === val) return;
	  if (isFunction) has(val, SRC) || hide(val, SRC, O[key] ? '' + O[key] : TPL.join(String(key)));
	  if (O === global) {
	    O[key] = val;
	  } else if (!safe) {
	    delete O[key];
	    hide(O, key, val);
	  } else if (O[key]) {
	    O[key] = val;
	  } else {
	    hide(O, key, val);
	  }
	// add fake Function#toString for correct work wrapped methods / constructors with methods like LoDash isNative
	})(Function.prototype, TO_STRING, function toString() {
	  return typeof this == 'function' && this[SRC] || $toString.call(this);
	});


/***/ }),
/* 19 */
/***/ (function(module, exports) {

	var id = 0;
	var px = Math.random();
	module.exports = function (key) {
	  return 'Symbol('.concat(key === undefined ? '' : key, ')_', (++id + px).toString(36));
	};


/***/ }),
/* 20 */
/***/ (function(module, exports, __webpack_require__) {

	// optional / simple context binding
	var aFunction = __webpack_require__(21);
	module.exports = function (fn, that, length) {
	  aFunction(fn);
	  if (that === undefined) return fn;
	  switch (length) {
	    case 1: return function (a) {
	      return fn.call(that, a);
	    };
	    case 2: return function (a, b) {
	      return fn.call(that, a, b);
	    };
	    case 3: return function (a, b, c) {
	      return fn.call(that, a, b, c);
	    };
	  }
	  return function (/* ...args */) {
	    return fn.apply(that, arguments);
	  };
	};


/***/ }),
/* 21 */
/***/ (function(module, exports) {

	module.exports = function (it) {
	  if (typeof it != 'function') throw TypeError(it + ' is not a function!');
	  return it;
	};


/***/ }),
/* 22 */
/***/ (function(module, exports, __webpack_require__) {

	var META = __webpack_require__(19)('meta');
	var isObject = __webpack_require__(13);
	var has = __webpack_require__(5);
	var setDesc = __webpack_require__(11).f;
	var id = 0;
	var isExtensible = Object.isExtensible || function () {
	  return true;
	};
	var FREEZE = !__webpack_require__(7)(function () {
	  return isExtensible(Object.preventExtensions({}));
	});
	var setMeta = function (it) {
	  setDesc(it, META, { value: {
	    i: 'O' + ++id, // object ID
	    w: {}          // weak collections IDs
	  } });
	};
	var fastKey = function (it, create) {
	  // return primitive with prefix
	  if (!isObject(it)) return typeof it == 'symbol' ? it : (typeof it == 'string' ? 'S' : 'P') + it;
	  if (!has(it, META)) {
	    // can't set metadata to uncaught frozen object
	    if (!isExtensible(it)) return 'F';
	    // not necessary to add metadata
	    if (!create) return 'E';
	    // add missing metadata
	    setMeta(it);
	  // return object ID
	  } return it[META].i;
	};
	var getWeak = function (it, create) {
	  if (!has(it, META)) {
	    // can't set metadata to uncaught frozen object
	    if (!isExtensible(it)) return true;
	    // not necessary to add metadata
	    if (!create) return false;
	    // add missing metadata
	    setMeta(it);
	  // return hash weak collections IDs
	  } return it[META].w;
	};
	// add metadata on freeze-family methods calling
	var onFreeze = function (it) {
	  if (FREEZE && meta.NEED && isExtensible(it) && !has(it, META)) setMeta(it);
	  return it;
	};
	var meta = module.exports = {
	  KEY: META,
	  NEED: false,
	  fastKey: fastKey,
	  getWeak: getWeak,
	  onFreeze: onFreeze
	};


/***/ }),
/* 23 */
/***/ (function(module, exports, __webpack_require__) {

	var core = __webpack_require__(9);
	var global = __webpack_require__(4);
	var SHARED = '__core-js_shared__';
	var store = global[SHARED] || (global[SHARED] = {});

	(module.exports = function (key, value) {
	  return store[key] || (store[key] = value !== undefined ? value : {});
	})('versions', []).push({
	  version: core.version,
	  mode: __webpack_require__(24) ? 'pure' : 'global',
	  copyright: '© 2018 Denis Pushkarev (zloirock.ru)'
	});


/***/ }),
/* 24 */
/***/ (function(module, exports) {

	module.exports = false;


/***/ }),
/* 25 */
/***/ (function(module, exports, __webpack_require__) {

	var def = __webpack_require__(11).f;
	var has = __webpack_require__(5);
	var TAG = __webpack_require__(26)('toStringTag');

	module.exports = function (it, tag, stat) {
	  if (it && !has(it = stat ? it : it.prototype, TAG)) def(it, TAG, { configurable: true, value: tag });
	};


/***/ }),
/* 26 */
/***/ (function(module, exports, __webpack_require__) {

	var store = __webpack_require__(23)('wks');
	var uid = __webpack_require__(19);
	var Symbol = __webpack_require__(4).Symbol;
	var USE_SYMBOL = typeof Symbol == 'function';

	var $exports = module.exports = function (name) {
	  return store[name] || (store[name] =
	    USE_SYMBOL && Symbol[name] || (USE_SYMBOL ? Symbol : uid)('Symbol.' + name));
	};

	$exports.store = store;


/***/ }),
/* 27 */
/***/ (function(module, exports, __webpack_require__) {

	exports.f = __webpack_require__(26);


/***/ }),
/* 28 */
/***/ (function(module, exports, __webpack_require__) {

	var global = __webpack_require__(4);
	var core = __webpack_require__(9);
	var LIBRARY = __webpack_require__(24);
	var wksExt = __webpack_require__(27);
	var defineProperty = __webpack_require__(11).f;
	module.exports = function (name) {
	  var $Symbol = core.Symbol || (core.Symbol = LIBRARY ? {} : global.Symbol || {});
	  if (name.charAt(0) != '_' && !(name in $Symbol)) defineProperty($Symbol, name, { value: wksExt.f(name) });
	};


/***/ }),
/* 29 */
/***/ (function(module, exports, __webpack_require__) {

	// all enumerable object keys, includes symbols
	var getKeys = __webpack_require__(30);
	var gOPS = __webpack_require__(42);
	var pIE = __webpack_require__(43);
	module.exports = function (it) {
	  var result = getKeys(it);
	  var getSymbols = gOPS.f;
	  if (getSymbols) {
	    var symbols = getSymbols(it);
	    var isEnum = pIE.f;
	    var i = 0;
	    var key;
	    while (symbols.length > i) if (isEnum.call(it, key = symbols[i++])) result.push(key);
	  } return result;
	};


/***/ }),
/* 30 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.2.14 / 15.2.3.14 Object.keys(O)
	var $keys = __webpack_require__(31);
	var enumBugKeys = __webpack_require__(41);

	module.exports = Object.keys || function keys(O) {
	  return $keys(O, enumBugKeys);
	};


/***/ }),
/* 31 */
/***/ (function(module, exports, __webpack_require__) {

	var has = __webpack_require__(5);
	var toIObject = __webpack_require__(32);
	var arrayIndexOf = __webpack_require__(36)(false);
	var IE_PROTO = __webpack_require__(40)('IE_PROTO');

	module.exports = function (object, names) {
	  var O = toIObject(object);
	  var i = 0;
	  var result = [];
	  var key;
	  for (key in O) if (key != IE_PROTO) has(O, key) && result.push(key);
	  // Don't enum bug & hidden keys
	  while (names.length > i) if (has(O, key = names[i++])) {
	    ~arrayIndexOf(result, key) || result.push(key);
	  }
	  return result;
	};


/***/ }),
/* 32 */
/***/ (function(module, exports, __webpack_require__) {

	// to indexed object, toObject with fallback for non-array-like ES3 strings
	var IObject = __webpack_require__(33);
	var defined = __webpack_require__(35);
	module.exports = function (it) {
	  return IObject(defined(it));
	};


/***/ }),
/* 33 */
/***/ (function(module, exports, __webpack_require__) {

	// fallback for non-array-like ES3 and non-enumerable old V8 strings
	var cof = __webpack_require__(34);
	// eslint-disable-next-line no-prototype-builtins
	module.exports = Object('z').propertyIsEnumerable(0) ? Object : function (it) {
	  return cof(it) == 'String' ? it.split('') : Object(it);
	};


/***/ }),
/* 34 */
/***/ (function(module, exports) {

	var toString = {}.toString;

	module.exports = function (it) {
	  return toString.call(it).slice(8, -1);
	};


/***/ }),
/* 35 */
/***/ (function(module, exports) {

	// 7.2.1 RequireObjectCoercible(argument)
	module.exports = function (it) {
	  if (it == undefined) throw TypeError("Can't call method on  " + it);
	  return it;
	};


/***/ }),
/* 36 */
/***/ (function(module, exports, __webpack_require__) {

	// false -> Array#indexOf
	// true  -> Array#includes
	var toIObject = __webpack_require__(32);
	var toLength = __webpack_require__(37);
	var toAbsoluteIndex = __webpack_require__(39);
	module.exports = function (IS_INCLUDES) {
	  return function ($this, el, fromIndex) {
	    var O = toIObject($this);
	    var length = toLength(O.length);
	    var index = toAbsoluteIndex(fromIndex, length);
	    var value;
	    // Array#includes uses SameValueZero equality algorithm
	    // eslint-disable-next-line no-self-compare
	    if (IS_INCLUDES && el != el) while (length > index) {
	      value = O[index++];
	      // eslint-disable-next-line no-self-compare
	      if (value != value) return true;
	    // Array#indexOf ignores holes, Array#includes - not
	    } else for (;length > index; index++) if (IS_INCLUDES || index in O) {
	      if (O[index] === el) return IS_INCLUDES || index || 0;
	    } return !IS_INCLUDES && -1;
	  };
	};


/***/ }),
/* 37 */
/***/ (function(module, exports, __webpack_require__) {

	// 7.1.15 ToLength
	var toInteger = __webpack_require__(38);
	var min = Math.min;
	module.exports = function (it) {
	  return it > 0 ? min(toInteger(it), 0x1fffffffffffff) : 0; // pow(2, 53) - 1 == 9007199254740991
	};


/***/ }),
/* 38 */
/***/ (function(module, exports) {

	// 7.1.4 ToInteger
	var ceil = Math.ceil;
	var floor = Math.floor;
	module.exports = function (it) {
	  return isNaN(it = +it) ? 0 : (it > 0 ? floor : ceil)(it);
	};


/***/ }),
/* 39 */
/***/ (function(module, exports, __webpack_require__) {

	var toInteger = __webpack_require__(38);
	var max = Math.max;
	var min = Math.min;
	module.exports = function (index, length) {
	  index = toInteger(index);
	  return index < 0 ? max(index + length, 0) : min(index, length);
	};


/***/ }),
/* 40 */
/***/ (function(module, exports, __webpack_require__) {

	var shared = __webpack_require__(23)('keys');
	var uid = __webpack_require__(19);
	module.exports = function (key) {
	  return shared[key] || (shared[key] = uid(key));
	};


/***/ }),
/* 41 */
/***/ (function(module, exports) {

	// IE 8- don't enum bug keys
	module.exports = (
	  'constructor,hasOwnProperty,isPrototypeOf,propertyIsEnumerable,toLocaleString,toString,valueOf'
	).split(',');


/***/ }),
/* 42 */
/***/ (function(module, exports) {

	exports.f = Object.getOwnPropertySymbols;


/***/ }),
/* 43 */
/***/ (function(module, exports) {

	exports.f = {}.propertyIsEnumerable;


/***/ }),
/* 44 */
/***/ (function(module, exports, __webpack_require__) {

	// 7.2.2 IsArray(argument)
	var cof = __webpack_require__(34);
	module.exports = Array.isArray || function isArray(arg) {
	  return cof(arg) == 'Array';
	};


/***/ }),
/* 45 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.2.2 / 15.2.3.5 Object.create(O [, Properties])
	var anObject = __webpack_require__(12);
	var dPs = __webpack_require__(46);
	var enumBugKeys = __webpack_require__(41);
	var IE_PROTO = __webpack_require__(40)('IE_PROTO');
	var Empty = function () { /* empty */ };
	var PROTOTYPE = 'prototype';

	// Create object with fake `null` prototype: use iframe Object with cleared prototype
	var createDict = function () {
	  // Thrash, waste and sodomy: IE GC bug
	  var iframe = __webpack_require__(15)('iframe');
	  var i = enumBugKeys.length;
	  var lt = '<';
	  var gt = '>';
	  var iframeDocument;
	  iframe.style.display = 'none';
	  __webpack_require__(47).appendChild(iframe);
	  iframe.src = 'javascript:'; // eslint-disable-line no-script-url
	  // createDict = iframe.contentWindow.Object;
	  // html.removeChild(iframe);
	  iframeDocument = iframe.contentWindow.document;
	  iframeDocument.open();
	  iframeDocument.write(lt + 'script' + gt + 'document.F=Object' + lt + '/script' + gt);
	  iframeDocument.close();
	  createDict = iframeDocument.F;
	  while (i--) delete createDict[PROTOTYPE][enumBugKeys[i]];
	  return createDict();
	};

	module.exports = Object.create || function create(O, Properties) {
	  var result;
	  if (O !== null) {
	    Empty[PROTOTYPE] = anObject(O);
	    result = new Empty();
	    Empty[PROTOTYPE] = null;
	    // add "__proto__" for Object.getPrototypeOf polyfill
	    result[IE_PROTO] = O;
	  } else result = createDict();
	  return Properties === undefined ? result : dPs(result, Properties);
	};


/***/ }),
/* 46 */
/***/ (function(module, exports, __webpack_require__) {

	var dP = __webpack_require__(11);
	var anObject = __webpack_require__(12);
	var getKeys = __webpack_require__(30);

	module.exports = __webpack_require__(6) ? Object.defineProperties : function defineProperties(O, Properties) {
	  anObject(O);
	  var keys = getKeys(Properties);
	  var length = keys.length;
	  var i = 0;
	  var P;
	  while (length > i) dP.f(O, P = keys[i++], Properties[P]);
	  return O;
	};


/***/ }),
/* 47 */
/***/ (function(module, exports, __webpack_require__) {

	var document = __webpack_require__(4).document;
	module.exports = document && document.documentElement;


/***/ }),
/* 48 */
/***/ (function(module, exports, __webpack_require__) {

	// fallback for IE11 buggy Object.getOwnPropertyNames with iframe and window
	var toIObject = __webpack_require__(32);
	var gOPN = __webpack_require__(49).f;
	var toString = {}.toString;

	var windowNames = typeof window == 'object' && window && Object.getOwnPropertyNames
	  ? Object.getOwnPropertyNames(window) : [];

	var getWindowNames = function (it) {
	  try {
	    return gOPN(it);
	  } catch (e) {
	    return windowNames.slice();
	  }
	};

	module.exports.f = function getOwnPropertyNames(it) {
	  return windowNames && toString.call(it) == '[object Window]' ? getWindowNames(it) : gOPN(toIObject(it));
	};


/***/ }),
/* 49 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.2.7 / 15.2.3.4 Object.getOwnPropertyNames(O)
	var $keys = __webpack_require__(31);
	var hiddenKeys = __webpack_require__(41).concat('length', 'prototype');

	exports.f = Object.getOwnPropertyNames || function getOwnPropertyNames(O) {
	  return $keys(O, hiddenKeys);
	};


/***/ }),
/* 50 */
/***/ (function(module, exports, __webpack_require__) {

	var pIE = __webpack_require__(43);
	var createDesc = __webpack_require__(17);
	var toIObject = __webpack_require__(32);
	var toPrimitive = __webpack_require__(16);
	var has = __webpack_require__(5);
	var IE8_DOM_DEFINE = __webpack_require__(14);
	var gOPD = Object.getOwnPropertyDescriptor;

	exports.f = __webpack_require__(6) ? gOPD : function getOwnPropertyDescriptor(O, P) {
	  O = toIObject(O);
	  P = toPrimitive(P, true);
	  if (IE8_DOM_DEFINE) try {
	    return gOPD(O, P);
	  } catch (e) { /* empty */ }
	  if (has(O, P)) return createDesc(!pIE.f.call(O, P), O[P]);
	};


/***/ }),
/* 51 */
/***/ (function(module, exports, __webpack_require__) {

	var $export = __webpack_require__(8);
	// 19.1.2.2 / 15.2.3.5 Object.create(O [, Properties])
	$export($export.S, 'Object', { create: __webpack_require__(45) });


/***/ }),
/* 52 */
/***/ (function(module, exports, __webpack_require__) {

	var $export = __webpack_require__(8);
	// 19.1.2.4 / 15.2.3.6 Object.defineProperty(O, P, Attributes)
	$export($export.S + $export.F * !__webpack_require__(6), 'Object', { defineProperty: __webpack_require__(11).f });


/***/ }),
/* 53 */
/***/ (function(module, exports, __webpack_require__) {

	var $export = __webpack_require__(8);
	// 19.1.2.3 / 15.2.3.7 Object.defineProperties(O, Properties)
	$export($export.S + $export.F * !__webpack_require__(6), 'Object', { defineProperties: __webpack_require__(46) });


/***/ }),
/* 54 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.2.6 Object.getOwnPropertyDescriptor(O, P)
	var toIObject = __webpack_require__(32);
	var $getOwnPropertyDescriptor = __webpack_require__(50).f;

	__webpack_require__(55)('getOwnPropertyDescriptor', function () {
	  return function getOwnPropertyDescriptor(it, key) {
	    return $getOwnPropertyDescriptor(toIObject(it), key);
	  };
	});


/***/ }),
/* 55 */
/***/ (function(module, exports, __webpack_require__) {

	// most Object methods by ES6 should accept primitives
	var $export = __webpack_require__(8);
	var core = __webpack_require__(9);
	var fails = __webpack_require__(7);
	module.exports = function (KEY, exec) {
	  var fn = (core.Object || {})[KEY] || Object[KEY];
	  var exp = {};
	  exp[KEY] = exec(fn);
	  $export($export.S + $export.F * fails(function () { fn(1); }), 'Object', exp);
	};


/***/ }),
/* 56 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.2.9 Object.getPrototypeOf(O)
	var toObject = __webpack_require__(57);
	var $getPrototypeOf = __webpack_require__(58);

	__webpack_require__(55)('getPrototypeOf', function () {
	  return function getPrototypeOf(it) {
	    return $getPrototypeOf(toObject(it));
	  };
	});


/***/ }),
/* 57 */
/***/ (function(module, exports, __webpack_require__) {

	// 7.1.13 ToObject(argument)
	var defined = __webpack_require__(35);
	module.exports = function (it) {
	  return Object(defined(it));
	};


/***/ }),
/* 58 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.2.9 / 15.2.3.2 Object.getPrototypeOf(O)
	var has = __webpack_require__(5);
	var toObject = __webpack_require__(57);
	var IE_PROTO = __webpack_require__(40)('IE_PROTO');
	var ObjectProto = Object.prototype;

	module.exports = Object.getPrototypeOf || function (O) {
	  O = toObject(O);
	  if (has(O, IE_PROTO)) return O[IE_PROTO];
	  if (typeof O.constructor == 'function' && O instanceof O.constructor) {
	    return O.constructor.prototype;
	  } return O instanceof Object ? ObjectProto : null;
	};


/***/ }),
/* 59 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.2.14 Object.keys(O)
	var toObject = __webpack_require__(57);
	var $keys = __webpack_require__(30);

	__webpack_require__(55)('keys', function () {
	  return function keys(it) {
	    return $keys(toObject(it));
	  };
	});


/***/ }),
/* 60 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.2.7 Object.getOwnPropertyNames(O)
	__webpack_require__(55)('getOwnPropertyNames', function () {
	  return __webpack_require__(48).f;
	});


/***/ }),
/* 61 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.2.5 Object.freeze(O)
	var isObject = __webpack_require__(13);
	var meta = __webpack_require__(22).onFreeze;

	__webpack_require__(55)('freeze', function ($freeze) {
	  return function freeze(it) {
	    return $freeze && isObject(it) ? $freeze(meta(it)) : it;
	  };
	});


/***/ }),
/* 62 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.2.17 Object.seal(O)
	var isObject = __webpack_require__(13);
	var meta = __webpack_require__(22).onFreeze;

	__webpack_require__(55)('seal', function ($seal) {
	  return function seal(it) {
	    return $seal && isObject(it) ? $seal(meta(it)) : it;
	  };
	});


/***/ }),
/* 63 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.2.15 Object.preventExtensions(O)
	var isObject = __webpack_require__(13);
	var meta = __webpack_require__(22).onFreeze;

	__webpack_require__(55)('preventExtensions', function ($preventExtensions) {
	  return function preventExtensions(it) {
	    return $preventExtensions && isObject(it) ? $preventExtensions(meta(it)) : it;
	  };
	});


/***/ }),
/* 64 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.2.12 Object.isFrozen(O)
	var isObject = __webpack_require__(13);

	__webpack_require__(55)('isFrozen', function ($isFrozen) {
	  return function isFrozen(it) {
	    return isObject(it) ? $isFrozen ? $isFrozen(it) : false : true;
	  };
	});


/***/ }),
/* 65 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.2.13 Object.isSealed(O)
	var isObject = __webpack_require__(13);

	__webpack_require__(55)('isSealed', function ($isSealed) {
	  return function isSealed(it) {
	    return isObject(it) ? $isSealed ? $isSealed(it) : false : true;
	  };
	});


/***/ }),
/* 66 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.2.11 Object.isExtensible(O)
	var isObject = __webpack_require__(13);

	__webpack_require__(55)('isExtensible', function ($isExtensible) {
	  return function isExtensible(it) {
	    return isObject(it) ? $isExtensible ? $isExtensible(it) : true : false;
	  };
	});


/***/ }),
/* 67 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.3.1 Object.assign(target, source)
	var $export = __webpack_require__(8);

	$export($export.S + $export.F, 'Object', { assign: __webpack_require__(68) });


/***/ }),
/* 68 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// 19.1.2.1 Object.assign(target, source, ...)
	var getKeys = __webpack_require__(30);
	var gOPS = __webpack_require__(42);
	var pIE = __webpack_require__(43);
	var toObject = __webpack_require__(57);
	var IObject = __webpack_require__(33);
	var $assign = Object.assign;

	// should work with symbols and should have deterministic property order (V8 bug)
	module.exports = !$assign || __webpack_require__(7)(function () {
	  var A = {};
	  var B = {};
	  // eslint-disable-next-line no-undef
	  var S = Symbol();
	  var K = 'abcdefghijklmnopqrst';
	  A[S] = 7;
	  K.split('').forEach(function (k) { B[k] = k; });
	  return $assign({}, A)[S] != 7 || Object.keys($assign({}, B)).join('') != K;
	}) ? function assign(target, source) { // eslint-disable-line no-unused-vars
	  var T = toObject(target);
	  var aLen = arguments.length;
	  var index = 1;
	  var getSymbols = gOPS.f;
	  var isEnum = pIE.f;
	  while (aLen > index) {
	    var S = IObject(arguments[index++]);
	    var keys = getSymbols ? getKeys(S).concat(getSymbols(S)) : getKeys(S);
	    var length = keys.length;
	    var j = 0;
	    var key;
	    while (length > j) if (isEnum.call(S, key = keys[j++])) T[key] = S[key];
	  } return T;
	} : $assign;


/***/ }),
/* 69 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.3.10 Object.is(value1, value2)
	var $export = __webpack_require__(8);
	$export($export.S, 'Object', { is: __webpack_require__(70) });


/***/ }),
/* 70 */
/***/ (function(module, exports) {

	// 7.2.9 SameValue(x, y)
	module.exports = Object.is || function is(x, y) {
	  // eslint-disable-next-line no-self-compare
	  return x === y ? x !== 0 || 1 / x === 1 / y : x != x && y != y;
	};


/***/ }),
/* 71 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.1.3.19 Object.setPrototypeOf(O, proto)
	var $export = __webpack_require__(8);
	$export($export.S, 'Object', { setPrototypeOf: __webpack_require__(72).set });


/***/ }),
/* 72 */
/***/ (function(module, exports, __webpack_require__) {

	// Works with __proto__ only. Old v8 can't work with null proto objects.
	/* eslint-disable no-proto */
	var isObject = __webpack_require__(13);
	var anObject = __webpack_require__(12);
	var check = function (O, proto) {
	  anObject(O);
	  if (!isObject(proto) && proto !== null) throw TypeError(proto + ": can't set as prototype!");
	};
	module.exports = {
	  set: Object.setPrototypeOf || ('__proto__' in {} ? // eslint-disable-line
	    function (test, buggy, set) {
	      try {
	        set = __webpack_require__(20)(Function.call, __webpack_require__(50).f(Object.prototype, '__proto__').set, 2);
	        set(test, []);
	        buggy = !(test instanceof Array);
	      } catch (e) { buggy = true; }
	      return function setPrototypeOf(O, proto) {
	        check(O, proto);
	        if (buggy) O.__proto__ = proto;
	        else set(O, proto);
	        return O;
	      };
	    }({}, false) : undefined),
	  check: check
	};


/***/ }),
/* 73 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// 19.1.3.6 Object.prototype.toString()
	var classof = __webpack_require__(74);
	var test = {};
	test[__webpack_require__(26)('toStringTag')] = 'z';
	if (test + '' != '[object z]') {
	  __webpack_require__(18)(Object.prototype, 'toString', function toString() {
	    return '[object ' + classof(this) + ']';
	  }, true);
	}


/***/ }),
/* 74 */
/***/ (function(module, exports, __webpack_require__) {

	// getting tag from 19.1.3.6 Object.prototype.toString()
	var cof = __webpack_require__(34);
	var TAG = __webpack_require__(26)('toStringTag');
	// ES3 wrong here
	var ARG = cof(function () { return arguments; }()) == 'Arguments';

	// fallback for IE11 Script Access Denied error
	var tryGet = function (it, key) {
	  try {
	    return it[key];
	  } catch (e) { /* empty */ }
	};

	module.exports = function (it) {
	  var O, T, B;
	  return it === undefined ? 'Undefined' : it === null ? 'Null'
	    // @@toStringTag case
	    : typeof (T = tryGet(O = Object(it), TAG)) == 'string' ? T
	    // builtinTag case
	    : ARG ? cof(O)
	    // ES3 arguments fallback
	    : (B = cof(O)) == 'Object' && typeof O.callee == 'function' ? 'Arguments' : B;
	};


/***/ }),
/* 75 */
/***/ (function(module, exports, __webpack_require__) {

	// 19.2.3.2 / 15.3.4.5 Function.prototype.bind(thisArg, args...)
	var $export = __webpack_require__(8);

	$export($export.P, 'Function', { bind: __webpack_require__(76) });


/***/ }),
/* 76 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var aFunction = __webpack_require__(21);
	var isObject = __webpack_require__(13);
	var invoke = __webpack_require__(77);
	var arraySlice = [].slice;
	var factories = {};

	var construct = function (F, len, args) {
	  if (!(len in factories)) {
	    for (var n = [], i = 0; i < len; i++) n[i] = 'a[' + i + ']';
	    // eslint-disable-next-line no-new-func
	    factories[len] = Function('F,a', 'return new F(' + n.join(',') + ')');
	  } return factories[len](F, args);
	};

	module.exports = Function.bind || function bind(that /* , ...args */) {
	  var fn = aFunction(this);
	  var partArgs = arraySlice.call(arguments, 1);
	  var bound = function (/* args... */) {
	    var args = partArgs.concat(arraySlice.call(arguments));
	    return this instanceof bound ? construct(fn, args.length, args) : invoke(fn, args, that);
	  };
	  if (isObject(fn.prototype)) bound.prototype = fn.prototype;
	  return bound;
	};


/***/ }),
/* 77 */
/***/ (function(module, exports) {

	// fast apply, http://jsperf.lnkit.com/fast-apply/5
	module.exports = function (fn, args, that) {
	  var un = that === undefined;
	  switch (args.length) {
	    case 0: return un ? fn()
	                      : fn.call(that);
	    case 1: return un ? fn(args[0])
	                      : fn.call(that, args[0]);
	    case 2: return un ? fn(args[0], args[1])
	                      : fn.call(that, args[0], args[1]);
	    case 3: return un ? fn(args[0], args[1], args[2])
	                      : fn.call(that, args[0], args[1], args[2]);
	    case 4: return un ? fn(args[0], args[1], args[2], args[3])
	                      : fn.call(that, args[0], args[1], args[2], args[3]);
	  } return fn.apply(that, args);
	};


/***/ }),
/* 78 */
/***/ (function(module, exports, __webpack_require__) {

	var dP = __webpack_require__(11).f;
	var FProto = Function.prototype;
	var nameRE = /^\s*function ([^ (]*)/;
	var NAME = 'name';

	// 19.2.4.2 name
	NAME in FProto || __webpack_require__(6) && dP(FProto, NAME, {
	  configurable: true,
	  get: function () {
	    try {
	      return ('' + this).match(nameRE)[1];
	    } catch (e) {
	      return '';
	    }
	  }
	});


/***/ }),
/* 79 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var isObject = __webpack_require__(13);
	var getPrototypeOf = __webpack_require__(58);
	var HAS_INSTANCE = __webpack_require__(26)('hasInstance');
	var FunctionProto = Function.prototype;
	// 19.2.3.6 Function.prototype[@@hasInstance](V)
	if (!(HAS_INSTANCE in FunctionProto)) __webpack_require__(11).f(FunctionProto, HAS_INSTANCE, { value: function (O) {
	  if (typeof this != 'function' || !isObject(O)) return false;
	  if (!isObject(this.prototype)) return O instanceof this;
	  // for environment w/o native `@@hasInstance` logic enough `instanceof`, but add this:
	  while (O = getPrototypeOf(O)) if (this.prototype === O) return true;
	  return false;
	} });


/***/ }),
/* 80 */
/***/ (function(module, exports, __webpack_require__) {

	var $export = __webpack_require__(8);
	var $parseInt = __webpack_require__(81);
	// 18.2.5 parseInt(string, radix)
	$export($export.G + $export.F * (parseInt != $parseInt), { parseInt: $parseInt });


/***/ }),
/* 81 */
/***/ (function(module, exports, __webpack_require__) {

	var $parseInt = __webpack_require__(4).parseInt;
	var $trim = __webpack_require__(82).trim;
	var ws = __webpack_require__(83);
	var hex = /^[-+]?0[xX]/;

	module.exports = $parseInt(ws + '08') !== 8 || $parseInt(ws + '0x16') !== 22 ? function parseInt(str, radix) {
	  var string = $trim(String(str), 3);
	  return $parseInt(string, (radix >>> 0) || (hex.test(string) ? 16 : 10));
	} : $parseInt;


/***/ }),
/* 82 */
/***/ (function(module, exports, __webpack_require__) {

	var $export = __webpack_require__(8);
	var defined = __webpack_require__(35);
	var fails = __webpack_require__(7);
	var spaces = __webpack_require__(83);
	var space = '[' + spaces + ']';
	var non = '\u200b\u0085';
	var ltrim = RegExp('^' + space + space + '*');
	var rtrim = RegExp(space + space + '*$');

	var exporter = function (KEY, exec, ALIAS) {
	  var exp = {};
	  var FORCE = fails(function () {
	    return !!spaces[KEY]() || non[KEY]() != non;
	  });
	  var fn = exp[KEY] = FORCE ? exec(trim) : spaces[KEY];
	  if (ALIAS) exp[ALIAS] = fn;
	  $export($export.P + $export.F * FORCE, 'String', exp);
	};

	// 1 -> String#trimLeft
	// 2 -> String#trimRight
	// 3 -> String#trim
	var trim = exporter.trim = function (string, TYPE) {
	  string = String(defined(string));
	  if (TYPE & 1) string = string.replace(ltrim, '');
	  if (TYPE & 2) string = string.replace(rtrim, '');
	  return string;
	};

	module.exports = exporter;


/***/ }),
/* 83 */
/***/ (function(module, exports) {

	module.exports = '\x09\x0A\x0B\x0C\x0D\x20\xA0\u1680\u180E\u2000\u2001\u2002\u2003' +
	  '\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u202F\u205F\u3000\u2028\u2029\uFEFF';


/***/ }),
/* 84 */
/***/ (function(module, exports, __webpack_require__) {

	var $export = __webpack_require__(8);
	var $parseFloat = __webpack_require__(85);
	// 18.2.4 parseFloat(string)
	$export($export.G + $export.F * (parseFloat != $parseFloat), { parseFloat: $parseFloat });


/***/ }),
/* 85 */
/***/ (function(module, exports, __webpack_require__) {

	var $parseFloat = __webpack_require__(4).parseFloat;
	var $trim = __webpack_require__(82).trim;

	module.exports = 1 / $parseFloat(__webpack_require__(83) + '-0') !== -Infinity ? function parseFloat(str) {
	  var string = $trim(String(str), 3);
	  var result = $parseFloat(string);
	  return result === 0 && string.charAt(0) == '-' ? -0 : result;
	} : $parseFloat;


/***/ }),
/* 86 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var global = __webpack_require__(4);
	var has = __webpack_require__(5);
	var cof = __webpack_require__(34);
	var inheritIfRequired = __webpack_require__(87);
	var toPrimitive = __webpack_require__(16);
	var fails = __webpack_require__(7);
	var gOPN = __webpack_require__(49).f;
	var gOPD = __webpack_require__(50).f;
	var dP = __webpack_require__(11).f;
	var $trim = __webpack_require__(82).trim;
	var NUMBER = 'Number';
	var $Number = global[NUMBER];
	var Base = $Number;
	var proto = $Number.prototype;
	// Opera ~12 has broken Object#toString
	var BROKEN_COF = cof(__webpack_require__(45)(proto)) == NUMBER;
	var TRIM = 'trim' in String.prototype;

	// 7.1.3 ToNumber(argument)
	var toNumber = function (argument) {
	  var it = toPrimitive(argument, false);
	  if (typeof it == 'string' && it.length > 2) {
	    it = TRIM ? it.trim() : $trim(it, 3);
	    var first = it.charCodeAt(0);
	    var third, radix, maxCode;
	    if (first === 43 || first === 45) {
	      third = it.charCodeAt(2);
	      if (third === 88 || third === 120) return NaN; // Number('+0x1') should be NaN, old V8 fix
	    } else if (first === 48) {
	      switch (it.charCodeAt(1)) {
	        case 66: case 98: radix = 2; maxCode = 49; break; // fast equal /^0b[01]+$/i
	        case 79: case 111: radix = 8; maxCode = 55; break; // fast equal /^0o[0-7]+$/i
	        default: return +it;
	      }
	      for (var digits = it.slice(2), i = 0, l = digits.length, code; i < l; i++) {
	        code = digits.charCodeAt(i);
	        // parseInt parses a string to a first unavailable symbol
	        // but ToNumber should return NaN if a string contains unavailable symbols
	        if (code < 48 || code > maxCode) return NaN;
	      } return parseInt(digits, radix);
	    }
	  } return +it;
	};

	if (!$Number(' 0o1') || !$Number('0b1') || $Number('+0x1')) {
	  $Number = function Number(value) {
	    var it = arguments.length < 1 ? 0 : value;
	    var that = this;
	    return that instanceof $Number
	      // check on 1..constructor(foo) case
	      && (BROKEN_COF ? fails(function () { proto.valueOf.call(that); }) : cof(that) != NUMBER)
	        ? inheritIfRequired(new Base(toNumber(it)), that, $Number) : toNumber(it);
	  };
	  for (var keys = __webpack_require__(6) ? gOPN(Base) : (
	    // ES3:
	    'MAX_VALUE,MIN_VALUE,NaN,NEGATIVE_INFINITY,POSITIVE_INFINITY,' +
	    // ES6 (in case, if modules with ES6 Number statics required before):
	    'EPSILON,isFinite,isInteger,isNaN,isSafeInteger,MAX_SAFE_INTEGER,' +
	    'MIN_SAFE_INTEGER,parseFloat,parseInt,isInteger'
	  ).split(','), j = 0, key; keys.length > j; j++) {
	    if (has(Base, key = keys[j]) && !has($Number, key)) {
	      dP($Number, key, gOPD(Base, key));
	    }
	  }
	  $Number.prototype = proto;
	  proto.constructor = $Number;
	  __webpack_require__(18)(global, NUMBER, $Number);
	}


/***/ }),
/* 87 */
/***/ (function(module, exports, __webpack_require__) {

	var isObject = __webpack_require__(13);
	var setPrototypeOf = __webpack_require__(72).set;
	module.exports = function (that, target, C) {
	  var S = target.constructor;
	  var P;
	  if (S !== C && typeof S == 'function' && (P = S.prototype) !== C.prototype && isObject(P) && setPrototypeOf) {
	    setPrototypeOf(that, P);
	  } return that;
	};


/***/ }),
/* 88 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var toInteger = __webpack_require__(38);
	var aNumberValue = __webpack_require__(89);
	var repeat = __webpack_require__(90);
	var $toFixed = 1.0.toFixed;
	var floor = Math.floor;
	var data = [0, 0, 0, 0, 0, 0];
	var ERROR = 'Number.toFixed: incorrect invocation!';
	var ZERO = '0';

	var multiply = function (n, c) {
	  var i = -1;
	  var c2 = c;
	  while (++i < 6) {
	    c2 += n * data[i];
	    data[i] = c2 % 1e7;
	    c2 = floor(c2 / 1e7);
	  }
	};
	var divide = function (n) {
	  var i = 6;
	  var c = 0;
	  while (--i >= 0) {
	    c += data[i];
	    data[i] = floor(c / n);
	    c = (c % n) * 1e7;
	  }
	};
	var numToString = function () {
	  var i = 6;
	  var s = '';
	  while (--i >= 0) {
	    if (s !== '' || i === 0 || data[i] !== 0) {
	      var t = String(data[i]);
	      s = s === '' ? t : s + repeat.call(ZERO, 7 - t.length) + t;
	    }
	  } return s;
	};
	var pow = function (x, n, acc) {
	  return n === 0 ? acc : n % 2 === 1 ? pow(x, n - 1, acc * x) : pow(x * x, n / 2, acc);
	};
	var log = function (x) {
	  var n = 0;
	  var x2 = x;
	  while (x2 >= 4096) {
	    n += 12;
	    x2 /= 4096;
	  }
	  while (x2 >= 2) {
	    n += 1;
	    x2 /= 2;
	  } return n;
	};

	$export($export.P + $export.F * (!!$toFixed && (
	  0.00008.toFixed(3) !== '0.000' ||
	  0.9.toFixed(0) !== '1' ||
	  1.255.toFixed(2) !== '1.25' ||
	  1000000000000000128.0.toFixed(0) !== '1000000000000000128'
	) || !__webpack_require__(7)(function () {
	  // V8 ~ Android 4.3-
	  $toFixed.call({});
	})), 'Number', {
	  toFixed: function toFixed(fractionDigits) {
	    var x = aNumberValue(this, ERROR);
	    var f = toInteger(fractionDigits);
	    var s = '';
	    var m = ZERO;
	    var e, z, j, k;
	    if (f < 0 || f > 20) throw RangeError(ERROR);
	    // eslint-disable-next-line no-self-compare
	    if (x != x) return 'NaN';
	    if (x <= -1e21 || x >= 1e21) return String(x);
	    if (x < 0) {
	      s = '-';
	      x = -x;
	    }
	    if (x > 1e-21) {
	      e = log(x * pow(2, 69, 1)) - 69;
	      z = e < 0 ? x * pow(2, -e, 1) : x / pow(2, e, 1);
	      z *= 0x10000000000000;
	      e = 52 - e;
	      if (e > 0) {
	        multiply(0, z);
	        j = f;
	        while (j >= 7) {
	          multiply(1e7, 0);
	          j -= 7;
	        }
	        multiply(pow(10, j, 1), 0);
	        j = e - 1;
	        while (j >= 23) {
	          divide(1 << 23);
	          j -= 23;
	        }
	        divide(1 << j);
	        multiply(1, 1);
	        divide(2);
	        m = numToString();
	      } else {
	        multiply(0, z);
	        multiply(1 << -e, 0);
	        m = numToString() + repeat.call(ZERO, f);
	      }
	    }
	    if (f > 0) {
	      k = m.length;
	      m = s + (k <= f ? '0.' + repeat.call(ZERO, f - k) + m : m.slice(0, k - f) + '.' + m.slice(k - f));
	    } else {
	      m = s + m;
	    } return m;
	  }
	});


/***/ }),
/* 89 */
/***/ (function(module, exports, __webpack_require__) {

	var cof = __webpack_require__(34);
	module.exports = function (it, msg) {
	  if (typeof it != 'number' && cof(it) != 'Number') throw TypeError(msg);
	  return +it;
	};


/***/ }),
/* 90 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var toInteger = __webpack_require__(38);
	var defined = __webpack_require__(35);

	module.exports = function repeat(count) {
	  var str = String(defined(this));
	  var res = '';
	  var n = toInteger(count);
	  if (n < 0 || n == Infinity) throw RangeError("Count can't be negative");
	  for (;n > 0; (n >>>= 1) && (str += str)) if (n & 1) res += str;
	  return res;
	};


/***/ }),
/* 91 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var $fails = __webpack_require__(7);
	var aNumberValue = __webpack_require__(89);
	var $toPrecision = 1.0.toPrecision;

	$export($export.P + $export.F * ($fails(function () {
	  // IE7-
	  return $toPrecision.call(1, undefined) !== '1';
	}) || !$fails(function () {
	  // V8 ~ Android 4.3-
	  $toPrecision.call({});
	})), 'Number', {
	  toPrecision: function toPrecision(precision) {
	    var that = aNumberValue(this, 'Number#toPrecision: incorrect invocation!');
	    return precision === undefined ? $toPrecision.call(that) : $toPrecision.call(that, precision);
	  }
	});


/***/ }),
/* 92 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.1.2.1 Number.EPSILON
	var $export = __webpack_require__(8);

	$export($export.S, 'Number', { EPSILON: Math.pow(2, -52) });


/***/ }),
/* 93 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.1.2.2 Number.isFinite(number)
	var $export = __webpack_require__(8);
	var _isFinite = __webpack_require__(4).isFinite;

	$export($export.S, 'Number', {
	  isFinite: function isFinite(it) {
	    return typeof it == 'number' && _isFinite(it);
	  }
	});


/***/ }),
/* 94 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.1.2.3 Number.isInteger(number)
	var $export = __webpack_require__(8);

	$export($export.S, 'Number', { isInteger: __webpack_require__(95) });


/***/ }),
/* 95 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.1.2.3 Number.isInteger(number)
	var isObject = __webpack_require__(13);
	var floor = Math.floor;
	module.exports = function isInteger(it) {
	  return !isObject(it) && isFinite(it) && floor(it) === it;
	};


/***/ }),
/* 96 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.1.2.4 Number.isNaN(number)
	var $export = __webpack_require__(8);

	$export($export.S, 'Number', {
	  isNaN: function isNaN(number) {
	    // eslint-disable-next-line no-self-compare
	    return number != number;
	  }
	});


/***/ }),
/* 97 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.1.2.5 Number.isSafeInteger(number)
	var $export = __webpack_require__(8);
	var isInteger = __webpack_require__(95);
	var abs = Math.abs;

	$export($export.S, 'Number', {
	  isSafeInteger: function isSafeInteger(number) {
	    return isInteger(number) && abs(number) <= 0x1fffffffffffff;
	  }
	});


/***/ }),
/* 98 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.1.2.6 Number.MAX_SAFE_INTEGER
	var $export = __webpack_require__(8);

	$export($export.S, 'Number', { MAX_SAFE_INTEGER: 0x1fffffffffffff });


/***/ }),
/* 99 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.1.2.10 Number.MIN_SAFE_INTEGER
	var $export = __webpack_require__(8);

	$export($export.S, 'Number', { MIN_SAFE_INTEGER: -0x1fffffffffffff });


/***/ }),
/* 100 */
/***/ (function(module, exports, __webpack_require__) {

	var $export = __webpack_require__(8);
	var $parseFloat = __webpack_require__(85);
	// 20.1.2.12 Number.parseFloat(string)
	$export($export.S + $export.F * (Number.parseFloat != $parseFloat), 'Number', { parseFloat: $parseFloat });


/***/ }),
/* 101 */
/***/ (function(module, exports, __webpack_require__) {

	var $export = __webpack_require__(8);
	var $parseInt = __webpack_require__(81);
	// 20.1.2.13 Number.parseInt(string, radix)
	$export($export.S + $export.F * (Number.parseInt != $parseInt), 'Number', { parseInt: $parseInt });


/***/ }),
/* 102 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.3 Math.acosh(x)
	var $export = __webpack_require__(8);
	var log1p = __webpack_require__(103);
	var sqrt = Math.sqrt;
	var $acosh = Math.acosh;

	$export($export.S + $export.F * !($acosh
	  // V8 bug: https://code.google.com/p/v8/issues/detail?id=3509
	  && Math.floor($acosh(Number.MAX_VALUE)) == 710
	  // Tor Browser bug: Math.acosh(Infinity) -> NaN
	  && $acosh(Infinity) == Infinity
	), 'Math', {
	  acosh: function acosh(x) {
	    return (x = +x) < 1 ? NaN : x > 94906265.62425156
	      ? Math.log(x) + Math.LN2
	      : log1p(x - 1 + sqrt(x - 1) * sqrt(x + 1));
	  }
	});


/***/ }),
/* 103 */
/***/ (function(module, exports) {

	// 20.2.2.20 Math.log1p(x)
	module.exports = Math.log1p || function log1p(x) {
	  return (x = +x) > -1e-8 && x < 1e-8 ? x - x * x / 2 : Math.log(1 + x);
	};


/***/ }),
/* 104 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.5 Math.asinh(x)
	var $export = __webpack_require__(8);
	var $asinh = Math.asinh;

	function asinh(x) {
	  return !isFinite(x = +x) || x == 0 ? x : x < 0 ? -asinh(-x) : Math.log(x + Math.sqrt(x * x + 1));
	}

	// Tor Browser bug: Math.asinh(0) -> -0
	$export($export.S + $export.F * !($asinh && 1 / $asinh(0) > 0), 'Math', { asinh: asinh });


/***/ }),
/* 105 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.7 Math.atanh(x)
	var $export = __webpack_require__(8);
	var $atanh = Math.atanh;

	// Tor Browser bug: Math.atanh(-0) -> 0
	$export($export.S + $export.F * !($atanh && 1 / $atanh(-0) < 0), 'Math', {
	  atanh: function atanh(x) {
	    return (x = +x) == 0 ? x : Math.log((1 + x) / (1 - x)) / 2;
	  }
	});


/***/ }),
/* 106 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.9 Math.cbrt(x)
	var $export = __webpack_require__(8);
	var sign = __webpack_require__(107);

	$export($export.S, 'Math', {
	  cbrt: function cbrt(x) {
	    return sign(x = +x) * Math.pow(Math.abs(x), 1 / 3);
	  }
	});


/***/ }),
/* 107 */
/***/ (function(module, exports) {

	// 20.2.2.28 Math.sign(x)
	module.exports = Math.sign || function sign(x) {
	  // eslint-disable-next-line no-self-compare
	  return (x = +x) == 0 || x != x ? x : x < 0 ? -1 : 1;
	};


/***/ }),
/* 108 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.11 Math.clz32(x)
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', {
	  clz32: function clz32(x) {
	    return (x >>>= 0) ? 31 - Math.floor(Math.log(x + 0.5) * Math.LOG2E) : 32;
	  }
	});


/***/ }),
/* 109 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.12 Math.cosh(x)
	var $export = __webpack_require__(8);
	var exp = Math.exp;

	$export($export.S, 'Math', {
	  cosh: function cosh(x) {
	    return (exp(x = +x) + exp(-x)) / 2;
	  }
	});


/***/ }),
/* 110 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.14 Math.expm1(x)
	var $export = __webpack_require__(8);
	var $expm1 = __webpack_require__(111);

	$export($export.S + $export.F * ($expm1 != Math.expm1), 'Math', { expm1: $expm1 });


/***/ }),
/* 111 */
/***/ (function(module, exports) {

	// 20.2.2.14 Math.expm1(x)
	var $expm1 = Math.expm1;
	module.exports = (!$expm1
	  // Old FF bug
	  || $expm1(10) > 22025.465794806719 || $expm1(10) < 22025.4657948067165168
	  // Tor Browser bug
	  || $expm1(-2e-17) != -2e-17
	) ? function expm1(x) {
	  return (x = +x) == 0 ? x : x > -1e-6 && x < 1e-6 ? x + x * x / 2 : Math.exp(x) - 1;
	} : $expm1;


/***/ }),
/* 112 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.16 Math.fround(x)
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', { fround: __webpack_require__(113) });


/***/ }),
/* 113 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.16 Math.fround(x)
	var sign = __webpack_require__(107);
	var pow = Math.pow;
	var EPSILON = pow(2, -52);
	var EPSILON32 = pow(2, -23);
	var MAX32 = pow(2, 127) * (2 - EPSILON32);
	var MIN32 = pow(2, -126);

	var roundTiesToEven = function (n) {
	  return n + 1 / EPSILON - 1 / EPSILON;
	};

	module.exports = Math.fround || function fround(x) {
	  var $abs = Math.abs(x);
	  var $sign = sign(x);
	  var a, result;
	  if ($abs < MIN32) return $sign * roundTiesToEven($abs / MIN32 / EPSILON32) * MIN32 * EPSILON32;
	  a = (1 + EPSILON32 / EPSILON) * $abs;
	  result = a - (a - $abs);
	  // eslint-disable-next-line no-self-compare
	  if (result > MAX32 || result != result) return $sign * Infinity;
	  return $sign * result;
	};


/***/ }),
/* 114 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.17 Math.hypot([value1[, value2[, … ]]])
	var $export = __webpack_require__(8);
	var abs = Math.abs;

	$export($export.S, 'Math', {
	  hypot: function hypot(value1, value2) { // eslint-disable-line no-unused-vars
	    var sum = 0;
	    var i = 0;
	    var aLen = arguments.length;
	    var larg = 0;
	    var arg, div;
	    while (i < aLen) {
	      arg = abs(arguments[i++]);
	      if (larg < arg) {
	        div = larg / arg;
	        sum = sum * div * div + 1;
	        larg = arg;
	      } else if (arg > 0) {
	        div = arg / larg;
	        sum += div * div;
	      } else sum += arg;
	    }
	    return larg === Infinity ? Infinity : larg * Math.sqrt(sum);
	  }
	});


/***/ }),
/* 115 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.18 Math.imul(x, y)
	var $export = __webpack_require__(8);
	var $imul = Math.imul;

	// some WebKit versions fails with big numbers, some has wrong arity
	$export($export.S + $export.F * __webpack_require__(7)(function () {
	  return $imul(0xffffffff, 5) != -5 || $imul.length != 2;
	}), 'Math', {
	  imul: function imul(x, y) {
	    var UINT16 = 0xffff;
	    var xn = +x;
	    var yn = +y;
	    var xl = UINT16 & xn;
	    var yl = UINT16 & yn;
	    return 0 | xl * yl + ((UINT16 & xn >>> 16) * yl + xl * (UINT16 & yn >>> 16) << 16 >>> 0);
	  }
	});


/***/ }),
/* 116 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.21 Math.log10(x)
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', {
	  log10: function log10(x) {
	    return Math.log(x) * Math.LOG10E;
	  }
	});


/***/ }),
/* 117 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.20 Math.log1p(x)
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', { log1p: __webpack_require__(103) });


/***/ }),
/* 118 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.22 Math.log2(x)
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', {
	  log2: function log2(x) {
	    return Math.log(x) / Math.LN2;
	  }
	});


/***/ }),
/* 119 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.28 Math.sign(x)
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', { sign: __webpack_require__(107) });


/***/ }),
/* 120 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.30 Math.sinh(x)
	var $export = __webpack_require__(8);
	var expm1 = __webpack_require__(111);
	var exp = Math.exp;

	// V8 near Chromium 38 has a problem with very small numbers
	$export($export.S + $export.F * __webpack_require__(7)(function () {
	  return !Math.sinh(-2e-17) != -2e-17;
	}), 'Math', {
	  sinh: function sinh(x) {
	    return Math.abs(x = +x) < 1
	      ? (expm1(x) - expm1(-x)) / 2
	      : (exp(x - 1) - exp(-x - 1)) * (Math.E / 2);
	  }
	});


/***/ }),
/* 121 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.33 Math.tanh(x)
	var $export = __webpack_require__(8);
	var expm1 = __webpack_require__(111);
	var exp = Math.exp;

	$export($export.S, 'Math', {
	  tanh: function tanh(x) {
	    var a = expm1(x = +x);
	    var b = expm1(-x);
	    return a == Infinity ? 1 : b == Infinity ? -1 : (a - b) / (exp(x) + exp(-x));
	  }
	});


/***/ }),
/* 122 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.2.2.34 Math.trunc(x)
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', {
	  trunc: function trunc(it) {
	    return (it > 0 ? Math.floor : Math.ceil)(it);
	  }
	});


/***/ }),
/* 123 */
/***/ (function(module, exports, __webpack_require__) {

	var $export = __webpack_require__(8);
	var toAbsoluteIndex = __webpack_require__(39);
	var fromCharCode = String.fromCharCode;
	var $fromCodePoint = String.fromCodePoint;

	// length should be 1, old FF problem
	$export($export.S + $export.F * (!!$fromCodePoint && $fromCodePoint.length != 1), 'String', {
	  // 21.1.2.2 String.fromCodePoint(...codePoints)
	  fromCodePoint: function fromCodePoint(x) { // eslint-disable-line no-unused-vars
	    var res = [];
	    var aLen = arguments.length;
	    var i = 0;
	    var code;
	    while (aLen > i) {
	      code = +arguments[i++];
	      if (toAbsoluteIndex(code, 0x10ffff) !== code) throw RangeError(code + ' is not a valid code point');
	      res.push(code < 0x10000
	        ? fromCharCode(code)
	        : fromCharCode(((code -= 0x10000) >> 10) + 0xd800, code % 0x400 + 0xdc00)
	      );
	    } return res.join('');
	  }
	});


/***/ }),
/* 124 */
/***/ (function(module, exports, __webpack_require__) {

	var $export = __webpack_require__(8);
	var toIObject = __webpack_require__(32);
	var toLength = __webpack_require__(37);

	$export($export.S, 'String', {
	  // 21.1.2.4 String.raw(callSite, ...substitutions)
	  raw: function raw(callSite) {
	    var tpl = toIObject(callSite.raw);
	    var len = toLength(tpl.length);
	    var aLen = arguments.length;
	    var res = [];
	    var i = 0;
	    while (len > i) {
	      res.push(String(tpl[i++]));
	      if (i < aLen) res.push(String(arguments[i]));
	    } return res.join('');
	  }
	});


/***/ }),
/* 125 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// 21.1.3.25 String.prototype.trim()
	__webpack_require__(82)('trim', function ($trim) {
	  return function trim() {
	    return $trim(this, 3);
	  };
	});


/***/ }),
/* 126 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $at = __webpack_require__(127)(true);

	// 21.1.3.27 String.prototype[@@iterator]()
	__webpack_require__(128)(String, 'String', function (iterated) {
	  this._t = String(iterated); // target
	  this._i = 0;                // next index
	// 21.1.5.2.1 %StringIteratorPrototype%.next()
	}, function () {
	  var O = this._t;
	  var index = this._i;
	  var point;
	  if (index >= O.length) return { value: undefined, done: true };
	  point = $at(O, index);
	  this._i += point.length;
	  return { value: point, done: false };
	});


/***/ }),
/* 127 */
/***/ (function(module, exports, __webpack_require__) {

	var toInteger = __webpack_require__(38);
	var defined = __webpack_require__(35);
	// true  -> String#at
	// false -> String#codePointAt
	module.exports = function (TO_STRING) {
	  return function (that, pos) {
	    var s = String(defined(that));
	    var i = toInteger(pos);
	    var l = s.length;
	    var a, b;
	    if (i < 0 || i >= l) return TO_STRING ? '' : undefined;
	    a = s.charCodeAt(i);
	    return a < 0xd800 || a > 0xdbff || i + 1 === l || (b = s.charCodeAt(i + 1)) < 0xdc00 || b > 0xdfff
	      ? TO_STRING ? s.charAt(i) : a
	      : TO_STRING ? s.slice(i, i + 2) : (a - 0xd800 << 10) + (b - 0xdc00) + 0x10000;
	  };
	};


/***/ }),
/* 128 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var LIBRARY = __webpack_require__(24);
	var $export = __webpack_require__(8);
	var redefine = __webpack_require__(18);
	var hide = __webpack_require__(10);
	var Iterators = __webpack_require__(129);
	var $iterCreate = __webpack_require__(130);
	var setToStringTag = __webpack_require__(25);
	var getPrototypeOf = __webpack_require__(58);
	var ITERATOR = __webpack_require__(26)('iterator');
	var BUGGY = !([].keys && 'next' in [].keys()); // Safari has buggy iterators w/o `next`
	var FF_ITERATOR = '@@iterator';
	var KEYS = 'keys';
	var VALUES = 'values';

	var returnThis = function () { return this; };

	module.exports = function (Base, NAME, Constructor, next, DEFAULT, IS_SET, FORCED) {
	  $iterCreate(Constructor, NAME, next);
	  var getMethod = function (kind) {
	    if (!BUGGY && kind in proto) return proto[kind];
	    switch (kind) {
	      case KEYS: return function keys() { return new Constructor(this, kind); };
	      case VALUES: return function values() { return new Constructor(this, kind); };
	    } return function entries() { return new Constructor(this, kind); };
	  };
	  var TAG = NAME + ' Iterator';
	  var DEF_VALUES = DEFAULT == VALUES;
	  var VALUES_BUG = false;
	  var proto = Base.prototype;
	  var $native = proto[ITERATOR] || proto[FF_ITERATOR] || DEFAULT && proto[DEFAULT];
	  var $default = $native || getMethod(DEFAULT);
	  var $entries = DEFAULT ? !DEF_VALUES ? $default : getMethod('entries') : undefined;
	  var $anyNative = NAME == 'Array' ? proto.entries || $native : $native;
	  var methods, key, IteratorPrototype;
	  // Fix native
	  if ($anyNative) {
	    IteratorPrototype = getPrototypeOf($anyNative.call(new Base()));
	    if (IteratorPrototype !== Object.prototype && IteratorPrototype.next) {
	      // Set @@toStringTag to native iterators
	      setToStringTag(IteratorPrototype, TAG, true);
	      // fix for some old engines
	      if (!LIBRARY && typeof IteratorPrototype[ITERATOR] != 'function') hide(IteratorPrototype, ITERATOR, returnThis);
	    }
	  }
	  // fix Array#{values, @@iterator}.name in V8 / FF
	  if (DEF_VALUES && $native && $native.name !== VALUES) {
	    VALUES_BUG = true;
	    $default = function values() { return $native.call(this); };
	  }
	  // Define iterator
	  if ((!LIBRARY || FORCED) && (BUGGY || VALUES_BUG || !proto[ITERATOR])) {
	    hide(proto, ITERATOR, $default);
	  }
	  // Plug for library
	  Iterators[NAME] = $default;
	  Iterators[TAG] = returnThis;
	  if (DEFAULT) {
	    methods = {
	      values: DEF_VALUES ? $default : getMethod(VALUES),
	      keys: IS_SET ? $default : getMethod(KEYS),
	      entries: $entries
	    };
	    if (FORCED) for (key in methods) {
	      if (!(key in proto)) redefine(proto, key, methods[key]);
	    } else $export($export.P + $export.F * (BUGGY || VALUES_BUG), NAME, methods);
	  }
	  return methods;
	};


/***/ }),
/* 129 */
/***/ (function(module, exports) {

	module.exports = {};


/***/ }),
/* 130 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var create = __webpack_require__(45);
	var descriptor = __webpack_require__(17);
	var setToStringTag = __webpack_require__(25);
	var IteratorPrototype = {};

	// 25.1.2.1.1 %IteratorPrototype%[@@iterator]()
	__webpack_require__(10)(IteratorPrototype, __webpack_require__(26)('iterator'), function () { return this; });

	module.exports = function (Constructor, NAME, next) {
	  Constructor.prototype = create(IteratorPrototype, { next: descriptor(1, next) });
	  setToStringTag(Constructor, NAME + ' Iterator');
	};


/***/ }),
/* 131 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var $at = __webpack_require__(127)(false);
	$export($export.P, 'String', {
	  // 21.1.3.3 String.prototype.codePointAt(pos)
	  codePointAt: function codePointAt(pos) {
	    return $at(this, pos);
	  }
	});


/***/ }),
/* 132 */
/***/ (function(module, exports, __webpack_require__) {

	// 21.1.3.6 String.prototype.endsWith(searchString [, endPosition])
	'use strict';
	var $export = __webpack_require__(8);
	var toLength = __webpack_require__(37);
	var context = __webpack_require__(133);
	var ENDS_WITH = 'endsWith';
	var $endsWith = ''[ENDS_WITH];

	$export($export.P + $export.F * __webpack_require__(135)(ENDS_WITH), 'String', {
	  endsWith: function endsWith(searchString /* , endPosition = @length */) {
	    var that = context(this, searchString, ENDS_WITH);
	    var endPosition = arguments.length > 1 ? arguments[1] : undefined;
	    var len = toLength(that.length);
	    var end = endPosition === undefined ? len : Math.min(toLength(endPosition), len);
	    var search = String(searchString);
	    return $endsWith
	      ? $endsWith.call(that, search, end)
	      : that.slice(end - search.length, end) === search;
	  }
	});


/***/ }),
/* 133 */
/***/ (function(module, exports, __webpack_require__) {

	// helper for String#{startsWith, endsWith, includes}
	var isRegExp = __webpack_require__(134);
	var defined = __webpack_require__(35);

	module.exports = function (that, searchString, NAME) {
	  if (isRegExp(searchString)) throw TypeError('String#' + NAME + " doesn't accept regex!");
	  return String(defined(that));
	};


/***/ }),
/* 134 */
/***/ (function(module, exports, __webpack_require__) {

	// 7.2.8 IsRegExp(argument)
	var isObject = __webpack_require__(13);
	var cof = __webpack_require__(34);
	var MATCH = __webpack_require__(26)('match');
	module.exports = function (it) {
	  var isRegExp;
	  return isObject(it) && ((isRegExp = it[MATCH]) !== undefined ? !!isRegExp : cof(it) == 'RegExp');
	};


/***/ }),
/* 135 */
/***/ (function(module, exports, __webpack_require__) {

	var MATCH = __webpack_require__(26)('match');
	module.exports = function (KEY) {
	  var re = /./;
	  try {
	    '/./'[KEY](re);
	  } catch (e) {
	    try {
	      re[MATCH] = false;
	      return !'/./'[KEY](re);
	    } catch (f) { /* empty */ }
	  } return true;
	};


/***/ }),
/* 136 */
/***/ (function(module, exports, __webpack_require__) {

	// 21.1.3.7 String.prototype.includes(searchString, position = 0)
	'use strict';
	var $export = __webpack_require__(8);
	var context = __webpack_require__(133);
	var INCLUDES = 'includes';

	$export($export.P + $export.F * __webpack_require__(135)(INCLUDES), 'String', {
	  includes: function includes(searchString /* , position = 0 */) {
	    return !!~context(this, searchString, INCLUDES)
	      .indexOf(searchString, arguments.length > 1 ? arguments[1] : undefined);
	  }
	});


/***/ }),
/* 137 */
/***/ (function(module, exports, __webpack_require__) {

	var $export = __webpack_require__(8);

	$export($export.P, 'String', {
	  // 21.1.3.13 String.prototype.repeat(count)
	  repeat: __webpack_require__(90)
	});


/***/ }),
/* 138 */
/***/ (function(module, exports, __webpack_require__) {

	// 21.1.3.18 String.prototype.startsWith(searchString [, position ])
	'use strict';
	var $export = __webpack_require__(8);
	var toLength = __webpack_require__(37);
	var context = __webpack_require__(133);
	var STARTS_WITH = 'startsWith';
	var $startsWith = ''[STARTS_WITH];

	$export($export.P + $export.F * __webpack_require__(135)(STARTS_WITH), 'String', {
	  startsWith: function startsWith(searchString /* , position = 0 */) {
	    var that = context(this, searchString, STARTS_WITH);
	    var index = toLength(Math.min(arguments.length > 1 ? arguments[1] : undefined, that.length));
	    var search = String(searchString);
	    return $startsWith
	      ? $startsWith.call(that, search, index)
	      : that.slice(index, index + search.length) === search;
	  }
	});


/***/ }),
/* 139 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// B.2.3.2 String.prototype.anchor(name)
	__webpack_require__(140)('anchor', function (createHTML) {
	  return function anchor(name) {
	    return createHTML(this, 'a', 'name', name);
	  };
	});


/***/ }),
/* 140 */
/***/ (function(module, exports, __webpack_require__) {

	var $export = __webpack_require__(8);
	var fails = __webpack_require__(7);
	var defined = __webpack_require__(35);
	var quot = /"/g;
	// B.2.3.2.1 CreateHTML(string, tag, attribute, value)
	var createHTML = function (string, tag, attribute, value) {
	  var S = String(defined(string));
	  var p1 = '<' + tag;
	  if (attribute !== '') p1 += ' ' + attribute + '="' + String(value).replace(quot, '&quot;') + '"';
	  return p1 + '>' + S + '</' + tag + '>';
	};
	module.exports = function (NAME, exec) {
	  var O = {};
	  O[NAME] = exec(createHTML);
	  $export($export.P + $export.F * fails(function () {
	    var test = ''[NAME]('"');
	    return test !== test.toLowerCase() || test.split('"').length > 3;
	  }), 'String', O);
	};


/***/ }),
/* 141 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// B.2.3.3 String.prototype.big()
	__webpack_require__(140)('big', function (createHTML) {
	  return function big() {
	    return createHTML(this, 'big', '', '');
	  };
	});


/***/ }),
/* 142 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// B.2.3.4 String.prototype.blink()
	__webpack_require__(140)('blink', function (createHTML) {
	  return function blink() {
	    return createHTML(this, 'blink', '', '');
	  };
	});


/***/ }),
/* 143 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// B.2.3.5 String.prototype.bold()
	__webpack_require__(140)('bold', function (createHTML) {
	  return function bold() {
	    return createHTML(this, 'b', '', '');
	  };
	});


/***/ }),
/* 144 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// B.2.3.6 String.prototype.fixed()
	__webpack_require__(140)('fixed', function (createHTML) {
	  return function fixed() {
	    return createHTML(this, 'tt', '', '');
	  };
	});


/***/ }),
/* 145 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// B.2.3.7 String.prototype.fontcolor(color)
	__webpack_require__(140)('fontcolor', function (createHTML) {
	  return function fontcolor(color) {
	    return createHTML(this, 'font', 'color', color);
	  };
	});


/***/ }),
/* 146 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// B.2.3.8 String.prototype.fontsize(size)
	__webpack_require__(140)('fontsize', function (createHTML) {
	  return function fontsize(size) {
	    return createHTML(this, 'font', 'size', size);
	  };
	});


/***/ }),
/* 147 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// B.2.3.9 String.prototype.italics()
	__webpack_require__(140)('italics', function (createHTML) {
	  return function italics() {
	    return createHTML(this, 'i', '', '');
	  };
	});


/***/ }),
/* 148 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// B.2.3.10 String.prototype.link(url)
	__webpack_require__(140)('link', function (createHTML) {
	  return function link(url) {
	    return createHTML(this, 'a', 'href', url);
	  };
	});


/***/ }),
/* 149 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// B.2.3.11 String.prototype.small()
	__webpack_require__(140)('small', function (createHTML) {
	  return function small() {
	    return createHTML(this, 'small', '', '');
	  };
	});


/***/ }),
/* 150 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// B.2.3.12 String.prototype.strike()
	__webpack_require__(140)('strike', function (createHTML) {
	  return function strike() {
	    return createHTML(this, 'strike', '', '');
	  };
	});


/***/ }),
/* 151 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// B.2.3.13 String.prototype.sub()
	__webpack_require__(140)('sub', function (createHTML) {
	  return function sub() {
	    return createHTML(this, 'sub', '', '');
	  };
	});


/***/ }),
/* 152 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// B.2.3.14 String.prototype.sup()
	__webpack_require__(140)('sup', function (createHTML) {
	  return function sup() {
	    return createHTML(this, 'sup', '', '');
	  };
	});


/***/ }),
/* 153 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.3.3.1 / 15.9.4.4 Date.now()
	var $export = __webpack_require__(8);

	$export($export.S, 'Date', { now: function () { return new Date().getTime(); } });


/***/ }),
/* 154 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var toObject = __webpack_require__(57);
	var toPrimitive = __webpack_require__(16);

	$export($export.P + $export.F * __webpack_require__(7)(function () {
	  return new Date(NaN).toJSON() !== null
	    || Date.prototype.toJSON.call({ toISOString: function () { return 1; } }) !== 1;
	}), 'Date', {
	  // eslint-disable-next-line no-unused-vars
	  toJSON: function toJSON(key) {
	    var O = toObject(this);
	    var pv = toPrimitive(O);
	    return typeof pv == 'number' && !isFinite(pv) ? null : O.toISOString();
	  }
	});


/***/ }),
/* 155 */
/***/ (function(module, exports, __webpack_require__) {

	// 20.3.4.36 / 15.9.5.43 Date.prototype.toISOString()
	var $export = __webpack_require__(8);
	var toISOString = __webpack_require__(156);

	// PhantomJS / old WebKit has a broken implementations
	$export($export.P + $export.F * (Date.prototype.toISOString !== toISOString), 'Date', {
	  toISOString: toISOString
	});


/***/ }),
/* 156 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// 20.3.4.36 / 15.9.5.43 Date.prototype.toISOString()
	var fails = __webpack_require__(7);
	var getTime = Date.prototype.getTime;
	var $toISOString = Date.prototype.toISOString;

	var lz = function (num) {
	  return num > 9 ? num : '0' + num;
	};

	// PhantomJS / old WebKit has a broken implementations
	module.exports = (fails(function () {
	  return $toISOString.call(new Date(-5e13 - 1)) != '0385-07-25T07:06:39.999Z';
	}) || !fails(function () {
	  $toISOString.call(new Date(NaN));
	})) ? function toISOString() {
	  if (!isFinite(getTime.call(this))) throw RangeError('Invalid time value');
	  var d = this;
	  var y = d.getUTCFullYear();
	  var m = d.getUTCMilliseconds();
	  var s = y < 0 ? '-' : y > 9999 ? '+' : '';
	  return s + ('00000' + Math.abs(y)).slice(s ? -6 : -4) +
	    '-' + lz(d.getUTCMonth() + 1) + '-' + lz(d.getUTCDate()) +
	    'T' + lz(d.getUTCHours()) + ':' + lz(d.getUTCMinutes()) +
	    ':' + lz(d.getUTCSeconds()) + '.' + (m > 99 ? m : '0' + lz(m)) + 'Z';
	} : $toISOString;


/***/ }),
/* 157 */
/***/ (function(module, exports, __webpack_require__) {

	var DateProto = Date.prototype;
	var INVALID_DATE = 'Invalid Date';
	var TO_STRING = 'toString';
	var $toString = DateProto[TO_STRING];
	var getTime = DateProto.getTime;
	if (new Date(NaN) + '' != INVALID_DATE) {
	  __webpack_require__(18)(DateProto, TO_STRING, function toString() {
	    var value = getTime.call(this);
	    // eslint-disable-next-line no-self-compare
	    return value === value ? $toString.call(this) : INVALID_DATE;
	  });
	}


/***/ }),
/* 158 */
/***/ (function(module, exports, __webpack_require__) {

	var TO_PRIMITIVE = __webpack_require__(26)('toPrimitive');
	var proto = Date.prototype;

	if (!(TO_PRIMITIVE in proto)) __webpack_require__(10)(proto, TO_PRIMITIVE, __webpack_require__(159));


/***/ }),
/* 159 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var anObject = __webpack_require__(12);
	var toPrimitive = __webpack_require__(16);
	var NUMBER = 'number';

	module.exports = function (hint) {
	  if (hint !== 'string' && hint !== NUMBER && hint !== 'default') throw TypeError('Incorrect hint');
	  return toPrimitive(anObject(this), hint != NUMBER);
	};


/***/ }),
/* 160 */
/***/ (function(module, exports, __webpack_require__) {

	// 22.1.2.2 / 15.4.3.2 Array.isArray(arg)
	var $export = __webpack_require__(8);

	$export($export.S, 'Array', { isArray: __webpack_require__(44) });


/***/ }),
/* 161 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var ctx = __webpack_require__(20);
	var $export = __webpack_require__(8);
	var toObject = __webpack_require__(57);
	var call = __webpack_require__(162);
	var isArrayIter = __webpack_require__(163);
	var toLength = __webpack_require__(37);
	var createProperty = __webpack_require__(164);
	var getIterFn = __webpack_require__(165);

	$export($export.S + $export.F * !__webpack_require__(166)(function (iter) { Array.from(iter); }), 'Array', {
	  // 22.1.2.1 Array.from(arrayLike, mapfn = undefined, thisArg = undefined)
	  from: function from(arrayLike /* , mapfn = undefined, thisArg = undefined */) {
	    var O = toObject(arrayLike);
	    var C = typeof this == 'function' ? this : Array;
	    var aLen = arguments.length;
	    var mapfn = aLen > 1 ? arguments[1] : undefined;
	    var mapping = mapfn !== undefined;
	    var index = 0;
	    var iterFn = getIterFn(O);
	    var length, result, step, iterator;
	    if (mapping) mapfn = ctx(mapfn, aLen > 2 ? arguments[2] : undefined, 2);
	    // if object isn't iterable or it's array with default iterator - use simple case
	    if (iterFn != undefined && !(C == Array && isArrayIter(iterFn))) {
	      for (iterator = iterFn.call(O), result = new C(); !(step = iterator.next()).done; index++) {
	        createProperty(result, index, mapping ? call(iterator, mapfn, [step.value, index], true) : step.value);
	      }
	    } else {
	      length = toLength(O.length);
	      for (result = new C(length); length > index; index++) {
	        createProperty(result, index, mapping ? mapfn(O[index], index) : O[index]);
	      }
	    }
	    result.length = index;
	    return result;
	  }
	});


/***/ }),
/* 162 */
/***/ (function(module, exports, __webpack_require__) {

	// call something on iterator step with safe closing on error
	var anObject = __webpack_require__(12);
	module.exports = function (iterator, fn, value, entries) {
	  try {
	    return entries ? fn(anObject(value)[0], value[1]) : fn(value);
	  // 7.4.6 IteratorClose(iterator, completion)
	  } catch (e) {
	    var ret = iterator['return'];
	    if (ret !== undefined) anObject(ret.call(iterator));
	    throw e;
	  }
	};


/***/ }),
/* 163 */
/***/ (function(module, exports, __webpack_require__) {

	// check on default Array iterator
	var Iterators = __webpack_require__(129);
	var ITERATOR = __webpack_require__(26)('iterator');
	var ArrayProto = Array.prototype;

	module.exports = function (it) {
	  return it !== undefined && (Iterators.Array === it || ArrayProto[ITERATOR] === it);
	};


/***/ }),
/* 164 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $defineProperty = __webpack_require__(11);
	var createDesc = __webpack_require__(17);

	module.exports = function (object, index, value) {
	  if (index in object) $defineProperty.f(object, index, createDesc(0, value));
	  else object[index] = value;
	};


/***/ }),
/* 165 */
/***/ (function(module, exports, __webpack_require__) {

	var classof = __webpack_require__(74);
	var ITERATOR = __webpack_require__(26)('iterator');
	var Iterators = __webpack_require__(129);
	module.exports = __webpack_require__(9).getIteratorMethod = function (it) {
	  if (it != undefined) return it[ITERATOR]
	    || it['@@iterator']
	    || Iterators[classof(it)];
	};


/***/ }),
/* 166 */
/***/ (function(module, exports, __webpack_require__) {

	var ITERATOR = __webpack_require__(26)('iterator');
	var SAFE_CLOSING = false;

	try {
	  var riter = [7][ITERATOR]();
	  riter['return'] = function () { SAFE_CLOSING = true; };
	  // eslint-disable-next-line no-throw-literal
	  Array.from(riter, function () { throw 2; });
	} catch (e) { /* empty */ }

	module.exports = function (exec, skipClosing) {
	  if (!skipClosing && !SAFE_CLOSING) return false;
	  var safe = false;
	  try {
	    var arr = [7];
	    var iter = arr[ITERATOR]();
	    iter.next = function () { return { done: safe = true }; };
	    arr[ITERATOR] = function () { return iter; };
	    exec(arr);
	  } catch (e) { /* empty */ }
	  return safe;
	};


/***/ }),
/* 167 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var createProperty = __webpack_require__(164);

	// WebKit Array.of isn't generic
	$export($export.S + $export.F * __webpack_require__(7)(function () {
	  function F() { /* empty */ }
	  return !(Array.of.call(F) instanceof F);
	}), 'Array', {
	  // 22.1.2.3 Array.of( ...items)
	  of: function of(/* ...args */) {
	    var index = 0;
	    var aLen = arguments.length;
	    var result = new (typeof this == 'function' ? this : Array)(aLen);
	    while (aLen > index) createProperty(result, index, arguments[index++]);
	    result.length = aLen;
	    return result;
	  }
	});


/***/ }),
/* 168 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// 22.1.3.13 Array.prototype.join(separator)
	var $export = __webpack_require__(8);
	var toIObject = __webpack_require__(32);
	var arrayJoin = [].join;

	// fallback for not array-like strings
	$export($export.P + $export.F * (__webpack_require__(33) != Object || !__webpack_require__(169)(arrayJoin)), 'Array', {
	  join: function join(separator) {
	    return arrayJoin.call(toIObject(this), separator === undefined ? ',' : separator);
	  }
	});


/***/ }),
/* 169 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var fails = __webpack_require__(7);

	module.exports = function (method, arg) {
	  return !!method && fails(function () {
	    // eslint-disable-next-line no-useless-call
	    arg ? method.call(null, function () { /* empty */ }, 1) : method.call(null);
	  });
	};


/***/ }),
/* 170 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var html = __webpack_require__(47);
	var cof = __webpack_require__(34);
	var toAbsoluteIndex = __webpack_require__(39);
	var toLength = __webpack_require__(37);
	var arraySlice = [].slice;

	// fallback for not array-like ES3 strings and DOM objects
	$export($export.P + $export.F * __webpack_require__(7)(function () {
	  if (html) arraySlice.call(html);
	}), 'Array', {
	  slice: function slice(begin, end) {
	    var len = toLength(this.length);
	    var klass = cof(this);
	    end = end === undefined ? len : end;
	    if (klass == 'Array') return arraySlice.call(this, begin, end);
	    var start = toAbsoluteIndex(begin, len);
	    var upTo = toAbsoluteIndex(end, len);
	    var size = toLength(upTo - start);
	    var cloned = new Array(size);
	    var i = 0;
	    for (; i < size; i++) cloned[i] = klass == 'String'
	      ? this.charAt(start + i)
	      : this[start + i];
	    return cloned;
	  }
	});


/***/ }),
/* 171 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var aFunction = __webpack_require__(21);
	var toObject = __webpack_require__(57);
	var fails = __webpack_require__(7);
	var $sort = [].sort;
	var test = [1, 2, 3];

	$export($export.P + $export.F * (fails(function () {
	  // IE8-
	  test.sort(undefined);
	}) || !fails(function () {
	  // V8 bug
	  test.sort(null);
	  // Old WebKit
	}) || !__webpack_require__(169)($sort)), 'Array', {
	  // 22.1.3.25 Array.prototype.sort(comparefn)
	  sort: function sort(comparefn) {
	    return comparefn === undefined
	      ? $sort.call(toObject(this))
	      : $sort.call(toObject(this), aFunction(comparefn));
	  }
	});


/***/ }),
/* 172 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var $forEach = __webpack_require__(173)(0);
	var STRICT = __webpack_require__(169)([].forEach, true);

	$export($export.P + $export.F * !STRICT, 'Array', {
	  // 22.1.3.10 / 15.4.4.18 Array.prototype.forEach(callbackfn [, thisArg])
	  forEach: function forEach(callbackfn /* , thisArg */) {
	    return $forEach(this, callbackfn, arguments[1]);
	  }
	});


/***/ }),
/* 173 */
/***/ (function(module, exports, __webpack_require__) {

	// 0 -> Array#forEach
	// 1 -> Array#map
	// 2 -> Array#filter
	// 3 -> Array#some
	// 4 -> Array#every
	// 5 -> Array#find
	// 6 -> Array#findIndex
	var ctx = __webpack_require__(20);
	var IObject = __webpack_require__(33);
	var toObject = __webpack_require__(57);
	var toLength = __webpack_require__(37);
	var asc = __webpack_require__(174);
	module.exports = function (TYPE, $create) {
	  var IS_MAP = TYPE == 1;
	  var IS_FILTER = TYPE == 2;
	  var IS_SOME = TYPE == 3;
	  var IS_EVERY = TYPE == 4;
	  var IS_FIND_INDEX = TYPE == 6;
	  var NO_HOLES = TYPE == 5 || IS_FIND_INDEX;
	  var create = $create || asc;
	  return function ($this, callbackfn, that) {
	    var O = toObject($this);
	    var self = IObject(O);
	    var f = ctx(callbackfn, that, 3);
	    var length = toLength(self.length);
	    var index = 0;
	    var result = IS_MAP ? create($this, length) : IS_FILTER ? create($this, 0) : undefined;
	    var val, res;
	    for (;length > index; index++) if (NO_HOLES || index in self) {
	      val = self[index];
	      res = f(val, index, O);
	      if (TYPE) {
	        if (IS_MAP) result[index] = res;   // map
	        else if (res) switch (TYPE) {
	          case 3: return true;             // some
	          case 5: return val;              // find
	          case 6: return index;            // findIndex
	          case 2: result.push(val);        // filter
	        } else if (IS_EVERY) return false; // every
	      }
	    }
	    return IS_FIND_INDEX ? -1 : IS_SOME || IS_EVERY ? IS_EVERY : result;
	  };
	};


/***/ }),
/* 174 */
/***/ (function(module, exports, __webpack_require__) {

	// 9.4.2.3 ArraySpeciesCreate(originalArray, length)
	var speciesConstructor = __webpack_require__(175);

	module.exports = function (original, length) {
	  return new (speciesConstructor(original))(length);
	};


/***/ }),
/* 175 */
/***/ (function(module, exports, __webpack_require__) {

	var isObject = __webpack_require__(13);
	var isArray = __webpack_require__(44);
	var SPECIES = __webpack_require__(26)('species');

	module.exports = function (original) {
	  var C;
	  if (isArray(original)) {
	    C = original.constructor;
	    // cross-realm fallback
	    if (typeof C == 'function' && (C === Array || isArray(C.prototype))) C = undefined;
	    if (isObject(C)) {
	      C = C[SPECIES];
	      if (C === null) C = undefined;
	    }
	  } return C === undefined ? Array : C;
	};


/***/ }),
/* 176 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var $map = __webpack_require__(173)(1);

	$export($export.P + $export.F * !__webpack_require__(169)([].map, true), 'Array', {
	  // 22.1.3.15 / 15.4.4.19 Array.prototype.map(callbackfn [, thisArg])
	  map: function map(callbackfn /* , thisArg */) {
	    return $map(this, callbackfn, arguments[1]);
	  }
	});


/***/ }),
/* 177 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var $filter = __webpack_require__(173)(2);

	$export($export.P + $export.F * !__webpack_require__(169)([].filter, true), 'Array', {
	  // 22.1.3.7 / 15.4.4.20 Array.prototype.filter(callbackfn [, thisArg])
	  filter: function filter(callbackfn /* , thisArg */) {
	    return $filter(this, callbackfn, arguments[1]);
	  }
	});


/***/ }),
/* 178 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var $some = __webpack_require__(173)(3);

	$export($export.P + $export.F * !__webpack_require__(169)([].some, true), 'Array', {
	  // 22.1.3.23 / 15.4.4.17 Array.prototype.some(callbackfn [, thisArg])
	  some: function some(callbackfn /* , thisArg */) {
	    return $some(this, callbackfn, arguments[1]);
	  }
	});


/***/ }),
/* 179 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var $every = __webpack_require__(173)(4);

	$export($export.P + $export.F * !__webpack_require__(169)([].every, true), 'Array', {
	  // 22.1.3.5 / 15.4.4.16 Array.prototype.every(callbackfn [, thisArg])
	  every: function every(callbackfn /* , thisArg */) {
	    return $every(this, callbackfn, arguments[1]);
	  }
	});


/***/ }),
/* 180 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var $reduce = __webpack_require__(181);

	$export($export.P + $export.F * !__webpack_require__(169)([].reduce, true), 'Array', {
	  // 22.1.3.18 / 15.4.4.21 Array.prototype.reduce(callbackfn [, initialValue])
	  reduce: function reduce(callbackfn /* , initialValue */) {
	    return $reduce(this, callbackfn, arguments.length, arguments[1], false);
	  }
	});


/***/ }),
/* 181 */
/***/ (function(module, exports, __webpack_require__) {

	var aFunction = __webpack_require__(21);
	var toObject = __webpack_require__(57);
	var IObject = __webpack_require__(33);
	var toLength = __webpack_require__(37);

	module.exports = function (that, callbackfn, aLen, memo, isRight) {
	  aFunction(callbackfn);
	  var O = toObject(that);
	  var self = IObject(O);
	  var length = toLength(O.length);
	  var index = isRight ? length - 1 : 0;
	  var i = isRight ? -1 : 1;
	  if (aLen < 2) for (;;) {
	    if (index in self) {
	      memo = self[index];
	      index += i;
	      break;
	    }
	    index += i;
	    if (isRight ? index < 0 : length <= index) {
	      throw TypeError('Reduce of empty array with no initial value');
	    }
	  }
	  for (;isRight ? index >= 0 : length > index; index += i) if (index in self) {
	    memo = callbackfn(memo, self[index], index, O);
	  }
	  return memo;
	};


/***/ }),
/* 182 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var $reduce = __webpack_require__(181);

	$export($export.P + $export.F * !__webpack_require__(169)([].reduceRight, true), 'Array', {
	  // 22.1.3.19 / 15.4.4.22 Array.prototype.reduceRight(callbackfn [, initialValue])
	  reduceRight: function reduceRight(callbackfn /* , initialValue */) {
	    return $reduce(this, callbackfn, arguments.length, arguments[1], true);
	  }
	});


/***/ }),
/* 183 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var $indexOf = __webpack_require__(36)(false);
	var $native = [].indexOf;
	var NEGATIVE_ZERO = !!$native && 1 / [1].indexOf(1, -0) < 0;

	$export($export.P + $export.F * (NEGATIVE_ZERO || !__webpack_require__(169)($native)), 'Array', {
	  // 22.1.3.11 / 15.4.4.14 Array.prototype.indexOf(searchElement [, fromIndex])
	  indexOf: function indexOf(searchElement /* , fromIndex = 0 */) {
	    return NEGATIVE_ZERO
	      // convert -0 to +0
	      ? $native.apply(this, arguments) || 0
	      : $indexOf(this, searchElement, arguments[1]);
	  }
	});


/***/ }),
/* 184 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var toIObject = __webpack_require__(32);
	var toInteger = __webpack_require__(38);
	var toLength = __webpack_require__(37);
	var $native = [].lastIndexOf;
	var NEGATIVE_ZERO = !!$native && 1 / [1].lastIndexOf(1, -0) < 0;

	$export($export.P + $export.F * (NEGATIVE_ZERO || !__webpack_require__(169)($native)), 'Array', {
	  // 22.1.3.14 / 15.4.4.15 Array.prototype.lastIndexOf(searchElement [, fromIndex])
	  lastIndexOf: function lastIndexOf(searchElement /* , fromIndex = @[*-1] */) {
	    // convert -0 to +0
	    if (NEGATIVE_ZERO) return $native.apply(this, arguments) || 0;
	    var O = toIObject(this);
	    var length = toLength(O.length);
	    var index = length - 1;
	    if (arguments.length > 1) index = Math.min(index, toInteger(arguments[1]));
	    if (index < 0) index = length + index;
	    for (;index >= 0; index--) if (index in O) if (O[index] === searchElement) return index || 0;
	    return -1;
	  }
	});


/***/ }),
/* 185 */
/***/ (function(module, exports, __webpack_require__) {

	// 22.1.3.3 Array.prototype.copyWithin(target, start, end = this.length)
	var $export = __webpack_require__(8);

	$export($export.P, 'Array', { copyWithin: __webpack_require__(186) });

	__webpack_require__(187)('copyWithin');


/***/ }),
/* 186 */
/***/ (function(module, exports, __webpack_require__) {

	// 22.1.3.3 Array.prototype.copyWithin(target, start, end = this.length)
	'use strict';
	var toObject = __webpack_require__(57);
	var toAbsoluteIndex = __webpack_require__(39);
	var toLength = __webpack_require__(37);

	module.exports = [].copyWithin || function copyWithin(target /* = 0 */, start /* = 0, end = @length */) {
	  var O = toObject(this);
	  var len = toLength(O.length);
	  var to = toAbsoluteIndex(target, len);
	  var from = toAbsoluteIndex(start, len);
	  var end = arguments.length > 2 ? arguments[2] : undefined;
	  var count = Math.min((end === undefined ? len : toAbsoluteIndex(end, len)) - from, len - to);
	  var inc = 1;
	  if (from < to && to < from + count) {
	    inc = -1;
	    from += count - 1;
	    to += count - 1;
	  }
	  while (count-- > 0) {
	    if (from in O) O[to] = O[from];
	    else delete O[to];
	    to += inc;
	    from += inc;
	  } return O;
	};


/***/ }),
/* 187 */
/***/ (function(module, exports, __webpack_require__) {

	// 22.1.3.31 Array.prototype[@@unscopables]
	var UNSCOPABLES = __webpack_require__(26)('unscopables');
	var ArrayProto = Array.prototype;
	if (ArrayProto[UNSCOPABLES] == undefined) __webpack_require__(10)(ArrayProto, UNSCOPABLES, {});
	module.exports = function (key) {
	  ArrayProto[UNSCOPABLES][key] = true;
	};


/***/ }),
/* 188 */
/***/ (function(module, exports, __webpack_require__) {

	// 22.1.3.6 Array.prototype.fill(value, start = 0, end = this.length)
	var $export = __webpack_require__(8);

	$export($export.P, 'Array', { fill: __webpack_require__(189) });

	__webpack_require__(187)('fill');


/***/ }),
/* 189 */
/***/ (function(module, exports, __webpack_require__) {

	// 22.1.3.6 Array.prototype.fill(value, start = 0, end = this.length)
	'use strict';
	var toObject = __webpack_require__(57);
	var toAbsoluteIndex = __webpack_require__(39);
	var toLength = __webpack_require__(37);
	module.exports = function fill(value /* , start = 0, end = @length */) {
	  var O = toObject(this);
	  var length = toLength(O.length);
	  var aLen = arguments.length;
	  var index = toAbsoluteIndex(aLen > 1 ? arguments[1] : undefined, length);
	  var end = aLen > 2 ? arguments[2] : undefined;
	  var endPos = end === undefined ? length : toAbsoluteIndex(end, length);
	  while (endPos > index) O[index++] = value;
	  return O;
	};


/***/ }),
/* 190 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// 22.1.3.8 Array.prototype.find(predicate, thisArg = undefined)
	var $export = __webpack_require__(8);
	var $find = __webpack_require__(173)(5);
	var KEY = 'find';
	var forced = true;
	// Shouldn't skip holes
	if (KEY in []) Array(1)[KEY](function () { forced = false; });
	$export($export.P + $export.F * forced, 'Array', {
	  find: function find(callbackfn /* , that = undefined */) {
	    return $find(this, callbackfn, arguments.length > 1 ? arguments[1] : undefined);
	  }
	});
	__webpack_require__(187)(KEY);


/***/ }),
/* 191 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// 22.1.3.9 Array.prototype.findIndex(predicate, thisArg = undefined)
	var $export = __webpack_require__(8);
	var $find = __webpack_require__(173)(6);
	var KEY = 'findIndex';
	var forced = true;
	// Shouldn't skip holes
	if (KEY in []) Array(1)[KEY](function () { forced = false; });
	$export($export.P + $export.F * forced, 'Array', {
	  findIndex: function findIndex(callbackfn /* , that = undefined */) {
	    return $find(this, callbackfn, arguments.length > 1 ? arguments[1] : undefined);
	  }
	});
	__webpack_require__(187)(KEY);


/***/ }),
/* 192 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(193)('Array');


/***/ }),
/* 193 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var global = __webpack_require__(4);
	var dP = __webpack_require__(11);
	var DESCRIPTORS = __webpack_require__(6);
	var SPECIES = __webpack_require__(26)('species');

	module.exports = function (KEY) {
	  var C = global[KEY];
	  if (DESCRIPTORS && C && !C[SPECIES]) dP.f(C, SPECIES, {
	    configurable: true,
	    get: function () { return this; }
	  });
	};


/***/ }),
/* 194 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var addToUnscopables = __webpack_require__(187);
	var step = __webpack_require__(195);
	var Iterators = __webpack_require__(129);
	var toIObject = __webpack_require__(32);

	// 22.1.3.4 Array.prototype.entries()
	// 22.1.3.13 Array.prototype.keys()
	// 22.1.3.29 Array.prototype.values()
	// 22.1.3.30 Array.prototype[@@iterator]()
	module.exports = __webpack_require__(128)(Array, 'Array', function (iterated, kind) {
	  this._t = toIObject(iterated); // target
	  this._i = 0;                   // next index
	  this._k = kind;                // kind
	// 22.1.5.2.1 %ArrayIteratorPrototype%.next()
	}, function () {
	  var O = this._t;
	  var kind = this._k;
	  var index = this._i++;
	  if (!O || index >= O.length) {
	    this._t = undefined;
	    return step(1);
	  }
	  if (kind == 'keys') return step(0, index);
	  if (kind == 'values') return step(0, O[index]);
	  return step(0, [index, O[index]]);
	}, 'values');

	// argumentsList[@@iterator] is %ArrayProto_values% (9.4.4.6, 9.4.4.7)
	Iterators.Arguments = Iterators.Array;

	addToUnscopables('keys');
	addToUnscopables('values');
	addToUnscopables('entries');


/***/ }),
/* 195 */
/***/ (function(module, exports) {

	module.exports = function (done, value) {
	  return { value: value, done: !!done };
	};


/***/ }),
/* 196 */
/***/ (function(module, exports, __webpack_require__) {

	var global = __webpack_require__(4);
	var inheritIfRequired = __webpack_require__(87);
	var dP = __webpack_require__(11).f;
	var gOPN = __webpack_require__(49).f;
	var isRegExp = __webpack_require__(134);
	var $flags = __webpack_require__(197);
	var $RegExp = global.RegExp;
	var Base = $RegExp;
	var proto = $RegExp.prototype;
	var re1 = /a/g;
	var re2 = /a/g;
	// "new" creates a new object, old webkit buggy here
	var CORRECT_NEW = new $RegExp(re1) !== re1;

	if (__webpack_require__(6) && (!CORRECT_NEW || __webpack_require__(7)(function () {
	  re2[__webpack_require__(26)('match')] = false;
	  // RegExp constructor can alter flags and IsRegExp works correct with @@match
	  return $RegExp(re1) != re1 || $RegExp(re2) == re2 || $RegExp(re1, 'i') != '/a/i';
	}))) {
	  $RegExp = function RegExp(p, f) {
	    var tiRE = this instanceof $RegExp;
	    var piRE = isRegExp(p);
	    var fiU = f === undefined;
	    return !tiRE && piRE && p.constructor === $RegExp && fiU ? p
	      : inheritIfRequired(CORRECT_NEW
	        ? new Base(piRE && !fiU ? p.source : p, f)
	        : Base((piRE = p instanceof $RegExp) ? p.source : p, piRE && fiU ? $flags.call(p) : f)
	      , tiRE ? this : proto, $RegExp);
	  };
	  var proxy = function (key) {
	    key in $RegExp || dP($RegExp, key, {
	      configurable: true,
	      get: function () { return Base[key]; },
	      set: function (it) { Base[key] = it; }
	    });
	  };
	  for (var keys = gOPN(Base), i = 0; keys.length > i;) proxy(keys[i++]);
	  proto.constructor = $RegExp;
	  $RegExp.prototype = proto;
	  __webpack_require__(18)(global, 'RegExp', $RegExp);
	}

	__webpack_require__(193)('RegExp');


/***/ }),
/* 197 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// 21.2.5.3 get RegExp.prototype.flags
	var anObject = __webpack_require__(12);
	module.exports = function () {
	  var that = anObject(this);
	  var result = '';
	  if (that.global) result += 'g';
	  if (that.ignoreCase) result += 'i';
	  if (that.multiline) result += 'm';
	  if (that.unicode) result += 'u';
	  if (that.sticky) result += 'y';
	  return result;
	};


/***/ }),
/* 198 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	__webpack_require__(199);
	var anObject = __webpack_require__(12);
	var $flags = __webpack_require__(197);
	var DESCRIPTORS = __webpack_require__(6);
	var TO_STRING = 'toString';
	var $toString = /./[TO_STRING];

	var define = function (fn) {
	  __webpack_require__(18)(RegExp.prototype, TO_STRING, fn, true);
	};

	// 21.2.5.14 RegExp.prototype.toString()
	if (__webpack_require__(7)(function () { return $toString.call({ source: 'a', flags: 'b' }) != '/a/b'; })) {
	  define(function toString() {
	    var R = anObject(this);
	    return '/'.concat(R.source, '/',
	      'flags' in R ? R.flags : !DESCRIPTORS && R instanceof RegExp ? $flags.call(R) : undefined);
	  });
	// FF44- RegExp#toString has a wrong name
	} else if ($toString.name != TO_STRING) {
	  define(function toString() {
	    return $toString.call(this);
	  });
	}


/***/ }),
/* 199 */
/***/ (function(module, exports, __webpack_require__) {

	// 21.2.5.3 get RegExp.prototype.flags()
	if (__webpack_require__(6) && /./g.flags != 'g') __webpack_require__(11).f(RegExp.prototype, 'flags', {
	  configurable: true,
	  get: __webpack_require__(197)
	});


/***/ }),
/* 200 */
/***/ (function(module, exports, __webpack_require__) {

	// @@match logic
	__webpack_require__(201)('match', 1, function (defined, MATCH, $match) {
	  // 21.1.3.11 String.prototype.match(regexp)
	  return [function match(regexp) {
	    'use strict';
	    var O = defined(this);
	    var fn = regexp == undefined ? undefined : regexp[MATCH];
	    return fn !== undefined ? fn.call(regexp, O) : new RegExp(regexp)[MATCH](String(O));
	  }, $match];
	});


/***/ }),
/* 201 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var hide = __webpack_require__(10);
	var redefine = __webpack_require__(18);
	var fails = __webpack_require__(7);
	var defined = __webpack_require__(35);
	var wks = __webpack_require__(26);

	module.exports = function (KEY, length, exec) {
	  var SYMBOL = wks(KEY);
	  var fns = exec(defined, SYMBOL, ''[KEY]);
	  var strfn = fns[0];
	  var rxfn = fns[1];
	  if (fails(function () {
	    var O = {};
	    O[SYMBOL] = function () { return 7; };
	    return ''[KEY](O) != 7;
	  })) {
	    redefine(String.prototype, KEY, strfn);
	    hide(RegExp.prototype, SYMBOL, length == 2
	      // 21.2.5.8 RegExp.prototype[@@replace](string, replaceValue)
	      // 21.2.5.11 RegExp.prototype[@@split](string, limit)
	      ? function (string, arg) { return rxfn.call(string, this, arg); }
	      // 21.2.5.6 RegExp.prototype[@@match](string)
	      // 21.2.5.9 RegExp.prototype[@@search](string)
	      : function (string) { return rxfn.call(string, this); }
	    );
	  }
	};


/***/ }),
/* 202 */
/***/ (function(module, exports, __webpack_require__) {

	// @@replace logic
	__webpack_require__(201)('replace', 2, function (defined, REPLACE, $replace) {
	  // 21.1.3.14 String.prototype.replace(searchValue, replaceValue)
	  return [function replace(searchValue, replaceValue) {
	    'use strict';
	    var O = defined(this);
	    var fn = searchValue == undefined ? undefined : searchValue[REPLACE];
	    return fn !== undefined
	      ? fn.call(searchValue, O, replaceValue)
	      : $replace.call(String(O), searchValue, replaceValue);
	  }, $replace];
	});


/***/ }),
/* 203 */
/***/ (function(module, exports, __webpack_require__) {

	// @@search logic
	__webpack_require__(201)('search', 1, function (defined, SEARCH, $search) {
	  // 21.1.3.15 String.prototype.search(regexp)
	  return [function search(regexp) {
	    'use strict';
	    var O = defined(this);
	    var fn = regexp == undefined ? undefined : regexp[SEARCH];
	    return fn !== undefined ? fn.call(regexp, O) : new RegExp(regexp)[SEARCH](String(O));
	  }, $search];
	});


/***/ }),
/* 204 */
/***/ (function(module, exports, __webpack_require__) {

	// @@split logic
	__webpack_require__(201)('split', 2, function (defined, SPLIT, $split) {
	  'use strict';
	  var isRegExp = __webpack_require__(134);
	  var _split = $split;
	  var $push = [].push;
	  var $SPLIT = 'split';
	  var LENGTH = 'length';
	  var LAST_INDEX = 'lastIndex';
	  if (
	    'abbc'[$SPLIT](/(b)*/)[1] == 'c' ||
	    'test'[$SPLIT](/(?:)/, -1)[LENGTH] != 4 ||
	    'ab'[$SPLIT](/(?:ab)*/)[LENGTH] != 2 ||
	    '.'[$SPLIT](/(.?)(.?)/)[LENGTH] != 4 ||
	    '.'[$SPLIT](/()()/)[LENGTH] > 1 ||
	    ''[$SPLIT](/.?/)[LENGTH]
	  ) {
	    var NPCG = /()??/.exec('')[1] === undefined; // nonparticipating capturing group
	    // based on es5-shim implementation, need to rework it
	    $split = function (separator, limit) {
	      var string = String(this);
	      if (separator === undefined && limit === 0) return [];
	      // If `separator` is not a regex, use native split
	      if (!isRegExp(separator)) return _split.call(string, separator, limit);
	      var output = [];
	      var flags = (separator.ignoreCase ? 'i' : '') +
	                  (separator.multiline ? 'm' : '') +
	                  (separator.unicode ? 'u' : '') +
	                  (separator.sticky ? 'y' : '');
	      var lastLastIndex = 0;
	      var splitLimit = limit === undefined ? 4294967295 : limit >>> 0;
	      // Make `global` and avoid `lastIndex` issues by working with a copy
	      var separatorCopy = new RegExp(separator.source, flags + 'g');
	      var separator2, match, lastIndex, lastLength, i;
	      // Doesn't need flags gy, but they don't hurt
	      if (!NPCG) separator2 = new RegExp('^' + separatorCopy.source + '$(?!\\s)', flags);
	      while (match = separatorCopy.exec(string)) {
	        // `separatorCopy.lastIndex` is not reliable cross-browser
	        lastIndex = match.index + match[0][LENGTH];
	        if (lastIndex > lastLastIndex) {
	          output.push(string.slice(lastLastIndex, match.index));
	          // Fix browsers whose `exec` methods don't consistently return `undefined` for NPCG
	          // eslint-disable-next-line no-loop-func
	          if (!NPCG && match[LENGTH] > 1) match[0].replace(separator2, function () {
	            for (i = 1; i < arguments[LENGTH] - 2; i++) if (arguments[i] === undefined) match[i] = undefined;
	          });
	          if (match[LENGTH] > 1 && match.index < string[LENGTH]) $push.apply(output, match.slice(1));
	          lastLength = match[0][LENGTH];
	          lastLastIndex = lastIndex;
	          if (output[LENGTH] >= splitLimit) break;
	        }
	        if (separatorCopy[LAST_INDEX] === match.index) separatorCopy[LAST_INDEX]++; // Avoid an infinite loop
	      }
	      if (lastLastIndex === string[LENGTH]) {
	        if (lastLength || !separatorCopy.test('')) output.push('');
	      } else output.push(string.slice(lastLastIndex));
	      return output[LENGTH] > splitLimit ? output.slice(0, splitLimit) : output;
	    };
	  // Chakra, V8
	  } else if ('0'[$SPLIT](undefined, 0)[LENGTH]) {
	    $split = function (separator, limit) {
	      return separator === undefined && limit === 0 ? [] : _split.call(this, separator, limit);
	    };
	  }
	  // 21.1.3.17 String.prototype.split(separator, limit)
	  return [function split(separator, limit) {
	    var O = defined(this);
	    var fn = separator == undefined ? undefined : separator[SPLIT];
	    return fn !== undefined ? fn.call(separator, O, limit) : $split.call(String(O), separator, limit);
	  }, $split];
	});


/***/ }),
/* 205 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var LIBRARY = __webpack_require__(24);
	var global = __webpack_require__(4);
	var ctx = __webpack_require__(20);
	var classof = __webpack_require__(74);
	var $export = __webpack_require__(8);
	var isObject = __webpack_require__(13);
	var aFunction = __webpack_require__(21);
	var anInstance = __webpack_require__(206);
	var forOf = __webpack_require__(207);
	var speciesConstructor = __webpack_require__(208);
	var task = __webpack_require__(209).set;
	var microtask = __webpack_require__(210)();
	var newPromiseCapabilityModule = __webpack_require__(211);
	var perform = __webpack_require__(212);
	var userAgent = __webpack_require__(213);
	var promiseResolve = __webpack_require__(214);
	var PROMISE = 'Promise';
	var TypeError = global.TypeError;
	var process = global.process;
	var versions = process && process.versions;
	var v8 = versions && versions.v8 || '';
	var $Promise = global[PROMISE];
	var isNode = classof(process) == 'process';
	var empty = function () { /* empty */ };
	var Internal, newGenericPromiseCapability, OwnPromiseCapability, Wrapper;
	var newPromiseCapability = newGenericPromiseCapability = newPromiseCapabilityModule.f;

	var USE_NATIVE = !!function () {
	  try {
	    // correct subclassing with @@species support
	    var promise = $Promise.resolve(1);
	    var FakePromise = (promise.constructor = {})[__webpack_require__(26)('species')] = function (exec) {
	      exec(empty, empty);
	    };
	    // unhandled rejections tracking support, NodeJS Promise without it fails @@species test
	    return (isNode || typeof PromiseRejectionEvent == 'function')
	      && promise.then(empty) instanceof FakePromise
	      // v8 6.6 (Node 10 and Chrome 66) have a bug with resolving custom thenables
	      // https://bugs.chromium.org/p/chromium/issues/detail?id=830565
	      // we can't detect it synchronously, so just check versions
	      && v8.indexOf('6.6') !== 0
	      && userAgent.indexOf('Chrome/66') === -1;
	  } catch (e) { /* empty */ }
	}();

	// helpers
	var isThenable = function (it) {
	  var then;
	  return isObject(it) && typeof (then = it.then) == 'function' ? then : false;
	};
	var notify = function (promise, isReject) {
	  if (promise._n) return;
	  promise._n = true;
	  var chain = promise._c;
	  microtask(function () {
	    var value = promise._v;
	    var ok = promise._s == 1;
	    var i = 0;
	    var run = function (reaction) {
	      var handler = ok ? reaction.ok : reaction.fail;
	      var resolve = reaction.resolve;
	      var reject = reaction.reject;
	      var domain = reaction.domain;
	      var result, then, exited;
	      try {
	        if (handler) {
	          if (!ok) {
	            if (promise._h == 2) onHandleUnhandled(promise);
	            promise._h = 1;
	          }
	          if (handler === true) result = value;
	          else {
	            if (domain) domain.enter();
	            result = handler(value); // may throw
	            if (domain) {
	              domain.exit();
	              exited = true;
	            }
	          }
	          if (result === reaction.promise) {
	            reject(TypeError('Promise-chain cycle'));
	          } else if (then = isThenable(result)) {
	            then.call(result, resolve, reject);
	          } else resolve(result);
	        } else reject(value);
	      } catch (e) {
	        if (domain && !exited) domain.exit();
	        reject(e);
	      }
	    };
	    while (chain.length > i) run(chain[i++]); // variable length - can't use forEach
	    promise._c = [];
	    promise._n = false;
	    if (isReject && !promise._h) onUnhandled(promise);
	  });
	};
	var onUnhandled = function (promise) {
	  task.call(global, function () {
	    var value = promise._v;
	    var unhandled = isUnhandled(promise);
	    var result, handler, console;
	    if (unhandled) {
	      result = perform(function () {
	        if (isNode) {
	          process.emit('unhandledRejection', value, promise);
	        } else if (handler = global.onunhandledrejection) {
	          handler({ promise: promise, reason: value });
	        } else if ((console = global.console) && console.error) {
	          console.error('Unhandled promise rejection', value);
	        }
	      });
	      // Browsers should not trigger `rejectionHandled` event if it was handled here, NodeJS - should
	      promise._h = isNode || isUnhandled(promise) ? 2 : 1;
	    } promise._a = undefined;
	    if (unhandled && result.e) throw result.v;
	  });
	};
	var isUnhandled = function (promise) {
	  return promise._h !== 1 && (promise._a || promise._c).length === 0;
	};
	var onHandleUnhandled = function (promise) {
	  task.call(global, function () {
	    var handler;
	    if (isNode) {
	      process.emit('rejectionHandled', promise);
	    } else if (handler = global.onrejectionhandled) {
	      handler({ promise: promise, reason: promise._v });
	    }
	  });
	};
	var $reject = function (value) {
	  var promise = this;
	  if (promise._d) return;
	  promise._d = true;
	  promise = promise._w || promise; // unwrap
	  promise._v = value;
	  promise._s = 2;
	  if (!promise._a) promise._a = promise._c.slice();
	  notify(promise, true);
	};
	var $resolve = function (value) {
	  var promise = this;
	  var then;
	  if (promise._d) return;
	  promise._d = true;
	  promise = promise._w || promise; // unwrap
	  try {
	    if (promise === value) throw TypeError("Promise can't be resolved itself");
	    if (then = isThenable(value)) {
	      microtask(function () {
	        var wrapper = { _w: promise, _d: false }; // wrap
	        try {
	          then.call(value, ctx($resolve, wrapper, 1), ctx($reject, wrapper, 1));
	        } catch (e) {
	          $reject.call(wrapper, e);
	        }
	      });
	    } else {
	      promise._v = value;
	      promise._s = 1;
	      notify(promise, false);
	    }
	  } catch (e) {
	    $reject.call({ _w: promise, _d: false }, e); // wrap
	  }
	};

	// constructor polyfill
	if (!USE_NATIVE) {
	  // 25.4.3.1 Promise(executor)
	  $Promise = function Promise(executor) {
	    anInstance(this, $Promise, PROMISE, '_h');
	    aFunction(executor);
	    Internal.call(this);
	    try {
	      executor(ctx($resolve, this, 1), ctx($reject, this, 1));
	    } catch (err) {
	      $reject.call(this, err);
	    }
	  };
	  // eslint-disable-next-line no-unused-vars
	  Internal = function Promise(executor) {
	    this._c = [];             // <- awaiting reactions
	    this._a = undefined;      // <- checked in isUnhandled reactions
	    this._s = 0;              // <- state
	    this._d = false;          // <- done
	    this._v = undefined;      // <- value
	    this._h = 0;              // <- rejection state, 0 - default, 1 - handled, 2 - unhandled
	    this._n = false;          // <- notify
	  };
	  Internal.prototype = __webpack_require__(215)($Promise.prototype, {
	    // 25.4.5.3 Promise.prototype.then(onFulfilled, onRejected)
	    then: function then(onFulfilled, onRejected) {
	      var reaction = newPromiseCapability(speciesConstructor(this, $Promise));
	      reaction.ok = typeof onFulfilled == 'function' ? onFulfilled : true;
	      reaction.fail = typeof onRejected == 'function' && onRejected;
	      reaction.domain = isNode ? process.domain : undefined;
	      this._c.push(reaction);
	      if (this._a) this._a.push(reaction);
	      if (this._s) notify(this, false);
	      return reaction.promise;
	    },
	    // 25.4.5.1 Promise.prototype.catch(onRejected)
	    'catch': function (onRejected) {
	      return this.then(undefined, onRejected);
	    }
	  });
	  OwnPromiseCapability = function () {
	    var promise = new Internal();
	    this.promise = promise;
	    this.resolve = ctx($resolve, promise, 1);
	    this.reject = ctx($reject, promise, 1);
	  };
	  newPromiseCapabilityModule.f = newPromiseCapability = function (C) {
	    return C === $Promise || C === Wrapper
	      ? new OwnPromiseCapability(C)
	      : newGenericPromiseCapability(C);
	  };
	}

	$export($export.G + $export.W + $export.F * !USE_NATIVE, { Promise: $Promise });
	__webpack_require__(25)($Promise, PROMISE);
	__webpack_require__(193)(PROMISE);
	Wrapper = __webpack_require__(9)[PROMISE];

	// statics
	$export($export.S + $export.F * !USE_NATIVE, PROMISE, {
	  // 25.4.4.5 Promise.reject(r)
	  reject: function reject(r) {
	    var capability = newPromiseCapability(this);
	    var $$reject = capability.reject;
	    $$reject(r);
	    return capability.promise;
	  }
	});
	$export($export.S + $export.F * (LIBRARY || !USE_NATIVE), PROMISE, {
	  // 25.4.4.6 Promise.resolve(x)
	  resolve: function resolve(x) {
	    return promiseResolve(LIBRARY && this === Wrapper ? $Promise : this, x);
	  }
	});
	$export($export.S + $export.F * !(USE_NATIVE && __webpack_require__(166)(function (iter) {
	  $Promise.all(iter)['catch'](empty);
	})), PROMISE, {
	  // 25.4.4.1 Promise.all(iterable)
	  all: function all(iterable) {
	    var C = this;
	    var capability = newPromiseCapability(C);
	    var resolve = capability.resolve;
	    var reject = capability.reject;
	    var result = perform(function () {
	      var values = [];
	      var index = 0;
	      var remaining = 1;
	      forOf(iterable, false, function (promise) {
	        var $index = index++;
	        var alreadyCalled = false;
	        values.push(undefined);
	        remaining++;
	        C.resolve(promise).then(function (value) {
	          if (alreadyCalled) return;
	          alreadyCalled = true;
	          values[$index] = value;
	          --remaining || resolve(values);
	        }, reject);
	      });
	      --remaining || resolve(values);
	    });
	    if (result.e) reject(result.v);
	    return capability.promise;
	  },
	  // 25.4.4.4 Promise.race(iterable)
	  race: function race(iterable) {
	    var C = this;
	    var capability = newPromiseCapability(C);
	    var reject = capability.reject;
	    var result = perform(function () {
	      forOf(iterable, false, function (promise) {
	        C.resolve(promise).then(capability.resolve, reject);
	      });
	    });
	    if (result.e) reject(result.v);
	    return capability.promise;
	  }
	});


/***/ }),
/* 206 */
/***/ (function(module, exports) {

	module.exports = function (it, Constructor, name, forbiddenField) {
	  if (!(it instanceof Constructor) || (forbiddenField !== undefined && forbiddenField in it)) {
	    throw TypeError(name + ': incorrect invocation!');
	  } return it;
	};


/***/ }),
/* 207 */
/***/ (function(module, exports, __webpack_require__) {

	var ctx = __webpack_require__(20);
	var call = __webpack_require__(162);
	var isArrayIter = __webpack_require__(163);
	var anObject = __webpack_require__(12);
	var toLength = __webpack_require__(37);
	var getIterFn = __webpack_require__(165);
	var BREAK = {};
	var RETURN = {};
	var exports = module.exports = function (iterable, entries, fn, that, ITERATOR) {
	  var iterFn = ITERATOR ? function () { return iterable; } : getIterFn(iterable);
	  var f = ctx(fn, that, entries ? 2 : 1);
	  var index = 0;
	  var length, step, iterator, result;
	  if (typeof iterFn != 'function') throw TypeError(iterable + ' is not iterable!');
	  // fast case for arrays with default iterator
	  if (isArrayIter(iterFn)) for (length = toLength(iterable.length); length > index; index++) {
	    result = entries ? f(anObject(step = iterable[index])[0], step[1]) : f(iterable[index]);
	    if (result === BREAK || result === RETURN) return result;
	  } else for (iterator = iterFn.call(iterable); !(step = iterator.next()).done;) {
	    result = call(iterator, f, step.value, entries);
	    if (result === BREAK || result === RETURN) return result;
	  }
	};
	exports.BREAK = BREAK;
	exports.RETURN = RETURN;


/***/ }),
/* 208 */
/***/ (function(module, exports, __webpack_require__) {

	// 7.3.20 SpeciesConstructor(O, defaultConstructor)
	var anObject = __webpack_require__(12);
	var aFunction = __webpack_require__(21);
	var SPECIES = __webpack_require__(26)('species');
	module.exports = function (O, D) {
	  var C = anObject(O).constructor;
	  var S;
	  return C === undefined || (S = anObject(C)[SPECIES]) == undefined ? D : aFunction(S);
	};


/***/ }),
/* 209 */
/***/ (function(module, exports, __webpack_require__) {

	var ctx = __webpack_require__(20);
	var invoke = __webpack_require__(77);
	var html = __webpack_require__(47);
	var cel = __webpack_require__(15);
	var global = __webpack_require__(4);
	var process = global.process;
	var setTask = global.setImmediate;
	var clearTask = global.clearImmediate;
	var MessageChannel = global.MessageChannel;
	var Dispatch = global.Dispatch;
	var counter = 0;
	var queue = {};
	var ONREADYSTATECHANGE = 'onreadystatechange';
	var defer, channel, port;
	var run = function () {
	  var id = +this;
	  // eslint-disable-next-line no-prototype-builtins
	  if (queue.hasOwnProperty(id)) {
	    var fn = queue[id];
	    delete queue[id];
	    fn();
	  }
	};
	var listener = function (event) {
	  run.call(event.data);
	};
	// Node.js 0.9+ & IE10+ has setImmediate, otherwise:
	if (!setTask || !clearTask) {
	  setTask = function setImmediate(fn) {
	    var args = [];
	    var i = 1;
	    while (arguments.length > i) args.push(arguments[i++]);
	    queue[++counter] = function () {
	      // eslint-disable-next-line no-new-func
	      invoke(typeof fn == 'function' ? fn : Function(fn), args);
	    };
	    defer(counter);
	    return counter;
	  };
	  clearTask = function clearImmediate(id) {
	    delete queue[id];
	  };
	  // Node.js 0.8-
	  if (__webpack_require__(34)(process) == 'process') {
	    defer = function (id) {
	      process.nextTick(ctx(run, id, 1));
	    };
	  // Sphere (JS game engine) Dispatch API
	  } else if (Dispatch && Dispatch.now) {
	    defer = function (id) {
	      Dispatch.now(ctx(run, id, 1));
	    };
	  // Browsers with MessageChannel, includes WebWorkers
	  } else if (MessageChannel) {
	    channel = new MessageChannel();
	    port = channel.port2;
	    channel.port1.onmessage = listener;
	    defer = ctx(port.postMessage, port, 1);
	  // Browsers with postMessage, skip WebWorkers
	  // IE8 has postMessage, but it's sync & typeof its postMessage is 'object'
	  } else if (global.addEventListener && typeof postMessage == 'function' && !global.importScripts) {
	    defer = function (id) {
	      global.postMessage(id + '', '*');
	    };
	    global.addEventListener('message', listener, false);
	  // IE8-
	  } else if (ONREADYSTATECHANGE in cel('script')) {
	    defer = function (id) {
	      html.appendChild(cel('script'))[ONREADYSTATECHANGE] = function () {
	        html.removeChild(this);
	        run.call(id);
	      };
	    };
	  // Rest old browsers
	  } else {
	    defer = function (id) {
	      setTimeout(ctx(run, id, 1), 0);
	    };
	  }
	}
	module.exports = {
	  set: setTask,
	  clear: clearTask
	};


/***/ }),
/* 210 */
/***/ (function(module, exports, __webpack_require__) {

	var global = __webpack_require__(4);
	var macrotask = __webpack_require__(209).set;
	var Observer = global.MutationObserver || global.WebKitMutationObserver;
	var process = global.process;
	var Promise = global.Promise;
	var isNode = __webpack_require__(34)(process) == 'process';

	module.exports = function () {
	  var head, last, notify;

	  var flush = function () {
	    var parent, fn;
	    if (isNode && (parent = process.domain)) parent.exit();
	    while (head) {
	      fn = head.fn;
	      head = head.next;
	      try {
	        fn();
	      } catch (e) {
	        if (head) notify();
	        else last = undefined;
	        throw e;
	      }
	    } last = undefined;
	    if (parent) parent.enter();
	  };

	  // Node.js
	  if (isNode) {
	    notify = function () {
	      process.nextTick(flush);
	    };
	  // browsers with MutationObserver, except iOS Safari - https://github.com/zloirock/core-js/issues/339
	  } else if (Observer && !(global.navigator && global.navigator.standalone)) {
	    var toggle = true;
	    var node = document.createTextNode('');
	    new Observer(flush).observe(node, { characterData: true }); // eslint-disable-line no-new
	    notify = function () {
	      node.data = toggle = !toggle;
	    };
	  // environments with maybe non-completely correct, but existent Promise
	  } else if (Promise && Promise.resolve) {
	    // Promise.resolve without an argument throws an error in LG WebOS 2
	    var promise = Promise.resolve(undefined);
	    notify = function () {
	      promise.then(flush);
	    };
	  // for other environments - macrotask based on:
	  // - setImmediate
	  // - MessageChannel
	  // - window.postMessag
	  // - onreadystatechange
	  // - setTimeout
	  } else {
	    notify = function () {
	      // strange IE + webpack dev server bug - use .call(global)
	      macrotask.call(global, flush);
	    };
	  }

	  return function (fn) {
	    var task = { fn: fn, next: undefined };
	    if (last) last.next = task;
	    if (!head) {
	      head = task;
	      notify();
	    } last = task;
	  };
	};


/***/ }),
/* 211 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// 25.4.1.5 NewPromiseCapability(C)
	var aFunction = __webpack_require__(21);

	function PromiseCapability(C) {
	  var resolve, reject;
	  this.promise = new C(function ($$resolve, $$reject) {
	    if (resolve !== undefined || reject !== undefined) throw TypeError('Bad Promise constructor');
	    resolve = $$resolve;
	    reject = $$reject;
	  });
	  this.resolve = aFunction(resolve);
	  this.reject = aFunction(reject);
	}

	module.exports.f = function (C) {
	  return new PromiseCapability(C);
	};


/***/ }),
/* 212 */
/***/ (function(module, exports) {

	module.exports = function (exec) {
	  try {
	    return { e: false, v: exec() };
	  } catch (e) {
	    return { e: true, v: e };
	  }
	};


/***/ }),
/* 213 */
/***/ (function(module, exports, __webpack_require__) {

	var global = __webpack_require__(4);
	var navigator = global.navigator;

	module.exports = navigator && navigator.userAgent || '';


/***/ }),
/* 214 */
/***/ (function(module, exports, __webpack_require__) {

	var anObject = __webpack_require__(12);
	var isObject = __webpack_require__(13);
	var newPromiseCapability = __webpack_require__(211);

	module.exports = function (C, x) {
	  anObject(C);
	  if (isObject(x) && x.constructor === C) return x;
	  var promiseCapability = newPromiseCapability.f(C);
	  var resolve = promiseCapability.resolve;
	  resolve(x);
	  return promiseCapability.promise;
	};


/***/ }),
/* 215 */
/***/ (function(module, exports, __webpack_require__) {

	var redefine = __webpack_require__(18);
	module.exports = function (target, src, safe) {
	  for (var key in src) redefine(target, key, src[key], safe);
	  return target;
	};


/***/ }),
/* 216 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var strong = __webpack_require__(217);
	var validate = __webpack_require__(218);
	var MAP = 'Map';

	// 23.1 Map Objects
	module.exports = __webpack_require__(219)(MAP, function (get) {
	  return function Map() { return get(this, arguments.length > 0 ? arguments[0] : undefined); };
	}, {
	  // 23.1.3.6 Map.prototype.get(key)
	  get: function get(key) {
	    var entry = strong.getEntry(validate(this, MAP), key);
	    return entry && entry.v;
	  },
	  // 23.1.3.9 Map.prototype.set(key, value)
	  set: function set(key, value) {
	    return strong.def(validate(this, MAP), key === 0 ? 0 : key, value);
	  }
	}, strong, true);


/***/ }),
/* 217 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var dP = __webpack_require__(11).f;
	var create = __webpack_require__(45);
	var redefineAll = __webpack_require__(215);
	var ctx = __webpack_require__(20);
	var anInstance = __webpack_require__(206);
	var forOf = __webpack_require__(207);
	var $iterDefine = __webpack_require__(128);
	var step = __webpack_require__(195);
	var setSpecies = __webpack_require__(193);
	var DESCRIPTORS = __webpack_require__(6);
	var fastKey = __webpack_require__(22).fastKey;
	var validate = __webpack_require__(218);
	var SIZE = DESCRIPTORS ? '_s' : 'size';

	var getEntry = function (that, key) {
	  // fast case
	  var index = fastKey(key);
	  var entry;
	  if (index !== 'F') return that._i[index];
	  // frozen object case
	  for (entry = that._f; entry; entry = entry.n) {
	    if (entry.k == key) return entry;
	  }
	};

	module.exports = {
	  getConstructor: function (wrapper, NAME, IS_MAP, ADDER) {
	    var C = wrapper(function (that, iterable) {
	      anInstance(that, C, NAME, '_i');
	      that._t = NAME;         // collection type
	      that._i = create(null); // index
	      that._f = undefined;    // first entry
	      that._l = undefined;    // last entry
	      that[SIZE] = 0;         // size
	      if (iterable != undefined) forOf(iterable, IS_MAP, that[ADDER], that);
	    });
	    redefineAll(C.prototype, {
	      // 23.1.3.1 Map.prototype.clear()
	      // 23.2.3.2 Set.prototype.clear()
	      clear: function clear() {
	        for (var that = validate(this, NAME), data = that._i, entry = that._f; entry; entry = entry.n) {
	          entry.r = true;
	          if (entry.p) entry.p = entry.p.n = undefined;
	          delete data[entry.i];
	        }
	        that._f = that._l = undefined;
	        that[SIZE] = 0;
	      },
	      // 23.1.3.3 Map.prototype.delete(key)
	      // 23.2.3.4 Set.prototype.delete(value)
	      'delete': function (key) {
	        var that = validate(this, NAME);
	        var entry = getEntry(that, key);
	        if (entry) {
	          var next = entry.n;
	          var prev = entry.p;
	          delete that._i[entry.i];
	          entry.r = true;
	          if (prev) prev.n = next;
	          if (next) next.p = prev;
	          if (that._f == entry) that._f = next;
	          if (that._l == entry) that._l = prev;
	          that[SIZE]--;
	        } return !!entry;
	      },
	      // 23.2.3.6 Set.prototype.forEach(callbackfn, thisArg = undefined)
	      // 23.1.3.5 Map.prototype.forEach(callbackfn, thisArg = undefined)
	      forEach: function forEach(callbackfn /* , that = undefined */) {
	        validate(this, NAME);
	        var f = ctx(callbackfn, arguments.length > 1 ? arguments[1] : undefined, 3);
	        var entry;
	        while (entry = entry ? entry.n : this._f) {
	          f(entry.v, entry.k, this);
	          // revert to the last existing entry
	          while (entry && entry.r) entry = entry.p;
	        }
	      },
	      // 23.1.3.7 Map.prototype.has(key)
	      // 23.2.3.7 Set.prototype.has(value)
	      has: function has(key) {
	        return !!getEntry(validate(this, NAME), key);
	      }
	    });
	    if (DESCRIPTORS) dP(C.prototype, 'size', {
	      get: function () {
	        return validate(this, NAME)[SIZE];
	      }
	    });
	    return C;
	  },
	  def: function (that, key, value) {
	    var entry = getEntry(that, key);
	    var prev, index;
	    // change existing entry
	    if (entry) {
	      entry.v = value;
	    // create new entry
	    } else {
	      that._l = entry = {
	        i: index = fastKey(key, true), // <- index
	        k: key,                        // <- key
	        v: value,                      // <- value
	        p: prev = that._l,             // <- previous entry
	        n: undefined,                  // <- next entry
	        r: false                       // <- removed
	      };
	      if (!that._f) that._f = entry;
	      if (prev) prev.n = entry;
	      that[SIZE]++;
	      // add to index
	      if (index !== 'F') that._i[index] = entry;
	    } return that;
	  },
	  getEntry: getEntry,
	  setStrong: function (C, NAME, IS_MAP) {
	    // add .keys, .values, .entries, [@@iterator]
	    // 23.1.3.4, 23.1.3.8, 23.1.3.11, 23.1.3.12, 23.2.3.5, 23.2.3.8, 23.2.3.10, 23.2.3.11
	    $iterDefine(C, NAME, function (iterated, kind) {
	      this._t = validate(iterated, NAME); // target
	      this._k = kind;                     // kind
	      this._l = undefined;                // previous
	    }, function () {
	      var that = this;
	      var kind = that._k;
	      var entry = that._l;
	      // revert to the last existing entry
	      while (entry && entry.r) entry = entry.p;
	      // get next entry
	      if (!that._t || !(that._l = entry = entry ? entry.n : that._t._f)) {
	        // or finish the iteration
	        that._t = undefined;
	        return step(1);
	      }
	      // return step by kind
	      if (kind == 'keys') return step(0, entry.k);
	      if (kind == 'values') return step(0, entry.v);
	      return step(0, [entry.k, entry.v]);
	    }, IS_MAP ? 'entries' : 'values', !IS_MAP, true);

	    // add [@@species], 23.1.2.2, 23.2.2.2
	    setSpecies(NAME);
	  }
	};


/***/ }),
/* 218 */
/***/ (function(module, exports, __webpack_require__) {

	var isObject = __webpack_require__(13);
	module.exports = function (it, TYPE) {
	  if (!isObject(it) || it._t !== TYPE) throw TypeError('Incompatible receiver, ' + TYPE + ' required!');
	  return it;
	};


/***/ }),
/* 219 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var global = __webpack_require__(4);
	var $export = __webpack_require__(8);
	var redefine = __webpack_require__(18);
	var redefineAll = __webpack_require__(215);
	var meta = __webpack_require__(22);
	var forOf = __webpack_require__(207);
	var anInstance = __webpack_require__(206);
	var isObject = __webpack_require__(13);
	var fails = __webpack_require__(7);
	var $iterDetect = __webpack_require__(166);
	var setToStringTag = __webpack_require__(25);
	var inheritIfRequired = __webpack_require__(87);

	module.exports = function (NAME, wrapper, methods, common, IS_MAP, IS_WEAK) {
	  var Base = global[NAME];
	  var C = Base;
	  var ADDER = IS_MAP ? 'set' : 'add';
	  var proto = C && C.prototype;
	  var O = {};
	  var fixMethod = function (KEY) {
	    var fn = proto[KEY];
	    redefine(proto, KEY,
	      KEY == 'delete' ? function (a) {
	        return IS_WEAK && !isObject(a) ? false : fn.call(this, a === 0 ? 0 : a);
	      } : KEY == 'has' ? function has(a) {
	        return IS_WEAK && !isObject(a) ? false : fn.call(this, a === 0 ? 0 : a);
	      } : KEY == 'get' ? function get(a) {
	        return IS_WEAK && !isObject(a) ? undefined : fn.call(this, a === 0 ? 0 : a);
	      } : KEY == 'add' ? function add(a) { fn.call(this, a === 0 ? 0 : a); return this; }
	        : function set(a, b) { fn.call(this, a === 0 ? 0 : a, b); return this; }
	    );
	  };
	  if (typeof C != 'function' || !(IS_WEAK || proto.forEach && !fails(function () {
	    new C().entries().next();
	  }))) {
	    // create collection constructor
	    C = common.getConstructor(wrapper, NAME, IS_MAP, ADDER);
	    redefineAll(C.prototype, methods);
	    meta.NEED = true;
	  } else {
	    var instance = new C();
	    // early implementations not supports chaining
	    var HASNT_CHAINING = instance[ADDER](IS_WEAK ? {} : -0, 1) != instance;
	    // V8 ~  Chromium 40- weak-collections throws on primitives, but should return false
	    var THROWS_ON_PRIMITIVES = fails(function () { instance.has(1); });
	    // most early implementations doesn't supports iterables, most modern - not close it correctly
	    var ACCEPT_ITERABLES = $iterDetect(function (iter) { new C(iter); }); // eslint-disable-line no-new
	    // for early implementations -0 and +0 not the same
	    var BUGGY_ZERO = !IS_WEAK && fails(function () {
	      // V8 ~ Chromium 42- fails only with 5+ elements
	      var $instance = new C();
	      var index = 5;
	      while (index--) $instance[ADDER](index, index);
	      return !$instance.has(-0);
	    });
	    if (!ACCEPT_ITERABLES) {
	      C = wrapper(function (target, iterable) {
	        anInstance(target, C, NAME);
	        var that = inheritIfRequired(new Base(), target, C);
	        if (iterable != undefined) forOf(iterable, IS_MAP, that[ADDER], that);
	        return that;
	      });
	      C.prototype = proto;
	      proto.constructor = C;
	    }
	    if (THROWS_ON_PRIMITIVES || BUGGY_ZERO) {
	      fixMethod('delete');
	      fixMethod('has');
	      IS_MAP && fixMethod('get');
	    }
	    if (BUGGY_ZERO || HASNT_CHAINING) fixMethod(ADDER);
	    // weak collections should not contains .clear method
	    if (IS_WEAK && proto.clear) delete proto.clear;
	  }

	  setToStringTag(C, NAME);

	  O[NAME] = C;
	  $export($export.G + $export.W + $export.F * (C != Base), O);

	  if (!IS_WEAK) common.setStrong(C, NAME, IS_MAP);

	  return C;
	};


/***/ }),
/* 220 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var strong = __webpack_require__(217);
	var validate = __webpack_require__(218);
	var SET = 'Set';

	// 23.2 Set Objects
	module.exports = __webpack_require__(219)(SET, function (get) {
	  return function Set() { return get(this, arguments.length > 0 ? arguments[0] : undefined); };
	}, {
	  // 23.2.3.1 Set.prototype.add(value)
	  add: function add(value) {
	    return strong.def(validate(this, SET), value = value === 0 ? 0 : value, value);
	  }
	}, strong);


/***/ }),
/* 221 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var each = __webpack_require__(173)(0);
	var redefine = __webpack_require__(18);
	var meta = __webpack_require__(22);
	var assign = __webpack_require__(68);
	var weak = __webpack_require__(222);
	var isObject = __webpack_require__(13);
	var fails = __webpack_require__(7);
	var validate = __webpack_require__(218);
	var WEAK_MAP = 'WeakMap';
	var getWeak = meta.getWeak;
	var isExtensible = Object.isExtensible;
	var uncaughtFrozenStore = weak.ufstore;
	var tmp = {};
	var InternalMap;

	var wrapper = function (get) {
	  return function WeakMap() {
	    return get(this, arguments.length > 0 ? arguments[0] : undefined);
	  };
	};

	var methods = {
	  // 23.3.3.3 WeakMap.prototype.get(key)
	  get: function get(key) {
	    if (isObject(key)) {
	      var data = getWeak(key);
	      if (data === true) return uncaughtFrozenStore(validate(this, WEAK_MAP)).get(key);
	      return data ? data[this._i] : undefined;
	    }
	  },
	  // 23.3.3.5 WeakMap.prototype.set(key, value)
	  set: function set(key, value) {
	    return weak.def(validate(this, WEAK_MAP), key, value);
	  }
	};

	// 23.3 WeakMap Objects
	var $WeakMap = module.exports = __webpack_require__(219)(WEAK_MAP, wrapper, methods, weak, true, true);

	// IE11 WeakMap frozen keys fix
	if (fails(function () { return new $WeakMap().set((Object.freeze || Object)(tmp), 7).get(tmp) != 7; })) {
	  InternalMap = weak.getConstructor(wrapper, WEAK_MAP);
	  assign(InternalMap.prototype, methods);
	  meta.NEED = true;
	  each(['delete', 'has', 'get', 'set'], function (key) {
	    var proto = $WeakMap.prototype;
	    var method = proto[key];
	    redefine(proto, key, function (a, b) {
	      // store frozen objects on internal weakmap shim
	      if (isObject(a) && !isExtensible(a)) {
	        if (!this._f) this._f = new InternalMap();
	        var result = this._f[key](a, b);
	        return key == 'set' ? this : result;
	      // store all the rest on native weakmap
	      } return method.call(this, a, b);
	    });
	  });
	}


/***/ }),
/* 222 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var redefineAll = __webpack_require__(215);
	var getWeak = __webpack_require__(22).getWeak;
	var anObject = __webpack_require__(12);
	var isObject = __webpack_require__(13);
	var anInstance = __webpack_require__(206);
	var forOf = __webpack_require__(207);
	var createArrayMethod = __webpack_require__(173);
	var $has = __webpack_require__(5);
	var validate = __webpack_require__(218);
	var arrayFind = createArrayMethod(5);
	var arrayFindIndex = createArrayMethod(6);
	var id = 0;

	// fallback for uncaught frozen keys
	var uncaughtFrozenStore = function (that) {
	  return that._l || (that._l = new UncaughtFrozenStore());
	};
	var UncaughtFrozenStore = function () {
	  this.a = [];
	};
	var findUncaughtFrozen = function (store, key) {
	  return arrayFind(store.a, function (it) {
	    return it[0] === key;
	  });
	};
	UncaughtFrozenStore.prototype = {
	  get: function (key) {
	    var entry = findUncaughtFrozen(this, key);
	    if (entry) return entry[1];
	  },
	  has: function (key) {
	    return !!findUncaughtFrozen(this, key);
	  },
	  set: function (key, value) {
	    var entry = findUncaughtFrozen(this, key);
	    if (entry) entry[1] = value;
	    else this.a.push([key, value]);
	  },
	  'delete': function (key) {
	    var index = arrayFindIndex(this.a, function (it) {
	      return it[0] === key;
	    });
	    if (~index) this.a.splice(index, 1);
	    return !!~index;
	  }
	};

	module.exports = {
	  getConstructor: function (wrapper, NAME, IS_MAP, ADDER) {
	    var C = wrapper(function (that, iterable) {
	      anInstance(that, C, NAME, '_i');
	      that._t = NAME;      // collection type
	      that._i = id++;      // collection id
	      that._l = undefined; // leak store for uncaught frozen objects
	      if (iterable != undefined) forOf(iterable, IS_MAP, that[ADDER], that);
	    });
	    redefineAll(C.prototype, {
	      // 23.3.3.2 WeakMap.prototype.delete(key)
	      // 23.4.3.3 WeakSet.prototype.delete(value)
	      'delete': function (key) {
	        if (!isObject(key)) return false;
	        var data = getWeak(key);
	        if (data === true) return uncaughtFrozenStore(validate(this, NAME))['delete'](key);
	        return data && $has(data, this._i) && delete data[this._i];
	      },
	      // 23.3.3.4 WeakMap.prototype.has(key)
	      // 23.4.3.4 WeakSet.prototype.has(value)
	      has: function has(key) {
	        if (!isObject(key)) return false;
	        var data = getWeak(key);
	        if (data === true) return uncaughtFrozenStore(validate(this, NAME)).has(key);
	        return data && $has(data, this._i);
	      }
	    });
	    return C;
	  },
	  def: function (that, key, value) {
	    var data = getWeak(anObject(key), true);
	    if (data === true) uncaughtFrozenStore(that).set(key, value);
	    else data[that._i] = value;
	    return that;
	  },
	  ufstore: uncaughtFrozenStore
	};


/***/ }),
/* 223 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var weak = __webpack_require__(222);
	var validate = __webpack_require__(218);
	var WEAK_SET = 'WeakSet';

	// 23.4 WeakSet Objects
	__webpack_require__(219)(WEAK_SET, function (get) {
	  return function WeakSet() { return get(this, arguments.length > 0 ? arguments[0] : undefined); };
	}, {
	  // 23.4.3.1 WeakSet.prototype.add(value)
	  add: function add(value) {
	    return weak.def(validate(this, WEAK_SET), value, true);
	  }
	}, weak, false, true);


/***/ }),
/* 224 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var $typed = __webpack_require__(225);
	var buffer = __webpack_require__(226);
	var anObject = __webpack_require__(12);
	var toAbsoluteIndex = __webpack_require__(39);
	var toLength = __webpack_require__(37);
	var isObject = __webpack_require__(13);
	var ArrayBuffer = __webpack_require__(4).ArrayBuffer;
	var speciesConstructor = __webpack_require__(208);
	var $ArrayBuffer = buffer.ArrayBuffer;
	var $DataView = buffer.DataView;
	var $isView = $typed.ABV && ArrayBuffer.isView;
	var $slice = $ArrayBuffer.prototype.slice;
	var VIEW = $typed.VIEW;
	var ARRAY_BUFFER = 'ArrayBuffer';

	$export($export.G + $export.W + $export.F * (ArrayBuffer !== $ArrayBuffer), { ArrayBuffer: $ArrayBuffer });

	$export($export.S + $export.F * !$typed.CONSTR, ARRAY_BUFFER, {
	  // 24.1.3.1 ArrayBuffer.isView(arg)
	  isView: function isView(it) {
	    return $isView && $isView(it) || isObject(it) && VIEW in it;
	  }
	});

	$export($export.P + $export.U + $export.F * __webpack_require__(7)(function () {
	  return !new $ArrayBuffer(2).slice(1, undefined).byteLength;
	}), ARRAY_BUFFER, {
	  // 24.1.4.3 ArrayBuffer.prototype.slice(start, end)
	  slice: function slice(start, end) {
	    if ($slice !== undefined && end === undefined) return $slice.call(anObject(this), start); // FF fix
	    var len = anObject(this).byteLength;
	    var first = toAbsoluteIndex(start, len);
	    var fin = toAbsoluteIndex(end === undefined ? len : end, len);
	    var result = new (speciesConstructor(this, $ArrayBuffer))(toLength(fin - first));
	    var viewS = new $DataView(this);
	    var viewT = new $DataView(result);
	    var index = 0;
	    while (first < fin) {
	      viewT.setUint8(index++, viewS.getUint8(first++));
	    } return result;
	  }
	});

	__webpack_require__(193)(ARRAY_BUFFER);


/***/ }),
/* 225 */
/***/ (function(module, exports, __webpack_require__) {

	var global = __webpack_require__(4);
	var hide = __webpack_require__(10);
	var uid = __webpack_require__(19);
	var TYPED = uid('typed_array');
	var VIEW = uid('view');
	var ABV = !!(global.ArrayBuffer && global.DataView);
	var CONSTR = ABV;
	var i = 0;
	var l = 9;
	var Typed;

	var TypedArrayConstructors = (
	  'Int8Array,Uint8Array,Uint8ClampedArray,Int16Array,Uint16Array,Int32Array,Uint32Array,Float32Array,Float64Array'
	).split(',');

	while (i < l) {
	  if (Typed = global[TypedArrayConstructors[i++]]) {
	    hide(Typed.prototype, TYPED, true);
	    hide(Typed.prototype, VIEW, true);
	  } else CONSTR = false;
	}

	module.exports = {
	  ABV: ABV,
	  CONSTR: CONSTR,
	  TYPED: TYPED,
	  VIEW: VIEW
	};


/***/ }),
/* 226 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var global = __webpack_require__(4);
	var DESCRIPTORS = __webpack_require__(6);
	var LIBRARY = __webpack_require__(24);
	var $typed = __webpack_require__(225);
	var hide = __webpack_require__(10);
	var redefineAll = __webpack_require__(215);
	var fails = __webpack_require__(7);
	var anInstance = __webpack_require__(206);
	var toInteger = __webpack_require__(38);
	var toLength = __webpack_require__(37);
	var toIndex = __webpack_require__(227);
	var gOPN = __webpack_require__(49).f;
	var dP = __webpack_require__(11).f;
	var arrayFill = __webpack_require__(189);
	var setToStringTag = __webpack_require__(25);
	var ARRAY_BUFFER = 'ArrayBuffer';
	var DATA_VIEW = 'DataView';
	var PROTOTYPE = 'prototype';
	var WRONG_LENGTH = 'Wrong length!';
	var WRONG_INDEX = 'Wrong index!';
	var $ArrayBuffer = global[ARRAY_BUFFER];
	var $DataView = global[DATA_VIEW];
	var Math = global.Math;
	var RangeError = global.RangeError;
	// eslint-disable-next-line no-shadow-restricted-names
	var Infinity = global.Infinity;
	var BaseBuffer = $ArrayBuffer;
	var abs = Math.abs;
	var pow = Math.pow;
	var floor = Math.floor;
	var log = Math.log;
	var LN2 = Math.LN2;
	var BUFFER = 'buffer';
	var BYTE_LENGTH = 'byteLength';
	var BYTE_OFFSET = 'byteOffset';
	var $BUFFER = DESCRIPTORS ? '_b' : BUFFER;
	var $LENGTH = DESCRIPTORS ? '_l' : BYTE_LENGTH;
	var $OFFSET = DESCRIPTORS ? '_o' : BYTE_OFFSET;

	// IEEE754 conversions based on https://github.com/feross/ieee754
	function packIEEE754(value, mLen, nBytes) {
	  var buffer = new Array(nBytes);
	  var eLen = nBytes * 8 - mLen - 1;
	  var eMax = (1 << eLen) - 1;
	  var eBias = eMax >> 1;
	  var rt = mLen === 23 ? pow(2, -24) - pow(2, -77) : 0;
	  var i = 0;
	  var s = value < 0 || value === 0 && 1 / value < 0 ? 1 : 0;
	  var e, m, c;
	  value = abs(value);
	  // eslint-disable-next-line no-self-compare
	  if (value != value || value === Infinity) {
	    // eslint-disable-next-line no-self-compare
	    m = value != value ? 1 : 0;
	    e = eMax;
	  } else {
	    e = floor(log(value) / LN2);
	    if (value * (c = pow(2, -e)) < 1) {
	      e--;
	      c *= 2;
	    }
	    if (e + eBias >= 1) {
	      value += rt / c;
	    } else {
	      value += rt * pow(2, 1 - eBias);
	    }
	    if (value * c >= 2) {
	      e++;
	      c /= 2;
	    }
	    if (e + eBias >= eMax) {
	      m = 0;
	      e = eMax;
	    } else if (e + eBias >= 1) {
	      m = (value * c - 1) * pow(2, mLen);
	      e = e + eBias;
	    } else {
	      m = value * pow(2, eBias - 1) * pow(2, mLen);
	      e = 0;
	    }
	  }
	  for (; mLen >= 8; buffer[i++] = m & 255, m /= 256, mLen -= 8);
	  e = e << mLen | m;
	  eLen += mLen;
	  for (; eLen > 0; buffer[i++] = e & 255, e /= 256, eLen -= 8);
	  buffer[--i] |= s * 128;
	  return buffer;
	}
	function unpackIEEE754(buffer, mLen, nBytes) {
	  var eLen = nBytes * 8 - mLen - 1;
	  var eMax = (1 << eLen) - 1;
	  var eBias = eMax >> 1;
	  var nBits = eLen - 7;
	  var i = nBytes - 1;
	  var s = buffer[i--];
	  var e = s & 127;
	  var m;
	  s >>= 7;
	  for (; nBits > 0; e = e * 256 + buffer[i], i--, nBits -= 8);
	  m = e & (1 << -nBits) - 1;
	  e >>= -nBits;
	  nBits += mLen;
	  for (; nBits > 0; m = m * 256 + buffer[i], i--, nBits -= 8);
	  if (e === 0) {
	    e = 1 - eBias;
	  } else if (e === eMax) {
	    return m ? NaN : s ? -Infinity : Infinity;
	  } else {
	    m = m + pow(2, mLen);
	    e = e - eBias;
	  } return (s ? -1 : 1) * m * pow(2, e - mLen);
	}

	function unpackI32(bytes) {
	  return bytes[3] << 24 | bytes[2] << 16 | bytes[1] << 8 | bytes[0];
	}
	function packI8(it) {
	  return [it & 0xff];
	}
	function packI16(it) {
	  return [it & 0xff, it >> 8 & 0xff];
	}
	function packI32(it) {
	  return [it & 0xff, it >> 8 & 0xff, it >> 16 & 0xff, it >> 24 & 0xff];
	}
	function packF64(it) {
	  return packIEEE754(it, 52, 8);
	}
	function packF32(it) {
	  return packIEEE754(it, 23, 4);
	}

	function addGetter(C, key, internal) {
	  dP(C[PROTOTYPE], key, { get: function () { return this[internal]; } });
	}

	function get(view, bytes, index, isLittleEndian) {
	  var numIndex = +index;
	  var intIndex = toIndex(numIndex);
	  if (intIndex + bytes > view[$LENGTH]) throw RangeError(WRONG_INDEX);
	  var store = view[$BUFFER]._b;
	  var start = intIndex + view[$OFFSET];
	  var pack = store.slice(start, start + bytes);
	  return isLittleEndian ? pack : pack.reverse();
	}
	function set(view, bytes, index, conversion, value, isLittleEndian) {
	  var numIndex = +index;
	  var intIndex = toIndex(numIndex);
	  if (intIndex + bytes > view[$LENGTH]) throw RangeError(WRONG_INDEX);
	  var store = view[$BUFFER]._b;
	  var start = intIndex + view[$OFFSET];
	  var pack = conversion(+value);
	  for (var i = 0; i < bytes; i++) store[start + i] = pack[isLittleEndian ? i : bytes - i - 1];
	}

	if (!$typed.ABV) {
	  $ArrayBuffer = function ArrayBuffer(length) {
	    anInstance(this, $ArrayBuffer, ARRAY_BUFFER);
	    var byteLength = toIndex(length);
	    this._b = arrayFill.call(new Array(byteLength), 0);
	    this[$LENGTH] = byteLength;
	  };

	  $DataView = function DataView(buffer, byteOffset, byteLength) {
	    anInstance(this, $DataView, DATA_VIEW);
	    anInstance(buffer, $ArrayBuffer, DATA_VIEW);
	    var bufferLength = buffer[$LENGTH];
	    var offset = toInteger(byteOffset);
	    if (offset < 0 || offset > bufferLength) throw RangeError('Wrong offset!');
	    byteLength = byteLength === undefined ? bufferLength - offset : toLength(byteLength);
	    if (offset + byteLength > bufferLength) throw RangeError(WRONG_LENGTH);
	    this[$BUFFER] = buffer;
	    this[$OFFSET] = offset;
	    this[$LENGTH] = byteLength;
	  };

	  if (DESCRIPTORS) {
	    addGetter($ArrayBuffer, BYTE_LENGTH, '_l');
	    addGetter($DataView, BUFFER, '_b');
	    addGetter($DataView, BYTE_LENGTH, '_l');
	    addGetter($DataView, BYTE_OFFSET, '_o');
	  }

	  redefineAll($DataView[PROTOTYPE], {
	    getInt8: function getInt8(byteOffset) {
	      return get(this, 1, byteOffset)[0] << 24 >> 24;
	    },
	    getUint8: function getUint8(byteOffset) {
	      return get(this, 1, byteOffset)[0];
	    },
	    getInt16: function getInt16(byteOffset /* , littleEndian */) {
	      var bytes = get(this, 2, byteOffset, arguments[1]);
	      return (bytes[1] << 8 | bytes[0]) << 16 >> 16;
	    },
	    getUint16: function getUint16(byteOffset /* , littleEndian */) {
	      var bytes = get(this, 2, byteOffset, arguments[1]);
	      return bytes[1] << 8 | bytes[0];
	    },
	    getInt32: function getInt32(byteOffset /* , littleEndian */) {
	      return unpackI32(get(this, 4, byteOffset, arguments[1]));
	    },
	    getUint32: function getUint32(byteOffset /* , littleEndian */) {
	      return unpackI32(get(this, 4, byteOffset, arguments[1])) >>> 0;
	    },
	    getFloat32: function getFloat32(byteOffset /* , littleEndian */) {
	      return unpackIEEE754(get(this, 4, byteOffset, arguments[1]), 23, 4);
	    },
	    getFloat64: function getFloat64(byteOffset /* , littleEndian */) {
	      return unpackIEEE754(get(this, 8, byteOffset, arguments[1]), 52, 8);
	    },
	    setInt8: function setInt8(byteOffset, value) {
	      set(this, 1, byteOffset, packI8, value);
	    },
	    setUint8: function setUint8(byteOffset, value) {
	      set(this, 1, byteOffset, packI8, value);
	    },
	    setInt16: function setInt16(byteOffset, value /* , littleEndian */) {
	      set(this, 2, byteOffset, packI16, value, arguments[2]);
	    },
	    setUint16: function setUint16(byteOffset, value /* , littleEndian */) {
	      set(this, 2, byteOffset, packI16, value, arguments[2]);
	    },
	    setInt32: function setInt32(byteOffset, value /* , littleEndian */) {
	      set(this, 4, byteOffset, packI32, value, arguments[2]);
	    },
	    setUint32: function setUint32(byteOffset, value /* , littleEndian */) {
	      set(this, 4, byteOffset, packI32, value, arguments[2]);
	    },
	    setFloat32: function setFloat32(byteOffset, value /* , littleEndian */) {
	      set(this, 4, byteOffset, packF32, value, arguments[2]);
	    },
	    setFloat64: function setFloat64(byteOffset, value /* , littleEndian */) {
	      set(this, 8, byteOffset, packF64, value, arguments[2]);
	    }
	  });
	} else {
	  if (!fails(function () {
	    $ArrayBuffer(1);
	  }) || !fails(function () {
	    new $ArrayBuffer(-1); // eslint-disable-line no-new
	  }) || fails(function () {
	    new $ArrayBuffer(); // eslint-disable-line no-new
	    new $ArrayBuffer(1.5); // eslint-disable-line no-new
	    new $ArrayBuffer(NaN); // eslint-disable-line no-new
	    return $ArrayBuffer.name != ARRAY_BUFFER;
	  })) {
	    $ArrayBuffer = function ArrayBuffer(length) {
	      anInstance(this, $ArrayBuffer);
	      return new BaseBuffer(toIndex(length));
	    };
	    var ArrayBufferProto = $ArrayBuffer[PROTOTYPE] = BaseBuffer[PROTOTYPE];
	    for (var keys = gOPN(BaseBuffer), j = 0, key; keys.length > j;) {
	      if (!((key = keys[j++]) in $ArrayBuffer)) hide($ArrayBuffer, key, BaseBuffer[key]);
	    }
	    if (!LIBRARY) ArrayBufferProto.constructor = $ArrayBuffer;
	  }
	  // iOS Safari 7.x bug
	  var view = new $DataView(new $ArrayBuffer(2));
	  var $setInt8 = $DataView[PROTOTYPE].setInt8;
	  view.setInt8(0, 2147483648);
	  view.setInt8(1, 2147483649);
	  if (view.getInt8(0) || !view.getInt8(1)) redefineAll($DataView[PROTOTYPE], {
	    setInt8: function setInt8(byteOffset, value) {
	      $setInt8.call(this, byteOffset, value << 24 >> 24);
	    },
	    setUint8: function setUint8(byteOffset, value) {
	      $setInt8.call(this, byteOffset, value << 24 >> 24);
	    }
	  }, true);
	}
	setToStringTag($ArrayBuffer, ARRAY_BUFFER);
	setToStringTag($DataView, DATA_VIEW);
	hide($DataView[PROTOTYPE], $typed.VIEW, true);
	exports[ARRAY_BUFFER] = $ArrayBuffer;
	exports[DATA_VIEW] = $DataView;


/***/ }),
/* 227 */
/***/ (function(module, exports, __webpack_require__) {

	// https://tc39.github.io/ecma262/#sec-toindex
	var toInteger = __webpack_require__(38);
	var toLength = __webpack_require__(37);
	module.exports = function (it) {
	  if (it === undefined) return 0;
	  var number = toInteger(it);
	  var length = toLength(number);
	  if (number !== length) throw RangeError('Wrong length!');
	  return length;
	};


/***/ }),
/* 228 */
/***/ (function(module, exports, __webpack_require__) {

	var $export = __webpack_require__(8);
	$export($export.G + $export.W + $export.F * !__webpack_require__(225).ABV, {
	  DataView: __webpack_require__(226).DataView
	});


/***/ }),
/* 229 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(230)('Int8', 1, function (init) {
	  return function Int8Array(data, byteOffset, length) {
	    return init(this, data, byteOffset, length);
	  };
	});


/***/ }),
/* 230 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	if (__webpack_require__(6)) {
	  var LIBRARY = __webpack_require__(24);
	  var global = __webpack_require__(4);
	  var fails = __webpack_require__(7);
	  var $export = __webpack_require__(8);
	  var $typed = __webpack_require__(225);
	  var $buffer = __webpack_require__(226);
	  var ctx = __webpack_require__(20);
	  var anInstance = __webpack_require__(206);
	  var propertyDesc = __webpack_require__(17);
	  var hide = __webpack_require__(10);
	  var redefineAll = __webpack_require__(215);
	  var toInteger = __webpack_require__(38);
	  var toLength = __webpack_require__(37);
	  var toIndex = __webpack_require__(227);
	  var toAbsoluteIndex = __webpack_require__(39);
	  var toPrimitive = __webpack_require__(16);
	  var has = __webpack_require__(5);
	  var classof = __webpack_require__(74);
	  var isObject = __webpack_require__(13);
	  var toObject = __webpack_require__(57);
	  var isArrayIter = __webpack_require__(163);
	  var create = __webpack_require__(45);
	  var getPrototypeOf = __webpack_require__(58);
	  var gOPN = __webpack_require__(49).f;
	  var getIterFn = __webpack_require__(165);
	  var uid = __webpack_require__(19);
	  var wks = __webpack_require__(26);
	  var createArrayMethod = __webpack_require__(173);
	  var createArrayIncludes = __webpack_require__(36);
	  var speciesConstructor = __webpack_require__(208);
	  var ArrayIterators = __webpack_require__(194);
	  var Iterators = __webpack_require__(129);
	  var $iterDetect = __webpack_require__(166);
	  var setSpecies = __webpack_require__(193);
	  var arrayFill = __webpack_require__(189);
	  var arrayCopyWithin = __webpack_require__(186);
	  var $DP = __webpack_require__(11);
	  var $GOPD = __webpack_require__(50);
	  var dP = $DP.f;
	  var gOPD = $GOPD.f;
	  var RangeError = global.RangeError;
	  var TypeError = global.TypeError;
	  var Uint8Array = global.Uint8Array;
	  var ARRAY_BUFFER = 'ArrayBuffer';
	  var SHARED_BUFFER = 'Shared' + ARRAY_BUFFER;
	  var BYTES_PER_ELEMENT = 'BYTES_PER_ELEMENT';
	  var PROTOTYPE = 'prototype';
	  var ArrayProto = Array[PROTOTYPE];
	  var $ArrayBuffer = $buffer.ArrayBuffer;
	  var $DataView = $buffer.DataView;
	  var arrayForEach = createArrayMethod(0);
	  var arrayFilter = createArrayMethod(2);
	  var arraySome = createArrayMethod(3);
	  var arrayEvery = createArrayMethod(4);
	  var arrayFind = createArrayMethod(5);
	  var arrayFindIndex = createArrayMethod(6);
	  var arrayIncludes = createArrayIncludes(true);
	  var arrayIndexOf = createArrayIncludes(false);
	  var arrayValues = ArrayIterators.values;
	  var arrayKeys = ArrayIterators.keys;
	  var arrayEntries = ArrayIterators.entries;
	  var arrayLastIndexOf = ArrayProto.lastIndexOf;
	  var arrayReduce = ArrayProto.reduce;
	  var arrayReduceRight = ArrayProto.reduceRight;
	  var arrayJoin = ArrayProto.join;
	  var arraySort = ArrayProto.sort;
	  var arraySlice = ArrayProto.slice;
	  var arrayToString = ArrayProto.toString;
	  var arrayToLocaleString = ArrayProto.toLocaleString;
	  var ITERATOR = wks('iterator');
	  var TAG = wks('toStringTag');
	  var TYPED_CONSTRUCTOR = uid('typed_constructor');
	  var DEF_CONSTRUCTOR = uid('def_constructor');
	  var ALL_CONSTRUCTORS = $typed.CONSTR;
	  var TYPED_ARRAY = $typed.TYPED;
	  var VIEW = $typed.VIEW;
	  var WRONG_LENGTH = 'Wrong length!';

	  var $map = createArrayMethod(1, function (O, length) {
	    return allocate(speciesConstructor(O, O[DEF_CONSTRUCTOR]), length);
	  });

	  var LITTLE_ENDIAN = fails(function () {
	    // eslint-disable-next-line no-undef
	    return new Uint8Array(new Uint16Array([1]).buffer)[0] === 1;
	  });

	  var FORCED_SET = !!Uint8Array && !!Uint8Array[PROTOTYPE].set && fails(function () {
	    new Uint8Array(1).set({});
	  });

	  var toOffset = function (it, BYTES) {
	    var offset = toInteger(it);
	    if (offset < 0 || offset % BYTES) throw RangeError('Wrong offset!');
	    return offset;
	  };

	  var validate = function (it) {
	    if (isObject(it) && TYPED_ARRAY in it) return it;
	    throw TypeError(it + ' is not a typed array!');
	  };

	  var allocate = function (C, length) {
	    if (!(isObject(C) && TYPED_CONSTRUCTOR in C)) {
	      throw TypeError('It is not a typed array constructor!');
	    } return new C(length);
	  };

	  var speciesFromList = function (O, list) {
	    return fromList(speciesConstructor(O, O[DEF_CONSTRUCTOR]), list);
	  };

	  var fromList = function (C, list) {
	    var index = 0;
	    var length = list.length;
	    var result = allocate(C, length);
	    while (length > index) result[index] = list[index++];
	    return result;
	  };

	  var addGetter = function (it, key, internal) {
	    dP(it, key, { get: function () { return this._d[internal]; } });
	  };

	  var $from = function from(source /* , mapfn, thisArg */) {
	    var O = toObject(source);
	    var aLen = arguments.length;
	    var mapfn = aLen > 1 ? arguments[1] : undefined;
	    var mapping = mapfn !== undefined;
	    var iterFn = getIterFn(O);
	    var i, length, values, result, step, iterator;
	    if (iterFn != undefined && !isArrayIter(iterFn)) {
	      for (iterator = iterFn.call(O), values = [], i = 0; !(step = iterator.next()).done; i++) {
	        values.push(step.value);
	      } O = values;
	    }
	    if (mapping && aLen > 2) mapfn = ctx(mapfn, arguments[2], 2);
	    for (i = 0, length = toLength(O.length), result = allocate(this, length); length > i; i++) {
	      result[i] = mapping ? mapfn(O[i], i) : O[i];
	    }
	    return result;
	  };

	  var $of = function of(/* ...items */) {
	    var index = 0;
	    var length = arguments.length;
	    var result = allocate(this, length);
	    while (length > index) result[index] = arguments[index++];
	    return result;
	  };

	  // iOS Safari 6.x fails here
	  var TO_LOCALE_BUG = !!Uint8Array && fails(function () { arrayToLocaleString.call(new Uint8Array(1)); });

	  var $toLocaleString = function toLocaleString() {
	    return arrayToLocaleString.apply(TO_LOCALE_BUG ? arraySlice.call(validate(this)) : validate(this), arguments);
	  };

	  var proto = {
	    copyWithin: function copyWithin(target, start /* , end */) {
	      return arrayCopyWithin.call(validate(this), target, start, arguments.length > 2 ? arguments[2] : undefined);
	    },
	    every: function every(callbackfn /* , thisArg */) {
	      return arrayEvery(validate(this), callbackfn, arguments.length > 1 ? arguments[1] : undefined);
	    },
	    fill: function fill(value /* , start, end */) { // eslint-disable-line no-unused-vars
	      return arrayFill.apply(validate(this), arguments);
	    },
	    filter: function filter(callbackfn /* , thisArg */) {
	      return speciesFromList(this, arrayFilter(validate(this), callbackfn,
	        arguments.length > 1 ? arguments[1] : undefined));
	    },
	    find: function find(predicate /* , thisArg */) {
	      return arrayFind(validate(this), predicate, arguments.length > 1 ? arguments[1] : undefined);
	    },
	    findIndex: function findIndex(predicate /* , thisArg */) {
	      return arrayFindIndex(validate(this), predicate, arguments.length > 1 ? arguments[1] : undefined);
	    },
	    forEach: function forEach(callbackfn /* , thisArg */) {
	      arrayForEach(validate(this), callbackfn, arguments.length > 1 ? arguments[1] : undefined);
	    },
	    indexOf: function indexOf(searchElement /* , fromIndex */) {
	      return arrayIndexOf(validate(this), searchElement, arguments.length > 1 ? arguments[1] : undefined);
	    },
	    includes: function includes(searchElement /* , fromIndex */) {
	      return arrayIncludes(validate(this), searchElement, arguments.length > 1 ? arguments[1] : undefined);
	    },
	    join: function join(separator) { // eslint-disable-line no-unused-vars
	      return arrayJoin.apply(validate(this), arguments);
	    },
	    lastIndexOf: function lastIndexOf(searchElement /* , fromIndex */) { // eslint-disable-line no-unused-vars
	      return arrayLastIndexOf.apply(validate(this), arguments);
	    },
	    map: function map(mapfn /* , thisArg */) {
	      return $map(validate(this), mapfn, arguments.length > 1 ? arguments[1] : undefined);
	    },
	    reduce: function reduce(callbackfn /* , initialValue */) { // eslint-disable-line no-unused-vars
	      return arrayReduce.apply(validate(this), arguments);
	    },
	    reduceRight: function reduceRight(callbackfn /* , initialValue */) { // eslint-disable-line no-unused-vars
	      return arrayReduceRight.apply(validate(this), arguments);
	    },
	    reverse: function reverse() {
	      var that = this;
	      var length = validate(that).length;
	      var middle = Math.floor(length / 2);
	      var index = 0;
	      var value;
	      while (index < middle) {
	        value = that[index];
	        that[index++] = that[--length];
	        that[length] = value;
	      } return that;
	    },
	    some: function some(callbackfn /* , thisArg */) {
	      return arraySome(validate(this), callbackfn, arguments.length > 1 ? arguments[1] : undefined);
	    },
	    sort: function sort(comparefn) {
	      return arraySort.call(validate(this), comparefn);
	    },
	    subarray: function subarray(begin, end) {
	      var O = validate(this);
	      var length = O.length;
	      var $begin = toAbsoluteIndex(begin, length);
	      return new (speciesConstructor(O, O[DEF_CONSTRUCTOR]))(
	        O.buffer,
	        O.byteOffset + $begin * O.BYTES_PER_ELEMENT,
	        toLength((end === undefined ? length : toAbsoluteIndex(end, length)) - $begin)
	      );
	    }
	  };

	  var $slice = function slice(start, end) {
	    return speciesFromList(this, arraySlice.call(validate(this), start, end));
	  };

	  var $set = function set(arrayLike /* , offset */) {
	    validate(this);
	    var offset = toOffset(arguments[1], 1);
	    var length = this.length;
	    var src = toObject(arrayLike);
	    var len = toLength(src.length);
	    var index = 0;
	    if (len + offset > length) throw RangeError(WRONG_LENGTH);
	    while (index < len) this[offset + index] = src[index++];
	  };

	  var $iterators = {
	    entries: function entries() {
	      return arrayEntries.call(validate(this));
	    },
	    keys: function keys() {
	      return arrayKeys.call(validate(this));
	    },
	    values: function values() {
	      return arrayValues.call(validate(this));
	    }
	  };

	  var isTAIndex = function (target, key) {
	    return isObject(target)
	      && target[TYPED_ARRAY]
	      && typeof key != 'symbol'
	      && key in target
	      && String(+key) == String(key);
	  };
	  var $getDesc = function getOwnPropertyDescriptor(target, key) {
	    return isTAIndex(target, key = toPrimitive(key, true))
	      ? propertyDesc(2, target[key])
	      : gOPD(target, key);
	  };
	  var $setDesc = function defineProperty(target, key, desc) {
	    if (isTAIndex(target, key = toPrimitive(key, true))
	      && isObject(desc)
	      && has(desc, 'value')
	      && !has(desc, 'get')
	      && !has(desc, 'set')
	      // TODO: add validation descriptor w/o calling accessors
	      && !desc.configurable
	      && (!has(desc, 'writable') || desc.writable)
	      && (!has(desc, 'enumerable') || desc.enumerable)
	    ) {
	      target[key] = desc.value;
	      return target;
	    } return dP(target, key, desc);
	  };

	  if (!ALL_CONSTRUCTORS) {
	    $GOPD.f = $getDesc;
	    $DP.f = $setDesc;
	  }

	  $export($export.S + $export.F * !ALL_CONSTRUCTORS, 'Object', {
	    getOwnPropertyDescriptor: $getDesc,
	    defineProperty: $setDesc
	  });

	  if (fails(function () { arrayToString.call({}); })) {
	    arrayToString = arrayToLocaleString = function toString() {
	      return arrayJoin.call(this);
	    };
	  }

	  var $TypedArrayPrototype$ = redefineAll({}, proto);
	  redefineAll($TypedArrayPrototype$, $iterators);
	  hide($TypedArrayPrototype$, ITERATOR, $iterators.values);
	  redefineAll($TypedArrayPrototype$, {
	    slice: $slice,
	    set: $set,
	    constructor: function () { /* noop */ },
	    toString: arrayToString,
	    toLocaleString: $toLocaleString
	  });
	  addGetter($TypedArrayPrototype$, 'buffer', 'b');
	  addGetter($TypedArrayPrototype$, 'byteOffset', 'o');
	  addGetter($TypedArrayPrototype$, 'byteLength', 'l');
	  addGetter($TypedArrayPrototype$, 'length', 'e');
	  dP($TypedArrayPrototype$, TAG, {
	    get: function () { return this[TYPED_ARRAY]; }
	  });

	  // eslint-disable-next-line max-statements
	  module.exports = function (KEY, BYTES, wrapper, CLAMPED) {
	    CLAMPED = !!CLAMPED;
	    var NAME = KEY + (CLAMPED ? 'Clamped' : '') + 'Array';
	    var GETTER = 'get' + KEY;
	    var SETTER = 'set' + KEY;
	    var TypedArray = global[NAME];
	    var Base = TypedArray || {};
	    var TAC = TypedArray && getPrototypeOf(TypedArray);
	    var FORCED = !TypedArray || !$typed.ABV;
	    var O = {};
	    var TypedArrayPrototype = TypedArray && TypedArray[PROTOTYPE];
	    var getter = function (that, index) {
	      var data = that._d;
	      return data.v[GETTER](index * BYTES + data.o, LITTLE_ENDIAN);
	    };
	    var setter = function (that, index, value) {
	      var data = that._d;
	      if (CLAMPED) value = (value = Math.round(value)) < 0 ? 0 : value > 0xff ? 0xff : value & 0xff;
	      data.v[SETTER](index * BYTES + data.o, value, LITTLE_ENDIAN);
	    };
	    var addElement = function (that, index) {
	      dP(that, index, {
	        get: function () {
	          return getter(this, index);
	        },
	        set: function (value) {
	          return setter(this, index, value);
	        },
	        enumerable: true
	      });
	    };
	    if (FORCED) {
	      TypedArray = wrapper(function (that, data, $offset, $length) {
	        anInstance(that, TypedArray, NAME, '_d');
	        var index = 0;
	        var offset = 0;
	        var buffer, byteLength, length, klass;
	        if (!isObject(data)) {
	          length = toIndex(data);
	          byteLength = length * BYTES;
	          buffer = new $ArrayBuffer(byteLength);
	        } else if (data instanceof $ArrayBuffer || (klass = classof(data)) == ARRAY_BUFFER || klass == SHARED_BUFFER) {
	          buffer = data;
	          offset = toOffset($offset, BYTES);
	          var $len = data.byteLength;
	          if ($length === undefined) {
	            if ($len % BYTES) throw RangeError(WRONG_LENGTH);
	            byteLength = $len - offset;
	            if (byteLength < 0) throw RangeError(WRONG_LENGTH);
	          } else {
	            byteLength = toLength($length) * BYTES;
	            if (byteLength + offset > $len) throw RangeError(WRONG_LENGTH);
	          }
	          length = byteLength / BYTES;
	        } else if (TYPED_ARRAY in data) {
	          return fromList(TypedArray, data);
	        } else {
	          return $from.call(TypedArray, data);
	        }
	        hide(that, '_d', {
	          b: buffer,
	          o: offset,
	          l: byteLength,
	          e: length,
	          v: new $DataView(buffer)
	        });
	        while (index < length) addElement(that, index++);
	      });
	      TypedArrayPrototype = TypedArray[PROTOTYPE] = create($TypedArrayPrototype$);
	      hide(TypedArrayPrototype, 'constructor', TypedArray);
	    } else if (!fails(function () {
	      TypedArray(1);
	    }) || !fails(function () {
	      new TypedArray(-1); // eslint-disable-line no-new
	    }) || !$iterDetect(function (iter) {
	      new TypedArray(); // eslint-disable-line no-new
	      new TypedArray(null); // eslint-disable-line no-new
	      new TypedArray(1.5); // eslint-disable-line no-new
	      new TypedArray(iter); // eslint-disable-line no-new
	    }, true)) {
	      TypedArray = wrapper(function (that, data, $offset, $length) {
	        anInstance(that, TypedArray, NAME);
	        var klass;
	        // `ws` module bug, temporarily remove validation length for Uint8Array
	        // https://github.com/websockets/ws/pull/645
	        if (!isObject(data)) return new Base(toIndex(data));
	        if (data instanceof $ArrayBuffer || (klass = classof(data)) == ARRAY_BUFFER || klass == SHARED_BUFFER) {
	          return $length !== undefined
	            ? new Base(data, toOffset($offset, BYTES), $length)
	            : $offset !== undefined
	              ? new Base(data, toOffset($offset, BYTES))
	              : new Base(data);
	        }
	        if (TYPED_ARRAY in data) return fromList(TypedArray, data);
	        return $from.call(TypedArray, data);
	      });
	      arrayForEach(TAC !== Function.prototype ? gOPN(Base).concat(gOPN(TAC)) : gOPN(Base), function (key) {
	        if (!(key in TypedArray)) hide(TypedArray, key, Base[key]);
	      });
	      TypedArray[PROTOTYPE] = TypedArrayPrototype;
	      if (!LIBRARY) TypedArrayPrototype.constructor = TypedArray;
	    }
	    var $nativeIterator = TypedArrayPrototype[ITERATOR];
	    var CORRECT_ITER_NAME = !!$nativeIterator
	      && ($nativeIterator.name == 'values' || $nativeIterator.name == undefined);
	    var $iterator = $iterators.values;
	    hide(TypedArray, TYPED_CONSTRUCTOR, true);
	    hide(TypedArrayPrototype, TYPED_ARRAY, NAME);
	    hide(TypedArrayPrototype, VIEW, true);
	    hide(TypedArrayPrototype, DEF_CONSTRUCTOR, TypedArray);

	    if (CLAMPED ? new TypedArray(1)[TAG] != NAME : !(TAG in TypedArrayPrototype)) {
	      dP(TypedArrayPrototype, TAG, {
	        get: function () { return NAME; }
	      });
	    }

	    O[NAME] = TypedArray;

	    $export($export.G + $export.W + $export.F * (TypedArray != Base), O);

	    $export($export.S, NAME, {
	      BYTES_PER_ELEMENT: BYTES
	    });

	    $export($export.S + $export.F * fails(function () { Base.of.call(TypedArray, 1); }), NAME, {
	      from: $from,
	      of: $of
	    });

	    if (!(BYTES_PER_ELEMENT in TypedArrayPrototype)) hide(TypedArrayPrototype, BYTES_PER_ELEMENT, BYTES);

	    $export($export.P, NAME, proto);

	    setSpecies(NAME);

	    $export($export.P + $export.F * FORCED_SET, NAME, { set: $set });

	    $export($export.P + $export.F * !CORRECT_ITER_NAME, NAME, $iterators);

	    if (!LIBRARY && TypedArrayPrototype.toString != arrayToString) TypedArrayPrototype.toString = arrayToString;

	    $export($export.P + $export.F * fails(function () {
	      new TypedArray(1).slice();
	    }), NAME, { slice: $slice });

	    $export($export.P + $export.F * (fails(function () {
	      return [1, 2].toLocaleString() != new TypedArray([1, 2]).toLocaleString();
	    }) || !fails(function () {
	      TypedArrayPrototype.toLocaleString.call([1, 2]);
	    })), NAME, { toLocaleString: $toLocaleString });

	    Iterators[NAME] = CORRECT_ITER_NAME ? $nativeIterator : $iterator;
	    if (!LIBRARY && !CORRECT_ITER_NAME) hide(TypedArrayPrototype, ITERATOR, $iterator);
	  };
	} else module.exports = function () { /* empty */ };


/***/ }),
/* 231 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(230)('Uint8', 1, function (init) {
	  return function Uint8Array(data, byteOffset, length) {
	    return init(this, data, byteOffset, length);
	  };
	});


/***/ }),
/* 232 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(230)('Uint8', 1, function (init) {
	  return function Uint8ClampedArray(data, byteOffset, length) {
	    return init(this, data, byteOffset, length);
	  };
	}, true);


/***/ }),
/* 233 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(230)('Int16', 2, function (init) {
	  return function Int16Array(data, byteOffset, length) {
	    return init(this, data, byteOffset, length);
	  };
	});


/***/ }),
/* 234 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(230)('Uint16', 2, function (init) {
	  return function Uint16Array(data, byteOffset, length) {
	    return init(this, data, byteOffset, length);
	  };
	});


/***/ }),
/* 235 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(230)('Int32', 4, function (init) {
	  return function Int32Array(data, byteOffset, length) {
	    return init(this, data, byteOffset, length);
	  };
	});


/***/ }),
/* 236 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(230)('Uint32', 4, function (init) {
	  return function Uint32Array(data, byteOffset, length) {
	    return init(this, data, byteOffset, length);
	  };
	});


/***/ }),
/* 237 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(230)('Float32', 4, function (init) {
	  return function Float32Array(data, byteOffset, length) {
	    return init(this, data, byteOffset, length);
	  };
	});


/***/ }),
/* 238 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(230)('Float64', 8, function (init) {
	  return function Float64Array(data, byteOffset, length) {
	    return init(this, data, byteOffset, length);
	  };
	});


/***/ }),
/* 239 */
/***/ (function(module, exports, __webpack_require__) {

	// 26.1.1 Reflect.apply(target, thisArgument, argumentsList)
	var $export = __webpack_require__(8);
	var aFunction = __webpack_require__(21);
	var anObject = __webpack_require__(12);
	var rApply = (__webpack_require__(4).Reflect || {}).apply;
	var fApply = Function.apply;
	// MS Edge argumentsList argument is optional
	$export($export.S + $export.F * !__webpack_require__(7)(function () {
	  rApply(function () { /* empty */ });
	}), 'Reflect', {
	  apply: function apply(target, thisArgument, argumentsList) {
	    var T = aFunction(target);
	    var L = anObject(argumentsList);
	    return rApply ? rApply(T, thisArgument, L) : fApply.call(T, thisArgument, L);
	  }
	});


/***/ }),
/* 240 */
/***/ (function(module, exports, __webpack_require__) {

	// 26.1.2 Reflect.construct(target, argumentsList [, newTarget])
	var $export = __webpack_require__(8);
	var create = __webpack_require__(45);
	var aFunction = __webpack_require__(21);
	var anObject = __webpack_require__(12);
	var isObject = __webpack_require__(13);
	var fails = __webpack_require__(7);
	var bind = __webpack_require__(76);
	var rConstruct = (__webpack_require__(4).Reflect || {}).construct;

	// MS Edge supports only 2 arguments and argumentsList argument is optional
	// FF Nightly sets third argument as `new.target`, but does not create `this` from it
	var NEW_TARGET_BUG = fails(function () {
	  function F() { /* empty */ }
	  return !(rConstruct(function () { /* empty */ }, [], F) instanceof F);
	});
	var ARGS_BUG = !fails(function () {
	  rConstruct(function () { /* empty */ });
	});

	$export($export.S + $export.F * (NEW_TARGET_BUG || ARGS_BUG), 'Reflect', {
	  construct: function construct(Target, args /* , newTarget */) {
	    aFunction(Target);
	    anObject(args);
	    var newTarget = arguments.length < 3 ? Target : aFunction(arguments[2]);
	    if (ARGS_BUG && !NEW_TARGET_BUG) return rConstruct(Target, args, newTarget);
	    if (Target == newTarget) {
	      // w/o altered newTarget, optimization for 0-4 arguments
	      switch (args.length) {
	        case 0: return new Target();
	        case 1: return new Target(args[0]);
	        case 2: return new Target(args[0], args[1]);
	        case 3: return new Target(args[0], args[1], args[2]);
	        case 4: return new Target(args[0], args[1], args[2], args[3]);
	      }
	      // w/o altered newTarget, lot of arguments case
	      var $args = [null];
	      $args.push.apply($args, args);
	      return new (bind.apply(Target, $args))();
	    }
	    // with altered newTarget, not support built-in constructors
	    var proto = newTarget.prototype;
	    var instance = create(isObject(proto) ? proto : Object.prototype);
	    var result = Function.apply.call(Target, instance, args);
	    return isObject(result) ? result : instance;
	  }
	});


/***/ }),
/* 241 */
/***/ (function(module, exports, __webpack_require__) {

	// 26.1.3 Reflect.defineProperty(target, propertyKey, attributes)
	var dP = __webpack_require__(11);
	var $export = __webpack_require__(8);
	var anObject = __webpack_require__(12);
	var toPrimitive = __webpack_require__(16);

	// MS Edge has broken Reflect.defineProperty - throwing instead of returning false
	$export($export.S + $export.F * __webpack_require__(7)(function () {
	  // eslint-disable-next-line no-undef
	  Reflect.defineProperty(dP.f({}, 1, { value: 1 }), 1, { value: 2 });
	}), 'Reflect', {
	  defineProperty: function defineProperty(target, propertyKey, attributes) {
	    anObject(target);
	    propertyKey = toPrimitive(propertyKey, true);
	    anObject(attributes);
	    try {
	      dP.f(target, propertyKey, attributes);
	      return true;
	    } catch (e) {
	      return false;
	    }
	  }
	});


/***/ }),
/* 242 */
/***/ (function(module, exports, __webpack_require__) {

	// 26.1.4 Reflect.deleteProperty(target, propertyKey)
	var $export = __webpack_require__(8);
	var gOPD = __webpack_require__(50).f;
	var anObject = __webpack_require__(12);

	$export($export.S, 'Reflect', {
	  deleteProperty: function deleteProperty(target, propertyKey) {
	    var desc = gOPD(anObject(target), propertyKey);
	    return desc && !desc.configurable ? false : delete target[propertyKey];
	  }
	});


/***/ }),
/* 243 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// 26.1.5 Reflect.enumerate(target)
	var $export = __webpack_require__(8);
	var anObject = __webpack_require__(12);
	var Enumerate = function (iterated) {
	  this._t = anObject(iterated); // target
	  this._i = 0;                  // next index
	  var keys = this._k = [];      // keys
	  var key;
	  for (key in iterated) keys.push(key);
	};
	__webpack_require__(130)(Enumerate, 'Object', function () {
	  var that = this;
	  var keys = that._k;
	  var key;
	  do {
	    if (that._i >= keys.length) return { value: undefined, done: true };
	  } while (!((key = keys[that._i++]) in that._t));
	  return { value: key, done: false };
	});

	$export($export.S, 'Reflect', {
	  enumerate: function enumerate(target) {
	    return new Enumerate(target);
	  }
	});


/***/ }),
/* 244 */
/***/ (function(module, exports, __webpack_require__) {

	// 26.1.6 Reflect.get(target, propertyKey [, receiver])
	var gOPD = __webpack_require__(50);
	var getPrototypeOf = __webpack_require__(58);
	var has = __webpack_require__(5);
	var $export = __webpack_require__(8);
	var isObject = __webpack_require__(13);
	var anObject = __webpack_require__(12);

	function get(target, propertyKey /* , receiver */) {
	  var receiver = arguments.length < 3 ? target : arguments[2];
	  var desc, proto;
	  if (anObject(target) === receiver) return target[propertyKey];
	  if (desc = gOPD.f(target, propertyKey)) return has(desc, 'value')
	    ? desc.value
	    : desc.get !== undefined
	      ? desc.get.call(receiver)
	      : undefined;
	  if (isObject(proto = getPrototypeOf(target))) return get(proto, propertyKey, receiver);
	}

	$export($export.S, 'Reflect', { get: get });


/***/ }),
/* 245 */
/***/ (function(module, exports, __webpack_require__) {

	// 26.1.7 Reflect.getOwnPropertyDescriptor(target, propertyKey)
	var gOPD = __webpack_require__(50);
	var $export = __webpack_require__(8);
	var anObject = __webpack_require__(12);

	$export($export.S, 'Reflect', {
	  getOwnPropertyDescriptor: function getOwnPropertyDescriptor(target, propertyKey) {
	    return gOPD.f(anObject(target), propertyKey);
	  }
	});


/***/ }),
/* 246 */
/***/ (function(module, exports, __webpack_require__) {

	// 26.1.8 Reflect.getPrototypeOf(target)
	var $export = __webpack_require__(8);
	var getProto = __webpack_require__(58);
	var anObject = __webpack_require__(12);

	$export($export.S, 'Reflect', {
	  getPrototypeOf: function getPrototypeOf(target) {
	    return getProto(anObject(target));
	  }
	});


/***/ }),
/* 247 */
/***/ (function(module, exports, __webpack_require__) {

	// 26.1.9 Reflect.has(target, propertyKey)
	var $export = __webpack_require__(8);

	$export($export.S, 'Reflect', {
	  has: function has(target, propertyKey) {
	    return propertyKey in target;
	  }
	});


/***/ }),
/* 248 */
/***/ (function(module, exports, __webpack_require__) {

	// 26.1.10 Reflect.isExtensible(target)
	var $export = __webpack_require__(8);
	var anObject = __webpack_require__(12);
	var $isExtensible = Object.isExtensible;

	$export($export.S, 'Reflect', {
	  isExtensible: function isExtensible(target) {
	    anObject(target);
	    return $isExtensible ? $isExtensible(target) : true;
	  }
	});


/***/ }),
/* 249 */
/***/ (function(module, exports, __webpack_require__) {

	// 26.1.11 Reflect.ownKeys(target)
	var $export = __webpack_require__(8);

	$export($export.S, 'Reflect', { ownKeys: __webpack_require__(250) });


/***/ }),
/* 250 */
/***/ (function(module, exports, __webpack_require__) {

	// all object keys, includes non-enumerable and symbols
	var gOPN = __webpack_require__(49);
	var gOPS = __webpack_require__(42);
	var anObject = __webpack_require__(12);
	var Reflect = __webpack_require__(4).Reflect;
	module.exports = Reflect && Reflect.ownKeys || function ownKeys(it) {
	  var keys = gOPN.f(anObject(it));
	  var getSymbols = gOPS.f;
	  return getSymbols ? keys.concat(getSymbols(it)) : keys;
	};


/***/ }),
/* 251 */
/***/ (function(module, exports, __webpack_require__) {

	// 26.1.12 Reflect.preventExtensions(target)
	var $export = __webpack_require__(8);
	var anObject = __webpack_require__(12);
	var $preventExtensions = Object.preventExtensions;

	$export($export.S, 'Reflect', {
	  preventExtensions: function preventExtensions(target) {
	    anObject(target);
	    try {
	      if ($preventExtensions) $preventExtensions(target);
	      return true;
	    } catch (e) {
	      return false;
	    }
	  }
	});


/***/ }),
/* 252 */
/***/ (function(module, exports, __webpack_require__) {

	// 26.1.13 Reflect.set(target, propertyKey, V [, receiver])
	var dP = __webpack_require__(11);
	var gOPD = __webpack_require__(50);
	var getPrototypeOf = __webpack_require__(58);
	var has = __webpack_require__(5);
	var $export = __webpack_require__(8);
	var createDesc = __webpack_require__(17);
	var anObject = __webpack_require__(12);
	var isObject = __webpack_require__(13);

	function set(target, propertyKey, V /* , receiver */) {
	  var receiver = arguments.length < 4 ? target : arguments[3];
	  var ownDesc = gOPD.f(anObject(target), propertyKey);
	  var existingDescriptor, proto;
	  if (!ownDesc) {
	    if (isObject(proto = getPrototypeOf(target))) {
	      return set(proto, propertyKey, V, receiver);
	    }
	    ownDesc = createDesc(0);
	  }
	  if (has(ownDesc, 'value')) {
	    if (ownDesc.writable === false || !isObject(receiver)) return false;
	    if (existingDescriptor = gOPD.f(receiver, propertyKey)) {
	      if (existingDescriptor.get || existingDescriptor.set || existingDescriptor.writable === false) return false;
	      existingDescriptor.value = V;
	      dP.f(receiver, propertyKey, existingDescriptor);
	    } else dP.f(receiver, propertyKey, createDesc(0, V));
	    return true;
	  }
	  return ownDesc.set === undefined ? false : (ownDesc.set.call(receiver, V), true);
	}

	$export($export.S, 'Reflect', { set: set });


/***/ }),
/* 253 */
/***/ (function(module, exports, __webpack_require__) {

	// 26.1.14 Reflect.setPrototypeOf(target, proto)
	var $export = __webpack_require__(8);
	var setProto = __webpack_require__(72);

	if (setProto) $export($export.S, 'Reflect', {
	  setPrototypeOf: function setPrototypeOf(target, proto) {
	    setProto.check(target, proto);
	    try {
	      setProto.set(target, proto);
	      return true;
	    } catch (e) {
	      return false;
	    }
	  }
	});


/***/ }),
/* 254 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// https://github.com/tc39/Array.prototype.includes
	var $export = __webpack_require__(8);
	var $includes = __webpack_require__(36)(true);

	$export($export.P, 'Array', {
	  includes: function includes(el /* , fromIndex = 0 */) {
	    return $includes(this, el, arguments.length > 1 ? arguments[1] : undefined);
	  }
	});

	__webpack_require__(187)('includes');


/***/ }),
/* 255 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// https://tc39.github.io/proposal-flatMap/#sec-Array.prototype.flatMap
	var $export = __webpack_require__(8);
	var flattenIntoArray = __webpack_require__(256);
	var toObject = __webpack_require__(57);
	var toLength = __webpack_require__(37);
	var aFunction = __webpack_require__(21);
	var arraySpeciesCreate = __webpack_require__(174);

	$export($export.P, 'Array', {
	  flatMap: function flatMap(callbackfn /* , thisArg */) {
	    var O = toObject(this);
	    var sourceLen, A;
	    aFunction(callbackfn);
	    sourceLen = toLength(O.length);
	    A = arraySpeciesCreate(O, 0);
	    flattenIntoArray(A, O, O, sourceLen, 0, 1, callbackfn, arguments[1]);
	    return A;
	  }
	});

	__webpack_require__(187)('flatMap');


/***/ }),
/* 256 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// https://tc39.github.io/proposal-flatMap/#sec-FlattenIntoArray
	var isArray = __webpack_require__(44);
	var isObject = __webpack_require__(13);
	var toLength = __webpack_require__(37);
	var ctx = __webpack_require__(20);
	var IS_CONCAT_SPREADABLE = __webpack_require__(26)('isConcatSpreadable');

	function flattenIntoArray(target, original, source, sourceLen, start, depth, mapper, thisArg) {
	  var targetIndex = start;
	  var sourceIndex = 0;
	  var mapFn = mapper ? ctx(mapper, thisArg, 3) : false;
	  var element, spreadable;

	  while (sourceIndex < sourceLen) {
	    if (sourceIndex in source) {
	      element = mapFn ? mapFn(source[sourceIndex], sourceIndex, original) : source[sourceIndex];

	      spreadable = false;
	      if (isObject(element)) {
	        spreadable = element[IS_CONCAT_SPREADABLE];
	        spreadable = spreadable !== undefined ? !!spreadable : isArray(element);
	      }

	      if (spreadable && depth > 0) {
	        targetIndex = flattenIntoArray(target, original, element, toLength(element.length), targetIndex, depth - 1) - 1;
	      } else {
	        if (targetIndex >= 0x1fffffffffffff) throw TypeError();
	        target[targetIndex] = element;
	      }

	      targetIndex++;
	    }
	    sourceIndex++;
	  }
	  return targetIndex;
	}

	module.exports = flattenIntoArray;


/***/ }),
/* 257 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// https://tc39.github.io/proposal-flatMap/#sec-Array.prototype.flatten
	var $export = __webpack_require__(8);
	var flattenIntoArray = __webpack_require__(256);
	var toObject = __webpack_require__(57);
	var toLength = __webpack_require__(37);
	var toInteger = __webpack_require__(38);
	var arraySpeciesCreate = __webpack_require__(174);

	$export($export.P, 'Array', {
	  flatten: function flatten(/* depthArg = 1 */) {
	    var depthArg = arguments[0];
	    var O = toObject(this);
	    var sourceLen = toLength(O.length);
	    var A = arraySpeciesCreate(O, 0);
	    flattenIntoArray(A, O, O, sourceLen, 0, depthArg === undefined ? 1 : toInteger(depthArg));
	    return A;
	  }
	});

	__webpack_require__(187)('flatten');


/***/ }),
/* 258 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// https://github.com/mathiasbynens/String.prototype.at
	var $export = __webpack_require__(8);
	var $at = __webpack_require__(127)(true);

	$export($export.P, 'String', {
	  at: function at(pos) {
	    return $at(this, pos);
	  }
	});


/***/ }),
/* 259 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// https://github.com/tc39/proposal-string-pad-start-end
	var $export = __webpack_require__(8);
	var $pad = __webpack_require__(260);
	var userAgent = __webpack_require__(213);

	// https://github.com/zloirock/core-js/issues/280
	$export($export.P + $export.F * /Version\/10\.\d+(\.\d+)? Safari\//.test(userAgent), 'String', {
	  padStart: function padStart(maxLength /* , fillString = ' ' */) {
	    return $pad(this, maxLength, arguments.length > 1 ? arguments[1] : undefined, true);
	  }
	});


/***/ }),
/* 260 */
/***/ (function(module, exports, __webpack_require__) {

	// https://github.com/tc39/proposal-string-pad-start-end
	var toLength = __webpack_require__(37);
	var repeat = __webpack_require__(90);
	var defined = __webpack_require__(35);

	module.exports = function (that, maxLength, fillString, left) {
	  var S = String(defined(that));
	  var stringLength = S.length;
	  var fillStr = fillString === undefined ? ' ' : String(fillString);
	  var intMaxLength = toLength(maxLength);
	  if (intMaxLength <= stringLength || fillStr == '') return S;
	  var fillLen = intMaxLength - stringLength;
	  var stringFiller = repeat.call(fillStr, Math.ceil(fillLen / fillStr.length));
	  if (stringFiller.length > fillLen) stringFiller = stringFiller.slice(0, fillLen);
	  return left ? stringFiller + S : S + stringFiller;
	};


/***/ }),
/* 261 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// https://github.com/tc39/proposal-string-pad-start-end
	var $export = __webpack_require__(8);
	var $pad = __webpack_require__(260);
	var userAgent = __webpack_require__(213);

	// https://github.com/zloirock/core-js/issues/280
	$export($export.P + $export.F * /Version\/10\.\d+(\.\d+)? Safari\//.test(userAgent), 'String', {
	  padEnd: function padEnd(maxLength /* , fillString = ' ' */) {
	    return $pad(this, maxLength, arguments.length > 1 ? arguments[1] : undefined, false);
	  }
	});


/***/ }),
/* 262 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// https://github.com/sebmarkbage/ecmascript-string-left-right-trim
	__webpack_require__(82)('trimLeft', function ($trim) {
	  return function trimLeft() {
	    return $trim(this, 1);
	  };
	}, 'trimStart');


/***/ }),
/* 263 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// https://github.com/sebmarkbage/ecmascript-string-left-right-trim
	__webpack_require__(82)('trimRight', function ($trim) {
	  return function trimRight() {
	    return $trim(this, 2);
	  };
	}, 'trimEnd');


/***/ }),
/* 264 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// https://tc39.github.io/String.prototype.matchAll/
	var $export = __webpack_require__(8);
	var defined = __webpack_require__(35);
	var toLength = __webpack_require__(37);
	var isRegExp = __webpack_require__(134);
	var getFlags = __webpack_require__(197);
	var RegExpProto = RegExp.prototype;

	var $RegExpStringIterator = function (regexp, string) {
	  this._r = regexp;
	  this._s = string;
	};

	__webpack_require__(130)($RegExpStringIterator, 'RegExp String', function next() {
	  var match = this._r.exec(this._s);
	  return { value: match, done: match === null };
	});

	$export($export.P, 'String', {
	  matchAll: function matchAll(regexp) {
	    defined(this);
	    if (!isRegExp(regexp)) throw TypeError(regexp + ' is not a regexp!');
	    var S = String(this);
	    var flags = 'flags' in RegExpProto ? String(regexp.flags) : getFlags.call(regexp);
	    var rx = new RegExp(regexp.source, ~flags.indexOf('g') ? flags : 'g' + flags);
	    rx.lastIndex = toLength(regexp.lastIndex);
	    return new $RegExpStringIterator(rx, S);
	  }
	});


/***/ }),
/* 265 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(28)('asyncIterator');


/***/ }),
/* 266 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(28)('observable');


/***/ }),
/* 267 */
/***/ (function(module, exports, __webpack_require__) {

	// https://github.com/tc39/proposal-object-getownpropertydescriptors
	var $export = __webpack_require__(8);
	var ownKeys = __webpack_require__(250);
	var toIObject = __webpack_require__(32);
	var gOPD = __webpack_require__(50);
	var createProperty = __webpack_require__(164);

	$export($export.S, 'Object', {
	  getOwnPropertyDescriptors: function getOwnPropertyDescriptors(object) {
	    var O = toIObject(object);
	    var getDesc = gOPD.f;
	    var keys = ownKeys(O);
	    var result = {};
	    var i = 0;
	    var key, desc;
	    while (keys.length > i) {
	      desc = getDesc(O, key = keys[i++]);
	      if (desc !== undefined) createProperty(result, key, desc);
	    }
	    return result;
	  }
	});


/***/ }),
/* 268 */
/***/ (function(module, exports, __webpack_require__) {

	// https://github.com/tc39/proposal-object-values-entries
	var $export = __webpack_require__(8);
	var $values = __webpack_require__(269)(false);

	$export($export.S, 'Object', {
	  values: function values(it) {
	    return $values(it);
	  }
	});


/***/ }),
/* 269 */
/***/ (function(module, exports, __webpack_require__) {

	var getKeys = __webpack_require__(30);
	var toIObject = __webpack_require__(32);
	var isEnum = __webpack_require__(43).f;
	module.exports = function (isEntries) {
	  return function (it) {
	    var O = toIObject(it);
	    var keys = getKeys(O);
	    var length = keys.length;
	    var i = 0;
	    var result = [];
	    var key;
	    while (length > i) if (isEnum.call(O, key = keys[i++])) {
	      result.push(isEntries ? [key, O[key]] : O[key]);
	    } return result;
	  };
	};


/***/ }),
/* 270 */
/***/ (function(module, exports, __webpack_require__) {

	// https://github.com/tc39/proposal-object-values-entries
	var $export = __webpack_require__(8);
	var $entries = __webpack_require__(269)(true);

	$export($export.S, 'Object', {
	  entries: function entries(it) {
	    return $entries(it);
	  }
	});


/***/ }),
/* 271 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var toObject = __webpack_require__(57);
	var aFunction = __webpack_require__(21);
	var $defineProperty = __webpack_require__(11);

	// B.2.2.2 Object.prototype.__defineGetter__(P, getter)
	__webpack_require__(6) && $export($export.P + __webpack_require__(272), 'Object', {
	  __defineGetter__: function __defineGetter__(P, getter) {
	    $defineProperty.f(toObject(this), P, { get: aFunction(getter), enumerable: true, configurable: true });
	  }
	});


/***/ }),
/* 272 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// Forced replacement prototype accessors methods
	module.exports = __webpack_require__(24) || !__webpack_require__(7)(function () {
	  var K = Math.random();
	  // In FF throws only define methods
	  // eslint-disable-next-line no-undef, no-useless-call
	  __defineSetter__.call(null, K, function () { /* empty */ });
	  delete __webpack_require__(4)[K];
	});


/***/ }),
/* 273 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var toObject = __webpack_require__(57);
	var aFunction = __webpack_require__(21);
	var $defineProperty = __webpack_require__(11);

	// B.2.2.3 Object.prototype.__defineSetter__(P, setter)
	__webpack_require__(6) && $export($export.P + __webpack_require__(272), 'Object', {
	  __defineSetter__: function __defineSetter__(P, setter) {
	    $defineProperty.f(toObject(this), P, { set: aFunction(setter), enumerable: true, configurable: true });
	  }
	});


/***/ }),
/* 274 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var toObject = __webpack_require__(57);
	var toPrimitive = __webpack_require__(16);
	var getPrototypeOf = __webpack_require__(58);
	var getOwnPropertyDescriptor = __webpack_require__(50).f;

	// B.2.2.4 Object.prototype.__lookupGetter__(P)
	__webpack_require__(6) && $export($export.P + __webpack_require__(272), 'Object', {
	  __lookupGetter__: function __lookupGetter__(P) {
	    var O = toObject(this);
	    var K = toPrimitive(P, true);
	    var D;
	    do {
	      if (D = getOwnPropertyDescriptor(O, K)) return D.get;
	    } while (O = getPrototypeOf(O));
	  }
	});


/***/ }),
/* 275 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	var $export = __webpack_require__(8);
	var toObject = __webpack_require__(57);
	var toPrimitive = __webpack_require__(16);
	var getPrototypeOf = __webpack_require__(58);
	var getOwnPropertyDescriptor = __webpack_require__(50).f;

	// B.2.2.5 Object.prototype.__lookupSetter__(P)
	__webpack_require__(6) && $export($export.P + __webpack_require__(272), 'Object', {
	  __lookupSetter__: function __lookupSetter__(P) {
	    var O = toObject(this);
	    var K = toPrimitive(P, true);
	    var D;
	    do {
	      if (D = getOwnPropertyDescriptor(O, K)) return D.set;
	    } while (O = getPrototypeOf(O));
	  }
	});


/***/ }),
/* 276 */
/***/ (function(module, exports, __webpack_require__) {

	// https://github.com/DavidBruant/Map-Set.prototype.toJSON
	var $export = __webpack_require__(8);

	$export($export.P + $export.R, 'Map', { toJSON: __webpack_require__(277)('Map') });


/***/ }),
/* 277 */
/***/ (function(module, exports, __webpack_require__) {

	// https://github.com/DavidBruant/Map-Set.prototype.toJSON
	var classof = __webpack_require__(74);
	var from = __webpack_require__(278);
	module.exports = function (NAME) {
	  return function toJSON() {
	    if (classof(this) != NAME) throw TypeError(NAME + "#toJSON isn't generic");
	    return from(this);
	  };
	};


/***/ }),
/* 278 */
/***/ (function(module, exports, __webpack_require__) {

	var forOf = __webpack_require__(207);

	module.exports = function (iter, ITERATOR) {
	  var result = [];
	  forOf(iter, false, result.push, result, ITERATOR);
	  return result;
	};


/***/ }),
/* 279 */
/***/ (function(module, exports, __webpack_require__) {

	// https://github.com/DavidBruant/Map-Set.prototype.toJSON
	var $export = __webpack_require__(8);

	$export($export.P + $export.R, 'Set', { toJSON: __webpack_require__(277)('Set') });


/***/ }),
/* 280 */
/***/ (function(module, exports, __webpack_require__) {

	// https://tc39.github.io/proposal-setmap-offrom/#sec-map.of
	__webpack_require__(281)('Map');


/***/ }),
/* 281 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// https://tc39.github.io/proposal-setmap-offrom/
	var $export = __webpack_require__(8);

	module.exports = function (COLLECTION) {
	  $export($export.S, COLLECTION, { of: function of() {
	    var length = arguments.length;
	    var A = new Array(length);
	    while (length--) A[length] = arguments[length];
	    return new this(A);
	  } });
	};


/***/ }),
/* 282 */
/***/ (function(module, exports, __webpack_require__) {

	// https://tc39.github.io/proposal-setmap-offrom/#sec-set.of
	__webpack_require__(281)('Set');


/***/ }),
/* 283 */
/***/ (function(module, exports, __webpack_require__) {

	// https://tc39.github.io/proposal-setmap-offrom/#sec-weakmap.of
	__webpack_require__(281)('WeakMap');


/***/ }),
/* 284 */
/***/ (function(module, exports, __webpack_require__) {

	// https://tc39.github.io/proposal-setmap-offrom/#sec-weakset.of
	__webpack_require__(281)('WeakSet');


/***/ }),
/* 285 */
/***/ (function(module, exports, __webpack_require__) {

	// https://tc39.github.io/proposal-setmap-offrom/#sec-map.from
	__webpack_require__(286)('Map');


/***/ }),
/* 286 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// https://tc39.github.io/proposal-setmap-offrom/
	var $export = __webpack_require__(8);
	var aFunction = __webpack_require__(21);
	var ctx = __webpack_require__(20);
	var forOf = __webpack_require__(207);

	module.exports = function (COLLECTION) {
	  $export($export.S, COLLECTION, { from: function from(source /* , mapFn, thisArg */) {
	    var mapFn = arguments[1];
	    var mapping, A, n, cb;
	    aFunction(this);
	    mapping = mapFn !== undefined;
	    if (mapping) aFunction(mapFn);
	    if (source == undefined) return new this();
	    A = [];
	    if (mapping) {
	      n = 0;
	      cb = ctx(mapFn, arguments[2], 2);
	      forOf(source, false, function (nextItem) {
	        A.push(cb(nextItem, n++));
	      });
	    } else {
	      forOf(source, false, A.push, A);
	    }
	    return new this(A);
	  } });
	};


/***/ }),
/* 287 */
/***/ (function(module, exports, __webpack_require__) {

	// https://tc39.github.io/proposal-setmap-offrom/#sec-set.from
	__webpack_require__(286)('Set');


/***/ }),
/* 288 */
/***/ (function(module, exports, __webpack_require__) {

	// https://tc39.github.io/proposal-setmap-offrom/#sec-weakmap.from
	__webpack_require__(286)('WeakMap');


/***/ }),
/* 289 */
/***/ (function(module, exports, __webpack_require__) {

	// https://tc39.github.io/proposal-setmap-offrom/#sec-weakset.from
	__webpack_require__(286)('WeakSet');


/***/ }),
/* 290 */
/***/ (function(module, exports, __webpack_require__) {

	// https://github.com/tc39/proposal-global
	var $export = __webpack_require__(8);

	$export($export.G, { global: __webpack_require__(4) });


/***/ }),
/* 291 */
/***/ (function(module, exports, __webpack_require__) {

	// https://github.com/tc39/proposal-global
	var $export = __webpack_require__(8);

	$export($export.S, 'System', { global: __webpack_require__(4) });


/***/ }),
/* 292 */
/***/ (function(module, exports, __webpack_require__) {

	// https://github.com/ljharb/proposal-is-error
	var $export = __webpack_require__(8);
	var cof = __webpack_require__(34);

	$export($export.S, 'Error', {
	  isError: function isError(it) {
	    return cof(it) === 'Error';
	  }
	});


/***/ }),
/* 293 */
/***/ (function(module, exports, __webpack_require__) {

	// https://rwaldron.github.io/proposal-math-extensions/
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', {
	  clamp: function clamp(x, lower, upper) {
	    return Math.min(upper, Math.max(lower, x));
	  }
	});


/***/ }),
/* 294 */
/***/ (function(module, exports, __webpack_require__) {

	// https://rwaldron.github.io/proposal-math-extensions/
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', { DEG_PER_RAD: Math.PI / 180 });


/***/ }),
/* 295 */
/***/ (function(module, exports, __webpack_require__) {

	// https://rwaldron.github.io/proposal-math-extensions/
	var $export = __webpack_require__(8);
	var RAD_PER_DEG = 180 / Math.PI;

	$export($export.S, 'Math', {
	  degrees: function degrees(radians) {
	    return radians * RAD_PER_DEG;
	  }
	});


/***/ }),
/* 296 */
/***/ (function(module, exports, __webpack_require__) {

	// https://rwaldron.github.io/proposal-math-extensions/
	var $export = __webpack_require__(8);
	var scale = __webpack_require__(297);
	var fround = __webpack_require__(113);

	$export($export.S, 'Math', {
	  fscale: function fscale(x, inLow, inHigh, outLow, outHigh) {
	    return fround(scale(x, inLow, inHigh, outLow, outHigh));
	  }
	});


/***/ }),
/* 297 */
/***/ (function(module, exports) {

	// https://rwaldron.github.io/proposal-math-extensions/
	module.exports = Math.scale || function scale(x, inLow, inHigh, outLow, outHigh) {
	  if (
	    arguments.length === 0
	      // eslint-disable-next-line no-self-compare
	      || x != x
	      // eslint-disable-next-line no-self-compare
	      || inLow != inLow
	      // eslint-disable-next-line no-self-compare
	      || inHigh != inHigh
	      // eslint-disable-next-line no-self-compare
	      || outLow != outLow
	      // eslint-disable-next-line no-self-compare
	      || outHigh != outHigh
	  ) return NaN;
	  if (x === Infinity || x === -Infinity) return x;
	  return (x - inLow) * (outHigh - outLow) / (inHigh - inLow) + outLow;
	};


/***/ }),
/* 298 */
/***/ (function(module, exports, __webpack_require__) {

	// https://gist.github.com/BrendanEich/4294d5c212a6d2254703
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', {
	  iaddh: function iaddh(x0, x1, y0, y1) {
	    var $x0 = x0 >>> 0;
	    var $x1 = x1 >>> 0;
	    var $y0 = y0 >>> 0;
	    return $x1 + (y1 >>> 0) + (($x0 & $y0 | ($x0 | $y0) & ~($x0 + $y0 >>> 0)) >>> 31) | 0;
	  }
	});


/***/ }),
/* 299 */
/***/ (function(module, exports, __webpack_require__) {

	// https://gist.github.com/BrendanEich/4294d5c212a6d2254703
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', {
	  isubh: function isubh(x0, x1, y0, y1) {
	    var $x0 = x0 >>> 0;
	    var $x1 = x1 >>> 0;
	    var $y0 = y0 >>> 0;
	    return $x1 - (y1 >>> 0) - ((~$x0 & $y0 | ~($x0 ^ $y0) & $x0 - $y0 >>> 0) >>> 31) | 0;
	  }
	});


/***/ }),
/* 300 */
/***/ (function(module, exports, __webpack_require__) {

	// https://gist.github.com/BrendanEich/4294d5c212a6d2254703
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', {
	  imulh: function imulh(u, v) {
	    var UINT16 = 0xffff;
	    var $u = +u;
	    var $v = +v;
	    var u0 = $u & UINT16;
	    var v0 = $v & UINT16;
	    var u1 = $u >> 16;
	    var v1 = $v >> 16;
	    var t = (u1 * v0 >>> 0) + (u0 * v0 >>> 16);
	    return u1 * v1 + (t >> 16) + ((u0 * v1 >>> 0) + (t & UINT16) >> 16);
	  }
	});


/***/ }),
/* 301 */
/***/ (function(module, exports, __webpack_require__) {

	// https://rwaldron.github.io/proposal-math-extensions/
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', { RAD_PER_DEG: 180 / Math.PI });


/***/ }),
/* 302 */
/***/ (function(module, exports, __webpack_require__) {

	// https://rwaldron.github.io/proposal-math-extensions/
	var $export = __webpack_require__(8);
	var DEG_PER_RAD = Math.PI / 180;

	$export($export.S, 'Math', {
	  radians: function radians(degrees) {
	    return degrees * DEG_PER_RAD;
	  }
	});


/***/ }),
/* 303 */
/***/ (function(module, exports, __webpack_require__) {

	// https://rwaldron.github.io/proposal-math-extensions/
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', { scale: __webpack_require__(297) });


/***/ }),
/* 304 */
/***/ (function(module, exports, __webpack_require__) {

	// https://gist.github.com/BrendanEich/4294d5c212a6d2254703
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', {
	  umulh: function umulh(u, v) {
	    var UINT16 = 0xffff;
	    var $u = +u;
	    var $v = +v;
	    var u0 = $u & UINT16;
	    var v0 = $v & UINT16;
	    var u1 = $u >>> 16;
	    var v1 = $v >>> 16;
	    var t = (u1 * v0 >>> 0) + (u0 * v0 >>> 16);
	    return u1 * v1 + (t >>> 16) + ((u0 * v1 >>> 0) + (t & UINT16) >>> 16);
	  }
	});


/***/ }),
/* 305 */
/***/ (function(module, exports, __webpack_require__) {

	// http://jfbastien.github.io/papers/Math.signbit.html
	var $export = __webpack_require__(8);

	$export($export.S, 'Math', { signbit: function signbit(x) {
	  // eslint-disable-next-line no-self-compare
	  return (x = +x) != x ? x : x == 0 ? 1 / x == Infinity : x > 0;
	} });


/***/ }),
/* 306 */
/***/ (function(module, exports, __webpack_require__) {

	// https://github.com/tc39/proposal-promise-finally
	'use strict';
	var $export = __webpack_require__(8);
	var core = __webpack_require__(9);
	var global = __webpack_require__(4);
	var speciesConstructor = __webpack_require__(208);
	var promiseResolve = __webpack_require__(214);

	$export($export.P + $export.R, 'Promise', { 'finally': function (onFinally) {
	  var C = speciesConstructor(this, core.Promise || global.Promise);
	  var isFunction = typeof onFinally == 'function';
	  return this.then(
	    isFunction ? function (x) {
	      return promiseResolve(C, onFinally()).then(function () { return x; });
	    } : onFinally,
	    isFunction ? function (e) {
	      return promiseResolve(C, onFinally()).then(function () { throw e; });
	    } : onFinally
	  );
	} });


/***/ }),
/* 307 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// https://github.com/tc39/proposal-promise-try
	var $export = __webpack_require__(8);
	var newPromiseCapability = __webpack_require__(211);
	var perform = __webpack_require__(212);

	$export($export.S, 'Promise', { 'try': function (callbackfn) {
	  var promiseCapability = newPromiseCapability.f(this);
	  var result = perform(callbackfn);
	  (result.e ? promiseCapability.reject : promiseCapability.resolve)(result.v);
	  return promiseCapability.promise;
	} });


/***/ }),
/* 308 */
/***/ (function(module, exports, __webpack_require__) {

	var metadata = __webpack_require__(309);
	var anObject = __webpack_require__(12);
	var toMetaKey = metadata.key;
	var ordinaryDefineOwnMetadata = metadata.set;

	metadata.exp({ defineMetadata: function defineMetadata(metadataKey, metadataValue, target, targetKey) {
	  ordinaryDefineOwnMetadata(metadataKey, metadataValue, anObject(target), toMetaKey(targetKey));
	} });


/***/ }),
/* 309 */
/***/ (function(module, exports, __webpack_require__) {

	var Map = __webpack_require__(216);
	var $export = __webpack_require__(8);
	var shared = __webpack_require__(23)('metadata');
	var store = shared.store || (shared.store = new (__webpack_require__(221))());

	var getOrCreateMetadataMap = function (target, targetKey, create) {
	  var targetMetadata = store.get(target);
	  if (!targetMetadata) {
	    if (!create) return undefined;
	    store.set(target, targetMetadata = new Map());
	  }
	  var keyMetadata = targetMetadata.get(targetKey);
	  if (!keyMetadata) {
	    if (!create) return undefined;
	    targetMetadata.set(targetKey, keyMetadata = new Map());
	  } return keyMetadata;
	};
	var ordinaryHasOwnMetadata = function (MetadataKey, O, P) {
	  var metadataMap = getOrCreateMetadataMap(O, P, false);
	  return metadataMap === undefined ? false : metadataMap.has(MetadataKey);
	};
	var ordinaryGetOwnMetadata = function (MetadataKey, O, P) {
	  var metadataMap = getOrCreateMetadataMap(O, P, false);
	  return metadataMap === undefined ? undefined : metadataMap.get(MetadataKey);
	};
	var ordinaryDefineOwnMetadata = function (MetadataKey, MetadataValue, O, P) {
	  getOrCreateMetadataMap(O, P, true).set(MetadataKey, MetadataValue);
	};
	var ordinaryOwnMetadataKeys = function (target, targetKey) {
	  var metadataMap = getOrCreateMetadataMap(target, targetKey, false);
	  var keys = [];
	  if (metadataMap) metadataMap.forEach(function (_, key) { keys.push(key); });
	  return keys;
	};
	var toMetaKey = function (it) {
	  return it === undefined || typeof it == 'symbol' ? it : String(it);
	};
	var exp = function (O) {
	  $export($export.S, 'Reflect', O);
	};

	module.exports = {
	  store: store,
	  map: getOrCreateMetadataMap,
	  has: ordinaryHasOwnMetadata,
	  get: ordinaryGetOwnMetadata,
	  set: ordinaryDefineOwnMetadata,
	  keys: ordinaryOwnMetadataKeys,
	  key: toMetaKey,
	  exp: exp
	};


/***/ }),
/* 310 */
/***/ (function(module, exports, __webpack_require__) {

	var metadata = __webpack_require__(309);
	var anObject = __webpack_require__(12);
	var toMetaKey = metadata.key;
	var getOrCreateMetadataMap = metadata.map;
	var store = metadata.store;

	metadata.exp({ deleteMetadata: function deleteMetadata(metadataKey, target /* , targetKey */) {
	  var targetKey = arguments.length < 3 ? undefined : toMetaKey(arguments[2]);
	  var metadataMap = getOrCreateMetadataMap(anObject(target), targetKey, false);
	  if (metadataMap === undefined || !metadataMap['delete'](metadataKey)) return false;
	  if (metadataMap.size) return true;
	  var targetMetadata = store.get(target);
	  targetMetadata['delete'](targetKey);
	  return !!targetMetadata.size || store['delete'](target);
	} });


/***/ }),
/* 311 */
/***/ (function(module, exports, __webpack_require__) {

	var metadata = __webpack_require__(309);
	var anObject = __webpack_require__(12);
	var getPrototypeOf = __webpack_require__(58);
	var ordinaryHasOwnMetadata = metadata.has;
	var ordinaryGetOwnMetadata = metadata.get;
	var toMetaKey = metadata.key;

	var ordinaryGetMetadata = function (MetadataKey, O, P) {
	  var hasOwn = ordinaryHasOwnMetadata(MetadataKey, O, P);
	  if (hasOwn) return ordinaryGetOwnMetadata(MetadataKey, O, P);
	  var parent = getPrototypeOf(O);
	  return parent !== null ? ordinaryGetMetadata(MetadataKey, parent, P) : undefined;
	};

	metadata.exp({ getMetadata: function getMetadata(metadataKey, target /* , targetKey */) {
	  return ordinaryGetMetadata(metadataKey, anObject(target), arguments.length < 3 ? undefined : toMetaKey(arguments[2]));
	} });


/***/ }),
/* 312 */
/***/ (function(module, exports, __webpack_require__) {

	var Set = __webpack_require__(220);
	var from = __webpack_require__(278);
	var metadata = __webpack_require__(309);
	var anObject = __webpack_require__(12);
	var getPrototypeOf = __webpack_require__(58);
	var ordinaryOwnMetadataKeys = metadata.keys;
	var toMetaKey = metadata.key;

	var ordinaryMetadataKeys = function (O, P) {
	  var oKeys = ordinaryOwnMetadataKeys(O, P);
	  var parent = getPrototypeOf(O);
	  if (parent === null) return oKeys;
	  var pKeys = ordinaryMetadataKeys(parent, P);
	  return pKeys.length ? oKeys.length ? from(new Set(oKeys.concat(pKeys))) : pKeys : oKeys;
	};

	metadata.exp({ getMetadataKeys: function getMetadataKeys(target /* , targetKey */) {
	  return ordinaryMetadataKeys(anObject(target), arguments.length < 2 ? undefined : toMetaKey(arguments[1]));
	} });


/***/ }),
/* 313 */
/***/ (function(module, exports, __webpack_require__) {

	var metadata = __webpack_require__(309);
	var anObject = __webpack_require__(12);
	var ordinaryGetOwnMetadata = metadata.get;
	var toMetaKey = metadata.key;

	metadata.exp({ getOwnMetadata: function getOwnMetadata(metadataKey, target /* , targetKey */) {
	  return ordinaryGetOwnMetadata(metadataKey, anObject(target)
	    , arguments.length < 3 ? undefined : toMetaKey(arguments[2]));
	} });


/***/ }),
/* 314 */
/***/ (function(module, exports, __webpack_require__) {

	var metadata = __webpack_require__(309);
	var anObject = __webpack_require__(12);
	var ordinaryOwnMetadataKeys = metadata.keys;
	var toMetaKey = metadata.key;

	metadata.exp({ getOwnMetadataKeys: function getOwnMetadataKeys(target /* , targetKey */) {
	  return ordinaryOwnMetadataKeys(anObject(target), arguments.length < 2 ? undefined : toMetaKey(arguments[1]));
	} });


/***/ }),
/* 315 */
/***/ (function(module, exports, __webpack_require__) {

	var metadata = __webpack_require__(309);
	var anObject = __webpack_require__(12);
	var getPrototypeOf = __webpack_require__(58);
	var ordinaryHasOwnMetadata = metadata.has;
	var toMetaKey = metadata.key;

	var ordinaryHasMetadata = function (MetadataKey, O, P) {
	  var hasOwn = ordinaryHasOwnMetadata(MetadataKey, O, P);
	  if (hasOwn) return true;
	  var parent = getPrototypeOf(O);
	  return parent !== null ? ordinaryHasMetadata(MetadataKey, parent, P) : false;
	};

	metadata.exp({ hasMetadata: function hasMetadata(metadataKey, target /* , targetKey */) {
	  return ordinaryHasMetadata(metadataKey, anObject(target), arguments.length < 3 ? undefined : toMetaKey(arguments[2]));
	} });


/***/ }),
/* 316 */
/***/ (function(module, exports, __webpack_require__) {

	var metadata = __webpack_require__(309);
	var anObject = __webpack_require__(12);
	var ordinaryHasOwnMetadata = metadata.has;
	var toMetaKey = metadata.key;

	metadata.exp({ hasOwnMetadata: function hasOwnMetadata(metadataKey, target /* , targetKey */) {
	  return ordinaryHasOwnMetadata(metadataKey, anObject(target)
	    , arguments.length < 3 ? undefined : toMetaKey(arguments[2]));
	} });


/***/ }),
/* 317 */
/***/ (function(module, exports, __webpack_require__) {

	var $metadata = __webpack_require__(309);
	var anObject = __webpack_require__(12);
	var aFunction = __webpack_require__(21);
	var toMetaKey = $metadata.key;
	var ordinaryDefineOwnMetadata = $metadata.set;

	$metadata.exp({ metadata: function metadata(metadataKey, metadataValue) {
	  return function decorator(target, targetKey) {
	    ordinaryDefineOwnMetadata(
	      metadataKey, metadataValue,
	      (targetKey !== undefined ? anObject : aFunction)(target),
	      toMetaKey(targetKey)
	    );
	  };
	} });


/***/ }),
/* 318 */
/***/ (function(module, exports, __webpack_require__) {

	// https://github.com/rwaldron/tc39-notes/blob/master/es6/2014-09/sept-25.md#510-globalasap-for-enqueuing-a-microtask
	var $export = __webpack_require__(8);
	var microtask = __webpack_require__(210)();
	var process = __webpack_require__(4).process;
	var isNode = __webpack_require__(34)(process) == 'process';

	$export($export.G, {
	  asap: function asap(fn) {
	    var domain = isNode && process.domain;
	    microtask(domain ? domain.bind(fn) : fn);
	  }
	});


/***/ }),
/* 319 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';
	// https://github.com/zenparsing/es-observable
	var $export = __webpack_require__(8);
	var global = __webpack_require__(4);
	var core = __webpack_require__(9);
	var microtask = __webpack_require__(210)();
	var OBSERVABLE = __webpack_require__(26)('observable');
	var aFunction = __webpack_require__(21);
	var anObject = __webpack_require__(12);
	var anInstance = __webpack_require__(206);
	var redefineAll = __webpack_require__(215);
	var hide = __webpack_require__(10);
	var forOf = __webpack_require__(207);
	var RETURN = forOf.RETURN;

	var getMethod = function (fn) {
	  return fn == null ? undefined : aFunction(fn);
	};

	var cleanupSubscription = function (subscription) {
	  var cleanup = subscription._c;
	  if (cleanup) {
	    subscription._c = undefined;
	    cleanup();
	  }
	};

	var subscriptionClosed = function (subscription) {
	  return subscription._o === undefined;
	};

	var closeSubscription = function (subscription) {
	  if (!subscriptionClosed(subscription)) {
	    subscription._o = undefined;
	    cleanupSubscription(subscription);
	  }
	};

	var Subscription = function (observer, subscriber) {
	  anObject(observer);
	  this._c = undefined;
	  this._o = observer;
	  observer = new SubscriptionObserver(this);
	  try {
	    var cleanup = subscriber(observer);
	    var subscription = cleanup;
	    if (cleanup != null) {
	      if (typeof cleanup.unsubscribe === 'function') cleanup = function () { subscription.unsubscribe(); };
	      else aFunction(cleanup);
	      this._c = cleanup;
	    }
	  } catch (e) {
	    observer.error(e);
	    return;
	  } if (subscriptionClosed(this)) cleanupSubscription(this);
	};

	Subscription.prototype = redefineAll({}, {
	  unsubscribe: function unsubscribe() { closeSubscription(this); }
	});

	var SubscriptionObserver = function (subscription) {
	  this._s = subscription;
	};

	SubscriptionObserver.prototype = redefineAll({}, {
	  next: function next(value) {
	    var subscription = this._s;
	    if (!subscriptionClosed(subscription)) {
	      var observer = subscription._o;
	      try {
	        var m = getMethod(observer.next);
	        if (m) return m.call(observer, value);
	      } catch (e) {
	        try {
	          closeSubscription(subscription);
	        } finally {
	          throw e;
	        }
	      }
	    }
	  },
	  error: function error(value) {
	    var subscription = this._s;
	    if (subscriptionClosed(subscription)) throw value;
	    var observer = subscription._o;
	    subscription._o = undefined;
	    try {
	      var m = getMethod(observer.error);
	      if (!m) throw value;
	      value = m.call(observer, value);
	    } catch (e) {
	      try {
	        cleanupSubscription(subscription);
	      } finally {
	        throw e;
	      }
	    } cleanupSubscription(subscription);
	    return value;
	  },
	  complete: function complete(value) {
	    var subscription = this._s;
	    if (!subscriptionClosed(subscription)) {
	      var observer = subscription._o;
	      subscription._o = undefined;
	      try {
	        var m = getMethod(observer.complete);
	        value = m ? m.call(observer, value) : undefined;
	      } catch (e) {
	        try {
	          cleanupSubscription(subscription);
	        } finally {
	          throw e;
	        }
	      } cleanupSubscription(subscription);
	      return value;
	    }
	  }
	});

	var $Observable = function Observable(subscriber) {
	  anInstance(this, $Observable, 'Observable', '_f')._f = aFunction(subscriber);
	};

	redefineAll($Observable.prototype, {
	  subscribe: function subscribe(observer) {
	    return new Subscription(observer, this._f);
	  },
	  forEach: function forEach(fn) {
	    var that = this;
	    return new (core.Promise || global.Promise)(function (resolve, reject) {
	      aFunction(fn);
	      var subscription = that.subscribe({
	        next: function (value) {
	          try {
	            return fn(value);
	          } catch (e) {
	            reject(e);
	            subscription.unsubscribe();
	          }
	        },
	        error: reject,
	        complete: resolve
	      });
	    });
	  }
	});

	redefineAll($Observable, {
	  from: function from(x) {
	    var C = typeof this === 'function' ? this : $Observable;
	    var method = getMethod(anObject(x)[OBSERVABLE]);
	    if (method) {
	      var observable = anObject(method.call(x));
	      return observable.constructor === C ? observable : new C(function (observer) {
	        return observable.subscribe(observer);
	      });
	    }
	    return new C(function (observer) {
	      var done = false;
	      microtask(function () {
	        if (!done) {
	          try {
	            if (forOf(x, false, function (it) {
	              observer.next(it);
	              if (done) return RETURN;
	            }) === RETURN) return;
	          } catch (e) {
	            if (done) throw e;
	            observer.error(e);
	            return;
	          } observer.complete();
	        }
	      });
	      return function () { done = true; };
	    });
	  },
	  of: function of() {
	    for (var i = 0, l = arguments.length, items = new Array(l); i < l;) items[i] = arguments[i++];
	    return new (typeof this === 'function' ? this : $Observable)(function (observer) {
	      var done = false;
	      microtask(function () {
	        if (!done) {
	          for (var j = 0; j < items.length; ++j) {
	            observer.next(items[j]);
	            if (done) return;
	          } observer.complete();
	        }
	      });
	      return function () { done = true; };
	    });
	  }
	});

	hide($Observable.prototype, OBSERVABLE, function () { return this; });

	$export($export.G, { Observable: $Observable });

	__webpack_require__(193)('Observable');


/***/ }),
/* 320 */
/***/ (function(module, exports, __webpack_require__) {

	// ie9- setTimeout & setInterval additional parameters fix
	var global = __webpack_require__(4);
	var $export = __webpack_require__(8);
	var userAgent = __webpack_require__(213);
	var slice = [].slice;
	var MSIE = /MSIE .\./.test(userAgent); // <- dirty ie9- check
	var wrap = function (set) {
	  return function (fn, time /* , ...args */) {
	    var boundArgs = arguments.length > 2;
	    var args = boundArgs ? slice.call(arguments, 2) : false;
	    return set(boundArgs ? function () {
	      // eslint-disable-next-line no-new-func
	      (typeof fn == 'function' ? fn : Function(fn)).apply(this, args);
	    } : fn, time);
	  };
	};
	$export($export.G + $export.B + $export.F * MSIE, {
	  setTimeout: wrap(global.setTimeout),
	  setInterval: wrap(global.setInterval)
	});


/***/ }),
/* 321 */
/***/ (function(module, exports, __webpack_require__) {

	var $export = __webpack_require__(8);
	var $task = __webpack_require__(209);
	$export($export.G + $export.B, {
	  setImmediate: $task.set,
	  clearImmediate: $task.clear
	});


/***/ }),
/* 322 */
/***/ (function(module, exports, __webpack_require__) {

	var $iterators = __webpack_require__(194);
	var getKeys = __webpack_require__(30);
	var redefine = __webpack_require__(18);
	var global = __webpack_require__(4);
	var hide = __webpack_require__(10);
	var Iterators = __webpack_require__(129);
	var wks = __webpack_require__(26);
	var ITERATOR = wks('iterator');
	var TO_STRING_TAG = wks('toStringTag');
	var ArrayValues = Iterators.Array;

	var DOMIterables = {
	  CSSRuleList: true, // TODO: Not spec compliant, should be false.
	  CSSStyleDeclaration: false,
	  CSSValueList: false,
	  ClientRectList: false,
	  DOMRectList: false,
	  DOMStringList: false,
	  DOMTokenList: true,
	  DataTransferItemList: false,
	  FileList: false,
	  HTMLAllCollection: false,
	  HTMLCollection: false,
	  HTMLFormElement: false,
	  HTMLSelectElement: false,
	  MediaList: true, // TODO: Not spec compliant, should be false.
	  MimeTypeArray: false,
	  NamedNodeMap: false,
	  NodeList: true,
	  PaintRequestList: false,
	  Plugin: false,
	  PluginArray: false,
	  SVGLengthList: false,
	  SVGNumberList: false,
	  SVGPathSegList: false,
	  SVGPointList: false,
	  SVGStringList: false,
	  SVGTransformList: false,
	  SourceBufferList: false,
	  StyleSheetList: true, // TODO: Not spec compliant, should be false.
	  TextTrackCueList: false,
	  TextTrackList: false,
	  TouchList: false
	};

	for (var collections = getKeys(DOMIterables), i = 0; i < collections.length; i++) {
	  var NAME = collections[i];
	  var explicit = DOMIterables[NAME];
	  var Collection = global[NAME];
	  var proto = Collection && Collection.prototype;
	  var key;
	  if (proto) {
	    if (!proto[ITERATOR]) hide(proto, ITERATOR, ArrayValues);
	    if (!proto[TO_STRING_TAG]) hide(proto, TO_STRING_TAG, NAME);
	    Iterators[NAME] = ArrayValues;
	    if (explicit) for (key in $iterators) if (!proto[key]) redefine(proto, key, $iterators[key], true);
	  }
	}


/***/ }),
/* 323 */
/***/ (function(module, exports) {

	/* WEBPACK VAR INJECTION */(function(global) {/**
	 * Copyright (c) 2014, Facebook, Inc.
	 * All rights reserved.
	 *
	 * This source code is licensed under the BSD-style license found in the
	 * https://raw.github.com/facebook/regenerator/master/LICENSE file. An
	 * additional grant of patent rights can be found in the PATENTS file in
	 * the same directory.
	 */

	!(function(global) {
	  "use strict";

	  var Op = Object.prototype;
	  var hasOwn = Op.hasOwnProperty;
	  var undefined; // More compressible than void 0.
	  var $Symbol = typeof Symbol === "function" ? Symbol : {};
	  var iteratorSymbol = $Symbol.iterator || "@@iterator";
	  var asyncIteratorSymbol = $Symbol.asyncIterator || "@@asyncIterator";
	  var toStringTagSymbol = $Symbol.toStringTag || "@@toStringTag";

	  var inModule = typeof module === "object";
	  var runtime = global.regeneratorRuntime;
	  if (runtime) {
	    if (inModule) {
	      // If regeneratorRuntime is defined globally and we're in a module,
	      // make the exports object identical to regeneratorRuntime.
	      module.exports = runtime;
	    }
	    // Don't bother evaluating the rest of this file if the runtime was
	    // already defined globally.
	    return;
	  }

	  // Define the runtime globally (as expected by generated code) as either
	  // module.exports (if we're in a module) or a new, empty object.
	  runtime = global.regeneratorRuntime = inModule ? module.exports : {};

	  function wrap(innerFn, outerFn, self, tryLocsList) {
	    // If outerFn provided and outerFn.prototype is a Generator, then outerFn.prototype instanceof Generator.
	    var protoGenerator = outerFn && outerFn.prototype instanceof Generator ? outerFn : Generator;
	    var generator = Object.create(protoGenerator.prototype);
	    var context = new Context(tryLocsList || []);

	    // The ._invoke method unifies the implementations of the .next,
	    // .throw, and .return methods.
	    generator._invoke = makeInvokeMethod(innerFn, self, context);

	    return generator;
	  }
	  runtime.wrap = wrap;

	  // Try/catch helper to minimize deoptimizations. Returns a completion
	  // record like context.tryEntries[i].completion. This interface could
	  // have been (and was previously) designed to take a closure to be
	  // invoked without arguments, but in all the cases we care about we
	  // already have an existing method we want to call, so there's no need
	  // to create a new function object. We can even get away with assuming
	  // the method takes exactly one argument, since that happens to be true
	  // in every case, so we don't have to touch the arguments object. The
	  // only additional allocation required is the completion record, which
	  // has a stable shape and so hopefully should be cheap to allocate.
	  function tryCatch(fn, obj, arg) {
	    try {
	      return { type: "normal", arg: fn.call(obj, arg) };
	    } catch (err) {
	      return { type: "throw", arg: err };
	    }
	  }

	  var GenStateSuspendedStart = "suspendedStart";
	  var GenStateSuspendedYield = "suspendedYield";
	  var GenStateExecuting = "executing";
	  var GenStateCompleted = "completed";

	  // Returning this object from the innerFn has the same effect as
	  // breaking out of the dispatch switch statement.
	  var ContinueSentinel = {};

	  // Dummy constructor functions that we use as the .constructor and
	  // .constructor.prototype properties for functions that return Generator
	  // objects. For full spec compliance, you may wish to configure your
	  // minifier not to mangle the names of these two functions.
	  function Generator() {}
	  function GeneratorFunction() {}
	  function GeneratorFunctionPrototype() {}

	  // This is a polyfill for %IteratorPrototype% for environments that
	  // don't natively support it.
	  var IteratorPrototype = {};
	  IteratorPrototype[iteratorSymbol] = function () {
	    return this;
	  };

	  var getProto = Object.getPrototypeOf;
	  var NativeIteratorPrototype = getProto && getProto(getProto(values([])));
	  if (NativeIteratorPrototype &&
	      NativeIteratorPrototype !== Op &&
	      hasOwn.call(NativeIteratorPrototype, iteratorSymbol)) {
	    // This environment has a native %IteratorPrototype%; use it instead
	    // of the polyfill.
	    IteratorPrototype = NativeIteratorPrototype;
	  }

	  var Gp = GeneratorFunctionPrototype.prototype =
	    Generator.prototype = Object.create(IteratorPrototype);
	  GeneratorFunction.prototype = Gp.constructor = GeneratorFunctionPrototype;
	  GeneratorFunctionPrototype.constructor = GeneratorFunction;
	  GeneratorFunctionPrototype[toStringTagSymbol] =
	    GeneratorFunction.displayName = "GeneratorFunction";

	  // Helper for defining the .next, .throw, and .return methods of the
	  // Iterator interface in terms of a single ._invoke method.
	  function defineIteratorMethods(prototype) {
	    ["next", "throw", "return"].forEach(function(method) {
	      prototype[method] = function(arg) {
	        return this._invoke(method, arg);
	      };
	    });
	  }

	  runtime.isGeneratorFunction = function(genFun) {
	    var ctor = typeof genFun === "function" && genFun.constructor;
	    return ctor
	      ? ctor === GeneratorFunction ||
	        // For the native GeneratorFunction constructor, the best we can
	        // do is to check its .name property.
	        (ctor.displayName || ctor.name) === "GeneratorFunction"
	      : false;
	  };

	  runtime.mark = function(genFun) {
	    if (Object.setPrototypeOf) {
	      Object.setPrototypeOf(genFun, GeneratorFunctionPrototype);
	    } else {
	      genFun.__proto__ = GeneratorFunctionPrototype;
	      if (!(toStringTagSymbol in genFun)) {
	        genFun[toStringTagSymbol] = "GeneratorFunction";
	      }
	    }
	    genFun.prototype = Object.create(Gp);
	    return genFun;
	  };

	  // Within the body of any async function, `await x` is transformed to
	  // `yield regeneratorRuntime.awrap(x)`, so that the runtime can test
	  // `hasOwn.call(value, "__await")` to determine if the yielded value is
	  // meant to be awaited.
	  runtime.awrap = function(arg) {
	    return { __await: arg };
	  };

	  function AsyncIterator(generator) {
	    function invoke(method, arg, resolve, reject) {
	      var record = tryCatch(generator[method], generator, arg);
	      if (record.type === "throw") {
	        reject(record.arg);
	      } else {
	        var result = record.arg;
	        var value = result.value;
	        if (value &&
	            typeof value === "object" &&
	            hasOwn.call(value, "__await")) {
	          return Promise.resolve(value.__await).then(function(value) {
	            invoke("next", value, resolve, reject);
	          }, function(err) {
	            invoke("throw", err, resolve, reject);
	          });
	        }

	        return Promise.resolve(value).then(function(unwrapped) {
	          // When a yielded Promise is resolved, its final value becomes
	          // the .value of the Promise<{value,done}> result for the
	          // current iteration. If the Promise is rejected, however, the
	          // result for this iteration will be rejected with the same
	          // reason. Note that rejections of yielded Promises are not
	          // thrown back into the generator function, as is the case
	          // when an awaited Promise is rejected. This difference in
	          // behavior between yield and await is important, because it
	          // allows the consumer to decide what to do with the yielded
	          // rejection (swallow it and continue, manually .throw it back
	          // into the generator, abandon iteration, whatever). With
	          // await, by contrast, there is no opportunity to examine the
	          // rejection reason outside the generator function, so the
	          // only option is to throw it from the await expression, and
	          // let the generator function handle the exception.
	          result.value = unwrapped;
	          resolve(result);
	        }, reject);
	      }
	    }

	    if (typeof global.process === "object" && global.process.domain) {
	      invoke = global.process.domain.bind(invoke);
	    }

	    var previousPromise;

	    function enqueue(method, arg) {
	      function callInvokeWithMethodAndArg() {
	        return new Promise(function(resolve, reject) {
	          invoke(method, arg, resolve, reject);
	        });
	      }

	      return previousPromise =
	        // If enqueue has been called before, then we want to wait until
	        // all previous Promises have been resolved before calling invoke,
	        // so that results are always delivered in the correct order. If
	        // enqueue has not been called before, then it is important to
	        // call invoke immediately, without waiting on a callback to fire,
	        // so that the async generator function has the opportunity to do
	        // any necessary setup in a predictable way. This predictability
	        // is why the Promise constructor synchronously invokes its
	        // executor callback, and why async functions synchronously
	        // execute code before the first await. Since we implement simple
	        // async functions in terms of async generators, it is especially
	        // important to get this right, even though it requires care.
	        previousPromise ? previousPromise.then(
	          callInvokeWithMethodAndArg,
	          // Avoid propagating failures to Promises returned by later
	          // invocations of the iterator.
	          callInvokeWithMethodAndArg
	        ) : callInvokeWithMethodAndArg();
	    }

	    // Define the unified helper method that is used to implement .next,
	    // .throw, and .return (see defineIteratorMethods).
	    this._invoke = enqueue;
	  }

	  defineIteratorMethods(AsyncIterator.prototype);
	  AsyncIterator.prototype[asyncIteratorSymbol] = function () {
	    return this;
	  };
	  runtime.AsyncIterator = AsyncIterator;

	  // Note that simple async functions are implemented on top of
	  // AsyncIterator objects; they just return a Promise for the value of
	  // the final result produced by the iterator.
	  runtime.async = function(innerFn, outerFn, self, tryLocsList) {
	    var iter = new AsyncIterator(
	      wrap(innerFn, outerFn, self, tryLocsList)
	    );

	    return runtime.isGeneratorFunction(outerFn)
	      ? iter // If outerFn is a generator, return the full iterator.
	      : iter.next().then(function(result) {
	          return result.done ? result.value : iter.next();
	        });
	  };

	  function makeInvokeMethod(innerFn, self, context) {
	    var state = GenStateSuspendedStart;

	    return function invoke(method, arg) {
	      if (state === GenStateExecuting) {
	        throw new Error("Generator is already running");
	      }

	      if (state === GenStateCompleted) {
	        if (method === "throw") {
	          throw arg;
	        }

	        // Be forgiving, per 25.3.3.3.3 of the spec:
	        // https://people.mozilla.org/~jorendorff/es6-draft.html#sec-generatorresume
	        return doneResult();
	      }

	      context.method = method;
	      context.arg = arg;

	      while (true) {
	        var delegate = context.delegate;
	        if (delegate) {
	          var delegateResult = maybeInvokeDelegate(delegate, context);
	          if (delegateResult) {
	            if (delegateResult === ContinueSentinel) continue;
	            return delegateResult;
	          }
	        }

	        if (context.method === "next") {
	          // Setting context._sent for legacy support of Babel's
	          // function.sent implementation.
	          context.sent = context._sent = context.arg;

	        } else if (context.method === "throw") {
	          if (state === GenStateSuspendedStart) {
	            state = GenStateCompleted;
	            throw context.arg;
	          }

	          context.dispatchException(context.arg);

	        } else if (context.method === "return") {
	          context.abrupt("return", context.arg);
	        }

	        state = GenStateExecuting;

	        var record = tryCatch(innerFn, self, context);
	        if (record.type === "normal") {
	          // If an exception is thrown from innerFn, we leave state ===
	          // GenStateExecuting and loop back for another invocation.
	          state = context.done
	            ? GenStateCompleted
	            : GenStateSuspendedYield;

	          if (record.arg === ContinueSentinel) {
	            continue;
	          }

	          return {
	            value: record.arg,
	            done: context.done
	          };

	        } else if (record.type === "throw") {
	          state = GenStateCompleted;
	          // Dispatch the exception by looping back around to the
	          // context.dispatchException(context.arg) call above.
	          context.method = "throw";
	          context.arg = record.arg;
	        }
	      }
	    };
	  }

	  // Call delegate.iterator[context.method](context.arg) and handle the
	  // result, either by returning a { value, done } result from the
	  // delegate iterator, or by modifying context.method and context.arg,
	  // setting context.delegate to null, and returning the ContinueSentinel.
	  function maybeInvokeDelegate(delegate, context) {
	    var method = delegate.iterator[context.method];
	    if (method === undefined) {
	      // A .throw or .return when the delegate iterator has no .throw
	      // method always terminates the yield* loop.
	      context.delegate = null;

	      if (context.method === "throw") {
	        if (delegate.iterator.return) {
	          // If the delegate iterator has a return method, give it a
	          // chance to clean up.
	          context.method = "return";
	          context.arg = undefined;
	          maybeInvokeDelegate(delegate, context);

	          if (context.method === "throw") {
	            // If maybeInvokeDelegate(context) changed context.method from
	            // "return" to "throw", let that override the TypeError below.
	            return ContinueSentinel;
	          }
	        }

	        context.method = "throw";
	        context.arg = new TypeError(
	          "The iterator does not provide a 'throw' method");
	      }

	      return ContinueSentinel;
	    }

	    var record = tryCatch(method, delegate.iterator, context.arg);

	    if (record.type === "throw") {
	      context.method = "throw";
	      context.arg = record.arg;
	      context.delegate = null;
	      return ContinueSentinel;
	    }

	    var info = record.arg;

	    if (! info) {
	      context.method = "throw";
	      context.arg = new TypeError("iterator result is not an object");
	      context.delegate = null;
	      return ContinueSentinel;
	    }

	    if (info.done) {
	      // Assign the result of the finished delegate to the temporary
	      // variable specified by delegate.resultName (see delegateYield).
	      context[delegate.resultName] = info.value;

	      // Resume execution at the desired location (see delegateYield).
	      context.next = delegate.nextLoc;

	      // If context.method was "throw" but the delegate handled the
	      // exception, let the outer generator proceed normally. If
	      // context.method was "next", forget context.arg since it has been
	      // "consumed" by the delegate iterator. If context.method was
	      // "return", allow the original .return call to continue in the
	      // outer generator.
	      if (context.method !== "return") {
	        context.method = "next";
	        context.arg = undefined;
	      }

	    } else {
	      // Re-yield the result returned by the delegate method.
	      return info;
	    }

	    // The delegate iterator is finished, so forget it and continue with
	    // the outer generator.
	    context.delegate = null;
	    return ContinueSentinel;
	  }

	  // Define Generator.prototype.{next,throw,return} in terms of the
	  // unified ._invoke helper method.
	  defineIteratorMethods(Gp);

	  Gp[toStringTagSymbol] = "Generator";

	  // A Generator should always return itself as the iterator object when the
	  // @@iterator function is called on it. Some browsers' implementations of the
	  // iterator prototype chain incorrectly implement this, causing the Generator
	  // object to not be returned from this call. This ensures that doesn't happen.
	  // See https://github.com/facebook/regenerator/issues/274 for more details.
	  Gp[iteratorSymbol] = function() {
	    return this;
	  };

	  Gp.toString = function() {
	    return "[object Generator]";
	  };

	  function pushTryEntry(locs) {
	    var entry = { tryLoc: locs[0] };

	    if (1 in locs) {
	      entry.catchLoc = locs[1];
	    }

	    if (2 in locs) {
	      entry.finallyLoc = locs[2];
	      entry.afterLoc = locs[3];
	    }

	    this.tryEntries.push(entry);
	  }

	  function resetTryEntry(entry) {
	    var record = entry.completion || {};
	    record.type = "normal";
	    delete record.arg;
	    entry.completion = record;
	  }

	  function Context(tryLocsList) {
	    // The root entry object (effectively a try statement without a catch
	    // or a finally block) gives us a place to store values thrown from
	    // locations where there is no enclosing try statement.
	    this.tryEntries = [{ tryLoc: "root" }];
	    tryLocsList.forEach(pushTryEntry, this);
	    this.reset(true);
	  }

	  runtime.keys = function(object) {
	    var keys = [];
	    for (var key in object) {
	      keys.push(key);
	    }
	    keys.reverse();

	    // Rather than returning an object with a next method, we keep
	    // things simple and return the next function itself.
	    return function next() {
	      while (keys.length) {
	        var key = keys.pop();
	        if (key in object) {
	          next.value = key;
	          next.done = false;
	          return next;
	        }
	      }

	      // To avoid creating an additional object, we just hang the .value
	      // and .done properties off the next function object itself. This
	      // also ensures that the minifier will not anonymize the function.
	      next.done = true;
	      return next;
	    };
	  };

	  function values(iterable) {
	    if (iterable) {
	      var iteratorMethod = iterable[iteratorSymbol];
	      if (iteratorMethod) {
	        return iteratorMethod.call(iterable);
	      }

	      if (typeof iterable.next === "function") {
	        return iterable;
	      }

	      if (!isNaN(iterable.length)) {
	        var i = -1, next = function next() {
	          while (++i < iterable.length) {
	            if (hasOwn.call(iterable, i)) {
	              next.value = iterable[i];
	              next.done = false;
	              return next;
	            }
	          }

	          next.value = undefined;
	          next.done = true;

	          return next;
	        };

	        return next.next = next;
	      }
	    }

	    // Return an iterator with no values.
	    return { next: doneResult };
	  }
	  runtime.values = values;

	  function doneResult() {
	    return { value: undefined, done: true };
	  }

	  Context.prototype = {
	    constructor: Context,

	    reset: function(skipTempReset) {
	      this.prev = 0;
	      this.next = 0;
	      // Resetting context._sent for legacy support of Babel's
	      // function.sent implementation.
	      this.sent = this._sent = undefined;
	      this.done = false;
	      this.delegate = null;

	      this.method = "next";
	      this.arg = undefined;

	      this.tryEntries.forEach(resetTryEntry);

	      if (!skipTempReset) {
	        for (var name in this) {
	          // Not sure about the optimal order of these conditions:
	          if (name.charAt(0) === "t" &&
	              hasOwn.call(this, name) &&
	              !isNaN(+name.slice(1))) {
	            this[name] = undefined;
	          }
	        }
	      }
	    },

	    stop: function() {
	      this.done = true;

	      var rootEntry = this.tryEntries[0];
	      var rootRecord = rootEntry.completion;
	      if (rootRecord.type === "throw") {
	        throw rootRecord.arg;
	      }

	      return this.rval;
	    },

	    dispatchException: function(exception) {
	      if (this.done) {
	        throw exception;
	      }

	      var context = this;
	      function handle(loc, caught) {
	        record.type = "throw";
	        record.arg = exception;
	        context.next = loc;

	        if (caught) {
	          // If the dispatched exception was caught by a catch block,
	          // then let that catch block handle the exception normally.
	          context.method = "next";
	          context.arg = undefined;
	        }

	        return !! caught;
	      }

	      for (var i = this.tryEntries.length - 1; i >= 0; --i) {
	        var entry = this.tryEntries[i];
	        var record = entry.completion;

	        if (entry.tryLoc === "root") {
	          // Exception thrown outside of any try block that could handle
	          // it, so set the completion value of the entire function to
	          // throw the exception.
	          return handle("end");
	        }

	        if (entry.tryLoc <= this.prev) {
	          var hasCatch = hasOwn.call(entry, "catchLoc");
	          var hasFinally = hasOwn.call(entry, "finallyLoc");

	          if (hasCatch && hasFinally) {
	            if (this.prev < entry.catchLoc) {
	              return handle(entry.catchLoc, true);
	            } else if (this.prev < entry.finallyLoc) {
	              return handle(entry.finallyLoc);
	            }

	          } else if (hasCatch) {
	            if (this.prev < entry.catchLoc) {
	              return handle(entry.catchLoc, true);
	            }

	          } else if (hasFinally) {
	            if (this.prev < entry.finallyLoc) {
	              return handle(entry.finallyLoc);
	            }

	          } else {
	            throw new Error("try statement without catch or finally");
	          }
	        }
	      }
	    },

	    abrupt: function(type, arg) {
	      for (var i = this.tryEntries.length - 1; i >= 0; --i) {
	        var entry = this.tryEntries[i];
	        if (entry.tryLoc <= this.prev &&
	            hasOwn.call(entry, "finallyLoc") &&
	            this.prev < entry.finallyLoc) {
	          var finallyEntry = entry;
	          break;
	        }
	      }

	      if (finallyEntry &&
	          (type === "break" ||
	           type === "continue") &&
	          finallyEntry.tryLoc <= arg &&
	          arg <= finallyEntry.finallyLoc) {
	        // Ignore the finally entry if control is not jumping to a
	        // location outside the try/catch block.
	        finallyEntry = null;
	      }

	      var record = finallyEntry ? finallyEntry.completion : {};
	      record.type = type;
	      record.arg = arg;

	      if (finallyEntry) {
	        this.method = "next";
	        this.next = finallyEntry.finallyLoc;
	        return ContinueSentinel;
	      }

	      return this.complete(record);
	    },

	    complete: function(record, afterLoc) {
	      if (record.type === "throw") {
	        throw record.arg;
	      }

	      if (record.type === "break" ||
	          record.type === "continue") {
	        this.next = record.arg;
	      } else if (record.type === "return") {
	        this.rval = this.arg = record.arg;
	        this.method = "return";
	        this.next = "end";
	      } else if (record.type === "normal" && afterLoc) {
	        this.next = afterLoc;
	      }

	      return ContinueSentinel;
	    },

	    finish: function(finallyLoc) {
	      for (var i = this.tryEntries.length - 1; i >= 0; --i) {
	        var entry = this.tryEntries[i];
	        if (entry.finallyLoc === finallyLoc) {
	          this.complete(entry.completion, entry.afterLoc);
	          resetTryEntry(entry);
	          return ContinueSentinel;
	        }
	      }
	    },

	    "catch": function(tryLoc) {
	      for (var i = this.tryEntries.length - 1; i >= 0; --i) {
	        var entry = this.tryEntries[i];
	        if (entry.tryLoc === tryLoc) {
	          var record = entry.completion;
	          if (record.type === "throw") {
	            var thrown = record.arg;
	            resetTryEntry(entry);
	          }
	          return thrown;
	        }
	      }

	      // The context.catch method must only be called with a location
	      // argument that corresponds to a known catch block.
	      throw new Error("illegal catch attempt");
	    },

	    delegateYield: function(iterable, resultName, nextLoc) {
	      this.delegate = {
	        iterator: values(iterable),
	        resultName: resultName,
	        nextLoc: nextLoc
	      };

	      if (this.method === "next") {
	        // Deliberately forget the last sent value so that we don't
	        // accidentally pass it on to the delegate.
	        this.arg = undefined;
	      }

	      return ContinueSentinel;
	    }
	  };
	})(
	  // Among the various tricks for obtaining a reference to the global
	  // object, this seems to be the most reliable technique that does not
	  // use indirect eval (which violates Content Security Policy).
	  typeof global === "object" ? global :
	  typeof window === "object" ? window :
	  typeof self === "object" ? self : this
	);

	/* WEBPACK VAR INJECTION */}.call(exports, (function() { return this; }())))

/***/ }),
/* 324 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(325);
	module.exports = __webpack_require__(9).RegExp.escape;


/***/ }),
/* 325 */
/***/ (function(module, exports, __webpack_require__) {

	// https://github.com/benjamingr/RexExp.escape
	var $export = __webpack_require__(8);
	var $re = __webpack_require__(326)(/[\\^$*+?.()|[\]{}]/g, '\\$&');

	$export($export.S, 'RegExp', { escape: function escape(it) { return $re(it); } });


/***/ }),
/* 326 */
/***/ (function(module, exports) {

	module.exports = function (regExp, replace) {
	  var replacer = replace === Object(replace) ? function (part) {
	    return replace[part];
	  } : replace;
	  return function (it) {
	    return String(it).replace(regExp, replacer);
	  };
	};


/***/ }),
/* 327 */
/***/ (function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(process) {'use strict';

	var EventEmitter = __webpack_require__(329);

	var devtools = __webpack_require__(330);
	var Chrome = __webpack_require__(372);

	function CDP(options, callback) {
	    if (typeof options === 'function') {
	        callback = options;
	        options = undefined;
	    }
	    var notifier = new EventEmitter();
	    if (typeof callback === 'function') {
	        // allow to register the error callback later
	        process.nextTick(function () {
	            new Chrome(options, notifier);
	        });
	        return notifier.once('connect', callback);
	    } else {
	        return new Promise(function (fulfill, reject) {
	            notifier.once('connect', fulfill);
	            notifier.once('error', reject);
	            new Chrome(options, notifier);
	        });
	    }
	}

	module.exports = CDP;
	module.exports.Protocol = devtools.Protocol;
	module.exports.List = devtools.List;
	module.exports.New = devtools.New;
	module.exports.Activate = devtools.Activate;
	module.exports.Close = devtools.Close;
	module.exports.Version = devtools.Version;
	/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(328)))

/***/ }),
/* 328 */
/***/ (function(module, exports) {

	// shim for using process in browser
	var process = module.exports = {};

	// cached from whatever global is present so that test runners that stub it
	// don't break things.  But we need to wrap it in a try catch in case it is
	// wrapped in strict mode code which doesn't define any globals.  It's inside a
	// function because try/catches deoptimize in certain engines.

	var cachedSetTimeout;
	var cachedClearTimeout;

	function defaultSetTimout() {
	    throw new Error('setTimeout has not been defined');
	}
	function defaultClearTimeout () {
	    throw new Error('clearTimeout has not been defined');
	}
	(function () {
	    try {
	        if (typeof setTimeout === 'function') {
	            cachedSetTimeout = setTimeout;
	        } else {
	            cachedSetTimeout = defaultSetTimout;
	        }
	    } catch (e) {
	        cachedSetTimeout = defaultSetTimout;
	    }
	    try {
	        if (typeof clearTimeout === 'function') {
	            cachedClearTimeout = clearTimeout;
	        } else {
	            cachedClearTimeout = defaultClearTimeout;
	        }
	    } catch (e) {
	        cachedClearTimeout = defaultClearTimeout;
	    }
	} ())
	function runTimeout(fun) {
	    if (cachedSetTimeout === setTimeout) {
	        //normal enviroments in sane situations
	        return setTimeout(fun, 0);
	    }
	    // if setTimeout wasn't available but was latter defined
	    if ((cachedSetTimeout === defaultSetTimout || !cachedSetTimeout) && setTimeout) {
	        cachedSetTimeout = setTimeout;
	        return setTimeout(fun, 0);
	    }
	    try {
	        // when when somebody has screwed with setTimeout but no I.E. maddness
	        return cachedSetTimeout(fun, 0);
	    } catch(e){
	        try {
	            // When we are in I.E. but the script has been evaled so I.E. doesn't trust the global object when called normally
	            return cachedSetTimeout.call(null, fun, 0);
	        } catch(e){
	            // same as above but when it's a version of I.E. that must have the global object for 'this', hopfully our context correct otherwise it will throw a global error
	            return cachedSetTimeout.call(this, fun, 0);
	        }
	    }


	}
	function runClearTimeout(marker) {
	    if (cachedClearTimeout === clearTimeout) {
	        //normal enviroments in sane situations
	        return clearTimeout(marker);
	    }
	    // if clearTimeout wasn't available but was latter defined
	    if ((cachedClearTimeout === defaultClearTimeout || !cachedClearTimeout) && clearTimeout) {
	        cachedClearTimeout = clearTimeout;
	        return clearTimeout(marker);
	    }
	    try {
	        // when when somebody has screwed with setTimeout but no I.E. maddness
	        return cachedClearTimeout(marker);
	    } catch (e){
	        try {
	            // When we are in I.E. but the script has been evaled so I.E. doesn't  trust the global object when called normally
	            return cachedClearTimeout.call(null, marker);
	        } catch (e){
	            // same as above but when it's a version of I.E. that must have the global object for 'this', hopfully our context correct otherwise it will throw a global error.
	            // Some versions of I.E. have different rules for clearTimeout vs setTimeout
	            return cachedClearTimeout.call(this, marker);
	        }
	    }



	}
	var queue = [];
	var draining = false;
	var currentQueue;
	var queueIndex = -1;

	function cleanUpNextTick() {
	    if (!draining || !currentQueue) {
	        return;
	    }
	    draining = false;
	    if (currentQueue.length) {
	        queue = currentQueue.concat(queue);
	    } else {
	        queueIndex = -1;
	    }
	    if (queue.length) {
	        drainQueue();
	    }
	}

	function drainQueue() {
	    if (draining) {
	        return;
	    }
	    var timeout = runTimeout(cleanUpNextTick);
	    draining = true;

	    var len = queue.length;
	    while(len) {
	        currentQueue = queue;
	        queue = [];
	        while (++queueIndex < len) {
	            if (currentQueue) {
	                currentQueue[queueIndex].run();
	            }
	        }
	        queueIndex = -1;
	        len = queue.length;
	    }
	    currentQueue = null;
	    draining = false;
	    runClearTimeout(timeout);
	}

	process.nextTick = function (fun) {
	    var args = new Array(arguments.length - 1);
	    if (arguments.length > 1) {
	        for (var i = 1; i < arguments.length; i++) {
	            args[i - 1] = arguments[i];
	        }
	    }
	    queue.push(new Item(fun, args));
	    if (queue.length === 1 && !draining) {
	        runTimeout(drainQueue);
	    }
	};

	// v8 likes predictible objects
	function Item(fun, array) {
	    this.fun = fun;
	    this.array = array;
	}
	Item.prototype.run = function () {
	    this.fun.apply(null, this.array);
	};
	process.title = 'browser';
	process.browser = true;
	process.env = {};
	process.argv = [];
	process.version = ''; // empty string to avoid regexp issues
	process.versions = {};

	function noop() {}

	process.on = noop;
	process.addListener = noop;
	process.once = noop;
	process.off = noop;
	process.removeListener = noop;
	process.removeAllListeners = noop;
	process.emit = noop;
	process.prependListener = noop;
	process.prependOnceListener = noop;

	process.listeners = function (name) { return [] }

	process.binding = function (name) {
	    throw new Error('process.binding is not supported');
	};

	process.cwd = function () { return '/' };
	process.chdir = function (dir) {
	    throw new Error('process.chdir is not supported');
	};
	process.umask = function() { return 0; };


/***/ }),
/* 329 */
/***/ (function(module, exports) {

	// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	function EventEmitter() {
	  this._events = this._events || {};
	  this._maxListeners = this._maxListeners || undefined;
	}
	module.exports = EventEmitter;

	// Backwards-compat with node 0.10.x
	EventEmitter.EventEmitter = EventEmitter;

	EventEmitter.prototype._events = undefined;
	EventEmitter.prototype._maxListeners = undefined;

	// By default EventEmitters will print a warning if more than 10 listeners are
	// added to it. This is a useful default which helps finding memory leaks.
	EventEmitter.defaultMaxListeners = 10;

	// Obviously not all Emitters should be limited to 10. This function allows
	// that to be increased. Set to zero for unlimited.
	EventEmitter.prototype.setMaxListeners = function(n) {
	  if (!isNumber(n) || n < 0 || isNaN(n))
	    throw TypeError('n must be a positive number');
	  this._maxListeners = n;
	  return this;
	};

	EventEmitter.prototype.emit = function(type) {
	  var er, handler, len, args, i, listeners;

	  if (!this._events)
	    this._events = {};

	  // If there is no 'error' event listener then throw.
	  if (type === 'error') {
	    if (!this._events.error ||
	        (isObject(this._events.error) && !this._events.error.length)) {
	      er = arguments[1];
	      if (er instanceof Error) {
	        throw er; // Unhandled 'error' event
	      } else {
	        // At least give some kind of context to the user
	        var err = new Error('Uncaught, unspecified "error" event. (' + er + ')');
	        err.context = er;
	        throw err;
	      }
	    }
	  }

	  handler = this._events[type];

	  if (isUndefined(handler))
	    return false;

	  if (isFunction(handler)) {
	    switch (arguments.length) {
	      // fast cases
	      case 1:
	        handler.call(this);
	        break;
	      case 2:
	        handler.call(this, arguments[1]);
	        break;
	      case 3:
	        handler.call(this, arguments[1], arguments[2]);
	        break;
	      // slower
	      default:
	        args = Array.prototype.slice.call(arguments, 1);
	        handler.apply(this, args);
	    }
	  } else if (isObject(handler)) {
	    args = Array.prototype.slice.call(arguments, 1);
	    listeners = handler.slice();
	    len = listeners.length;
	    for (i = 0; i < len; i++)
	      listeners[i].apply(this, args);
	  }

	  return true;
	};

	EventEmitter.prototype.addListener = function(type, listener) {
	  var m;

	  if (!isFunction(listener))
	    throw TypeError('listener must be a function');

	  if (!this._events)
	    this._events = {};

	  // To avoid recursion in the case that type === "newListener"! Before
	  // adding it to the listeners, first emit "newListener".
	  if (this._events.newListener)
	    this.emit('newListener', type,
	              isFunction(listener.listener) ?
	              listener.listener : listener);

	  if (!this._events[type])
	    // Optimize the case of one listener. Don't need the extra array object.
	    this._events[type] = listener;
	  else if (isObject(this._events[type]))
	    // If we've already got an array, just append.
	    this._events[type].push(listener);
	  else
	    // Adding the second element, need to change to array.
	    this._events[type] = [this._events[type], listener];

	  // Check for listener leak
	  if (isObject(this._events[type]) && !this._events[type].warned) {
	    if (!isUndefined(this._maxListeners)) {
	      m = this._maxListeners;
	    } else {
	      m = EventEmitter.defaultMaxListeners;
	    }

	    if (m && m > 0 && this._events[type].length > m) {
	      this._events[type].warned = true;
	      console.error('(node) warning: possible EventEmitter memory ' +
	                    'leak detected. %d listeners added. ' +
	                    'Use emitter.setMaxListeners() to increase limit.',
	                    this._events[type].length);
	      if (typeof console.trace === 'function') {
	        // not supported in IE 10
	        console.trace();
	      }
	    }
	  }

	  return this;
	};

	EventEmitter.prototype.on = EventEmitter.prototype.addListener;

	EventEmitter.prototype.once = function(type, listener) {
	  if (!isFunction(listener))
	    throw TypeError('listener must be a function');

	  var fired = false;

	  function g() {
	    this.removeListener(type, g);

	    if (!fired) {
	      fired = true;
	      listener.apply(this, arguments);
	    }
	  }

	  g.listener = listener;
	  this.on(type, g);

	  return this;
	};

	// emits a 'removeListener' event iff the listener was removed
	EventEmitter.prototype.removeListener = function(type, listener) {
	  var list, position, length, i;

	  if (!isFunction(listener))
	    throw TypeError('listener must be a function');

	  if (!this._events || !this._events[type])
	    return this;

	  list = this._events[type];
	  length = list.length;
	  position = -1;

	  if (list === listener ||
	      (isFunction(list.listener) && list.listener === listener)) {
	    delete this._events[type];
	    if (this._events.removeListener)
	      this.emit('removeListener', type, listener);

	  } else if (isObject(list)) {
	    for (i = length; i-- > 0;) {
	      if (list[i] === listener ||
	          (list[i].listener && list[i].listener === listener)) {
	        position = i;
	        break;
	      }
	    }

	    if (position < 0)
	      return this;

	    if (list.length === 1) {
	      list.length = 0;
	      delete this._events[type];
	    } else {
	      list.splice(position, 1);
	    }

	    if (this._events.removeListener)
	      this.emit('removeListener', type, listener);
	  }

	  return this;
	};

	EventEmitter.prototype.removeAllListeners = function(type) {
	  var key, listeners;

	  if (!this._events)
	    return this;

	  // not listening for removeListener, no need to emit
	  if (!this._events.removeListener) {
	    if (arguments.length === 0)
	      this._events = {};
	    else if (this._events[type])
	      delete this._events[type];
	    return this;
	  }

	  // emit removeListener for all listeners on all events
	  if (arguments.length === 0) {
	    for (key in this._events) {
	      if (key === 'removeListener') continue;
	      this.removeAllListeners(key);
	    }
	    this.removeAllListeners('removeListener');
	    this._events = {};
	    return this;
	  }

	  listeners = this._events[type];

	  if (isFunction(listeners)) {
	    this.removeListener(type, listeners);
	  } else if (listeners) {
	    // LIFO order
	    while (listeners.length)
	      this.removeListener(type, listeners[listeners.length - 1]);
	  }
	  delete this._events[type];

	  return this;
	};

	EventEmitter.prototype.listeners = function(type) {
	  var ret;
	  if (!this._events || !this._events[type])
	    ret = [];
	  else if (isFunction(this._events[type]))
	    ret = [this._events[type]];
	  else
	    ret = this._events[type].slice();
	  return ret;
	};

	EventEmitter.prototype.listenerCount = function(type) {
	  if (this._events) {
	    var evlistener = this._events[type];

	    if (isFunction(evlistener))
	      return 1;
	    else if (evlistener)
	      return evlistener.length;
	  }
	  return 0;
	};

	EventEmitter.listenerCount = function(emitter, type) {
	  return emitter.listenerCount(type);
	};

	function isFunction(arg) {
	  return typeof arg === 'function';
	}

	function isNumber(arg) {
	  return typeof arg === 'number';
	}

	function isObject(arg) {
	  return typeof arg === 'object' && arg !== null;
	}

	function isUndefined(arg) {
	  return arg === void 0;
	}


/***/ }),
/* 330 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';

	var http = __webpack_require__(331);
	var https = __webpack_require__(368);

	var defaults = __webpack_require__(369);
	var externalRequest = __webpack_require__(370);

	// options.path must be specified; callback(err, data)
	function devToolsInterface(options, callback) {
	    options.host = options.host || defaults.HOST;
	    options.port = options.port || defaults.PORT;
	    options.secure = !!options.secure;
	    options.useHostName = !!options.useHostName;
	    externalRequest(options.secure ? https : http, options, callback);
	}

	// wrapper that allows to return a promise if the callback is omitted, it works
	// for DevTools methods
	function promisesWrapper(func) {
	    return function (options, callback) {
	        // options is an optional argument
	        if (typeof options === 'function') {
	            callback = options;
	            options = undefined;
	        }
	        options = options || {};
	        // just call the function otherwise wrap a promise around its execution
	        if (typeof callback === 'function') {
	            func(options, callback);
	            return undefined;
	        } else {
	            return new Promise(function (fulfill, reject) {
	                func(options, function (err, result) {
	                    if (err) {
	                        reject(err);
	                    } else {
	                        fulfill(result);
	                    }
	                });
	            });
	        }
	    };
	}

	function Protocol(options, callback) {
	    // if the local protocol is requested
	    if (options.local) {
	        var localDescriptor = __webpack_require__(371);
	        callback(null, localDescriptor);
	        return;
	    }
	    // try to fecth the protocol remotely
	    options.path = '/json/protocol';
	    devToolsInterface(options, function (err, descriptor) {
	        if (err) {
	            callback(err);
	        } else {
	            callback(null, JSON.parse(descriptor));
	        }
	    });
	}

	function List(options, callback) {
	    options.path = '/json/list';
	    devToolsInterface(options, function (err, tabs) {
	        if (err) {
	            callback(err);
	        } else {
	            callback(null, JSON.parse(tabs));
	        }
	    });
	}

	function New(options, callback) {
	    options.path = '/json/new';
	    if (Object.prototype.hasOwnProperty.call(options, 'url')) {
	        options.path += '?' + options.url;
	    }
	    devToolsInterface(options, function (err, tab) {
	        if (err) {
	            callback(err);
	        } else {
	            callback(null, JSON.parse(tab));
	        }
	    });
	}

	function Activate(options, callback) {
	    options.path = '/json/activate/' + options.id;
	    devToolsInterface(options, function (err) {
	        if (err) {
	            callback(err);
	        } else {
	            callback(null);
	        }
	    });
	}

	function Close(options, callback) {
	    options.path = '/json/close/' + options.id;
	    devToolsInterface(options, function (err) {
	        if (err) {
	            callback(err);
	        } else {
	            callback(null);
	        }
	    });
	}

	function Version(options, callback) {
	    options.path = '/json/version';
	    devToolsInterface(options, function (err, versionInfo) {
	        if (err) {
	            callback(err);
	        } else {
	            callback(null, JSON.parse(versionInfo));
	        }
	    });
	}

	module.exports.Protocol = promisesWrapper(Protocol);
	module.exports.List = promisesWrapper(List);
	module.exports.New = promisesWrapper(New);
	module.exports.Activate = promisesWrapper(Activate);
	module.exports.Close = promisesWrapper(Close);
	module.exports.Version = promisesWrapper(Version);

/***/ }),
/* 331 */
/***/ (function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(global) {var ClientRequest = __webpack_require__(332)
	var response = __webpack_require__(339)
	var extend = __webpack_require__(359)
	var statusCodes = __webpack_require__(360)
	var url = __webpack_require__(361)

	var http = exports

	http.request = function (opts, cb) {
		if (typeof opts === 'string')
			opts = url.parse(opts)
		else
			opts = extend(opts)

		// Normally, the page is loaded from http or https, so not specifying a protocol
		// will result in a (valid) protocol-relative url. However, this won't work if
		// the protocol is something else, like 'file:'
		var defaultProtocol = global.location.protocol.search(/^https?:$/) === -1 ? 'http:' : ''

		var protocol = opts.protocol || defaultProtocol
		var host = opts.hostname || opts.host
		var port = opts.port
		var path = opts.path || '/'

		// Necessary for IPv6 addresses
		if (host && host.indexOf(':') !== -1)
			host = '[' + host + ']'

		// This may be a relative url. The browser should always be able to interpret it correctly.
		opts.url = (host ? (protocol + '//' + host) : '') + (port ? ':' + port : '') + path
		opts.method = (opts.method || 'GET').toUpperCase()
		opts.headers = opts.headers || {}

		// Also valid opts.auth, opts.mode

		var req = new ClientRequest(opts)
		if (cb)
			req.on('response', cb)
		return req
	}

	http.get = function get (opts, cb) {
		var req = http.request(opts, cb)
		req.end()
		return req
	}

	http.ClientRequest = ClientRequest
	http.IncomingMessage = response.IncomingMessage

	http.Agent = function () {}
	http.Agent.defaultMaxSockets = 4

	http.globalAgent = new http.Agent()

	http.STATUS_CODES = statusCodes

	http.METHODS = [
		'CHECKOUT',
		'CONNECT',
		'COPY',
		'DELETE',
		'GET',
		'HEAD',
		'LOCK',
		'M-SEARCH',
		'MERGE',
		'MKACTIVITY',
		'MKCOL',
		'MOVE',
		'NOTIFY',
		'OPTIONS',
		'PATCH',
		'POST',
		'PROPFIND',
		'PROPPATCH',
		'PURGE',
		'PUT',
		'REPORT',
		'SEARCH',
		'SUBSCRIBE',
		'TRACE',
		'UNLOCK',
		'UNSUBSCRIBE'
	]
	/* WEBPACK VAR INJECTION */}.call(exports, (function() { return this; }())))

/***/ }),
/* 332 */
/***/ (function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(Buffer, global, process) {var capability = __webpack_require__(337)
	var inherits = __webpack_require__(338)
	var response = __webpack_require__(339)
	var stream = __webpack_require__(340)
	var toArrayBuffer = __webpack_require__(358)

	var IncomingMessage = response.IncomingMessage
	var rStates = response.readyStates

	function decideMode (preferBinary, useFetch) {
		if (capability.fetch && useFetch) {
			return 'fetch'
		} else if (capability.mozchunkedarraybuffer) {
			return 'moz-chunked-arraybuffer'
		} else if (capability.msstream) {
			return 'ms-stream'
		} else if (capability.arraybuffer && preferBinary) {
			return 'arraybuffer'
		} else if (capability.vbArray && preferBinary) {
			return 'text:vbarray'
		} else {
			return 'text'
		}
	}

	var ClientRequest = module.exports = function (opts) {
		var self = this
		stream.Writable.call(self)

		self._opts = opts
		self._body = []
		self._headers = {}
		if (opts.auth)
			self.setHeader('Authorization', 'Basic ' + new Buffer(opts.auth).toString('base64'))
		Object.keys(opts.headers).forEach(function (name) {
			self.setHeader(name, opts.headers[name])
		})

		var preferBinary
		var useFetch = true
		if (opts.mode === 'disable-fetch' || ('requestTimeout' in opts && !capability.abortController)) {
			// If the use of XHR should be preferred. Not typically needed.
			useFetch = false
			preferBinary = true
		} else if (opts.mode === 'prefer-streaming') {
			// If streaming is a high priority but binary compatibility and
			// the accuracy of the 'content-type' header aren't
			preferBinary = false
		} else if (opts.mode === 'allow-wrong-content-type') {
			// If streaming is more important than preserving the 'content-type' header
			preferBinary = !capability.overrideMimeType
		} else if (!opts.mode || opts.mode === 'default' || opts.mode === 'prefer-fast') {
			// Use binary if text streaming may corrupt data or the content-type header, or for speed
			preferBinary = true
		} else {
			throw new Error('Invalid value for opts.mode')
		}
		self._mode = decideMode(preferBinary, useFetch)
		self._fetchTimer = null

		self.on('finish', function () {
			self._onFinish()
		})
	}

	inherits(ClientRequest, stream.Writable)

	ClientRequest.prototype.setHeader = function (name, value) {
		var self = this
		var lowerName = name.toLowerCase()
		// This check is not necessary, but it prevents warnings from browsers about setting unsafe
		// headers. To be honest I'm not entirely sure hiding these warnings is a good thing, but
		// http-browserify did it, so I will too.
		if (unsafeHeaders.indexOf(lowerName) !== -1)
			return

		self._headers[lowerName] = {
			name: name,
			value: value
		}
	}

	ClientRequest.prototype.getHeader = function (name) {
		var header = this._headers[name.toLowerCase()]
		if (header)
			return header.value
		return null
	}

	ClientRequest.prototype.removeHeader = function (name) {
		var self = this
		delete self._headers[name.toLowerCase()]
	}

	ClientRequest.prototype._onFinish = function () {
		var self = this

		if (self._destroyed)
			return
		var opts = self._opts

		var headersObj = self._headers
		var body = null
		if (opts.method !== 'GET' && opts.method !== 'HEAD') {
			if (capability.arraybuffer) {
				body = toArrayBuffer(Buffer.concat(self._body))
			} else if (capability.blobConstructor) {
				body = new global.Blob(self._body.map(function (buffer) {
					return toArrayBuffer(buffer)
				}), {
					type: (headersObj['content-type'] || {}).value || ''
				})
			} else {
				// get utf8 string
				body = Buffer.concat(self._body).toString()
			}
		}

		// create flattened list of headers
		var headersList = []
		Object.keys(headersObj).forEach(function (keyName) {
			var name = headersObj[keyName].name
			var value = headersObj[keyName].value
			if (Array.isArray(value)) {
				value.forEach(function (v) {
					headersList.push([name, v])
				})
			} else {
				headersList.push([name, value])
			}
		})

		if (self._mode === 'fetch') {
			var signal = null
			var fetchTimer = null
			if (capability.abortController) {
				var controller = new AbortController()
				signal = controller.signal
				self._fetchAbortController = controller

				if ('requestTimeout' in opts && opts.requestTimeout !== 0) {
					self._fetchTimer = global.setTimeout(function () {
						self.emit('requestTimeout')
						if (self._fetchAbortController)
							self._fetchAbortController.abort()
					}, opts.requestTimeout)
				}
			}

			global.fetch(self._opts.url, {
				method: self._opts.method,
				headers: headersList,
				body: body || undefined,
				mode: 'cors',
				credentials: opts.withCredentials ? 'include' : 'same-origin',
				signal: signal
			}).then(function (response) {
				self._fetchResponse = response
				self._connect()
			}, function (reason) {
				global.clearTimeout(self._fetchTimer)
				if (!self._destroyed)
					self.emit('error', reason)
			})
		} else {
			var xhr = self._xhr = new global.XMLHttpRequest()
			try {
				xhr.open(self._opts.method, self._opts.url, true)
			} catch (err) {
				process.nextTick(function () {
					self.emit('error', err)
				})
				return
			}

			// Can't set responseType on really old browsers
			if ('responseType' in xhr)
				xhr.responseType = self._mode.split(':')[0]

			if ('withCredentials' in xhr)
				xhr.withCredentials = !!opts.withCredentials

			if (self._mode === 'text' && 'overrideMimeType' in xhr)
				xhr.overrideMimeType('text/plain; charset=x-user-defined')

			if ('requestTimeout' in opts) {
				xhr.timeout = opts.requestTimeout
				xhr.ontimeout = function () {
					self.emit('requestTimeout')
				}
			}

			headersList.forEach(function (header) {
				xhr.setRequestHeader(header[0], header[1])
			})

			self._response = null
			xhr.onreadystatechange = function () {
				switch (xhr.readyState) {
					case rStates.LOADING:
					case rStates.DONE:
						self._onXHRProgress()
						break
				}
			}
			// Necessary for streaming in Firefox, since xhr.response is ONLY defined
			// in onprogress, not in onreadystatechange with xhr.readyState = 3
			if (self._mode === 'moz-chunked-arraybuffer') {
				xhr.onprogress = function () {
					self._onXHRProgress()
				}
			}

			xhr.onerror = function () {
				if (self._destroyed)
					return
				self.emit('error', new Error('XHR error'))
			}

			try {
				xhr.send(body)
			} catch (err) {
				process.nextTick(function () {
					self.emit('error', err)
				})
				return
			}
		}
	}

	/**
	 * Checks if xhr.status is readable and non-zero, indicating no error.
	 * Even though the spec says it should be available in readyState 3,
	 * accessing it throws an exception in IE8
	 */
	function statusValid (xhr) {
		try {
			var status = xhr.status
			return (status !== null && status !== 0)
		} catch (e) {
			return false
		}
	}

	ClientRequest.prototype._onXHRProgress = function () {
		var self = this

		if (!statusValid(self._xhr) || self._destroyed)
			return

		if (!self._response)
			self._connect()

		self._response._onXHRProgress()
	}

	ClientRequest.prototype._connect = function () {
		var self = this

		if (self._destroyed)
			return

		self._response = new IncomingMessage(self._xhr, self._fetchResponse, self._mode, self._fetchTimer)
		self._response.on('error', function(err) {
			self.emit('error', err)
		})

		self.emit('response', self._response)
	}

	ClientRequest.prototype._write = function (chunk, encoding, cb) {
		var self = this

		self._body.push(chunk)
		cb()
	}

	ClientRequest.prototype.abort = ClientRequest.prototype.destroy = function () {
		var self = this
		self._destroyed = true
		global.clearTimeout(self._fetchTimer)
		if (self._response)
			self._response._destroyed = true
		if (self._xhr)
			self._xhr.abort()
		else if (self._fetchAbortController)
			self._fetchAbortController.abort()
	}

	ClientRequest.prototype.end = function (data, encoding, cb) {
		var self = this
		if (typeof data === 'function') {
			cb = data
			data = undefined
		}

		stream.Writable.prototype.end.call(self, data, encoding, cb)
	}

	ClientRequest.prototype.flushHeaders = function () {}
	ClientRequest.prototype.setTimeout = function () {}
	ClientRequest.prototype.setNoDelay = function () {}
	ClientRequest.prototype.setSocketKeepAlive = function () {}

	// Taken from http://www.w3.org/TR/XMLHttpRequest/#the-setrequestheader%28%29-method
	var unsafeHeaders = [
		'accept-charset',
		'accept-encoding',
		'access-control-request-headers',
		'access-control-request-method',
		'connection',
		'content-length',
		'cookie',
		'cookie2',
		'date',
		'dnt',
		'expect',
		'host',
		'keep-alive',
		'origin',
		'referer',
		'te',
		'trailer',
		'transfer-encoding',
		'upgrade',
		'via'
	]

	/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(333).Buffer, (function() { return this; }()), __webpack_require__(328)))

/***/ }),
/* 333 */
/***/ (function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(global) {/*!
	 * The buffer module from node.js, for the browser.
	 *
	 * @author   Feross Aboukhadijeh <feross@feross.org> <http://feross.org>
	 * @license  MIT
	 */
	/* eslint-disable no-proto */

	'use strict'

	var base64 = __webpack_require__(334)
	var ieee754 = __webpack_require__(335)
	var isArray = __webpack_require__(336)

	exports.Buffer = Buffer
	exports.SlowBuffer = SlowBuffer
	exports.INSPECT_MAX_BYTES = 50

	/**
	 * If `Buffer.TYPED_ARRAY_SUPPORT`:
	 *   === true    Use Uint8Array implementation (fastest)
	 *   === false   Use Object implementation (most compatible, even IE6)
	 *
	 * Browsers that support typed arrays are IE 10+, Firefox 4+, Chrome 7+, Safari 5.1+,
	 * Opera 11.6+, iOS 4.2+.
	 *
	 * Due to various browser bugs, sometimes the Object implementation will be used even
	 * when the browser supports typed arrays.
	 *
	 * Note:
	 *
	 *   - Firefox 4-29 lacks support for adding new properties to `Uint8Array` instances,
	 *     See: https://bugzilla.mozilla.org/show_bug.cgi?id=695438.
	 *
	 *   - Chrome 9-10 is missing the `TypedArray.prototype.subarray` function.
	 *
	 *   - IE10 has a broken `TypedArray.prototype.subarray` function which returns arrays of
	 *     incorrect length in some situations.

	 * We detect these buggy browsers and set `Buffer.TYPED_ARRAY_SUPPORT` to `false` so they
	 * get the Object implementation, which is slower but behaves correctly.
	 */
	Buffer.TYPED_ARRAY_SUPPORT = global.TYPED_ARRAY_SUPPORT !== undefined
	  ? global.TYPED_ARRAY_SUPPORT
	  : typedArraySupport()

	/*
	 * Export kMaxLength after typed array support is determined.
	 */
	exports.kMaxLength = kMaxLength()

	function typedArraySupport () {
	  try {
	    var arr = new Uint8Array(1)
	    arr.__proto__ = {__proto__: Uint8Array.prototype, foo: function () { return 42 }}
	    return arr.foo() === 42 && // typed array instances can be augmented
	        typeof arr.subarray === 'function' && // chrome 9-10 lack `subarray`
	        arr.subarray(1, 1).byteLength === 0 // ie10 has broken `subarray`
	  } catch (e) {
	    return false
	  }
	}

	function kMaxLength () {
	  return Buffer.TYPED_ARRAY_SUPPORT
	    ? 0x7fffffff
	    : 0x3fffffff
	}

	function createBuffer (that, length) {
	  if (kMaxLength() < length) {
	    throw new RangeError('Invalid typed array length')
	  }
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    // Return an augmented `Uint8Array` instance, for best performance
	    that = new Uint8Array(length)
	    that.__proto__ = Buffer.prototype
	  } else {
	    // Fallback: Return an object instance of the Buffer class
	    if (that === null) {
	      that = new Buffer(length)
	    }
	    that.length = length
	  }

	  return that
	}

	/**
	 * The Buffer constructor returns instances of `Uint8Array` that have their
	 * prototype changed to `Buffer.prototype`. Furthermore, `Buffer` is a subclass of
	 * `Uint8Array`, so the returned instances will have all the node `Buffer` methods
	 * and the `Uint8Array` methods. Square bracket notation works as expected -- it
	 * returns a single octet.
	 *
	 * The `Uint8Array` prototype remains unmodified.
	 */

	function Buffer (arg, encodingOrOffset, length) {
	  if (!Buffer.TYPED_ARRAY_SUPPORT && !(this instanceof Buffer)) {
	    return new Buffer(arg, encodingOrOffset, length)
	  }

	  // Common case.
	  if (typeof arg === 'number') {
	    if (typeof encodingOrOffset === 'string') {
	      throw new Error(
	        'If encoding is specified then the first argument must be a string'
	      )
	    }
	    return allocUnsafe(this, arg)
	  }
	  return from(this, arg, encodingOrOffset, length)
	}

	Buffer.poolSize = 8192 // not used by this implementation

	// TODO: Legacy, not needed anymore. Remove in next major version.
	Buffer._augment = function (arr) {
	  arr.__proto__ = Buffer.prototype
	  return arr
	}

	function from (that, value, encodingOrOffset, length) {
	  if (typeof value === 'number') {
	    throw new TypeError('"value" argument must not be a number')
	  }

	  if (typeof ArrayBuffer !== 'undefined' && value instanceof ArrayBuffer) {
	    return fromArrayBuffer(that, value, encodingOrOffset, length)
	  }

	  if (typeof value === 'string') {
	    return fromString(that, value, encodingOrOffset)
	  }

	  return fromObject(that, value)
	}

	/**
	 * Functionally equivalent to Buffer(arg, encoding) but throws a TypeError
	 * if value is a number.
	 * Buffer.from(str[, encoding])
	 * Buffer.from(array)
	 * Buffer.from(buffer)
	 * Buffer.from(arrayBuffer[, byteOffset[, length]])
	 **/
	Buffer.from = function (value, encodingOrOffset, length) {
	  return from(null, value, encodingOrOffset, length)
	}

	if (Buffer.TYPED_ARRAY_SUPPORT) {
	  Buffer.prototype.__proto__ = Uint8Array.prototype
	  Buffer.__proto__ = Uint8Array
	  if (typeof Symbol !== 'undefined' && Symbol.species &&
	      Buffer[Symbol.species] === Buffer) {
	    // Fix subarray() in ES2016. See: https://github.com/feross/buffer/pull/97
	    Object.defineProperty(Buffer, Symbol.species, {
	      value: null,
	      configurable: true
	    })
	  }
	}

	function assertSize (size) {
	  if (typeof size !== 'number') {
	    throw new TypeError('"size" argument must be a number')
	  } else if (size < 0) {
	    throw new RangeError('"size" argument must not be negative')
	  }
	}

	function alloc (that, size, fill, encoding) {
	  assertSize(size)
	  if (size <= 0) {
	    return createBuffer(that, size)
	  }
	  if (fill !== undefined) {
	    // Only pay attention to encoding if it's a string. This
	    // prevents accidentally sending in a number that would
	    // be interpretted as a start offset.
	    return typeof encoding === 'string'
	      ? createBuffer(that, size).fill(fill, encoding)
	      : createBuffer(that, size).fill(fill)
	  }
	  return createBuffer(that, size)
	}

	/**
	 * Creates a new filled Buffer instance.
	 * alloc(size[, fill[, encoding]])
	 **/
	Buffer.alloc = function (size, fill, encoding) {
	  return alloc(null, size, fill, encoding)
	}

	function allocUnsafe (that, size) {
	  assertSize(size)
	  that = createBuffer(that, size < 0 ? 0 : checked(size) | 0)
	  if (!Buffer.TYPED_ARRAY_SUPPORT) {
	    for (var i = 0; i < size; ++i) {
	      that[i] = 0
	    }
	  }
	  return that
	}

	/**
	 * Equivalent to Buffer(num), by default creates a non-zero-filled Buffer instance.
	 * */
	Buffer.allocUnsafe = function (size) {
	  return allocUnsafe(null, size)
	}
	/**
	 * Equivalent to SlowBuffer(num), by default creates a non-zero-filled Buffer instance.
	 */
	Buffer.allocUnsafeSlow = function (size) {
	  return allocUnsafe(null, size)
	}

	function fromString (that, string, encoding) {
	  if (typeof encoding !== 'string' || encoding === '') {
	    encoding = 'utf8'
	  }

	  if (!Buffer.isEncoding(encoding)) {
	    throw new TypeError('"encoding" must be a valid string encoding')
	  }

	  var length = byteLength(string, encoding) | 0
	  that = createBuffer(that, length)

	  var actual = that.write(string, encoding)

	  if (actual !== length) {
	    // Writing a hex string, for example, that contains invalid characters will
	    // cause everything after the first invalid character to be ignored. (e.g.
	    // 'abxxcd' will be treated as 'ab')
	    that = that.slice(0, actual)
	  }

	  return that
	}

	function fromArrayLike (that, array) {
	  var length = array.length < 0 ? 0 : checked(array.length) | 0
	  that = createBuffer(that, length)
	  for (var i = 0; i < length; i += 1) {
	    that[i] = array[i] & 255
	  }
	  return that
	}

	function fromArrayBuffer (that, array, byteOffset, length) {
	  array.byteLength // this throws if `array` is not a valid ArrayBuffer

	  if (byteOffset < 0 || array.byteLength < byteOffset) {
	    throw new RangeError('\'offset\' is out of bounds')
	  }

	  if (array.byteLength < byteOffset + (length || 0)) {
	    throw new RangeError('\'length\' is out of bounds')
	  }

	  if (byteOffset === undefined && length === undefined) {
	    array = new Uint8Array(array)
	  } else if (length === undefined) {
	    array = new Uint8Array(array, byteOffset)
	  } else {
	    array = new Uint8Array(array, byteOffset, length)
	  }

	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    // Return an augmented `Uint8Array` instance, for best performance
	    that = array
	    that.__proto__ = Buffer.prototype
	  } else {
	    // Fallback: Return an object instance of the Buffer class
	    that = fromArrayLike(that, array)
	  }
	  return that
	}

	function fromObject (that, obj) {
	  if (Buffer.isBuffer(obj)) {
	    var len = checked(obj.length) | 0
	    that = createBuffer(that, len)

	    if (that.length === 0) {
	      return that
	    }

	    obj.copy(that, 0, 0, len)
	    return that
	  }

	  if (obj) {
	    if ((typeof ArrayBuffer !== 'undefined' &&
	        obj.buffer instanceof ArrayBuffer) || 'length' in obj) {
	      if (typeof obj.length !== 'number' || isnan(obj.length)) {
	        return createBuffer(that, 0)
	      }
	      return fromArrayLike(that, obj)
	    }

	    if (obj.type === 'Buffer' && isArray(obj.data)) {
	      return fromArrayLike(that, obj.data)
	    }
	  }

	  throw new TypeError('First argument must be a string, Buffer, ArrayBuffer, Array, or array-like object.')
	}

	function checked (length) {
	  // Note: cannot use `length < kMaxLength()` here because that fails when
	  // length is NaN (which is otherwise coerced to zero.)
	  if (length >= kMaxLength()) {
	    throw new RangeError('Attempt to allocate Buffer larger than maximum ' +
	                         'size: 0x' + kMaxLength().toString(16) + ' bytes')
	  }
	  return length | 0
	}

	function SlowBuffer (length) {
	  if (+length != length) { // eslint-disable-line eqeqeq
	    length = 0
	  }
	  return Buffer.alloc(+length)
	}

	Buffer.isBuffer = function isBuffer (b) {
	  return !!(b != null && b._isBuffer)
	}

	Buffer.compare = function compare (a, b) {
	  if (!Buffer.isBuffer(a) || !Buffer.isBuffer(b)) {
	    throw new TypeError('Arguments must be Buffers')
	  }

	  if (a === b) return 0

	  var x = a.length
	  var y = b.length

	  for (var i = 0, len = Math.min(x, y); i < len; ++i) {
	    if (a[i] !== b[i]) {
	      x = a[i]
	      y = b[i]
	      break
	    }
	  }

	  if (x < y) return -1
	  if (y < x) return 1
	  return 0
	}

	Buffer.isEncoding = function isEncoding (encoding) {
	  switch (String(encoding).toLowerCase()) {
	    case 'hex':
	    case 'utf8':
	    case 'utf-8':
	    case 'ascii':
	    case 'latin1':
	    case 'binary':
	    case 'base64':
	    case 'ucs2':
	    case 'ucs-2':
	    case 'utf16le':
	    case 'utf-16le':
	      return true
	    default:
	      return false
	  }
	}

	Buffer.concat = function concat (list, length) {
	  if (!isArray(list)) {
	    throw new TypeError('"list" argument must be an Array of Buffers')
	  }

	  if (list.length === 0) {
	    return Buffer.alloc(0)
	  }

	  var i
	  if (length === undefined) {
	    length = 0
	    for (i = 0; i < list.length; ++i) {
	      length += list[i].length
	    }
	  }

	  var buffer = Buffer.allocUnsafe(length)
	  var pos = 0
	  for (i = 0; i < list.length; ++i) {
	    var buf = list[i]
	    if (!Buffer.isBuffer(buf)) {
	      throw new TypeError('"list" argument must be an Array of Buffers')
	    }
	    buf.copy(buffer, pos)
	    pos += buf.length
	  }
	  return buffer
	}

	function byteLength (string, encoding) {
	  if (Buffer.isBuffer(string)) {
	    return string.length
	  }
	  if (typeof ArrayBuffer !== 'undefined' && typeof ArrayBuffer.isView === 'function' &&
	      (ArrayBuffer.isView(string) || string instanceof ArrayBuffer)) {
	    return string.byteLength
	  }
	  if (typeof string !== 'string') {
	    string = '' + string
	  }

	  var len = string.length
	  if (len === 0) return 0

	  // Use a for loop to avoid recursion
	  var loweredCase = false
	  for (;;) {
	    switch (encoding) {
	      case 'ascii':
	      case 'latin1':
	      case 'binary':
	        return len
	      case 'utf8':
	      case 'utf-8':
	      case undefined:
	        return utf8ToBytes(string).length
	      case 'ucs2':
	      case 'ucs-2':
	      case 'utf16le':
	      case 'utf-16le':
	        return len * 2
	      case 'hex':
	        return len >>> 1
	      case 'base64':
	        return base64ToBytes(string).length
	      default:
	        if (loweredCase) return utf8ToBytes(string).length // assume utf8
	        encoding = ('' + encoding).toLowerCase()
	        loweredCase = true
	    }
	  }
	}
	Buffer.byteLength = byteLength

	function slowToString (encoding, start, end) {
	  var loweredCase = false

	  // No need to verify that "this.length <= MAX_UINT32" since it's a read-only
	  // property of a typed array.

	  // This behaves neither like String nor Uint8Array in that we set start/end
	  // to their upper/lower bounds if the value passed is out of range.
	  // undefined is handled specially as per ECMA-262 6th Edition,
	  // Section 13.3.3.7 Runtime Semantics: KeyedBindingInitialization.
	  if (start === undefined || start < 0) {
	    start = 0
	  }
	  // Return early if start > this.length. Done here to prevent potential uint32
	  // coercion fail below.
	  if (start > this.length) {
	    return ''
	  }

	  if (end === undefined || end > this.length) {
	    end = this.length
	  }

	  if (end <= 0) {
	    return ''
	  }

	  // Force coersion to uint32. This will also coerce falsey/NaN values to 0.
	  end >>>= 0
	  start >>>= 0

	  if (end <= start) {
	    return ''
	  }

	  if (!encoding) encoding = 'utf8'

	  while (true) {
	    switch (encoding) {
	      case 'hex':
	        return hexSlice(this, start, end)

	      case 'utf8':
	      case 'utf-8':
	        return utf8Slice(this, start, end)

	      case 'ascii':
	        return asciiSlice(this, start, end)

	      case 'latin1':
	      case 'binary':
	        return latin1Slice(this, start, end)

	      case 'base64':
	        return base64Slice(this, start, end)

	      case 'ucs2':
	      case 'ucs-2':
	      case 'utf16le':
	      case 'utf-16le':
	        return utf16leSlice(this, start, end)

	      default:
	        if (loweredCase) throw new TypeError('Unknown encoding: ' + encoding)
	        encoding = (encoding + '').toLowerCase()
	        loweredCase = true
	    }
	  }
	}

	// The property is used by `Buffer.isBuffer` and `is-buffer` (in Safari 5-7) to detect
	// Buffer instances.
	Buffer.prototype._isBuffer = true

	function swap (b, n, m) {
	  var i = b[n]
	  b[n] = b[m]
	  b[m] = i
	}

	Buffer.prototype.swap16 = function swap16 () {
	  var len = this.length
	  if (len % 2 !== 0) {
	    throw new RangeError('Buffer size must be a multiple of 16-bits')
	  }
	  for (var i = 0; i < len; i += 2) {
	    swap(this, i, i + 1)
	  }
	  return this
	}

	Buffer.prototype.swap32 = function swap32 () {
	  var len = this.length
	  if (len % 4 !== 0) {
	    throw new RangeError('Buffer size must be a multiple of 32-bits')
	  }
	  for (var i = 0; i < len; i += 4) {
	    swap(this, i, i + 3)
	    swap(this, i + 1, i + 2)
	  }
	  return this
	}

	Buffer.prototype.swap64 = function swap64 () {
	  var len = this.length
	  if (len % 8 !== 0) {
	    throw new RangeError('Buffer size must be a multiple of 64-bits')
	  }
	  for (var i = 0; i < len; i += 8) {
	    swap(this, i, i + 7)
	    swap(this, i + 1, i + 6)
	    swap(this, i + 2, i + 5)
	    swap(this, i + 3, i + 4)
	  }
	  return this
	}

	Buffer.prototype.toString = function toString () {
	  var length = this.length | 0
	  if (length === 0) return ''
	  if (arguments.length === 0) return utf8Slice(this, 0, length)
	  return slowToString.apply(this, arguments)
	}

	Buffer.prototype.equals = function equals (b) {
	  if (!Buffer.isBuffer(b)) throw new TypeError('Argument must be a Buffer')
	  if (this === b) return true
	  return Buffer.compare(this, b) === 0
	}

	Buffer.prototype.inspect = function inspect () {
	  var str = ''
	  var max = exports.INSPECT_MAX_BYTES
	  if (this.length > 0) {
	    str = this.toString('hex', 0, max).match(/.{2}/g).join(' ')
	    if (this.length > max) str += ' ... '
	  }
	  return '<Buffer ' + str + '>'
	}

	Buffer.prototype.compare = function compare (target, start, end, thisStart, thisEnd) {
	  if (!Buffer.isBuffer(target)) {
	    throw new TypeError('Argument must be a Buffer')
	  }

	  if (start === undefined) {
	    start = 0
	  }
	  if (end === undefined) {
	    end = target ? target.length : 0
	  }
	  if (thisStart === undefined) {
	    thisStart = 0
	  }
	  if (thisEnd === undefined) {
	    thisEnd = this.length
	  }

	  if (start < 0 || end > target.length || thisStart < 0 || thisEnd > this.length) {
	    throw new RangeError('out of range index')
	  }

	  if (thisStart >= thisEnd && start >= end) {
	    return 0
	  }
	  if (thisStart >= thisEnd) {
	    return -1
	  }
	  if (start >= end) {
	    return 1
	  }

	  start >>>= 0
	  end >>>= 0
	  thisStart >>>= 0
	  thisEnd >>>= 0

	  if (this === target) return 0

	  var x = thisEnd - thisStart
	  var y = end - start
	  var len = Math.min(x, y)

	  var thisCopy = this.slice(thisStart, thisEnd)
	  var targetCopy = target.slice(start, end)

	  for (var i = 0; i < len; ++i) {
	    if (thisCopy[i] !== targetCopy[i]) {
	      x = thisCopy[i]
	      y = targetCopy[i]
	      break
	    }
	  }

	  if (x < y) return -1
	  if (y < x) return 1
	  return 0
	}

	// Finds either the first index of `val` in `buffer` at offset >= `byteOffset`,
	// OR the last index of `val` in `buffer` at offset <= `byteOffset`.
	//
	// Arguments:
	// - buffer - a Buffer to search
	// - val - a string, Buffer, or number
	// - byteOffset - an index into `buffer`; will be clamped to an int32
	// - encoding - an optional encoding, relevant is val is a string
	// - dir - true for indexOf, false for lastIndexOf
	function bidirectionalIndexOf (buffer, val, byteOffset, encoding, dir) {
	  // Empty buffer means no match
	  if (buffer.length === 0) return -1

	  // Normalize byteOffset
	  if (typeof byteOffset === 'string') {
	    encoding = byteOffset
	    byteOffset = 0
	  } else if (byteOffset > 0x7fffffff) {
	    byteOffset = 0x7fffffff
	  } else if (byteOffset < -0x80000000) {
	    byteOffset = -0x80000000
	  }
	  byteOffset = +byteOffset  // Coerce to Number.
	  if (isNaN(byteOffset)) {
	    // byteOffset: it it's undefined, null, NaN, "foo", etc, search whole buffer
	    byteOffset = dir ? 0 : (buffer.length - 1)
	  }

	  // Normalize byteOffset: negative offsets start from the end of the buffer
	  if (byteOffset < 0) byteOffset = buffer.length + byteOffset
	  if (byteOffset >= buffer.length) {
	    if (dir) return -1
	    else byteOffset = buffer.length - 1
	  } else if (byteOffset < 0) {
	    if (dir) byteOffset = 0
	    else return -1
	  }

	  // Normalize val
	  if (typeof val === 'string') {
	    val = Buffer.from(val, encoding)
	  }

	  // Finally, search either indexOf (if dir is true) or lastIndexOf
	  if (Buffer.isBuffer(val)) {
	    // Special case: looking for empty string/buffer always fails
	    if (val.length === 0) {
	      return -1
	    }
	    return arrayIndexOf(buffer, val, byteOffset, encoding, dir)
	  } else if (typeof val === 'number') {
	    val = val & 0xFF // Search for a byte value [0-255]
	    if (Buffer.TYPED_ARRAY_SUPPORT &&
	        typeof Uint8Array.prototype.indexOf === 'function') {
	      if (dir) {
	        return Uint8Array.prototype.indexOf.call(buffer, val, byteOffset)
	      } else {
	        return Uint8Array.prototype.lastIndexOf.call(buffer, val, byteOffset)
	      }
	    }
	    return arrayIndexOf(buffer, [ val ], byteOffset, encoding, dir)
	  }

	  throw new TypeError('val must be string, number or Buffer')
	}

	function arrayIndexOf (arr, val, byteOffset, encoding, dir) {
	  var indexSize = 1
	  var arrLength = arr.length
	  var valLength = val.length

	  if (encoding !== undefined) {
	    encoding = String(encoding).toLowerCase()
	    if (encoding === 'ucs2' || encoding === 'ucs-2' ||
	        encoding === 'utf16le' || encoding === 'utf-16le') {
	      if (arr.length < 2 || val.length < 2) {
	        return -1
	      }
	      indexSize = 2
	      arrLength /= 2
	      valLength /= 2
	      byteOffset /= 2
	    }
	  }

	  function read (buf, i) {
	    if (indexSize === 1) {
	      return buf[i]
	    } else {
	      return buf.readUInt16BE(i * indexSize)
	    }
	  }

	  var i
	  if (dir) {
	    var foundIndex = -1
	    for (i = byteOffset; i < arrLength; i++) {
	      if (read(arr, i) === read(val, foundIndex === -1 ? 0 : i - foundIndex)) {
	        if (foundIndex === -1) foundIndex = i
	        if (i - foundIndex + 1 === valLength) return foundIndex * indexSize
	      } else {
	        if (foundIndex !== -1) i -= i - foundIndex
	        foundIndex = -1
	      }
	    }
	  } else {
	    if (byteOffset + valLength > arrLength) byteOffset = arrLength - valLength
	    for (i = byteOffset; i >= 0; i--) {
	      var found = true
	      for (var j = 0; j < valLength; j++) {
	        if (read(arr, i + j) !== read(val, j)) {
	          found = false
	          break
	        }
	      }
	      if (found) return i
	    }
	  }

	  return -1
	}

	Buffer.prototype.includes = function includes (val, byteOffset, encoding) {
	  return this.indexOf(val, byteOffset, encoding) !== -1
	}

	Buffer.prototype.indexOf = function indexOf (val, byteOffset, encoding) {
	  return bidirectionalIndexOf(this, val, byteOffset, encoding, true)
	}

	Buffer.prototype.lastIndexOf = function lastIndexOf (val, byteOffset, encoding) {
	  return bidirectionalIndexOf(this, val, byteOffset, encoding, false)
	}

	function hexWrite (buf, string, offset, length) {
	  offset = Number(offset) || 0
	  var remaining = buf.length - offset
	  if (!length) {
	    length = remaining
	  } else {
	    length = Number(length)
	    if (length > remaining) {
	      length = remaining
	    }
	  }

	  // must be an even number of digits
	  var strLen = string.length
	  if (strLen % 2 !== 0) throw new TypeError('Invalid hex string')

	  if (length > strLen / 2) {
	    length = strLen / 2
	  }
	  for (var i = 0; i < length; ++i) {
	    var parsed = parseInt(string.substr(i * 2, 2), 16)
	    if (isNaN(parsed)) return i
	    buf[offset + i] = parsed
	  }
	  return i
	}

	function utf8Write (buf, string, offset, length) {
	  return blitBuffer(utf8ToBytes(string, buf.length - offset), buf, offset, length)
	}

	function asciiWrite (buf, string, offset, length) {
	  return blitBuffer(asciiToBytes(string), buf, offset, length)
	}

	function latin1Write (buf, string, offset, length) {
	  return asciiWrite(buf, string, offset, length)
	}

	function base64Write (buf, string, offset, length) {
	  return blitBuffer(base64ToBytes(string), buf, offset, length)
	}

	function ucs2Write (buf, string, offset, length) {
	  return blitBuffer(utf16leToBytes(string, buf.length - offset), buf, offset, length)
	}

	Buffer.prototype.write = function write (string, offset, length, encoding) {
	  // Buffer#write(string)
	  if (offset === undefined) {
	    encoding = 'utf8'
	    length = this.length
	    offset = 0
	  // Buffer#write(string, encoding)
	  } else if (length === undefined && typeof offset === 'string') {
	    encoding = offset
	    length = this.length
	    offset = 0
	  // Buffer#write(string, offset[, length][, encoding])
	  } else if (isFinite(offset)) {
	    offset = offset | 0
	    if (isFinite(length)) {
	      length = length | 0
	      if (encoding === undefined) encoding = 'utf8'
	    } else {
	      encoding = length
	      length = undefined
	    }
	  // legacy write(string, encoding, offset, length) - remove in v0.13
	  } else {
	    throw new Error(
	      'Buffer.write(string, encoding, offset[, length]) is no longer supported'
	    )
	  }

	  var remaining = this.length - offset
	  if (length === undefined || length > remaining) length = remaining

	  if ((string.length > 0 && (length < 0 || offset < 0)) || offset > this.length) {
	    throw new RangeError('Attempt to write outside buffer bounds')
	  }

	  if (!encoding) encoding = 'utf8'

	  var loweredCase = false
	  for (;;) {
	    switch (encoding) {
	      case 'hex':
	        return hexWrite(this, string, offset, length)

	      case 'utf8':
	      case 'utf-8':
	        return utf8Write(this, string, offset, length)

	      case 'ascii':
	        return asciiWrite(this, string, offset, length)

	      case 'latin1':
	      case 'binary':
	        return latin1Write(this, string, offset, length)

	      case 'base64':
	        // Warning: maxLength not taken into account in base64Write
	        return base64Write(this, string, offset, length)

	      case 'ucs2':
	      case 'ucs-2':
	      case 'utf16le':
	      case 'utf-16le':
	        return ucs2Write(this, string, offset, length)

	      default:
	        if (loweredCase) throw new TypeError('Unknown encoding: ' + encoding)
	        encoding = ('' + encoding).toLowerCase()
	        loweredCase = true
	    }
	  }
	}

	Buffer.prototype.toJSON = function toJSON () {
	  return {
	    type: 'Buffer',
	    data: Array.prototype.slice.call(this._arr || this, 0)
	  }
	}

	function base64Slice (buf, start, end) {
	  if (start === 0 && end === buf.length) {
	    return base64.fromByteArray(buf)
	  } else {
	    return base64.fromByteArray(buf.slice(start, end))
	  }
	}

	function utf8Slice (buf, start, end) {
	  end = Math.min(buf.length, end)
	  var res = []

	  var i = start
	  while (i < end) {
	    var firstByte = buf[i]
	    var codePoint = null
	    var bytesPerSequence = (firstByte > 0xEF) ? 4
	      : (firstByte > 0xDF) ? 3
	      : (firstByte > 0xBF) ? 2
	      : 1

	    if (i + bytesPerSequence <= end) {
	      var secondByte, thirdByte, fourthByte, tempCodePoint

	      switch (bytesPerSequence) {
	        case 1:
	          if (firstByte < 0x80) {
	            codePoint = firstByte
	          }
	          break
	        case 2:
	          secondByte = buf[i + 1]
	          if ((secondByte & 0xC0) === 0x80) {
	            tempCodePoint = (firstByte & 0x1F) << 0x6 | (secondByte & 0x3F)
	            if (tempCodePoint > 0x7F) {
	              codePoint = tempCodePoint
	            }
	          }
	          break
	        case 3:
	          secondByte = buf[i + 1]
	          thirdByte = buf[i + 2]
	          if ((secondByte & 0xC0) === 0x80 && (thirdByte & 0xC0) === 0x80) {
	            tempCodePoint = (firstByte & 0xF) << 0xC | (secondByte & 0x3F) << 0x6 | (thirdByte & 0x3F)
	            if (tempCodePoint > 0x7FF && (tempCodePoint < 0xD800 || tempCodePoint > 0xDFFF)) {
	              codePoint = tempCodePoint
	            }
	          }
	          break
	        case 4:
	          secondByte = buf[i + 1]
	          thirdByte = buf[i + 2]
	          fourthByte = buf[i + 3]
	          if ((secondByte & 0xC0) === 0x80 && (thirdByte & 0xC0) === 0x80 && (fourthByte & 0xC0) === 0x80) {
	            tempCodePoint = (firstByte & 0xF) << 0x12 | (secondByte & 0x3F) << 0xC | (thirdByte & 0x3F) << 0x6 | (fourthByte & 0x3F)
	            if (tempCodePoint > 0xFFFF && tempCodePoint < 0x110000) {
	              codePoint = tempCodePoint
	            }
	          }
	      }
	    }

	    if (codePoint === null) {
	      // we did not generate a valid codePoint so insert a
	      // replacement char (U+FFFD) and advance only 1 byte
	      codePoint = 0xFFFD
	      bytesPerSequence = 1
	    } else if (codePoint > 0xFFFF) {
	      // encode to utf16 (surrogate pair dance)
	      codePoint -= 0x10000
	      res.push(codePoint >>> 10 & 0x3FF | 0xD800)
	      codePoint = 0xDC00 | codePoint & 0x3FF
	    }

	    res.push(codePoint)
	    i += bytesPerSequence
	  }

	  return decodeCodePointsArray(res)
	}

	// Based on http://stackoverflow.com/a/22747272/680742, the browser with
	// the lowest limit is Chrome, with 0x10000 args.
	// We go 1 magnitude less, for safety
	var MAX_ARGUMENTS_LENGTH = 0x1000

	function decodeCodePointsArray (codePoints) {
	  var len = codePoints.length
	  if (len <= MAX_ARGUMENTS_LENGTH) {
	    return String.fromCharCode.apply(String, codePoints) // avoid extra slice()
	  }

	  // Decode in chunks to avoid "call stack size exceeded".
	  var res = ''
	  var i = 0
	  while (i < len) {
	    res += String.fromCharCode.apply(
	      String,
	      codePoints.slice(i, i += MAX_ARGUMENTS_LENGTH)
	    )
	  }
	  return res
	}

	function asciiSlice (buf, start, end) {
	  var ret = ''
	  end = Math.min(buf.length, end)

	  for (var i = start; i < end; ++i) {
	    ret += String.fromCharCode(buf[i] & 0x7F)
	  }
	  return ret
	}

	function latin1Slice (buf, start, end) {
	  var ret = ''
	  end = Math.min(buf.length, end)

	  for (var i = start; i < end; ++i) {
	    ret += String.fromCharCode(buf[i])
	  }
	  return ret
	}

	function hexSlice (buf, start, end) {
	  var len = buf.length

	  if (!start || start < 0) start = 0
	  if (!end || end < 0 || end > len) end = len

	  var out = ''
	  for (var i = start; i < end; ++i) {
	    out += toHex(buf[i])
	  }
	  return out
	}

	function utf16leSlice (buf, start, end) {
	  var bytes = buf.slice(start, end)
	  var res = ''
	  for (var i = 0; i < bytes.length; i += 2) {
	    res += String.fromCharCode(bytes[i] + bytes[i + 1] * 256)
	  }
	  return res
	}

	Buffer.prototype.slice = function slice (start, end) {
	  var len = this.length
	  start = ~~start
	  end = end === undefined ? len : ~~end

	  if (start < 0) {
	    start += len
	    if (start < 0) start = 0
	  } else if (start > len) {
	    start = len
	  }

	  if (end < 0) {
	    end += len
	    if (end < 0) end = 0
	  } else if (end > len) {
	    end = len
	  }

	  if (end < start) end = start

	  var newBuf
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    newBuf = this.subarray(start, end)
	    newBuf.__proto__ = Buffer.prototype
	  } else {
	    var sliceLen = end - start
	    newBuf = new Buffer(sliceLen, undefined)
	    for (var i = 0; i < sliceLen; ++i) {
	      newBuf[i] = this[i + start]
	    }
	  }

	  return newBuf
	}

	/*
	 * Need to make sure that buffer isn't trying to write out of bounds.
	 */
	function checkOffset (offset, ext, length) {
	  if ((offset % 1) !== 0 || offset < 0) throw new RangeError('offset is not uint')
	  if (offset + ext > length) throw new RangeError('Trying to access beyond buffer length')
	}

	Buffer.prototype.readUIntLE = function readUIntLE (offset, byteLength, noAssert) {
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) checkOffset(offset, byteLength, this.length)

	  var val = this[offset]
	  var mul = 1
	  var i = 0
	  while (++i < byteLength && (mul *= 0x100)) {
	    val += this[offset + i] * mul
	  }

	  return val
	}

	Buffer.prototype.readUIntBE = function readUIntBE (offset, byteLength, noAssert) {
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) {
	    checkOffset(offset, byteLength, this.length)
	  }

	  var val = this[offset + --byteLength]
	  var mul = 1
	  while (byteLength > 0 && (mul *= 0x100)) {
	    val += this[offset + --byteLength] * mul
	  }

	  return val
	}

	Buffer.prototype.readUInt8 = function readUInt8 (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 1, this.length)
	  return this[offset]
	}

	Buffer.prototype.readUInt16LE = function readUInt16LE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 2, this.length)
	  return this[offset] | (this[offset + 1] << 8)
	}

	Buffer.prototype.readUInt16BE = function readUInt16BE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 2, this.length)
	  return (this[offset] << 8) | this[offset + 1]
	}

	Buffer.prototype.readUInt32LE = function readUInt32LE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)

	  return ((this[offset]) |
	      (this[offset + 1] << 8) |
	      (this[offset + 2] << 16)) +
	      (this[offset + 3] * 0x1000000)
	}

	Buffer.prototype.readUInt32BE = function readUInt32BE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)

	  return (this[offset] * 0x1000000) +
	    ((this[offset + 1] << 16) |
	    (this[offset + 2] << 8) |
	    this[offset + 3])
	}

	Buffer.prototype.readIntLE = function readIntLE (offset, byteLength, noAssert) {
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) checkOffset(offset, byteLength, this.length)

	  var val = this[offset]
	  var mul = 1
	  var i = 0
	  while (++i < byteLength && (mul *= 0x100)) {
	    val += this[offset + i] * mul
	  }
	  mul *= 0x80

	  if (val >= mul) val -= Math.pow(2, 8 * byteLength)

	  return val
	}

	Buffer.prototype.readIntBE = function readIntBE (offset, byteLength, noAssert) {
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) checkOffset(offset, byteLength, this.length)

	  var i = byteLength
	  var mul = 1
	  var val = this[offset + --i]
	  while (i > 0 && (mul *= 0x100)) {
	    val += this[offset + --i] * mul
	  }
	  mul *= 0x80

	  if (val >= mul) val -= Math.pow(2, 8 * byteLength)

	  return val
	}

	Buffer.prototype.readInt8 = function readInt8 (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 1, this.length)
	  if (!(this[offset] & 0x80)) return (this[offset])
	  return ((0xff - this[offset] + 1) * -1)
	}

	Buffer.prototype.readInt16LE = function readInt16LE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 2, this.length)
	  var val = this[offset] | (this[offset + 1] << 8)
	  return (val & 0x8000) ? val | 0xFFFF0000 : val
	}

	Buffer.prototype.readInt16BE = function readInt16BE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 2, this.length)
	  var val = this[offset + 1] | (this[offset] << 8)
	  return (val & 0x8000) ? val | 0xFFFF0000 : val
	}

	Buffer.prototype.readInt32LE = function readInt32LE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)

	  return (this[offset]) |
	    (this[offset + 1] << 8) |
	    (this[offset + 2] << 16) |
	    (this[offset + 3] << 24)
	}

	Buffer.prototype.readInt32BE = function readInt32BE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)

	  return (this[offset] << 24) |
	    (this[offset + 1] << 16) |
	    (this[offset + 2] << 8) |
	    (this[offset + 3])
	}

	Buffer.prototype.readFloatLE = function readFloatLE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)
	  return ieee754.read(this, offset, true, 23, 4)
	}

	Buffer.prototype.readFloatBE = function readFloatBE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)
	  return ieee754.read(this, offset, false, 23, 4)
	}

	Buffer.prototype.readDoubleLE = function readDoubleLE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 8, this.length)
	  return ieee754.read(this, offset, true, 52, 8)
	}

	Buffer.prototype.readDoubleBE = function readDoubleBE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 8, this.length)
	  return ieee754.read(this, offset, false, 52, 8)
	}

	function checkInt (buf, value, offset, ext, max, min) {
	  if (!Buffer.isBuffer(buf)) throw new TypeError('"buffer" argument must be a Buffer instance')
	  if (value > max || value < min) throw new RangeError('"value" argument is out of bounds')
	  if (offset + ext > buf.length) throw new RangeError('Index out of range')
	}

	Buffer.prototype.writeUIntLE = function writeUIntLE (value, offset, byteLength, noAssert) {
	  value = +value
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) {
	    var maxBytes = Math.pow(2, 8 * byteLength) - 1
	    checkInt(this, value, offset, byteLength, maxBytes, 0)
	  }

	  var mul = 1
	  var i = 0
	  this[offset] = value & 0xFF
	  while (++i < byteLength && (mul *= 0x100)) {
	    this[offset + i] = (value / mul) & 0xFF
	  }

	  return offset + byteLength
	}

	Buffer.prototype.writeUIntBE = function writeUIntBE (value, offset, byteLength, noAssert) {
	  value = +value
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) {
	    var maxBytes = Math.pow(2, 8 * byteLength) - 1
	    checkInt(this, value, offset, byteLength, maxBytes, 0)
	  }

	  var i = byteLength - 1
	  var mul = 1
	  this[offset + i] = value & 0xFF
	  while (--i >= 0 && (mul *= 0x100)) {
	    this[offset + i] = (value / mul) & 0xFF
	  }

	  return offset + byteLength
	}

	Buffer.prototype.writeUInt8 = function writeUInt8 (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 1, 0xff, 0)
	  if (!Buffer.TYPED_ARRAY_SUPPORT) value = Math.floor(value)
	  this[offset] = (value & 0xff)
	  return offset + 1
	}

	function objectWriteUInt16 (buf, value, offset, littleEndian) {
	  if (value < 0) value = 0xffff + value + 1
	  for (var i = 0, j = Math.min(buf.length - offset, 2); i < j; ++i) {
	    buf[offset + i] = (value & (0xff << (8 * (littleEndian ? i : 1 - i)))) >>>
	      (littleEndian ? i : 1 - i) * 8
	  }
	}

	Buffer.prototype.writeUInt16LE = function writeUInt16LE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 2, 0xffff, 0)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value & 0xff)
	    this[offset + 1] = (value >>> 8)
	  } else {
	    objectWriteUInt16(this, value, offset, true)
	  }
	  return offset + 2
	}

	Buffer.prototype.writeUInt16BE = function writeUInt16BE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 2, 0xffff, 0)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value >>> 8)
	    this[offset + 1] = (value & 0xff)
	  } else {
	    objectWriteUInt16(this, value, offset, false)
	  }
	  return offset + 2
	}

	function objectWriteUInt32 (buf, value, offset, littleEndian) {
	  if (value < 0) value = 0xffffffff + value + 1
	  for (var i = 0, j = Math.min(buf.length - offset, 4); i < j; ++i) {
	    buf[offset + i] = (value >>> (littleEndian ? i : 3 - i) * 8) & 0xff
	  }
	}

	Buffer.prototype.writeUInt32LE = function writeUInt32LE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 4, 0xffffffff, 0)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset + 3] = (value >>> 24)
	    this[offset + 2] = (value >>> 16)
	    this[offset + 1] = (value >>> 8)
	    this[offset] = (value & 0xff)
	  } else {
	    objectWriteUInt32(this, value, offset, true)
	  }
	  return offset + 4
	}

	Buffer.prototype.writeUInt32BE = function writeUInt32BE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 4, 0xffffffff, 0)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value >>> 24)
	    this[offset + 1] = (value >>> 16)
	    this[offset + 2] = (value >>> 8)
	    this[offset + 3] = (value & 0xff)
	  } else {
	    objectWriteUInt32(this, value, offset, false)
	  }
	  return offset + 4
	}

	Buffer.prototype.writeIntLE = function writeIntLE (value, offset, byteLength, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) {
	    var limit = Math.pow(2, 8 * byteLength - 1)

	    checkInt(this, value, offset, byteLength, limit - 1, -limit)
	  }

	  var i = 0
	  var mul = 1
	  var sub = 0
	  this[offset] = value & 0xFF
	  while (++i < byteLength && (mul *= 0x100)) {
	    if (value < 0 && sub === 0 && this[offset + i - 1] !== 0) {
	      sub = 1
	    }
	    this[offset + i] = ((value / mul) >> 0) - sub & 0xFF
	  }

	  return offset + byteLength
	}

	Buffer.prototype.writeIntBE = function writeIntBE (value, offset, byteLength, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) {
	    var limit = Math.pow(2, 8 * byteLength - 1)

	    checkInt(this, value, offset, byteLength, limit - 1, -limit)
	  }

	  var i = byteLength - 1
	  var mul = 1
	  var sub = 0
	  this[offset + i] = value & 0xFF
	  while (--i >= 0 && (mul *= 0x100)) {
	    if (value < 0 && sub === 0 && this[offset + i + 1] !== 0) {
	      sub = 1
	    }
	    this[offset + i] = ((value / mul) >> 0) - sub & 0xFF
	  }

	  return offset + byteLength
	}

	Buffer.prototype.writeInt8 = function writeInt8 (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 1, 0x7f, -0x80)
	  if (!Buffer.TYPED_ARRAY_SUPPORT) value = Math.floor(value)
	  if (value < 0) value = 0xff + value + 1
	  this[offset] = (value & 0xff)
	  return offset + 1
	}

	Buffer.prototype.writeInt16LE = function writeInt16LE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 2, 0x7fff, -0x8000)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value & 0xff)
	    this[offset + 1] = (value >>> 8)
	  } else {
	    objectWriteUInt16(this, value, offset, true)
	  }
	  return offset + 2
	}

	Buffer.prototype.writeInt16BE = function writeInt16BE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 2, 0x7fff, -0x8000)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value >>> 8)
	    this[offset + 1] = (value & 0xff)
	  } else {
	    objectWriteUInt16(this, value, offset, false)
	  }
	  return offset + 2
	}

	Buffer.prototype.writeInt32LE = function writeInt32LE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value & 0xff)
	    this[offset + 1] = (value >>> 8)
	    this[offset + 2] = (value >>> 16)
	    this[offset + 3] = (value >>> 24)
	  } else {
	    objectWriteUInt32(this, value, offset, true)
	  }
	  return offset + 4
	}

	Buffer.prototype.writeInt32BE = function writeInt32BE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000)
	  if (value < 0) value = 0xffffffff + value + 1
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value >>> 24)
	    this[offset + 1] = (value >>> 16)
	    this[offset + 2] = (value >>> 8)
	    this[offset + 3] = (value & 0xff)
	  } else {
	    objectWriteUInt32(this, value, offset, false)
	  }
	  return offset + 4
	}

	function checkIEEE754 (buf, value, offset, ext, max, min) {
	  if (offset + ext > buf.length) throw new RangeError('Index out of range')
	  if (offset < 0) throw new RangeError('Index out of range')
	}

	function writeFloat (buf, value, offset, littleEndian, noAssert) {
	  if (!noAssert) {
	    checkIEEE754(buf, value, offset, 4, 3.4028234663852886e+38, -3.4028234663852886e+38)
	  }
	  ieee754.write(buf, value, offset, littleEndian, 23, 4)
	  return offset + 4
	}

	Buffer.prototype.writeFloatLE = function writeFloatLE (value, offset, noAssert) {
	  return writeFloat(this, value, offset, true, noAssert)
	}

	Buffer.prototype.writeFloatBE = function writeFloatBE (value, offset, noAssert) {
	  return writeFloat(this, value, offset, false, noAssert)
	}

	function writeDouble (buf, value, offset, littleEndian, noAssert) {
	  if (!noAssert) {
	    checkIEEE754(buf, value, offset, 8, 1.7976931348623157E+308, -1.7976931348623157E+308)
	  }
	  ieee754.write(buf, value, offset, littleEndian, 52, 8)
	  return offset + 8
	}

	Buffer.prototype.writeDoubleLE = function writeDoubleLE (value, offset, noAssert) {
	  return writeDouble(this, value, offset, true, noAssert)
	}

	Buffer.prototype.writeDoubleBE = function writeDoubleBE (value, offset, noAssert) {
	  return writeDouble(this, value, offset, false, noAssert)
	}

	// copy(targetBuffer, targetStart=0, sourceStart=0, sourceEnd=buffer.length)
	Buffer.prototype.copy = function copy (target, targetStart, start, end) {
	  if (!start) start = 0
	  if (!end && end !== 0) end = this.length
	  if (targetStart >= target.length) targetStart = target.length
	  if (!targetStart) targetStart = 0
	  if (end > 0 && end < start) end = start

	  // Copy 0 bytes; we're done
	  if (end === start) return 0
	  if (target.length === 0 || this.length === 0) return 0

	  // Fatal error conditions
	  if (targetStart < 0) {
	    throw new RangeError('targetStart out of bounds')
	  }
	  if (start < 0 || start >= this.length) throw new RangeError('sourceStart out of bounds')
	  if (end < 0) throw new RangeError('sourceEnd out of bounds')

	  // Are we oob?
	  if (end > this.length) end = this.length
	  if (target.length - targetStart < end - start) {
	    end = target.length - targetStart + start
	  }

	  var len = end - start
	  var i

	  if (this === target && start < targetStart && targetStart < end) {
	    // descending copy from end
	    for (i = len - 1; i >= 0; --i) {
	      target[i + targetStart] = this[i + start]
	    }
	  } else if (len < 1000 || !Buffer.TYPED_ARRAY_SUPPORT) {
	    // ascending copy from start
	    for (i = 0; i < len; ++i) {
	      target[i + targetStart] = this[i + start]
	    }
	  } else {
	    Uint8Array.prototype.set.call(
	      target,
	      this.subarray(start, start + len),
	      targetStart
	    )
	  }

	  return len
	}

	// Usage:
	//    buffer.fill(number[, offset[, end]])
	//    buffer.fill(buffer[, offset[, end]])
	//    buffer.fill(string[, offset[, end]][, encoding])
	Buffer.prototype.fill = function fill (val, start, end, encoding) {
	  // Handle string cases:
	  if (typeof val === 'string') {
	    if (typeof start === 'string') {
	      encoding = start
	      start = 0
	      end = this.length
	    } else if (typeof end === 'string') {
	      encoding = end
	      end = this.length
	    }
	    if (val.length === 1) {
	      var code = val.charCodeAt(0)
	      if (code < 256) {
	        val = code
	      }
	    }
	    if (encoding !== undefined && typeof encoding !== 'string') {
	      throw new TypeError('encoding must be a string')
	    }
	    if (typeof encoding === 'string' && !Buffer.isEncoding(encoding)) {
	      throw new TypeError('Unknown encoding: ' + encoding)
	    }
	  } else if (typeof val === 'number') {
	    val = val & 255
	  }

	  // Invalid ranges are not set to a default, so can range check early.
	  if (start < 0 || this.length < start || this.length < end) {
	    throw new RangeError('Out of range index')
	  }

	  if (end <= start) {
	    return this
	  }

	  start = start >>> 0
	  end = end === undefined ? this.length : end >>> 0

	  if (!val) val = 0

	  var i
	  if (typeof val === 'number') {
	    for (i = start; i < end; ++i) {
	      this[i] = val
	    }
	  } else {
	    var bytes = Buffer.isBuffer(val)
	      ? val
	      : utf8ToBytes(new Buffer(val, encoding).toString())
	    var len = bytes.length
	    for (i = 0; i < end - start; ++i) {
	      this[i + start] = bytes[i % len]
	    }
	  }

	  return this
	}

	// HELPER FUNCTIONS
	// ================

	var INVALID_BASE64_RE = /[^+\/0-9A-Za-z-_]/g

	function base64clean (str) {
	  // Node strips out invalid characters like \n and \t from the string, base64-js does not
	  str = stringtrim(str).replace(INVALID_BASE64_RE, '')
	  // Node converts strings with length < 2 to ''
	  if (str.length < 2) return ''
	  // Node allows for non-padded base64 strings (missing trailing ===), base64-js does not
	  while (str.length % 4 !== 0) {
	    str = str + '='
	  }
	  return str
	}

	function stringtrim (str) {
	  if (str.trim) return str.trim()
	  return str.replace(/^\s+|\s+$/g, '')
	}

	function toHex (n) {
	  if (n < 16) return '0' + n.toString(16)
	  return n.toString(16)
	}

	function utf8ToBytes (string, units) {
	  units = units || Infinity
	  var codePoint
	  var length = string.length
	  var leadSurrogate = null
	  var bytes = []

	  for (var i = 0; i < length; ++i) {
	    codePoint = string.charCodeAt(i)

	    // is surrogate component
	    if (codePoint > 0xD7FF && codePoint < 0xE000) {
	      // last char was a lead
	      if (!leadSurrogate) {
	        // no lead yet
	        if (codePoint > 0xDBFF) {
	          // unexpected trail
	          if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
	          continue
	        } else if (i + 1 === length) {
	          // unpaired lead
	          if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
	          continue
	        }

	        // valid lead
	        leadSurrogate = codePoint

	        continue
	      }

	      // 2 leads in a row
	      if (codePoint < 0xDC00) {
	        if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
	        leadSurrogate = codePoint
	        continue
	      }

	      // valid surrogate pair
	      codePoint = (leadSurrogate - 0xD800 << 10 | codePoint - 0xDC00) + 0x10000
	    } else if (leadSurrogate) {
	      // valid bmp char, but last char was a lead
	      if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
	    }

	    leadSurrogate = null

	    // encode utf8
	    if (codePoint < 0x80) {
	      if ((units -= 1) < 0) break
	      bytes.push(codePoint)
	    } else if (codePoint < 0x800) {
	      if ((units -= 2) < 0) break
	      bytes.push(
	        codePoint >> 0x6 | 0xC0,
	        codePoint & 0x3F | 0x80
	      )
	    } else if (codePoint < 0x10000) {
	      if ((units -= 3) < 0) break
	      bytes.push(
	        codePoint >> 0xC | 0xE0,
	        codePoint >> 0x6 & 0x3F | 0x80,
	        codePoint & 0x3F | 0x80
	      )
	    } else if (codePoint < 0x110000) {
	      if ((units -= 4) < 0) break
	      bytes.push(
	        codePoint >> 0x12 | 0xF0,
	        codePoint >> 0xC & 0x3F | 0x80,
	        codePoint >> 0x6 & 0x3F | 0x80,
	        codePoint & 0x3F | 0x80
	      )
	    } else {
	      throw new Error('Invalid code point')
	    }
	  }

	  return bytes
	}

	function asciiToBytes (str) {
	  var byteArray = []
	  for (var i = 0; i < str.length; ++i) {
	    // Node's code seems to be doing this and not & 0x7F..
	    byteArray.push(str.charCodeAt(i) & 0xFF)
	  }
	  return byteArray
	}

	function utf16leToBytes (str, units) {
	  var c, hi, lo
	  var byteArray = []
	  for (var i = 0; i < str.length; ++i) {
	    if ((units -= 2) < 0) break

	    c = str.charCodeAt(i)
	    hi = c >> 8
	    lo = c % 256
	    byteArray.push(lo)
	    byteArray.push(hi)
	  }

	  return byteArray
	}

	function base64ToBytes (str) {
	  return base64.toByteArray(base64clean(str))
	}

	function blitBuffer (src, dst, offset, length) {
	  for (var i = 0; i < length; ++i) {
	    if ((i + offset >= dst.length) || (i >= src.length)) break
	    dst[i + offset] = src[i]
	  }
	  return i
	}

	function isnan (val) {
	  return val !== val // eslint-disable-line no-self-compare
	}

	/* WEBPACK VAR INJECTION */}.call(exports, (function() { return this; }())))

/***/ }),
/* 334 */
/***/ (function(module, exports) {

	'use strict'

	exports.byteLength = byteLength
	exports.toByteArray = toByteArray
	exports.fromByteArray = fromByteArray

	var lookup = []
	var revLookup = []
	var Arr = typeof Uint8Array !== 'undefined' ? Uint8Array : Array

	var code = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
	for (var i = 0, len = code.length; i < len; ++i) {
	  lookup[i] = code[i]
	  revLookup[code.charCodeAt(i)] = i
	}

	// Support decoding URL-safe base64 strings, as Node.js does.
	// See: https://en.wikipedia.org/wiki/Base64#URL_applications
	revLookup['-'.charCodeAt(0)] = 62
	revLookup['_'.charCodeAt(0)] = 63

	function getLens (b64) {
	  var len = b64.length

	  if (len % 4 > 0) {
	    throw new Error('Invalid string. Length must be a multiple of 4')
	  }

	  // Trim off extra bytes after placeholder bytes are found
	  // See: https://github.com/beatgammit/base64-js/issues/42
	  var validLen = b64.indexOf('=')
	  if (validLen === -1) validLen = len

	  var placeHoldersLen = validLen === len
	    ? 0
	    : 4 - (validLen % 4)

	  return [validLen, placeHoldersLen]
	}

	// base64 is 4/3 + up to two characters of the original data
	function byteLength (b64) {
	  var lens = getLens(b64)
	  var validLen = lens[0]
	  var placeHoldersLen = lens[1]
	  return ((validLen + placeHoldersLen) * 3 / 4) - placeHoldersLen
	}

	function _byteLength (b64, validLen, placeHoldersLen) {
	  return ((validLen + placeHoldersLen) * 3 / 4) - placeHoldersLen
	}

	function toByteArray (b64) {
	  var tmp
	  var lens = getLens(b64)
	  var validLen = lens[0]
	  var placeHoldersLen = lens[1]

	  var arr = new Arr(_byteLength(b64, validLen, placeHoldersLen))

	  var curByte = 0

	  // if there are placeholders, only get up to the last complete 4 chars
	  var len = placeHoldersLen > 0
	    ? validLen - 4
	    : validLen

	  for (var i = 0; i < len; i += 4) {
	    tmp =
	      (revLookup[b64.charCodeAt(i)] << 18) |
	      (revLookup[b64.charCodeAt(i + 1)] << 12) |
	      (revLookup[b64.charCodeAt(i + 2)] << 6) |
	      revLookup[b64.charCodeAt(i + 3)]
	    arr[curByte++] = (tmp >> 16) & 0xFF
	    arr[curByte++] = (tmp >> 8) & 0xFF
	    arr[curByte++] = tmp & 0xFF
	  }

	  if (placeHoldersLen === 2) {
	    tmp =
	      (revLookup[b64.charCodeAt(i)] << 2) |
	      (revLookup[b64.charCodeAt(i + 1)] >> 4)
	    arr[curByte++] = tmp & 0xFF
	  }

	  if (placeHoldersLen === 1) {
	    tmp =
	      (revLookup[b64.charCodeAt(i)] << 10) |
	      (revLookup[b64.charCodeAt(i + 1)] << 4) |
	      (revLookup[b64.charCodeAt(i + 2)] >> 2)
	    arr[curByte++] = (tmp >> 8) & 0xFF
	    arr[curByte++] = tmp & 0xFF
	  }

	  return arr
	}

	function tripletToBase64 (num) {
	  return lookup[num >> 18 & 0x3F] +
	    lookup[num >> 12 & 0x3F] +
	    lookup[num >> 6 & 0x3F] +
	    lookup[num & 0x3F]
	}

	function encodeChunk (uint8, start, end) {
	  var tmp
	  var output = []
	  for (var i = start; i < end; i += 3) {
	    tmp =
	      ((uint8[i] << 16) & 0xFF0000) +
	      ((uint8[i + 1] << 8) & 0xFF00) +
	      (uint8[i + 2] & 0xFF)
	    output.push(tripletToBase64(tmp))
	  }
	  return output.join('')
	}

	function fromByteArray (uint8) {
	  var tmp
	  var len = uint8.length
	  var extraBytes = len % 3 // if we have 1 byte left, pad 2 bytes
	  var parts = []
	  var maxChunkLength = 16383 // must be multiple of 3

	  // go through the array every three bytes, we'll deal with trailing stuff later
	  for (var i = 0, len2 = len - extraBytes; i < len2; i += maxChunkLength) {
	    parts.push(encodeChunk(
	      uint8, i, (i + maxChunkLength) > len2 ? len2 : (i + maxChunkLength)
	    ))
	  }

	  // pad the end with zeros, but make sure to not forget the extra bytes
	  if (extraBytes === 1) {
	    tmp = uint8[len - 1]
	    parts.push(
	      lookup[tmp >> 2] +
	      lookup[(tmp << 4) & 0x3F] +
	      '=='
	    )
	  } else if (extraBytes === 2) {
	    tmp = (uint8[len - 2] << 8) + uint8[len - 1]
	    parts.push(
	      lookup[tmp >> 10] +
	      lookup[(tmp >> 4) & 0x3F] +
	      lookup[(tmp << 2) & 0x3F] +
	      '='
	    )
	  }

	  return parts.join('')
	}


/***/ }),
/* 335 */
/***/ (function(module, exports) {

	exports.read = function (buffer, offset, isLE, mLen, nBytes) {
	  var e, m
	  var eLen = (nBytes * 8) - mLen - 1
	  var eMax = (1 << eLen) - 1
	  var eBias = eMax >> 1
	  var nBits = -7
	  var i = isLE ? (nBytes - 1) : 0
	  var d = isLE ? -1 : 1
	  var s = buffer[offset + i]

	  i += d

	  e = s & ((1 << (-nBits)) - 1)
	  s >>= (-nBits)
	  nBits += eLen
	  for (; nBits > 0; e = (e * 256) + buffer[offset + i], i += d, nBits -= 8) {}

	  m = e & ((1 << (-nBits)) - 1)
	  e >>= (-nBits)
	  nBits += mLen
	  for (; nBits > 0; m = (m * 256) + buffer[offset + i], i += d, nBits -= 8) {}

	  if (e === 0) {
	    e = 1 - eBias
	  } else if (e === eMax) {
	    return m ? NaN : ((s ? -1 : 1) * Infinity)
	  } else {
	    m = m + Math.pow(2, mLen)
	    e = e - eBias
	  }
	  return (s ? -1 : 1) * m * Math.pow(2, e - mLen)
	}

	exports.write = function (buffer, value, offset, isLE, mLen, nBytes) {
	  var e, m, c
	  var eLen = (nBytes * 8) - mLen - 1
	  var eMax = (1 << eLen) - 1
	  var eBias = eMax >> 1
	  var rt = (mLen === 23 ? Math.pow(2, -24) - Math.pow(2, -77) : 0)
	  var i = isLE ? 0 : (nBytes - 1)
	  var d = isLE ? 1 : -1
	  var s = value < 0 || (value === 0 && 1 / value < 0) ? 1 : 0

	  value = Math.abs(value)

	  if (isNaN(value) || value === Infinity) {
	    m = isNaN(value) ? 1 : 0
	    e = eMax
	  } else {
	    e = Math.floor(Math.log(value) / Math.LN2)
	    if (value * (c = Math.pow(2, -e)) < 1) {
	      e--
	      c *= 2
	    }
	    if (e + eBias >= 1) {
	      value += rt / c
	    } else {
	      value += rt * Math.pow(2, 1 - eBias)
	    }
	    if (value * c >= 2) {
	      e++
	      c /= 2
	    }

	    if (e + eBias >= eMax) {
	      m = 0
	      e = eMax
	    } else if (e + eBias >= 1) {
	      m = ((value * c) - 1) * Math.pow(2, mLen)
	      e = e + eBias
	    } else {
	      m = value * Math.pow(2, eBias - 1) * Math.pow(2, mLen)
	      e = 0
	    }
	  }

	  for (; mLen >= 8; buffer[offset + i] = m & 0xff, i += d, m /= 256, mLen -= 8) {}

	  e = (e << mLen) | m
	  eLen += mLen
	  for (; eLen > 0; buffer[offset + i] = e & 0xff, i += d, e /= 256, eLen -= 8) {}

	  buffer[offset + i - d] |= s * 128
	}


/***/ }),
/* 336 */
/***/ (function(module, exports) {

	var toString = {}.toString;

	module.exports = Array.isArray || function (arr) {
	  return toString.call(arr) == '[object Array]';
	};


/***/ }),
/* 337 */
/***/ (function(module, exports) {

	/* WEBPACK VAR INJECTION */(function(global) {exports.fetch = isFunction(global.fetch) && isFunction(global.ReadableStream)

	exports.writableStream = isFunction(global.WritableStream)

	exports.abortController = isFunction(global.AbortController)

	exports.blobConstructor = false
	try {
		new Blob([new ArrayBuffer(1)])
		exports.blobConstructor = true
	} catch (e) {}

	// The xhr request to example.com may violate some restrictive CSP configurations,
	// so if we're running in a browser that supports `fetch`, avoid calling getXHR()
	// and assume support for certain features below.
	var xhr
	function getXHR () {
		// Cache the xhr value
		if (xhr !== undefined) return xhr

		if (global.XMLHttpRequest) {
			xhr = new global.XMLHttpRequest()
			// If XDomainRequest is available (ie only, where xhr might not work
			// cross domain), use the page location. Otherwise use example.com
			// Note: this doesn't actually make an http request.
			try {
				xhr.open('GET', global.XDomainRequest ? '/' : 'https://example.com')
			} catch(e) {
				xhr = null
			}
		} else {
			// Service workers don't have XHR
			xhr = null
		}
		return xhr
	}

	function checkTypeSupport (type) {
		var xhr = getXHR()
		if (!xhr) return false
		try {
			xhr.responseType = type
			return xhr.responseType === type
		} catch (e) {}
		return false
	}

	// For some strange reason, Safari 7.0 reports typeof global.ArrayBuffer === 'object'.
	// Safari 7.1 appears to have fixed this bug.
	var haveArrayBuffer = typeof global.ArrayBuffer !== 'undefined'
	var haveSlice = haveArrayBuffer && isFunction(global.ArrayBuffer.prototype.slice)

	// If fetch is supported, then arraybuffer will be supported too. Skip calling
	// checkTypeSupport(), since that calls getXHR().
	exports.arraybuffer = exports.fetch || (haveArrayBuffer && checkTypeSupport('arraybuffer'))

	// These next two tests unavoidably show warnings in Chrome. Since fetch will always
	// be used if it's available, just return false for these to avoid the warnings.
	exports.msstream = !exports.fetch && haveSlice && checkTypeSupport('ms-stream')
	exports.mozchunkedarraybuffer = !exports.fetch && haveArrayBuffer &&
		checkTypeSupport('moz-chunked-arraybuffer')

	// If fetch is supported, then overrideMimeType will be supported too. Skip calling
	// getXHR().
	exports.overrideMimeType = exports.fetch || (getXHR() ? isFunction(getXHR().overrideMimeType) : false)

	exports.vbArray = isFunction(global.VBArray)

	function isFunction (value) {
		return typeof value === 'function'
	}

	xhr = null // Help gc

	/* WEBPACK VAR INJECTION */}.call(exports, (function() { return this; }())))

/***/ }),
/* 338 */
/***/ (function(module, exports) {

	if (typeof Object.create === 'function') {
	  // implementation from standard node.js 'util' module
	  module.exports = function inherits(ctor, superCtor) {
	    ctor.super_ = superCtor
	    ctor.prototype = Object.create(superCtor.prototype, {
	      constructor: {
	        value: ctor,
	        enumerable: false,
	        writable: true,
	        configurable: true
	      }
	    });
	  };
	} else {
	  // old school shim for old browsers
	  module.exports = function inherits(ctor, superCtor) {
	    ctor.super_ = superCtor
	    var TempCtor = function () {}
	    TempCtor.prototype = superCtor.prototype
	    ctor.prototype = new TempCtor()
	    ctor.prototype.constructor = ctor
	  }
	}


/***/ }),
/* 339 */
/***/ (function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(process, Buffer, global) {var capability = __webpack_require__(337)
	var inherits = __webpack_require__(338)
	var stream = __webpack_require__(340)

	var rStates = exports.readyStates = {
		UNSENT: 0,
		OPENED: 1,
		HEADERS_RECEIVED: 2,
		LOADING: 3,
		DONE: 4
	}

	var IncomingMessage = exports.IncomingMessage = function (xhr, response, mode, fetchTimer) {
		var self = this
		stream.Readable.call(self)

		self._mode = mode
		self.headers = {}
		self.rawHeaders = []
		self.trailers = {}
		self.rawTrailers = []

		// Fake the 'close' event, but only once 'end' fires
		self.on('end', function () {
			// The nextTick is necessary to prevent the 'request' module from causing an infinite loop
			process.nextTick(function () {
				self.emit('close')
			})
		})

		if (mode === 'fetch') {
			self._fetchResponse = response

			self.url = response.url
			self.statusCode = response.status
			self.statusMessage = response.statusText
			
			response.headers.forEach(function (header, key){
				self.headers[key.toLowerCase()] = header
				self.rawHeaders.push(key, header)
			})

			if (capability.writableStream) {
				var writable = new WritableStream({
					write: function (chunk) {
						return new Promise(function (resolve, reject) {
							if (self._destroyed) {
								reject()
							} else if(self.push(new Buffer(chunk))) {
								resolve()
							} else {
								self._resumeFetch = resolve
							}
						})
					},
					close: function () {
						global.clearTimeout(fetchTimer)
						if (!self._destroyed)
							self.push(null)
					},
					abort: function (err) {
						if (!self._destroyed)
							self.emit('error', err)
					}
				})

				try {
					response.body.pipeTo(writable).catch(function (err) {
						global.clearTimeout(fetchTimer)
						if (!self._destroyed)
							self.emit('error', err)
					})
					return
				} catch (e) {} // pipeTo method isn't defined. Can't find a better way to feature test this
			}
			// fallback for when writableStream or pipeTo aren't available
			var reader = response.body.getReader()
			function read () {
				reader.read().then(function (result) {
					if (self._destroyed)
						return
					if (result.done) {
						global.clearTimeout(fetchTimer)
						self.push(null)
						return
					}
					self.push(new Buffer(result.value))
					read()
				}).catch(function (err) {
					global.clearTimeout(fetchTimer)
					if (!self._destroyed)
						self.emit('error', err)
				})
			}
			read()
		} else {
			self._xhr = xhr
			self._pos = 0

			self.url = xhr.responseURL
			self.statusCode = xhr.status
			self.statusMessage = xhr.statusText
			var headers = xhr.getAllResponseHeaders().split(/\r?\n/)
			headers.forEach(function (header) {
				var matches = header.match(/^([^:]+):\s*(.*)/)
				if (matches) {
					var key = matches[1].toLowerCase()
					if (key === 'set-cookie') {
						if (self.headers[key] === undefined) {
							self.headers[key] = []
						}
						self.headers[key].push(matches[2])
					} else if (self.headers[key] !== undefined) {
						self.headers[key] += ', ' + matches[2]
					} else {
						self.headers[key] = matches[2]
					}
					self.rawHeaders.push(matches[1], matches[2])
				}
			})

			self._charset = 'x-user-defined'
			if (!capability.overrideMimeType) {
				var mimeType = self.rawHeaders['mime-type']
				if (mimeType) {
					var charsetMatch = mimeType.match(/;\s*charset=([^;])(;|$)/)
					if (charsetMatch) {
						self._charset = charsetMatch[1].toLowerCase()
					}
				}
				if (!self._charset)
					self._charset = 'utf-8' // best guess
			}
		}
	}

	inherits(IncomingMessage, stream.Readable)

	IncomingMessage.prototype._read = function () {
		var self = this

		var resolve = self._resumeFetch
		if (resolve) {
			self._resumeFetch = null
			resolve()
		}
	}

	IncomingMessage.prototype._onXHRProgress = function () {
		var self = this

		var xhr = self._xhr

		var response = null
		switch (self._mode) {
			case 'text:vbarray': // For IE9
				if (xhr.readyState !== rStates.DONE)
					break
				try {
					// This fails in IE8
					response = new global.VBArray(xhr.responseBody).toArray()
				} catch (e) {}
				if (response !== null) {
					self.push(new Buffer(response))
					break
				}
				// Falls through in IE8	
			case 'text':
				try { // This will fail when readyState = 3 in IE9. Switch mode and wait for readyState = 4
					response = xhr.responseText
				} catch (e) {
					self._mode = 'text:vbarray'
					break
				}
				if (response.length > self._pos) {
					var newData = response.substr(self._pos)
					if (self._charset === 'x-user-defined') {
						var buffer = new Buffer(newData.length)
						for (var i = 0; i < newData.length; i++)
							buffer[i] = newData.charCodeAt(i) & 0xff

						self.push(buffer)
					} else {
						self.push(newData, self._charset)
					}
					self._pos = response.length
				}
				break
			case 'arraybuffer':
				if (xhr.readyState !== rStates.DONE || !xhr.response)
					break
				response = xhr.response
				self.push(new Buffer(new Uint8Array(response)))
				break
			case 'moz-chunked-arraybuffer': // take whole
				response = xhr.response
				if (xhr.readyState !== rStates.LOADING || !response)
					break
				self.push(new Buffer(new Uint8Array(response)))
				break
			case 'ms-stream':
				response = xhr.response
				if (xhr.readyState !== rStates.LOADING)
					break
				var reader = new global.MSStreamReader()
				reader.onprogress = function () {
					if (reader.result.byteLength > self._pos) {
						self.push(new Buffer(new Uint8Array(reader.result.slice(self._pos))))
						self._pos = reader.result.byteLength
					}
				}
				reader.onload = function () {
					self.push(null)
				}
				// reader.onerror = ??? // TODO: this
				reader.readAsArrayBuffer(response)
				break
		}

		// The ms-stream case handles end separately in reader.onload()
		if (self._xhr.readyState === rStates.DONE && self._mode !== 'ms-stream') {
			self.push(null)
		}
	}

	/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(328), __webpack_require__(333).Buffer, (function() { return this; }())))

/***/ }),
/* 340 */
/***/ (function(module, exports, __webpack_require__) {

	exports = module.exports = __webpack_require__(341);
	exports.Stream = exports;
	exports.Readable = exports;
	exports.Writable = __webpack_require__(351);
	exports.Duplex = __webpack_require__(350);
	exports.Transform = __webpack_require__(356);
	exports.PassThrough = __webpack_require__(357);


/***/ }),
/* 341 */
/***/ (function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(global, process) {// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	'use strict';

	/*<replacement>*/

	var pna = __webpack_require__(342);
	/*</replacement>*/

	module.exports = Readable;

	/*<replacement>*/
	var isArray = __webpack_require__(336);
	/*</replacement>*/

	/*<replacement>*/
	var Duplex;
	/*</replacement>*/

	Readable.ReadableState = ReadableState;

	/*<replacement>*/
	var EE = __webpack_require__(329).EventEmitter;

	var EElistenerCount = function (emitter, type) {
	  return emitter.listeners(type).length;
	};
	/*</replacement>*/

	/*<replacement>*/
	var Stream = __webpack_require__(343);
	/*</replacement>*/

	/*<replacement>*/

	var Buffer = __webpack_require__(344).Buffer;
	var OurUint8Array = global.Uint8Array || function () {};
	function _uint8ArrayToBuffer(chunk) {
	  return Buffer.from(chunk);
	}
	function _isUint8Array(obj) {
	  return Buffer.isBuffer(obj) || obj instanceof OurUint8Array;
	}

	/*</replacement>*/

	/*<replacement>*/
	var util = __webpack_require__(345);
	util.inherits = __webpack_require__(338);
	/*</replacement>*/

	/*<replacement>*/
	var debugUtil = __webpack_require__(346);
	var debug = void 0;
	if (debugUtil && debugUtil.debuglog) {
	  debug = debugUtil.debuglog('stream');
	} else {
	  debug = function () {};
	}
	/*</replacement>*/

	var BufferList = __webpack_require__(347);
	var destroyImpl = __webpack_require__(349);
	var StringDecoder;

	util.inherits(Readable, Stream);

	var kProxyEvents = ['error', 'close', 'destroy', 'pause', 'resume'];

	function prependListener(emitter, event, fn) {
	  // Sadly this is not cacheable as some libraries bundle their own
	  // event emitter implementation with them.
	  if (typeof emitter.prependListener === 'function') return emitter.prependListener(event, fn);

	  // This is a hack to make sure that our error handler is attached before any
	  // userland ones.  NEVER DO THIS. This is here only because this code needs
	  // to continue to work with older versions of Node.js that do not include
	  // the prependListener() method. The goal is to eventually remove this hack.
	  if (!emitter._events || !emitter._events[event]) emitter.on(event, fn);else if (isArray(emitter._events[event])) emitter._events[event].unshift(fn);else emitter._events[event] = [fn, emitter._events[event]];
	}

	function ReadableState(options, stream) {
	  Duplex = Duplex || __webpack_require__(350);

	  options = options || {};

	  // Duplex streams are both readable and writable, but share
	  // the same options object.
	  // However, some cases require setting options to different
	  // values for the readable and the writable sides of the duplex stream.
	  // These options can be provided separately as readableXXX and writableXXX.
	  var isDuplex = stream instanceof Duplex;

	  // object stream flag. Used to make read(n) ignore n and to
	  // make all the buffer merging and length checks go away
	  this.objectMode = !!options.objectMode;

	  if (isDuplex) this.objectMode = this.objectMode || !!options.readableObjectMode;

	  // the point at which it stops calling _read() to fill the buffer
	  // Note: 0 is a valid value, means "don't call _read preemptively ever"
	  var hwm = options.highWaterMark;
	  var readableHwm = options.readableHighWaterMark;
	  var defaultHwm = this.objectMode ? 16 : 16 * 1024;

	  if (hwm || hwm === 0) this.highWaterMark = hwm;else if (isDuplex && (readableHwm || readableHwm === 0)) this.highWaterMark = readableHwm;else this.highWaterMark = defaultHwm;

	  // cast to ints.
	  this.highWaterMark = Math.floor(this.highWaterMark);

	  // A linked list is used to store data chunks instead of an array because the
	  // linked list can remove elements from the beginning faster than
	  // array.shift()
	  this.buffer = new BufferList();
	  this.length = 0;
	  this.pipes = null;
	  this.pipesCount = 0;
	  this.flowing = null;
	  this.ended = false;
	  this.endEmitted = false;
	  this.reading = false;

	  // a flag to be able to tell if the event 'readable'/'data' is emitted
	  // immediately, or on a later tick.  We set this to true at first, because
	  // any actions that shouldn't happen until "later" should generally also
	  // not happen before the first read call.
	  this.sync = true;

	  // whenever we return null, then we set a flag to say
	  // that we're awaiting a 'readable' event emission.
	  this.needReadable = false;
	  this.emittedReadable = false;
	  this.readableListening = false;
	  this.resumeScheduled = false;

	  // has it been destroyed
	  this.destroyed = false;

	  // Crypto is kind of old and crusty.  Historically, its default string
	  // encoding is 'binary' so we have to make this configurable.
	  // Everything else in the universe uses 'utf8', though.
	  this.defaultEncoding = options.defaultEncoding || 'utf8';

	  // the number of writers that are awaiting a drain event in .pipe()s
	  this.awaitDrain = 0;

	  // if true, a maybeReadMore has been scheduled
	  this.readingMore = false;

	  this.decoder = null;
	  this.encoding = null;
	  if (options.encoding) {
	    if (!StringDecoder) StringDecoder = __webpack_require__(355).StringDecoder;
	    this.decoder = new StringDecoder(options.encoding);
	    this.encoding = options.encoding;
	  }
	}

	function Readable(options) {
	  Duplex = Duplex || __webpack_require__(350);

	  if (!(this instanceof Readable)) return new Readable(options);

	  this._readableState = new ReadableState(options, this);

	  // legacy
	  this.readable = true;

	  if (options) {
	    if (typeof options.read === 'function') this._read = options.read;

	    if (typeof options.destroy === 'function') this._destroy = options.destroy;
	  }

	  Stream.call(this);
	}

	Object.defineProperty(Readable.prototype, 'destroyed', {
	  get: function () {
	    if (this._readableState === undefined) {
	      return false;
	    }
	    return this._readableState.destroyed;
	  },
	  set: function (value) {
	    // we ignore the value if the stream
	    // has not been initialized yet
	    if (!this._readableState) {
	      return;
	    }

	    // backward compatibility, the user is explicitly
	    // managing destroyed
	    this._readableState.destroyed = value;
	  }
	});

	Readable.prototype.destroy = destroyImpl.destroy;
	Readable.prototype._undestroy = destroyImpl.undestroy;
	Readable.prototype._destroy = function (err, cb) {
	  this.push(null);
	  cb(err);
	};

	// Manually shove something into the read() buffer.
	// This returns true if the highWaterMark has not been hit yet,
	// similar to how Writable.write() returns true if you should
	// write() some more.
	Readable.prototype.push = function (chunk, encoding) {
	  var state = this._readableState;
	  var skipChunkCheck;

	  if (!state.objectMode) {
	    if (typeof chunk === 'string') {
	      encoding = encoding || state.defaultEncoding;
	      if (encoding !== state.encoding) {
	        chunk = Buffer.from(chunk, encoding);
	        encoding = '';
	      }
	      skipChunkCheck = true;
	    }
	  } else {
	    skipChunkCheck = true;
	  }

	  return readableAddChunk(this, chunk, encoding, false, skipChunkCheck);
	};

	// Unshift should *always* be something directly out of read()
	Readable.prototype.unshift = function (chunk) {
	  return readableAddChunk(this, chunk, null, true, false);
	};

	function readableAddChunk(stream, chunk, encoding, addToFront, skipChunkCheck) {
	  var state = stream._readableState;
	  if (chunk === null) {
	    state.reading = false;
	    onEofChunk(stream, state);
	  } else {
	    var er;
	    if (!skipChunkCheck) er = chunkInvalid(state, chunk);
	    if (er) {
	      stream.emit('error', er);
	    } else if (state.objectMode || chunk && chunk.length > 0) {
	      if (typeof chunk !== 'string' && !state.objectMode && Object.getPrototypeOf(chunk) !== Buffer.prototype) {
	        chunk = _uint8ArrayToBuffer(chunk);
	      }

	      if (addToFront) {
	        if (state.endEmitted) stream.emit('error', new Error('stream.unshift() after end event'));else addChunk(stream, state, chunk, true);
	      } else if (state.ended) {
	        stream.emit('error', new Error('stream.push() after EOF'));
	      } else {
	        state.reading = false;
	        if (state.decoder && !encoding) {
	          chunk = state.decoder.write(chunk);
	          if (state.objectMode || chunk.length !== 0) addChunk(stream, state, chunk, false);else maybeReadMore(stream, state);
	        } else {
	          addChunk(stream, state, chunk, false);
	        }
	      }
	    } else if (!addToFront) {
	      state.reading = false;
	    }
	  }

	  return needMoreData(state);
	}

	function addChunk(stream, state, chunk, addToFront) {
	  if (state.flowing && state.length === 0 && !state.sync) {
	    stream.emit('data', chunk);
	    stream.read(0);
	  } else {
	    // update the buffer info.
	    state.length += state.objectMode ? 1 : chunk.length;
	    if (addToFront) state.buffer.unshift(chunk);else state.buffer.push(chunk);

	    if (state.needReadable) emitReadable(stream);
	  }
	  maybeReadMore(stream, state);
	}

	function chunkInvalid(state, chunk) {
	  var er;
	  if (!_isUint8Array(chunk) && typeof chunk !== 'string' && chunk !== undefined && !state.objectMode) {
	    er = new TypeError('Invalid non-string/buffer chunk');
	  }
	  return er;
	}

	// if it's past the high water mark, we can push in some more.
	// Also, if we have no data yet, we can stand some
	// more bytes.  This is to work around cases where hwm=0,
	// such as the repl.  Also, if the push() triggered a
	// readable event, and the user called read(largeNumber) such that
	// needReadable was set, then we ought to push more, so that another
	// 'readable' event will be triggered.
	function needMoreData(state) {
	  return !state.ended && (state.needReadable || state.length < state.highWaterMark || state.length === 0);
	}

	Readable.prototype.isPaused = function () {
	  return this._readableState.flowing === false;
	};

	// backwards compatibility.
	Readable.prototype.setEncoding = function (enc) {
	  if (!StringDecoder) StringDecoder = __webpack_require__(355).StringDecoder;
	  this._readableState.decoder = new StringDecoder(enc);
	  this._readableState.encoding = enc;
	  return this;
	};

	// Don't raise the hwm > 8MB
	var MAX_HWM = 0x800000;
	function computeNewHighWaterMark(n) {
	  if (n >= MAX_HWM) {
	    n = MAX_HWM;
	  } else {
	    // Get the next highest power of 2 to prevent increasing hwm excessively in
	    // tiny amounts
	    n--;
	    n |= n >>> 1;
	    n |= n >>> 2;
	    n |= n >>> 4;
	    n |= n >>> 8;
	    n |= n >>> 16;
	    n++;
	  }
	  return n;
	}

	// This function is designed to be inlinable, so please take care when making
	// changes to the function body.
	function howMuchToRead(n, state) {
	  if (n <= 0 || state.length === 0 && state.ended) return 0;
	  if (state.objectMode) return 1;
	  if (n !== n) {
	    // Only flow one buffer at a time
	    if (state.flowing && state.length) return state.buffer.head.data.length;else return state.length;
	  }
	  // If we're asking for more than the current hwm, then raise the hwm.
	  if (n > state.highWaterMark) state.highWaterMark = computeNewHighWaterMark(n);
	  if (n <= state.length) return n;
	  // Don't have enough
	  if (!state.ended) {
	    state.needReadable = true;
	    return 0;
	  }
	  return state.length;
	}

	// you can override either this method, or the async _read(n) below.
	Readable.prototype.read = function (n) {
	  debug('read', n);
	  n = parseInt(n, 10);
	  var state = this._readableState;
	  var nOrig = n;

	  if (n !== 0) state.emittedReadable = false;

	  // if we're doing read(0) to trigger a readable event, but we
	  // already have a bunch of data in the buffer, then just trigger
	  // the 'readable' event and move on.
	  if (n === 0 && state.needReadable && (state.length >= state.highWaterMark || state.ended)) {
	    debug('read: emitReadable', state.length, state.ended);
	    if (state.length === 0 && state.ended) endReadable(this);else emitReadable(this);
	    return null;
	  }

	  n = howMuchToRead(n, state);

	  // if we've ended, and we're now clear, then finish it up.
	  if (n === 0 && state.ended) {
	    if (state.length === 0) endReadable(this);
	    return null;
	  }

	  // All the actual chunk generation logic needs to be
	  // *below* the call to _read.  The reason is that in certain
	  // synthetic stream cases, such as passthrough streams, _read
	  // may be a completely synchronous operation which may change
	  // the state of the read buffer, providing enough data when
	  // before there was *not* enough.
	  //
	  // So, the steps are:
	  // 1. Figure out what the state of things will be after we do
	  // a read from the buffer.
	  //
	  // 2. If that resulting state will trigger a _read, then call _read.
	  // Note that this may be asynchronous, or synchronous.  Yes, it is
	  // deeply ugly to write APIs this way, but that still doesn't mean
	  // that the Readable class should behave improperly, as streams are
	  // designed to be sync/async agnostic.
	  // Take note if the _read call is sync or async (ie, if the read call
	  // has returned yet), so that we know whether or not it's safe to emit
	  // 'readable' etc.
	  //
	  // 3. Actually pull the requested chunks out of the buffer and return.

	  // if we need a readable event, then we need to do some reading.
	  var doRead = state.needReadable;
	  debug('need readable', doRead);

	  // if we currently have less than the highWaterMark, then also read some
	  if (state.length === 0 || state.length - n < state.highWaterMark) {
	    doRead = true;
	    debug('length less than watermark', doRead);
	  }

	  // however, if we've ended, then there's no point, and if we're already
	  // reading, then it's unnecessary.
	  if (state.ended || state.reading) {
	    doRead = false;
	    debug('reading or ended', doRead);
	  } else if (doRead) {
	    debug('do read');
	    state.reading = true;
	    state.sync = true;
	    // if the length is currently zero, then we *need* a readable event.
	    if (state.length === 0) state.needReadable = true;
	    // call internal read method
	    this._read(state.highWaterMark);
	    state.sync = false;
	    // If _read pushed data synchronously, then `reading` will be false,
	    // and we need to re-evaluate how much data we can return to the user.
	    if (!state.reading) n = howMuchToRead(nOrig, state);
	  }

	  var ret;
	  if (n > 0) ret = fromList(n, state);else ret = null;

	  if (ret === null) {
	    state.needReadable = true;
	    n = 0;
	  } else {
	    state.length -= n;
	  }

	  if (state.length === 0) {
	    // If we have nothing in the buffer, then we want to know
	    // as soon as we *do* get something into the buffer.
	    if (!state.ended) state.needReadable = true;

	    // If we tried to read() past the EOF, then emit end on the next tick.
	    if (nOrig !== n && state.ended) endReadable(this);
	  }

	  if (ret !== null) this.emit('data', ret);

	  return ret;
	};

	function onEofChunk(stream, state) {
	  if (state.ended) return;
	  if (state.decoder) {
	    var chunk = state.decoder.end();
	    if (chunk && chunk.length) {
	      state.buffer.push(chunk);
	      state.length += state.objectMode ? 1 : chunk.length;
	    }
	  }
	  state.ended = true;

	  // emit 'readable' now to make sure it gets picked up.
	  emitReadable(stream);
	}

	// Don't emit readable right away in sync mode, because this can trigger
	// another read() call => stack overflow.  This way, it might trigger
	// a nextTick recursion warning, but that's not so bad.
	function emitReadable(stream) {
	  var state = stream._readableState;
	  state.needReadable = false;
	  if (!state.emittedReadable) {
	    debug('emitReadable', state.flowing);
	    state.emittedReadable = true;
	    if (state.sync) pna.nextTick(emitReadable_, stream);else emitReadable_(stream);
	  }
	}

	function emitReadable_(stream) {
	  debug('emit readable');
	  stream.emit('readable');
	  flow(stream);
	}

	// at this point, the user has presumably seen the 'readable' event,
	// and called read() to consume some data.  that may have triggered
	// in turn another _read(n) call, in which case reading = true if
	// it's in progress.
	// However, if we're not ended, or reading, and the length < hwm,
	// then go ahead and try to read some more preemptively.
	function maybeReadMore(stream, state) {
	  if (!state.readingMore) {
	    state.readingMore = true;
	    pna.nextTick(maybeReadMore_, stream, state);
	  }
	}

	function maybeReadMore_(stream, state) {
	  var len = state.length;
	  while (!state.reading && !state.flowing && !state.ended && state.length < state.highWaterMark) {
	    debug('maybeReadMore read 0');
	    stream.read(0);
	    if (len === state.length)
	      // didn't get any data, stop spinning.
	      break;else len = state.length;
	  }
	  state.readingMore = false;
	}

	// abstract method.  to be overridden in specific implementation classes.
	// call cb(er, data) where data is <= n in length.
	// for virtual (non-string, non-buffer) streams, "length" is somewhat
	// arbitrary, and perhaps not very meaningful.
	Readable.prototype._read = function (n) {
	  this.emit('error', new Error('_read() is not implemented'));
	};

	Readable.prototype.pipe = function (dest, pipeOpts) {
	  var src = this;
	  var state = this._readableState;

	  switch (state.pipesCount) {
	    case 0:
	      state.pipes = dest;
	      break;
	    case 1:
	      state.pipes = [state.pipes, dest];
	      break;
	    default:
	      state.pipes.push(dest);
	      break;
	  }
	  state.pipesCount += 1;
	  debug('pipe count=%d opts=%j', state.pipesCount, pipeOpts);

	  var doEnd = (!pipeOpts || pipeOpts.end !== false) && dest !== process.stdout && dest !== process.stderr;

	  var endFn = doEnd ? onend : unpipe;
	  if (state.endEmitted) pna.nextTick(endFn);else src.once('end', endFn);

	  dest.on('unpipe', onunpipe);
	  function onunpipe(readable, unpipeInfo) {
	    debug('onunpipe');
	    if (readable === src) {
	      if (unpipeInfo && unpipeInfo.hasUnpiped === false) {
	        unpipeInfo.hasUnpiped = true;
	        cleanup();
	      }
	    }
	  }

	  function onend() {
	    debug('onend');
	    dest.end();
	  }

	  // when the dest drains, it reduces the awaitDrain counter
	  // on the source.  This would be more elegant with a .once()
	  // handler in flow(), but adding and removing repeatedly is
	  // too slow.
	  var ondrain = pipeOnDrain(src);
	  dest.on('drain', ondrain);

	  var cleanedUp = false;
	  function cleanup() {
	    debug('cleanup');
	    // cleanup event handlers once the pipe is broken
	    dest.removeListener('close', onclose);
	    dest.removeListener('finish', onfinish);
	    dest.removeListener('drain', ondrain);
	    dest.removeListener('error', onerror);
	    dest.removeListener('unpipe', onunpipe);
	    src.removeListener('end', onend);
	    src.removeListener('end', unpipe);
	    src.removeListener('data', ondata);

	    cleanedUp = true;

	    // if the reader is waiting for a drain event from this
	    // specific writer, then it would cause it to never start
	    // flowing again.
	    // So, if this is awaiting a drain, then we just call it now.
	    // If we don't know, then assume that we are waiting for one.
	    if (state.awaitDrain && (!dest._writableState || dest._writableState.needDrain)) ondrain();
	  }

	  // If the user pushes more data while we're writing to dest then we'll end up
	  // in ondata again. However, we only want to increase awaitDrain once because
	  // dest will only emit one 'drain' event for the multiple writes.
	  // => Introduce a guard on increasing awaitDrain.
	  var increasedAwaitDrain = false;
	  src.on('data', ondata);
	  function ondata(chunk) {
	    debug('ondata');
	    increasedAwaitDrain = false;
	    var ret = dest.write(chunk);
	    if (false === ret && !increasedAwaitDrain) {
	      // If the user unpiped during `dest.write()`, it is possible
	      // to get stuck in a permanently paused state if that write
	      // also returned false.
	      // => Check whether `dest` is still a piping destination.
	      if ((state.pipesCount === 1 && state.pipes === dest || state.pipesCount > 1 && indexOf(state.pipes, dest) !== -1) && !cleanedUp) {
	        debug('false write response, pause', src._readableState.awaitDrain);
	        src._readableState.awaitDrain++;
	        increasedAwaitDrain = true;
	      }
	      src.pause();
	    }
	  }

	  // if the dest has an error, then stop piping into it.
	  // however, don't suppress the throwing behavior for this.
	  function onerror(er) {
	    debug('onerror', er);
	    unpipe();
	    dest.removeListener('error', onerror);
	    if (EElistenerCount(dest, 'error') === 0) dest.emit('error', er);
	  }

	  // Make sure our error handler is attached before userland ones.
	  prependListener(dest, 'error', onerror);

	  // Both close and finish should trigger unpipe, but only once.
	  function onclose() {
	    dest.removeListener('finish', onfinish);
	    unpipe();
	  }
	  dest.once('close', onclose);
	  function onfinish() {
	    debug('onfinish');
	    dest.removeListener('close', onclose);
	    unpipe();
	  }
	  dest.once('finish', onfinish);

	  function unpipe() {
	    debug('unpipe');
	    src.unpipe(dest);
	  }

	  // tell the dest that it's being piped to
	  dest.emit('pipe', src);

	  // start the flow if it hasn't been started already.
	  if (!state.flowing) {
	    debug('pipe resume');
	    src.resume();
	  }

	  return dest;
	};

	function pipeOnDrain(src) {
	  return function () {
	    var state = src._readableState;
	    debug('pipeOnDrain', state.awaitDrain);
	    if (state.awaitDrain) state.awaitDrain--;
	    if (state.awaitDrain === 0 && EElistenerCount(src, 'data')) {
	      state.flowing = true;
	      flow(src);
	    }
	  };
	}

	Readable.prototype.unpipe = function (dest) {
	  var state = this._readableState;
	  var unpipeInfo = { hasUnpiped: false };

	  // if we're not piping anywhere, then do nothing.
	  if (state.pipesCount === 0) return this;

	  // just one destination.  most common case.
	  if (state.pipesCount === 1) {
	    // passed in one, but it's not the right one.
	    if (dest && dest !== state.pipes) return this;

	    if (!dest) dest = state.pipes;

	    // got a match.
	    state.pipes = null;
	    state.pipesCount = 0;
	    state.flowing = false;
	    if (dest) dest.emit('unpipe', this, unpipeInfo);
	    return this;
	  }

	  // slow case. multiple pipe destinations.

	  if (!dest) {
	    // remove all.
	    var dests = state.pipes;
	    var len = state.pipesCount;
	    state.pipes = null;
	    state.pipesCount = 0;
	    state.flowing = false;

	    for (var i = 0; i < len; i++) {
	      dests[i].emit('unpipe', this, unpipeInfo);
	    }return this;
	  }

	  // try to find the right one.
	  var index = indexOf(state.pipes, dest);
	  if (index === -1) return this;

	  state.pipes.splice(index, 1);
	  state.pipesCount -= 1;
	  if (state.pipesCount === 1) state.pipes = state.pipes[0];

	  dest.emit('unpipe', this, unpipeInfo);

	  return this;
	};

	// set up data events if they are asked for
	// Ensure readable listeners eventually get something
	Readable.prototype.on = function (ev, fn) {
	  var res = Stream.prototype.on.call(this, ev, fn);

	  if (ev === 'data') {
	    // Start flowing on next tick if stream isn't explicitly paused
	    if (this._readableState.flowing !== false) this.resume();
	  } else if (ev === 'readable') {
	    var state = this._readableState;
	    if (!state.endEmitted && !state.readableListening) {
	      state.readableListening = state.needReadable = true;
	      state.emittedReadable = false;
	      if (!state.reading) {
	        pna.nextTick(nReadingNextTick, this);
	      } else if (state.length) {
	        emitReadable(this);
	      }
	    }
	  }

	  return res;
	};
	Readable.prototype.addListener = Readable.prototype.on;

	function nReadingNextTick(self) {
	  debug('readable nexttick read 0');
	  self.read(0);
	}

	// pause() and resume() are remnants of the legacy readable stream API
	// If the user uses them, then switch into old mode.
	Readable.prototype.resume = function () {
	  var state = this._readableState;
	  if (!state.flowing) {
	    debug('resume');
	    state.flowing = true;
	    resume(this, state);
	  }
	  return this;
	};

	function resume(stream, state) {
	  if (!state.resumeScheduled) {
	    state.resumeScheduled = true;
	    pna.nextTick(resume_, stream, state);
	  }
	}

	function resume_(stream, state) {
	  if (!state.reading) {
	    debug('resume read 0');
	    stream.read(0);
	  }

	  state.resumeScheduled = false;
	  state.awaitDrain = 0;
	  stream.emit('resume');
	  flow(stream);
	  if (state.flowing && !state.reading) stream.read(0);
	}

	Readable.prototype.pause = function () {
	  debug('call pause flowing=%j', this._readableState.flowing);
	  if (false !== this._readableState.flowing) {
	    debug('pause');
	    this._readableState.flowing = false;
	    this.emit('pause');
	  }
	  return this;
	};

	function flow(stream) {
	  var state = stream._readableState;
	  debug('flow', state.flowing);
	  while (state.flowing && stream.read() !== null) {}
	}

	// wrap an old-style stream as the async data source.
	// This is *not* part of the readable stream interface.
	// It is an ugly unfortunate mess of history.
	Readable.prototype.wrap = function (stream) {
	  var _this = this;

	  var state = this._readableState;
	  var paused = false;

	  stream.on('end', function () {
	    debug('wrapped end');
	    if (state.decoder && !state.ended) {
	      var chunk = state.decoder.end();
	      if (chunk && chunk.length) _this.push(chunk);
	    }

	    _this.push(null);
	  });

	  stream.on('data', function (chunk) {
	    debug('wrapped data');
	    if (state.decoder) chunk = state.decoder.write(chunk);

	    // don't skip over falsy values in objectMode
	    if (state.objectMode && (chunk === null || chunk === undefined)) return;else if (!state.objectMode && (!chunk || !chunk.length)) return;

	    var ret = _this.push(chunk);
	    if (!ret) {
	      paused = true;
	      stream.pause();
	    }
	  });

	  // proxy all the other methods.
	  // important when wrapping filters and duplexes.
	  for (var i in stream) {
	    if (this[i] === undefined && typeof stream[i] === 'function') {
	      this[i] = function (method) {
	        return function () {
	          return stream[method].apply(stream, arguments);
	        };
	      }(i);
	    }
	  }

	  // proxy certain important events.
	  for (var n = 0; n < kProxyEvents.length; n++) {
	    stream.on(kProxyEvents[n], this.emit.bind(this, kProxyEvents[n]));
	  }

	  // when we try to consume some more bytes, simply unpause the
	  // underlying stream.
	  this._read = function (n) {
	    debug('wrapped _read', n);
	    if (paused) {
	      paused = false;
	      stream.resume();
	    }
	  };

	  return this;
	};

	Object.defineProperty(Readable.prototype, 'readableHighWaterMark', {
	  // making it explicit this property is not enumerable
	  // because otherwise some prototype manipulation in
	  // userland will fail
	  enumerable: false,
	  get: function () {
	    return this._readableState.highWaterMark;
	  }
	});

	// exposed for testing purposes only.
	Readable._fromList = fromList;

	// Pluck off n bytes from an array of buffers.
	// Length is the combined lengths of all the buffers in the list.
	// This function is designed to be inlinable, so please take care when making
	// changes to the function body.
	function fromList(n, state) {
	  // nothing buffered
	  if (state.length === 0) return null;

	  var ret;
	  if (state.objectMode) ret = state.buffer.shift();else if (!n || n >= state.length) {
	    // read it all, truncate the list
	    if (state.decoder) ret = state.buffer.join('');else if (state.buffer.length === 1) ret = state.buffer.head.data;else ret = state.buffer.concat(state.length);
	    state.buffer.clear();
	  } else {
	    // read part of list
	    ret = fromListPartial(n, state.buffer, state.decoder);
	  }

	  return ret;
	}

	// Extracts only enough buffered data to satisfy the amount requested.
	// This function is designed to be inlinable, so please take care when making
	// changes to the function body.
	function fromListPartial(n, list, hasStrings) {
	  var ret;
	  if (n < list.head.data.length) {
	    // slice is the same for buffers and strings
	    ret = list.head.data.slice(0, n);
	    list.head.data = list.head.data.slice(n);
	  } else if (n === list.head.data.length) {
	    // first chunk is a perfect match
	    ret = list.shift();
	  } else {
	    // result spans more than one buffer
	    ret = hasStrings ? copyFromBufferString(n, list) : copyFromBuffer(n, list);
	  }
	  return ret;
	}

	// Copies a specified amount of characters from the list of buffered data
	// chunks.
	// This function is designed to be inlinable, so please take care when making
	// changes to the function body.
	function copyFromBufferString(n, list) {
	  var p = list.head;
	  var c = 1;
	  var ret = p.data;
	  n -= ret.length;
	  while (p = p.next) {
	    var str = p.data;
	    var nb = n > str.length ? str.length : n;
	    if (nb === str.length) ret += str;else ret += str.slice(0, n);
	    n -= nb;
	    if (n === 0) {
	      if (nb === str.length) {
	        ++c;
	        if (p.next) list.head = p.next;else list.head = list.tail = null;
	      } else {
	        list.head = p;
	        p.data = str.slice(nb);
	      }
	      break;
	    }
	    ++c;
	  }
	  list.length -= c;
	  return ret;
	}

	// Copies a specified amount of bytes from the list of buffered data chunks.
	// This function is designed to be inlinable, so please take care when making
	// changes to the function body.
	function copyFromBuffer(n, list) {
	  var ret = Buffer.allocUnsafe(n);
	  var p = list.head;
	  var c = 1;
	  p.data.copy(ret);
	  n -= p.data.length;
	  while (p = p.next) {
	    var buf = p.data;
	    var nb = n > buf.length ? buf.length : n;
	    buf.copy(ret, ret.length - n, 0, nb);
	    n -= nb;
	    if (n === 0) {
	      if (nb === buf.length) {
	        ++c;
	        if (p.next) list.head = p.next;else list.head = list.tail = null;
	      } else {
	        list.head = p;
	        p.data = buf.slice(nb);
	      }
	      break;
	    }
	    ++c;
	  }
	  list.length -= c;
	  return ret;
	}

	function endReadable(stream) {
	  var state = stream._readableState;

	  // If we get here before consuming all the bytes, then that is a
	  // bug in node.  Should never happen.
	  if (state.length > 0) throw new Error('"endReadable()" called on non-empty stream');

	  if (!state.endEmitted) {
	    state.ended = true;
	    pna.nextTick(endReadableNT, state, stream);
	  }
	}

	function endReadableNT(state, stream) {
	  // Check that we didn't get one last unshift.
	  if (!state.endEmitted && state.length === 0) {
	    state.endEmitted = true;
	    stream.readable = false;
	    stream.emit('end');
	  }
	}

	function indexOf(xs, x) {
	  for (var i = 0, l = xs.length; i < l; i++) {
	    if (xs[i] === x) return i;
	  }
	  return -1;
	}
	/* WEBPACK VAR INJECTION */}.call(exports, (function() { return this; }()), __webpack_require__(328)))

/***/ }),
/* 342 */
/***/ (function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(process) {'use strict';

	if (!process.version ||
	    process.version.indexOf('v0.') === 0 ||
	    process.version.indexOf('v1.') === 0 && process.version.indexOf('v1.8.') !== 0) {
	  module.exports = { nextTick: nextTick };
	} else {
	  module.exports = process
	}

	function nextTick(fn, arg1, arg2, arg3) {
	  if (typeof fn !== 'function') {
	    throw new TypeError('"callback" argument must be a function');
	  }
	  var len = arguments.length;
	  var args, i;
	  switch (len) {
	  case 0:
	  case 1:
	    return process.nextTick(fn);
	  case 2:
	    return process.nextTick(function afterTickOne() {
	      fn.call(null, arg1);
	    });
	  case 3:
	    return process.nextTick(function afterTickTwo() {
	      fn.call(null, arg1, arg2);
	    });
	  case 4:
	    return process.nextTick(function afterTickThree() {
	      fn.call(null, arg1, arg2, arg3);
	    });
	  default:
	    args = new Array(len - 1);
	    i = 0;
	    while (i < args.length) {
	      args[i++] = arguments[i];
	    }
	    return process.nextTick(function afterTick() {
	      fn.apply(null, args);
	    });
	  }
	}


	/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(328)))

/***/ }),
/* 343 */
/***/ (function(module, exports, __webpack_require__) {

	module.exports = __webpack_require__(329).EventEmitter;


/***/ }),
/* 344 */
/***/ (function(module, exports, __webpack_require__) {

	/* eslint-disable node/no-deprecated-api */
	var buffer = __webpack_require__(333)
	var Buffer = buffer.Buffer

	// alternative to using Object.keys for old browsers
	function copyProps (src, dst) {
	  for (var key in src) {
	    dst[key] = src[key]
	  }
	}
	if (Buffer.from && Buffer.alloc && Buffer.allocUnsafe && Buffer.allocUnsafeSlow) {
	  module.exports = buffer
	} else {
	  // Copy properties from require('buffer')
	  copyProps(buffer, exports)
	  exports.Buffer = SafeBuffer
	}

	function SafeBuffer (arg, encodingOrOffset, length) {
	  return Buffer(arg, encodingOrOffset, length)
	}

	// Copy static methods from Buffer
	copyProps(Buffer, SafeBuffer)

	SafeBuffer.from = function (arg, encodingOrOffset, length) {
	  if (typeof arg === 'number') {
	    throw new TypeError('Argument must not be a number')
	  }
	  return Buffer(arg, encodingOrOffset, length)
	}

	SafeBuffer.alloc = function (size, fill, encoding) {
	  if (typeof size !== 'number') {
	    throw new TypeError('Argument must be a number')
	  }
	  var buf = Buffer(size)
	  if (fill !== undefined) {
	    if (typeof encoding === 'string') {
	      buf.fill(fill, encoding)
	    } else {
	      buf.fill(fill)
	    }
	  } else {
	    buf.fill(0)
	  }
	  return buf
	}

	SafeBuffer.allocUnsafe = function (size) {
	  if (typeof size !== 'number') {
	    throw new TypeError('Argument must be a number')
	  }
	  return Buffer(size)
	}

	SafeBuffer.allocUnsafeSlow = function (size) {
	  if (typeof size !== 'number') {
	    throw new TypeError('Argument must be a number')
	  }
	  return buffer.SlowBuffer(size)
	}


/***/ }),
/* 345 */
/***/ (function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(Buffer) {// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	// NOTE: These type checking functions intentionally don't use `instanceof`
	// because it is fragile and can be easily faked with `Object.create()`.

	function isArray(arg) {
	  if (Array.isArray) {
	    return Array.isArray(arg);
	  }
	  return objectToString(arg) === '[object Array]';
	}
	exports.isArray = isArray;

	function isBoolean(arg) {
	  return typeof arg === 'boolean';
	}
	exports.isBoolean = isBoolean;

	function isNull(arg) {
	  return arg === null;
	}
	exports.isNull = isNull;

	function isNullOrUndefined(arg) {
	  return arg == null;
	}
	exports.isNullOrUndefined = isNullOrUndefined;

	function isNumber(arg) {
	  return typeof arg === 'number';
	}
	exports.isNumber = isNumber;

	function isString(arg) {
	  return typeof arg === 'string';
	}
	exports.isString = isString;

	function isSymbol(arg) {
	  return typeof arg === 'symbol';
	}
	exports.isSymbol = isSymbol;

	function isUndefined(arg) {
	  return arg === void 0;
	}
	exports.isUndefined = isUndefined;

	function isRegExp(re) {
	  return objectToString(re) === '[object RegExp]';
	}
	exports.isRegExp = isRegExp;

	function isObject(arg) {
	  return typeof arg === 'object' && arg !== null;
	}
	exports.isObject = isObject;

	function isDate(d) {
	  return objectToString(d) === '[object Date]';
	}
	exports.isDate = isDate;

	function isError(e) {
	  return (objectToString(e) === '[object Error]' || e instanceof Error);
	}
	exports.isError = isError;

	function isFunction(arg) {
	  return typeof arg === 'function';
	}
	exports.isFunction = isFunction;

	function isPrimitive(arg) {
	  return arg === null ||
	         typeof arg === 'boolean' ||
	         typeof arg === 'number' ||
	         typeof arg === 'string' ||
	         typeof arg === 'symbol' ||  // ES6 symbol
	         typeof arg === 'undefined';
	}
	exports.isPrimitive = isPrimitive;

	exports.isBuffer = Buffer.isBuffer;

	function objectToString(o) {
	  return Object.prototype.toString.call(o);
	}

	/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(333).Buffer))

/***/ }),
/* 346 */
/***/ (function(module, exports) {

	/* (ignored) */

/***/ }),
/* 347 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';

	function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } }

	var Buffer = __webpack_require__(344).Buffer;
	var util = __webpack_require__(348);

	function copyBuffer(src, target, offset) {
	  src.copy(target, offset);
	}

	module.exports = function () {
	  function BufferList() {
	    _classCallCheck(this, BufferList);

	    this.head = null;
	    this.tail = null;
	    this.length = 0;
	  }

	  BufferList.prototype.push = function push(v) {
	    var entry = { data: v, next: null };
	    if (this.length > 0) this.tail.next = entry;else this.head = entry;
	    this.tail = entry;
	    ++this.length;
	  };

	  BufferList.prototype.unshift = function unshift(v) {
	    var entry = { data: v, next: this.head };
	    if (this.length === 0) this.tail = entry;
	    this.head = entry;
	    ++this.length;
	  };

	  BufferList.prototype.shift = function shift() {
	    if (this.length === 0) return;
	    var ret = this.head.data;
	    if (this.length === 1) this.head = this.tail = null;else this.head = this.head.next;
	    --this.length;
	    return ret;
	  };

	  BufferList.prototype.clear = function clear() {
	    this.head = this.tail = null;
	    this.length = 0;
	  };

	  BufferList.prototype.join = function join(s) {
	    if (this.length === 0) return '';
	    var p = this.head;
	    var ret = '' + p.data;
	    while (p = p.next) {
	      ret += s + p.data;
	    }return ret;
	  };

	  BufferList.prototype.concat = function concat(n) {
	    if (this.length === 0) return Buffer.alloc(0);
	    if (this.length === 1) return this.head.data;
	    var ret = Buffer.allocUnsafe(n >>> 0);
	    var p = this.head;
	    var i = 0;
	    while (p) {
	      copyBuffer(p.data, ret, i);
	      i += p.data.length;
	      p = p.next;
	    }
	    return ret;
	  };

	  return BufferList;
	}();

	if (util && util.inspect && util.inspect.custom) {
	  module.exports.prototype[util.inspect.custom] = function () {
	    var obj = util.inspect({ length: this.length });
	    return this.constructor.name + ' ' + obj;
	  };
	}

/***/ }),
/* 348 */
/***/ (function(module, exports) {

	/* (ignored) */

/***/ }),
/* 349 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';

	/*<replacement>*/

	var pna = __webpack_require__(342);
	/*</replacement>*/

	// undocumented cb() API, needed for core, not for public API
	function destroy(err, cb) {
	  var _this = this;

	  var readableDestroyed = this._readableState && this._readableState.destroyed;
	  var writableDestroyed = this._writableState && this._writableState.destroyed;

	  if (readableDestroyed || writableDestroyed) {
	    if (cb) {
	      cb(err);
	    } else if (err && (!this._writableState || !this._writableState.errorEmitted)) {
	      pna.nextTick(emitErrorNT, this, err);
	    }
	    return this;
	  }

	  // we set destroyed to true before firing error callbacks in order
	  // to make it re-entrance safe in case destroy() is called within callbacks

	  if (this._readableState) {
	    this._readableState.destroyed = true;
	  }

	  // if this is a duplex stream mark the writable part as destroyed as well
	  if (this._writableState) {
	    this._writableState.destroyed = true;
	  }

	  this._destroy(err || null, function (err) {
	    if (!cb && err) {
	      pna.nextTick(emitErrorNT, _this, err);
	      if (_this._writableState) {
	        _this._writableState.errorEmitted = true;
	      }
	    } else if (cb) {
	      cb(err);
	    }
	  });

	  return this;
	}

	function undestroy() {
	  if (this._readableState) {
	    this._readableState.destroyed = false;
	    this._readableState.reading = false;
	    this._readableState.ended = false;
	    this._readableState.endEmitted = false;
	  }

	  if (this._writableState) {
	    this._writableState.destroyed = false;
	    this._writableState.ended = false;
	    this._writableState.ending = false;
	    this._writableState.finished = false;
	    this._writableState.errorEmitted = false;
	  }
	}

	function emitErrorNT(self, err) {
	  self.emit('error', err);
	}

	module.exports = {
	  destroy: destroy,
	  undestroy: undestroy
	};

/***/ }),
/* 350 */
/***/ (function(module, exports, __webpack_require__) {

	// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	// a duplex stream is just a stream that is both readable and writable.
	// Since JS doesn't have multiple prototypal inheritance, this class
	// prototypally inherits from Readable, and then parasitically from
	// Writable.

	'use strict';

	/*<replacement>*/

	var pna = __webpack_require__(342);
	/*</replacement>*/

	/*<replacement>*/
	var objectKeys = Object.keys || function (obj) {
	  var keys = [];
	  for (var key in obj) {
	    keys.push(key);
	  }return keys;
	};
	/*</replacement>*/

	module.exports = Duplex;

	/*<replacement>*/
	var util = __webpack_require__(345);
	util.inherits = __webpack_require__(338);
	/*</replacement>*/

	var Readable = __webpack_require__(341);
	var Writable = __webpack_require__(351);

	util.inherits(Duplex, Readable);

	{
	  // avoid scope creep, the keys array can then be collected
	  var keys = objectKeys(Writable.prototype);
	  for (var v = 0; v < keys.length; v++) {
	    var method = keys[v];
	    if (!Duplex.prototype[method]) Duplex.prototype[method] = Writable.prototype[method];
	  }
	}

	function Duplex(options) {
	  if (!(this instanceof Duplex)) return new Duplex(options);

	  Readable.call(this, options);
	  Writable.call(this, options);

	  if (options && options.readable === false) this.readable = false;

	  if (options && options.writable === false) this.writable = false;

	  this.allowHalfOpen = true;
	  if (options && options.allowHalfOpen === false) this.allowHalfOpen = false;

	  this.once('end', onend);
	}

	Object.defineProperty(Duplex.prototype, 'writableHighWaterMark', {
	  // making it explicit this property is not enumerable
	  // because otherwise some prototype manipulation in
	  // userland will fail
	  enumerable: false,
	  get: function () {
	    return this._writableState.highWaterMark;
	  }
	});

	// the no-half-open enforcer
	function onend() {
	  // if we allow half-open state, or if the writable side ended,
	  // then we're ok.
	  if (this.allowHalfOpen || this._writableState.ended) return;

	  // no more data can be written.
	  // But allow more writes to happen in this tick.
	  pna.nextTick(onEndNT, this);
	}

	function onEndNT(self) {
	  self.end();
	}

	Object.defineProperty(Duplex.prototype, 'destroyed', {
	  get: function () {
	    if (this._readableState === undefined || this._writableState === undefined) {
	      return false;
	    }
	    return this._readableState.destroyed && this._writableState.destroyed;
	  },
	  set: function (value) {
	    // we ignore the value if the stream
	    // has not been initialized yet
	    if (this._readableState === undefined || this._writableState === undefined) {
	      return;
	    }

	    // backward compatibility, the user is explicitly
	    // managing destroyed
	    this._readableState.destroyed = value;
	    this._writableState.destroyed = value;
	  }
	});

	Duplex.prototype._destroy = function (err, cb) {
	  this.push(null);
	  this.end();

	  pna.nextTick(cb, err);
	};

/***/ }),
/* 351 */
/***/ (function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(process, setImmediate, global) {// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	// A bit simpler than readable streams.
	// Implement an async ._write(chunk, encoding, cb), and it'll handle all
	// the drain event emission and buffering.

	'use strict';

	/*<replacement>*/

	var pna = __webpack_require__(342);
	/*</replacement>*/

	module.exports = Writable;

	/* <replacement> */
	function WriteReq(chunk, encoding, cb) {
	  this.chunk = chunk;
	  this.encoding = encoding;
	  this.callback = cb;
	  this.next = null;
	}

	// It seems a linked list but it is not
	// there will be only 2 of these for each stream
	function CorkedRequest(state) {
	  var _this = this;

	  this.next = null;
	  this.entry = null;
	  this.finish = function () {
	    onCorkedFinish(_this, state);
	  };
	}
	/* </replacement> */

	/*<replacement>*/
	var asyncWrite = !process.browser && ['v0.10', 'v0.9.'].indexOf(process.version.slice(0, 5)) > -1 ? setImmediate : pna.nextTick;
	/*</replacement>*/

	/*<replacement>*/
	var Duplex;
	/*</replacement>*/

	Writable.WritableState = WritableState;

	/*<replacement>*/
	var util = __webpack_require__(345);
	util.inherits = __webpack_require__(338);
	/*</replacement>*/

	/*<replacement>*/
	var internalUtil = {
	  deprecate: __webpack_require__(354)
	};
	/*</replacement>*/

	/*<replacement>*/
	var Stream = __webpack_require__(343);
	/*</replacement>*/

	/*<replacement>*/

	var Buffer = __webpack_require__(344).Buffer;
	var OurUint8Array = global.Uint8Array || function () {};
	function _uint8ArrayToBuffer(chunk) {
	  return Buffer.from(chunk);
	}
	function _isUint8Array(obj) {
	  return Buffer.isBuffer(obj) || obj instanceof OurUint8Array;
	}

	/*</replacement>*/

	var destroyImpl = __webpack_require__(349);

	util.inherits(Writable, Stream);

	function nop() {}

	function WritableState(options, stream) {
	  Duplex = Duplex || __webpack_require__(350);

	  options = options || {};

	  // Duplex streams are both readable and writable, but share
	  // the same options object.
	  // However, some cases require setting options to different
	  // values for the readable and the writable sides of the duplex stream.
	  // These options can be provided separately as readableXXX and writableXXX.
	  var isDuplex = stream instanceof Duplex;

	  // object stream flag to indicate whether or not this stream
	  // contains buffers or objects.
	  this.objectMode = !!options.objectMode;

	  if (isDuplex) this.objectMode = this.objectMode || !!options.writableObjectMode;

	  // the point at which write() starts returning false
	  // Note: 0 is a valid value, means that we always return false if
	  // the entire buffer is not flushed immediately on write()
	  var hwm = options.highWaterMark;
	  var writableHwm = options.writableHighWaterMark;
	  var defaultHwm = this.objectMode ? 16 : 16 * 1024;

	  if (hwm || hwm === 0) this.highWaterMark = hwm;else if (isDuplex && (writableHwm || writableHwm === 0)) this.highWaterMark = writableHwm;else this.highWaterMark = defaultHwm;

	  // cast to ints.
	  this.highWaterMark = Math.floor(this.highWaterMark);

	  // if _final has been called
	  this.finalCalled = false;

	  // drain event flag.
	  this.needDrain = false;
	  // at the start of calling end()
	  this.ending = false;
	  // when end() has been called, and returned
	  this.ended = false;
	  // when 'finish' is emitted
	  this.finished = false;

	  // has it been destroyed
	  this.destroyed = false;

	  // should we decode strings into buffers before passing to _write?
	  // this is here so that some node-core streams can optimize string
	  // handling at a lower level.
	  var noDecode = options.decodeStrings === false;
	  this.decodeStrings = !noDecode;

	  // Crypto is kind of old and crusty.  Historically, its default string
	  // encoding is 'binary' so we have to make this configurable.
	  // Everything else in the universe uses 'utf8', though.
	  this.defaultEncoding = options.defaultEncoding || 'utf8';

	  // not an actual buffer we keep track of, but a measurement
	  // of how much we're waiting to get pushed to some underlying
	  // socket or file.
	  this.length = 0;

	  // a flag to see when we're in the middle of a write.
	  this.writing = false;

	  // when true all writes will be buffered until .uncork() call
	  this.corked = 0;

	  // a flag to be able to tell if the onwrite cb is called immediately,
	  // or on a later tick.  We set this to true at first, because any
	  // actions that shouldn't happen until "later" should generally also
	  // not happen before the first write call.
	  this.sync = true;

	  // a flag to know if we're processing previously buffered items, which
	  // may call the _write() callback in the same tick, so that we don't
	  // end up in an overlapped onwrite situation.
	  this.bufferProcessing = false;

	  // the callback that's passed to _write(chunk,cb)
	  this.onwrite = function (er) {
	    onwrite(stream, er);
	  };

	  // the callback that the user supplies to write(chunk,encoding,cb)
	  this.writecb = null;

	  // the amount that is being written when _write is called.
	  this.writelen = 0;

	  this.bufferedRequest = null;
	  this.lastBufferedRequest = null;

	  // number of pending user-supplied write callbacks
	  // this must be 0 before 'finish' can be emitted
	  this.pendingcb = 0;

	  // emit prefinish if the only thing we're waiting for is _write cbs
	  // This is relevant for synchronous Transform streams
	  this.prefinished = false;

	  // True if the error was already emitted and should not be thrown again
	  this.errorEmitted = false;

	  // count buffered requests
	  this.bufferedRequestCount = 0;

	  // allocate the first CorkedRequest, there is always
	  // one allocated and free to use, and we maintain at most two
	  this.corkedRequestsFree = new CorkedRequest(this);
	}

	WritableState.prototype.getBuffer = function getBuffer() {
	  var current = this.bufferedRequest;
	  var out = [];
	  while (current) {
	    out.push(current);
	    current = current.next;
	  }
	  return out;
	};

	(function () {
	  try {
	    Object.defineProperty(WritableState.prototype, 'buffer', {
	      get: internalUtil.deprecate(function () {
	        return this.getBuffer();
	      }, '_writableState.buffer is deprecated. Use _writableState.getBuffer ' + 'instead.', 'DEP0003')
	    });
	  } catch (_) {}
	})();

	// Test _writableState for inheritance to account for Duplex streams,
	// whose prototype chain only points to Readable.
	var realHasInstance;
	if (typeof Symbol === 'function' && Symbol.hasInstance && typeof Function.prototype[Symbol.hasInstance] === 'function') {
	  realHasInstance = Function.prototype[Symbol.hasInstance];
	  Object.defineProperty(Writable, Symbol.hasInstance, {
	    value: function (object) {
	      if (realHasInstance.call(this, object)) return true;
	      if (this !== Writable) return false;

	      return object && object._writableState instanceof WritableState;
	    }
	  });
	} else {
	  realHasInstance = function (object) {
	    return object instanceof this;
	  };
	}

	function Writable(options) {
	  Duplex = Duplex || __webpack_require__(350);

	  // Writable ctor is applied to Duplexes, too.
	  // `realHasInstance` is necessary because using plain `instanceof`
	  // would return false, as no `_writableState` property is attached.

	  // Trying to use the custom `instanceof` for Writable here will also break the
	  // Node.js LazyTransform implementation, which has a non-trivial getter for
	  // `_writableState` that would lead to infinite recursion.
	  if (!realHasInstance.call(Writable, this) && !(this instanceof Duplex)) {
	    return new Writable(options);
	  }

	  this._writableState = new WritableState(options, this);

	  // legacy.
	  this.writable = true;

	  if (options) {
	    if (typeof options.write === 'function') this._write = options.write;

	    if (typeof options.writev === 'function') this._writev = options.writev;

	    if (typeof options.destroy === 'function') this._destroy = options.destroy;

	    if (typeof options.final === 'function') this._final = options.final;
	  }

	  Stream.call(this);
	}

	// Otherwise people can pipe Writable streams, which is just wrong.
	Writable.prototype.pipe = function () {
	  this.emit('error', new Error('Cannot pipe, not readable'));
	};

	function writeAfterEnd(stream, cb) {
	  var er = new Error('write after end');
	  // TODO: defer error events consistently everywhere, not just the cb
	  stream.emit('error', er);
	  pna.nextTick(cb, er);
	}

	// Checks that a user-supplied chunk is valid, especially for the particular
	// mode the stream is in. Currently this means that `null` is never accepted
	// and undefined/non-string values are only allowed in object mode.
	function validChunk(stream, state, chunk, cb) {
	  var valid = true;
	  var er = false;

	  if (chunk === null) {
	    er = new TypeError('May not write null values to stream');
	  } else if (typeof chunk !== 'string' && chunk !== undefined && !state.objectMode) {
	    er = new TypeError('Invalid non-string/buffer chunk');
	  }
	  if (er) {
	    stream.emit('error', er);
	    pna.nextTick(cb, er);
	    valid = false;
	  }
	  return valid;
	}

	Writable.prototype.write = function (chunk, encoding, cb) {
	  var state = this._writableState;
	  var ret = false;
	  var isBuf = !state.objectMode && _isUint8Array(chunk);

	  if (isBuf && !Buffer.isBuffer(chunk)) {
	    chunk = _uint8ArrayToBuffer(chunk);
	  }

	  if (typeof encoding === 'function') {
	    cb = encoding;
	    encoding = null;
	  }

	  if (isBuf) encoding = 'buffer';else if (!encoding) encoding = state.defaultEncoding;

	  if (typeof cb !== 'function') cb = nop;

	  if (state.ended) writeAfterEnd(this, cb);else if (isBuf || validChunk(this, state, chunk, cb)) {
	    state.pendingcb++;
	    ret = writeOrBuffer(this, state, isBuf, chunk, encoding, cb);
	  }

	  return ret;
	};

	Writable.prototype.cork = function () {
	  var state = this._writableState;

	  state.corked++;
	};

	Writable.prototype.uncork = function () {
	  var state = this._writableState;

	  if (state.corked) {
	    state.corked--;

	    if (!state.writing && !state.corked && !state.finished && !state.bufferProcessing && state.bufferedRequest) clearBuffer(this, state);
	  }
	};

	Writable.prototype.setDefaultEncoding = function setDefaultEncoding(encoding) {
	  // node::ParseEncoding() requires lower case.
	  if (typeof encoding === 'string') encoding = encoding.toLowerCase();
	  if (!(['hex', 'utf8', 'utf-8', 'ascii', 'binary', 'base64', 'ucs2', 'ucs-2', 'utf16le', 'utf-16le', 'raw'].indexOf((encoding + '').toLowerCase()) > -1)) throw new TypeError('Unknown encoding: ' + encoding);
	  this._writableState.defaultEncoding = encoding;
	  return this;
	};

	function decodeChunk(state, chunk, encoding) {
	  if (!state.objectMode && state.decodeStrings !== false && typeof chunk === 'string') {
	    chunk = Buffer.from(chunk, encoding);
	  }
	  return chunk;
	}

	Object.defineProperty(Writable.prototype, 'writableHighWaterMark', {
	  // making it explicit this property is not enumerable
	  // because otherwise some prototype manipulation in
	  // userland will fail
	  enumerable: false,
	  get: function () {
	    return this._writableState.highWaterMark;
	  }
	});

	// if we're already writing something, then just put this
	// in the queue, and wait our turn.  Otherwise, call _write
	// If we return false, then we need a drain event, so set that flag.
	function writeOrBuffer(stream, state, isBuf, chunk, encoding, cb) {
	  if (!isBuf) {
	    var newChunk = decodeChunk(state, chunk, encoding);
	    if (chunk !== newChunk) {
	      isBuf = true;
	      encoding = 'buffer';
	      chunk = newChunk;
	    }
	  }
	  var len = state.objectMode ? 1 : chunk.length;

	  state.length += len;

	  var ret = state.length < state.highWaterMark;
	  // we must ensure that previous needDrain will not be reset to false.
	  if (!ret) state.needDrain = true;

	  if (state.writing || state.corked) {
	    var last = state.lastBufferedRequest;
	    state.lastBufferedRequest = {
	      chunk: chunk,
	      encoding: encoding,
	      isBuf: isBuf,
	      callback: cb,
	      next: null
	    };
	    if (last) {
	      last.next = state.lastBufferedRequest;
	    } else {
	      state.bufferedRequest = state.lastBufferedRequest;
	    }
	    state.bufferedRequestCount += 1;
	  } else {
	    doWrite(stream, state, false, len, chunk, encoding, cb);
	  }

	  return ret;
	}

	function doWrite(stream, state, writev, len, chunk, encoding, cb) {
	  state.writelen = len;
	  state.writecb = cb;
	  state.writing = true;
	  state.sync = true;
	  if (writev) stream._writev(chunk, state.onwrite);else stream._write(chunk, encoding, state.onwrite);
	  state.sync = false;
	}

	function onwriteError(stream, state, sync, er, cb) {
	  --state.pendingcb;

	  if (sync) {
	    // defer the callback if we are being called synchronously
	    // to avoid piling up things on the stack
	    pna.nextTick(cb, er);
	    // this can emit finish, and it will always happen
	    // after error
	    pna.nextTick(finishMaybe, stream, state);
	    stream._writableState.errorEmitted = true;
	    stream.emit('error', er);
	  } else {
	    // the caller expect this to happen before if
	    // it is async
	    cb(er);
	    stream._writableState.errorEmitted = true;
	    stream.emit('error', er);
	    // this can emit finish, but finish must
	    // always follow error
	    finishMaybe(stream, state);
	  }
	}

	function onwriteStateUpdate(state) {
	  state.writing = false;
	  state.writecb = null;
	  state.length -= state.writelen;
	  state.writelen = 0;
	}

	function onwrite(stream, er) {
	  var state = stream._writableState;
	  var sync = state.sync;
	  var cb = state.writecb;

	  onwriteStateUpdate(state);

	  if (er) onwriteError(stream, state, sync, er, cb);else {
	    // Check if we're actually ready to finish, but don't emit yet
	    var finished = needFinish(state);

	    if (!finished && !state.corked && !state.bufferProcessing && state.bufferedRequest) {
	      clearBuffer(stream, state);
	    }

	    if (sync) {
	      /*<replacement>*/
	      asyncWrite(afterWrite, stream, state, finished, cb);
	      /*</replacement>*/
	    } else {
	      afterWrite(stream, state, finished, cb);
	    }
	  }
	}

	function afterWrite(stream, state, finished, cb) {
	  if (!finished) onwriteDrain(stream, state);
	  state.pendingcb--;
	  cb();
	  finishMaybe(stream, state);
	}

	// Must force callback to be called on nextTick, so that we don't
	// emit 'drain' before the write() consumer gets the 'false' return
	// value, and has a chance to attach a 'drain' listener.
	function onwriteDrain(stream, state) {
	  if (state.length === 0 && state.needDrain) {
	    state.needDrain = false;
	    stream.emit('drain');
	  }
	}

	// if there's something in the buffer waiting, then process it
	function clearBuffer(stream, state) {
	  state.bufferProcessing = true;
	  var entry = state.bufferedRequest;

	  if (stream._writev && entry && entry.next) {
	    // Fast case, write everything using _writev()
	    var l = state.bufferedRequestCount;
	    var buffer = new Array(l);
	    var holder = state.corkedRequestsFree;
	    holder.entry = entry;

	    var count = 0;
	    var allBuffers = true;
	    while (entry) {
	      buffer[count] = entry;
	      if (!entry.isBuf) allBuffers = false;
	      entry = entry.next;
	      count += 1;
	    }
	    buffer.allBuffers = allBuffers;

	    doWrite(stream, state, true, state.length, buffer, '', holder.finish);

	    // doWrite is almost always async, defer these to save a bit of time
	    // as the hot path ends with doWrite
	    state.pendingcb++;
	    state.lastBufferedRequest = null;
	    if (holder.next) {
	      state.corkedRequestsFree = holder.next;
	      holder.next = null;
	    } else {
	      state.corkedRequestsFree = new CorkedRequest(state);
	    }
	    state.bufferedRequestCount = 0;
	  } else {
	    // Slow case, write chunks one-by-one
	    while (entry) {
	      var chunk = entry.chunk;
	      var encoding = entry.encoding;
	      var cb = entry.callback;
	      var len = state.objectMode ? 1 : chunk.length;

	      doWrite(stream, state, false, len, chunk, encoding, cb);
	      entry = entry.next;
	      state.bufferedRequestCount--;
	      // if we didn't call the onwrite immediately, then
	      // it means that we need to wait until it does.
	      // also, that means that the chunk and cb are currently
	      // being processed, so move the buffer counter past them.
	      if (state.writing) {
	        break;
	      }
	    }

	    if (entry === null) state.lastBufferedRequest = null;
	  }

	  state.bufferedRequest = entry;
	  state.bufferProcessing = false;
	}

	Writable.prototype._write = function (chunk, encoding, cb) {
	  cb(new Error('_write() is not implemented'));
	};

	Writable.prototype._writev = null;

	Writable.prototype.end = function (chunk, encoding, cb) {
	  var state = this._writableState;

	  if (typeof chunk === 'function') {
	    cb = chunk;
	    chunk = null;
	    encoding = null;
	  } else if (typeof encoding === 'function') {
	    cb = encoding;
	    encoding = null;
	  }

	  if (chunk !== null && chunk !== undefined) this.write(chunk, encoding);

	  // .end() fully uncorks
	  if (state.corked) {
	    state.corked = 1;
	    this.uncork();
	  }

	  // ignore unnecessary end() calls.
	  if (!state.ending && !state.finished) endWritable(this, state, cb);
	};

	function needFinish(state) {
	  return state.ending && state.length === 0 && state.bufferedRequest === null && !state.finished && !state.writing;
	}
	function callFinal(stream, state) {
	  stream._final(function (err) {
	    state.pendingcb--;
	    if (err) {
	      stream.emit('error', err);
	    }
	    state.prefinished = true;
	    stream.emit('prefinish');
	    finishMaybe(stream, state);
	  });
	}
	function prefinish(stream, state) {
	  if (!state.prefinished && !state.finalCalled) {
	    if (typeof stream._final === 'function') {
	      state.pendingcb++;
	      state.finalCalled = true;
	      pna.nextTick(callFinal, stream, state);
	    } else {
	      state.prefinished = true;
	      stream.emit('prefinish');
	    }
	  }
	}

	function finishMaybe(stream, state) {
	  var need = needFinish(state);
	  if (need) {
	    prefinish(stream, state);
	    if (state.pendingcb === 0) {
	      state.finished = true;
	      stream.emit('finish');
	    }
	  }
	  return need;
	}

	function endWritable(stream, state, cb) {
	  state.ending = true;
	  finishMaybe(stream, state);
	  if (cb) {
	    if (state.finished) pna.nextTick(cb);else stream.once('finish', cb);
	  }
	  state.ended = true;
	  stream.writable = false;
	}

	function onCorkedFinish(corkReq, state, err) {
	  var entry = corkReq.entry;
	  corkReq.entry = null;
	  while (entry) {
	    var cb = entry.callback;
	    state.pendingcb--;
	    cb(err);
	    entry = entry.next;
	  }
	  if (state.corkedRequestsFree) {
	    state.corkedRequestsFree.next = corkReq;
	  } else {
	    state.corkedRequestsFree = corkReq;
	  }
	}

	Object.defineProperty(Writable.prototype, 'destroyed', {
	  get: function () {
	    if (this._writableState === undefined) {
	      return false;
	    }
	    return this._writableState.destroyed;
	  },
	  set: function (value) {
	    // we ignore the value if the stream
	    // has not been initialized yet
	    if (!this._writableState) {
	      return;
	    }

	    // backward compatibility, the user is explicitly
	    // managing destroyed
	    this._writableState.destroyed = value;
	  }
	});

	Writable.prototype.destroy = destroyImpl.destroy;
	Writable.prototype._undestroy = destroyImpl.undestroy;
	Writable.prototype._destroy = function (err, cb) {
	  this.end();
	  cb(err);
	};
	/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(328), __webpack_require__(352).setImmediate, (function() { return this; }())))

/***/ }),
/* 352 */
/***/ (function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(global) {var scope = (typeof global !== "undefined" && global) ||
	            (typeof self !== "undefined" && self) ||
	            window;
	var apply = Function.prototype.apply;

	// DOM APIs, for completeness

	exports.setTimeout = function() {
	  return new Timeout(apply.call(setTimeout, scope, arguments), clearTimeout);
	};
	exports.setInterval = function() {
	  return new Timeout(apply.call(setInterval, scope, arguments), clearInterval);
	};
	exports.clearTimeout =
	exports.clearInterval = function(timeout) {
	  if (timeout) {
	    timeout.close();
	  }
	};

	function Timeout(id, clearFn) {
	  this._id = id;
	  this._clearFn = clearFn;
	}
	Timeout.prototype.unref = Timeout.prototype.ref = function() {};
	Timeout.prototype.close = function() {
	  this._clearFn.call(scope, this._id);
	};

	// Does not start the time, just sets up the members needed.
	exports.enroll = function(item, msecs) {
	  clearTimeout(item._idleTimeoutId);
	  item._idleTimeout = msecs;
	};

	exports.unenroll = function(item) {
	  clearTimeout(item._idleTimeoutId);
	  item._idleTimeout = -1;
	};

	exports._unrefActive = exports.active = function(item) {
	  clearTimeout(item._idleTimeoutId);

	  var msecs = item._idleTimeout;
	  if (msecs >= 0) {
	    item._idleTimeoutId = setTimeout(function onTimeout() {
	      if (item._onTimeout)
	        item._onTimeout();
	    }, msecs);
	  }
	};

	// setimmediate attaches itself to the global object
	__webpack_require__(353);
	// On some exotic environments, it's not clear which object `setimmediate` was
	// able to install onto.  Search each possibility in the same order as the
	// `setimmediate` library.
	exports.setImmediate = (typeof self !== "undefined" && self.setImmediate) ||
	                       (typeof global !== "undefined" && global.setImmediate) ||
	                       (this && this.setImmediate);
	exports.clearImmediate = (typeof self !== "undefined" && self.clearImmediate) ||
	                         (typeof global !== "undefined" && global.clearImmediate) ||
	                         (this && this.clearImmediate);

	/* WEBPACK VAR INJECTION */}.call(exports, (function() { return this; }())))

/***/ }),
/* 353 */
/***/ (function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(global, process) {(function (global, undefined) {
	    "use strict";

	    if (global.setImmediate) {
	        return;
	    }

	    var nextHandle = 1; // Spec says greater than zero
	    var tasksByHandle = {};
	    var currentlyRunningATask = false;
	    var doc = global.document;
	    var registerImmediate;

	    function setImmediate(callback) {
	      // Callback can either be a function or a string
	      if (typeof callback !== "function") {
	        callback = new Function("" + callback);
	      }
	      // Copy function arguments
	      var args = new Array(arguments.length - 1);
	      for (var i = 0; i < args.length; i++) {
	          args[i] = arguments[i + 1];
	      }
	      // Store and register the task
	      var task = { callback: callback, args: args };
	      tasksByHandle[nextHandle] = task;
	      registerImmediate(nextHandle);
	      return nextHandle++;
	    }

	    function clearImmediate(handle) {
	        delete tasksByHandle[handle];
	    }

	    function run(task) {
	        var callback = task.callback;
	        var args = task.args;
	        switch (args.length) {
	        case 0:
	            callback();
	            break;
	        case 1:
	            callback(args[0]);
	            break;
	        case 2:
	            callback(args[0], args[1]);
	            break;
	        case 3:
	            callback(args[0], args[1], args[2]);
	            break;
	        default:
	            callback.apply(undefined, args);
	            break;
	        }
	    }

	    function runIfPresent(handle) {
	        // From the spec: "Wait until any invocations of this algorithm started before this one have completed."
	        // So if we're currently running a task, we'll need to delay this invocation.
	        if (currentlyRunningATask) {
	            // Delay by doing a setTimeout. setImmediate was tried instead, but in Firefox 7 it generated a
	            // "too much recursion" error.
	            setTimeout(runIfPresent, 0, handle);
	        } else {
	            var task = tasksByHandle[handle];
	            if (task) {
	                currentlyRunningATask = true;
	                try {
	                    run(task);
	                } finally {
	                    clearImmediate(handle);
	                    currentlyRunningATask = false;
	                }
	            }
	        }
	    }

	    function installNextTickImplementation() {
	        registerImmediate = function(handle) {
	            process.nextTick(function () { runIfPresent(handle); });
	        };
	    }

	    function canUsePostMessage() {
	        // The test against `importScripts` prevents this implementation from being installed inside a web worker,
	        // where `global.postMessage` means something completely different and can't be used for this purpose.
	        if (global.postMessage && !global.importScripts) {
	            var postMessageIsAsynchronous = true;
	            var oldOnMessage = global.onmessage;
	            global.onmessage = function() {
	                postMessageIsAsynchronous = false;
	            };
	            global.postMessage("", "*");
	            global.onmessage = oldOnMessage;
	            return postMessageIsAsynchronous;
	        }
	    }

	    function installPostMessageImplementation() {
	        // Installs an event handler on `global` for the `message` event: see
	        // * https://developer.mozilla.org/en/DOM/window.postMessage
	        // * http://www.whatwg.org/specs/web-apps/current-work/multipage/comms.html#crossDocumentMessages

	        var messagePrefix = "setImmediate$" + Math.random() + "$";
	        var onGlobalMessage = function(event) {
	            if (event.source === global &&
	                typeof event.data === "string" &&
	                event.data.indexOf(messagePrefix) === 0) {
	                runIfPresent(+event.data.slice(messagePrefix.length));
	            }
	        };

	        if (global.addEventListener) {
	            global.addEventListener("message", onGlobalMessage, false);
	        } else {
	            global.attachEvent("onmessage", onGlobalMessage);
	        }

	        registerImmediate = function(handle) {
	            global.postMessage(messagePrefix + handle, "*");
	        };
	    }

	    function installMessageChannelImplementation() {
	        var channel = new MessageChannel();
	        channel.port1.onmessage = function(event) {
	            var handle = event.data;
	            runIfPresent(handle);
	        };

	        registerImmediate = function(handle) {
	            channel.port2.postMessage(handle);
	        };
	    }

	    function installReadyStateChangeImplementation() {
	        var html = doc.documentElement;
	        registerImmediate = function(handle) {
	            // Create a <script> element; its readystatechange event will be fired asynchronously once it is inserted
	            // into the document. Do so, thus queuing up the task. Remember to clean up once it's been called.
	            var script = doc.createElement("script");
	            script.onreadystatechange = function () {
	                runIfPresent(handle);
	                script.onreadystatechange = null;
	                html.removeChild(script);
	                script = null;
	            };
	            html.appendChild(script);
	        };
	    }

	    function installSetTimeoutImplementation() {
	        registerImmediate = function(handle) {
	            setTimeout(runIfPresent, 0, handle);
	        };
	    }

	    // If supported, we should attach to the prototype of global, since that is where setTimeout et al. live.
	    var attachTo = Object.getPrototypeOf && Object.getPrototypeOf(global);
	    attachTo = attachTo && attachTo.setTimeout ? attachTo : global;

	    // Don't get fooled by e.g. browserify environments.
	    if ({}.toString.call(global.process) === "[object process]") {
	        // For Node.js before 0.9
	        installNextTickImplementation();

	    } else if (canUsePostMessage()) {
	        // For non-IE10 modern browsers
	        installPostMessageImplementation();

	    } else if (global.MessageChannel) {
	        // For web workers, where supported
	        installMessageChannelImplementation();

	    } else if (doc && "onreadystatechange" in doc.createElement("script")) {
	        // For IE 6–8
	        installReadyStateChangeImplementation();

	    } else {
	        // For older browsers
	        installSetTimeoutImplementation();
	    }

	    attachTo.setImmediate = setImmediate;
	    attachTo.clearImmediate = clearImmediate;
	}(typeof self === "undefined" ? typeof global === "undefined" ? this : global : self));

	/* WEBPACK VAR INJECTION */}.call(exports, (function() { return this; }()), __webpack_require__(328)))

/***/ }),
/* 354 */
/***/ (function(module, exports) {

	/* WEBPACK VAR INJECTION */(function(global) {
	/**
	 * Module exports.
	 */

	module.exports = deprecate;

	/**
	 * Mark that a method should not be used.
	 * Returns a modified function which warns once by default.
	 *
	 * If `localStorage.noDeprecation = true` is set, then it is a no-op.
	 *
	 * If `localStorage.throwDeprecation = true` is set, then deprecated functions
	 * will throw an Error when invoked.
	 *
	 * If `localStorage.traceDeprecation = true` is set, then deprecated functions
	 * will invoke `console.trace()` instead of `console.error()`.
	 *
	 * @param {Function} fn - the function to deprecate
	 * @param {String} msg - the string to print to the console when `fn` is invoked
	 * @returns {Function} a new "deprecated" version of `fn`
	 * @api public
	 */

	function deprecate (fn, msg) {
	  if (config('noDeprecation')) {
	    return fn;
	  }

	  var warned = false;
	  function deprecated() {
	    if (!warned) {
	      if (config('throwDeprecation')) {
	        throw new Error(msg);
	      } else if (config('traceDeprecation')) {
	        console.trace(msg);
	      } else {
	        console.warn(msg);
	      }
	      warned = true;
	    }
	    return fn.apply(this, arguments);
	  }

	  return deprecated;
	}

	/**
	 * Checks `localStorage` for boolean values for the given `name`.
	 *
	 * @param {String} name
	 * @returns {Boolean}
	 * @api private
	 */

	function config (name) {
	  // accessing global.localStorage can trigger a DOMException in sandboxed iframes
	  try {
	    if (!global.localStorage) return false;
	  } catch (_) {
	    return false;
	  }
	  var val = global.localStorage[name];
	  if (null == val) return false;
	  return String(val).toLowerCase() === 'true';
	}

	/* WEBPACK VAR INJECTION */}.call(exports, (function() { return this; }())))

/***/ }),
/* 355 */
/***/ (function(module, exports, __webpack_require__) {

	// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	'use strict';

	/*<replacement>*/

	var Buffer = __webpack_require__(344).Buffer;
	/*</replacement>*/

	var isEncoding = Buffer.isEncoding || function (encoding) {
	  encoding = '' + encoding;
	  switch (encoding && encoding.toLowerCase()) {
	    case 'hex':case 'utf8':case 'utf-8':case 'ascii':case 'binary':case 'base64':case 'ucs2':case 'ucs-2':case 'utf16le':case 'utf-16le':case 'raw':
	      return true;
	    default:
	      return false;
	  }
	};

	function _normalizeEncoding(enc) {
	  if (!enc) return 'utf8';
	  var retried;
	  while (true) {
	    switch (enc) {
	      case 'utf8':
	      case 'utf-8':
	        return 'utf8';
	      case 'ucs2':
	      case 'ucs-2':
	      case 'utf16le':
	      case 'utf-16le':
	        return 'utf16le';
	      case 'latin1':
	      case 'binary':
	        return 'latin1';
	      case 'base64':
	      case 'ascii':
	      case 'hex':
	        return enc;
	      default:
	        if (retried) return; // undefined
	        enc = ('' + enc).toLowerCase();
	        retried = true;
	    }
	  }
	};

	// Do not cache `Buffer.isEncoding` when checking encoding names as some
	// modules monkey-patch it to support additional encodings
	function normalizeEncoding(enc) {
	  var nenc = _normalizeEncoding(enc);
	  if (typeof nenc !== 'string' && (Buffer.isEncoding === isEncoding || !isEncoding(enc))) throw new Error('Unknown encoding: ' + enc);
	  return nenc || enc;
	}

	// StringDecoder provides an interface for efficiently splitting a series of
	// buffers into a series of JS strings without breaking apart multi-byte
	// characters.
	exports.StringDecoder = StringDecoder;
	function StringDecoder(encoding) {
	  this.encoding = normalizeEncoding(encoding);
	  var nb;
	  switch (this.encoding) {
	    case 'utf16le':
	      this.text = utf16Text;
	      this.end = utf16End;
	      nb = 4;
	      break;
	    case 'utf8':
	      this.fillLast = utf8FillLast;
	      nb = 4;
	      break;
	    case 'base64':
	      this.text = base64Text;
	      this.end = base64End;
	      nb = 3;
	      break;
	    default:
	      this.write = simpleWrite;
	      this.end = simpleEnd;
	      return;
	  }
	  this.lastNeed = 0;
	  this.lastTotal = 0;
	  this.lastChar = Buffer.allocUnsafe(nb);
	}

	StringDecoder.prototype.write = function (buf) {
	  if (buf.length === 0) return '';
	  var r;
	  var i;
	  if (this.lastNeed) {
	    r = this.fillLast(buf);
	    if (r === undefined) return '';
	    i = this.lastNeed;
	    this.lastNeed = 0;
	  } else {
	    i = 0;
	  }
	  if (i < buf.length) return r ? r + this.text(buf, i) : this.text(buf, i);
	  return r || '';
	};

	StringDecoder.prototype.end = utf8End;

	// Returns only complete characters in a Buffer
	StringDecoder.prototype.text = utf8Text;

	// Attempts to complete a partial non-UTF-8 character using bytes from a Buffer
	StringDecoder.prototype.fillLast = function (buf) {
	  if (this.lastNeed <= buf.length) {
	    buf.copy(this.lastChar, this.lastTotal - this.lastNeed, 0, this.lastNeed);
	    return this.lastChar.toString(this.encoding, 0, this.lastTotal);
	  }
	  buf.copy(this.lastChar, this.lastTotal - this.lastNeed, 0, buf.length);
	  this.lastNeed -= buf.length;
	};

	// Checks the type of a UTF-8 byte, whether it's ASCII, a leading byte, or a
	// continuation byte. If an invalid byte is detected, -2 is returned.
	function utf8CheckByte(byte) {
	  if (byte <= 0x7F) return 0;else if (byte >> 5 === 0x06) return 2;else if (byte >> 4 === 0x0E) return 3;else if (byte >> 3 === 0x1E) return 4;
	  return byte >> 6 === 0x02 ? -1 : -2;
	}

	// Checks at most 3 bytes at the end of a Buffer in order to detect an
	// incomplete multi-byte UTF-8 character. The total number of bytes (2, 3, or 4)
	// needed to complete the UTF-8 character (if applicable) are returned.
	function utf8CheckIncomplete(self, buf, i) {
	  var j = buf.length - 1;
	  if (j < i) return 0;
	  var nb = utf8CheckByte(buf[j]);
	  if (nb >= 0) {
	    if (nb > 0) self.lastNeed = nb - 1;
	    return nb;
	  }
	  if (--j < i || nb === -2) return 0;
	  nb = utf8CheckByte(buf[j]);
	  if (nb >= 0) {
	    if (nb > 0) self.lastNeed = nb - 2;
	    return nb;
	  }
	  if (--j < i || nb === -2) return 0;
	  nb = utf8CheckByte(buf[j]);
	  if (nb >= 0) {
	    if (nb > 0) {
	      if (nb === 2) nb = 0;else self.lastNeed = nb - 3;
	    }
	    return nb;
	  }
	  return 0;
	}

	// Validates as many continuation bytes for a multi-byte UTF-8 character as
	// needed or are available. If we see a non-continuation byte where we expect
	// one, we "replace" the validated continuation bytes we've seen so far with
	// a single UTF-8 replacement character ('\ufffd'), to match v8's UTF-8 decoding
	// behavior. The continuation byte check is included three times in the case
	// where all of the continuation bytes for a character exist in the same buffer.
	// It is also done this way as a slight performance increase instead of using a
	// loop.
	function utf8CheckExtraBytes(self, buf, p) {
	  if ((buf[0] & 0xC0) !== 0x80) {
	    self.lastNeed = 0;
	    return '\ufffd';
	  }
	  if (self.lastNeed > 1 && buf.length > 1) {
	    if ((buf[1] & 0xC0) !== 0x80) {
	      self.lastNeed = 1;
	      return '\ufffd';
	    }
	    if (self.lastNeed > 2 && buf.length > 2) {
	      if ((buf[2] & 0xC0) !== 0x80) {
	        self.lastNeed = 2;
	        return '\ufffd';
	      }
	    }
	  }
	}

	// Attempts to complete a multi-byte UTF-8 character using bytes from a Buffer.
	function utf8FillLast(buf) {
	  var p = this.lastTotal - this.lastNeed;
	  var r = utf8CheckExtraBytes(this, buf, p);
	  if (r !== undefined) return r;
	  if (this.lastNeed <= buf.length) {
	    buf.copy(this.lastChar, p, 0, this.lastNeed);
	    return this.lastChar.toString(this.encoding, 0, this.lastTotal);
	  }
	  buf.copy(this.lastChar, p, 0, buf.length);
	  this.lastNeed -= buf.length;
	}

	// Returns all complete UTF-8 characters in a Buffer. If the Buffer ended on a
	// partial character, the character's bytes are buffered until the required
	// number of bytes are available.
	function utf8Text(buf, i) {
	  var total = utf8CheckIncomplete(this, buf, i);
	  if (!this.lastNeed) return buf.toString('utf8', i);
	  this.lastTotal = total;
	  var end = buf.length - (total - this.lastNeed);
	  buf.copy(this.lastChar, 0, end);
	  return buf.toString('utf8', i, end);
	}

	// For UTF-8, a replacement character is added when ending on a partial
	// character.
	function utf8End(buf) {
	  var r = buf && buf.length ? this.write(buf) : '';
	  if (this.lastNeed) return r + '\ufffd';
	  return r;
	}

	// UTF-16LE typically needs two bytes per character, but even if we have an even
	// number of bytes available, we need to check if we end on a leading/high
	// surrogate. In that case, we need to wait for the next two bytes in order to
	// decode the last character properly.
	function utf16Text(buf, i) {
	  if ((buf.length - i) % 2 === 0) {
	    var r = buf.toString('utf16le', i);
	    if (r) {
	      var c = r.charCodeAt(r.length - 1);
	      if (c >= 0xD800 && c <= 0xDBFF) {
	        this.lastNeed = 2;
	        this.lastTotal = 4;
	        this.lastChar[0] = buf[buf.length - 2];
	        this.lastChar[1] = buf[buf.length - 1];
	        return r.slice(0, -1);
	      }
	    }
	    return r;
	  }
	  this.lastNeed = 1;
	  this.lastTotal = 2;
	  this.lastChar[0] = buf[buf.length - 1];
	  return buf.toString('utf16le', i, buf.length - 1);
	}

	// For UTF-16LE we do not explicitly append special replacement characters if we
	// end on a partial character, we simply let v8 handle that.
	function utf16End(buf) {
	  var r = buf && buf.length ? this.write(buf) : '';
	  if (this.lastNeed) {
	    var end = this.lastTotal - this.lastNeed;
	    return r + this.lastChar.toString('utf16le', 0, end);
	  }
	  return r;
	}

	function base64Text(buf, i) {
	  var n = (buf.length - i) % 3;
	  if (n === 0) return buf.toString('base64', i);
	  this.lastNeed = 3 - n;
	  this.lastTotal = 3;
	  if (n === 1) {
	    this.lastChar[0] = buf[buf.length - 1];
	  } else {
	    this.lastChar[0] = buf[buf.length - 2];
	    this.lastChar[1] = buf[buf.length - 1];
	  }
	  return buf.toString('base64', i, buf.length - n);
	}

	function base64End(buf) {
	  var r = buf && buf.length ? this.write(buf) : '';
	  if (this.lastNeed) return r + this.lastChar.toString('base64', 0, 3 - this.lastNeed);
	  return r;
	}

	// Pass bytes on through for single-byte encodings (e.g. ascii, latin1, hex)
	function simpleWrite(buf) {
	  return buf.toString(this.encoding);
	}

	function simpleEnd(buf) {
	  return buf && buf.length ? this.write(buf) : '';
	}

/***/ }),
/* 356 */
/***/ (function(module, exports, __webpack_require__) {

	// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	// a transform stream is a readable/writable stream where you do
	// something with the data.  Sometimes it's called a "filter",
	// but that's not a great name for it, since that implies a thing where
	// some bits pass through, and others are simply ignored.  (That would
	// be a valid example of a transform, of course.)
	//
	// While the output is causally related to the input, it's not a
	// necessarily symmetric or synchronous transformation.  For example,
	// a zlib stream might take multiple plain-text writes(), and then
	// emit a single compressed chunk some time in the future.
	//
	// Here's how this works:
	//
	// The Transform stream has all the aspects of the readable and writable
	// stream classes.  When you write(chunk), that calls _write(chunk,cb)
	// internally, and returns false if there's a lot of pending writes
	// buffered up.  When you call read(), that calls _read(n) until
	// there's enough pending readable data buffered up.
	//
	// In a transform stream, the written data is placed in a buffer.  When
	// _read(n) is called, it transforms the queued up data, calling the
	// buffered _write cb's as it consumes chunks.  If consuming a single
	// written chunk would result in multiple output chunks, then the first
	// outputted bit calls the readcb, and subsequent chunks just go into
	// the read buffer, and will cause it to emit 'readable' if necessary.
	//
	// This way, back-pressure is actually determined by the reading side,
	// since _read has to be called to start processing a new chunk.  However,
	// a pathological inflate type of transform can cause excessive buffering
	// here.  For example, imagine a stream where every byte of input is
	// interpreted as an integer from 0-255, and then results in that many
	// bytes of output.  Writing the 4 bytes {ff,ff,ff,ff} would result in
	// 1kb of data being output.  In this case, you could write a very small
	// amount of input, and end up with a very large amount of output.  In
	// such a pathological inflating mechanism, there'd be no way to tell
	// the system to stop doing the transform.  A single 4MB write could
	// cause the system to run out of memory.
	//
	// However, even in such a pathological case, only a single written chunk
	// would be consumed, and then the rest would wait (un-transformed) until
	// the results of the previous transformed chunk were consumed.

	'use strict';

	module.exports = Transform;

	var Duplex = __webpack_require__(350);

	/*<replacement>*/
	var util = __webpack_require__(345);
	util.inherits = __webpack_require__(338);
	/*</replacement>*/

	util.inherits(Transform, Duplex);

	function afterTransform(er, data) {
	  var ts = this._transformState;
	  ts.transforming = false;

	  var cb = ts.writecb;

	  if (!cb) {
	    return this.emit('error', new Error('write callback called multiple times'));
	  }

	  ts.writechunk = null;
	  ts.writecb = null;

	  if (data != null) // single equals check for both `null` and `undefined`
	    this.push(data);

	  cb(er);

	  var rs = this._readableState;
	  rs.reading = false;
	  if (rs.needReadable || rs.length < rs.highWaterMark) {
	    this._read(rs.highWaterMark);
	  }
	}

	function Transform(options) {
	  if (!(this instanceof Transform)) return new Transform(options);

	  Duplex.call(this, options);

	  this._transformState = {
	    afterTransform: afterTransform.bind(this),
	    needTransform: false,
	    transforming: false,
	    writecb: null,
	    writechunk: null,
	    writeencoding: null
	  };

	  // start out asking for a readable event once data is transformed.
	  this._readableState.needReadable = true;

	  // we have implemented the _read method, and done the other things
	  // that Readable wants before the first _read call, so unset the
	  // sync guard flag.
	  this._readableState.sync = false;

	  if (options) {
	    if (typeof options.transform === 'function') this._transform = options.transform;

	    if (typeof options.flush === 'function') this._flush = options.flush;
	  }

	  // When the writable side finishes, then flush out anything remaining.
	  this.on('prefinish', prefinish);
	}

	function prefinish() {
	  var _this = this;

	  if (typeof this._flush === 'function') {
	    this._flush(function (er, data) {
	      done(_this, er, data);
	    });
	  } else {
	    done(this, null, null);
	  }
	}

	Transform.prototype.push = function (chunk, encoding) {
	  this._transformState.needTransform = false;
	  return Duplex.prototype.push.call(this, chunk, encoding);
	};

	// This is the part where you do stuff!
	// override this function in implementation classes.
	// 'chunk' is an input chunk.
	//
	// Call `push(newChunk)` to pass along transformed output
	// to the readable side.  You may call 'push' zero or more times.
	//
	// Call `cb(err)` when you are done with this chunk.  If you pass
	// an error, then that'll put the hurt on the whole operation.  If you
	// never call cb(), then you'll never get another chunk.
	Transform.prototype._transform = function (chunk, encoding, cb) {
	  throw new Error('_transform() is not implemented');
	};

	Transform.prototype._write = function (chunk, encoding, cb) {
	  var ts = this._transformState;
	  ts.writecb = cb;
	  ts.writechunk = chunk;
	  ts.writeencoding = encoding;
	  if (!ts.transforming) {
	    var rs = this._readableState;
	    if (ts.needTransform || rs.needReadable || rs.length < rs.highWaterMark) this._read(rs.highWaterMark);
	  }
	};

	// Doesn't matter what the args are here.
	// _transform does all the work.
	// That we got here means that the readable side wants more data.
	Transform.prototype._read = function (n) {
	  var ts = this._transformState;

	  if (ts.writechunk !== null && ts.writecb && !ts.transforming) {
	    ts.transforming = true;
	    this._transform(ts.writechunk, ts.writeencoding, ts.afterTransform);
	  } else {
	    // mark that we need a transform, so that any data that comes in
	    // will get processed, now that we've asked for it.
	    ts.needTransform = true;
	  }
	};

	Transform.prototype._destroy = function (err, cb) {
	  var _this2 = this;

	  Duplex.prototype._destroy.call(this, err, function (err2) {
	    cb(err2);
	    _this2.emit('close');
	  });
	};

	function done(stream, er, data) {
	  if (er) return stream.emit('error', er);

	  if (data != null) // single equals check for both `null` and `undefined`
	    stream.push(data);

	  // if there's nothing in the write buffer, then that means
	  // that nothing more will ever be provided
	  if (stream._writableState.length) throw new Error('Calling transform done when ws.length != 0');

	  if (stream._transformState.transforming) throw new Error('Calling transform done when still transforming');

	  return stream.push(null);
	}

/***/ }),
/* 357 */
/***/ (function(module, exports, __webpack_require__) {

	// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	// a passthrough stream.
	// basically just the most minimal sort of Transform stream.
	// Every written chunk gets output as-is.

	'use strict';

	module.exports = PassThrough;

	var Transform = __webpack_require__(356);

	/*<replacement>*/
	var util = __webpack_require__(345);
	util.inherits = __webpack_require__(338);
	/*</replacement>*/

	util.inherits(PassThrough, Transform);

	function PassThrough(options) {
	  if (!(this instanceof PassThrough)) return new PassThrough(options);

	  Transform.call(this, options);
	}

	PassThrough.prototype._transform = function (chunk, encoding, cb) {
	  cb(null, chunk);
	};

/***/ }),
/* 358 */
/***/ (function(module, exports, __webpack_require__) {

	var Buffer = __webpack_require__(333).Buffer

	module.exports = function (buf) {
		// If the buffer is backed by a Uint8Array, a faster version will work
		if (buf instanceof Uint8Array) {
			// If the buffer isn't a subarray, return the underlying ArrayBuffer
			if (buf.byteOffset === 0 && buf.byteLength === buf.buffer.byteLength) {
				return buf.buffer
			} else if (typeof buf.buffer.slice === 'function') {
				// Otherwise we need to get a proper copy
				return buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength)
			}
		}

		if (Buffer.isBuffer(buf)) {
			// This is the slow version that will work with any Buffer
			// implementation (even in old browsers)
			var arrayCopy = new Uint8Array(buf.length)
			var len = buf.length
			for (var i = 0; i < len; i++) {
				arrayCopy[i] = buf[i]
			}
			return arrayCopy.buffer
		} else {
			throw new Error('Argument must be a Buffer')
		}
	}


/***/ }),
/* 359 */
/***/ (function(module, exports) {

	module.exports = extend

	var hasOwnProperty = Object.prototype.hasOwnProperty;

	function extend() {
	    var target = {}

	    for (var i = 0; i < arguments.length; i++) {
	        var source = arguments[i]

	        for (var key in source) {
	            if (hasOwnProperty.call(source, key)) {
	                target[key] = source[key]
	            }
	        }
	    }

	    return target
	}


/***/ }),
/* 360 */
/***/ (function(module, exports) {

	module.exports = {
	  "100": "Continue",
	  "101": "Switching Protocols",
	  "102": "Processing",
	  "200": "OK",
	  "201": "Created",
	  "202": "Accepted",
	  "203": "Non-Authoritative Information",
	  "204": "No Content",
	  "205": "Reset Content",
	  "206": "Partial Content",
	  "207": "Multi-Status",
	  "208": "Already Reported",
	  "226": "IM Used",
	  "300": "Multiple Choices",
	  "301": "Moved Permanently",
	  "302": "Found",
	  "303": "See Other",
	  "304": "Not Modified",
	  "305": "Use Proxy",
	  "307": "Temporary Redirect",
	  "308": "Permanent Redirect",
	  "400": "Bad Request",
	  "401": "Unauthorized",
	  "402": "Payment Required",
	  "403": "Forbidden",
	  "404": "Not Found",
	  "405": "Method Not Allowed",
	  "406": "Not Acceptable",
	  "407": "Proxy Authentication Required",
	  "408": "Request Timeout",
	  "409": "Conflict",
	  "410": "Gone",
	  "411": "Length Required",
	  "412": "Precondition Failed",
	  "413": "Payload Too Large",
	  "414": "URI Too Long",
	  "415": "Unsupported Media Type",
	  "416": "Range Not Satisfiable",
	  "417": "Expectation Failed",
	  "418": "I'm a teapot",
	  "421": "Misdirected Request",
	  "422": "Unprocessable Entity",
	  "423": "Locked",
	  "424": "Failed Dependency",
	  "425": "Unordered Collection",
	  "426": "Upgrade Required",
	  "428": "Precondition Required",
	  "429": "Too Many Requests",
	  "431": "Request Header Fields Too Large",
	  "451": "Unavailable For Legal Reasons",
	  "500": "Internal Server Error",
	  "501": "Not Implemented",
	  "502": "Bad Gateway",
	  "503": "Service Unavailable",
	  "504": "Gateway Timeout",
	  "505": "HTTP Version Not Supported",
	  "506": "Variant Also Negotiates",
	  "507": "Insufficient Storage",
	  "508": "Loop Detected",
	  "509": "Bandwidth Limit Exceeded",
	  "510": "Not Extended",
	  "511": "Network Authentication Required"
	}


/***/ }),
/* 361 */
/***/ (function(module, exports, __webpack_require__) {

	// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	'use strict';

	var punycode = __webpack_require__(362);
	var util = __webpack_require__(364);

	exports.parse = urlParse;
	exports.resolve = urlResolve;
	exports.resolveObject = urlResolveObject;
	exports.format = urlFormat;

	exports.Url = Url;

	function Url() {
	  this.protocol = null;
	  this.slashes = null;
	  this.auth = null;
	  this.host = null;
	  this.port = null;
	  this.hostname = null;
	  this.hash = null;
	  this.search = null;
	  this.query = null;
	  this.pathname = null;
	  this.path = null;
	  this.href = null;
	}

	// Reference: RFC 3986, RFC 1808, RFC 2396

	// define these here so at least they only have to be
	// compiled once on the first module load.
	var protocolPattern = /^([a-z0-9.+-]+:)/i,
	    portPattern = /:[0-9]*$/,

	    // Special case for a simple path URL
	    simplePathPattern = /^(\/\/?(?!\/)[^\?\s]*)(\?[^\s]*)?$/,

	    // RFC 2396: characters reserved for delimiting URLs.
	    // We actually just auto-escape these.
	    delims = ['<', '>', '"', '`', ' ', '\r', '\n', '\t'],

	    // RFC 2396: characters not allowed for various reasons.
	    unwise = ['{', '}', '|', '\\', '^', '`'].concat(delims),

	    // Allowed by RFCs, but cause of XSS attacks.  Always escape these.
	    autoEscape = ['\''].concat(unwise),
	    // Characters that are never ever allowed in a hostname.
	    // Note that any invalid chars are also handled, but these
	    // are the ones that are *expected* to be seen, so we fast-path
	    // them.
	    nonHostChars = ['%', '/', '?', ';', '#'].concat(autoEscape),
	    hostEndingChars = ['/', '?', '#'],
	    hostnameMaxLen = 255,
	    hostnamePartPattern = /^[+a-z0-9A-Z_-]{0,63}$/,
	    hostnamePartStart = /^([+a-z0-9A-Z_-]{0,63})(.*)$/,
	    // protocols that can allow "unsafe" and "unwise" chars.
	    unsafeProtocol = {
	      'javascript': true,
	      'javascript:': true
	    },
	    // protocols that never have a hostname.
	    hostlessProtocol = {
	      'javascript': true,
	      'javascript:': true
	    },
	    // protocols that always contain a // bit.
	    slashedProtocol = {
	      'http': true,
	      'https': true,
	      'ftp': true,
	      'gopher': true,
	      'file': true,
	      'http:': true,
	      'https:': true,
	      'ftp:': true,
	      'gopher:': true,
	      'file:': true
	    },
	    querystring = __webpack_require__(365);

	function urlParse(url, parseQueryString, slashesDenoteHost) {
	  if (url && util.isObject(url) && url instanceof Url) return url;

	  var u = new Url;
	  u.parse(url, parseQueryString, slashesDenoteHost);
	  return u;
	}

	Url.prototype.parse = function(url, parseQueryString, slashesDenoteHost) {
	  if (!util.isString(url)) {
	    throw new TypeError("Parameter 'url' must be a string, not " + typeof url);
	  }

	  // Copy chrome, IE, opera backslash-handling behavior.
	  // Back slashes before the query string get converted to forward slashes
	  // See: https://code.google.com/p/chromium/issues/detail?id=25916
	  var queryIndex = url.indexOf('?'),
	      splitter =
	          (queryIndex !== -1 && queryIndex < url.indexOf('#')) ? '?' : '#',
	      uSplit = url.split(splitter),
	      slashRegex = /\\/g;
	  uSplit[0] = uSplit[0].replace(slashRegex, '/');
	  url = uSplit.join(splitter);

	  var rest = url;

	  // trim before proceeding.
	  // This is to support parse stuff like "  http://foo.com  \n"
	  rest = rest.trim();

	  if (!slashesDenoteHost && url.split('#').length === 1) {
	    // Try fast path regexp
	    var simplePath = simplePathPattern.exec(rest);
	    if (simplePath) {
	      this.path = rest;
	      this.href = rest;
	      this.pathname = simplePath[1];
	      if (simplePath[2]) {
	        this.search = simplePath[2];
	        if (parseQueryString) {
	          this.query = querystring.parse(this.search.substr(1));
	        } else {
	          this.query = this.search.substr(1);
	        }
	      } else if (parseQueryString) {
	        this.search = '';
	        this.query = {};
	      }
	      return this;
	    }
	  }

	  var proto = protocolPattern.exec(rest);
	  if (proto) {
	    proto = proto[0];
	    var lowerProto = proto.toLowerCase();
	    this.protocol = lowerProto;
	    rest = rest.substr(proto.length);
	  }

	  // figure out if it's got a host
	  // user@server is *always* interpreted as a hostname, and url
	  // resolution will treat //foo/bar as host=foo,path=bar because that's
	  // how the browser resolves relative URLs.
	  if (slashesDenoteHost || proto || rest.match(/^\/\/[^@\/]+@[^@\/]+/)) {
	    var slashes = rest.substr(0, 2) === '//';
	    if (slashes && !(proto && hostlessProtocol[proto])) {
	      rest = rest.substr(2);
	      this.slashes = true;
	    }
	  }

	  if (!hostlessProtocol[proto] &&
	      (slashes || (proto && !slashedProtocol[proto]))) {

	    // there's a hostname.
	    // the first instance of /, ?, ;, or # ends the host.
	    //
	    // If there is an @ in the hostname, then non-host chars *are* allowed
	    // to the left of the last @ sign, unless some host-ending character
	    // comes *before* the @-sign.
	    // URLs are obnoxious.
	    //
	    // ex:
	    // http://a@b@c/ => user:a@b host:c
	    // http://a@b?@c => user:a host:c path:/?@c

	    // v0.12 TODO(isaacs): This is not quite how Chrome does things.
	    // Review our test case against browsers more comprehensively.

	    // find the first instance of any hostEndingChars
	    var hostEnd = -1;
	    for (var i = 0; i < hostEndingChars.length; i++) {
	      var hec = rest.indexOf(hostEndingChars[i]);
	      if (hec !== -1 && (hostEnd === -1 || hec < hostEnd))
	        hostEnd = hec;
	    }

	    // at this point, either we have an explicit point where the
	    // auth portion cannot go past, or the last @ char is the decider.
	    var auth, atSign;
	    if (hostEnd === -1) {
	      // atSign can be anywhere.
	      atSign = rest.lastIndexOf('@');
	    } else {
	      // atSign must be in auth portion.
	      // http://a@b/c@d => host:b auth:a path:/c@d
	      atSign = rest.lastIndexOf('@', hostEnd);
	    }

	    // Now we have a portion which is definitely the auth.
	    // Pull that off.
	    if (atSign !== -1) {
	      auth = rest.slice(0, atSign);
	      rest = rest.slice(atSign + 1);
	      this.auth = decodeURIComponent(auth);
	    }

	    // the host is the remaining to the left of the first non-host char
	    hostEnd = -1;
	    for (var i = 0; i < nonHostChars.length; i++) {
	      var hec = rest.indexOf(nonHostChars[i]);
	      if (hec !== -1 && (hostEnd === -1 || hec < hostEnd))
	        hostEnd = hec;
	    }
	    // if we still have not hit it, then the entire thing is a host.
	    if (hostEnd === -1)
	      hostEnd = rest.length;

	    this.host = rest.slice(0, hostEnd);
	    rest = rest.slice(hostEnd);

	    // pull out port.
	    this.parseHost();

	    // we've indicated that there is a hostname,
	    // so even if it's empty, it has to be present.
	    this.hostname = this.hostname || '';

	    // if hostname begins with [ and ends with ]
	    // assume that it's an IPv6 address.
	    var ipv6Hostname = this.hostname[0] === '[' &&
	        this.hostname[this.hostname.length - 1] === ']';

	    // validate a little.
	    if (!ipv6Hostname) {
	      var hostparts = this.hostname.split(/\./);
	      for (var i = 0, l = hostparts.length; i < l; i++) {
	        var part = hostparts[i];
	        if (!part) continue;
	        if (!part.match(hostnamePartPattern)) {
	          var newpart = '';
	          for (var j = 0, k = part.length; j < k; j++) {
	            if (part.charCodeAt(j) > 127) {
	              // we replace non-ASCII char with a temporary placeholder
	              // we need this to make sure size of hostname is not
	              // broken by replacing non-ASCII by nothing
	              newpart += 'x';
	            } else {
	              newpart += part[j];
	            }
	          }
	          // we test again with ASCII char only
	          if (!newpart.match(hostnamePartPattern)) {
	            var validParts = hostparts.slice(0, i);
	            var notHost = hostparts.slice(i + 1);
	            var bit = part.match(hostnamePartStart);
	            if (bit) {
	              validParts.push(bit[1]);
	              notHost.unshift(bit[2]);
	            }
	            if (notHost.length) {
	              rest = '/' + notHost.join('.') + rest;
	            }
	            this.hostname = validParts.join('.');
	            break;
	          }
	        }
	      }
	    }

	    if (this.hostname.length > hostnameMaxLen) {
	      this.hostname = '';
	    } else {
	      // hostnames are always lower case.
	      this.hostname = this.hostname.toLowerCase();
	    }

	    if (!ipv6Hostname) {
	      // IDNA Support: Returns a punycoded representation of "domain".
	      // It only converts parts of the domain name that
	      // have non-ASCII characters, i.e. it doesn't matter if
	      // you call it with a domain that already is ASCII-only.
	      this.hostname = punycode.toASCII(this.hostname);
	    }

	    var p = this.port ? ':' + this.port : '';
	    var h = this.hostname || '';
	    this.host = h + p;
	    this.href += this.host;

	    // strip [ and ] from the hostname
	    // the host field still retains them, though
	    if (ipv6Hostname) {
	      this.hostname = this.hostname.substr(1, this.hostname.length - 2);
	      if (rest[0] !== '/') {
	        rest = '/' + rest;
	      }
	    }
	  }

	  // now rest is set to the post-host stuff.
	  // chop off any delim chars.
	  if (!unsafeProtocol[lowerProto]) {

	    // First, make 100% sure that any "autoEscape" chars get
	    // escaped, even if encodeURIComponent doesn't think they
	    // need to be.
	    for (var i = 0, l = autoEscape.length; i < l; i++) {
	      var ae = autoEscape[i];
	      if (rest.indexOf(ae) === -1)
	        continue;
	      var esc = encodeURIComponent(ae);
	      if (esc === ae) {
	        esc = escape(ae);
	      }
	      rest = rest.split(ae).join(esc);
	    }
	  }


	  // chop off from the tail first.
	  var hash = rest.indexOf('#');
	  if (hash !== -1) {
	    // got a fragment string.
	    this.hash = rest.substr(hash);
	    rest = rest.slice(0, hash);
	  }
	  var qm = rest.indexOf('?');
	  if (qm !== -1) {
	    this.search = rest.substr(qm);
	    this.query = rest.substr(qm + 1);
	    if (parseQueryString) {
	      this.query = querystring.parse(this.query);
	    }
	    rest = rest.slice(0, qm);
	  } else if (parseQueryString) {
	    // no query string, but parseQueryString still requested
	    this.search = '';
	    this.query = {};
	  }
	  if (rest) this.pathname = rest;
	  if (slashedProtocol[lowerProto] &&
	      this.hostname && !this.pathname) {
	    this.pathname = '/';
	  }

	  //to support http.request
	  if (this.pathname || this.search) {
	    var p = this.pathname || '';
	    var s = this.search || '';
	    this.path = p + s;
	  }

	  // finally, reconstruct the href based on what has been validated.
	  this.href = this.format();
	  return this;
	};

	// format a parsed object into a url string
	function urlFormat(obj) {
	  // ensure it's an object, and not a string url.
	  // If it's an obj, this is a no-op.
	  // this way, you can call url_format() on strings
	  // to clean up potentially wonky urls.
	  if (util.isString(obj)) obj = urlParse(obj);
	  if (!(obj instanceof Url)) return Url.prototype.format.call(obj);
	  return obj.format();
	}

	Url.prototype.format = function() {
	  var auth = this.auth || '';
	  if (auth) {
	    auth = encodeURIComponent(auth);
	    auth = auth.replace(/%3A/i, ':');
	    auth += '@';
	  }

	  var protocol = this.protocol || '',
	      pathname = this.pathname || '',
	      hash = this.hash || '',
	      host = false,
	      query = '';

	  if (this.host) {
	    host = auth + this.host;
	  } else if (this.hostname) {
	    host = auth + (this.hostname.indexOf(':') === -1 ?
	        this.hostname :
	        '[' + this.hostname + ']');
	    if (this.port) {
	      host += ':' + this.port;
	    }
	  }

	  if (this.query &&
	      util.isObject(this.query) &&
	      Object.keys(this.query).length) {
	    query = querystring.stringify(this.query);
	  }

	  var search = this.search || (query && ('?' + query)) || '';

	  if (protocol && protocol.substr(-1) !== ':') protocol += ':';

	  // only the slashedProtocols get the //.  Not mailto:, xmpp:, etc.
	  // unless they had them to begin with.
	  if (this.slashes ||
	      (!protocol || slashedProtocol[protocol]) && host !== false) {
	    host = '//' + (host || '');
	    if (pathname && pathname.charAt(0) !== '/') pathname = '/' + pathname;
	  } else if (!host) {
	    host = '';
	  }

	  if (hash && hash.charAt(0) !== '#') hash = '#' + hash;
	  if (search && search.charAt(0) !== '?') search = '?' + search;

	  pathname = pathname.replace(/[?#]/g, function(match) {
	    return encodeURIComponent(match);
	  });
	  search = search.replace('#', '%23');

	  return protocol + host + pathname + search + hash;
	};

	function urlResolve(source, relative) {
	  return urlParse(source, false, true).resolve(relative);
	}

	Url.prototype.resolve = function(relative) {
	  return this.resolveObject(urlParse(relative, false, true)).format();
	};

	function urlResolveObject(source, relative) {
	  if (!source) return relative;
	  return urlParse(source, false, true).resolveObject(relative);
	}

	Url.prototype.resolveObject = function(relative) {
	  if (util.isString(relative)) {
	    var rel = new Url();
	    rel.parse(relative, false, true);
	    relative = rel;
	  }

	  var result = new Url();
	  var tkeys = Object.keys(this);
	  for (var tk = 0; tk < tkeys.length; tk++) {
	    var tkey = tkeys[tk];
	    result[tkey] = this[tkey];
	  }

	  // hash is always overridden, no matter what.
	  // even href="" will remove it.
	  result.hash = relative.hash;

	  // if the relative url is empty, then there's nothing left to do here.
	  if (relative.href === '') {
	    result.href = result.format();
	    return result;
	  }

	  // hrefs like //foo/bar always cut to the protocol.
	  if (relative.slashes && !relative.protocol) {
	    // take everything except the protocol from relative
	    var rkeys = Object.keys(relative);
	    for (var rk = 0; rk < rkeys.length; rk++) {
	      var rkey = rkeys[rk];
	      if (rkey !== 'protocol')
	        result[rkey] = relative[rkey];
	    }

	    //urlParse appends trailing / to urls like http://www.example.com
	    if (slashedProtocol[result.protocol] &&
	        result.hostname && !result.pathname) {
	      result.path = result.pathname = '/';
	    }

	    result.href = result.format();
	    return result;
	  }

	  if (relative.protocol && relative.protocol !== result.protocol) {
	    // if it's a known url protocol, then changing
	    // the protocol does weird things
	    // first, if it's not file:, then we MUST have a host,
	    // and if there was a path
	    // to begin with, then we MUST have a path.
	    // if it is file:, then the host is dropped,
	    // because that's known to be hostless.
	    // anything else is assumed to be absolute.
	    if (!slashedProtocol[relative.protocol]) {
	      var keys = Object.keys(relative);
	      for (var v = 0; v < keys.length; v++) {
	        var k = keys[v];
	        result[k] = relative[k];
	      }
	      result.href = result.format();
	      return result;
	    }

	    result.protocol = relative.protocol;
	    if (!relative.host && !hostlessProtocol[relative.protocol]) {
	      var relPath = (relative.pathname || '').split('/');
	      while (relPath.length && !(relative.host = relPath.shift()));
	      if (!relative.host) relative.host = '';
	      if (!relative.hostname) relative.hostname = '';
	      if (relPath[0] !== '') relPath.unshift('');
	      if (relPath.length < 2) relPath.unshift('');
	      result.pathname = relPath.join('/');
	    } else {
	      result.pathname = relative.pathname;
	    }
	    result.search = relative.search;
	    result.query = relative.query;
	    result.host = relative.host || '';
	    result.auth = relative.auth;
	    result.hostname = relative.hostname || relative.host;
	    result.port = relative.port;
	    // to support http.request
	    if (result.pathname || result.search) {
	      var p = result.pathname || '';
	      var s = result.search || '';
	      result.path = p + s;
	    }
	    result.slashes = result.slashes || relative.slashes;
	    result.href = result.format();
	    return result;
	  }

	  var isSourceAbs = (result.pathname && result.pathname.charAt(0) === '/'),
	      isRelAbs = (
	          relative.host ||
	          relative.pathname && relative.pathname.charAt(0) === '/'
	      ),
	      mustEndAbs = (isRelAbs || isSourceAbs ||
	                    (result.host && relative.pathname)),
	      removeAllDots = mustEndAbs,
	      srcPath = result.pathname && result.pathname.split('/') || [],
	      relPath = relative.pathname && relative.pathname.split('/') || [],
	      psychotic = result.protocol && !slashedProtocol[result.protocol];

	  // if the url is a non-slashed url, then relative
	  // links like ../.. should be able
	  // to crawl up to the hostname, as well.  This is strange.
	  // result.protocol has already been set by now.
	  // Later on, put the first path part into the host field.
	  if (psychotic) {
	    result.hostname = '';
	    result.port = null;
	    if (result.host) {
	      if (srcPath[0] === '') srcPath[0] = result.host;
	      else srcPath.unshift(result.host);
	    }
	    result.host = '';
	    if (relative.protocol) {
	      relative.hostname = null;
	      relative.port = null;
	      if (relative.host) {
	        if (relPath[0] === '') relPath[0] = relative.host;
	        else relPath.unshift(relative.host);
	      }
	      relative.host = null;
	    }
	    mustEndAbs = mustEndAbs && (relPath[0] === '' || srcPath[0] === '');
	  }

	  if (isRelAbs) {
	    // it's absolute.
	    result.host = (relative.host || relative.host === '') ?
	                  relative.host : result.host;
	    result.hostname = (relative.hostname || relative.hostname === '') ?
	                      relative.hostname : result.hostname;
	    result.search = relative.search;
	    result.query = relative.query;
	    srcPath = relPath;
	    // fall through to the dot-handling below.
	  } else if (relPath.length) {
	    // it's relative
	    // throw away the existing file, and take the new path instead.
	    if (!srcPath) srcPath = [];
	    srcPath.pop();
	    srcPath = srcPath.concat(relPath);
	    result.search = relative.search;
	    result.query = relative.query;
	  } else if (!util.isNullOrUndefined(relative.search)) {
	    // just pull out the search.
	    // like href='?foo'.
	    // Put this after the other two cases because it simplifies the booleans
	    if (psychotic) {
	      result.hostname = result.host = srcPath.shift();
	      //occationaly the auth can get stuck only in host
	      //this especially happens in cases like
	      //url.resolveObject('mailto:local1@domain1', 'local2@domain2')
	      var authInHost = result.host && result.host.indexOf('@') > 0 ?
	                       result.host.split('@') : false;
	      if (authInHost) {
	        result.auth = authInHost.shift();
	        result.host = result.hostname = authInHost.shift();
	      }
	    }
	    result.search = relative.search;
	    result.query = relative.query;
	    //to support http.request
	    if (!util.isNull(result.pathname) || !util.isNull(result.search)) {
	      result.path = (result.pathname ? result.pathname : '') +
	                    (result.search ? result.search : '');
	    }
	    result.href = result.format();
	    return result;
	  }

	  if (!srcPath.length) {
	    // no path at all.  easy.
	    // we've already handled the other stuff above.
	    result.pathname = null;
	    //to support http.request
	    if (result.search) {
	      result.path = '/' + result.search;
	    } else {
	      result.path = null;
	    }
	    result.href = result.format();
	    return result;
	  }

	  // if a url ENDs in . or .., then it must get a trailing slash.
	  // however, if it ends in anything else non-slashy,
	  // then it must NOT get a trailing slash.
	  var last = srcPath.slice(-1)[0];
	  var hasTrailingSlash = (
	      (result.host || relative.host || srcPath.length > 1) &&
	      (last === '.' || last === '..') || last === '');

	  // strip single dots, resolve double dots to parent dir
	  // if the path tries to go above the root, `up` ends up > 0
	  var up = 0;
	  for (var i = srcPath.length; i >= 0; i--) {
	    last = srcPath[i];
	    if (last === '.') {
	      srcPath.splice(i, 1);
	    } else if (last === '..') {
	      srcPath.splice(i, 1);
	      up++;
	    } else if (up) {
	      srcPath.splice(i, 1);
	      up--;
	    }
	  }

	  // if the path is allowed to go above the root, restore leading ..s
	  if (!mustEndAbs && !removeAllDots) {
	    for (; up--; up) {
	      srcPath.unshift('..');
	    }
	  }

	  if (mustEndAbs && srcPath[0] !== '' &&
	      (!srcPath[0] || srcPath[0].charAt(0) !== '/')) {
	    srcPath.unshift('');
	  }

	  if (hasTrailingSlash && (srcPath.join('/').substr(-1) !== '/')) {
	    srcPath.push('');
	  }

	  var isAbsolute = srcPath[0] === '' ||
	      (srcPath[0] && srcPath[0].charAt(0) === '/');

	  // put the host back
	  if (psychotic) {
	    result.hostname = result.host = isAbsolute ? '' :
	                                    srcPath.length ? srcPath.shift() : '';
	    //occationaly the auth can get stuck only in host
	    //this especially happens in cases like
	    //url.resolveObject('mailto:local1@domain1', 'local2@domain2')
	    var authInHost = result.host && result.host.indexOf('@') > 0 ?
	                     result.host.split('@') : false;
	    if (authInHost) {
	      result.auth = authInHost.shift();
	      result.host = result.hostname = authInHost.shift();
	    }
	  }

	  mustEndAbs = mustEndAbs || (result.host && srcPath.length);

	  if (mustEndAbs && !isAbsolute) {
	    srcPath.unshift('');
	  }

	  if (!srcPath.length) {
	    result.pathname = null;
	    result.path = null;
	  } else {
	    result.pathname = srcPath.join('/');
	  }

	  //to support request.http
	  if (!util.isNull(result.pathname) || !util.isNull(result.search)) {
	    result.path = (result.pathname ? result.pathname : '') +
	                  (result.search ? result.search : '');
	  }
	  result.auth = relative.auth || result.auth;
	  result.slashes = result.slashes || relative.slashes;
	  result.href = result.format();
	  return result;
	};

	Url.prototype.parseHost = function() {
	  var host = this.host;
	  var port = portPattern.exec(host);
	  if (port) {
	    port = port[0];
	    if (port !== ':') {
	      this.port = port.substr(1);
	    }
	    host = host.substr(0, host.length - port.length);
	  }
	  if (host) this.hostname = host;
	};


/***/ }),
/* 362 */
/***/ (function(module, exports, __webpack_require__) {

	var __WEBPACK_AMD_DEFINE_RESULT__;/* WEBPACK VAR INJECTION */(function(module, global) {/*! https://mths.be/punycode v1.3.2 by @mathias */
	;(function(root) {

		/** Detect free variables */
		var freeExports = typeof exports == 'object' && exports &&
			!exports.nodeType && exports;
		var freeModule = typeof module == 'object' && module &&
			!module.nodeType && module;
		var freeGlobal = typeof global == 'object' && global;
		if (
			freeGlobal.global === freeGlobal ||
			freeGlobal.window === freeGlobal ||
			freeGlobal.self === freeGlobal
		) {
			root = freeGlobal;
		}

		/**
		 * The `punycode` object.
		 * @name punycode
		 * @type Object
		 */
		var punycode,

		/** Highest positive signed 32-bit float value */
		maxInt = 2147483647, // aka. 0x7FFFFFFF or 2^31-1

		/** Bootstring parameters */
		base = 36,
		tMin = 1,
		tMax = 26,
		skew = 38,
		damp = 700,
		initialBias = 72,
		initialN = 128, // 0x80
		delimiter = '-', // '\x2D'

		/** Regular expressions */
		regexPunycode = /^xn--/,
		regexNonASCII = /[^\x20-\x7E]/, // unprintable ASCII chars + non-ASCII chars
		regexSeparators = /[\x2E\u3002\uFF0E\uFF61]/g, // RFC 3490 separators

		/** Error messages */
		errors = {
			'overflow': 'Overflow: input needs wider integers to process',
			'not-basic': 'Illegal input >= 0x80 (not a basic code point)',
			'invalid-input': 'Invalid input'
		},

		/** Convenience shortcuts */
		baseMinusTMin = base - tMin,
		floor = Math.floor,
		stringFromCharCode = String.fromCharCode,

		/** Temporary variable */
		key;

		/*--------------------------------------------------------------------------*/

		/**
		 * A generic error utility function.
		 * @private
		 * @param {String} type The error type.
		 * @returns {Error} Throws a `RangeError` with the applicable error message.
		 */
		function error(type) {
			throw RangeError(errors[type]);
		}

		/**
		 * A generic `Array#map` utility function.
		 * @private
		 * @param {Array} array The array to iterate over.
		 * @param {Function} callback The function that gets called for every array
		 * item.
		 * @returns {Array} A new array of values returned by the callback function.
		 */
		function map(array, fn) {
			var length = array.length;
			var result = [];
			while (length--) {
				result[length] = fn(array[length]);
			}
			return result;
		}

		/**
		 * A simple `Array#map`-like wrapper to work with domain name strings or email
		 * addresses.
		 * @private
		 * @param {String} domain The domain name or email address.
		 * @param {Function} callback The function that gets called for every
		 * character.
		 * @returns {Array} A new string of characters returned by the callback
		 * function.
		 */
		function mapDomain(string, fn) {
			var parts = string.split('@');
			var result = '';
			if (parts.length > 1) {
				// In email addresses, only the domain name should be punycoded. Leave
				// the local part (i.e. everything up to `@`) intact.
				result = parts[0] + '@';
				string = parts[1];
			}
			// Avoid `split(regex)` for IE8 compatibility. See #17.
			string = string.replace(regexSeparators, '\x2E');
			var labels = string.split('.');
			var encoded = map(labels, fn).join('.');
			return result + encoded;
		}

		/**
		 * Creates an array containing the numeric code points of each Unicode
		 * character in the string. While JavaScript uses UCS-2 internally,
		 * this function will convert a pair of surrogate halves (each of which
		 * UCS-2 exposes as separate characters) into a single code point,
		 * matching UTF-16.
		 * @see `punycode.ucs2.encode`
		 * @see <https://mathiasbynens.be/notes/javascript-encoding>
		 * @memberOf punycode.ucs2
		 * @name decode
		 * @param {String} string The Unicode input string (UCS-2).
		 * @returns {Array} The new array of code points.
		 */
		function ucs2decode(string) {
			var output = [],
			    counter = 0,
			    length = string.length,
			    value,
			    extra;
			while (counter < length) {
				value = string.charCodeAt(counter++);
				if (value >= 0xD800 && value <= 0xDBFF && counter < length) {
					// high surrogate, and there is a next character
					extra = string.charCodeAt(counter++);
					if ((extra & 0xFC00) == 0xDC00) { // low surrogate
						output.push(((value & 0x3FF) << 10) + (extra & 0x3FF) + 0x10000);
					} else {
						// unmatched surrogate; only append this code unit, in case the next
						// code unit is the high surrogate of a surrogate pair
						output.push(value);
						counter--;
					}
				} else {
					output.push(value);
				}
			}
			return output;
		}

		/**
		 * Creates a string based on an array of numeric code points.
		 * @see `punycode.ucs2.decode`
		 * @memberOf punycode.ucs2
		 * @name encode
		 * @param {Array} codePoints The array of numeric code points.
		 * @returns {String} The new Unicode string (UCS-2).
		 */
		function ucs2encode(array) {
			return map(array, function(value) {
				var output = '';
				if (value > 0xFFFF) {
					value -= 0x10000;
					output += stringFromCharCode(value >>> 10 & 0x3FF | 0xD800);
					value = 0xDC00 | value & 0x3FF;
				}
				output += stringFromCharCode(value);
				return output;
			}).join('');
		}

		/**
		 * Converts a basic code point into a digit/integer.
		 * @see `digitToBasic()`
		 * @private
		 * @param {Number} codePoint The basic numeric code point value.
		 * @returns {Number} The numeric value of a basic code point (for use in
		 * representing integers) in the range `0` to `base - 1`, or `base` if
		 * the code point does not represent a value.
		 */
		function basicToDigit(codePoint) {
			if (codePoint - 48 < 10) {
				return codePoint - 22;
			}
			if (codePoint - 65 < 26) {
				return codePoint - 65;
			}
			if (codePoint - 97 < 26) {
				return codePoint - 97;
			}
			return base;
		}

		/**
		 * Converts a digit/integer into a basic code point.
		 * @see `basicToDigit()`
		 * @private
		 * @param {Number} digit The numeric value of a basic code point.
		 * @returns {Number} The basic code point whose value (when used for
		 * representing integers) is `digit`, which needs to be in the range
		 * `0` to `base - 1`. If `flag` is non-zero, the uppercase form is
		 * used; else, the lowercase form is used. The behavior is undefined
		 * if `flag` is non-zero and `digit` has no uppercase form.
		 */
		function digitToBasic(digit, flag) {
			//  0..25 map to ASCII a..z or A..Z
			// 26..35 map to ASCII 0..9
			return digit + 22 + 75 * (digit < 26) - ((flag != 0) << 5);
		}

		/**
		 * Bias adaptation function as per section 3.4 of RFC 3492.
		 * http://tools.ietf.org/html/rfc3492#section-3.4
		 * @private
		 */
		function adapt(delta, numPoints, firstTime) {
			var k = 0;
			delta = firstTime ? floor(delta / damp) : delta >> 1;
			delta += floor(delta / numPoints);
			for (/* no initialization */; delta > baseMinusTMin * tMax >> 1; k += base) {
				delta = floor(delta / baseMinusTMin);
			}
			return floor(k + (baseMinusTMin + 1) * delta / (delta + skew));
		}

		/**
		 * Converts a Punycode string of ASCII-only symbols to a string of Unicode
		 * symbols.
		 * @memberOf punycode
		 * @param {String} input The Punycode string of ASCII-only symbols.
		 * @returns {String} The resulting string of Unicode symbols.
		 */
		function decode(input) {
			// Don't use UCS-2
			var output = [],
			    inputLength = input.length,
			    out,
			    i = 0,
			    n = initialN,
			    bias = initialBias,
			    basic,
			    j,
			    index,
			    oldi,
			    w,
			    k,
			    digit,
			    t,
			    /** Cached calculation results */
			    baseMinusT;

			// Handle the basic code points: let `basic` be the number of input code
			// points before the last delimiter, or `0` if there is none, then copy
			// the first basic code points to the output.

			basic = input.lastIndexOf(delimiter);
			if (basic < 0) {
				basic = 0;
			}

			for (j = 0; j < basic; ++j) {
				// if it's not a basic code point
				if (input.charCodeAt(j) >= 0x80) {
					error('not-basic');
				}
				output.push(input.charCodeAt(j));
			}

			// Main decoding loop: start just after the last delimiter if any basic code
			// points were copied; start at the beginning otherwise.

			for (index = basic > 0 ? basic + 1 : 0; index < inputLength; /* no final expression */) {

				// `index` is the index of the next character to be consumed.
				// Decode a generalized variable-length integer into `delta`,
				// which gets added to `i`. The overflow checking is easier
				// if we increase `i` as we go, then subtract off its starting
				// value at the end to obtain `delta`.
				for (oldi = i, w = 1, k = base; /* no condition */; k += base) {

					if (index >= inputLength) {
						error('invalid-input');
					}

					digit = basicToDigit(input.charCodeAt(index++));

					if (digit >= base || digit > floor((maxInt - i) / w)) {
						error('overflow');
					}

					i += digit * w;
					t = k <= bias ? tMin : (k >= bias + tMax ? tMax : k - bias);

					if (digit < t) {
						break;
					}

					baseMinusT = base - t;
					if (w > floor(maxInt / baseMinusT)) {
						error('overflow');
					}

					w *= baseMinusT;

				}

				out = output.length + 1;
				bias = adapt(i - oldi, out, oldi == 0);

				// `i` was supposed to wrap around from `out` to `0`,
				// incrementing `n` each time, so we'll fix that now:
				if (floor(i / out) > maxInt - n) {
					error('overflow');
				}

				n += floor(i / out);
				i %= out;

				// Insert `n` at position `i` of the output
				output.splice(i++, 0, n);

			}

			return ucs2encode(output);
		}

		/**
		 * Converts a string of Unicode symbols (e.g. a domain name label) to a
		 * Punycode string of ASCII-only symbols.
		 * @memberOf punycode
		 * @param {String} input The string of Unicode symbols.
		 * @returns {String} The resulting Punycode string of ASCII-only symbols.
		 */
		function encode(input) {
			var n,
			    delta,
			    handledCPCount,
			    basicLength,
			    bias,
			    j,
			    m,
			    q,
			    k,
			    t,
			    currentValue,
			    output = [],
			    /** `inputLength` will hold the number of code points in `input`. */
			    inputLength,
			    /** Cached calculation results */
			    handledCPCountPlusOne,
			    baseMinusT,
			    qMinusT;

			// Convert the input in UCS-2 to Unicode
			input = ucs2decode(input);

			// Cache the length
			inputLength = input.length;

			// Initialize the state
			n = initialN;
			delta = 0;
			bias = initialBias;

			// Handle the basic code points
			for (j = 0; j < inputLength; ++j) {
				currentValue = input[j];
				if (currentValue < 0x80) {
					output.push(stringFromCharCode(currentValue));
				}
			}

			handledCPCount = basicLength = output.length;

			// `handledCPCount` is the number of code points that have been handled;
			// `basicLength` is the number of basic code points.

			// Finish the basic string - if it is not empty - with a delimiter
			if (basicLength) {
				output.push(delimiter);
			}

			// Main encoding loop:
			while (handledCPCount < inputLength) {

				// All non-basic code points < n have been handled already. Find the next
				// larger one:
				for (m = maxInt, j = 0; j < inputLength; ++j) {
					currentValue = input[j];
					if (currentValue >= n && currentValue < m) {
						m = currentValue;
					}
				}

				// Increase `delta` enough to advance the decoder's <n,i> state to <m,0>,
				// but guard against overflow
				handledCPCountPlusOne = handledCPCount + 1;
				if (m - n > floor((maxInt - delta) / handledCPCountPlusOne)) {
					error('overflow');
				}

				delta += (m - n) * handledCPCountPlusOne;
				n = m;

				for (j = 0; j < inputLength; ++j) {
					currentValue = input[j];

					if (currentValue < n && ++delta > maxInt) {
						error('overflow');
					}

					if (currentValue == n) {
						// Represent delta as a generalized variable-length integer
						for (q = delta, k = base; /* no condition */; k += base) {
							t = k <= bias ? tMin : (k >= bias + tMax ? tMax : k - bias);
							if (q < t) {
								break;
							}
							qMinusT = q - t;
							baseMinusT = base - t;
							output.push(
								stringFromCharCode(digitToBasic(t + qMinusT % baseMinusT, 0))
							);
							q = floor(qMinusT / baseMinusT);
						}

						output.push(stringFromCharCode(digitToBasic(q, 0)));
						bias = adapt(delta, handledCPCountPlusOne, handledCPCount == basicLength);
						delta = 0;
						++handledCPCount;
					}
				}

				++delta;
				++n;

			}
			return output.join('');
		}

		/**
		 * Converts a Punycode string representing a domain name or an email address
		 * to Unicode. Only the Punycoded parts of the input will be converted, i.e.
		 * it doesn't matter if you call it on a string that has already been
		 * converted to Unicode.
		 * @memberOf punycode
		 * @param {String} input The Punycoded domain name or email address to
		 * convert to Unicode.
		 * @returns {String} The Unicode representation of the given Punycode
		 * string.
		 */
		function toUnicode(input) {
			return mapDomain(input, function(string) {
				return regexPunycode.test(string)
					? decode(string.slice(4).toLowerCase())
					: string;
			});
		}

		/**
		 * Converts a Unicode string representing a domain name or an email address to
		 * Punycode. Only the non-ASCII parts of the domain name will be converted,
		 * i.e. it doesn't matter if you call it with a domain that's already in
		 * ASCII.
		 * @memberOf punycode
		 * @param {String} input The domain name or email address to convert, as a
		 * Unicode string.
		 * @returns {String} The Punycode representation of the given domain name or
		 * email address.
		 */
		function toASCII(input) {
			return mapDomain(input, function(string) {
				return regexNonASCII.test(string)
					? 'xn--' + encode(string)
					: string;
			});
		}

		/*--------------------------------------------------------------------------*/

		/** Define the public API */
		punycode = {
			/**
			 * A string representing the current Punycode.js version number.
			 * @memberOf punycode
			 * @type String
			 */
			'version': '1.3.2',
			/**
			 * An object of methods to convert from JavaScript's internal character
			 * representation (UCS-2) to Unicode code points, and back.
			 * @see <https://mathiasbynens.be/notes/javascript-encoding>
			 * @memberOf punycode
			 * @type Object
			 */
			'ucs2': {
				'decode': ucs2decode,
				'encode': ucs2encode
			},
			'decode': decode,
			'encode': encode,
			'toASCII': toASCII,
			'toUnicode': toUnicode
		};

		/** Expose `punycode` */
		// Some AMD build optimizers, like r.js, check for specific condition patterns
		// like the following:
		if (
			true
		) {
			!(__WEBPACK_AMD_DEFINE_RESULT__ = function() {
				return punycode;
			}.call(exports, __webpack_require__, exports, module), __WEBPACK_AMD_DEFINE_RESULT__ !== undefined && (module.exports = __WEBPACK_AMD_DEFINE_RESULT__));
		} else if (freeExports && freeModule) {
			if (module.exports == freeExports) { // in Node.js or RingoJS v0.8.0+
				freeModule.exports = punycode;
			} else { // in Narwhal or RingoJS v0.7.0-
				for (key in punycode) {
					punycode.hasOwnProperty(key) && (freeExports[key] = punycode[key]);
				}
			}
		} else { // in Rhino or a web browser
			root.punycode = punycode;
		}

	}(this));

	/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(363)(module), (function() { return this; }())))

/***/ }),
/* 363 */
/***/ (function(module, exports) {

	module.exports = function(module) {
		if(!module.webpackPolyfill) {
			module.deprecate = function() {};
			module.paths = [];
			// module.parent = undefined by default
			module.children = [];
			module.webpackPolyfill = 1;
		}
		return module;
	}


/***/ }),
/* 364 */
/***/ (function(module, exports) {

	'use strict';

	module.exports = {
	  isString: function(arg) {
	    return typeof(arg) === 'string';
	  },
	  isObject: function(arg) {
	    return typeof(arg) === 'object' && arg !== null;
	  },
	  isNull: function(arg) {
	    return arg === null;
	  },
	  isNullOrUndefined: function(arg) {
	    return arg == null;
	  }
	};


/***/ }),
/* 365 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';

	exports.decode = exports.parse = __webpack_require__(366);
	exports.encode = exports.stringify = __webpack_require__(367);


/***/ }),
/* 366 */
/***/ (function(module, exports) {

	// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	'use strict';

	// If obj.hasOwnProperty has been overridden, then calling
	// obj.hasOwnProperty(prop) will break.
	// See: https://github.com/joyent/node/issues/1707
	function hasOwnProperty(obj, prop) {
	  return Object.prototype.hasOwnProperty.call(obj, prop);
	}

	module.exports = function(qs, sep, eq, options) {
	  sep = sep || '&';
	  eq = eq || '=';
	  var obj = {};

	  if (typeof qs !== 'string' || qs.length === 0) {
	    return obj;
	  }

	  var regexp = /\+/g;
	  qs = qs.split(sep);

	  var maxKeys = 1000;
	  if (options && typeof options.maxKeys === 'number') {
	    maxKeys = options.maxKeys;
	  }

	  var len = qs.length;
	  // maxKeys <= 0 means that we should not limit keys count
	  if (maxKeys > 0 && len > maxKeys) {
	    len = maxKeys;
	  }

	  for (var i = 0; i < len; ++i) {
	    var x = qs[i].replace(regexp, '%20'),
	        idx = x.indexOf(eq),
	        kstr, vstr, k, v;

	    if (idx >= 0) {
	      kstr = x.substr(0, idx);
	      vstr = x.substr(idx + 1);
	    } else {
	      kstr = x;
	      vstr = '';
	    }

	    k = decodeURIComponent(kstr);
	    v = decodeURIComponent(vstr);

	    if (!hasOwnProperty(obj, k)) {
	      obj[k] = v;
	    } else if (Array.isArray(obj[k])) {
	      obj[k].push(v);
	    } else {
	      obj[k] = [obj[k], v];
	    }
	  }

	  return obj;
	};


/***/ }),
/* 367 */
/***/ (function(module, exports) {

	// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	'use strict';

	var stringifyPrimitive = function(v) {
	  switch (typeof v) {
	    case 'string':
	      return v;

	    case 'boolean':
	      return v ? 'true' : 'false';

	    case 'number':
	      return isFinite(v) ? v : '';

	    default:
	      return '';
	  }
	};

	module.exports = function(obj, sep, eq, name) {
	  sep = sep || '&';
	  eq = eq || '=';
	  if (obj === null) {
	    obj = undefined;
	  }

	  if (typeof obj === 'object') {
	    return Object.keys(obj).map(function(k) {
	      var ks = encodeURIComponent(stringifyPrimitive(k)) + eq;
	      if (Array.isArray(obj[k])) {
	        return obj[k].map(function(v) {
	          return ks + encodeURIComponent(stringifyPrimitive(v));
	        }).join(sep);
	      } else {
	        return ks + encodeURIComponent(stringifyPrimitive(obj[k]));
	      }
	    }).join(sep);

	  }

	  if (!name) return '';
	  return encodeURIComponent(stringifyPrimitive(name)) + eq +
	         encodeURIComponent(stringifyPrimitive(obj));
	};


/***/ }),
/* 368 */
/***/ (function(module, exports, __webpack_require__) {

	var http = __webpack_require__(331);

	var https = module.exports;

	for (var key in http) {
	    if (http.hasOwnProperty(key)) https[key] = http[key];
	};

	https.request = function (params, cb) {
	    if (!params) params = {};
	    params.scheme = 'https';
	    params.protocol = 'https:';
	    return http.request.call(this, params, cb);
	}


/***/ }),
/* 369 */
/***/ (function(module, exports) {

	'use strict';

	module.exports.HOST = 'localhost';
	module.exports.PORT = 9222;

/***/ }),
/* 370 */
/***/ (function(module, exports) {

	module.exports = (function criWrapper(_, options, callback) {
	    window.criRequest(options, callback); // eslint-disable-line no-undef
	});

/***/ }),
/* 371 */
/***/ (function(module, exports) {

	module.exports = {"version":{"major":"1","minor":"3"},"domains":[{"domain":"Accessibility","experimental":true,"dependencies":["DOM"],"types":[{"id":"AXNodeId","description":"Unique accessibility node identifier.","type":"string"},{"id":"AXValueType","description":"Enum of possible property types.","type":"string","enum":["boolean","tristate","booleanOrUndefined","idref","idrefList","integer","node","nodeList","number","string","computedString","token","tokenList","domRelation","role","internalRole","valueUndefined"]},{"id":"AXValueSourceType","description":"Enum of possible property sources.","type":"string","enum":["attribute","implicit","style","contents","placeholder","relatedElement"]},{"id":"AXValueNativeSourceType","description":"Enum of possible native property sources (as a subtype of a particular AXValueSourceType).","type":"string","enum":["figcaption","label","labelfor","labelwrapped","legend","tablecaption","title","other"]},{"id":"AXValueSource","description":"A single source for a computed AX property.","type":"object","properties":[{"name":"type","description":"What type of source this is.","$ref":"AXValueSourceType"},{"name":"value","description":"The value of this property source.","optional":true,"$ref":"AXValue"},{"name":"attribute","description":"The name of the relevant attribute, if any.","optional":true,"type":"string"},{"name":"attributeValue","description":"The value of the relevant attribute, if any.","optional":true,"$ref":"AXValue"},{"name":"superseded","description":"Whether this source is superseded by a higher priority source.","optional":true,"type":"boolean"},{"name":"nativeSource","description":"The native markup source for this value, e.g. a <label> element.","optional":true,"$ref":"AXValueNativeSourceType"},{"name":"nativeSourceValue","description":"The value, such as a node or node list, of the native source.","optional":true,"$ref":"AXValue"},{"name":"invalid","description":"Whether the value for this property is invalid.","optional":true,"type":"boolean"},{"name":"invalidReason","description":"Reason for the value being invalid, if it is.","optional":true,"type":"string"}]},{"id":"AXRelatedNode","type":"object","properties":[{"name":"backendDOMNodeId","description":"The BackendNodeId of the related DOM node.","$ref":"DOM.BackendNodeId"},{"name":"idref","description":"The IDRef value provided, if any.","optional":true,"type":"string"},{"name":"text","description":"The text alternative of this node in the current context.","optional":true,"type":"string"}]},{"id":"AXProperty","type":"object","properties":[{"name":"name","description":"The name of this property.","$ref":"AXPropertyName"},{"name":"value","description":"The value of this property.","$ref":"AXValue"}]},{"id":"AXValue","description":"A single computed AX property.","type":"object","properties":[{"name":"type","description":"The type of this value.","$ref":"AXValueType"},{"name":"value","description":"The computed value of this property.","optional":true,"type":"any"},{"name":"relatedNodes","description":"One or more related nodes, if applicable.","optional":true,"type":"array","items":{"$ref":"AXRelatedNode"}},{"name":"sources","description":"The sources which contributed to the computation of this property.","optional":true,"type":"array","items":{"$ref":"AXValueSource"}}]},{"id":"AXPropertyName","description":"Values of AXProperty name: from 'busy' to 'roledescription' - states which apply to every AX\nnode, from 'live' to 'root' - attributes which apply to nodes in live regions, from\n'autocomplete' to 'valuetext' - attributes which apply to widgets, from 'checked' to 'selected'\n- states which apply to widgets, from 'activedescendant' to 'owns' - relationships between\nelements other than parent/child/sibling.","type":"string","enum":["busy","disabled","editable","focusable","focused","hidden","hiddenRoot","invalid","keyshortcuts","settable","roledescription","live","atomic","relevant","root","autocomplete","hasPopup","level","multiselectable","orientation","multiline","readonly","required","valuemin","valuemax","valuetext","checked","expanded","modal","pressed","selected","activedescendant","controls","describedby","details","errormessage","flowto","labelledby","owns"]},{"id":"AXNode","description":"A node in the accessibility tree.","type":"object","properties":[{"name":"nodeId","description":"Unique identifier for this node.","$ref":"AXNodeId"},{"name":"ignored","description":"Whether this node is ignored for accessibility","type":"boolean"},{"name":"ignoredReasons","description":"Collection of reasons why this node is hidden.","optional":true,"type":"array","items":{"$ref":"AXProperty"}},{"name":"role","description":"This `Node`'s role, whether explicit or implicit.","optional":true,"$ref":"AXValue"},{"name":"name","description":"The accessible name for this `Node`.","optional":true,"$ref":"AXValue"},{"name":"description","description":"The accessible description for this `Node`.","optional":true,"$ref":"AXValue"},{"name":"value","description":"The value for this `Node`.","optional":true,"$ref":"AXValue"},{"name":"properties","description":"All other properties","optional":true,"type":"array","items":{"$ref":"AXProperty"}},{"name":"childIds","description":"IDs for each of this node's child nodes.","optional":true,"type":"array","items":{"$ref":"AXNodeId"}},{"name":"backendDOMNodeId","description":"The backend ID for the associated DOM node, if any.","optional":true,"$ref":"DOM.BackendNodeId"}]}],"commands":[{"name":"disable","description":"Disables the accessibility domain."},{"name":"enable","description":"Enables the accessibility domain which causes `AXNodeId`s to remain consistent between method calls.\nThis turns on accessibility for the page, which can impact performance until accessibility is disabled."},{"name":"getPartialAXTree","description":"Fetches the accessibility node and partial accessibility tree for this DOM node, if it exists.","experimental":true,"parameters":[{"name":"nodeId","description":"Identifier of the node to get the partial accessibility tree for.","optional":true,"$ref":"DOM.NodeId"},{"name":"backendNodeId","description":"Identifier of the backend node to get the partial accessibility tree for.","optional":true,"$ref":"DOM.BackendNodeId"},{"name":"objectId","description":"JavaScript object id of the node wrapper to get the partial accessibility tree for.","optional":true,"$ref":"Runtime.RemoteObjectId"},{"name":"fetchRelatives","description":"Whether to fetch this nodes ancestors, siblings and children. Defaults to true.","optional":true,"type":"boolean"}],"returns":[{"name":"nodes","description":"The `Accessibility.AXNode` for this DOM node, if it exists, plus its ancestors, siblings and\nchildren, if requested.","type":"array","items":{"$ref":"AXNode"}}]},{"name":"getFullAXTree","description":"Fetches the entire accessibility tree","experimental":true,"returns":[{"name":"nodes","type":"array","items":{"$ref":"AXNode"}}]}]},{"domain":"Animation","experimental":true,"dependencies":["Runtime","DOM"],"types":[{"id":"Animation","description":"Animation instance.","type":"object","properties":[{"name":"id","description":"`Animation`'s id.","type":"string"},{"name":"name","description":"`Animation`'s name.","type":"string"},{"name":"pausedState","description":"`Animation`'s internal paused state.","type":"boolean"},{"name":"playState","description":"`Animation`'s play state.","type":"string"},{"name":"playbackRate","description":"`Animation`'s playback rate.","type":"number"},{"name":"startTime","description":"`Animation`'s start time.","type":"number"},{"name":"currentTime","description":"`Animation`'s current time.","type":"number"},{"name":"type","description":"Animation type of `Animation`.","type":"string","enum":["CSSTransition","CSSAnimation","WebAnimation"]},{"name":"source","description":"`Animation`'s source animation node.","optional":true,"$ref":"AnimationEffect"},{"name":"cssId","description":"A unique ID for `Animation` representing the sources that triggered this CSS\nanimation/transition.","optional":true,"type":"string"}]},{"id":"AnimationEffect","description":"AnimationEffect instance","type":"object","properties":[{"name":"delay","description":"`AnimationEffect`'s delay.","type":"number"},{"name":"endDelay","description":"`AnimationEffect`'s end delay.","type":"number"},{"name":"iterationStart","description":"`AnimationEffect`'s iteration start.","type":"number"},{"name":"iterations","description":"`AnimationEffect`'s iterations.","type":"number"},{"name":"duration","description":"`AnimationEffect`'s iteration duration.","type":"number"},{"name":"direction","description":"`AnimationEffect`'s playback direction.","type":"string"},{"name":"fill","description":"`AnimationEffect`'s fill mode.","type":"string"},{"name":"backendNodeId","description":"`AnimationEffect`'s target node.","optional":true,"$ref":"DOM.BackendNodeId"},{"name":"keyframesRule","description":"`AnimationEffect`'s keyframes.","optional":true,"$ref":"KeyframesRule"},{"name":"easing","description":"`AnimationEffect`'s timing function.","type":"string"}]},{"id":"KeyframesRule","description":"Keyframes Rule","type":"object","properties":[{"name":"name","description":"CSS keyframed animation's name.","optional":true,"type":"string"},{"name":"keyframes","description":"List of animation keyframes.","type":"array","items":{"$ref":"KeyframeStyle"}}]},{"id":"KeyframeStyle","description":"Keyframe Style","type":"object","properties":[{"name":"offset","description":"Keyframe's time offset.","type":"string"},{"name":"easing","description":"`AnimationEffect`'s timing function.","type":"string"}]}],"commands":[{"name":"disable","description":"Disables animation domain notifications."},{"name":"enable","description":"Enables animation domain notifications."},{"name":"getCurrentTime","description":"Returns the current time of the an animation.","parameters":[{"name":"id","description":"Id of animation.","type":"string"}],"returns":[{"name":"currentTime","description":"Current time of the page.","type":"number"}]},{"name":"getPlaybackRate","description":"Gets the playback rate of the document timeline.","returns":[{"name":"playbackRate","description":"Playback rate for animations on page.","type":"number"}]},{"name":"releaseAnimations","description":"Releases a set of animations to no longer be manipulated.","parameters":[{"name":"animations","description":"List of animation ids to seek.","type":"array","items":{"type":"string"}}]},{"name":"resolveAnimation","description":"Gets the remote object of the Animation.","parameters":[{"name":"animationId","description":"Animation id.","type":"string"}],"returns":[{"name":"remoteObject","description":"Corresponding remote object.","$ref":"Runtime.RemoteObject"}]},{"name":"seekAnimations","description":"Seek a set of animations to a particular time within each animation.","parameters":[{"name":"animations","description":"List of animation ids to seek.","type":"array","items":{"type":"string"}},{"name":"currentTime","description":"Set the current time of each animation.","type":"number"}]},{"name":"setPaused","description":"Sets the paused state of a set of animations.","parameters":[{"name":"animations","description":"Animations to set the pause state of.","type":"array","items":{"type":"string"}},{"name":"paused","description":"Paused state to set to.","type":"boolean"}]},{"name":"setPlaybackRate","description":"Sets the playback rate of the document timeline.","parameters":[{"name":"playbackRate","description":"Playback rate for animations on page","type":"number"}]},{"name":"setTiming","description":"Sets the timing of an animation node.","parameters":[{"name":"animationId","description":"Animation id.","type":"string"},{"name":"duration","description":"Duration of the animation.","type":"number"},{"name":"delay","description":"Delay of the animation.","type":"number"}]}],"events":[{"name":"animationCanceled","description":"Event for when an animation has been cancelled.","parameters":[{"name":"id","description":"Id of the animation that was cancelled.","type":"string"}]},{"name":"animationCreated","description":"Event for each animation that has been created.","parameters":[{"name":"id","description":"Id of the animation that was created.","type":"string"}]},{"name":"animationStarted","description":"Event for animation that has been started.","parameters":[{"name":"animation","description":"Animation that was started.","$ref":"Animation"}]}]},{"domain":"ApplicationCache","experimental":true,"types":[{"id":"ApplicationCacheResource","description":"Detailed application cache resource information.","type":"object","properties":[{"name":"url","description":"Resource url.","type":"string"},{"name":"size","description":"Resource size.","type":"integer"},{"name":"type","description":"Resource type.","type":"string"}]},{"id":"ApplicationCache","description":"Detailed application cache information.","type":"object","properties":[{"name":"manifestURL","description":"Manifest URL.","type":"string"},{"name":"size","description":"Application cache size.","type":"number"},{"name":"creationTime","description":"Application cache creation time.","type":"number"},{"name":"updateTime","description":"Application cache update time.","type":"number"},{"name":"resources","description":"Application cache resources.","type":"array","items":{"$ref":"ApplicationCacheResource"}}]},{"id":"FrameWithManifest","description":"Frame identifier - manifest URL pair.","type":"object","properties":[{"name":"frameId","description":"Frame identifier.","$ref":"Page.FrameId"},{"name":"manifestURL","description":"Manifest URL.","type":"string"},{"name":"status","description":"Application cache status.","type":"integer"}]}],"commands":[{"name":"enable","description":"Enables application cache domain notifications."},{"name":"getApplicationCacheForFrame","description":"Returns relevant application cache data for the document in given frame.","parameters":[{"name":"frameId","description":"Identifier of the frame containing document whose application cache is retrieved.","$ref":"Page.FrameId"}],"returns":[{"name":"applicationCache","description":"Relevant application cache data for the document in given frame.","$ref":"ApplicationCache"}]},{"name":"getFramesWithManifests","description":"Returns array of frame identifiers with manifest urls for each frame containing a document\nassociated with some application cache.","returns":[{"name":"frameIds","description":"Array of frame identifiers with manifest urls for each frame containing a document\nassociated with some application cache.","type":"array","items":{"$ref":"FrameWithManifest"}}]},{"name":"getManifestForFrame","description":"Returns manifest URL for document in the given frame.","parameters":[{"name":"frameId","description":"Identifier of the frame containing document whose manifest is retrieved.","$ref":"Page.FrameId"}],"returns":[{"name":"manifestURL","description":"Manifest URL for document in the given frame.","type":"string"}]}],"events":[{"name":"applicationCacheStatusUpdated","parameters":[{"name":"frameId","description":"Identifier of the frame containing document whose application cache updated status.","$ref":"Page.FrameId"},{"name":"manifestURL","description":"Manifest URL.","type":"string"},{"name":"status","description":"Updated application cache status.","type":"integer"}]},{"name":"networkStateUpdated","parameters":[{"name":"isNowOnline","type":"boolean"}]}]},{"domain":"Audits","description":"Audits domain allows investigation of page violations and possible improvements.","experimental":true,"dependencies":["Network"],"commands":[{"name":"getEncodedResponse","description":"Returns the response body and size if it were re-encoded with the specified settings. Only\napplies to images.","parameters":[{"name":"requestId","description":"Identifier of the network request to get content for.","$ref":"Network.RequestId"},{"name":"encoding","description":"The encoding to use.","type":"string","enum":["webp","jpeg","png"]},{"name":"quality","description":"The quality of the encoding (0-1). (defaults to 1)","optional":true,"type":"number"},{"name":"sizeOnly","description":"Whether to only return the size information (defaults to false).","optional":true,"type":"boolean"}],"returns":[{"name":"body","description":"The encoded body as a base64 string. Omitted if sizeOnly is true.","optional":true,"type":"string"},{"name":"originalSize","description":"Size before re-encoding.","type":"integer"},{"name":"encodedSize","description":"Size after re-encoding.","type":"integer"}]}]},{"domain":"Browser","description":"The Browser domain defines methods and events for browser managing.","types":[{"id":"WindowID","experimental":true,"type":"integer"},{"id":"WindowState","description":"The state of the browser window.","experimental":true,"type":"string","enum":["normal","minimized","maximized","fullscreen"]},{"id":"Bounds","description":"Browser window bounds information","experimental":true,"type":"object","properties":[{"name":"left","description":"The offset from the left edge of the screen to the window in pixels.","optional":true,"type":"integer"},{"name":"top","description":"The offset from the top edge of the screen to the window in pixels.","optional":true,"type":"integer"},{"name":"width","description":"The window width in pixels.","optional":true,"type":"integer"},{"name":"height","description":"The window height in pixels.","optional":true,"type":"integer"},{"name":"windowState","description":"The window state. Default to normal.","optional":true,"$ref":"WindowState"}]},{"id":"PermissionType","experimental":true,"type":"string","enum":["accessibilityEvents","audioCapture","backgroundSync","backgroundFetch","clipboardRead","clipboardWrite","durableStorage","flash","geolocation","midi","midiSysex","notifications","paymentHandler","protectedMediaIdentifier","sensors","videoCapture"]},{"id":"Bucket","description":"Chrome histogram bucket.","experimental":true,"type":"object","properties":[{"name":"low","description":"Minimum value (inclusive).","type":"integer"},{"name":"high","description":"Maximum value (exclusive).","type":"integer"},{"name":"count","description":"Number of samples.","type":"integer"}]},{"id":"Histogram","description":"Chrome histogram.","experimental":true,"type":"object","properties":[{"name":"name","description":"Name.","type":"string"},{"name":"sum","description":"Sum of sample values.","type":"integer"},{"name":"count","description":"Total number of samples.","type":"integer"},{"name":"buckets","description":"Buckets.","type":"array","items":{"$ref":"Bucket"}}]}],"commands":[{"name":"grantPermissions","description":"Grant specific permissions to the given origin and reject all others.","experimental":true,"parameters":[{"name":"origin","type":"string"},{"name":"permissions","type":"array","items":{"$ref":"PermissionType"}},{"name":"browserContextId","description":"BrowserContext to override permissions. When omitted, default browser context is used.","optional":true,"$ref":"Target.BrowserContextID"}]},{"name":"resetPermissions","description":"Reset all permission management for all origins.","experimental":true,"parameters":[{"name":"browserContextId","description":"BrowserContext to reset permissions. When omitted, default browser context is used.","optional":true,"$ref":"Target.BrowserContextID"}]},{"name":"close","description":"Close browser gracefully."},{"name":"crash","description":"Crashes browser on the main thread.","experimental":true},{"name":"getVersion","description":"Returns version information.","returns":[{"name":"protocolVersion","description":"Protocol version.","type":"string"},{"name":"product","description":"Product name.","type":"string"},{"name":"revision","description":"Product revision.","type":"string"},{"name":"userAgent","description":"User-Agent.","type":"string"},{"name":"jsVersion","description":"V8 version.","type":"string"}]},{"name":"getBrowserCommandLine","description":"Returns the command line switches for the browser process if, and only if\n--enable-automation is on the commandline.","experimental":true,"returns":[{"name":"arguments","description":"Commandline parameters","type":"array","items":{"type":"string"}}]},{"name":"getHistograms","description":"Get Chrome histograms.","experimental":true,"parameters":[{"name":"query","description":"Requested substring in name. Only histograms which have query as a\nsubstring in their name are extracted. An empty or absent query returns\nall histograms.","optional":true,"type":"string"},{"name":"delta","description":"If true, retrieve delta since last call.","optional":true,"type":"boolean"}],"returns":[{"name":"histograms","description":"Histograms.","type":"array","items":{"$ref":"Histogram"}}]},{"name":"getHistogram","description":"Get a Chrome histogram by name.","experimental":true,"parameters":[{"name":"name","description":"Requested histogram name.","type":"string"},{"name":"delta","description":"If true, retrieve delta since last call.","optional":true,"type":"boolean"}],"returns":[{"name":"histogram","description":"Histogram.","$ref":"Histogram"}]},{"name":"getWindowBounds","description":"Get position and size of the browser window.","experimental":true,"parameters":[{"name":"windowId","description":"Browser window id.","$ref":"WindowID"}],"returns":[{"name":"bounds","description":"Bounds information of the window. When window state is 'minimized', the restored window\nposition and size are returned.","$ref":"Bounds"}]},{"name":"getWindowForTarget","description":"Get the browser window that contains the devtools target.","experimental":true,"parameters":[{"name":"targetId","description":"Devtools agent host id. If called as a part of the session, associated targetId is used.","optional":true,"$ref":"Target.TargetID"}],"returns":[{"name":"windowId","description":"Browser window id.","$ref":"WindowID"},{"name":"bounds","description":"Bounds information of the window. When window state is 'minimized', the restored window\nposition and size are returned.","$ref":"Bounds"}]},{"name":"setWindowBounds","description":"Set position and/or size of the browser window.","experimental":true,"parameters":[{"name":"windowId","description":"Browser window id.","$ref":"WindowID"},{"name":"bounds","description":"New window bounds. The 'minimized', 'maximized' and 'fullscreen' states cannot be combined\nwith 'left', 'top', 'width' or 'height'. Leaves unspecified fields unchanged.","$ref":"Bounds"}]},{"name":"setDockTile","description":"Set dock tile details, platform-specific.","experimental":true,"parameters":[{"name":"badgeLabel","optional":true,"type":"string"},{"name":"image","description":"Png encoded image.","optional":true,"type":"string"}]}]},{"domain":"CSS","description":"This domain exposes CSS read/write operations. All CSS objects (stylesheets, rules, and styles)\nhave an associated `id` used in subsequent operations on the related object. Each object type has\na specific `id` structure, and those are not interchangeable between objects of different kinds.\nCSS objects can be loaded using the `get*ForNode()` calls (which accept a DOM node id). A client\ncan also keep track of stylesheets via the `styleSheetAdded`/`styleSheetRemoved` events and\nsubsequently load the required stylesheet contents using the `getStyleSheet[Text]()` methods.","experimental":true,"dependencies":["DOM"],"types":[{"id":"StyleSheetId","type":"string"},{"id":"StyleSheetOrigin","description":"Stylesheet type: \"injected\" for stylesheets injected via extension, \"user-agent\" for user-agent\nstylesheets, \"inspector\" for stylesheets created by the inspector (i.e. those holding the \"via\ninspector\" rules), \"regular\" for regular stylesheets.","type":"string","enum":["injected","user-agent","inspector","regular"]},{"id":"PseudoElementMatches","description":"CSS rule collection for a single pseudo style.","type":"object","properties":[{"name":"pseudoType","description":"Pseudo element type.","$ref":"DOM.PseudoType"},{"name":"matches","description":"Matches of CSS rules applicable to the pseudo style.","type":"array","items":{"$ref":"RuleMatch"}}]},{"id":"InheritedStyleEntry","description":"Inherited CSS rule collection from ancestor node.","type":"object","properties":[{"name":"inlineStyle","description":"The ancestor node's inline style, if any, in the style inheritance chain.","optional":true,"$ref":"CSSStyle"},{"name":"matchedCSSRules","description":"Matches of CSS rules matching the ancestor node in the style inheritance chain.","type":"array","items":{"$ref":"RuleMatch"}}]},{"id":"RuleMatch","description":"Match data for a CSS rule.","type":"object","properties":[{"name":"rule","description":"CSS rule in the match.","$ref":"CSSRule"},{"name":"matchingSelectors","description":"Matching selector indices in the rule's selectorList selectors (0-based).","type":"array","items":{"type":"integer"}}]},{"id":"Value","description":"Data for a simple selector (these are delimited by commas in a selector list).","type":"object","properties":[{"name":"text","description":"Value text.","type":"string"},{"name":"range","description":"Value range in the underlying resource (if available).","optional":true,"$ref":"SourceRange"}]},{"id":"SelectorList","description":"Selector list data.","type":"object","properties":[{"name":"selectors","description":"Selectors in the list.","type":"array","items":{"$ref":"Value"}},{"name":"text","description":"Rule selector text.","type":"string"}]},{"id":"CSSStyleSheetHeader","description":"CSS stylesheet metainformation.","type":"object","properties":[{"name":"styleSheetId","description":"The stylesheet identifier.","$ref":"StyleSheetId"},{"name":"frameId","description":"Owner frame identifier.","$ref":"Page.FrameId"},{"name":"sourceURL","description":"Stylesheet resource URL.","type":"string"},{"name":"sourceMapURL","description":"URL of source map associated with the stylesheet (if any).","optional":true,"type":"string"},{"name":"origin","description":"Stylesheet origin.","$ref":"StyleSheetOrigin"},{"name":"title","description":"Stylesheet title.","type":"string"},{"name":"ownerNode","description":"The backend id for the owner node of the stylesheet.","optional":true,"$ref":"DOM.BackendNodeId"},{"name":"disabled","description":"Denotes whether the stylesheet is disabled.","type":"boolean"},{"name":"hasSourceURL","description":"Whether the sourceURL field value comes from the sourceURL comment.","optional":true,"type":"boolean"},{"name":"isInline","description":"Whether this stylesheet is created for STYLE tag by parser. This flag is not set for\ndocument.written STYLE tags.","type":"boolean"},{"name":"startLine","description":"Line offset of the stylesheet within the resource (zero based).","type":"number"},{"name":"startColumn","description":"Column offset of the stylesheet within the resource (zero based).","type":"number"},{"name":"length","description":"Size of the content (in characters).","type":"number"}]},{"id":"CSSRule","description":"CSS rule representation.","type":"object","properties":[{"name":"styleSheetId","description":"The css style sheet identifier (absent for user agent stylesheet and user-specified\nstylesheet rules) this rule came from.","optional":true,"$ref":"StyleSheetId"},{"name":"selectorList","description":"Rule selector data.","$ref":"SelectorList"},{"name":"origin","description":"Parent stylesheet's origin.","$ref":"StyleSheetOrigin"},{"name":"style","description":"Associated style declaration.","$ref":"CSSStyle"},{"name":"media","description":"Media list array (for rules involving media queries). The array enumerates media queries\nstarting with the innermost one, going outwards.","optional":true,"type":"array","items":{"$ref":"CSSMedia"}}]},{"id":"RuleUsage","description":"CSS coverage information.","type":"object","properties":[{"name":"styleSheetId","description":"The css style sheet identifier (absent for user agent stylesheet and user-specified\nstylesheet rules) this rule came from.","$ref":"StyleSheetId"},{"name":"startOffset","description":"Offset of the start of the rule (including selector) from the beginning of the stylesheet.","type":"number"},{"name":"endOffset","description":"Offset of the end of the rule body from the beginning of the stylesheet.","type":"number"},{"name":"used","description":"Indicates whether the rule was actually used by some element in the page.","type":"boolean"}]},{"id":"SourceRange","description":"Text range within a resource. All numbers are zero-based.","type":"object","properties":[{"name":"startLine","description":"Start line of range.","type":"integer"},{"name":"startColumn","description":"Start column of range (inclusive).","type":"integer"},{"name":"endLine","description":"End line of range","type":"integer"},{"name":"endColumn","description":"End column of range (exclusive).","type":"integer"}]},{"id":"ShorthandEntry","type":"object","properties":[{"name":"name","description":"Shorthand name.","type":"string"},{"name":"value","description":"Shorthand value.","type":"string"},{"name":"important","description":"Whether the property has \"!important\" annotation (implies `false` if absent).","optional":true,"type":"boolean"}]},{"id":"CSSComputedStyleProperty","type":"object","properties":[{"name":"name","description":"Computed style property name.","type":"string"},{"name":"value","description":"Computed style property value.","type":"string"}]},{"id":"CSSStyle","description":"CSS style representation.","type":"object","properties":[{"name":"styleSheetId","description":"The css style sheet identifier (absent for user agent stylesheet and user-specified\nstylesheet rules) this rule came from.","optional":true,"$ref":"StyleSheetId"},{"name":"cssProperties","description":"CSS properties in the style.","type":"array","items":{"$ref":"CSSProperty"}},{"name":"shorthandEntries","description":"Computed values for all shorthands found in the style.","type":"array","items":{"$ref":"ShorthandEntry"}},{"name":"cssText","description":"Style declaration text (if available).","optional":true,"type":"string"},{"name":"range","description":"Style declaration range in the enclosing stylesheet (if available).","optional":true,"$ref":"SourceRange"}]},{"id":"CSSProperty","description":"CSS property declaration data.","type":"object","properties":[{"name":"name","description":"The property name.","type":"string"},{"name":"value","description":"The property value.","type":"string"},{"name":"important","description":"Whether the property has \"!important\" annotation (implies `false` if absent).","optional":true,"type":"boolean"},{"name":"implicit","description":"Whether the property is implicit (implies `false` if absent).","optional":true,"type":"boolean"},{"name":"text","description":"The full property text as specified in the style.","optional":true,"type":"string"},{"name":"parsedOk","description":"Whether the property is understood by the browser (implies `true` if absent).","optional":true,"type":"boolean"},{"name":"disabled","description":"Whether the property is disabled by the user (present for source-based properties only).","optional":true,"type":"boolean"},{"name":"range","description":"The entire property range in the enclosing style declaration (if available).","optional":true,"$ref":"SourceRange"}]},{"id":"CSSMedia","description":"CSS media rule descriptor.","type":"object","properties":[{"name":"text","description":"Media query text.","type":"string"},{"name":"source","description":"Source of the media query: \"mediaRule\" if specified by a @media rule, \"importRule\" if\nspecified by an @import rule, \"linkedSheet\" if specified by a \"media\" attribute in a linked\nstylesheet's LINK tag, \"inlineSheet\" if specified by a \"media\" attribute in an inline\nstylesheet's STYLE tag.","type":"string","enum":["mediaRule","importRule","linkedSheet","inlineSheet"]},{"name":"sourceURL","description":"URL of the document containing the media query description.","optional":true,"type":"string"},{"name":"range","description":"The associated rule (@media or @import) header range in the enclosing stylesheet (if\navailable).","optional":true,"$ref":"SourceRange"},{"name":"styleSheetId","description":"Identifier of the stylesheet containing this object (if exists).","optional":true,"$ref":"StyleSheetId"},{"name":"mediaList","description":"Array of media queries.","optional":true,"type":"array","items":{"$ref":"MediaQuery"}}]},{"id":"MediaQuery","description":"Media query descriptor.","type":"object","properties":[{"name":"expressions","description":"Array of media query expressions.","type":"array","items":{"$ref":"MediaQueryExpression"}},{"name":"active","description":"Whether the media query condition is satisfied.","type":"boolean"}]},{"id":"MediaQueryExpression","description":"Media query expression descriptor.","type":"object","properties":[{"name":"value","description":"Media query expression value.","type":"number"},{"name":"unit","description":"Media query expression units.","type":"string"},{"name":"feature","description":"Media query expression feature.","type":"string"},{"name":"valueRange","description":"The associated range of the value text in the enclosing stylesheet (if available).","optional":true,"$ref":"SourceRange"},{"name":"computedLength","description":"Computed length of media query expression (if applicable).","optional":true,"type":"number"}]},{"id":"PlatformFontUsage","description":"Information about amount of glyphs that were rendered with given font.","type":"object","properties":[{"name":"familyName","description":"Font's family name reported by platform.","type":"string"},{"name":"isCustomFont","description":"Indicates if the font was downloaded or resolved locally.","type":"boolean"},{"name":"glyphCount","description":"Amount of glyphs that were rendered with this font.","type":"number"}]},{"id":"FontFace","description":"Properties of a web font: https://www.w3.org/TR/2008/REC-CSS2-20080411/fonts.html#font-descriptions","type":"object","properties":[{"name":"fontFamily","description":"The font-family.","type":"string"},{"name":"fontStyle","description":"The font-style.","type":"string"},{"name":"fontVariant","description":"The font-variant.","type":"string"},{"name":"fontWeight","description":"The font-weight.","type":"string"},{"name":"fontStretch","description":"The font-stretch.","type":"string"},{"name":"unicodeRange","description":"The unicode-range.","type":"string"},{"name":"src","description":"The src.","type":"string"},{"name":"platformFontFamily","description":"The resolved platform font family","type":"string"}]},{"id":"CSSKeyframesRule","description":"CSS keyframes rule representation.","type":"object","properties":[{"name":"animationName","description":"Animation name.","$ref":"Value"},{"name":"keyframes","description":"List of keyframes.","type":"array","items":{"$ref":"CSSKeyframeRule"}}]},{"id":"CSSKeyframeRule","description":"CSS keyframe rule representation.","type":"object","properties":[{"name":"styleSheetId","description":"The css style sheet identifier (absent for user agent stylesheet and user-specified\nstylesheet rules) this rule came from.","optional":true,"$ref":"StyleSheetId"},{"name":"origin","description":"Parent stylesheet's origin.","$ref":"StyleSheetOrigin"},{"name":"keyText","description":"Associated key text.","$ref":"Value"},{"name":"style","description":"Associated style declaration.","$ref":"CSSStyle"}]},{"id":"StyleDeclarationEdit","description":"A descriptor of operation to mutate style declaration text.","type":"object","properties":[{"name":"styleSheetId","description":"The css style sheet identifier.","$ref":"StyleSheetId"},{"name":"range","description":"The range of the style text in the enclosing stylesheet.","$ref":"SourceRange"},{"name":"text","description":"New style text.","type":"string"}]}],"commands":[{"name":"addRule","description":"Inserts a new rule with the given `ruleText` in a stylesheet with given `styleSheetId`, at the\nposition specified by `location`.","parameters":[{"name":"styleSheetId","description":"The css style sheet identifier where a new rule should be inserted.","$ref":"StyleSheetId"},{"name":"ruleText","description":"The text of a new rule.","type":"string"},{"name":"location","description":"Text position of a new rule in the target style sheet.","$ref":"SourceRange"}],"returns":[{"name":"rule","description":"The newly created rule.","$ref":"CSSRule"}]},{"name":"collectClassNames","description":"Returns all class names from specified stylesheet.","parameters":[{"name":"styleSheetId","$ref":"StyleSheetId"}],"returns":[{"name":"classNames","description":"Class name list.","type":"array","items":{"type":"string"}}]},{"name":"createStyleSheet","description":"Creates a new special \"via-inspector\" stylesheet in the frame with given `frameId`.","parameters":[{"name":"frameId","description":"Identifier of the frame where \"via-inspector\" stylesheet should be created.","$ref":"Page.FrameId"}],"returns":[{"name":"styleSheetId","description":"Identifier of the created \"via-inspector\" stylesheet.","$ref":"StyleSheetId"}]},{"name":"disable","description":"Disables the CSS agent for the given page."},{"name":"enable","description":"Enables the CSS agent for the given page. Clients should not assume that the CSS agent has been\nenabled until the result of this command is received."},{"name":"forcePseudoState","description":"Ensures that the given node will have specified pseudo-classes whenever its style is computed by\nthe browser.","parameters":[{"name":"nodeId","description":"The element id for which to force the pseudo state.","$ref":"DOM.NodeId"},{"name":"forcedPseudoClasses","description":"Element pseudo classes to force when computing the element's style.","type":"array","items":{"type":"string"}}]},{"name":"getBackgroundColors","parameters":[{"name":"nodeId","description":"Id of the node to get background colors for.","$ref":"DOM.NodeId"}],"returns":[{"name":"backgroundColors","description":"The range of background colors behind this element, if it contains any visible text. If no\nvisible text is present, this will be undefined. In the case of a flat background color,\nthis will consist of simply that color. In the case of a gradient, this will consist of each\nof the color stops. For anything more complicated, this will be an empty array. Images will\nbe ignored (as if the image had failed to load).","optional":true,"type":"array","items":{"type":"string"}},{"name":"computedFontSize","description":"The computed font size for this node, as a CSS computed value string (e.g. '12px').","optional":true,"type":"string"},{"name":"computedFontWeight","description":"The computed font weight for this node, as a CSS computed value string (e.g. 'normal' or\n'100').","optional":true,"type":"string"},{"name":"computedBodyFontSize","description":"The computed font size for the document body, as a computed CSS value string (e.g. '16px').","optional":true,"type":"string"}]},{"name":"getComputedStyleForNode","description":"Returns the computed style for a DOM node identified by `nodeId`.","parameters":[{"name":"nodeId","$ref":"DOM.NodeId"}],"returns":[{"name":"computedStyle","description":"Computed style for the specified DOM node.","type":"array","items":{"$ref":"CSSComputedStyleProperty"}}]},{"name":"getInlineStylesForNode","description":"Returns the styles defined inline (explicitly in the \"style\" attribute and implicitly, using DOM\nattributes) for a DOM node identified by `nodeId`.","parameters":[{"name":"nodeId","$ref":"DOM.NodeId"}],"returns":[{"name":"inlineStyle","description":"Inline style for the specified DOM node.","optional":true,"$ref":"CSSStyle"},{"name":"attributesStyle","description":"Attribute-defined element style (e.g. resulting from \"width=20 height=100%\").","optional":true,"$ref":"CSSStyle"}]},{"name":"getMatchedStylesForNode","description":"Returns requested styles for a DOM node identified by `nodeId`.","parameters":[{"name":"nodeId","$ref":"DOM.NodeId"}],"returns":[{"name":"inlineStyle","description":"Inline style for the specified DOM node.","optional":true,"$ref":"CSSStyle"},{"name":"attributesStyle","description":"Attribute-defined element style (e.g. resulting from \"width=20 height=100%\").","optional":true,"$ref":"CSSStyle"},{"name":"matchedCSSRules","description":"CSS rules matching this node, from all applicable stylesheets.","optional":true,"type":"array","items":{"$ref":"RuleMatch"}},{"name":"pseudoElements","description":"Pseudo style matches for this node.","optional":true,"type":"array","items":{"$ref":"PseudoElementMatches"}},{"name":"inherited","description":"A chain of inherited styles (from the immediate node parent up to the DOM tree root).","optional":true,"type":"array","items":{"$ref":"InheritedStyleEntry"}},{"name":"cssKeyframesRules","description":"A list of CSS keyframed animations matching this node.","optional":true,"type":"array","items":{"$ref":"CSSKeyframesRule"}}]},{"name":"getMediaQueries","description":"Returns all media queries parsed by the rendering engine.","returns":[{"name":"medias","type":"array","items":{"$ref":"CSSMedia"}}]},{"name":"getPlatformFontsForNode","description":"Requests information about platform fonts which we used to render child TextNodes in the given\nnode.","parameters":[{"name":"nodeId","$ref":"DOM.NodeId"}],"returns":[{"name":"fonts","description":"Usage statistics for every employed platform font.","type":"array","items":{"$ref":"PlatformFontUsage"}}]},{"name":"getStyleSheetText","description":"Returns the current textual content for a stylesheet.","parameters":[{"name":"styleSheetId","$ref":"StyleSheetId"}],"returns":[{"name":"text","description":"The stylesheet text.","type":"string"}]},{"name":"setEffectivePropertyValueForNode","description":"Find a rule with the given active property for the given node and set the new value for this\nproperty","parameters":[{"name":"nodeId","description":"The element id for which to set property.","$ref":"DOM.NodeId"},{"name":"propertyName","type":"string"},{"name":"value","type":"string"}]},{"name":"setKeyframeKey","description":"Modifies the keyframe rule key text.","parameters":[{"name":"styleSheetId","$ref":"StyleSheetId"},{"name":"range","$ref":"SourceRange"},{"name":"keyText","type":"string"}],"returns":[{"name":"keyText","description":"The resulting key text after modification.","$ref":"Value"}]},{"name":"setMediaText","description":"Modifies the rule selector.","parameters":[{"name":"styleSheetId","$ref":"StyleSheetId"},{"name":"range","$ref":"SourceRange"},{"name":"text","type":"string"}],"returns":[{"name":"media","description":"The resulting CSS media rule after modification.","$ref":"CSSMedia"}]},{"name":"setRuleSelector","description":"Modifies the rule selector.","parameters":[{"name":"styleSheetId","$ref":"StyleSheetId"},{"name":"range","$ref":"SourceRange"},{"name":"selector","type":"string"}],"returns":[{"name":"selectorList","description":"The resulting selector list after modification.","$ref":"SelectorList"}]},{"name":"setStyleSheetText","description":"Sets the new stylesheet text.","parameters":[{"name":"styleSheetId","$ref":"StyleSheetId"},{"name":"text","type":"string"}],"returns":[{"name":"sourceMapURL","description":"URL of source map associated with script (if any).","optional":true,"type":"string"}]},{"name":"setStyleTexts","description":"Applies specified style edits one after another in the given order.","parameters":[{"name":"edits","type":"array","items":{"$ref":"StyleDeclarationEdit"}}],"returns":[{"name":"styles","description":"The resulting styles after modification.","type":"array","items":{"$ref":"CSSStyle"}}]},{"name":"startRuleUsageTracking","description":"Enables the selector recording."},{"name":"stopRuleUsageTracking","description":"Stop tracking rule usage and return the list of rules that were used since last call to\n`takeCoverageDelta` (or since start of coverage instrumentation)","returns":[{"name":"ruleUsage","type":"array","items":{"$ref":"RuleUsage"}}]},{"name":"takeCoverageDelta","description":"Obtain list of rules that became used since last call to this method (or since start of coverage\ninstrumentation)","returns":[{"name":"coverage","type":"array","items":{"$ref":"RuleUsage"}}]}],"events":[{"name":"fontsUpdated","description":"Fires whenever a web font is updated.  A non-empty font parameter indicates a successfully loaded\nweb font","parameters":[{"name":"font","description":"The web font that has loaded.","optional":true,"$ref":"FontFace"}]},{"name":"mediaQueryResultChanged","description":"Fires whenever a MediaQuery result changes (for example, after a browser window has been\nresized.) The current implementation considers only viewport-dependent media features."},{"name":"styleSheetAdded","description":"Fired whenever an active document stylesheet is added.","parameters":[{"name":"header","description":"Added stylesheet metainfo.","$ref":"CSSStyleSheetHeader"}]},{"name":"styleSheetChanged","description":"Fired whenever a stylesheet is changed as a result of the client operation.","parameters":[{"name":"styleSheetId","$ref":"StyleSheetId"}]},{"name":"styleSheetRemoved","description":"Fired whenever an active document stylesheet is removed.","parameters":[{"name":"styleSheetId","description":"Identifier of the removed stylesheet.","$ref":"StyleSheetId"}]}]},{"domain":"CacheStorage","experimental":true,"types":[{"id":"CacheId","description":"Unique identifier of the Cache object.","type":"string"},{"id":"CachedResponseType","description":"type of HTTP response cached","type":"string","enum":["basic","cors","default","error","opaqueResponse","opaqueRedirect"]},{"id":"DataEntry","description":"Data entry.","type":"object","properties":[{"name":"requestURL","description":"Request URL.","type":"string"},{"name":"requestMethod","description":"Request method.","type":"string"},{"name":"requestHeaders","description":"Request headers","type":"array","items":{"$ref":"Header"}},{"name":"responseTime","description":"Number of seconds since epoch.","type":"number"},{"name":"responseStatus","description":"HTTP response status code.","type":"integer"},{"name":"responseStatusText","description":"HTTP response status text.","type":"string"},{"name":"responseType","description":"HTTP response type","$ref":"CachedResponseType"},{"name":"responseHeaders","description":"Response headers","type":"array","items":{"$ref":"Header"}}]},{"id":"Cache","description":"Cache identifier.","type":"object","properties":[{"name":"cacheId","description":"An opaque unique id of the cache.","$ref":"CacheId"},{"name":"securityOrigin","description":"Security origin of the cache.","type":"string"},{"name":"cacheName","description":"The name of the cache.","type":"string"}]},{"id":"Header","type":"object","properties":[{"name":"name","type":"string"},{"name":"value","type":"string"}]},{"id":"CachedResponse","description":"Cached response","type":"object","properties":[{"name":"body","description":"Entry content, base64-encoded.","type":"string"}]}],"commands":[{"name":"deleteCache","description":"Deletes a cache.","parameters":[{"name":"cacheId","description":"Id of cache for deletion.","$ref":"CacheId"}]},{"name":"deleteEntry","description":"Deletes a cache entry.","parameters":[{"name":"cacheId","description":"Id of cache where the entry will be deleted.","$ref":"CacheId"},{"name":"request","description":"URL spec of the request.","type":"string"}]},{"name":"requestCacheNames","description":"Requests cache names.","parameters":[{"name":"securityOrigin","description":"Security origin.","type":"string"}],"returns":[{"name":"caches","description":"Caches for the security origin.","type":"array","items":{"$ref":"Cache"}}]},{"name":"requestCachedResponse","description":"Fetches cache entry.","parameters":[{"name":"cacheId","description":"Id of cache that contains the enty.","$ref":"CacheId"},{"name":"requestURL","description":"URL spec of the request.","type":"string"}],"returns":[{"name":"response","description":"Response read from the cache.","$ref":"CachedResponse"}]},{"name":"requestEntries","description":"Requests data from cache.","parameters":[{"name":"cacheId","description":"ID of cache to get entries from.","$ref":"CacheId"},{"name":"skipCount","description":"Number of records to skip.","type":"integer"},{"name":"pageSize","description":"Number of records to fetch.","type":"integer"}],"returns":[{"name":"cacheDataEntries","description":"Array of object store data entries.","type":"array","items":{"$ref":"DataEntry"}},{"name":"hasMore","description":"If true, there are more entries to fetch in the given range.","type":"boolean"}]}]},{"domain":"DOM","description":"This domain exposes DOM read/write operations. Each DOM Node is represented with its mirror object\nthat has an `id`. This `id` can be used to get additional information on the Node, resolve it into\nthe JavaScript object wrapper, etc. It is important that client receives DOM events only for the\nnodes that are known to the client. Backend keeps track of the nodes that were sent to the client\nand never sends the same node twice. It is client's responsibility to collect information about\nthe nodes that were sent to the client.<p>Note that `iframe` owner elements will return\ncorresponding document elements as their child nodes.</p>","dependencies":["Runtime"],"types":[{"id":"NodeId","description":"Unique DOM node identifier.","type":"integer"},{"id":"BackendNodeId","description":"Unique DOM node identifier used to reference a node that may not have been pushed to the\nfront-end.","type":"integer"},{"id":"BackendNode","description":"Backend node with a friendly name.","type":"object","properties":[{"name":"nodeType","description":"`Node`'s nodeType.","type":"integer"},{"name":"nodeName","description":"`Node`'s nodeName.","type":"string"},{"name":"backendNodeId","$ref":"BackendNodeId"}]},{"id":"PseudoType","description":"Pseudo element type.","type":"string","enum":["first-line","first-letter","before","after","backdrop","selection","first-line-inherited","scrollbar","scrollbar-thumb","scrollbar-button","scrollbar-track","scrollbar-track-piece","scrollbar-corner","resizer","input-list-button"]},{"id":"ShadowRootType","description":"Shadow root type.","type":"string","enum":["user-agent","open","closed"]},{"id":"Node","description":"DOM interaction is implemented in terms of mirror objects that represent the actual DOM nodes.\nDOMNode is a base node mirror type.","type":"object","properties":[{"name":"nodeId","description":"Node identifier that is passed into the rest of the DOM messages as the `nodeId`. Backend\nwill only push node with given `id` once. It is aware of all requested nodes and will only\nfire DOM events for nodes known to the client.","$ref":"NodeId"},{"name":"parentId","description":"The id of the parent node if any.","optional":true,"$ref":"NodeId"},{"name":"backendNodeId","description":"The BackendNodeId for this node.","$ref":"BackendNodeId"},{"name":"nodeType","description":"`Node`'s nodeType.","type":"integer"},{"name":"nodeName","description":"`Node`'s nodeName.","type":"string"},{"name":"localName","description":"`Node`'s localName.","type":"string"},{"name":"nodeValue","description":"`Node`'s nodeValue.","type":"string"},{"name":"childNodeCount","description":"Child count for `Container` nodes.","optional":true,"type":"integer"},{"name":"children","description":"Child nodes of this node when requested with children.","optional":true,"type":"array","items":{"$ref":"Node"}},{"name":"attributes","description":"Attributes of the `Element` node in the form of flat array `[name1, value1, name2, value2]`.","optional":true,"type":"array","items":{"type":"string"}},{"name":"documentURL","description":"Document URL that `Document` or `FrameOwner` node points to.","optional":true,"type":"string"},{"name":"baseURL","description":"Base URL that `Document` or `FrameOwner` node uses for URL completion.","optional":true,"type":"string"},{"name":"publicId","description":"`DocumentType`'s publicId.","optional":true,"type":"string"},{"name":"systemId","description":"`DocumentType`'s systemId.","optional":true,"type":"string"},{"name":"internalSubset","description":"`DocumentType`'s internalSubset.","optional":true,"type":"string"},{"name":"xmlVersion","description":"`Document`'s XML version in case of XML documents.","optional":true,"type":"string"},{"name":"name","description":"`Attr`'s name.","optional":true,"type":"string"},{"name":"value","description":"`Attr`'s value.","optional":true,"type":"string"},{"name":"pseudoType","description":"Pseudo element type for this node.","optional":true,"$ref":"PseudoType"},{"name":"shadowRootType","description":"Shadow root type.","optional":true,"$ref":"ShadowRootType"},{"name":"frameId","description":"Frame ID for frame owner elements.","optional":true,"$ref":"Page.FrameId"},{"name":"contentDocument","description":"Content document for frame owner elements.","optional":true,"$ref":"Node"},{"name":"shadowRoots","description":"Shadow root list for given element host.","optional":true,"type":"array","items":{"$ref":"Node"}},{"name":"templateContent","description":"Content document fragment for template elements.","optional":true,"$ref":"Node"},{"name":"pseudoElements","description":"Pseudo elements associated with this node.","optional":true,"type":"array","items":{"$ref":"Node"}},{"name":"importedDocument","description":"Import document for the HTMLImport links.","optional":true,"$ref":"Node"},{"name":"distributedNodes","description":"Distributed nodes for given insertion point.","optional":true,"type":"array","items":{"$ref":"BackendNode"}},{"name":"isSVG","description":"Whether the node is SVG.","optional":true,"type":"boolean"}]},{"id":"RGBA","description":"A structure holding an RGBA color.","type":"object","properties":[{"name":"r","description":"The red component, in the [0-255] range.","type":"integer"},{"name":"g","description":"The green component, in the [0-255] range.","type":"integer"},{"name":"b","description":"The blue component, in the [0-255] range.","type":"integer"},{"name":"a","description":"The alpha component, in the [0-1] range (default: 1).","optional":true,"type":"number"}]},{"id":"Quad","description":"An array of quad vertices, x immediately followed by y for each point, points clock-wise.","type":"array","items":{"type":"number"}},{"id":"BoxModel","description":"Box model.","type":"object","properties":[{"name":"content","description":"Content box","$ref":"Quad"},{"name":"padding","description":"Padding box","$ref":"Quad"},{"name":"border","description":"Border box","$ref":"Quad"},{"name":"margin","description":"Margin box","$ref":"Quad"},{"name":"width","description":"Node width","type":"integer"},{"name":"height","description":"Node height","type":"integer"},{"name":"shapeOutside","description":"Shape outside coordinates","optional":true,"$ref":"ShapeOutsideInfo"}]},{"id":"ShapeOutsideInfo","description":"CSS Shape Outside details.","type":"object","properties":[{"name":"bounds","description":"Shape bounds","$ref":"Quad"},{"name":"shape","description":"Shape coordinate details","type":"array","items":{"type":"any"}},{"name":"marginShape","description":"Margin shape bounds","type":"array","items":{"type":"any"}}]},{"id":"Rect","description":"Rectangle.","type":"object","properties":[{"name":"x","description":"X coordinate","type":"number"},{"name":"y","description":"Y coordinate","type":"number"},{"name":"width","description":"Rectangle width","type":"number"},{"name":"height","description":"Rectangle height","type":"number"}]}],"commands":[{"name":"collectClassNamesFromSubtree","description":"Collects class names for the node with given id and all of it's child nodes.","experimental":true,"parameters":[{"name":"nodeId","description":"Id of the node to collect class names.","$ref":"NodeId"}],"returns":[{"name":"classNames","description":"Class name list.","type":"array","items":{"type":"string"}}]},{"name":"copyTo","description":"Creates a deep copy of the specified node and places it into the target container before the\ngiven anchor.","experimental":true,"parameters":[{"name":"nodeId","description":"Id of the node to copy.","$ref":"NodeId"},{"name":"targetNodeId","description":"Id of the element to drop the copy into.","$ref":"NodeId"},{"name":"insertBeforeNodeId","description":"Drop the copy before this node (if absent, the copy becomes the last child of\n`targetNodeId`).","optional":true,"$ref":"NodeId"}],"returns":[{"name":"nodeId","description":"Id of the node clone.","$ref":"NodeId"}]},{"name":"describeNode","description":"Describes node given its id, does not require domain to be enabled. Does not start tracking any\nobjects, can be used for automation.","parameters":[{"name":"nodeId","description":"Identifier of the node.","optional":true,"$ref":"NodeId"},{"name":"backendNodeId","description":"Identifier of the backend node.","optional":true,"$ref":"BackendNodeId"},{"name":"objectId","description":"JavaScript object id of the node wrapper.","optional":true,"$ref":"Runtime.RemoteObjectId"},{"name":"depth","description":"The maximum depth at which children should be retrieved, defaults to 1. Use -1 for the\nentire subtree or provide an integer larger than 0.","optional":true,"type":"integer"},{"name":"pierce","description":"Whether or not iframes and shadow roots should be traversed when returning the subtree\n(default is false).","optional":true,"type":"boolean"}],"returns":[{"name":"node","description":"Node description.","$ref":"Node"}]},{"name":"disable","description":"Disables DOM agent for the given page."},{"name":"discardSearchResults","description":"Discards search results from the session with the given id. `getSearchResults` should no longer\nbe called for that search.","experimental":true,"parameters":[{"name":"searchId","description":"Unique search session identifier.","type":"string"}]},{"name":"enable","description":"Enables DOM agent for the given page."},{"name":"focus","description":"Focuses the given element.","parameters":[{"name":"nodeId","description":"Identifier of the node.","optional":true,"$ref":"NodeId"},{"name":"backendNodeId","description":"Identifier of the backend node.","optional":true,"$ref":"BackendNodeId"},{"name":"objectId","description":"JavaScript object id of the node wrapper.","optional":true,"$ref":"Runtime.RemoteObjectId"}]},{"name":"getAttributes","description":"Returns attributes for the specified node.","parameters":[{"name":"nodeId","description":"Id of the node to retrieve attibutes for.","$ref":"NodeId"}],"returns":[{"name":"attributes","description":"An interleaved array of node attribute names and values.","type":"array","items":{"type":"string"}}]},{"name":"getBoxModel","description":"Returns boxes for the given node.","parameters":[{"name":"nodeId","description":"Identifier of the node.","optional":true,"$ref":"NodeId"},{"name":"backendNodeId","description":"Identifier of the backend node.","optional":true,"$ref":"BackendNodeId"},{"name":"objectId","description":"JavaScript object id of the node wrapper.","optional":true,"$ref":"Runtime.RemoteObjectId"}],"returns":[{"name":"model","description":"Box model for the node.","$ref":"BoxModel"}]},{"name":"getContentQuads","description":"Returns quads that describe node position on the page. This method\nmight return multiple quads for inline nodes.","experimental":true,"parameters":[{"name":"nodeId","description":"Identifier of the node.","optional":true,"$ref":"NodeId"},{"name":"backendNodeId","description":"Identifier of the backend node.","optional":true,"$ref":"BackendNodeId"},{"name":"objectId","description":"JavaScript object id of the node wrapper.","optional":true,"$ref":"Runtime.RemoteObjectId"}],"returns":[{"name":"quads","description":"Quads that describe node layout relative to viewport.","type":"array","items":{"$ref":"Quad"}}]},{"name":"getDocument","description":"Returns the root DOM node (and optionally the subtree) to the caller.","parameters":[{"name":"depth","description":"The maximum depth at which children should be retrieved, defaults to 1. Use -1 for the\nentire subtree or provide an integer larger than 0.","optional":true,"type":"integer"},{"name":"pierce","description":"Whether or not iframes and shadow roots should be traversed when returning the subtree\n(default is false).","optional":true,"type":"boolean"}],"returns":[{"name":"root","description":"Resulting node.","$ref":"Node"}]},{"name":"getFlattenedDocument","description":"Returns the root DOM node (and optionally the subtree) to the caller.","parameters":[{"name":"depth","description":"The maximum depth at which children should be retrieved, defaults to 1. Use -1 for the\nentire subtree or provide an integer larger than 0.","optional":true,"type":"integer"},{"name":"pierce","description":"Whether or not iframes and shadow roots should be traversed when returning the subtree\n(default is false).","optional":true,"type":"boolean"}],"returns":[{"name":"nodes","description":"Resulting node.","type":"array","items":{"$ref":"Node"}}]},{"name":"getNodeForLocation","description":"Returns node id at given location. Depending on whether DOM domain is enabled, nodeId is\neither returned or not.","experimental":true,"parameters":[{"name":"x","description":"X coordinate.","type":"integer"},{"name":"y","description":"Y coordinate.","type":"integer"},{"name":"includeUserAgentShadowDOM","description":"False to skip to the nearest non-UA shadow root ancestor (default: false).","optional":true,"type":"boolean"}],"returns":[{"name":"backendNodeId","description":"Resulting node.","$ref":"BackendNodeId"},{"name":"nodeId","description":"Id of the node at given coordinates, only when enabled.","optional":true,"$ref":"NodeId"}]},{"name":"getOuterHTML","description":"Returns node's HTML markup.","parameters":[{"name":"nodeId","description":"Identifier of the node.","optional":true,"$ref":"NodeId"},{"name":"backendNodeId","description":"Identifier of the backend node.","optional":true,"$ref":"BackendNodeId"},{"name":"objectId","description":"JavaScript object id of the node wrapper.","optional":true,"$ref":"Runtime.RemoteObjectId"}],"returns":[{"name":"outerHTML","description":"Outer HTML markup.","type":"string"}]},{"name":"getRelayoutBoundary","description":"Returns the id of the nearest ancestor that is a relayout boundary.","experimental":true,"parameters":[{"name":"nodeId","description":"Id of the node.","$ref":"NodeId"}],"returns":[{"name":"nodeId","description":"Relayout boundary node id for the given node.","$ref":"NodeId"}]},{"name":"getSearchResults","description":"Returns search results from given `fromIndex` to given `toIndex` from the search with the given\nidentifier.","experimental":true,"parameters":[{"name":"searchId","description":"Unique search session identifier.","type":"string"},{"name":"fromIndex","description":"Start index of the search result to be returned.","type":"integer"},{"name":"toIndex","description":"End index of the search result to be returned.","type":"integer"}],"returns":[{"name":"nodeIds","description":"Ids of the search result nodes.","type":"array","items":{"$ref":"NodeId"}}]},{"name":"hideHighlight","description":"Hides any highlight.","redirect":"Overlay"},{"name":"highlightNode","description":"Highlights DOM node.","redirect":"Overlay"},{"name":"highlightRect","description":"Highlights given rectangle.","redirect":"Overlay"},{"name":"markUndoableState","description":"Marks last undoable state.","experimental":true},{"name":"moveTo","description":"Moves node into the new container, places it before the given anchor.","parameters":[{"name":"nodeId","description":"Id of the node to move.","$ref":"NodeId"},{"name":"targetNodeId","description":"Id of the element to drop the moved node into.","$ref":"NodeId"},{"name":"insertBeforeNodeId","description":"Drop node before this one (if absent, the moved node becomes the last child of\n`targetNodeId`).","optional":true,"$ref":"NodeId"}],"returns":[{"name":"nodeId","description":"New id of the moved node.","$ref":"NodeId"}]},{"name":"performSearch","description":"Searches for a given string in the DOM tree. Use `getSearchResults` to access search results or\n`cancelSearch` to end this search session.","experimental":true,"parameters":[{"name":"query","description":"Plain text or query selector or XPath search query.","type":"string"},{"name":"includeUserAgentShadowDOM","description":"True to search in user agent shadow DOM.","optional":true,"type":"boolean"}],"returns":[{"name":"searchId","description":"Unique search session identifier.","type":"string"},{"name":"resultCount","description":"Number of search results.","type":"integer"}]},{"name":"pushNodeByPathToFrontend","description":"Requests that the node is sent to the caller given its path. // FIXME, use XPath","experimental":true,"parameters":[{"name":"path","description":"Path to node in the proprietary format.","type":"string"}],"returns":[{"name":"nodeId","description":"Id of the node for given path.","$ref":"NodeId"}]},{"name":"pushNodesByBackendIdsToFrontend","description":"Requests that a batch of nodes is sent to the caller given their backend node ids.","experimental":true,"parameters":[{"name":"backendNodeIds","description":"The array of backend node ids.","type":"array","items":{"$ref":"BackendNodeId"}}],"returns":[{"name":"nodeIds","description":"The array of ids of pushed nodes that correspond to the backend ids specified in\nbackendNodeIds.","type":"array","items":{"$ref":"NodeId"}}]},{"name":"querySelector","description":"Executes `querySelector` on a given node.","parameters":[{"name":"nodeId","description":"Id of the node to query upon.","$ref":"NodeId"},{"name":"selector","description":"Selector string.","type":"string"}],"returns":[{"name":"nodeId","description":"Query selector result.","$ref":"NodeId"}]},{"name":"querySelectorAll","description":"Executes `querySelectorAll` on a given node.","parameters":[{"name":"nodeId","description":"Id of the node to query upon.","$ref":"NodeId"},{"name":"selector","description":"Selector string.","type":"string"}],"returns":[{"name":"nodeIds","description":"Query selector result.","type":"array","items":{"$ref":"NodeId"}}]},{"name":"redo","description":"Re-does the last undone action.","experimental":true},{"name":"removeAttribute","description":"Removes attribute with given name from an element with given id.","parameters":[{"name":"nodeId","description":"Id of the element to remove attribute from.","$ref":"NodeId"},{"name":"name","description":"Name of the attribute to remove.","type":"string"}]},{"name":"removeNode","description":"Removes node with given id.","parameters":[{"name":"nodeId","description":"Id of the node to remove.","$ref":"NodeId"}]},{"name":"requestChildNodes","description":"Requests that children of the node with given id are returned to the caller in form of\n`setChildNodes` events where not only immediate children are retrieved, but all children down to\nthe specified depth.","parameters":[{"name":"nodeId","description":"Id of the node to get children for.","$ref":"NodeId"},{"name":"depth","description":"The maximum depth at which children should be retrieved, defaults to 1. Use -1 for the\nentire subtree or provide an integer larger than 0.","optional":true,"type":"integer"},{"name":"pierce","description":"Whether or not iframes and shadow roots should be traversed when returning the sub-tree\n(default is false).","optional":true,"type":"boolean"}]},{"name":"requestNode","description":"Requests that the node is sent to the caller given the JavaScript node object reference. All\nnodes that form the path from the node to the root are also sent to the client as a series of\n`setChildNodes` notifications.","parameters":[{"name":"objectId","description":"JavaScript object id to convert into node.","$ref":"Runtime.RemoteObjectId"}],"returns":[{"name":"nodeId","description":"Node id for given object.","$ref":"NodeId"}]},{"name":"resolveNode","description":"Resolves the JavaScript node object for a given NodeId or BackendNodeId.","parameters":[{"name":"nodeId","description":"Id of the node to resolve.","optional":true,"$ref":"NodeId"},{"name":"backendNodeId","description":"Backend identifier of the node to resolve.","optional":true,"$ref":"DOM.BackendNodeId"},{"name":"objectGroup","description":"Symbolic group name that can be used to release multiple objects.","optional":true,"type":"string"}],"returns":[{"name":"object","description":"JavaScript object wrapper for given node.","$ref":"Runtime.RemoteObject"}]},{"name":"setAttributeValue","description":"Sets attribute for an element with given id.","parameters":[{"name":"nodeId","description":"Id of the element to set attribute for.","$ref":"NodeId"},{"name":"name","description":"Attribute name.","type":"string"},{"name":"value","description":"Attribute value.","type":"string"}]},{"name":"setAttributesAsText","description":"Sets attributes on element with given id. This method is useful when user edits some existing\nattribute value and types in several attribute name/value pairs.","parameters":[{"name":"nodeId","description":"Id of the element to set attributes for.","$ref":"NodeId"},{"name":"text","description":"Text with a number of attributes. Will parse this text using HTML parser.","type":"string"},{"name":"name","description":"Attribute name to replace with new attributes derived from text in case text parsed\nsuccessfully.","optional":true,"type":"string"}]},{"name":"setFileInputFiles","description":"Sets files for the given file input element.","parameters":[{"name":"files","description":"Array of file paths to set.","type":"array","items":{"type":"string"}},{"name":"nodeId","description":"Identifier of the node.","optional":true,"$ref":"NodeId"},{"name":"backendNodeId","description":"Identifier of the backend node.","optional":true,"$ref":"BackendNodeId"},{"name":"objectId","description":"JavaScript object id of the node wrapper.","optional":true,"$ref":"Runtime.RemoteObjectId"}]},{"name":"setInspectedNode","description":"Enables console to refer to the node with given id via $x (see Command Line API for more details\n$x functions).","experimental":true,"parameters":[{"name":"nodeId","description":"DOM node id to be accessible by means of $x command line API.","$ref":"NodeId"}]},{"name":"setNodeName","description":"Sets node name for a node with given id.","parameters":[{"name":"nodeId","description":"Id of the node to set name for.","$ref":"NodeId"},{"name":"name","description":"New node's name.","type":"string"}],"returns":[{"name":"nodeId","description":"New node's id.","$ref":"NodeId"}]},{"name":"setNodeValue","description":"Sets node value for a node with given id.","parameters":[{"name":"nodeId","description":"Id of the node to set value for.","$ref":"NodeId"},{"name":"value","description":"New node's value.","type":"string"}]},{"name":"setOuterHTML","description":"Sets node HTML markup, returns new node id.","parameters":[{"name":"nodeId","description":"Id of the node to set markup for.","$ref":"NodeId"},{"name":"outerHTML","description":"Outer HTML markup to set.","type":"string"}]},{"name":"undo","description":"Undoes the last performed action.","experimental":true},{"name":"getFrameOwner","description":"Returns iframe node that owns iframe with the given domain.","experimental":true,"parameters":[{"name":"frameId","$ref":"Page.FrameId"}],"returns":[{"name":"backendNodeId","description":"Resulting node.","$ref":"BackendNodeId"},{"name":"nodeId","description":"Id of the node at given coordinates, only when enabled.","optional":true,"$ref":"NodeId"}]}],"events":[{"name":"attributeModified","description":"Fired when `Element`'s attribute is modified.","parameters":[{"name":"nodeId","description":"Id of the node that has changed.","$ref":"NodeId"},{"name":"name","description":"Attribute name.","type":"string"},{"name":"value","description":"Attribute value.","type":"string"}]},{"name":"attributeRemoved","description":"Fired when `Element`'s attribute is removed.","parameters":[{"name":"nodeId","description":"Id of the node that has changed.","$ref":"NodeId"},{"name":"name","description":"A ttribute name.","type":"string"}]},{"name":"characterDataModified","description":"Mirrors `DOMCharacterDataModified` event.","parameters":[{"name":"nodeId","description":"Id of the node that has changed.","$ref":"NodeId"},{"name":"characterData","description":"New text value.","type":"string"}]},{"name":"childNodeCountUpdated","description":"Fired when `Container`'s child node count has changed.","parameters":[{"name":"nodeId","description":"Id of the node that has changed.","$ref":"NodeId"},{"name":"childNodeCount","description":"New node count.","type":"integer"}]},{"name":"childNodeInserted","description":"Mirrors `DOMNodeInserted` event.","parameters":[{"name":"parentNodeId","description":"Id of the node that has changed.","$ref":"NodeId"},{"name":"previousNodeId","description":"If of the previous siblint.","$ref":"NodeId"},{"name":"node","description":"Inserted node data.","$ref":"Node"}]},{"name":"childNodeRemoved","description":"Mirrors `DOMNodeRemoved` event.","parameters":[{"name":"parentNodeId","description":"Parent id.","$ref":"NodeId"},{"name":"nodeId","description":"Id of the node that has been removed.","$ref":"NodeId"}]},{"name":"distributedNodesUpdated","description":"Called when distrubution is changed.","experimental":true,"parameters":[{"name":"insertionPointId","description":"Insertion point where distrubuted nodes were updated.","$ref":"NodeId"},{"name":"distributedNodes","description":"Distributed nodes for given insertion point.","type":"array","items":{"$ref":"BackendNode"}}]},{"name":"documentUpdated","description":"Fired when `Document` has been totally updated. Node ids are no longer valid."},{"name":"inlineStyleInvalidated","description":"Fired when `Element`'s inline style is modified via a CSS property modification.","experimental":true,"parameters":[{"name":"nodeIds","description":"Ids of the nodes for which the inline styles have been invalidated.","type":"array","items":{"$ref":"NodeId"}}]},{"name":"pseudoElementAdded","description":"Called when a pseudo element is added to an element.","experimental":true,"parameters":[{"name":"parentId","description":"Pseudo element's parent element id.","$ref":"NodeId"},{"name":"pseudoElement","description":"The added pseudo element.","$ref":"Node"}]},{"name":"pseudoElementRemoved","description":"Called when a pseudo element is removed from an element.","experimental":true,"parameters":[{"name":"parentId","description":"Pseudo element's parent element id.","$ref":"NodeId"},{"name":"pseudoElementId","description":"The removed pseudo element id.","$ref":"NodeId"}]},{"name":"setChildNodes","description":"Fired when backend wants to provide client with the missing DOM structure. This happens upon\nmost of the calls requesting node ids.","parameters":[{"name":"parentId","description":"Parent node id to populate with children.","$ref":"NodeId"},{"name":"nodes","description":"Child nodes array.","type":"array","items":{"$ref":"Node"}}]},{"name":"shadowRootPopped","description":"Called when shadow root is popped from the element.","experimental":true,"parameters":[{"name":"hostId","description":"Host element id.","$ref":"NodeId"},{"name":"rootId","description":"Shadow root id.","$ref":"NodeId"}]},{"name":"shadowRootPushed","description":"Called when shadow root is pushed into the element.","experimental":true,"parameters":[{"name":"hostId","description":"Host element id.","$ref":"NodeId"},{"name":"root","description":"Shadow root.","$ref":"Node"}]}]},{"domain":"DOMDebugger","description":"DOM debugging allows setting breakpoints on particular DOM operations and events. JavaScript\nexecution will stop on these operations as if there was a regular breakpoint set.","dependencies":["DOM","Debugger","Runtime"],"types":[{"id":"DOMBreakpointType","description":"DOM breakpoint type.","type":"string","enum":["subtree-modified","attribute-modified","node-removed"]},{"id":"EventListener","description":"Object event listener.","type":"object","properties":[{"name":"type","description":"`EventListener`'s type.","type":"string"},{"name":"useCapture","description":"`EventListener`'s useCapture.","type":"boolean"},{"name":"passive","description":"`EventListener`'s passive flag.","type":"boolean"},{"name":"once","description":"`EventListener`'s once flag.","type":"boolean"},{"name":"scriptId","description":"Script id of the handler code.","$ref":"Runtime.ScriptId"},{"name":"lineNumber","description":"Line number in the script (0-based).","type":"integer"},{"name":"columnNumber","description":"Column number in the script (0-based).","type":"integer"},{"name":"handler","description":"Event handler function value.","optional":true,"$ref":"Runtime.RemoteObject"},{"name":"originalHandler","description":"Event original handler function value.","optional":true,"$ref":"Runtime.RemoteObject"},{"name":"backendNodeId","description":"Node the listener is added to (if any).","optional":true,"$ref":"DOM.BackendNodeId"}]}],"commands":[{"name":"getEventListeners","description":"Returns event listeners of the given object.","parameters":[{"name":"objectId","description":"Identifier of the object to return listeners for.","$ref":"Runtime.RemoteObjectId"},{"name":"depth","description":"The maximum depth at which Node children should be retrieved, defaults to 1. Use -1 for the\nentire subtree or provide an integer larger than 0.","optional":true,"type":"integer"},{"name":"pierce","description":"Whether or not iframes and shadow roots should be traversed when returning the subtree\n(default is false). Reports listeners for all contexts if pierce is enabled.","optional":true,"type":"boolean"}],"returns":[{"name":"listeners","description":"Array of relevant listeners.","type":"array","items":{"$ref":"EventListener"}}]},{"name":"removeDOMBreakpoint","description":"Removes DOM breakpoint that was set using `setDOMBreakpoint`.","parameters":[{"name":"nodeId","description":"Identifier of the node to remove breakpoint from.","$ref":"DOM.NodeId"},{"name":"type","description":"Type of the breakpoint to remove.","$ref":"DOMBreakpointType"}]},{"name":"removeEventListenerBreakpoint","description":"Removes breakpoint on particular DOM event.","parameters":[{"name":"eventName","description":"Event name.","type":"string"},{"name":"targetName","description":"EventTarget interface name.","experimental":true,"optional":true,"type":"string"}]},{"name":"removeInstrumentationBreakpoint","description":"Removes breakpoint on particular native event.","experimental":true,"parameters":[{"name":"eventName","description":"Instrumentation name to stop on.","type":"string"}]},{"name":"removeXHRBreakpoint","description":"Removes breakpoint from XMLHttpRequest.","parameters":[{"name":"url","description":"Resource URL substring.","type":"string"}]},{"name":"setDOMBreakpoint","description":"Sets breakpoint on particular operation with DOM.","parameters":[{"name":"nodeId","description":"Identifier of the node to set breakpoint on.","$ref":"DOM.NodeId"},{"name":"type","description":"Type of the operation to stop upon.","$ref":"DOMBreakpointType"}]},{"name":"setEventListenerBreakpoint","description":"Sets breakpoint on particular DOM event.","parameters":[{"name":"eventName","description":"DOM Event name to stop on (any DOM event will do).","type":"string"},{"name":"targetName","description":"EventTarget interface name to stop on. If equal to `\"*\"` or not provided, will stop on any\nEventTarget.","experimental":true,"optional":true,"type":"string"}]},{"name":"setInstrumentationBreakpoint","description":"Sets breakpoint on particular native event.","experimental":true,"parameters":[{"name":"eventName","description":"Instrumentation name to stop on.","type":"string"}]},{"name":"setXHRBreakpoint","description":"Sets breakpoint on XMLHttpRequest.","parameters":[{"name":"url","description":"Resource URL substring. All XHRs having this substring in the URL will get stopped upon.","type":"string"}]}]},{"domain":"DOMSnapshot","description":"This domain facilitates obtaining document snapshots with DOM, layout, and style information.","experimental":true,"dependencies":["CSS","DOM","DOMDebugger","Page"],"types":[{"id":"DOMNode","description":"A Node in the DOM tree.","type":"object","properties":[{"name":"nodeType","description":"`Node`'s nodeType.","type":"integer"},{"name":"nodeName","description":"`Node`'s nodeName.","type":"string"},{"name":"nodeValue","description":"`Node`'s nodeValue.","type":"string"},{"name":"textValue","description":"Only set for textarea elements, contains the text value.","optional":true,"type":"string"},{"name":"inputValue","description":"Only set for input elements, contains the input's associated text value.","optional":true,"type":"string"},{"name":"inputChecked","description":"Only set for radio and checkbox input elements, indicates if the element has been checked","optional":true,"type":"boolean"},{"name":"optionSelected","description":"Only set for option elements, indicates if the element has been selected","optional":true,"type":"boolean"},{"name":"backendNodeId","description":"`Node`'s id, corresponds to DOM.Node.backendNodeId.","$ref":"DOM.BackendNodeId"},{"name":"childNodeIndexes","description":"The indexes of the node's child nodes in the `domNodes` array returned by `getSnapshot`, if\nany.","optional":true,"type":"array","items":{"type":"integer"}},{"name":"attributes","description":"Attributes of an `Element` node.","optional":true,"type":"array","items":{"$ref":"NameValue"}},{"name":"pseudoElementIndexes","description":"Indexes of pseudo elements associated with this node in the `domNodes` array returned by\n`getSnapshot`, if any.","optional":true,"type":"array","items":{"type":"integer"}},{"name":"layoutNodeIndex","description":"The index of the node's related layout tree node in the `layoutTreeNodes` array returned by\n`getSnapshot`, if any.","optional":true,"type":"integer"},{"name":"documentURL","description":"Document URL that `Document` or `FrameOwner` node points to.","optional":true,"type":"string"},{"name":"baseURL","description":"Base URL that `Document` or `FrameOwner` node uses for URL completion.","optional":true,"type":"string"},{"name":"contentLanguage","description":"Only set for documents, contains the document's content language.","optional":true,"type":"string"},{"name":"documentEncoding","description":"Only set for documents, contains the document's character set encoding.","optional":true,"type":"string"},{"name":"publicId","description":"`DocumentType` node's publicId.","optional":true,"type":"string"},{"name":"systemId","description":"`DocumentType` node's systemId.","optional":true,"type":"string"},{"name":"frameId","description":"Frame ID for frame owner elements and also for the document node.","optional":true,"$ref":"Page.FrameId"},{"name":"contentDocumentIndex","description":"The index of a frame owner element's content document in the `domNodes` array returned by\n`getSnapshot`, if any.","optional":true,"type":"integer"},{"name":"pseudoType","description":"Type of a pseudo element node.","optional":true,"$ref":"DOM.PseudoType"},{"name":"shadowRootType","description":"Shadow root type.","optional":true,"$ref":"DOM.ShadowRootType"},{"name":"isClickable","description":"Whether this DOM node responds to mouse clicks. This includes nodes that have had click\nevent listeners attached via JavaScript as well as anchor tags that naturally navigate when\nclicked.","optional":true,"type":"boolean"},{"name":"eventListeners","description":"Details of the node's event listeners, if any.","optional":true,"type":"array","items":{"$ref":"DOMDebugger.EventListener"}},{"name":"currentSourceURL","description":"The selected url for nodes with a srcset attribute.","optional":true,"type":"string"},{"name":"originURL","description":"The url of the script (if any) that generates this node.","optional":true,"type":"string"},{"name":"scrollOffsetX","description":"Scroll offsets, set when this node is a Document.","optional":true,"type":"number"},{"name":"scrollOffsetY","optional":true,"type":"number"}]},{"id":"InlineTextBox","description":"Details of post layout rendered text positions. The exact layout should not be regarded as\nstable and may change between versions.","type":"object","properties":[{"name":"boundingBox","description":"The bounding box in document coordinates. Note that scroll offset of the document is ignored.","$ref":"DOM.Rect"},{"name":"startCharacterIndex","description":"The starting index in characters, for this post layout textbox substring. Characters that\nwould be represented as a surrogate pair in UTF-16 have length 2.","type":"integer"},{"name":"numCharacters","description":"The number of characters in this post layout textbox substring. Characters that would be\nrepresented as a surrogate pair in UTF-16 have length 2.","type":"integer"}]},{"id":"LayoutTreeNode","description":"Details of an element in the DOM tree with a LayoutObject.","type":"object","properties":[{"name":"domNodeIndex","description":"The index of the related DOM node in the `domNodes` array returned by `getSnapshot`.","type":"integer"},{"name":"boundingBox","description":"The bounding box in document coordinates. Note that scroll offset of the document is ignored.","$ref":"DOM.Rect"},{"name":"layoutText","description":"Contents of the LayoutText, if any.","optional":true,"type":"string"},{"name":"inlineTextNodes","description":"The post-layout inline text nodes, if any.","optional":true,"type":"array","items":{"$ref":"InlineTextBox"}},{"name":"styleIndex","description":"Index into the `computedStyles` array returned by `getSnapshot`.","optional":true,"type":"integer"},{"name":"paintOrder","description":"Global paint order index, which is determined by the stacking order of the nodes. Nodes\nthat are painted together will have the same index. Only provided if includePaintOrder in\ngetSnapshot was true.","optional":true,"type":"integer"},{"name":"isStackingContext","description":"Set to true to indicate the element begins a new stacking context.","optional":true,"type":"boolean"}]},{"id":"ComputedStyle","description":"A subset of the full ComputedStyle as defined by the request whitelist.","type":"object","properties":[{"name":"properties","description":"Name/value pairs of computed style properties.","type":"array","items":{"$ref":"NameValue"}}]},{"id":"NameValue","description":"A name/value pair.","type":"object","properties":[{"name":"name","description":"Attribute/property name.","type":"string"},{"name":"value","description":"Attribute/property value.","type":"string"}]},{"id":"StringIndex","description":"Index of the string in the strings table.","type":"integer"},{"id":"ArrayOfStrings","description":"Index of the string in the strings table.","type":"array","items":{"$ref":"StringIndex"}},{"id":"RareStringData","description":"Data that is only present on rare nodes.","type":"object","properties":[{"name":"index","type":"array","items":{"type":"integer"}},{"name":"value","type":"array","items":{"$ref":"StringIndex"}}]},{"id":"RareBooleanData","type":"object","properties":[{"name":"index","type":"array","items":{"type":"integer"}}]},{"id":"RareIntegerData","type":"object","properties":[{"name":"index","type":"array","items":{"type":"integer"}},{"name":"value","type":"array","items":{"type":"integer"}}]},{"id":"Rectangle","type":"array","items":{"type":"number"}},{"id":"DocumentSnapshot","description":"Document snapshot.","type":"object","properties":[{"name":"documentURL","description":"Document URL that `Document` or `FrameOwner` node points to.","$ref":"StringIndex"},{"name":"baseURL","description":"Base URL that `Document` or `FrameOwner` node uses for URL completion.","$ref":"StringIndex"},{"name":"contentLanguage","description":"Contains the document's content language.","$ref":"StringIndex"},{"name":"encodingName","description":"Contains the document's character set encoding.","$ref":"StringIndex"},{"name":"publicId","description":"`DocumentType` node's publicId.","$ref":"StringIndex"},{"name":"systemId","description":"`DocumentType` node's systemId.","$ref":"StringIndex"},{"name":"frameId","description":"Frame ID for frame owner elements and also for the document node.","$ref":"StringIndex"},{"name":"nodes","description":"A table with dom nodes.","$ref":"NodeTreeSnapshot"},{"name":"layout","description":"The nodes in the layout tree.","$ref":"LayoutTreeSnapshot"},{"name":"textBoxes","description":"The post-layout inline text nodes.","$ref":"TextBoxSnapshot"},{"name":"scrollOffsetX","description":"Scroll offsets.","optional":true,"type":"number"},{"name":"scrollOffsetY","optional":true,"type":"number"}]},{"id":"NodeTreeSnapshot","description":"Table containing nodes.","type":"object","properties":[{"name":"parentIndex","description":"Parent node index.","optional":true,"type":"array","items":{"type":"integer"}},{"name":"nodeType","description":"`Node`'s nodeType.","optional":true,"type":"array","items":{"type":"integer"}},{"name":"nodeName","description":"`Node`'s nodeName.","optional":true,"type":"array","items":{"$ref":"StringIndex"}},{"name":"nodeValue","description":"`Node`'s nodeValue.","optional":true,"type":"array","items":{"$ref":"StringIndex"}},{"name":"backendNodeId","description":"`Node`'s id, corresponds to DOM.Node.backendNodeId.","optional":true,"type":"array","items":{"$ref":"DOM.BackendNodeId"}},{"name":"attributes","description":"Attributes of an `Element` node. Flatten name, value pairs.","optional":true,"type":"array","items":{"$ref":"ArrayOfStrings"}},{"name":"textValue","description":"Only set for textarea elements, contains the text value.","optional":true,"$ref":"RareStringData"},{"name":"inputValue","description":"Only set for input elements, contains the input's associated text value.","optional":true,"$ref":"RareStringData"},{"name":"inputChecked","description":"Only set for radio and checkbox input elements, indicates if the element has been checked","optional":true,"$ref":"RareBooleanData"},{"name":"optionSelected","description":"Only set for option elements, indicates if the element has been selected","optional":true,"$ref":"RareBooleanData"},{"name":"contentDocumentIndex","description":"The index of the document in the list of the snapshot documents.","optional":true,"$ref":"RareIntegerData"},{"name":"pseudoType","description":"Type of a pseudo element node.","optional":true,"$ref":"RareStringData"},{"name":"isClickable","description":"Whether this DOM node responds to mouse clicks. This includes nodes that have had click\nevent listeners attached via JavaScript as well as anchor tags that naturally navigate when\nclicked.","optional":true,"$ref":"RareBooleanData"},{"name":"currentSourceURL","description":"The selected url for nodes with a srcset attribute.","optional":true,"$ref":"RareStringData"},{"name":"originURL","description":"The url of the script (if any) that generates this node.","optional":true,"$ref":"RareStringData"}]},{"id":"LayoutTreeSnapshot","description":"Details of an element in the DOM tree with a LayoutObject.","type":"object","properties":[{"name":"nodeIndex","description":"The index of the related DOM node in the `domNodes` array returned by `getSnapshot`.","type":"array","items":{"type":"integer"}},{"name":"styles","description":"Index into the `computedStyles` array returned by `captureSnapshot`.","type":"array","items":{"$ref":"ArrayOfStrings"}},{"name":"bounds","description":"The absolute position bounding box.","type":"array","items":{"$ref":"Rectangle"}},{"name":"text","description":"Contents of the LayoutText, if any.","type":"array","items":{"$ref":"StringIndex"}},{"name":"stackingContexts","description":"Stacking context information.","$ref":"RareBooleanData"}]},{"id":"TextBoxSnapshot","description":"Details of post layout rendered text positions. The exact layout should not be regarded as\nstable and may change between versions.","type":"object","properties":[{"name":"layoutIndex","description":"Intex of th elayout tree node that owns this box collection.","type":"array","items":{"type":"integer"}},{"name":"bounds","description":"The absolute position bounding box.","type":"array","items":{"$ref":"Rectangle"}},{"name":"start","description":"The starting index in characters, for this post layout textbox substring. Characters that\nwould be represented as a surrogate pair in UTF-16 have length 2.","type":"array","items":{"type":"integer"}},{"name":"length","description":"The number of characters in this post layout textbox substring. Characters that would be\nrepresented as a surrogate pair in UTF-16 have length 2.","type":"array","items":{"type":"integer"}}]}],"commands":[{"name":"disable","description":"Disables DOM snapshot agent for the given page."},{"name":"enable","description":"Enables DOM snapshot agent for the given page."},{"name":"getSnapshot","description":"Returns a document snapshot, including the full DOM tree of the root node (including iframes,\ntemplate contents, and imported documents) in a flattened array, as well as layout and\nwhite-listed computed style information for the nodes. Shadow DOM in the returned DOM tree is\nflattened.","deprecated":true,"parameters":[{"name":"computedStyleWhitelist","description":"Whitelist of computed styles to return.","type":"array","items":{"type":"string"}},{"name":"includeEventListeners","description":"Whether or not to retrieve details of DOM listeners (default false).","optional":true,"type":"boolean"},{"name":"includePaintOrder","description":"Whether to determine and include the paint order index of LayoutTreeNodes (default false).","optional":true,"type":"boolean"},{"name":"includeUserAgentShadowTree","description":"Whether to include UA shadow tree in the snapshot (default false).","optional":true,"type":"boolean"}],"returns":[{"name":"domNodes","description":"The nodes in the DOM tree. The DOMNode at index 0 corresponds to the root document.","type":"array","items":{"$ref":"DOMNode"}},{"name":"layoutTreeNodes","description":"The nodes in the layout tree.","type":"array","items":{"$ref":"LayoutTreeNode"}},{"name":"computedStyles","description":"Whitelisted ComputedStyle properties for each node in the layout tree.","type":"array","items":{"$ref":"ComputedStyle"}}]},{"name":"captureSnapshot","description":"Returns a document snapshot, including the full DOM tree of the root node (including iframes,\ntemplate contents, and imported documents) in a flattened array, as well as layout and\nwhite-listed computed style information for the nodes. Shadow DOM in the returned DOM tree is\nflattened.","parameters":[{"name":"computedStyles","description":"Whitelist of computed styles to return.","type":"array","items":{"type":"string"}}],"returns":[{"name":"documents","description":"The nodes in the DOM tree. The DOMNode at index 0 corresponds to the root document.","type":"array","items":{"$ref":"DocumentSnapshot"}},{"name":"strings","description":"Shared string table that all string properties refer to with indexes.","type":"array","items":{"type":"string"}}]}]},{"domain":"DOMStorage","description":"Query and modify DOM storage.","experimental":true,"types":[{"id":"StorageId","description":"DOM Storage identifier.","type":"object","properties":[{"name":"securityOrigin","description":"Security origin for the storage.","type":"string"},{"name":"isLocalStorage","description":"Whether the storage is local storage (not session storage).","type":"boolean"}]},{"id":"Item","description":"DOM Storage item.","type":"array","items":{"type":"string"}}],"commands":[{"name":"clear","parameters":[{"name":"storageId","$ref":"StorageId"}]},{"name":"disable","description":"Disables storage tracking, prevents storage events from being sent to the client."},{"name":"enable","description":"Enables storage tracking, storage events will now be delivered to the client."},{"name":"getDOMStorageItems","parameters":[{"name":"storageId","$ref":"StorageId"}],"returns":[{"name":"entries","type":"array","items":{"$ref":"Item"}}]},{"name":"removeDOMStorageItem","parameters":[{"name":"storageId","$ref":"StorageId"},{"name":"key","type":"string"}]},{"name":"setDOMStorageItem","parameters":[{"name":"storageId","$ref":"StorageId"},{"name":"key","type":"string"},{"name":"value","type":"string"}]}],"events":[{"name":"domStorageItemAdded","parameters":[{"name":"storageId","$ref":"StorageId"},{"name":"key","type":"string"},{"name":"newValue","type":"string"}]},{"name":"domStorageItemRemoved","parameters":[{"name":"storageId","$ref":"StorageId"},{"name":"key","type":"string"}]},{"name":"domStorageItemUpdated","parameters":[{"name":"storageId","$ref":"StorageId"},{"name":"key","type":"string"},{"name":"oldValue","type":"string"},{"name":"newValue","type":"string"}]},{"name":"domStorageItemsCleared","parameters":[{"name":"storageId","$ref":"StorageId"}]}]},{"domain":"Database","experimental":true,"types":[{"id":"DatabaseId","description":"Unique identifier of Database object.","type":"string"},{"id":"Database","description":"Database object.","type":"object","properties":[{"name":"id","description":"Database ID.","$ref":"DatabaseId"},{"name":"domain","description":"Database domain.","type":"string"},{"name":"name","description":"Database name.","type":"string"},{"name":"version","description":"Database version.","type":"string"}]},{"id":"Error","description":"Database error.","type":"object","properties":[{"name":"message","description":"Error message.","type":"string"},{"name":"code","description":"Error code.","type":"integer"}]}],"commands":[{"name":"disable","description":"Disables database tracking, prevents database events from being sent to the client."},{"name":"enable","description":"Enables database tracking, database events will now be delivered to the client."},{"name":"executeSQL","parameters":[{"name":"databaseId","$ref":"DatabaseId"},{"name":"query","type":"string"}],"returns":[{"name":"columnNames","optional":true,"type":"array","items":{"type":"string"}},{"name":"values","optional":true,"type":"array","items":{"type":"any"}},{"name":"sqlError","optional":true,"$ref":"Error"}]},{"name":"getDatabaseTableNames","parameters":[{"name":"databaseId","$ref":"DatabaseId"}],"returns":[{"name":"tableNames","type":"array","items":{"type":"string"}}]}],"events":[{"name":"addDatabase","parameters":[{"name":"database","$ref":"Database"}]}]},{"domain":"DeviceOrientation","experimental":true,"commands":[{"name":"clearDeviceOrientationOverride","description":"Clears the overridden Device Orientation."},{"name":"setDeviceOrientationOverride","description":"Overrides the Device Orientation.","parameters":[{"name":"alpha","description":"Mock alpha","type":"number"},{"name":"beta","description":"Mock beta","type":"number"},{"name":"gamma","description":"Mock gamma","type":"number"}]}]},{"domain":"Emulation","description":"This domain emulates different environments for the page.","dependencies":["DOM","Page","Runtime"],"types":[{"id":"ScreenOrientation","description":"Screen orientation.","type":"object","properties":[{"name":"type","description":"Orientation type.","type":"string","enum":["portraitPrimary","portraitSecondary","landscapePrimary","landscapeSecondary"]},{"name":"angle","description":"Orientation angle.","type":"integer"}]},{"id":"VirtualTimePolicy","description":"advance: If the scheduler runs out of immediate work, the virtual time base may fast forward to\nallow the next delayed task (if any) to run; pause: The virtual time base may not advance;\npauseIfNetworkFetchesPending: The virtual time base may not advance if there are any pending\nresource fetches.","experimental":true,"type":"string","enum":["advance","pause","pauseIfNetworkFetchesPending"]}],"commands":[{"name":"canEmulate","description":"Tells whether emulation is supported.","returns":[{"name":"result","description":"True if emulation is supported.","type":"boolean"}]},{"name":"clearDeviceMetricsOverride","description":"Clears the overriden device metrics."},{"name":"clearGeolocationOverride","description":"Clears the overriden Geolocation Position and Error."},{"name":"resetPageScaleFactor","description":"Requests that page scale factor is reset to initial values.","experimental":true},{"name":"setFocusEmulationEnabled","description":"Enables or disables simulating a focused and active page.","experimental":true,"parameters":[{"name":"enabled","description":"Whether to enable to disable focus emulation.","type":"boolean"}]},{"name":"setCPUThrottlingRate","description":"Enables CPU throttling to emulate slow CPUs.","experimental":true,"parameters":[{"name":"rate","description":"Throttling rate as a slowdown factor (1 is no throttle, 2 is 2x slowdown, etc).","type":"number"}]},{"name":"setDefaultBackgroundColorOverride","description":"Sets or clears an override of the default background color of the frame. This override is used\nif the content does not specify one.","parameters":[{"name":"color","description":"RGBA of the default background color. If not specified, any existing override will be\ncleared.","optional":true,"$ref":"DOM.RGBA"}]},{"name":"setDeviceMetricsOverride","description":"Overrides the values of device screen dimensions (window.screen.width, window.screen.height,\nwindow.innerWidth, window.innerHeight, and \"device-width\"/\"device-height\"-related CSS media\nquery results).","parameters":[{"name":"width","description":"Overriding width value in pixels (minimum 0, maximum 10000000). 0 disables the override.","type":"integer"},{"name":"height","description":"Overriding height value in pixels (minimum 0, maximum 10000000). 0 disables the override.","type":"integer"},{"name":"deviceScaleFactor","description":"Overriding device scale factor value. 0 disables the override.","type":"number"},{"name":"mobile","description":"Whether to emulate mobile device. This includes viewport meta tag, overlay scrollbars, text\nautosizing and more.","type":"boolean"},{"name":"scale","description":"Scale to apply to resulting view image.","experimental":true,"optional":true,"type":"number"},{"name":"screenWidth","description":"Overriding screen width value in pixels (minimum 0, maximum 10000000).","experimental":true,"optional":true,"type":"integer"},{"name":"screenHeight","description":"Overriding screen height value in pixels (minimum 0, maximum 10000000).","experimental":true,"optional":true,"type":"integer"},{"name":"positionX","description":"Overriding view X position on screen in pixels (minimum 0, maximum 10000000).","experimental":true,"optional":true,"type":"integer"},{"name":"positionY","description":"Overriding view Y position on screen in pixels (minimum 0, maximum 10000000).","experimental":true,"optional":true,"type":"integer"},{"name":"dontSetVisibleSize","description":"Do not set visible view size, rely upon explicit setVisibleSize call.","experimental":true,"optional":true,"type":"boolean"},{"name":"screenOrientation","description":"Screen orientation override.","optional":true,"$ref":"ScreenOrientation"},{"name":"viewport","description":"If set, the visible area of the page will be overridden to this viewport. This viewport\nchange is not observed by the page, e.g. viewport-relative elements do not change positions.","experimental":true,"optional":true,"$ref":"Page.Viewport"}]},{"name":"setScrollbarsHidden","experimental":true,"parameters":[{"name":"hidden","description":"Whether scrollbars should be always hidden.","type":"boolean"}]},{"name":"setDocumentCookieDisabled","experimental":true,"parameters":[{"name":"disabled","description":"Whether document.coookie API should be disabled.","type":"boolean"}]},{"name":"setEmitTouchEventsForMouse","experimental":true,"parameters":[{"name":"enabled","description":"Whether touch emulation based on mouse input should be enabled.","type":"boolean"},{"name":"configuration","description":"Touch/gesture events configuration. Default: current platform.","optional":true,"type":"string","enum":["mobile","desktop"]}]},{"name":"setEmulatedMedia","description":"Emulates the given media for CSS media queries.","parameters":[{"name":"media","description":"Media type to emulate. Empty string disables the override.","type":"string"}]},{"name":"setGeolocationOverride","description":"Overrides the Geolocation Position or Error. Omitting any of the parameters emulates position\nunavailable.","parameters":[{"name":"latitude","description":"Mock latitude","optional":true,"type":"number"},{"name":"longitude","description":"Mock longitude","optional":true,"type":"number"},{"name":"accuracy","description":"Mock accuracy","optional":true,"type":"number"}]},{"name":"setNavigatorOverrides","description":"Overrides value returned by the javascript navigator object.","experimental":true,"deprecated":true,"parameters":[{"name":"platform","description":"The platform navigator.platform should return.","type":"string"}]},{"name":"setPageScaleFactor","description":"Sets a specified page scale factor.","experimental":true,"parameters":[{"name":"pageScaleFactor","description":"Page scale factor.","type":"number"}]},{"name":"setScriptExecutionDisabled","description":"Switches script execution in the page.","parameters":[{"name":"value","description":"Whether script execution should be disabled in the page.","type":"boolean"}]},{"name":"setTouchEmulationEnabled","description":"Enables touch on platforms which do not support them.","parameters":[{"name":"enabled","description":"Whether the touch event emulation should be enabled.","type":"boolean"},{"name":"maxTouchPoints","description":"Maximum touch points supported. Defaults to one.","optional":true,"type":"integer"}]},{"name":"setVirtualTimePolicy","description":"Turns on virtual time for all frames (replacing real-time with a synthetic time source) and sets\nthe current virtual time policy.  Note this supersedes any previous time budget.","experimental":true,"parameters":[{"name":"policy","$ref":"VirtualTimePolicy"},{"name":"budget","description":"If set, after this many virtual milliseconds have elapsed virtual time will be paused and a\nvirtualTimeBudgetExpired event is sent.","optional":true,"type":"number"},{"name":"maxVirtualTimeTaskStarvationCount","description":"If set this specifies the maximum number of tasks that can be run before virtual is forced\nforwards to prevent deadlock.","optional":true,"type":"integer"},{"name":"waitForNavigation","description":"If set the virtual time policy change should be deferred until any frame starts navigating.\nNote any previous deferred policy change is superseded.","optional":true,"type":"boolean"},{"name":"initialVirtualTime","description":"If set, base::Time::Now will be overriden to initially return this value.","optional":true,"$ref":"Network.TimeSinceEpoch"}],"returns":[{"name":"virtualTimeTicksBase","description":"Absolute timestamp at which virtual time was first enabled (up time in milliseconds).","type":"number"}]},{"name":"setVisibleSize","description":"Resizes the frame/viewport of the page. Note that this does not affect the frame's container\n(e.g. browser window). Can be used to produce screenshots of the specified size. Not supported\non Android.","experimental":true,"deprecated":true,"parameters":[{"name":"width","description":"Frame width (DIP).","type":"integer"},{"name":"height","description":"Frame height (DIP).","type":"integer"}]},{"name":"setUserAgentOverride","description":"Allows overriding user agent with the given string.","parameters":[{"name":"userAgent","description":"User agent to use.","type":"string"},{"name":"acceptLanguage","description":"Browser langugage to emulate.","optional":true,"type":"string"},{"name":"platform","description":"The platform navigator.platform should return.","optional":true,"type":"string"}]}],"events":[{"name":"virtualTimeAdvanced","description":"Notification sent after the virtual time has advanced.","experimental":true,"parameters":[{"name":"virtualTimeElapsed","description":"The amount of virtual time that has elapsed in milliseconds since virtual time was first\nenabled.","type":"number"}]},{"name":"virtualTimeBudgetExpired","description":"Notification sent after the virtual time budget for the current VirtualTimePolicy has run out.","experimental":true},{"name":"virtualTimePaused","description":"Notification sent after the virtual time has paused.","experimental":true,"parameters":[{"name":"virtualTimeElapsed","description":"The amount of virtual time that has elapsed in milliseconds since virtual time was first\nenabled.","type":"number"}]}]},{"domain":"HeadlessExperimental","description":"This domain provides experimental commands only supported in headless mode.","experimental":true,"dependencies":["Page","Runtime"],"types":[{"id":"ScreenshotParams","description":"Encoding options for a screenshot.","type":"object","properties":[{"name":"format","description":"Image compression format (defaults to png).","optional":true,"type":"string","enum":["jpeg","png"]},{"name":"quality","description":"Compression quality from range [0..100] (jpeg only).","optional":true,"type":"integer"}]}],"commands":[{"name":"beginFrame","description":"Sends a BeginFrame to the target and returns when the frame was completed. Optionally captures a\nscreenshot from the resulting frame. Requires that the target was created with enabled\nBeginFrameControl. Designed for use with --run-all-compositor-stages-before-draw, see also\nhttps://goo.gl/3zHXhB for more background.","parameters":[{"name":"frameTimeTicks","description":"Timestamp of this BeginFrame in Renderer TimeTicks (milliseconds of uptime). If not set,\nthe current time will be used.","optional":true,"type":"number"},{"name":"interval","description":"The interval between BeginFrames that is reported to the compositor, in milliseconds.\nDefaults to a 60 frames/second interval, i.e. about 16.666 milliseconds.","optional":true,"type":"number"},{"name":"noDisplayUpdates","description":"Whether updates should not be committed and drawn onto the display. False by default. If\ntrue, only side effects of the BeginFrame will be run, such as layout and animations, but\nany visual updates may not be visible on the display or in screenshots.","optional":true,"type":"boolean"},{"name":"screenshot","description":"If set, a screenshot of the frame will be captured and returned in the response. Otherwise,\nno screenshot will be captured. Note that capturing a screenshot can fail, for example,\nduring renderer initialization. In such a case, no screenshot data will be returned.","optional":true,"$ref":"ScreenshotParams"}],"returns":[{"name":"hasDamage","description":"Whether the BeginFrame resulted in damage and, thus, a new frame was committed to the\ndisplay. Reported for diagnostic uses, may be removed in the future.","type":"boolean"},{"name":"screenshotData","description":"Base64-encoded image data of the screenshot, if one was requested and successfully taken.","optional":true,"type":"string"}]},{"name":"disable","description":"Disables headless events for the target."},{"name":"enable","description":"Enables headless events for the target."}],"events":[{"name":"needsBeginFramesChanged","description":"Issued when the target starts or stops needing BeginFrames.","parameters":[{"name":"needsBeginFrames","description":"True if BeginFrames are needed, false otherwise.","type":"boolean"}]}]},{"domain":"IO","description":"Input/Output operations for streams produced by DevTools.","types":[{"id":"StreamHandle","description":"This is either obtained from another method or specifed as `blob:&lt;uuid&gt;` where\n`&lt;uuid&gt` is an UUID of a Blob.","type":"string"}],"commands":[{"name":"close","description":"Close the stream, discard any temporary backing storage.","parameters":[{"name":"handle","description":"Handle of the stream to close.","$ref":"StreamHandle"}]},{"name":"read","description":"Read a chunk of the stream","parameters":[{"name":"handle","description":"Handle of the stream to read.","$ref":"StreamHandle"},{"name":"offset","description":"Seek to the specified offset before reading (if not specificed, proceed with offset\nfollowing the last read). Some types of streams may only support sequential reads.","optional":true,"type":"integer"},{"name":"size","description":"Maximum number of bytes to read (left upon the agent discretion if not specified).","optional":true,"type":"integer"}],"returns":[{"name":"base64Encoded","description":"Set if the data is base64-encoded","optional":true,"type":"boolean"},{"name":"data","description":"Data that were read.","type":"string"},{"name":"eof","description":"Set if the end-of-file condition occured while reading.","type":"boolean"}]},{"name":"resolveBlob","description":"Return UUID of Blob object specified by a remote object id.","parameters":[{"name":"objectId","description":"Object id of a Blob object wrapper.","$ref":"Runtime.RemoteObjectId"}],"returns":[{"name":"uuid","description":"UUID of the specified Blob.","type":"string"}]}]},{"domain":"IndexedDB","experimental":true,"dependencies":["Runtime"],"types":[{"id":"DatabaseWithObjectStores","description":"Database with an array of object stores.","type":"object","properties":[{"name":"name","description":"Database name.","type":"string"},{"name":"version","description":"Database version.","type":"integer"},{"name":"objectStores","description":"Object stores in this database.","type":"array","items":{"$ref":"ObjectStore"}}]},{"id":"ObjectStore","description":"Object store.","type":"object","properties":[{"name":"name","description":"Object store name.","type":"string"},{"name":"keyPath","description":"Object store key path.","$ref":"KeyPath"},{"name":"autoIncrement","description":"If true, object store has auto increment flag set.","type":"boolean"},{"name":"indexes","description":"Indexes in this object store.","type":"array","items":{"$ref":"ObjectStoreIndex"}}]},{"id":"ObjectStoreIndex","description":"Object store index.","type":"object","properties":[{"name":"name","description":"Index name.","type":"string"},{"name":"keyPath","description":"Index key path.","$ref":"KeyPath"},{"name":"unique","description":"If true, index is unique.","type":"boolean"},{"name":"multiEntry","description":"If true, index allows multiple entries for a key.","type":"boolean"}]},{"id":"Key","description":"Key.","type":"object","properties":[{"name":"type","description":"Key type.","type":"string","enum":["number","string","date","array"]},{"name":"number","description":"Number value.","optional":true,"type":"number"},{"name":"string","description":"String value.","optional":true,"type":"string"},{"name":"date","description":"Date value.","optional":true,"type":"number"},{"name":"array","description":"Array value.","optional":true,"type":"array","items":{"$ref":"Key"}}]},{"id":"KeyRange","description":"Key range.","type":"object","properties":[{"name":"lower","description":"Lower bound.","optional":true,"$ref":"Key"},{"name":"upper","description":"Upper bound.","optional":true,"$ref":"Key"},{"name":"lowerOpen","description":"If true lower bound is open.","type":"boolean"},{"name":"upperOpen","description":"If true upper bound is open.","type":"boolean"}]},{"id":"DataEntry","description":"Data entry.","type":"object","properties":[{"name":"key","description":"Key object.","$ref":"Runtime.RemoteObject"},{"name":"primaryKey","description":"Primary key object.","$ref":"Runtime.RemoteObject"},{"name":"value","description":"Value object.","$ref":"Runtime.RemoteObject"}]},{"id":"KeyPath","description":"Key path.","type":"object","properties":[{"name":"type","description":"Key path type.","type":"string","enum":["null","string","array"]},{"name":"string","description":"String value.","optional":true,"type":"string"},{"name":"array","description":"Array value.","optional":true,"type":"array","items":{"type":"string"}}]}],"commands":[{"name":"clearObjectStore","description":"Clears all entries from an object store.","parameters":[{"name":"securityOrigin","description":"Security origin.","type":"string"},{"name":"databaseName","description":"Database name.","type":"string"},{"name":"objectStoreName","description":"Object store name.","type":"string"}]},{"name":"deleteDatabase","description":"Deletes a database.","parameters":[{"name":"securityOrigin","description":"Security origin.","type":"string"},{"name":"databaseName","description":"Database name.","type":"string"}]},{"name":"deleteObjectStoreEntries","description":"Delete a range of entries from an object store","parameters":[{"name":"securityOrigin","type":"string"},{"name":"databaseName","type":"string"},{"name":"objectStoreName","type":"string"},{"name":"keyRange","description":"Range of entry keys to delete","$ref":"KeyRange"}]},{"name":"disable","description":"Disables events from backend."},{"name":"enable","description":"Enables events from backend."},{"name":"requestData","description":"Requests data from object store or index.","parameters":[{"name":"securityOrigin","description":"Security origin.","type":"string"},{"name":"databaseName","description":"Database name.","type":"string"},{"name":"objectStoreName","description":"Object store name.","type":"string"},{"name":"indexName","description":"Index name, empty string for object store data requests.","type":"string"},{"name":"skipCount","description":"Number of records to skip.","type":"integer"},{"name":"pageSize","description":"Number of records to fetch.","type":"integer"},{"name":"keyRange","description":"Key range.","optional":true,"$ref":"KeyRange"}],"returns":[{"name":"objectStoreDataEntries","description":"Array of object store data entries.","type":"array","items":{"$ref":"DataEntry"}},{"name":"hasMore","description":"If true, there are more entries to fetch in the given range.","type":"boolean"}]},{"name":"requestDatabase","description":"Requests database with given name in given frame.","parameters":[{"name":"securityOrigin","description":"Security origin.","type":"string"},{"name":"databaseName","description":"Database name.","type":"string"}],"returns":[{"name":"databaseWithObjectStores","description":"Database with an array of object stores.","$ref":"DatabaseWithObjectStores"}]},{"name":"requestDatabaseNames","description":"Requests database names for given security origin.","parameters":[{"name":"securityOrigin","description":"Security origin.","type":"string"}],"returns":[{"name":"databaseNames","description":"Database names for origin.","type":"array","items":{"type":"string"}}]}]},{"domain":"Input","types":[{"id":"TouchPoint","type":"object","properties":[{"name":"x","description":"X coordinate of the event relative to the main frame's viewport in CSS pixels.","type":"number"},{"name":"y","description":"Y coordinate of the event relative to the main frame's viewport in CSS pixels. 0 refers to\nthe top of the viewport and Y increases as it proceeds towards the bottom of the viewport.","type":"number"},{"name":"radiusX","description":"X radius of the touch area (default: 1.0).","optional":true,"type":"number"},{"name":"radiusY","description":"Y radius of the touch area (default: 1.0).","optional":true,"type":"number"},{"name":"rotationAngle","description":"Rotation angle (default: 0.0).","optional":true,"type":"number"},{"name":"force","description":"Force (default: 1.0).","optional":true,"type":"number"},{"name":"id","description":"Identifier used to track touch sources between events, must be unique within an event.","optional":true,"type":"number"}]},{"id":"GestureSourceType","experimental":true,"type":"string","enum":["default","touch","mouse"]},{"id":"TimeSinceEpoch","description":"UTC time in seconds, counted from January 1, 1970.","type":"number"}],"commands":[{"name":"dispatchKeyEvent","description":"Dispatches a key event to the page.","parameters":[{"name":"type","description":"Type of the key event.","type":"string","enum":["keyDown","keyUp","rawKeyDown","char"]},{"name":"modifiers","description":"Bit field representing pressed modifier keys. Alt=1, Ctrl=2, Meta/Command=4, Shift=8\n(default: 0).","optional":true,"type":"integer"},{"name":"timestamp","description":"Time at which the event occurred.","optional":true,"$ref":"TimeSinceEpoch"},{"name":"text","description":"Text as generated by processing a virtual key code with a keyboard layout. Not needed for\nfor `keyUp` and `rawKeyDown` events (default: \"\")","optional":true,"type":"string"},{"name":"unmodifiedText","description":"Text that would have been generated by the keyboard if no modifiers were pressed (except for\nshift). Useful for shortcut (accelerator) key handling (default: \"\").","optional":true,"type":"string"},{"name":"keyIdentifier","description":"Unique key identifier (e.g., 'U+0041') (default: \"\").","optional":true,"type":"string"},{"name":"code","description":"Unique DOM defined string value for each physical key (e.g., 'KeyA') (default: \"\").","optional":true,"type":"string"},{"name":"key","description":"Unique DOM defined string value describing the meaning of the key in the context of active\nmodifiers, keyboard layout, etc (e.g., 'AltGr') (default: \"\").","optional":true,"type":"string"},{"name":"windowsVirtualKeyCode","description":"Windows virtual key code (default: 0).","optional":true,"type":"integer"},{"name":"nativeVirtualKeyCode","description":"Native virtual key code (default: 0).","optional":true,"type":"integer"},{"name":"autoRepeat","description":"Whether the event was generated from auto repeat (default: false).","optional":true,"type":"boolean"},{"name":"isKeypad","description":"Whether the event was generated from the keypad (default: false).","optional":true,"type":"boolean"},{"name":"isSystemKey","description":"Whether the event was a system key event (default: false).","optional":true,"type":"boolean"},{"name":"location","description":"Whether the event was from the left or right side of the keyboard. 1=Left, 2=Right (default:\n0).","optional":true,"type":"integer"}]},{"name":"insertText","description":"This method emulates inserting text that doesn't come from a key press,\nfor example an emoji keyboard or an IME.","experimental":true,"parameters":[{"name":"text","description":"The text to insert.","type":"string"}]},{"name":"dispatchMouseEvent","description":"Dispatches a mouse event to the page.","parameters":[{"name":"type","description":"Type of the mouse event.","type":"string","enum":["mousePressed","mouseReleased","mouseMoved","mouseWheel"]},{"name":"x","description":"X coordinate of the event relative to the main frame's viewport in CSS pixels.","type":"number"},{"name":"y","description":"Y coordinate of the event relative to the main frame's viewport in CSS pixels. 0 refers to\nthe top of the viewport and Y increases as it proceeds towards the bottom of the viewport.","type":"number"},{"name":"modifiers","description":"Bit field representing pressed modifier keys. Alt=1, Ctrl=2, Meta/Command=4, Shift=8\n(default: 0).","optional":true,"type":"integer"},{"name":"timestamp","description":"Time at which the event occurred.","optional":true,"$ref":"TimeSinceEpoch"},{"name":"button","description":"Mouse button (default: \"none\").","optional":true,"type":"string","enum":["none","left","middle","right"]},{"name":"clickCount","description":"Number of times the mouse button was clicked (default: 0).","optional":true,"type":"integer"},{"name":"deltaX","description":"X delta in CSS pixels for mouse wheel event (default: 0).","optional":true,"type":"number"},{"name":"deltaY","description":"Y delta in CSS pixels for mouse wheel event (default: 0).","optional":true,"type":"number"}]},{"name":"dispatchTouchEvent","description":"Dispatches a touch event to the page.","parameters":[{"name":"type","description":"Type of the touch event. TouchEnd and TouchCancel must not contain any touch points, while\nTouchStart and TouchMove must contains at least one.","type":"string","enum":["touchStart","touchEnd","touchMove","touchCancel"]},{"name":"touchPoints","description":"Active touch points on the touch device. One event per any changed point (compared to\nprevious touch event in a sequence) is generated, emulating pressing/moving/releasing points\none by one.","type":"array","items":{"$ref":"TouchPoint"}},{"name":"modifiers","description":"Bit field representing pressed modifier keys. Alt=1, Ctrl=2, Meta/Command=4, Shift=8\n(default: 0).","optional":true,"type":"integer"},{"name":"timestamp","description":"Time at which the event occurred.","optional":true,"$ref":"TimeSinceEpoch"}]},{"name":"emulateTouchFromMouseEvent","description":"Emulates touch event from the mouse event parameters.","experimental":true,"parameters":[{"name":"type","description":"Type of the mouse event.","type":"string","enum":["mousePressed","mouseReleased","mouseMoved","mouseWheel"]},{"name":"x","description":"X coordinate of the mouse pointer in DIP.","type":"integer"},{"name":"y","description":"Y coordinate of the mouse pointer in DIP.","type":"integer"},{"name":"button","description":"Mouse button.","type":"string","enum":["none","left","middle","right"]},{"name":"timestamp","description":"Time at which the event occurred (default: current time).","optional":true,"$ref":"TimeSinceEpoch"},{"name":"deltaX","description":"X delta in DIP for mouse wheel event (default: 0).","optional":true,"type":"number"},{"name":"deltaY","description":"Y delta in DIP for mouse wheel event (default: 0).","optional":true,"type":"number"},{"name":"modifiers","description":"Bit field representing pressed modifier keys. Alt=1, Ctrl=2, Meta/Command=4, Shift=8\n(default: 0).","optional":true,"type":"integer"},{"name":"clickCount","description":"Number of times the mouse button was clicked (default: 0).","optional":true,"type":"integer"}]},{"name":"setIgnoreInputEvents","description":"Ignores input events (useful while auditing page).","parameters":[{"name":"ignore","description":"Ignores input events processing when set to true.","type":"boolean"}]},{"name":"synthesizePinchGesture","description":"Synthesizes a pinch gesture over a time period by issuing appropriate touch events.","experimental":true,"parameters":[{"name":"x","description":"X coordinate of the start of the gesture in CSS pixels.","type":"number"},{"name":"y","description":"Y coordinate of the start of the gesture in CSS pixels.","type":"number"},{"name":"scaleFactor","description":"Relative scale factor after zooming (>1.0 zooms in, <1.0 zooms out).","type":"number"},{"name":"relativeSpeed","description":"Relative pointer speed in pixels per second (default: 800).","optional":true,"type":"integer"},{"name":"gestureSourceType","description":"Which type of input events to be generated (default: 'default', which queries the platform\nfor the preferred input type).","optional":true,"$ref":"GestureSourceType"}]},{"name":"synthesizeScrollGesture","description":"Synthesizes a scroll gesture over a time period by issuing appropriate touch events.","experimental":true,"parameters":[{"name":"x","description":"X coordinate of the start of the gesture in CSS pixels.","type":"number"},{"name":"y","description":"Y coordinate of the start of the gesture in CSS pixels.","type":"number"},{"name":"xDistance","description":"The distance to scroll along the X axis (positive to scroll left).","optional":true,"type":"number"},{"name":"yDistance","description":"The distance to scroll along the Y axis (positive to scroll up).","optional":true,"type":"number"},{"name":"xOverscroll","description":"The number of additional pixels to scroll back along the X axis, in addition to the given\ndistance.","optional":true,"type":"number"},{"name":"yOverscroll","description":"The number of additional pixels to scroll back along the Y axis, in addition to the given\ndistance.","optional":true,"type":"number"},{"name":"preventFling","description":"Prevent fling (default: true).","optional":true,"type":"boolean"},{"name":"speed","description":"Swipe speed in pixels per second (default: 800).","optional":true,"type":"integer"},{"name":"gestureSourceType","description":"Which type of input events to be generated (default: 'default', which queries the platform\nfor the preferred input type).","optional":true,"$ref":"GestureSourceType"},{"name":"repeatCount","description":"The number of times to repeat the gesture (default: 0).","optional":true,"type":"integer"},{"name":"repeatDelayMs","description":"The number of milliseconds delay between each repeat. (default: 250).","optional":true,"type":"integer"},{"name":"interactionMarkerName","description":"The name of the interaction markers to generate, if not empty (default: \"\").","optional":true,"type":"string"}]},{"name":"synthesizeTapGesture","description":"Synthesizes a tap gesture over a time period by issuing appropriate touch events.","experimental":true,"parameters":[{"name":"x","description":"X coordinate of the start of the gesture in CSS pixels.","type":"number"},{"name":"y","description":"Y coordinate of the start of the gesture in CSS pixels.","type":"number"},{"name":"duration","description":"Duration between touchdown and touchup events in ms (default: 50).","optional":true,"type":"integer"},{"name":"tapCount","description":"Number of times to perform the tap (e.g. 2 for double tap, default: 1).","optional":true,"type":"integer"},{"name":"gestureSourceType","description":"Which type of input events to be generated (default: 'default', which queries the platform\nfor the preferred input type).","optional":true,"$ref":"GestureSourceType"}]}]},{"domain":"Inspector","experimental":true,"commands":[{"name":"disable","description":"Disables inspector domain notifications."},{"name":"enable","description":"Enables inspector domain notifications."}],"events":[{"name":"detached","description":"Fired when remote debugging connection is about to be terminated. Contains detach reason.","parameters":[{"name":"reason","description":"The reason why connection has been terminated.","type":"string"}]},{"name":"targetCrashed","description":"Fired when debugging target has crashed"},{"name":"targetReloadedAfterCrash","description":"Fired when debugging target has reloaded after crash"}]},{"domain":"LayerTree","experimental":true,"dependencies":["DOM"],"types":[{"id":"LayerId","description":"Unique Layer identifier.","type":"string"},{"id":"SnapshotId","description":"Unique snapshot identifier.","type":"string"},{"id":"ScrollRect","description":"Rectangle where scrolling happens on the main thread.","type":"object","properties":[{"name":"rect","description":"Rectangle itself.","$ref":"DOM.Rect"},{"name":"type","description":"Reason for rectangle to force scrolling on the main thread","type":"string","enum":["RepaintsOnScroll","TouchEventHandler","WheelEventHandler"]}]},{"id":"StickyPositionConstraint","description":"Sticky position constraints.","type":"object","properties":[{"name":"stickyBoxRect","description":"Layout rectangle of the sticky element before being shifted","$ref":"DOM.Rect"},{"name":"containingBlockRect","description":"Layout rectangle of the containing block of the sticky element","$ref":"DOM.Rect"},{"name":"nearestLayerShiftingStickyBox","description":"The nearest sticky layer that shifts the sticky box","optional":true,"$ref":"LayerId"},{"name":"nearestLayerShiftingContainingBlock","description":"The nearest sticky layer that shifts the containing block","optional":true,"$ref":"LayerId"}]},{"id":"PictureTile","description":"Serialized fragment of layer picture along with its offset within the layer.","type":"object","properties":[{"name":"x","description":"Offset from owning layer left boundary","type":"number"},{"name":"y","description":"Offset from owning layer top boundary","type":"number"},{"name":"picture","description":"Base64-encoded snapshot data.","type":"string"}]},{"id":"Layer","description":"Information about a compositing layer.","type":"object","properties":[{"name":"layerId","description":"The unique id for this layer.","$ref":"LayerId"},{"name":"parentLayerId","description":"The id of parent (not present for root).","optional":true,"$ref":"LayerId"},{"name":"backendNodeId","description":"The backend id for the node associated with this layer.","optional":true,"$ref":"DOM.BackendNodeId"},{"name":"offsetX","description":"Offset from parent layer, X coordinate.","type":"number"},{"name":"offsetY","description":"Offset from parent layer, Y coordinate.","type":"number"},{"name":"width","description":"Layer width.","type":"number"},{"name":"height","description":"Layer height.","type":"number"},{"name":"transform","description":"Transformation matrix for layer, default is identity matrix","optional":true,"type":"array","items":{"type":"number"}},{"name":"anchorX","description":"Transform anchor point X, absent if no transform specified","optional":true,"type":"number"},{"name":"anchorY","description":"Transform anchor point Y, absent if no transform specified","optional":true,"type":"number"},{"name":"anchorZ","description":"Transform anchor point Z, absent if no transform specified","optional":true,"type":"number"},{"name":"paintCount","description":"Indicates how many time this layer has painted.","type":"integer"},{"name":"drawsContent","description":"Indicates whether this layer hosts any content, rather than being used for\ntransform/scrolling purposes only.","type":"boolean"},{"name":"invisible","description":"Set if layer is not visible.","optional":true,"type":"boolean"},{"name":"scrollRects","description":"Rectangles scrolling on main thread only.","optional":true,"type":"array","items":{"$ref":"ScrollRect"}},{"name":"stickyPositionConstraint","description":"Sticky position constraint information","optional":true,"$ref":"StickyPositionConstraint"}]},{"id":"PaintProfile","description":"Array of timings, one per paint step.","type":"array","items":{"type":"number"}}],"commands":[{"name":"compositingReasons","description":"Provides the reasons why the given layer was composited.","parameters":[{"name":"layerId","description":"The id of the layer for which we want to get the reasons it was composited.","$ref":"LayerId"}],"returns":[{"name":"compositingReasons","description":"A list of strings specifying reasons for the given layer to become composited.","type":"array","items":{"type":"string"}}]},{"name":"disable","description":"Disables compositing tree inspection."},{"name":"enable","description":"Enables compositing tree inspection."},{"name":"loadSnapshot","description":"Returns the snapshot identifier.","parameters":[{"name":"tiles","description":"An array of tiles composing the snapshot.","type":"array","items":{"$ref":"PictureTile"}}],"returns":[{"name":"snapshotId","description":"The id of the snapshot.","$ref":"SnapshotId"}]},{"name":"makeSnapshot","description":"Returns the layer snapshot identifier.","parameters":[{"name":"layerId","description":"The id of the layer.","$ref":"LayerId"}],"returns":[{"name":"snapshotId","description":"The id of the layer snapshot.","$ref":"SnapshotId"}]},{"name":"profileSnapshot","parameters":[{"name":"snapshotId","description":"The id of the layer snapshot.","$ref":"SnapshotId"},{"name":"minRepeatCount","description":"The maximum number of times to replay the snapshot (1, if not specified).","optional":true,"type":"integer"},{"name":"minDuration","description":"The minimum duration (in seconds) to replay the snapshot.","optional":true,"type":"number"},{"name":"clipRect","description":"The clip rectangle to apply when replaying the snapshot.","optional":true,"$ref":"DOM.Rect"}],"returns":[{"name":"timings","description":"The array of paint profiles, one per run.","type":"array","items":{"$ref":"PaintProfile"}}]},{"name":"releaseSnapshot","description":"Releases layer snapshot captured by the back-end.","parameters":[{"name":"snapshotId","description":"The id of the layer snapshot.","$ref":"SnapshotId"}]},{"name":"replaySnapshot","description":"Replays the layer snapshot and returns the resulting bitmap.","parameters":[{"name":"snapshotId","description":"The id of the layer snapshot.","$ref":"SnapshotId"},{"name":"fromStep","description":"The first step to replay from (replay from the very start if not specified).","optional":true,"type":"integer"},{"name":"toStep","description":"The last step to replay to (replay till the end if not specified).","optional":true,"type":"integer"},{"name":"scale","description":"The scale to apply while replaying (defaults to 1).","optional":true,"type":"number"}],"returns":[{"name":"dataURL","description":"A data: URL for resulting image.","type":"string"}]},{"name":"snapshotCommandLog","description":"Replays the layer snapshot and returns canvas log.","parameters":[{"name":"snapshotId","description":"The id of the layer snapshot.","$ref":"SnapshotId"}],"returns":[{"name":"commandLog","description":"The array of canvas function calls.","type":"array","items":{"type":"object"}}]}],"events":[{"name":"layerPainted","parameters":[{"name":"layerId","description":"The id of the painted layer.","$ref":"LayerId"},{"name":"clip","description":"Clip rectangle.","$ref":"DOM.Rect"}]},{"name":"layerTreeDidChange","parameters":[{"name":"layers","description":"Layer tree, absent if not in the comspositing mode.","optional":true,"type":"array","items":{"$ref":"Layer"}}]}]},{"domain":"Log","description":"Provides access to log entries.","dependencies":["Runtime","Network"],"types":[{"id":"LogEntry","description":"Log entry.","type":"object","properties":[{"name":"source","description":"Log entry source.","type":"string","enum":["xml","javascript","network","storage","appcache","rendering","security","deprecation","worker","violation","intervention","recommendation","other"]},{"name":"level","description":"Log entry severity.","type":"string","enum":["verbose","info","warning","error"]},{"name":"text","description":"Logged text.","type":"string"},{"name":"timestamp","description":"Timestamp when this entry was added.","$ref":"Runtime.Timestamp"},{"name":"url","description":"URL of the resource if known.","optional":true,"type":"string"},{"name":"lineNumber","description":"Line number in the resource.","optional":true,"type":"integer"},{"name":"stackTrace","description":"JavaScript stack trace.","optional":true,"$ref":"Runtime.StackTrace"},{"name":"networkRequestId","description":"Identifier of the network request associated with this entry.","optional":true,"$ref":"Network.RequestId"},{"name":"workerId","description":"Identifier of the worker associated with this entry.","optional":true,"type":"string"},{"name":"args","description":"Call arguments.","optional":true,"type":"array","items":{"$ref":"Runtime.RemoteObject"}}]},{"id":"ViolationSetting","description":"Violation configuration setting.","type":"object","properties":[{"name":"name","description":"Violation type.","type":"string","enum":["longTask","longLayout","blockedEvent","blockedParser","discouragedAPIUse","handler","recurringHandler"]},{"name":"threshold","description":"Time threshold to trigger upon.","type":"number"}]}],"commands":[{"name":"clear","description":"Clears the log."},{"name":"disable","description":"Disables log domain, prevents further log entries from being reported to the client."},{"name":"enable","description":"Enables log domain, sends the entries collected so far to the client by means of the\n`entryAdded` notification."},{"name":"startViolationsReport","description":"start violation reporting.","parameters":[{"name":"config","description":"Configuration for violations.","type":"array","items":{"$ref":"ViolationSetting"}}]},{"name":"stopViolationsReport","description":"Stop violation reporting."}],"events":[{"name":"entryAdded","description":"Issued when new message was logged.","parameters":[{"name":"entry","description":"The entry.","$ref":"LogEntry"}]}]},{"domain":"Memory","experimental":true,"types":[{"id":"PressureLevel","description":"Memory pressure level.","type":"string","enum":["moderate","critical"]},{"id":"SamplingProfileNode","description":"Heap profile sample.","type":"object","properties":[{"name":"size","description":"Size of the sampled allocation.","type":"number"},{"name":"total","description":"Total bytes attributed to this sample.","type":"number"},{"name":"stack","description":"Execution stack at the point of allocation.","type":"array","items":{"type":"string"}}]},{"id":"SamplingProfile","description":"Array of heap profile samples.","type":"object","properties":[{"name":"samples","type":"array","items":{"$ref":"SamplingProfileNode"}},{"name":"modules","type":"array","items":{"$ref":"Module"}}]},{"id":"Module","description":"Executable module information","type":"object","properties":[{"name":"name","description":"Name of the module.","type":"string"},{"name":"uuid","description":"UUID of the module.","type":"string"},{"name":"baseAddress","description":"Base address where the module is loaded into memory. Encoded as a decimal\nor hexadecimal (0x prefixed) string.","type":"string"},{"name":"size","description":"Size of the module in bytes.","type":"number"}]}],"commands":[{"name":"getDOMCounters","returns":[{"name":"documents","type":"integer"},{"name":"nodes","type":"integer"},{"name":"jsEventListeners","type":"integer"}]},{"name":"prepareForLeakDetection"},{"name":"setPressureNotificationsSuppressed","description":"Enable/disable suppressing memory pressure notifications in all processes.","parameters":[{"name":"suppressed","description":"If true, memory pressure notifications will be suppressed.","type":"boolean"}]},{"name":"simulatePressureNotification","description":"Simulate a memory pressure notification in all processes.","parameters":[{"name":"level","description":"Memory pressure level of the notification.","$ref":"PressureLevel"}]},{"name":"startSampling","description":"Start collecting native memory profile.","parameters":[{"name":"samplingInterval","description":"Average number of bytes between samples.","optional":true,"type":"integer"},{"name":"suppressRandomness","description":"Do not randomize intervals between samples.","optional":true,"type":"boolean"}]},{"name":"stopSampling","description":"Stop collecting native memory profile."},{"name":"getAllTimeSamplingProfile","description":"Retrieve native memory allocations profile\ncollected since renderer process startup.","returns":[{"name":"profile","$ref":"SamplingProfile"}]},{"name":"getBrowserSamplingProfile","description":"Retrieve native memory allocations profile\ncollected since browser process startup.","returns":[{"name":"profile","$ref":"SamplingProfile"}]},{"name":"getSamplingProfile","description":"Retrieve native memory allocations profile collected since last\n`startSampling` call.","returns":[{"name":"profile","$ref":"SamplingProfile"}]}]},{"domain":"Network","description":"Network domain allows tracking network activities of the page. It exposes information about http,\nfile, data and other requests and responses, their headers, bodies, timing, etc.","dependencies":["Debugger","Runtime","Security"],"types":[{"id":"ResourceType","description":"Resource type as it was perceived by the rendering engine.","type":"string","enum":["Document","Stylesheet","Image","Media","Font","Script","TextTrack","XHR","Fetch","EventSource","WebSocket","Manifest","SignedExchange","Ping","CSPViolationReport","Other"]},{"id":"LoaderId","description":"Unique loader identifier.","type":"string"},{"id":"RequestId","description":"Unique request identifier.","type":"string"},{"id":"InterceptionId","description":"Unique intercepted request identifier.","type":"string"},{"id":"ErrorReason","description":"Network level fetch failure reason.","type":"string","enum":["Failed","Aborted","TimedOut","AccessDenied","ConnectionClosed","ConnectionReset","ConnectionRefused","ConnectionAborted","ConnectionFailed","NameNotResolved","InternetDisconnected","AddressUnreachable","BlockedByClient","BlockedByResponse"]},{"id":"TimeSinceEpoch","description":"UTC time in seconds, counted from January 1, 1970.","type":"number"},{"id":"MonotonicTime","description":"Monotonically increasing time in seconds since an arbitrary point in the past.","type":"number"},{"id":"Headers","description":"Request / response headers as keys / values of JSON object.","type":"object"},{"id":"ConnectionType","description":"The underlying connection technology that the browser is supposedly using.","type":"string","enum":["none","cellular2g","cellular3g","cellular4g","bluetooth","ethernet","wifi","wimax","other"]},{"id":"CookieSameSite","description":"Represents the cookie's 'SameSite' status:\nhttps://tools.ietf.org/html/draft-west-first-party-cookies","type":"string","enum":["Strict","Lax"]},{"id":"ResourceTiming","description":"Timing information for the request.","type":"object","properties":[{"name":"requestTime","description":"Timing's requestTime is a baseline in seconds, while the other numbers are ticks in\nmilliseconds relatively to this requestTime.","type":"number"},{"name":"proxyStart","description":"Started resolving proxy.","type":"number"},{"name":"proxyEnd","description":"Finished resolving proxy.","type":"number"},{"name":"dnsStart","description":"Started DNS address resolve.","type":"number"},{"name":"dnsEnd","description":"Finished DNS address resolve.","type":"number"},{"name":"connectStart","description":"Started connecting to the remote host.","type":"number"},{"name":"connectEnd","description":"Connected to the remote host.","type":"number"},{"name":"sslStart","description":"Started SSL handshake.","type":"number"},{"name":"sslEnd","description":"Finished SSL handshake.","type":"number"},{"name":"workerStart","description":"Started running ServiceWorker.","experimental":true,"type":"number"},{"name":"workerReady","description":"Finished Starting ServiceWorker.","experimental":true,"type":"number"},{"name":"sendStart","description":"Started sending request.","type":"number"},{"name":"sendEnd","description":"Finished sending request.","type":"number"},{"name":"pushStart","description":"Time the server started pushing request.","experimental":true,"type":"number"},{"name":"pushEnd","description":"Time the server finished pushing request.","experimental":true,"type":"number"},{"name":"receiveHeadersEnd","description":"Finished receiving response headers.","type":"number"}]},{"id":"ResourcePriority","description":"Loading priority of a resource request.","type":"string","enum":["VeryLow","Low","Medium","High","VeryHigh"]},{"id":"Request","description":"HTTP request data.","type":"object","properties":[{"name":"url","description":"Request URL (without fragment).","type":"string"},{"name":"urlFragment","description":"Fragment of the requested URL starting with hash, if present.","optional":true,"type":"string"},{"name":"method","description":"HTTP request method.","type":"string"},{"name":"headers","description":"HTTP request headers.","$ref":"Headers"},{"name":"postData","description":"HTTP POST request data.","optional":true,"type":"string"},{"name":"hasPostData","description":"True when the request has POST data. Note that postData might still be omitted when this flag is true when the data is too long.","optional":true,"type":"boolean"},{"name":"mixedContentType","description":"The mixed content type of the request.","optional":true,"$ref":"Security.MixedContentType"},{"name":"initialPriority","description":"Priority of the resource request at the time request is sent.","$ref":"ResourcePriority"},{"name":"referrerPolicy","description":"The referrer policy of the request, as defined in https://www.w3.org/TR/referrer-policy/","type":"string","enum":["unsafe-url","no-referrer-when-downgrade","no-referrer","origin","origin-when-cross-origin","same-origin","strict-origin","strict-origin-when-cross-origin"]},{"name":"isLinkPreload","description":"Whether is loaded via link preload.","optional":true,"type":"boolean"}]},{"id":"SignedCertificateTimestamp","description":"Details of a signed certificate timestamp (SCT).","type":"object","properties":[{"name":"status","description":"Validation status.","type":"string"},{"name":"origin","description":"Origin.","type":"string"},{"name":"logDescription","description":"Log name / description.","type":"string"},{"name":"logId","description":"Log ID.","type":"string"},{"name":"timestamp","description":"Issuance date.","$ref":"TimeSinceEpoch"},{"name":"hashAlgorithm","description":"Hash algorithm.","type":"string"},{"name":"signatureAlgorithm","description":"Signature algorithm.","type":"string"},{"name":"signatureData","description":"Signature data.","type":"string"}]},{"id":"SecurityDetails","description":"Security details about a request.","type":"object","properties":[{"name":"protocol","description":"Protocol name (e.g. \"TLS 1.2\" or \"QUIC\").","type":"string"},{"name":"keyExchange","description":"Key Exchange used by the connection, or the empty string if not applicable.","type":"string"},{"name":"keyExchangeGroup","description":"(EC)DH group used by the connection, if applicable.","optional":true,"type":"string"},{"name":"cipher","description":"Cipher name.","type":"string"},{"name":"mac","description":"TLS MAC. Note that AEAD ciphers do not have separate MACs.","optional":true,"type":"string"},{"name":"certificateId","description":"Certificate ID value.","$ref":"Security.CertificateId"},{"name":"subjectName","description":"Certificate subject name.","type":"string"},{"name":"sanList","description":"Subject Alternative Name (SAN) DNS names and IP addresses.","type":"array","items":{"type":"string"}},{"name":"issuer","description":"Name of the issuing CA.","type":"string"},{"name":"validFrom","description":"Certificate valid from date.","$ref":"TimeSinceEpoch"},{"name":"validTo","description":"Certificate valid to (expiration) date","$ref":"TimeSinceEpoch"},{"name":"signedCertificateTimestampList","description":"List of signed certificate timestamps (SCTs).","type":"array","items":{"$ref":"SignedCertificateTimestamp"}},{"name":"certificateTransparencyCompliance","description":"Whether the request complied with Certificate Transparency policy","$ref":"CertificateTransparencyCompliance"}]},{"id":"CertificateTransparencyCompliance","description":"Whether the request complied with Certificate Transparency policy.","type":"string","enum":["unknown","not-compliant","compliant"]},{"id":"BlockedReason","description":"The reason why request was blocked.","type":"string","enum":["other","csp","mixed-content","origin","inspector","subresource-filter","content-type","collapsed-by-client"]},{"id":"Response","description":"HTTP response data.","type":"object","properties":[{"name":"url","description":"Response URL. This URL can be different from CachedResource.url in case of redirect.","type":"string"},{"name":"status","description":"HTTP response status code.","type":"integer"},{"name":"statusText","description":"HTTP response status text.","type":"string"},{"name":"headers","description":"HTTP response headers.","$ref":"Headers"},{"name":"headersText","description":"HTTP response headers text.","optional":true,"type":"string"},{"name":"mimeType","description":"Resource mimeType as determined by the browser.","type":"string"},{"name":"requestHeaders","description":"Refined HTTP request headers that were actually transmitted over the network.","optional":true,"$ref":"Headers"},{"name":"requestHeadersText","description":"HTTP request headers text.","optional":true,"type":"string"},{"name":"connectionReused","description":"Specifies whether physical connection was actually reused for this request.","type":"boolean"},{"name":"connectionId","description":"Physical connection id that was actually used for this request.","type":"number"},{"name":"remoteIPAddress","description":"Remote IP address.","optional":true,"type":"string"},{"name":"remotePort","description":"Remote port.","optional":true,"type":"integer"},{"name":"fromDiskCache","description":"Specifies that the request was served from the disk cache.","optional":true,"type":"boolean"},{"name":"fromServiceWorker","description":"Specifies that the request was served from the ServiceWorker.","optional":true,"type":"boolean"},{"name":"encodedDataLength","description":"Total number of bytes received for this request so far.","type":"number"},{"name":"timing","description":"Timing information for the given request.","optional":true,"$ref":"ResourceTiming"},{"name":"protocol","description":"Protocol used to fetch this request.","optional":true,"type":"string"},{"name":"securityState","description":"Security state of the request resource.","$ref":"Security.SecurityState"},{"name":"securityDetails","description":"Security details for the request.","optional":true,"$ref":"SecurityDetails"}]},{"id":"WebSocketRequest","description":"WebSocket request data.","type":"object","properties":[{"name":"headers","description":"HTTP request headers.","$ref":"Headers"}]},{"id":"WebSocketResponse","description":"WebSocket response data.","type":"object","properties":[{"name":"status","description":"HTTP response status code.","type":"integer"},{"name":"statusText","description":"HTTP response status text.","type":"string"},{"name":"headers","description":"HTTP response headers.","$ref":"Headers"},{"name":"headersText","description":"HTTP response headers text.","optional":true,"type":"string"},{"name":"requestHeaders","description":"HTTP request headers.","optional":true,"$ref":"Headers"},{"name":"requestHeadersText","description":"HTTP request headers text.","optional":true,"type":"string"}]},{"id":"WebSocketFrame","description":"WebSocket frame data.","type":"object","properties":[{"name":"opcode","description":"WebSocket frame opcode.","type":"number"},{"name":"mask","description":"WebSocke frame mask.","type":"boolean"},{"name":"payloadData","description":"WebSocke frame payload data.","type":"string"}]},{"id":"CachedResource","description":"Information about the cached resource.","type":"object","properties":[{"name":"url","description":"Resource URL. This is the url of the original network request.","type":"string"},{"name":"type","description":"Type of this resource.","$ref":"ResourceType"},{"name":"response","description":"Cached response data.","optional":true,"$ref":"Response"},{"name":"bodySize","description":"Cached response body size.","type":"number"}]},{"id":"Initiator","description":"Information about the request initiator.","type":"object","properties":[{"name":"type","description":"Type of this initiator.","type":"string","enum":["parser","script","preload","SignedExchange","other"]},{"name":"stack","description":"Initiator JavaScript stack trace, set for Script only.","optional":true,"$ref":"Runtime.StackTrace"},{"name":"url","description":"Initiator URL, set for Parser type or for Script type (when script is importing module) or for SignedExchange type.","optional":true,"type":"string"},{"name":"lineNumber","description":"Initiator line number, set for Parser type or for Script type (when script is importing\nmodule) (0-based).","optional":true,"type":"number"}]},{"id":"Cookie","description":"Cookie object","type":"object","properties":[{"name":"name","description":"Cookie name.","type":"string"},{"name":"value","description":"Cookie value.","type":"string"},{"name":"domain","description":"Cookie domain.","type":"string"},{"name":"path","description":"Cookie path.","type":"string"},{"name":"expires","description":"Cookie expiration date as the number of seconds since the UNIX epoch.","type":"number"},{"name":"size","description":"Cookie size.","type":"integer"},{"name":"httpOnly","description":"True if cookie is http-only.","type":"boolean"},{"name":"secure","description":"True if cookie is secure.","type":"boolean"},{"name":"session","description":"True in case of session cookie.","type":"boolean"},{"name":"sameSite","description":"Cookie SameSite type.","optional":true,"$ref":"CookieSameSite"}]},{"id":"CookieParam","description":"Cookie parameter object","type":"object","properties":[{"name":"name","description":"Cookie name.","type":"string"},{"name":"value","description":"Cookie value.","type":"string"},{"name":"url","description":"The request-URI to associate with the setting of the cookie. This value can affect the\ndefault domain and path values of the created cookie.","optional":true,"type":"string"},{"name":"domain","description":"Cookie domain.","optional":true,"type":"string"},{"name":"path","description":"Cookie path.","optional":true,"type":"string"},{"name":"secure","description":"True if cookie is secure.","optional":true,"type":"boolean"},{"name":"httpOnly","description":"True if cookie is http-only.","optional":true,"type":"boolean"},{"name":"sameSite","description":"Cookie SameSite type.","optional":true,"$ref":"CookieSameSite"},{"name":"expires","description":"Cookie expiration date, session cookie if not set","optional":true,"$ref":"TimeSinceEpoch"}]},{"id":"AuthChallenge","description":"Authorization challenge for HTTP status code 401 or 407.","experimental":true,"type":"object","properties":[{"name":"source","description":"Source of the authentication challenge.","optional":true,"type":"string","enum":["Server","Proxy"]},{"name":"origin","description":"Origin of the challenger.","type":"string"},{"name":"scheme","description":"The authentication scheme used, such as basic or digest","type":"string"},{"name":"realm","description":"The realm of the challenge. May be empty.","type":"string"}]},{"id":"AuthChallengeResponse","description":"Response to an AuthChallenge.","experimental":true,"type":"object","properties":[{"name":"response","description":"The decision on what to do in response to the authorization challenge.  Default means\ndeferring to the default behavior of the net stack, which will likely either the Cancel\nauthentication or display a popup dialog box.","type":"string","enum":["Default","CancelAuth","ProvideCredentials"]},{"name":"username","description":"The username to provide, possibly empty. Should only be set if response is\nProvideCredentials.","optional":true,"type":"string"},{"name":"password","description":"The password to provide, possibly empty. Should only be set if response is\nProvideCredentials.","optional":true,"type":"string"}]},{"id":"InterceptionStage","description":"Stages of the interception to begin intercepting. Request will intercept before the request is\nsent. Response will intercept after the response is received.","experimental":true,"type":"string","enum":["Request","HeadersReceived"]},{"id":"RequestPattern","description":"Request pattern for interception.","experimental":true,"type":"object","properties":[{"name":"urlPattern","description":"Wildcards ('*' -> zero or more, '?' -> exactly one) are allowed. Escape character is\nbackslash. Omitting is equivalent to \"*\".","optional":true,"type":"string"},{"name":"resourceType","description":"If set, only requests for matching resource types will be intercepted.","optional":true,"$ref":"ResourceType"},{"name":"interceptionStage","description":"Stage at wich to begin intercepting requests. Default is Request.","optional":true,"$ref":"InterceptionStage"}]},{"id":"SignedExchangeSignature","description":"Information about a signed exchange signature.\nhttps://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#rfc.section.3.1","experimental":true,"type":"object","properties":[{"name":"label","description":"Signed exchange signature label.","type":"string"},{"name":"signature","description":"The hex string of signed exchange signature.","type":"string"},{"name":"integrity","description":"Signed exchange signature integrity.","type":"string"},{"name":"certUrl","description":"Signed exchange signature cert Url.","optional":true,"type":"string"},{"name":"certSha256","description":"The hex string of signed exchange signature cert sha256.","optional":true,"type":"string"},{"name":"validityUrl","description":"Signed exchange signature validity Url.","type":"string"},{"name":"date","description":"Signed exchange signature date.","type":"integer"},{"name":"expires","description":"Signed exchange signature expires.","type":"integer"},{"name":"certificates","description":"The encoded certificates.","optional":true,"type":"array","items":{"type":"string"}}]},{"id":"SignedExchangeHeader","description":"Information about a signed exchange header.\nhttps://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#cbor-representation","experimental":true,"type":"object","properties":[{"name":"requestUrl","description":"Signed exchange request URL.","type":"string"},{"name":"requestMethod","description":"Signed exchange request method.","type":"string"},{"name":"responseCode","description":"Signed exchange response code.","type":"integer"},{"name":"responseHeaders","description":"Signed exchange response headers.","$ref":"Headers"},{"name":"signatures","description":"Signed exchange response signature.","type":"array","items":{"$ref":"SignedExchangeSignature"}}]},{"id":"SignedExchangeErrorField","description":"Field type for a signed exchange related error.","experimental":true,"type":"string","enum":["signatureSig","signatureIntegrity","signatureCertUrl","signatureCertSha256","signatureValidityUrl","signatureTimestamps"]},{"id":"SignedExchangeError","description":"Information about a signed exchange response.","experimental":true,"type":"object","properties":[{"name":"message","description":"Error message.","type":"string"},{"name":"signatureIndex","description":"The index of the signature which caused the error.","optional":true,"type":"integer"},{"name":"errorField","description":"The field which caused the error.","optional":true,"$ref":"SignedExchangeErrorField"}]},{"id":"SignedExchangeInfo","description":"Information about a signed exchange response.","experimental":true,"type":"object","properties":[{"name":"outerResponse","description":"The outer response of signed HTTP exchange which was received from network.","$ref":"Response"},{"name":"header","description":"Information about the signed exchange header.","optional":true,"$ref":"SignedExchangeHeader"},{"name":"securityDetails","description":"Security details for the signed exchange header.","optional":true,"$ref":"SecurityDetails"},{"name":"errors","description":"Errors occurred while handling the signed exchagne.","optional":true,"type":"array","items":{"$ref":"SignedExchangeError"}}]}],"commands":[{"name":"canClearBrowserCache","description":"Tells whether clearing browser cache is supported.","deprecated":true,"returns":[{"name":"result","description":"True if browser cache can be cleared.","type":"boolean"}]},{"name":"canClearBrowserCookies","description":"Tells whether clearing browser cookies is supported.","deprecated":true,"returns":[{"name":"result","description":"True if browser cookies can be cleared.","type":"boolean"}]},{"name":"canEmulateNetworkConditions","description":"Tells whether emulation of network conditions is supported.","deprecated":true,"returns":[{"name":"result","description":"True if emulation of network conditions is supported.","type":"boolean"}]},{"name":"clearBrowserCache","description":"Clears browser cache."},{"name":"clearBrowserCookies","description":"Clears browser cookies."},{"name":"continueInterceptedRequest","description":"Response to Network.requestIntercepted which either modifies the request to continue with any\nmodifications, or blocks it, or completes it with the provided response bytes. If a network\nfetch occurs as a result which encounters a redirect an additional Network.requestIntercepted\nevent will be sent with the same InterceptionId.","experimental":true,"parameters":[{"name":"interceptionId","$ref":"InterceptionId"},{"name":"errorReason","description":"If set this causes the request to fail with the given reason. Passing `Aborted` for requests\nmarked with `isNavigationRequest` also cancels the navigation. Must not be set in response\nto an authChallenge.","optional":true,"$ref":"ErrorReason"},{"name":"rawResponse","description":"If set the requests completes using with the provided base64 encoded raw response, including\nHTTP status line and headers etc... Must not be set in response to an authChallenge.","optional":true,"type":"string"},{"name":"url","description":"If set the request url will be modified in a way that's not observable by page. Must not be\nset in response to an authChallenge.","optional":true,"type":"string"},{"name":"method","description":"If set this allows the request method to be overridden. Must not be set in response to an\nauthChallenge.","optional":true,"type":"string"},{"name":"postData","description":"If set this allows postData to be set. Must not be set in response to an authChallenge.","optional":true,"type":"string"},{"name":"headers","description":"If set this allows the request headers to be changed. Must not be set in response to an\nauthChallenge.","optional":true,"$ref":"Headers"},{"name":"authChallengeResponse","description":"Response to a requestIntercepted with an authChallenge. Must not be set otherwise.","optional":true,"$ref":"AuthChallengeResponse"}]},{"name":"deleteCookies","description":"Deletes browser cookies with matching name and url or domain/path pair.","parameters":[{"name":"name","description":"Name of the cookies to remove.","type":"string"},{"name":"url","description":"If specified, deletes all the cookies with the given name where domain and path match\nprovided URL.","optional":true,"type":"string"},{"name":"domain","description":"If specified, deletes only cookies with the exact domain.","optional":true,"type":"string"},{"name":"path","description":"If specified, deletes only cookies with the exact path.","optional":true,"type":"string"}]},{"name":"disable","description":"Disables network tracking, prevents network events from being sent to the client."},{"name":"emulateNetworkConditions","description":"Activates emulation of network conditions.","parameters":[{"name":"offline","description":"True to emulate internet disconnection.","type":"boolean"},{"name":"latency","description":"Minimum latency from request sent to response headers received (ms).","type":"number"},{"name":"downloadThroughput","description":"Maximal aggregated download throughput (bytes/sec). -1 disables download throttling.","type":"number"},{"name":"uploadThroughput","description":"Maximal aggregated upload throughput (bytes/sec).  -1 disables upload throttling.","type":"number"},{"name":"connectionType","description":"Connection type if known.","optional":true,"$ref":"ConnectionType"}]},{"name":"enable","description":"Enables network tracking, network events will now be delivered to the client.","parameters":[{"name":"maxTotalBufferSize","description":"Buffer size in bytes to use when preserving network payloads (XHRs, etc).","experimental":true,"optional":true,"type":"integer"},{"name":"maxResourceBufferSize","description":"Per-resource buffer size in bytes to use when preserving network payloads (XHRs, etc).","experimental":true,"optional":true,"type":"integer"},{"name":"maxPostDataSize","description":"Longest post body size (in bytes) that would be included in requestWillBeSent notification","optional":true,"type":"integer"}]},{"name":"getAllCookies","description":"Returns all browser cookies. Depending on the backend support, will return detailed cookie\ninformation in the `cookies` field.","returns":[{"name":"cookies","description":"Array of cookie objects.","type":"array","items":{"$ref":"Cookie"}}]},{"name":"getCertificate","description":"Returns the DER-encoded certificate.","experimental":true,"parameters":[{"name":"origin","description":"Origin to get certificate for.","type":"string"}],"returns":[{"name":"tableNames","type":"array","items":{"type":"string"}}]},{"name":"getCookies","description":"Returns all browser cookies for the current URL. Depending on the backend support, will return\ndetailed cookie information in the `cookies` field.","parameters":[{"name":"urls","description":"The list of URLs for which applicable cookies will be fetched","optional":true,"type":"array","items":{"type":"string"}}],"returns":[{"name":"cookies","description":"Array of cookie objects.","type":"array","items":{"$ref":"Cookie"}}]},{"name":"getResponseBody","description":"Returns content served for the given request.","parameters":[{"name":"requestId","description":"Identifier of the network request to get content for.","$ref":"RequestId"}],"returns":[{"name":"body","description":"Response body.","type":"string"},{"name":"base64Encoded","description":"True, if content was sent as base64.","type":"boolean"}]},{"name":"getRequestPostData","description":"Returns post data sent with the request. Returns an error when no data was sent with the request.","parameters":[{"name":"requestId","description":"Identifier of the network request to get content for.","$ref":"RequestId"}],"returns":[{"name":"postData","description":"Base64-encoded request body.","type":"string"}]},{"name":"getResponseBodyForInterception","description":"Returns content served for the given currently intercepted request.","experimental":true,"parameters":[{"name":"interceptionId","description":"Identifier for the intercepted request to get body for.","$ref":"InterceptionId"}],"returns":[{"name":"body","description":"Response body.","type":"string"},{"name":"base64Encoded","description":"True, if content was sent as base64.","type":"boolean"}]},{"name":"takeResponseBodyForInterceptionAsStream","description":"Returns a handle to the stream representing the response body. Note that after this command,\nthe intercepted request can't be continued as is -- you either need to cancel it or to provide\nthe response body. The stream only supports sequential read, IO.read will fail if the position\nis specified.","experimental":true,"parameters":[{"name":"interceptionId","$ref":"InterceptionId"}],"returns":[{"name":"stream","$ref":"IO.StreamHandle"}]},{"name":"replayXHR","description":"This method sends a new XMLHttpRequest which is identical to the original one. The following\nparameters should be identical: method, url, async, request body, extra headers, withCredentials\nattribute, user, password.","experimental":true,"parameters":[{"name":"requestId","description":"Identifier of XHR to replay.","$ref":"RequestId"}]},{"name":"searchInResponseBody","description":"Searches for given string in response content.","experimental":true,"parameters":[{"name":"requestId","description":"Identifier of the network response to search.","$ref":"RequestId"},{"name":"query","description":"String to search for.","type":"string"},{"name":"caseSensitive","description":"If true, search is case sensitive.","optional":true,"type":"boolean"},{"name":"isRegex","description":"If true, treats string parameter as regex.","optional":true,"type":"boolean"}],"returns":[{"name":"result","description":"List of search matches.","type":"array","items":{"$ref":"Debugger.SearchMatch"}}]},{"name":"setBlockedURLs","description":"Blocks URLs from loading.","experimental":true,"parameters":[{"name":"urls","description":"URL patterns to block. Wildcards ('*') are allowed.","type":"array","items":{"type":"string"}}]},{"name":"setBypassServiceWorker","description":"Toggles ignoring of service worker for each request.","experimental":true,"parameters":[{"name":"bypass","description":"Bypass service worker and load from network.","type":"boolean"}]},{"name":"setCacheDisabled","description":"Toggles ignoring cache for each request. If `true`, cache will not be used.","parameters":[{"name":"cacheDisabled","description":"Cache disabled state.","type":"boolean"}]},{"name":"setCookie","description":"Sets a cookie with the given cookie data; may overwrite equivalent cookies if they exist.","parameters":[{"name":"name","description":"Cookie name.","type":"string"},{"name":"value","description":"Cookie value.","type":"string"},{"name":"url","description":"The request-URI to associate with the setting of the cookie. This value can affect the\ndefault domain and path values of the created cookie.","optional":true,"type":"string"},{"name":"domain","description":"Cookie domain.","optional":true,"type":"string"},{"name":"path","description":"Cookie path.","optional":true,"type":"string"},{"name":"secure","description":"True if cookie is secure.","optional":true,"type":"boolean"},{"name":"httpOnly","description":"True if cookie is http-only.","optional":true,"type":"boolean"},{"name":"sameSite","description":"Cookie SameSite type.","optional":true,"$ref":"CookieSameSite"},{"name":"expires","description":"Cookie expiration date, session cookie if not set","optional":true,"$ref":"TimeSinceEpoch"}],"returns":[{"name":"success","description":"True if successfully set cookie.","type":"boolean"}]},{"name":"setCookies","description":"Sets given cookies.","parameters":[{"name":"cookies","description":"Cookies to be set.","type":"array","items":{"$ref":"CookieParam"}}]},{"name":"setDataSizeLimitsForTest","description":"For testing.","experimental":true,"parameters":[{"name":"maxTotalSize","description":"Maximum total buffer size.","type":"integer"},{"name":"maxResourceSize","description":"Maximum per-resource size.","type":"integer"}]},{"name":"setExtraHTTPHeaders","description":"Specifies whether to always send extra HTTP headers with the requests from this page.","parameters":[{"name":"headers","description":"Map with extra HTTP headers.","$ref":"Headers"}]},{"name":"setRequestInterception","description":"Sets the requests to intercept that match a the provided patterns and optionally resource types.","experimental":true,"parameters":[{"name":"patterns","description":"Requests matching any of these patterns will be forwarded and wait for the corresponding\ncontinueInterceptedRequest call.","type":"array","items":{"$ref":"RequestPattern"}}]},{"name":"setUserAgentOverride","description":"Allows overriding user agent with the given string.","redirect":"Emulation","parameters":[{"name":"userAgent","description":"User agent to use.","type":"string"},{"name":"acceptLanguage","description":"Browser langugage to emulate.","optional":true,"type":"string"},{"name":"platform","description":"The platform navigator.platform should return.","optional":true,"type":"string"}]}],"events":[{"name":"dataReceived","description":"Fired when data chunk was received over the network.","parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"timestamp","description":"Timestamp.","$ref":"MonotonicTime"},{"name":"dataLength","description":"Data chunk length.","type":"integer"},{"name":"encodedDataLength","description":"Actual bytes received (might be less than dataLength for compressed encodings).","type":"integer"}]},{"name":"eventSourceMessageReceived","description":"Fired when EventSource message is received.","parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"timestamp","description":"Timestamp.","$ref":"MonotonicTime"},{"name":"eventName","description":"Message type.","type":"string"},{"name":"eventId","description":"Message identifier.","type":"string"},{"name":"data","description":"Message content.","type":"string"}]},{"name":"loadingFailed","description":"Fired when HTTP request has failed to load.","parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"timestamp","description":"Timestamp.","$ref":"MonotonicTime"},{"name":"type","description":"Resource type.","$ref":"ResourceType"},{"name":"errorText","description":"User friendly error message.","type":"string"},{"name":"canceled","description":"True if loading was canceled.","optional":true,"type":"boolean"},{"name":"blockedReason","description":"The reason why loading was blocked, if any.","optional":true,"$ref":"BlockedReason"}]},{"name":"loadingFinished","description":"Fired when HTTP request has finished loading.","parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"timestamp","description":"Timestamp.","$ref":"MonotonicTime"},{"name":"encodedDataLength","description":"Total number of bytes received for this request.","type":"number"},{"name":"shouldReportCorbBlocking","description":"Set when 1) response was blocked by Cross-Origin Read Blocking and also\n2) this needs to be reported to the DevTools console.","optional":true,"type":"boolean"}]},{"name":"requestIntercepted","description":"Details of an intercepted HTTP request, which must be either allowed, blocked, modified or\nmocked.","experimental":true,"parameters":[{"name":"interceptionId","description":"Each request the page makes will have a unique id, however if any redirects are encountered\nwhile processing that fetch, they will be reported with the same id as the original fetch.\nLikewise if HTTP authentication is needed then the same fetch id will be used.","$ref":"InterceptionId"},{"name":"request","$ref":"Request"},{"name":"frameId","description":"The id of the frame that initiated the request.","$ref":"Page.FrameId"},{"name":"resourceType","description":"How the requested resource will be used.","$ref":"ResourceType"},{"name":"isNavigationRequest","description":"Whether this is a navigation request, which can abort the navigation completely.","type":"boolean"},{"name":"isDownload","description":"Set if the request is a navigation that will result in a download.\nOnly present after response is received from the server (i.e. HeadersReceived stage).","optional":true,"type":"boolean"},{"name":"redirectUrl","description":"Redirect location, only sent if a redirect was intercepted.","optional":true,"type":"string"},{"name":"authChallenge","description":"Details of the Authorization Challenge encountered. If this is set then\ncontinueInterceptedRequest must contain an authChallengeResponse.","optional":true,"$ref":"AuthChallenge"},{"name":"responseErrorReason","description":"Response error if intercepted at response stage or if redirect occurred while intercepting\nrequest.","optional":true,"$ref":"ErrorReason"},{"name":"responseStatusCode","description":"Response code if intercepted at response stage or if redirect occurred while intercepting\nrequest or auth retry occurred.","optional":true,"type":"integer"},{"name":"responseHeaders","description":"Response headers if intercepted at the response stage or if redirect occurred while\nintercepting request or auth retry occurred.","optional":true,"$ref":"Headers"}]},{"name":"requestServedFromCache","description":"Fired if request ended up loading from cache.","parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"}]},{"name":"requestWillBeSent","description":"Fired when page is about to send HTTP request.","parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"loaderId","description":"Loader identifier. Empty string if the request is fetched from worker.","$ref":"LoaderId"},{"name":"documentURL","description":"URL of the document this request is loaded for.","type":"string"},{"name":"request","description":"Request data.","$ref":"Request"},{"name":"timestamp","description":"Timestamp.","$ref":"MonotonicTime"},{"name":"wallTime","description":"Timestamp.","$ref":"TimeSinceEpoch"},{"name":"initiator","description":"Request initiator.","$ref":"Initiator"},{"name":"redirectResponse","description":"Redirect response data.","optional":true,"$ref":"Response"},{"name":"type","description":"Type of this resource.","optional":true,"$ref":"ResourceType"},{"name":"frameId","description":"Frame identifier.","optional":true,"$ref":"Page.FrameId"},{"name":"hasUserGesture","description":"Whether the request is initiated by a user gesture. Defaults to false.","optional":true,"type":"boolean"}]},{"name":"resourceChangedPriority","description":"Fired when resource loading priority is changed","experimental":true,"parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"newPriority","description":"New priority","$ref":"ResourcePriority"},{"name":"timestamp","description":"Timestamp.","$ref":"MonotonicTime"}]},{"name":"signedExchangeReceived","description":"Fired when a signed exchange was received over the network","experimental":true,"parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"info","description":"Information about the signed exchange response.","$ref":"SignedExchangeInfo"}]},{"name":"responseReceived","description":"Fired when HTTP response is available.","parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"loaderId","description":"Loader identifier. Empty string if the request is fetched from worker.","$ref":"LoaderId"},{"name":"timestamp","description":"Timestamp.","$ref":"MonotonicTime"},{"name":"type","description":"Resource type.","$ref":"ResourceType"},{"name":"response","description":"Response data.","$ref":"Response"},{"name":"frameId","description":"Frame identifier.","optional":true,"$ref":"Page.FrameId"}]},{"name":"webSocketClosed","description":"Fired when WebSocket is closed.","parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"timestamp","description":"Timestamp.","$ref":"MonotonicTime"}]},{"name":"webSocketCreated","description":"Fired upon WebSocket creation.","parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"url","description":"WebSocket request URL.","type":"string"},{"name":"initiator","description":"Request initiator.","optional":true,"$ref":"Initiator"}]},{"name":"webSocketFrameError","description":"Fired when WebSocket frame error occurs.","parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"timestamp","description":"Timestamp.","$ref":"MonotonicTime"},{"name":"errorMessage","description":"WebSocket frame error message.","type":"string"}]},{"name":"webSocketFrameReceived","description":"Fired when WebSocket frame is received.","parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"timestamp","description":"Timestamp.","$ref":"MonotonicTime"},{"name":"response","description":"WebSocket response data.","$ref":"WebSocketFrame"}]},{"name":"webSocketFrameSent","description":"Fired when WebSocket frame is sent.","parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"timestamp","description":"Timestamp.","$ref":"MonotonicTime"},{"name":"response","description":"WebSocket response data.","$ref":"WebSocketFrame"}]},{"name":"webSocketHandshakeResponseReceived","description":"Fired when WebSocket handshake response becomes available.","parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"timestamp","description":"Timestamp.","$ref":"MonotonicTime"},{"name":"response","description":"WebSocket response data.","$ref":"WebSocketResponse"}]},{"name":"webSocketWillSendHandshakeRequest","description":"Fired when WebSocket is about to initiate handshake.","parameters":[{"name":"requestId","description":"Request identifier.","$ref":"RequestId"},{"name":"timestamp","description":"Timestamp.","$ref":"MonotonicTime"},{"name":"wallTime","description":"UTC Timestamp.","$ref":"TimeSinceEpoch"},{"name":"request","description":"WebSocket request data.","$ref":"WebSocketRequest"}]}]},{"domain":"Overlay","description":"This domain provides various functionality related to drawing atop the inspected page.","experimental":true,"dependencies":["DOM","Page","Runtime"],"types":[{"id":"HighlightConfig","description":"Configuration data for the highlighting of page elements.","type":"object","properties":[{"name":"showInfo","description":"Whether the node info tooltip should be shown (default: false).","optional":true,"type":"boolean"},{"name":"showRulers","description":"Whether the rulers should be shown (default: false).","optional":true,"type":"boolean"},{"name":"showExtensionLines","description":"Whether the extension lines from node to the rulers should be shown (default: false).","optional":true,"type":"boolean"},{"name":"displayAsMaterial","optional":true,"type":"boolean"},{"name":"contentColor","description":"The content box highlight fill color (default: transparent).","optional":true,"$ref":"DOM.RGBA"},{"name":"paddingColor","description":"The padding highlight fill color (default: transparent).","optional":true,"$ref":"DOM.RGBA"},{"name":"borderColor","description":"The border highlight fill color (default: transparent).","optional":true,"$ref":"DOM.RGBA"},{"name":"marginColor","description":"The margin highlight fill color (default: transparent).","optional":true,"$ref":"DOM.RGBA"},{"name":"eventTargetColor","description":"The event target element highlight fill color (default: transparent).","optional":true,"$ref":"DOM.RGBA"},{"name":"shapeColor","description":"The shape outside fill color (default: transparent).","optional":true,"$ref":"DOM.RGBA"},{"name":"shapeMarginColor","description":"The shape margin fill color (default: transparent).","optional":true,"$ref":"DOM.RGBA"},{"name":"selectorList","description":"Selectors to highlight relevant nodes.","optional":true,"type":"string"},{"name":"cssGridColor","description":"The grid layout color (default: transparent).","optional":true,"$ref":"DOM.RGBA"}]},{"id":"InspectMode","type":"string","enum":["searchForNode","searchForUAShadowDOM","none"]}],"commands":[{"name":"disable","description":"Disables domain notifications."},{"name":"enable","description":"Enables domain notifications."},{"name":"getHighlightObjectForTest","description":"For testing.","parameters":[{"name":"nodeId","description":"Id of the node to get highlight object for.","$ref":"DOM.NodeId"}],"returns":[{"name":"highlight","description":"Highlight data for the node.","type":"object"}]},{"name":"hideHighlight","description":"Hides any highlight."},{"name":"highlightFrame","description":"Highlights owner element of the frame with given id.","parameters":[{"name":"frameId","description":"Identifier of the frame to highlight.","$ref":"Page.FrameId"},{"name":"contentColor","description":"The content box highlight fill color (default: transparent).","optional":true,"$ref":"DOM.RGBA"},{"name":"contentOutlineColor","description":"The content box highlight outline color (default: transparent).","optional":true,"$ref":"DOM.RGBA"}]},{"name":"highlightNode","description":"Highlights DOM node with given id or with the given JavaScript object wrapper. Either nodeId or\nobjectId must be specified.","parameters":[{"name":"highlightConfig","description":"A descriptor for the highlight appearance.","$ref":"HighlightConfig"},{"name":"nodeId","description":"Identifier of the node to highlight.","optional":true,"$ref":"DOM.NodeId"},{"name":"backendNodeId","description":"Identifier of the backend node to highlight.","optional":true,"$ref":"DOM.BackendNodeId"},{"name":"objectId","description":"JavaScript object id of the node to be highlighted.","optional":true,"$ref":"Runtime.RemoteObjectId"}]},{"name":"highlightQuad","description":"Highlights given quad. Coordinates are absolute with respect to the main frame viewport.","parameters":[{"name":"quad","description":"Quad to highlight","$ref":"DOM.Quad"},{"name":"color","description":"The highlight fill color (default: transparent).","optional":true,"$ref":"DOM.RGBA"},{"name":"outlineColor","description":"The highlight outline color (default: transparent).","optional":true,"$ref":"DOM.RGBA"}]},{"name":"highlightRect","description":"Highlights given rectangle. Coordinates are absolute with respect to the main frame viewport.","parameters":[{"name":"x","description":"X coordinate","type":"integer"},{"name":"y","description":"Y coordinate","type":"integer"},{"name":"width","description":"Rectangle width","type":"integer"},{"name":"height","description":"Rectangle height","type":"integer"},{"name":"color","description":"The highlight fill color (default: transparent).","optional":true,"$ref":"DOM.RGBA"},{"name":"outlineColor","description":"The highlight outline color (default: transparent).","optional":true,"$ref":"DOM.RGBA"}]},{"name":"setInspectMode","description":"Enters the 'inspect' mode. In this mode, elements that user is hovering over are highlighted.\nBackend then generates 'inspectNodeRequested' event upon element selection.","parameters":[{"name":"mode","description":"Set an inspection mode.","$ref":"InspectMode"},{"name":"highlightConfig","description":"A descriptor for the highlight appearance of hovered-over nodes. May be omitted if `enabled\n== false`.","optional":true,"$ref":"HighlightConfig"}]},{"name":"setPausedInDebuggerMessage","parameters":[{"name":"message","description":"The message to display, also triggers resume and step over controls.","optional":true,"type":"string"}]},{"name":"setShowDebugBorders","description":"Requests that backend shows debug borders on layers","parameters":[{"name":"show","description":"True for showing debug borders","type":"boolean"}]},{"name":"setShowFPSCounter","description":"Requests that backend shows the FPS counter","parameters":[{"name":"show","description":"True for showing the FPS counter","type":"boolean"}]},{"name":"setShowPaintRects","description":"Requests that backend shows paint rectangles","parameters":[{"name":"result","description":"True for showing paint rectangles","type":"boolean"}]},{"name":"setShowScrollBottleneckRects","description":"Requests that backend shows scroll bottleneck rects","parameters":[{"name":"show","description":"True for showing scroll bottleneck rects","type":"boolean"}]},{"name":"setShowHitTestBorders","description":"Requests that backend shows hit-test borders on layers","parameters":[{"name":"show","description":"True for showing hit-test borders","type":"boolean"}]},{"name":"setShowViewportSizeOnResize","description":"Paints viewport size upon main frame resize.","parameters":[{"name":"show","description":"Whether to paint size or not.","type":"boolean"}]},{"name":"setSuspended","parameters":[{"name":"suspended","description":"Whether overlay should be suspended and not consume any resources until resumed.","type":"boolean"}]}],"events":[{"name":"inspectNodeRequested","description":"Fired when the node should be inspected. This happens after call to `setInspectMode` or when\nuser manually inspects an element.","parameters":[{"name":"backendNodeId","description":"Id of the node to inspect.","$ref":"DOM.BackendNodeId"}]},{"name":"nodeHighlightRequested","description":"Fired when the node should be highlighted. This happens after call to `setInspectMode`.","parameters":[{"name":"nodeId","$ref":"DOM.NodeId"}]},{"name":"screenshotRequested","description":"Fired when user asks to capture screenshot of some area on the page.","parameters":[{"name":"viewport","description":"Viewport to capture, in CSS.","$ref":"Page.Viewport"}]}]},{"domain":"Page","description":"Actions and events related to the inspected page belong to the page domain.","dependencies":["Debugger","DOM","Network","Runtime"],"types":[{"id":"FrameId","description":"Unique frame identifier.","type":"string"},{"id":"Frame","description":"Information about the Frame on the page.","type":"object","properties":[{"name":"id","description":"Frame unique identifier.","type":"string"},{"name":"parentId","description":"Parent frame identifier.","optional":true,"type":"string"},{"name":"loaderId","description":"Identifier of the loader associated with this frame.","$ref":"Network.LoaderId"},{"name":"name","description":"Frame's name as specified in the tag.","optional":true,"type":"string"},{"name":"url","description":"Frame document's URL.","type":"string"},{"name":"securityOrigin","description":"Frame document's security origin.","type":"string"},{"name":"mimeType","description":"Frame document's mimeType as determined by the browser.","type":"string"},{"name":"unreachableUrl","description":"If the frame failed to load, this contains the URL that could not be loaded.","experimental":true,"optional":true,"type":"string"}]},{"id":"FrameResource","description":"Information about the Resource on the page.","experimental":true,"type":"object","properties":[{"name":"url","description":"Resource URL.","type":"string"},{"name":"type","description":"Type of this resource.","$ref":"Network.ResourceType"},{"name":"mimeType","description":"Resource mimeType as determined by the browser.","type":"string"},{"name":"lastModified","description":"last-modified timestamp as reported by server.","optional":true,"$ref":"Network.TimeSinceEpoch"},{"name":"contentSize","description":"Resource content size.","optional":true,"type":"number"},{"name":"failed","description":"True if the resource failed to load.","optional":true,"type":"boolean"},{"name":"canceled","description":"True if the resource was canceled during loading.","optional":true,"type":"boolean"}]},{"id":"FrameResourceTree","description":"Information about the Frame hierarchy along with their cached resources.","experimental":true,"type":"object","properties":[{"name":"frame","description":"Frame information for this tree item.","$ref":"Frame"},{"name":"childFrames","description":"Child frames.","optional":true,"type":"array","items":{"$ref":"FrameResourceTree"}},{"name":"resources","description":"Information about frame resources.","type":"array","items":{"$ref":"FrameResource"}}]},{"id":"FrameTree","description":"Information about the Frame hierarchy.","type":"object","properties":[{"name":"frame","description":"Frame information for this tree item.","$ref":"Frame"},{"name":"childFrames","description":"Child frames.","optional":true,"type":"array","items":{"$ref":"FrameTree"}}]},{"id":"ScriptIdentifier","description":"Unique script identifier.","type":"string"},{"id":"TransitionType","description":"Transition type.","type":"string","enum":["link","typed","address_bar","auto_bookmark","auto_subframe","manual_subframe","generated","auto_toplevel","form_submit","reload","keyword","keyword_generated","other"]},{"id":"NavigationEntry","description":"Navigation history entry.","type":"object","properties":[{"name":"id","description":"Unique id of the navigation history entry.","type":"integer"},{"name":"url","description":"URL of the navigation history entry.","type":"string"},{"name":"userTypedURL","description":"URL that the user typed in the url bar.","type":"string"},{"name":"title","description":"Title of the navigation history entry.","type":"string"},{"name":"transitionType","description":"Transition type.","$ref":"TransitionType"}]},{"id":"ScreencastFrameMetadata","description":"Screencast frame metadata.","experimental":true,"type":"object","properties":[{"name":"offsetTop","description":"Top offset in DIP.","type":"number"},{"name":"pageScaleFactor","description":"Page scale factor.","type":"number"},{"name":"deviceWidth","description":"Device screen width in DIP.","type":"number"},{"name":"deviceHeight","description":"Device screen height in DIP.","type":"number"},{"name":"scrollOffsetX","description":"Position of horizontal scroll in CSS pixels.","type":"number"},{"name":"scrollOffsetY","description":"Position of vertical scroll in CSS pixels.","type":"number"},{"name":"timestamp","description":"Frame swap timestamp.","optional":true,"$ref":"Network.TimeSinceEpoch"}]},{"id":"DialogType","description":"Javascript dialog type.","type":"string","enum":["alert","confirm","prompt","beforeunload"]},{"id":"AppManifestError","description":"Error while paring app manifest.","type":"object","properties":[{"name":"message","description":"Error message.","type":"string"},{"name":"critical","description":"If criticial, this is a non-recoverable parse error.","type":"integer"},{"name":"line","description":"Error line.","type":"integer"},{"name":"column","description":"Error column.","type":"integer"}]},{"id":"LayoutViewport","description":"Layout viewport position and dimensions.","type":"object","properties":[{"name":"pageX","description":"Horizontal offset relative to the document (CSS pixels).","type":"integer"},{"name":"pageY","description":"Vertical offset relative to the document (CSS pixels).","type":"integer"},{"name":"clientWidth","description":"Width (CSS pixels), excludes scrollbar if present.","type":"integer"},{"name":"clientHeight","description":"Height (CSS pixels), excludes scrollbar if present.","type":"integer"}]},{"id":"VisualViewport","description":"Visual viewport position, dimensions, and scale.","type":"object","properties":[{"name":"offsetX","description":"Horizontal offset relative to the layout viewport (CSS pixels).","type":"number"},{"name":"offsetY","description":"Vertical offset relative to the layout viewport (CSS pixels).","type":"number"},{"name":"pageX","description":"Horizontal offset relative to the document (CSS pixels).","type":"number"},{"name":"pageY","description":"Vertical offset relative to the document (CSS pixels).","type":"number"},{"name":"clientWidth","description":"Width (CSS pixels), excludes scrollbar if present.","type":"number"},{"name":"clientHeight","description":"Height (CSS pixels), excludes scrollbar if present.","type":"number"},{"name":"scale","description":"Scale relative to the ideal viewport (size at width=device-width).","type":"number"}]},{"id":"Viewport","description":"Viewport for capturing screenshot.","type":"object","properties":[{"name":"x","description":"X offset in CSS pixels.","type":"number"},{"name":"y","description":"Y offset in CSS pixels","type":"number"},{"name":"width","description":"Rectangle width in CSS pixels","type":"number"},{"name":"height","description":"Rectangle height in CSS pixels","type":"number"},{"name":"scale","description":"Page scale factor.","type":"number"}]},{"id":"FontFamilies","description":"Generic font families collection.","experimental":true,"type":"object","properties":[{"name":"standard","description":"The standard font-family.","optional":true,"type":"string"},{"name":"fixed","description":"The fixed font-family.","optional":true,"type":"string"},{"name":"serif","description":"The serif font-family.","optional":true,"type":"string"},{"name":"sansSerif","description":"The sansSerif font-family.","optional":true,"type":"string"},{"name":"cursive","description":"The cursive font-family.","optional":true,"type":"string"},{"name":"fantasy","description":"The fantasy font-family.","optional":true,"type":"string"},{"name":"pictograph","description":"The pictograph font-family.","optional":true,"type":"string"}]},{"id":"FontSizes","description":"Default font sizes.","experimental":true,"type":"object","properties":[{"name":"standard","description":"Default standard font size.","optional":true,"type":"integer"},{"name":"fixed","description":"Default fixed font size.","optional":true,"type":"integer"}]}],"commands":[{"name":"addScriptToEvaluateOnLoad","description":"Deprecated, please use addScriptToEvaluateOnNewDocument instead.","experimental":true,"deprecated":true,"parameters":[{"name":"scriptSource","type":"string"}],"returns":[{"name":"identifier","description":"Identifier of the added script.","$ref":"ScriptIdentifier"}]},{"name":"addScriptToEvaluateOnNewDocument","description":"Evaluates given script in every frame upon creation (before loading frame's scripts).","parameters":[{"name":"source","type":"string"},{"name":"worldName","description":"If specified, creates an isolated world with the given name and evaluates given script in it.\nThis world name will be used as the ExecutionContextDescription::name when the corresponding\nevent is emitted.","experimental":true,"optional":true,"type":"string"}],"returns":[{"name":"identifier","description":"Identifier of the added script.","$ref":"ScriptIdentifier"}]},{"name":"bringToFront","description":"Brings page to front (activates tab)."},{"name":"captureScreenshot","description":"Capture page screenshot.","parameters":[{"name":"format","description":"Image compression format (defaults to png).","optional":true,"type":"string","enum":["jpeg","png"]},{"name":"quality","description":"Compression quality from range [0..100] (jpeg only).","optional":true,"type":"integer"},{"name":"clip","description":"Capture the screenshot of a given region only.","optional":true,"$ref":"Viewport"},{"name":"fromSurface","description":"Capture the screenshot from the surface, rather than the view. Defaults to true.","experimental":true,"optional":true,"type":"boolean"}],"returns":[{"name":"data","description":"Base64-encoded image data.","type":"string"}]},{"name":"captureSnapshot","description":"Returns a snapshot of the page as a string. For MHTML format, the serialization includes\niframes, shadow DOM, external resources, and element-inline styles.","experimental":true,"parameters":[{"name":"format","description":"Format (defaults to mhtml).","optional":true,"type":"string","enum":["mhtml"]}],"returns":[{"name":"data","description":"Serialized page data.","type":"string"}]},{"name":"clearDeviceMetricsOverride","description":"Clears the overriden device metrics.","experimental":true,"deprecated":true,"redirect":"Emulation"},{"name":"clearDeviceOrientationOverride","description":"Clears the overridden Device Orientation.","experimental":true,"deprecated":true,"redirect":"DeviceOrientation"},{"name":"clearGeolocationOverride","description":"Clears the overriden Geolocation Position and Error.","deprecated":true,"redirect":"Emulation"},{"name":"createIsolatedWorld","description":"Creates an isolated world for the given frame.","parameters":[{"name":"frameId","description":"Id of the frame in which the isolated world should be created.","$ref":"FrameId"},{"name":"worldName","description":"An optional name which is reported in the Execution Context.","optional":true,"type":"string"},{"name":"grantUniveralAccess","description":"Whether or not universal access should be granted to the isolated world. This is a powerful\noption, use with caution.","optional":true,"type":"boolean"}],"returns":[{"name":"executionContextId","description":"Execution context of the isolated world.","$ref":"Runtime.ExecutionContextId"}]},{"name":"deleteCookie","description":"Deletes browser cookie with given name, domain and path.","experimental":true,"deprecated":true,"redirect":"Network","parameters":[{"name":"cookieName","description":"Name of the cookie to remove.","type":"string"},{"name":"url","description":"URL to match cooke domain and path.","type":"string"}]},{"name":"disable","description":"Disables page domain notifications."},{"name":"enable","description":"Enables page domain notifications."},{"name":"getAppManifest","returns":[{"name":"url","description":"Manifest location.","type":"string"},{"name":"errors","type":"array","items":{"$ref":"AppManifestError"}},{"name":"data","description":"Manifest content.","optional":true,"type":"string"}]},{"name":"getCookies","description":"Returns all browser cookies. Depending on the backend support, will return detailed cookie\ninformation in the `cookies` field.","experimental":true,"deprecated":true,"redirect":"Network","returns":[{"name":"cookies","description":"Array of cookie objects.","type":"array","items":{"$ref":"Network.Cookie"}}]},{"name":"getFrameTree","description":"Returns present frame tree structure.","returns":[{"name":"frameTree","description":"Present frame tree structure.","$ref":"FrameTree"}]},{"name":"getLayoutMetrics","description":"Returns metrics relating to the layouting of the page, such as viewport bounds/scale.","returns":[{"name":"layoutViewport","description":"Metrics relating to the layout viewport.","$ref":"LayoutViewport"},{"name":"visualViewport","description":"Metrics relating to the visual viewport.","$ref":"VisualViewport"},{"name":"contentSize","description":"Size of scrollable area.","$ref":"DOM.Rect"}]},{"name":"getNavigationHistory","description":"Returns navigation history for the current page.","returns":[{"name":"currentIndex","description":"Index of the current navigation history entry.","type":"integer"},{"name":"entries","description":"Array of navigation history entries.","type":"array","items":{"$ref":"NavigationEntry"}}]},{"name":"getResourceContent","description":"Returns content of the given resource.","experimental":true,"parameters":[{"name":"frameId","description":"Frame id to get resource for.","$ref":"FrameId"},{"name":"url","description":"URL of the resource to get content for.","type":"string"}],"returns":[{"name":"content","description":"Resource content.","type":"string"},{"name":"base64Encoded","description":"True, if content was served as base64.","type":"boolean"}]},{"name":"getResourceTree","description":"Returns present frame / resource tree structure.","experimental":true,"returns":[{"name":"frameTree","description":"Present frame / resource tree structure.","$ref":"FrameResourceTree"}]},{"name":"handleJavaScriptDialog","description":"Accepts or dismisses a JavaScript initiated dialog (alert, confirm, prompt, or onbeforeunload).","parameters":[{"name":"accept","description":"Whether to accept or dismiss the dialog.","type":"boolean"},{"name":"promptText","description":"The text to enter into the dialog prompt before accepting. Used only if this is a prompt\ndialog.","optional":true,"type":"string"}]},{"name":"navigate","description":"Navigates current page to the given URL.","parameters":[{"name":"url","description":"URL to navigate the page to.","type":"string"},{"name":"referrer","description":"Referrer URL.","optional":true,"type":"string"},{"name":"transitionType","description":"Intended transition type.","optional":true,"$ref":"TransitionType"},{"name":"frameId","description":"Frame id to navigate, if not specified navigates the top frame.","optional":true,"$ref":"FrameId"}],"returns":[{"name":"frameId","description":"Frame id that has navigated (or failed to navigate)","$ref":"FrameId"},{"name":"loaderId","description":"Loader identifier.","optional":true,"$ref":"Network.LoaderId"},{"name":"errorText","description":"User friendly error message, present if and only if navigation has failed.","optional":true,"type":"string"}]},{"name":"navigateToHistoryEntry","description":"Navigates current page to the given history entry.","parameters":[{"name":"entryId","description":"Unique id of the entry to navigate to.","type":"integer"}]},{"name":"printToPDF","description":"Print page as PDF.","parameters":[{"name":"landscape","description":"Paper orientation. Defaults to false.","optional":true,"type":"boolean"},{"name":"displayHeaderFooter","description":"Display header and footer. Defaults to false.","optional":true,"type":"boolean"},{"name":"printBackground","description":"Print background graphics. Defaults to false.","optional":true,"type":"boolean"},{"name":"scale","description":"Scale of the webpage rendering. Defaults to 1.","optional":true,"type":"number"},{"name":"paperWidth","description":"Paper width in inches. Defaults to 8.5 inches.","optional":true,"type":"number"},{"name":"paperHeight","description":"Paper height in inches. Defaults to 11 inches.","optional":true,"type":"number"},{"name":"marginTop","description":"Top margin in inches. Defaults to 1cm (~0.4 inches).","optional":true,"type":"number"},{"name":"marginBottom","description":"Bottom margin in inches. Defaults to 1cm (~0.4 inches).","optional":true,"type":"number"},{"name":"marginLeft","description":"Left margin in inches. Defaults to 1cm (~0.4 inches).","optional":true,"type":"number"},{"name":"marginRight","description":"Right margin in inches. Defaults to 1cm (~0.4 inches).","optional":true,"type":"number"},{"name":"pageRanges","description":"Paper ranges to print, e.g., '1-5, 8, 11-13'. Defaults to the empty string, which means\nprint all pages.","optional":true,"type":"string"},{"name":"ignoreInvalidPageRanges","description":"Whether to silently ignore invalid but successfully parsed page ranges, such as '3-2'.\nDefaults to false.","optional":true,"type":"boolean"},{"name":"headerTemplate","description":"HTML template for the print header. Should be valid HTML markup with following\nclasses used to inject printing values into them:\n- `date`: formatted print date\n- `title`: document title\n- `url`: document location\n- `pageNumber`: current page number\n- `totalPages`: total pages in the document\n\nFor example, `<span class=title></span>` would generate span containing the title.","optional":true,"type":"string"},{"name":"footerTemplate","description":"HTML template for the print footer. Should use the same format as the `headerTemplate`.","optional":true,"type":"string"},{"name":"preferCSSPageSize","description":"Whether or not to prefer page size as defined by css. Defaults to false,\nin which case the content will be scaled to fit the paper size.","optional":true,"type":"boolean"}],"returns":[{"name":"data","description":"Base64-encoded pdf data.","type":"string"}]},{"name":"reload","description":"Reloads given page optionally ignoring the cache.","parameters":[{"name":"ignoreCache","description":"If true, browser cache is ignored (as if the user pressed Shift+refresh).","optional":true,"type":"boolean"},{"name":"scriptToEvaluateOnLoad","description":"If set, the script will be injected into all frames of the inspected page after reload.\nArgument will be ignored if reloading dataURL origin.","optional":true,"type":"string"}]},{"name":"removeScriptToEvaluateOnLoad","description":"Deprecated, please use removeScriptToEvaluateOnNewDocument instead.","experimental":true,"deprecated":true,"parameters":[{"name":"identifier","$ref":"ScriptIdentifier"}]},{"name":"removeScriptToEvaluateOnNewDocument","description":"Removes given script from the list.","parameters":[{"name":"identifier","$ref":"ScriptIdentifier"}]},{"name":"requestAppBanner","experimental":true},{"name":"screencastFrameAck","description":"Acknowledges that a screencast frame has been received by the frontend.","experimental":true,"parameters":[{"name":"sessionId","description":"Frame number.","type":"integer"}]},{"name":"searchInResource","description":"Searches for given string in resource content.","experimental":true,"parameters":[{"name":"frameId","description":"Frame id for resource to search in.","$ref":"FrameId"},{"name":"url","description":"URL of the resource to search in.","type":"string"},{"name":"query","description":"String to search for.","type":"string"},{"name":"caseSensitive","description":"If true, search is case sensitive.","optional":true,"type":"boolean"},{"name":"isRegex","description":"If true, treats string parameter as regex.","optional":true,"type":"boolean"}],"returns":[{"name":"result","description":"List of search matches.","type":"array","items":{"$ref":"Debugger.SearchMatch"}}]},{"name":"setAdBlockingEnabled","description":"Enable Chrome's experimental ad filter on all sites.","experimental":true,"parameters":[{"name":"enabled","description":"Whether to block ads.","type":"boolean"}]},{"name":"setBypassCSP","description":"Enable page Content Security Policy by-passing.","experimental":true,"parameters":[{"name":"enabled","description":"Whether to bypass page CSP.","type":"boolean"}]},{"name":"setDeviceMetricsOverride","description":"Overrides the values of device screen dimensions (window.screen.width, window.screen.height,\nwindow.innerWidth, window.innerHeight, and \"device-width\"/\"device-height\"-related CSS media\nquery results).","experimental":true,"deprecated":true,"redirect":"Emulation","parameters":[{"name":"width","description":"Overriding width value in pixels (minimum 0, maximum 10000000). 0 disables the override.","type":"integer"},{"name":"height","description":"Overriding height value in pixels (minimum 0, maximum 10000000). 0 disables the override.","type":"integer"},{"name":"deviceScaleFactor","description":"Overriding device scale factor value. 0 disables the override.","type":"number"},{"name":"mobile","description":"Whether to emulate mobile device. This includes viewport meta tag, overlay scrollbars, text\nautosizing and more.","type":"boolean"},{"name":"scale","description":"Scale to apply to resulting view image.","optional":true,"type":"number"},{"name":"screenWidth","description":"Overriding screen width value in pixels (minimum 0, maximum 10000000).","optional":true,"type":"integer"},{"name":"screenHeight","description":"Overriding screen height value in pixels (minimum 0, maximum 10000000).","optional":true,"type":"integer"},{"name":"positionX","description":"Overriding view X position on screen in pixels (minimum 0, maximum 10000000).","optional":true,"type":"integer"},{"name":"positionY","description":"Overriding view Y position on screen in pixels (minimum 0, maximum 10000000).","optional":true,"type":"integer"},{"name":"dontSetVisibleSize","description":"Do not set visible view size, rely upon explicit setVisibleSize call.","optional":true,"type":"boolean"},{"name":"screenOrientation","description":"Screen orientation override.","optional":true,"$ref":"Emulation.ScreenOrientation"},{"name":"viewport","description":"The viewport dimensions and scale. If not set, the override is cleared.","optional":true,"$ref":"Viewport"}]},{"name":"setDeviceOrientationOverride","description":"Overrides the Device Orientation.","experimental":true,"deprecated":true,"redirect":"DeviceOrientation","parameters":[{"name":"alpha","description":"Mock alpha","type":"number"},{"name":"beta","description":"Mock beta","type":"number"},{"name":"gamma","description":"Mock gamma","type":"number"}]},{"name":"setFontFamilies","description":"Set generic font families.","experimental":true,"parameters":[{"name":"fontFamilies","description":"Specifies font families to set. If a font family is not specified, it won't be changed.","$ref":"FontFamilies"}]},{"name":"setFontSizes","description":"Set default font sizes.","experimental":true,"parameters":[{"name":"fontSizes","description":"Specifies font sizes to set. If a font size is not specified, it won't be changed.","$ref":"FontSizes"}]},{"name":"setDocumentContent","description":"Sets given markup as the document's HTML.","parameters":[{"name":"frameId","description":"Frame id to set HTML for.","$ref":"FrameId"},{"name":"html","description":"HTML content to set.","type":"string"}]},{"name":"setDownloadBehavior","description":"Set the behavior when downloading a file.","experimental":true,"parameters":[{"name":"behavior","description":"Whether to allow all or deny all download requests, or use default Chrome behavior if\navailable (otherwise deny).","type":"string","enum":["deny","allow","default"]},{"name":"downloadPath","description":"The default path to save downloaded files to. This is requred if behavior is set to 'allow'","optional":true,"type":"string"}]},{"name":"setGeolocationOverride","description":"Overrides the Geolocation Position or Error. Omitting any of the parameters emulates position\nunavailable.","deprecated":true,"redirect":"Emulation","parameters":[{"name":"latitude","description":"Mock latitude","optional":true,"type":"number"},{"name":"longitude","description":"Mock longitude","optional":true,"type":"number"},{"name":"accuracy","description":"Mock accuracy","optional":true,"type":"number"}]},{"name":"setLifecycleEventsEnabled","description":"Controls whether page will emit lifecycle events.","experimental":true,"parameters":[{"name":"enabled","description":"If true, starts emitting lifecycle events.","type":"boolean"}]},{"name":"setTouchEmulationEnabled","description":"Toggles mouse event-based touch event emulation.","experimental":true,"deprecated":true,"redirect":"Emulation","parameters":[{"name":"enabled","description":"Whether the touch event emulation should be enabled.","type":"boolean"},{"name":"configuration","description":"Touch/gesture events configuration. Default: current platform.","optional":true,"type":"string","enum":["mobile","desktop"]}]},{"name":"startScreencast","description":"Starts sending each frame using the `screencastFrame` event.","experimental":true,"parameters":[{"name":"format","description":"Image compression format.","optional":true,"type":"string","enum":["jpeg","png"]},{"name":"quality","description":"Compression quality from range [0..100].","optional":true,"type":"integer"},{"name":"maxWidth","description":"Maximum screenshot width.","optional":true,"type":"integer"},{"name":"maxHeight","description":"Maximum screenshot height.","optional":true,"type":"integer"},{"name":"everyNthFrame","description":"Send every n-th frame.","optional":true,"type":"integer"}]},{"name":"stopLoading","description":"Force the page stop all navigations and pending resource fetches."},{"name":"crash","description":"Crashes renderer on the IO thread, generates minidumps.","experimental":true},{"name":"close","description":"Tries to close page, running its beforeunload hooks, if any.","experimental":true},{"name":"setWebLifecycleState","description":"Tries to update the web lifecycle state of the page.\nIt will transition the page to the given state according to:\nhttps://github.com/WICG/web-lifecycle/","experimental":true,"parameters":[{"name":"state","description":"Target lifecycle state","type":"string","enum":["frozen","active"]}]},{"name":"stopScreencast","description":"Stops sending each frame in the `screencastFrame`.","experimental":true},{"name":"setProduceCompilationCache","description":"Forces compilation cache to be generated for every subresource script.","experimental":true,"parameters":[{"name":"enabled","type":"boolean"}]},{"name":"addCompilationCache","description":"Seeds compilation cache for given url. Compilation cache does not survive\ncross-process navigation.","experimental":true,"parameters":[{"name":"url","type":"string"},{"name":"data","description":"Base64-encoded data","type":"string"}]},{"name":"clearCompilationCache","description":"Clears seeded compilation cache.","experimental":true},{"name":"generateTestReport","description":"Generates a report for testing.","experimental":true,"parameters":[{"name":"message","description":"Message to be displayed in the report.","type":"string"},{"name":"group","description":"Specifies the endpoint group to deliver the report to.","optional":true,"type":"string"}]}],"events":[{"name":"domContentEventFired","parameters":[{"name":"timestamp","$ref":"Network.MonotonicTime"}]},{"name":"frameAttached","description":"Fired when frame has been attached to its parent.","parameters":[{"name":"frameId","description":"Id of the frame that has been attached.","$ref":"FrameId"},{"name":"parentFrameId","description":"Parent frame identifier.","$ref":"FrameId"},{"name":"stack","description":"JavaScript stack trace of when frame was attached, only set if frame initiated from script.","optional":true,"$ref":"Runtime.StackTrace"}]},{"name":"frameClearedScheduledNavigation","description":"Fired when frame no longer has a scheduled navigation.","experimental":true,"parameters":[{"name":"frameId","description":"Id of the frame that has cleared its scheduled navigation.","$ref":"FrameId"}]},{"name":"frameDetached","description":"Fired when frame has been detached from its parent.","parameters":[{"name":"frameId","description":"Id of the frame that has been detached.","$ref":"FrameId"}]},{"name":"frameNavigated","description":"Fired once navigation of the frame has completed. Frame is now associated with the new loader.","parameters":[{"name":"frame","description":"Frame object.","$ref":"Frame"}]},{"name":"frameResized","experimental":true},{"name":"frameScheduledNavigation","description":"Fired when frame schedules a potential navigation.","experimental":true,"parameters":[{"name":"frameId","description":"Id of the frame that has scheduled a navigation.","$ref":"FrameId"},{"name":"delay","description":"Delay (in seconds) until the navigation is scheduled to begin. The navigation is not\nguaranteed to start.","type":"number"},{"name":"reason","description":"The reason for the navigation.","type":"string","enum":["formSubmissionGet","formSubmissionPost","httpHeaderRefresh","scriptInitiated","metaTagRefresh","pageBlockInterstitial","reload"]},{"name":"url","description":"The destination URL for the scheduled navigation.","type":"string"}]},{"name":"frameStartedLoading","description":"Fired when frame has started loading.","experimental":true,"parameters":[{"name":"frameId","description":"Id of the frame that has started loading.","$ref":"FrameId"}]},{"name":"frameStoppedLoading","description":"Fired when frame has stopped loading.","experimental":true,"parameters":[{"name":"frameId","description":"Id of the frame that has stopped loading.","$ref":"FrameId"}]},{"name":"interstitialHidden","description":"Fired when interstitial page was hidden"},{"name":"interstitialShown","description":"Fired when interstitial page was shown"},{"name":"javascriptDialogClosed","description":"Fired when a JavaScript initiated dialog (alert, confirm, prompt, or onbeforeunload) has been\nclosed.","parameters":[{"name":"result","description":"Whether dialog was confirmed.","type":"boolean"},{"name":"userInput","description":"User input in case of prompt.","type":"string"}]},{"name":"javascriptDialogOpening","description":"Fired when a JavaScript initiated dialog (alert, confirm, prompt, or onbeforeunload) is about to\nopen.","parameters":[{"name":"url","description":"Frame url.","type":"string"},{"name":"message","description":"Message that will be displayed by the dialog.","type":"string"},{"name":"type","description":"Dialog type.","$ref":"DialogType"},{"name":"hasBrowserHandler","description":"True iff browser is capable showing or acting on the given dialog. When browser has no\ndialog handler for given target, calling alert while Page domain is engaged will stall\nthe page execution. Execution can be resumed via calling Page.handleJavaScriptDialog.","type":"boolean"},{"name":"defaultPrompt","description":"Default dialog prompt.","optional":true,"type":"string"}]},{"name":"lifecycleEvent","description":"Fired for top level page lifecycle events such as navigation, load, paint, etc.","parameters":[{"name":"frameId","description":"Id of the frame.","$ref":"FrameId"},{"name":"loaderId","description":"Loader identifier. Empty string if the request is fetched from worker.","$ref":"Network.LoaderId"},{"name":"name","type":"string"},{"name":"timestamp","$ref":"Network.MonotonicTime"}]},{"name":"loadEventFired","parameters":[{"name":"timestamp","$ref":"Network.MonotonicTime"}]},{"name":"navigatedWithinDocument","description":"Fired when same-document navigation happens, e.g. due to history API usage or anchor navigation.","experimental":true,"parameters":[{"name":"frameId","description":"Id of the frame.","$ref":"FrameId"},{"name":"url","description":"Frame's new url.","type":"string"}]},{"name":"screencastFrame","description":"Compressed image data requested by the `startScreencast`.","experimental":true,"parameters":[{"name":"data","description":"Base64-encoded compressed image.","type":"string"},{"name":"metadata","description":"Screencast frame metadata.","$ref":"ScreencastFrameMetadata"},{"name":"sessionId","description":"Frame number.","type":"integer"}]},{"name":"screencastVisibilityChanged","description":"Fired when the page with currently enabled screencast was shown or hidden `.","experimental":true,"parameters":[{"name":"visible","description":"True if the page is visible.","type":"boolean"}]},{"name":"windowOpen","description":"Fired when a new window is going to be opened, via window.open(), link click, form submission,\netc.","parameters":[{"name":"url","description":"The URL for the new window.","type":"string"},{"name":"windowName","description":"Window name.","type":"string"},{"name":"windowFeatures","description":"An array of enabled window features.","type":"array","items":{"type":"string"}},{"name":"userGesture","description":"Whether or not it was triggered by user gesture.","type":"boolean"}]},{"name":"compilationCacheProduced","description":"Issued for every compilation cache generated. Is only available\nif Page.setGenerateCompilationCache is enabled.","experimental":true,"parameters":[{"name":"url","type":"string"},{"name":"data","description":"Base64-encoded data","type":"string"}]}]},{"domain":"Performance","types":[{"id":"Metric","description":"Run-time execution metric.","type":"object","properties":[{"name":"name","description":"Metric name.","type":"string"},{"name":"value","description":"Metric value.","type":"number"}]}],"commands":[{"name":"disable","description":"Disable collecting and reporting metrics."},{"name":"enable","description":"Enable collecting and reporting metrics."},{"name":"setTimeDomain","description":"Sets time domain to use for collecting and reporting duration metrics.\nNote that this must be called before enabling metrics collection. Calling\nthis method while metrics collection is enabled returns an error.","experimental":true,"parameters":[{"name":"timeDomain","description":"Time domain","type":"string","enum":["timeTicks","threadTicks"]}]},{"name":"getMetrics","description":"Retrieve current values of run-time metrics.","returns":[{"name":"metrics","description":"Current values for run-time metrics.","type":"array","items":{"$ref":"Metric"}}]}],"events":[{"name":"metrics","description":"Current values of the metrics.","parameters":[{"name":"metrics","description":"Current values of the metrics.","type":"array","items":{"$ref":"Metric"}},{"name":"title","description":"Timestamp title.","type":"string"}]}]},{"domain":"Security","description":"Security","types":[{"id":"CertificateId","description":"An internal certificate ID value.","type":"integer"},{"id":"MixedContentType","description":"A description of mixed content (HTTP resources on HTTPS pages), as defined by\nhttps://www.w3.org/TR/mixed-content/#categories","type":"string","enum":["blockable","optionally-blockable","none"]},{"id":"SecurityState","description":"The security level of a page or resource.","type":"string","enum":["unknown","neutral","insecure","secure","info"]},{"id":"SecurityStateExplanation","description":"An explanation of an factor contributing to the security state.","type":"object","properties":[{"name":"securityState","description":"Security state representing the severity of the factor being explained.","$ref":"SecurityState"},{"name":"title","description":"Title describing the type of factor.","type":"string"},{"name":"summary","description":"Short phrase describing the type of factor.","type":"string"},{"name":"description","description":"Full text explanation of the factor.","type":"string"},{"name":"mixedContentType","description":"The type of mixed content described by the explanation.","$ref":"MixedContentType"},{"name":"certificate","description":"Page certificate.","type":"array","items":{"type":"string"}},{"name":"recommendations","description":"Recommendations to fix any issues.","optional":true,"type":"array","items":{"type":"string"}}]},{"id":"InsecureContentStatus","description":"Information about insecure content on the page.","type":"object","properties":[{"name":"ranMixedContent","description":"True if the page was loaded over HTTPS and ran mixed (HTTP) content such as scripts.","type":"boolean"},{"name":"displayedMixedContent","description":"True if the page was loaded over HTTPS and displayed mixed (HTTP) content such as images.","type":"boolean"},{"name":"containedMixedForm","description":"True if the page was loaded over HTTPS and contained a form targeting an insecure url.","type":"boolean"},{"name":"ranContentWithCertErrors","description":"True if the page was loaded over HTTPS without certificate errors, and ran content such as\nscripts that were loaded with certificate errors.","type":"boolean"},{"name":"displayedContentWithCertErrors","description":"True if the page was loaded over HTTPS without certificate errors, and displayed content\nsuch as images that were loaded with certificate errors.","type":"boolean"},{"name":"ranInsecureContentStyle","description":"Security state representing a page that ran insecure content.","$ref":"SecurityState"},{"name":"displayedInsecureContentStyle","description":"Security state representing a page that displayed insecure content.","$ref":"SecurityState"}]},{"id":"CertificateErrorAction","description":"The action to take when a certificate error occurs. continue will continue processing the\nrequest and cancel will cancel the request.","type":"string","enum":["continue","cancel"]}],"commands":[{"name":"disable","description":"Disables tracking security state changes."},{"name":"enable","description":"Enables tracking security state changes."},{"name":"setIgnoreCertificateErrors","description":"Enable/disable whether all certificate errors should be ignored.","experimental":true,"parameters":[{"name":"ignore","description":"If true, all certificate errors will be ignored.","type":"boolean"}]},{"name":"handleCertificateError","description":"Handles a certificate error that fired a certificateError event.","deprecated":true,"parameters":[{"name":"eventId","description":"The ID of the event.","type":"integer"},{"name":"action","description":"The action to take on the certificate error.","$ref":"CertificateErrorAction"}]},{"name":"setOverrideCertificateErrors","description":"Enable/disable overriding certificate errors. If enabled, all certificate error events need to\nbe handled by the DevTools client and should be answered with `handleCertificateError` commands.","deprecated":true,"parameters":[{"name":"override","description":"If true, certificate errors will be overridden.","type":"boolean"}]}],"events":[{"name":"certificateError","description":"There is a certificate error. If overriding certificate errors is enabled, then it should be\nhandled with the `handleCertificateError` command. Note: this event does not fire if the\ncertificate error has been allowed internally. Only one client per target should override\ncertificate errors at the same time.","deprecated":true,"parameters":[{"name":"eventId","description":"The ID of the event.","type":"integer"},{"name":"errorType","description":"The type of the error.","type":"string"},{"name":"requestURL","description":"The url that was requested.","type":"string"}]},{"name":"securityStateChanged","description":"The security state of the page changed.","parameters":[{"name":"securityState","description":"Security state.","$ref":"SecurityState"},{"name":"schemeIsCryptographic","description":"True if the page was loaded over cryptographic transport such as HTTPS.","type":"boolean"},{"name":"explanations","description":"List of explanations for the security state. If the overall security state is `insecure` or\n`warning`, at least one corresponding explanation should be included.","type":"array","items":{"$ref":"SecurityStateExplanation"}},{"name":"insecureContentStatus","description":"Information about insecure content on the page.","$ref":"InsecureContentStatus"},{"name":"summary","description":"Overrides user-visible description of the state.","optional":true,"type":"string"}]}]},{"domain":"ServiceWorker","experimental":true,"types":[{"id":"ServiceWorkerRegistration","description":"ServiceWorker registration.","type":"object","properties":[{"name":"registrationId","type":"string"},{"name":"scopeURL","type":"string"},{"name":"isDeleted","type":"boolean"}]},{"id":"ServiceWorkerVersionRunningStatus","type":"string","enum":["stopped","starting","running","stopping"]},{"id":"ServiceWorkerVersionStatus","type":"string","enum":["new","installing","installed","activating","activated","redundant"]},{"id":"ServiceWorkerVersion","description":"ServiceWorker version.","type":"object","properties":[{"name":"versionId","type":"string"},{"name":"registrationId","type":"string"},{"name":"scriptURL","type":"string"},{"name":"runningStatus","$ref":"ServiceWorkerVersionRunningStatus"},{"name":"status","$ref":"ServiceWorkerVersionStatus"},{"name":"scriptLastModified","description":"The Last-Modified header value of the main script.","optional":true,"type":"number"},{"name":"scriptResponseTime","description":"The time at which the response headers of the main script were received from the server.\nFor cached script it is the last time the cache entry was validated.","optional":true,"type":"number"},{"name":"controlledClients","optional":true,"type":"array","items":{"$ref":"Target.TargetID"}},{"name":"targetId","optional":true,"$ref":"Target.TargetID"}]},{"id":"ServiceWorkerErrorMessage","description":"ServiceWorker error message.","type":"object","properties":[{"name":"errorMessage","type":"string"},{"name":"registrationId","type":"string"},{"name":"versionId","type":"string"},{"name":"sourceURL","type":"string"},{"name":"lineNumber","type":"integer"},{"name":"columnNumber","type":"integer"}]}],"commands":[{"name":"deliverPushMessage","parameters":[{"name":"origin","type":"string"},{"name":"registrationId","type":"string"},{"name":"data","type":"string"}]},{"name":"disable"},{"name":"dispatchSyncEvent","parameters":[{"name":"origin","type":"string"},{"name":"registrationId","type":"string"},{"name":"tag","type":"string"},{"name":"lastChance","type":"boolean"}]},{"name":"enable"},{"name":"inspectWorker","parameters":[{"name":"versionId","type":"string"}]},{"name":"setForceUpdateOnPageLoad","parameters":[{"name":"forceUpdateOnPageLoad","type":"boolean"}]},{"name":"skipWaiting","parameters":[{"name":"scopeURL","type":"string"}]},{"name":"startWorker","parameters":[{"name":"scopeURL","type":"string"}]},{"name":"stopAllWorkers"},{"name":"stopWorker","parameters":[{"name":"versionId","type":"string"}]},{"name":"unregister","parameters":[{"name":"scopeURL","type":"string"}]},{"name":"updateRegistration","parameters":[{"name":"scopeURL","type":"string"}]}],"events":[{"name":"workerErrorReported","parameters":[{"name":"errorMessage","$ref":"ServiceWorkerErrorMessage"}]},{"name":"workerRegistrationUpdated","parameters":[{"name":"registrations","type":"array","items":{"$ref":"ServiceWorkerRegistration"}}]},{"name":"workerVersionUpdated","parameters":[{"name":"versions","type":"array","items":{"$ref":"ServiceWorkerVersion"}}]}]},{"domain":"Storage","experimental":true,"types":[{"id":"StorageType","description":"Enum of possible storage types.","type":"string","enum":["appcache","cookies","file_systems","indexeddb","local_storage","shader_cache","websql","service_workers","cache_storage","all","other"]},{"id":"UsageForType","description":"Usage for a storage type.","type":"object","properties":[{"name":"storageType","description":"Name of storage type.","$ref":"StorageType"},{"name":"usage","description":"Storage usage (bytes).","type":"number"}]}],"commands":[{"name":"clearDataForOrigin","description":"Clears storage for origin.","parameters":[{"name":"origin","description":"Security origin.","type":"string"},{"name":"storageTypes","description":"Comma separated origin names.","type":"string"}]},{"name":"getUsageAndQuota","description":"Returns usage and quota in bytes.","parameters":[{"name":"origin","description":"Security origin.","type":"string"}],"returns":[{"name":"usage","description":"Storage usage (bytes).","type":"number"},{"name":"quota","description":"Storage quota (bytes).","type":"number"},{"name":"usageBreakdown","description":"Storage usage per type (bytes).","type":"array","items":{"$ref":"UsageForType"}}]},{"name":"trackCacheStorageForOrigin","description":"Registers origin to be notified when an update occurs to its cache storage list.","parameters":[{"name":"origin","description":"Security origin.","type":"string"}]},{"name":"trackIndexedDBForOrigin","description":"Registers origin to be notified when an update occurs to its IndexedDB.","parameters":[{"name":"origin","description":"Security origin.","type":"string"}]},{"name":"untrackCacheStorageForOrigin","description":"Unregisters origin from receiving notifications for cache storage.","parameters":[{"name":"origin","description":"Security origin.","type":"string"}]},{"name":"untrackIndexedDBForOrigin","description":"Unregisters origin from receiving notifications for IndexedDB.","parameters":[{"name":"origin","description":"Security origin.","type":"string"}]}],"events":[{"name":"cacheStorageContentUpdated","description":"A cache's contents have been modified.","parameters":[{"name":"origin","description":"Origin to update.","type":"string"},{"name":"cacheName","description":"Name of cache in origin.","type":"string"}]},{"name":"cacheStorageListUpdated","description":"A cache has been added/deleted.","parameters":[{"name":"origin","description":"Origin to update.","type":"string"}]},{"name":"indexedDBContentUpdated","description":"The origin's IndexedDB object store has been modified.","parameters":[{"name":"origin","description":"Origin to update.","type":"string"},{"name":"databaseName","description":"Database to update.","type":"string"},{"name":"objectStoreName","description":"ObjectStore to update.","type":"string"}]},{"name":"indexedDBListUpdated","description":"The origin's IndexedDB database list has been modified.","parameters":[{"name":"origin","description":"Origin to update.","type":"string"}]}]},{"domain":"SystemInfo","description":"The SystemInfo domain defines methods and events for querying low-level system information.","experimental":true,"types":[{"id":"GPUDevice","description":"Describes a single graphics processor (GPU).","type":"object","properties":[{"name":"vendorId","description":"PCI ID of the GPU vendor, if available; 0 otherwise.","type":"number"},{"name":"deviceId","description":"PCI ID of the GPU device, if available; 0 otherwise.","type":"number"},{"name":"vendorString","description":"String description of the GPU vendor, if the PCI ID is not available.","type":"string"},{"name":"deviceString","description":"String description of the GPU device, if the PCI ID is not available.","type":"string"}]},{"id":"GPUInfo","description":"Provides information about the GPU(s) on the system.","type":"object","properties":[{"name":"devices","description":"The graphics devices on the system. Element 0 is the primary GPU.","type":"array","items":{"$ref":"GPUDevice"}},{"name":"auxAttributes","description":"An optional dictionary of additional GPU related attributes.","optional":true,"type":"object"},{"name":"featureStatus","description":"An optional dictionary of graphics features and their status.","optional":true,"type":"object"},{"name":"driverBugWorkarounds","description":"An optional array of GPU driver bug workarounds.","type":"array","items":{"type":"string"}}]},{"id":"ProcessInfo","description":"Represents process info.","type":"object","properties":[{"name":"type","description":"Specifies process type.","type":"string"},{"name":"id","description":"Specifies process id.","type":"integer"},{"name":"cpuTime","description":"Specifies cumulative CPU usage in seconds across all threads of the\nprocess since the process start.","type":"number"}]}],"commands":[{"name":"getInfo","description":"Returns information about the system.","returns":[{"name":"gpu","description":"Information about the GPUs on the system.","$ref":"GPUInfo"},{"name":"modelName","description":"A platform-dependent description of the model of the machine. On Mac OS, this is, for\nexample, 'MacBookPro'. Will be the empty string if not supported.","type":"string"},{"name":"modelVersion","description":"A platform-dependent description of the version of the machine. On Mac OS, this is, for\nexample, '10.1'. Will be the empty string if not supported.","type":"string"},{"name":"commandLine","description":"The command line string used to launch the browser. Will be the empty string if not\nsupported.","type":"string"}]},{"name":"getProcessInfo","description":"Returns information about all running processes.","returns":[{"name":"processInfo","description":"An array of process info blocks.","type":"array","items":{"$ref":"ProcessInfo"}}]}]},{"domain":"Target","description":"Supports additional targets discovery and allows to attach to them.","types":[{"id":"TargetID","type":"string"},{"id":"SessionID","description":"Unique identifier of attached debugging session.","type":"string"},{"id":"BrowserContextID","experimental":true,"type":"string"},{"id":"TargetInfo","type":"object","properties":[{"name":"targetId","$ref":"TargetID"},{"name":"type","type":"string"},{"name":"title","type":"string"},{"name":"url","type":"string"},{"name":"attached","description":"Whether the target has an attached client.","type":"boolean"},{"name":"openerId","description":"Opener target Id","optional":true,"$ref":"TargetID"},{"name":"browserContextId","experimental":true,"optional":true,"$ref":"BrowserContextID"}]},{"id":"RemoteLocation","experimental":true,"type":"object","properties":[{"name":"host","type":"string"},{"name":"port","type":"integer"}]}],"commands":[{"name":"activateTarget","description":"Activates (focuses) the target.","parameters":[{"name":"targetId","$ref":"TargetID"}]},{"name":"attachToTarget","description":"Attaches to the target with given id.","parameters":[{"name":"targetId","$ref":"TargetID"},{"name":"flatten","description":"Enables \"flat\" access to the session via specifying sessionId attribute in the commands.","experimental":true,"optional":true,"type":"boolean"}],"returns":[{"name":"sessionId","description":"Id assigned to the session.","$ref":"SessionID"}]},{"name":"attachToBrowserTarget","description":"Attaches to the browser target, only uses flat sessionId mode.","experimental":true,"returns":[{"name":"sessionId","description":"Id assigned to the session.","$ref":"SessionID"}]},{"name":"closeTarget","description":"Closes the target. If the target is a page that gets closed too.","parameters":[{"name":"targetId","$ref":"TargetID"}],"returns":[{"name":"success","type":"boolean"}]},{"name":"exposeDevToolsProtocol","description":"Inject object to the target's main frame that provides a communication\nchannel with browser target.\n\nInjected object will be available as `window[bindingName]`.\n\nThe object has the follwing API:\n- `binding.send(json)` - a method to send messages over the remote debugging protocol\n- `binding.onmessage = json => handleMessage(json)` - a callback that will be called for the protocol notifications and command responses.","experimental":true,"parameters":[{"name":"targetId","$ref":"TargetID"},{"name":"bindingName","description":"Binding name, 'cdp' if not specified.","optional":true,"type":"string"}]},{"name":"createBrowserContext","description":"Creates a new empty BrowserContext. Similar to an incognito profile but you can have more than\none.","experimental":true,"returns":[{"name":"browserContextId","description":"The id of the context created.","$ref":"BrowserContextID"}]},{"name":"getBrowserContexts","description":"Returns all browser contexts created with `Target.createBrowserContext` method.","experimental":true,"returns":[{"name":"browserContextIds","description":"An array of browser context ids.","type":"array","items":{"$ref":"BrowserContextID"}}]},{"name":"createTarget","description":"Creates a new page.","parameters":[{"name":"url","description":"The initial URL the page will be navigated to.","type":"string"},{"name":"width","description":"Frame width in DIP (headless chrome only).","optional":true,"type":"integer"},{"name":"height","description":"Frame height in DIP (headless chrome only).","optional":true,"type":"integer"},{"name":"browserContextId","description":"The browser context to create the page in.","optional":true,"$ref":"BrowserContextID"},{"name":"enableBeginFrameControl","description":"Whether BeginFrames for this target will be controlled via DevTools (headless chrome only,\nnot supported on MacOS yet, false by default).","experimental":true,"optional":true,"type":"boolean"}],"returns":[{"name":"targetId","description":"The id of the page opened.","$ref":"TargetID"}]},{"name":"detachFromTarget","description":"Detaches session with given id.","parameters":[{"name":"sessionId","description":"Session to detach.","optional":true,"$ref":"SessionID"},{"name":"targetId","description":"Deprecated.","deprecated":true,"optional":true,"$ref":"TargetID"}]},{"name":"disposeBrowserContext","description":"Deletes a BrowserContext. All the belonging pages will be closed without calling their\nbeforeunload hooks.","experimental":true,"parameters":[{"name":"browserContextId","$ref":"BrowserContextID"}]},{"name":"getTargetInfo","description":"Returns information about a target.","experimental":true,"parameters":[{"name":"targetId","optional":true,"$ref":"TargetID"}],"returns":[{"name":"targetInfo","$ref":"TargetInfo"}]},{"name":"getTargets","description":"Retrieves a list of available targets.","returns":[{"name":"targetInfos","description":"The list of targets.","type":"array","items":{"$ref":"TargetInfo"}}]},{"name":"sendMessageToTarget","description":"Sends protocol message over session with given id.","parameters":[{"name":"message","type":"string"},{"name":"sessionId","description":"Identifier of the session.","optional":true,"$ref":"SessionID"},{"name":"targetId","description":"Deprecated.","deprecated":true,"optional":true,"$ref":"TargetID"}]},{"name":"setAutoAttach","description":"Controls whether to automatically attach to new targets which are considered to be related to\nthis one. When turned on, attaches to all existing related targets as well. When turned off,\nautomatically detaches from all currently attached targets.","experimental":true,"parameters":[{"name":"autoAttach","description":"Whether to auto-attach to related targets.","type":"boolean"},{"name":"waitForDebuggerOnStart","description":"Whether to pause new targets when attaching to them. Use `Runtime.runIfWaitingForDebugger`\nto run paused targets.","type":"boolean"},{"name":"flatten","description":"Enables \"flat\" access to the session via specifying sessionId attribute in the commands.","experimental":true,"optional":true,"type":"boolean"}]},{"name":"setDiscoverTargets","description":"Controls whether to discover available targets and notify via\n`targetCreated/targetInfoChanged/targetDestroyed` events.","parameters":[{"name":"discover","description":"Whether to discover available targets.","type":"boolean"}]},{"name":"setRemoteLocations","description":"Enables target discovery for the specified locations, when `setDiscoverTargets` was set to\n`true`.","experimental":true,"parameters":[{"name":"locations","description":"List of remote locations.","type":"array","items":{"$ref":"RemoteLocation"}}]}],"events":[{"name":"attachedToTarget","description":"Issued when attached to target because of auto-attach or `attachToTarget` command.","experimental":true,"parameters":[{"name":"sessionId","description":"Identifier assigned to the session used to send/receive messages.","$ref":"SessionID"},{"name":"targetInfo","$ref":"TargetInfo"},{"name":"waitingForDebugger","type":"boolean"}]},{"name":"detachedFromTarget","description":"Issued when detached from target for any reason (including `detachFromTarget` command). Can be\nissued multiple times per target if multiple sessions have been attached to it.","experimental":true,"parameters":[{"name":"sessionId","description":"Detached session identifier.","$ref":"SessionID"},{"name":"targetId","description":"Deprecated.","deprecated":true,"optional":true,"$ref":"TargetID"}]},{"name":"receivedMessageFromTarget","description":"Notifies about a new protocol message received from the session (as reported in\n`attachedToTarget` event).","parameters":[{"name":"sessionId","description":"Identifier of a session which sends a message.","$ref":"SessionID"},{"name":"message","type":"string"},{"name":"targetId","description":"Deprecated.","deprecated":true,"optional":true,"$ref":"TargetID"}]},{"name":"targetCreated","description":"Issued when a possible inspection target is created.","parameters":[{"name":"targetInfo","$ref":"TargetInfo"}]},{"name":"targetDestroyed","description":"Issued when a target is destroyed.","parameters":[{"name":"targetId","$ref":"TargetID"}]},{"name":"targetCrashed","description":"Issued when a target has crashed.","parameters":[{"name":"targetId","$ref":"TargetID"},{"name":"status","description":"Termination status type.","type":"string"},{"name":"errorCode","description":"Termination error code.","type":"integer"}]},{"name":"targetInfoChanged","description":"Issued when some information about a target has changed. This only happens between\n`targetCreated` and `targetDestroyed`.","parameters":[{"name":"targetInfo","$ref":"TargetInfo"}]}]},{"domain":"Tethering","description":"The Tethering domain defines methods and events for browser port binding.","experimental":true,"commands":[{"name":"bind","description":"Request browser port binding.","parameters":[{"name":"port","description":"Port number to bind.","type":"integer"}]},{"name":"unbind","description":"Request browser port unbinding.","parameters":[{"name":"port","description":"Port number to unbind.","type":"integer"}]}],"events":[{"name":"accepted","description":"Informs that port was successfully bound and got a specified connection id.","parameters":[{"name":"port","description":"Port number that was successfully bound.","type":"integer"},{"name":"connectionId","description":"Connection id to be used.","type":"string"}]}]},{"domain":"Tracing","experimental":true,"dependencies":["IO"],"types":[{"id":"MemoryDumpConfig","description":"Configuration for memory dump. Used only when \"memory-infra\" category is enabled.","type":"object"},{"id":"TraceConfig","type":"object","properties":[{"name":"recordMode","description":"Controls how the trace buffer stores data.","optional":true,"type":"string","enum":["recordUntilFull","recordContinuously","recordAsMuchAsPossible","echoToConsole"]},{"name":"enableSampling","description":"Turns on JavaScript stack sampling.","optional":true,"type":"boolean"},{"name":"enableSystrace","description":"Turns on system tracing.","optional":true,"type":"boolean"},{"name":"enableArgumentFilter","description":"Turns on argument filter.","optional":true,"type":"boolean"},{"name":"includedCategories","description":"Included category filters.","optional":true,"type":"array","items":{"type":"string"}},{"name":"excludedCategories","description":"Excluded category filters.","optional":true,"type":"array","items":{"type":"string"}},{"name":"syntheticDelays","description":"Configuration to synthesize the delays in tracing.","optional":true,"type":"array","items":{"type":"string"}},{"name":"memoryDumpConfig","description":"Configuration for memory dump triggers. Used only when \"memory-infra\" category is enabled.","optional":true,"$ref":"MemoryDumpConfig"}]},{"id":"StreamCompression","description":"Compression type to use for traces returned via streams.","type":"string","enum":["none","gzip"]}],"commands":[{"name":"end","description":"Stop trace events collection."},{"name":"getCategories","description":"Gets supported tracing categories.","returns":[{"name":"categories","description":"A list of supported tracing categories.","type":"array","items":{"type":"string"}}]},{"name":"recordClockSyncMarker","description":"Record a clock sync marker in the trace.","parameters":[{"name":"syncId","description":"The ID of this clock sync marker","type":"string"}]},{"name":"requestMemoryDump","description":"Request a global memory dump.","returns":[{"name":"dumpGuid","description":"GUID of the resulting global memory dump.","type":"string"},{"name":"success","description":"True iff the global memory dump succeeded.","type":"boolean"}]},{"name":"start","description":"Start trace events collection.","parameters":[{"name":"categories","description":"Category/tag filter","deprecated":true,"optional":true,"type":"string"},{"name":"options","description":"Tracing options","deprecated":true,"optional":true,"type":"string"},{"name":"bufferUsageReportingInterval","description":"If set, the agent will issue bufferUsage events at this interval, specified in milliseconds","optional":true,"type":"number"},{"name":"transferMode","description":"Whether to report trace events as series of dataCollected events or to save trace to a\nstream (defaults to `ReportEvents`).","optional":true,"type":"string","enum":["ReportEvents","ReturnAsStream"]},{"name":"streamCompression","description":"Compression format to use. This only applies when using `ReturnAsStream`\ntransfer mode (defaults to `none`)","optional":true,"$ref":"StreamCompression"},{"name":"traceConfig","optional":true,"$ref":"TraceConfig"}]}],"events":[{"name":"bufferUsage","parameters":[{"name":"percentFull","description":"A number in range [0..1] that indicates the used size of event buffer as a fraction of its\ntotal size.","optional":true,"type":"number"},{"name":"eventCount","description":"An approximate number of events in the trace log.","optional":true,"type":"number"},{"name":"value","description":"A number in range [0..1] that indicates the used size of event buffer as a fraction of its\ntotal size.","optional":true,"type":"number"}]},{"name":"dataCollected","description":"Contains an bucket of collected trace events. When tracing is stopped collected events will be\nsend as a sequence of dataCollected events followed by tracingComplete event.","parameters":[{"name":"value","type":"array","items":{"type":"object"}}]},{"name":"tracingComplete","description":"Signals that tracing is stopped and there is no trace buffers pending flush, all data were\ndelivered via dataCollected events.","parameters":[{"name":"stream","description":"A handle of the stream that holds resulting trace data.","optional":true,"$ref":"IO.StreamHandle"},{"name":"streamCompression","description":"Compression format of returned stream.","optional":true,"$ref":"StreamCompression"}]}]},{"domain":"Testing","description":"Testing domain is a dumping ground for the capabilities requires for browser or app testing that do not fit other\ndomains.","experimental":true,"dependencies":["Page"],"commands":[{"name":"generateTestReport","description":"Generates a report for testing.","parameters":[{"name":"message","description":"Message to be displayed in the report.","type":"string"},{"name":"group","description":"Specifies the endpoint group to deliver the report to.","optional":true,"type":"string"}]}]},{"domain":"Fetch","description":"A domain for letting clients substitute browser's network layer with client code.","experimental":true,"dependencies":["Network","IO","Page"],"types":[{"id":"RequestId","description":"Unique request identifier.","type":"string"},{"id":"RequestStage","description":"Stages of the request to handle. Request will intercept before the request is\nsent. Response will intercept after the response is received (but before response\nbody is received.","experimental":true,"type":"string","enum":["Request","Response"]},{"id":"RequestPattern","experimental":true,"type":"object","properties":[{"name":"urlPattern","description":"Wildcards ('*' -> zero or more, '?' -> exactly one) are allowed. Escape character is\nbackslash. Omitting is equivalent to \"*\".","optional":true,"type":"string"},{"name":"resourceType","description":"If set, only requests for matching resource types will be intercepted.","optional":true,"$ref":"Network.ResourceType"},{"name":"requestStage","description":"Stage at wich to begin intercepting requests. Default is Request.","optional":true,"$ref":"RequestStage"}]},{"id":"HeaderEntry","description":"Response HTTP header entry","type":"object","properties":[{"name":"name","type":"string"},{"name":"value","type":"string"}]},{"id":"AuthChallenge","description":"Authorization challenge for HTTP status code 401 or 407.","experimental":true,"type":"object","properties":[{"name":"source","description":"Source of the authentication challenge.","optional":true,"type":"string","enum":["Server","Proxy"]},{"name":"origin","description":"Origin of the challenger.","type":"string"},{"name":"scheme","description":"The authentication scheme used, such as basic or digest","type":"string"},{"name":"realm","description":"The realm of the challenge. May be empty.","type":"string"}]},{"id":"AuthChallengeResponse","description":"Response to an AuthChallenge.","experimental":true,"type":"object","properties":[{"name":"response","description":"The decision on what to do in response to the authorization challenge.  Default means\ndeferring to the default behavior of the net stack, which will likely either the Cancel\nauthentication or display a popup dialog box.","type":"string","enum":["Default","CancelAuth","ProvideCredentials"]},{"name":"username","description":"The username to provide, possibly empty. Should only be set if response is\nProvideCredentials.","optional":true,"type":"string"},{"name":"password","description":"The password to provide, possibly empty. Should only be set if response is\nProvideCredentials.","optional":true,"type":"string"}]}],"commands":[{"name":"disable","description":"Disables the fetch domain."},{"name":"enable","description":"Enables issuing of requestPaused events. A request will be paused until client\ncalls one of failRequest, fulfillRequest or continueRequest/continueWithAuth.","parameters":[{"name":"patterns","description":"If specified, only requests matching any of these patterns will produce\nfetchRequested event and will be paused until clients response. If not set,\nall requests will be affected.","optional":true,"type":"array","items":{"$ref":"RequestPattern"}},{"name":"handleAuthRequests","description":"If true, authRequired events will be issued and requests will be paused\nexpecting a call to continueWithAuth.","optional":true,"type":"boolean"}]},{"name":"failRequest","description":"Causes the request to fail with specified reason.","parameters":[{"name":"requestId","description":"An id the client received in requestPaused event.","$ref":"RequestId"},{"name":"errorReason","description":"Causes the request to fail with the given reason.","$ref":"Network.ErrorReason"}]},{"name":"fulfillRequest","description":"Provides response to the request.","parameters":[{"name":"requestId","description":"An id the client received in requestPaused event.","$ref":"RequestId"},{"name":"responseCode","description":"An HTTP response code.","type":"integer"},{"name":"responseHeaders","description":"Response headers.","type":"array","items":{"$ref":"HeaderEntry"}},{"name":"body","description":"A response body.","optional":true,"type":"string"},{"name":"responsePhrase","description":"A textual representation of responseCode.\nIf absent, a standard phrase mathcing responseCode is used.","optional":true,"type":"string"}]},{"name":"continueRequest","description":"Continues the request, optionally modifying some of its parameters.","parameters":[{"name":"requestId","description":"An id the client received in requestPaused event.","$ref":"RequestId"},{"name":"url","description":"If set, the request url will be modified in a way that's not observable by page.","optional":true,"type":"string"},{"name":"method","description":"If set, the request method is overridden.","optional":true,"type":"string"},{"name":"postData","description":"If set, overrides the post data in the request.","optional":true,"type":"string"},{"name":"headers","description":"If set, overrides the request headrts.","optional":true,"type":"array","items":{"$ref":"HeaderEntry"}}]},{"name":"continueWithAuth","description":"Continues a request supplying authChallengeResponse following authRequired event.","parameters":[{"name":"requestId","description":"An id the client received in authRequired event.","$ref":"RequestId"},{"name":"authChallengeResponse","description":"Response to  with an authChallenge.","$ref":"AuthChallengeResponse"}]},{"name":"getResponseBody","description":"Causes the body of the response to be received from the server and\nreturned as a single string. May only be issued for a request that\nis paused in the Response stage and is mutually exclusive with\ntakeResponseBodyForInterceptionAsStream. Calling other methods that\naffect the request or disabling fetch domain before body is received\nresults in an undefined behavior.","parameters":[{"name":"requestId","description":"Identifier for the intercepted request to get body for.","$ref":"RequestId"}],"returns":[{"name":"body","description":"Response body.","type":"string"},{"name":"base64Encoded","description":"True, if content was sent as base64.","type":"boolean"}]},{"name":"takeResponseBodyAsStream","description":"Returns a handle to the stream representing the response body.\nThe request must be paused in the HeadersReceived stage.\nNote that after this command the request can't be continued\nas is -- client either needs to cancel it or to provide the\nresponse body.\nThe stream only supports sequential read, IO.read will fail if the position\nis specified.\nThis method is mutually exclusive with getResponseBody.\nCalling other methods that affect the request or disabling fetch\ndomain before body is received results in an undefined behavior.","parameters":[{"name":"requestId","$ref":"RequestId"}],"returns":[{"name":"stream","$ref":"IO.StreamHandle"}]}],"events":[{"name":"requestPaused","description":"Issued when the domain is enabled and the request URL matches the\nspecified filter. The request is paused until the client responds\nwith one of continueRequest, failRequest or fulfillRequest.\nThe stage of the request can be determined by presence of responseErrorReason\nand responseStatusCode -- the request is at the response stage if either\nof these fields is present and in the request stage otherwise.","parameters":[{"name":"requestId","description":"Each request the page makes will have a unique id.","$ref":"RequestId"},{"name":"request","description":"The details of the request.","$ref":"Network.Request"},{"name":"frameId","description":"The id of the frame that initiated the request.","$ref":"Page.FrameId"},{"name":"resourceType","description":"How the requested resource will be used.","$ref":"Network.ResourceType"},{"name":"responseErrorReason","description":"Response error if intercepted at response stage.","optional":true,"$ref":"Network.ErrorReason"},{"name":"responseStatusCode","description":"Response code if intercepted at response stage.","optional":true,"type":"integer"},{"name":"responseHeaders","description":"Response headers if intercepted at the response stage.","optional":true,"type":"array","items":{"$ref":"HeaderEntry"}}]},{"name":"authRequired","description":"Issued when the domain is enabled with handleAuthRequests set to true.\nThe request is paused until client responds with continueWithAuth.","parameters":[{"name":"requestId","description":"Each request the page makes will have a unique id.","$ref":"RequestId"},{"name":"request","description":"The details of the request.","$ref":"Network.Request"},{"name":"frameId","description":"The id of the frame that initiated the request.","$ref":"Page.FrameId"},{"name":"resourceType","description":"How the requested resource will be used.","$ref":"Network.ResourceType"},{"name":"authChallenge","description":"Details of the Authorization Challenge encountered.\nIf this is set, client should respond with continueRequest that\ncontains AuthChallengeResponse.","$ref":"AuthChallenge"}]}]},{"domain":"Console","description":"This domain is deprecated - use Runtime or Log instead.","deprecated":true,"dependencies":["Runtime"],"types":[{"id":"ConsoleMessage","description":"Console message.","type":"object","properties":[{"name":"source","description":"Message source.","type":"string","enum":["xml","javascript","network","console-api","storage","appcache","rendering","security","other","deprecation","worker"]},{"name":"level","description":"Message severity.","type":"string","enum":["log","warning","error","debug","info"]},{"name":"text","description":"Message text.","type":"string"},{"name":"url","description":"URL of the message origin.","optional":true,"type":"string"},{"name":"line","description":"Line number in the resource that generated this message (1-based).","optional":true,"type":"integer"},{"name":"column","description":"Column number in the resource that generated this message (1-based).","optional":true,"type":"integer"}]}],"commands":[{"name":"clearMessages","description":"Does nothing."},{"name":"disable","description":"Disables console domain, prevents further console messages from being reported to the client."},{"name":"enable","description":"Enables console domain, sends the messages collected so far to the client by means of the\n`messageAdded` notification."}],"events":[{"name":"messageAdded","description":"Issued when new console message is added.","parameters":[{"name":"message","description":"Console message that has been added.","$ref":"ConsoleMessage"}]}]},{"domain":"Debugger","description":"Debugger domain exposes JavaScript debugging capabilities. It allows setting and removing\nbreakpoints, stepping through execution, exploring stack traces, etc.","dependencies":["Runtime"],"types":[{"id":"BreakpointId","description":"Breakpoint identifier.","type":"string"},{"id":"CallFrameId","description":"Call frame identifier.","type":"string"},{"id":"Location","description":"Location in the source code.","type":"object","properties":[{"name":"scriptId","description":"Script identifier as reported in the `Debugger.scriptParsed`.","$ref":"Runtime.ScriptId"},{"name":"lineNumber","description":"Line number in the script (0-based).","type":"integer"},{"name":"columnNumber","description":"Column number in the script (0-based).","optional":true,"type":"integer"}]},{"id":"ScriptPosition","description":"Location in the source code.","experimental":true,"type":"object","properties":[{"name":"lineNumber","type":"integer"},{"name":"columnNumber","type":"integer"}]},{"id":"CallFrame","description":"JavaScript call frame. Array of call frames form the call stack.","type":"object","properties":[{"name":"callFrameId","description":"Call frame identifier. This identifier is only valid while the virtual machine is paused.","$ref":"CallFrameId"},{"name":"functionName","description":"Name of the JavaScript function called on this call frame.","type":"string"},{"name":"functionLocation","description":"Location in the source code.","optional":true,"$ref":"Location"},{"name":"location","description":"Location in the source code.","$ref":"Location"},{"name":"url","description":"JavaScript script name or url.","type":"string"},{"name":"scopeChain","description":"Scope chain for this call frame.","type":"array","items":{"$ref":"Scope"}},{"name":"this","description":"`this` object for this call frame.","$ref":"Runtime.RemoteObject"},{"name":"returnValue","description":"The value being returned, if the function is at return point.","optional":true,"$ref":"Runtime.RemoteObject"}]},{"id":"Scope","description":"Scope description.","type":"object","properties":[{"name":"type","description":"Scope type.","type":"string","enum":["global","local","with","closure","catch","block","script","eval","module"]},{"name":"object","description":"Object representing the scope. For `global` and `with` scopes it represents the actual\nobject; for the rest of the scopes, it is artificial transient object enumerating scope\nvariables as its properties.","$ref":"Runtime.RemoteObject"},{"name":"name","optional":true,"type":"string"},{"name":"startLocation","description":"Location in the source code where scope starts","optional":true,"$ref":"Location"},{"name":"endLocation","description":"Location in the source code where scope ends","optional":true,"$ref":"Location"}]},{"id":"SearchMatch","description":"Search match for resource.","type":"object","properties":[{"name":"lineNumber","description":"Line number in resource content.","type":"number"},{"name":"lineContent","description":"Line with match content.","type":"string"}]},{"id":"BreakLocation","type":"object","properties":[{"name":"scriptId","description":"Script identifier as reported in the `Debugger.scriptParsed`.","$ref":"Runtime.ScriptId"},{"name":"lineNumber","description":"Line number in the script (0-based).","type":"integer"},{"name":"columnNumber","description":"Column number in the script (0-based).","optional":true,"type":"integer"},{"name":"type","optional":true,"type":"string","enum":["debuggerStatement","call","return"]}]}],"commands":[{"name":"continueToLocation","description":"Continues execution until specific location is reached.","parameters":[{"name":"location","description":"Location to continue to.","$ref":"Location"},{"name":"targetCallFrames","optional":true,"type":"string","enum":["any","current"]}]},{"name":"disable","description":"Disables debugger for given page."},{"name":"enable","description":"Enables debugger for the given page. Clients should not assume that the debugging has been\nenabled until the result for this command is received.","returns":[{"name":"debuggerId","description":"Unique identifier of the debugger.","experimental":true,"$ref":"Runtime.UniqueDebuggerId"}]},{"name":"evaluateOnCallFrame","description":"Evaluates expression on a given call frame.","parameters":[{"name":"callFrameId","description":"Call frame identifier to evaluate on.","$ref":"CallFrameId"},{"name":"expression","description":"Expression to evaluate.","type":"string"},{"name":"objectGroup","description":"String object group name to put result into (allows rapid releasing resulting object handles\nusing `releaseObjectGroup`).","optional":true,"type":"string"},{"name":"includeCommandLineAPI","description":"Specifies whether command line API should be available to the evaluated expression, defaults\nto false.","optional":true,"type":"boolean"},{"name":"silent","description":"In silent mode exceptions thrown during evaluation are not reported and do not pause\nexecution. Overrides `setPauseOnException` state.","optional":true,"type":"boolean"},{"name":"returnByValue","description":"Whether the result is expected to be a JSON object that should be sent by value.","optional":true,"type":"boolean"},{"name":"generatePreview","description":"Whether preview should be generated for the result.","experimental":true,"optional":true,"type":"boolean"},{"name":"throwOnSideEffect","description":"Whether to throw an exception if side effect cannot be ruled out during evaluation.","optional":true,"type":"boolean"},{"name":"timeout","description":"Terminate execution after timing out (number of milliseconds).","experimental":true,"optional":true,"$ref":"Runtime.TimeDelta"}],"returns":[{"name":"result","description":"Object wrapper for the evaluation result.","$ref":"Runtime.RemoteObject"},{"name":"exceptionDetails","description":"Exception details.","optional":true,"$ref":"Runtime.ExceptionDetails"}]},{"name":"getPossibleBreakpoints","description":"Returns possible locations for breakpoint. scriptId in start and end range locations should be\nthe same.","parameters":[{"name":"start","description":"Start of range to search possible breakpoint locations in.","$ref":"Location"},{"name":"end","description":"End of range to search possible breakpoint locations in (excluding). When not specified, end\nof scripts is used as end of range.","optional":true,"$ref":"Location"},{"name":"restrictToFunction","description":"Only consider locations which are in the same (non-nested) function as start.","optional":true,"type":"boolean"}],"returns":[{"name":"locations","description":"List of the possible breakpoint locations.","type":"array","items":{"$ref":"BreakLocation"}}]},{"name":"getScriptSource","description":"Returns source for the script with given id.","parameters":[{"name":"scriptId","description":"Id of the script to get source for.","$ref":"Runtime.ScriptId"}],"returns":[{"name":"scriptSource","description":"Script source.","type":"string"}]},{"name":"getStackTrace","description":"Returns stack trace with given `stackTraceId`.","experimental":true,"parameters":[{"name":"stackTraceId","$ref":"Runtime.StackTraceId"}],"returns":[{"name":"stackTrace","$ref":"Runtime.StackTrace"}]},{"name":"pause","description":"Stops on the next JavaScript statement."},{"name":"pauseOnAsyncCall","experimental":true,"parameters":[{"name":"parentStackTraceId","description":"Debugger will pause when async call with given stack trace is started.","$ref":"Runtime.StackTraceId"}]},{"name":"removeBreakpoint","description":"Removes JavaScript breakpoint.","parameters":[{"name":"breakpointId","$ref":"BreakpointId"}]},{"name":"restartFrame","description":"Restarts particular call frame from the beginning.","parameters":[{"name":"callFrameId","description":"Call frame identifier to evaluate on.","$ref":"CallFrameId"}],"returns":[{"name":"callFrames","description":"New stack trace.","type":"array","items":{"$ref":"CallFrame"}},{"name":"asyncStackTrace","description":"Async stack trace, if any.","optional":true,"$ref":"Runtime.StackTrace"},{"name":"asyncStackTraceId","description":"Async stack trace, if any.","experimental":true,"optional":true,"$ref":"Runtime.StackTraceId"}]},{"name":"resume","description":"Resumes JavaScript execution."},{"name":"scheduleStepIntoAsync","description":"This method is deprecated - use Debugger.stepInto with breakOnAsyncCall and\nDebugger.pauseOnAsyncTask instead. Steps into next scheduled async task if any is scheduled\nbefore next pause. Returns success when async task is actually scheduled, returns error if no\ntask were scheduled or another scheduleStepIntoAsync was called.","experimental":true},{"name":"searchInContent","description":"Searches for given string in script content.","parameters":[{"name":"scriptId","description":"Id of the script to search in.","$ref":"Runtime.ScriptId"},{"name":"query","description":"String to search for.","type":"string"},{"name":"caseSensitive","description":"If true, search is case sensitive.","optional":true,"type":"boolean"},{"name":"isRegex","description":"If true, treats string parameter as regex.","optional":true,"type":"boolean"}],"returns":[{"name":"result","description":"List of search matches.","type":"array","items":{"$ref":"SearchMatch"}}]},{"name":"setAsyncCallStackDepth","description":"Enables or disables async call stacks tracking.","parameters":[{"name":"maxDepth","description":"Maximum depth of async call stacks. Setting to `0` will effectively disable collecting async\ncall stacks (default).","type":"integer"}]},{"name":"setBlackboxPatterns","description":"Replace previous blackbox patterns with passed ones. Forces backend to skip stepping/pausing in\nscripts with url matching one of the patterns. VM will try to leave blackboxed script by\nperforming 'step in' several times, finally resorting to 'step out' if unsuccessful.","experimental":true,"parameters":[{"name":"patterns","description":"Array of regexps that will be used to check script url for blackbox state.","type":"array","items":{"type":"string"}}]},{"name":"setBlackboxedRanges","description":"Makes backend skip steps in the script in blackboxed ranges. VM will try leave blacklisted\nscripts by performing 'step in' several times, finally resorting to 'step out' if unsuccessful.\nPositions array contains positions where blackbox state is changed. First interval isn't\nblackboxed. Array should be sorted.","experimental":true,"parameters":[{"name":"scriptId","description":"Id of the script.","$ref":"Runtime.ScriptId"},{"name":"positions","type":"array","items":{"$ref":"ScriptPosition"}}]},{"name":"setBreakpoint","description":"Sets JavaScript breakpoint at a given location.","parameters":[{"name":"location","description":"Location to set breakpoint in.","$ref":"Location"},{"name":"condition","description":"Expression to use as a breakpoint condition. When specified, debugger will only stop on the\nbreakpoint if this expression evaluates to true.","optional":true,"type":"string"}],"returns":[{"name":"breakpointId","description":"Id of the created breakpoint for further reference.","$ref":"BreakpointId"},{"name":"actualLocation","description":"Location this breakpoint resolved into.","$ref":"Location"}]},{"name":"setBreakpointByUrl","description":"Sets JavaScript breakpoint at given location specified either by URL or URL regex. Once this\ncommand is issued, all existing parsed scripts will have breakpoints resolved and returned in\n`locations` property. Further matching script parsing will result in subsequent\n`breakpointResolved` events issued. This logical breakpoint will survive page reloads.","parameters":[{"name":"lineNumber","description":"Line number to set breakpoint at.","type":"integer"},{"name":"url","description":"URL of the resources to set breakpoint on.","optional":true,"type":"string"},{"name":"urlRegex","description":"Regex pattern for the URLs of the resources to set breakpoints on. Either `url` or\n`urlRegex` must be specified.","optional":true,"type":"string"},{"name":"scriptHash","description":"Script hash of the resources to set breakpoint on.","optional":true,"type":"string"},{"name":"columnNumber","description":"Offset in the line to set breakpoint at.","optional":true,"type":"integer"},{"name":"condition","description":"Expression to use as a breakpoint condition. When specified, debugger will only stop on the\nbreakpoint if this expression evaluates to true.","optional":true,"type":"string"}],"returns":[{"name":"breakpointId","description":"Id of the created breakpoint for further reference.","$ref":"BreakpointId"},{"name":"locations","description":"List of the locations this breakpoint resolved into upon addition.","type":"array","items":{"$ref":"Location"}}]},{"name":"setBreakpointOnFunctionCall","description":"Sets JavaScript breakpoint before each call to the given function.\nIf another function was created from the same source as a given one,\ncalling it will also trigger the breakpoint.","experimental":true,"parameters":[{"name":"objectId","description":"Function object id.","$ref":"Runtime.RemoteObjectId"},{"name":"condition","description":"Expression to use as a breakpoint condition. When specified, debugger will\nstop on the breakpoint if this expression evaluates to true.","optional":true,"type":"string"}],"returns":[{"name":"breakpointId","description":"Id of the created breakpoint for further reference.","$ref":"BreakpointId"}]},{"name":"setBreakpointsActive","description":"Activates / deactivates all breakpoints on the page.","parameters":[{"name":"active","description":"New value for breakpoints active state.","type":"boolean"}]},{"name":"setPauseOnExceptions","description":"Defines pause on exceptions state. Can be set to stop on all exceptions, uncaught exceptions or\nno exceptions. Initial pause on exceptions state is `none`.","parameters":[{"name":"state","description":"Pause on exceptions mode.","type":"string","enum":["none","uncaught","all"]}]},{"name":"setReturnValue","description":"Changes return value in top frame. Available only at return break position.","experimental":true,"parameters":[{"name":"newValue","description":"New return value.","$ref":"Runtime.CallArgument"}]},{"name":"setScriptSource","description":"Edits JavaScript source live.","parameters":[{"name":"scriptId","description":"Id of the script to edit.","$ref":"Runtime.ScriptId"},{"name":"scriptSource","description":"New content of the script.","type":"string"},{"name":"dryRun","description":"If true the change will not actually be applied. Dry run may be used to get result\ndescription without actually modifying the code.","optional":true,"type":"boolean"}],"returns":[{"name":"callFrames","description":"New stack trace in case editing has happened while VM was stopped.","optional":true,"type":"array","items":{"$ref":"CallFrame"}},{"name":"stackChanged","description":"Whether current call stack  was modified after applying the changes.","optional":true,"type":"boolean"},{"name":"asyncStackTrace","description":"Async stack trace, if any.","optional":true,"$ref":"Runtime.StackTrace"},{"name":"asyncStackTraceId","description":"Async stack trace, if any.","experimental":true,"optional":true,"$ref":"Runtime.StackTraceId"},{"name":"exceptionDetails","description":"Exception details if any.","optional":true,"$ref":"Runtime.ExceptionDetails"}]},{"name":"setSkipAllPauses","description":"Makes page not interrupt on any pauses (breakpoint, exception, dom exception etc).","parameters":[{"name":"skip","description":"New value for skip pauses state.","type":"boolean"}]},{"name":"setVariableValue","description":"Changes value of variable in a callframe. Object-based scopes are not supported and must be\nmutated manually.","parameters":[{"name":"scopeNumber","description":"0-based number of scope as was listed in scope chain. Only 'local', 'closure' and 'catch'\nscope types are allowed. Other scopes could be manipulated manually.","type":"integer"},{"name":"variableName","description":"Variable name.","type":"string"},{"name":"newValue","description":"New variable value.","$ref":"Runtime.CallArgument"},{"name":"callFrameId","description":"Id of callframe that holds variable.","$ref":"CallFrameId"}]},{"name":"stepInto","description":"Steps into the function call.","parameters":[{"name":"breakOnAsyncCall","description":"Debugger will issue additional Debugger.paused notification if any async task is scheduled\nbefore next pause.","experimental":true,"optional":true,"type":"boolean"}]},{"name":"stepOut","description":"Steps out of the function call."},{"name":"stepOver","description":"Steps over the statement."}],"events":[{"name":"breakpointResolved","description":"Fired when breakpoint is resolved to an actual script and location.","parameters":[{"name":"breakpointId","description":"Breakpoint unique identifier.","$ref":"BreakpointId"},{"name":"location","description":"Actual breakpoint location.","$ref":"Location"}]},{"name":"paused","description":"Fired when the virtual machine stopped on breakpoint or exception or any other stop criteria.","parameters":[{"name":"callFrames","description":"Call stack the virtual machine stopped on.","type":"array","items":{"$ref":"CallFrame"}},{"name":"reason","description":"Pause reason.","type":"string","enum":["XHR","DOM","EventListener","exception","assert","debugCommand","promiseRejection","OOM","other","ambiguous"]},{"name":"data","description":"Object containing break-specific auxiliary properties.","optional":true,"type":"object"},{"name":"hitBreakpoints","description":"Hit breakpoints IDs","optional":true,"type":"array","items":{"type":"string"}},{"name":"asyncStackTrace","description":"Async stack trace, if any.","optional":true,"$ref":"Runtime.StackTrace"},{"name":"asyncStackTraceId","description":"Async stack trace, if any.","experimental":true,"optional":true,"$ref":"Runtime.StackTraceId"},{"name":"asyncCallStackTraceId","description":"Just scheduled async call will have this stack trace as parent stack during async execution.\nThis field is available only after `Debugger.stepInto` call with `breakOnAsynCall` flag.","experimental":true,"optional":true,"$ref":"Runtime.StackTraceId"}]},{"name":"resumed","description":"Fired when the virtual machine resumed execution."},{"name":"scriptFailedToParse","description":"Fired when virtual machine fails to parse the script.","parameters":[{"name":"scriptId","description":"Identifier of the script parsed.","$ref":"Runtime.ScriptId"},{"name":"url","description":"URL or name of the script parsed (if any).","type":"string"},{"name":"startLine","description":"Line offset of the script within the resource with given URL (for script tags).","type":"integer"},{"name":"startColumn","description":"Column offset of the script within the resource with given URL.","type":"integer"},{"name":"endLine","description":"Last line of the script.","type":"integer"},{"name":"endColumn","description":"Length of the last line of the script.","type":"integer"},{"name":"executionContextId","description":"Specifies script creation context.","$ref":"Runtime.ExecutionContextId"},{"name":"hash","description":"Content hash of the script.","type":"string"},{"name":"executionContextAuxData","description":"Embedder-specific auxiliary data.","optional":true,"type":"object"},{"name":"sourceMapURL","description":"URL of source map associated with script (if any).","optional":true,"type":"string"},{"name":"hasSourceURL","description":"True, if this script has sourceURL.","optional":true,"type":"boolean"},{"name":"isModule","description":"True, if this script is ES6 module.","optional":true,"type":"boolean"},{"name":"length","description":"This script length.","optional":true,"type":"integer"},{"name":"stackTrace","description":"JavaScript top stack frame of where the script parsed event was triggered if available.","experimental":true,"optional":true,"$ref":"Runtime.StackTrace"}]},{"name":"scriptParsed","description":"Fired when virtual machine parses script. This event is also fired for all known and uncollected\nscripts upon enabling debugger.","parameters":[{"name":"scriptId","description":"Identifier of the script parsed.","$ref":"Runtime.ScriptId"},{"name":"url","description":"URL or name of the script parsed (if any).","type":"string"},{"name":"startLine","description":"Line offset of the script within the resource with given URL (for script tags).","type":"integer"},{"name":"startColumn","description":"Column offset of the script within the resource with given URL.","type":"integer"},{"name":"endLine","description":"Last line of the script.","type":"integer"},{"name":"endColumn","description":"Length of the last line of the script.","type":"integer"},{"name":"executionContextId","description":"Specifies script creation context.","$ref":"Runtime.ExecutionContextId"},{"name":"hash","description":"Content hash of the script.","type":"string"},{"name":"executionContextAuxData","description":"Embedder-specific auxiliary data.","optional":true,"type":"object"},{"name":"isLiveEdit","description":"True, if this script is generated as a result of the live edit operation.","experimental":true,"optional":true,"type":"boolean"},{"name":"sourceMapURL","description":"URL of source map associated with script (if any).","optional":true,"type":"string"},{"name":"hasSourceURL","description":"True, if this script has sourceURL.","optional":true,"type":"boolean"},{"name":"isModule","description":"True, if this script is ES6 module.","optional":true,"type":"boolean"},{"name":"length","description":"This script length.","optional":true,"type":"integer"},{"name":"stackTrace","description":"JavaScript top stack frame of where the script parsed event was triggered if available.","experimental":true,"optional":true,"$ref":"Runtime.StackTrace"}]}]},{"domain":"HeapProfiler","experimental":true,"dependencies":["Runtime"],"types":[{"id":"HeapSnapshotObjectId","description":"Heap snapshot object id.","type":"string"},{"id":"SamplingHeapProfileNode","description":"Sampling Heap Profile node. Holds callsite information, allocation statistics and child nodes.","type":"object","properties":[{"name":"callFrame","description":"Function location.","$ref":"Runtime.CallFrame"},{"name":"selfSize","description":"Allocations size in bytes for the node excluding children.","type":"number"},{"name":"id","description":"Node id. Ids are unique across all profiles collected between startSampling and stopSampling.","type":"integer"},{"name":"children","description":"Child nodes.","type":"array","items":{"$ref":"SamplingHeapProfileNode"}}]},{"id":"SamplingHeapProfileSample","description":"A single sample from a sampling profile.","type":"object","properties":[{"name":"size","description":"Allocation size in bytes attributed to the sample.","type":"number"},{"name":"nodeId","description":"Id of the corresponding profile tree node.","type":"integer"},{"name":"ordinal","description":"Time-ordered sample ordinal number. It is unique across all profiles retrieved\nbetween startSampling and stopSampling.","type":"number"}]},{"id":"SamplingHeapProfile","description":"Sampling profile.","type":"object","properties":[{"name":"head","$ref":"SamplingHeapProfileNode"},{"name":"samples","type":"array","items":{"$ref":"SamplingHeapProfileSample"}}]}],"commands":[{"name":"addInspectedHeapObject","description":"Enables console to refer to the node with given id via $x (see Command Line API for more details\n$x functions).","parameters":[{"name":"heapObjectId","description":"Heap snapshot object id to be accessible by means of $x command line API.","$ref":"HeapSnapshotObjectId"}]},{"name":"collectGarbage"},{"name":"disable"},{"name":"enable"},{"name":"getHeapObjectId","parameters":[{"name":"objectId","description":"Identifier of the object to get heap object id for.","$ref":"Runtime.RemoteObjectId"}],"returns":[{"name":"heapSnapshotObjectId","description":"Id of the heap snapshot object corresponding to the passed remote object id.","$ref":"HeapSnapshotObjectId"}]},{"name":"getObjectByHeapObjectId","parameters":[{"name":"objectId","$ref":"HeapSnapshotObjectId"},{"name":"objectGroup","description":"Symbolic group name that can be used to release multiple objects.","optional":true,"type":"string"}],"returns":[{"name":"result","description":"Evaluation result.","$ref":"Runtime.RemoteObject"}]},{"name":"getSamplingProfile","returns":[{"name":"profile","description":"Return the sampling profile being collected.","$ref":"SamplingHeapProfile"}]},{"name":"startSampling","parameters":[{"name":"samplingInterval","description":"Average sample interval in bytes. Poisson distribution is used for the intervals. The\ndefault value is 32768 bytes.","optional":true,"type":"number"}]},{"name":"startTrackingHeapObjects","parameters":[{"name":"trackAllocations","optional":true,"type":"boolean"}]},{"name":"stopSampling","returns":[{"name":"profile","description":"Recorded sampling heap profile.","$ref":"SamplingHeapProfile"}]},{"name":"stopTrackingHeapObjects","parameters":[{"name":"reportProgress","description":"If true 'reportHeapSnapshotProgress' events will be generated while snapshot is being taken\nwhen the tracking is stopped.","optional":true,"type":"boolean"}]},{"name":"takeHeapSnapshot","parameters":[{"name":"reportProgress","description":"If true 'reportHeapSnapshotProgress' events will be generated while snapshot is being taken.","optional":true,"type":"boolean"}]}],"events":[{"name":"addHeapSnapshotChunk","parameters":[{"name":"chunk","type":"string"}]},{"name":"heapStatsUpdate","description":"If heap objects tracking has been started then backend may send update for one or more fragments","parameters":[{"name":"statsUpdate","description":"An array of triplets. Each triplet describes a fragment. The first integer is the fragment\nindex, the second integer is a total count of objects for the fragment, the third integer is\na total size of the objects for the fragment.","type":"array","items":{"type":"integer"}}]},{"name":"lastSeenObjectId","description":"If heap objects tracking has been started then backend regularly sends a current value for last\nseen object id and corresponding timestamp. If the were changes in the heap since last event\nthen one or more heapStatsUpdate events will be sent before a new lastSeenObjectId event.","parameters":[{"name":"lastSeenObjectId","type":"integer"},{"name":"timestamp","type":"number"}]},{"name":"reportHeapSnapshotProgress","parameters":[{"name":"done","type":"integer"},{"name":"total","type":"integer"},{"name":"finished","optional":true,"type":"boolean"}]},{"name":"resetProfiles"}]},{"domain":"Profiler","dependencies":["Runtime","Debugger"],"types":[{"id":"ProfileNode","description":"Profile node. Holds callsite information, execution statistics and child nodes.","type":"object","properties":[{"name":"id","description":"Unique id of the node.","type":"integer"},{"name":"callFrame","description":"Function location.","$ref":"Runtime.CallFrame"},{"name":"hitCount","description":"Number of samples where this node was on top of the call stack.","optional":true,"type":"integer"},{"name":"children","description":"Child node ids.","optional":true,"type":"array","items":{"type":"integer"}},{"name":"deoptReason","description":"The reason of being not optimized. The function may be deoptimized or marked as don't\noptimize.","optional":true,"type":"string"},{"name":"positionTicks","description":"An array of source position ticks.","optional":true,"type":"array","items":{"$ref":"PositionTickInfo"}}]},{"id":"Profile","description":"Profile.","type":"object","properties":[{"name":"nodes","description":"The list of profile nodes. First item is the root node.","type":"array","items":{"$ref":"ProfileNode"}},{"name":"startTime","description":"Profiling start timestamp in microseconds.","type":"number"},{"name":"endTime","description":"Profiling end timestamp in microseconds.","type":"number"},{"name":"samples","description":"Ids of samples top nodes.","optional":true,"type":"array","items":{"type":"integer"}},{"name":"timeDeltas","description":"Time intervals between adjacent samples in microseconds. The first delta is relative to the\nprofile startTime.","optional":true,"type":"array","items":{"type":"integer"}}]},{"id":"PositionTickInfo","description":"Specifies a number of samples attributed to a certain source position.","type":"object","properties":[{"name":"line","description":"Source line number (1-based).","type":"integer"},{"name":"ticks","description":"Number of samples attributed to the source line.","type":"integer"}]},{"id":"CoverageRange","description":"Coverage data for a source range.","type":"object","properties":[{"name":"startOffset","description":"JavaScript script source offset for the range start.","type":"integer"},{"name":"endOffset","description":"JavaScript script source offset for the range end.","type":"integer"},{"name":"count","description":"Collected execution count of the source range.","type":"integer"}]},{"id":"FunctionCoverage","description":"Coverage data for a JavaScript function.","type":"object","properties":[{"name":"functionName","description":"JavaScript function name.","type":"string"},{"name":"ranges","description":"Source ranges inside the function with coverage data.","type":"array","items":{"$ref":"CoverageRange"}},{"name":"isBlockCoverage","description":"Whether coverage data for this function has block granularity.","type":"boolean"}]},{"id":"ScriptCoverage","description":"Coverage data for a JavaScript script.","type":"object","properties":[{"name":"scriptId","description":"JavaScript script id.","$ref":"Runtime.ScriptId"},{"name":"url","description":"JavaScript script name or url.","type":"string"},{"name":"functions","description":"Functions contained in the script that has coverage data.","type":"array","items":{"$ref":"FunctionCoverage"}}]},{"id":"TypeObject","description":"Describes a type collected during runtime.","experimental":true,"type":"object","properties":[{"name":"name","description":"Name of a type collected with type profiling.","type":"string"}]},{"id":"TypeProfileEntry","description":"Source offset and types for a parameter or return value.","experimental":true,"type":"object","properties":[{"name":"offset","description":"Source offset of the parameter or end of function for return values.","type":"integer"},{"name":"types","description":"The types for this parameter or return value.","type":"array","items":{"$ref":"TypeObject"}}]},{"id":"ScriptTypeProfile","description":"Type profile data collected during runtime for a JavaScript script.","experimental":true,"type":"object","properties":[{"name":"scriptId","description":"JavaScript script id.","$ref":"Runtime.ScriptId"},{"name":"url","description":"JavaScript script name or url.","type":"string"},{"name":"entries","description":"Type profile entries for parameters and return values of the functions in the script.","type":"array","items":{"$ref":"TypeProfileEntry"}}]}],"commands":[{"name":"disable"},{"name":"enable"},{"name":"getBestEffortCoverage","description":"Collect coverage data for the current isolate. The coverage data may be incomplete due to\ngarbage collection.","returns":[{"name":"result","description":"Coverage data for the current isolate.","type":"array","items":{"$ref":"ScriptCoverage"}}]},{"name":"setSamplingInterval","description":"Changes CPU profiler sampling interval. Must be called before CPU profiles recording started.","parameters":[{"name":"interval","description":"New sampling interval in microseconds.","type":"integer"}]},{"name":"start"},{"name":"startPreciseCoverage","description":"Enable precise code coverage. Coverage data for JavaScript executed before enabling precise code\ncoverage may be incomplete. Enabling prevents running optimized code and resets execution\ncounters.","parameters":[{"name":"callCount","description":"Collect accurate call counts beyond simple 'covered' or 'not covered'.","optional":true,"type":"boolean"},{"name":"detailed","description":"Collect block-based coverage.","optional":true,"type":"boolean"}]},{"name":"startTypeProfile","description":"Enable type profile.","experimental":true},{"name":"stop","returns":[{"name":"profile","description":"Recorded profile.","$ref":"Profile"}]},{"name":"stopPreciseCoverage","description":"Disable precise code coverage. Disabling releases unnecessary execution count records and allows\nexecuting optimized code."},{"name":"stopTypeProfile","description":"Disable type profile. Disabling releases type profile data collected so far.","experimental":true},{"name":"takePreciseCoverage","description":"Collect coverage data for the current isolate, and resets execution counters. Precise code\ncoverage needs to have started.","returns":[{"name":"result","description":"Coverage data for the current isolate.","type":"array","items":{"$ref":"ScriptCoverage"}}]},{"name":"takeTypeProfile","description":"Collect type profile.","experimental":true,"returns":[{"name":"result","description":"Type profile for all scripts since startTypeProfile() was turned on.","type":"array","items":{"$ref":"ScriptTypeProfile"}}]}],"events":[{"name":"consoleProfileFinished","parameters":[{"name":"id","type":"string"},{"name":"location","description":"Location of console.profileEnd().","$ref":"Debugger.Location"},{"name":"profile","$ref":"Profile"},{"name":"title","description":"Profile title passed as an argument to console.profile().","optional":true,"type":"string"}]},{"name":"consoleProfileStarted","description":"Sent when new profile recording is started using console.profile() call.","parameters":[{"name":"id","type":"string"},{"name":"location","description":"Location of console.profile().","$ref":"Debugger.Location"},{"name":"title","description":"Profile title passed as an argument to console.profile().","optional":true,"type":"string"}]}]},{"domain":"Runtime","description":"Runtime domain exposes JavaScript runtime by means of remote evaluation and mirror objects.\nEvaluation results are returned as mirror object that expose object type, string representation\nand unique identifier that can be used for further object reference. Original objects are\nmaintained in memory unless they are either explicitly released or are released along with the\nother objects in their object group.","types":[{"id":"ScriptId","description":"Unique script identifier.","type":"string"},{"id":"RemoteObjectId","description":"Unique object identifier.","type":"string"},{"id":"UnserializableValue","description":"Primitive value which cannot be JSON-stringified. Includes values `-0`, `NaN`, `Infinity`,\n`-Infinity`, and bigint literals.","type":"string"},{"id":"RemoteObject","description":"Mirror object referencing original JavaScript object.","type":"object","properties":[{"name":"type","description":"Object type.","type":"string","enum":["object","function","undefined","string","number","boolean","symbol","bigint"]},{"name":"subtype","description":"Object subtype hint. Specified for `object` type values only.","optional":true,"type":"string","enum":["array","null","node","regexp","date","map","set","weakmap","weakset","iterator","generator","error","proxy","promise","typedarray","arraybuffer","dataview"]},{"name":"className","description":"Object class (constructor) name. Specified for `object` type values only.","optional":true,"type":"string"},{"name":"value","description":"Remote object value in case of primitive values or JSON values (if it was requested).","optional":true,"type":"any"},{"name":"unserializableValue","description":"Primitive value which can not be JSON-stringified does not have `value`, but gets this\nproperty.","optional":true,"$ref":"UnserializableValue"},{"name":"description","description":"String representation of the object.","optional":true,"type":"string"},{"name":"objectId","description":"Unique object identifier (for non-primitive values).","optional":true,"$ref":"RemoteObjectId"},{"name":"preview","description":"Preview containing abbreviated property values. Specified for `object` type values only.","experimental":true,"optional":true,"$ref":"ObjectPreview"},{"name":"customPreview","experimental":true,"optional":true,"$ref":"CustomPreview"}]},{"id":"CustomPreview","experimental":true,"type":"object","properties":[{"name":"header","description":"The JSON-stringified result of formatter.header(object, config) call.\nIt contains json ML array that represents RemoteObject.","type":"string"},{"name":"bodyGetterId","description":"If formatter returns true as a result of formatter.hasBody call then bodyGetterId will\ncontain RemoteObjectId for the function that returns result of formatter.body(object, config) call.\nThe result value is json ML array.","optional":true,"$ref":"RemoteObjectId"}]},{"id":"ObjectPreview","description":"Object containing abbreviated remote object value.","experimental":true,"type":"object","properties":[{"name":"type","description":"Object type.","type":"string","enum":["object","function","undefined","string","number","boolean","symbol","bigint"]},{"name":"subtype","description":"Object subtype hint. Specified for `object` type values only.","optional":true,"type":"string","enum":["array","null","node","regexp","date","map","set","weakmap","weakset","iterator","generator","error"]},{"name":"description","description":"String representation of the object.","optional":true,"type":"string"},{"name":"overflow","description":"True iff some of the properties or entries of the original object did not fit.","type":"boolean"},{"name":"properties","description":"List of the properties.","type":"array","items":{"$ref":"PropertyPreview"}},{"name":"entries","description":"List of the entries. Specified for `map` and `set` subtype values only.","optional":true,"type":"array","items":{"$ref":"EntryPreview"}}]},{"id":"PropertyPreview","experimental":true,"type":"object","properties":[{"name":"name","description":"Property name.","type":"string"},{"name":"type","description":"Object type. Accessor means that the property itself is an accessor property.","type":"string","enum":["object","function","undefined","string","number","boolean","symbol","accessor","bigint"]},{"name":"value","description":"User-friendly property value string.","optional":true,"type":"string"},{"name":"valuePreview","description":"Nested value preview.","optional":true,"$ref":"ObjectPreview"},{"name":"subtype","description":"Object subtype hint. Specified for `object` type values only.","optional":true,"type":"string","enum":["array","null","node","regexp","date","map","set","weakmap","weakset","iterator","generator","error"]}]},{"id":"EntryPreview","experimental":true,"type":"object","properties":[{"name":"key","description":"Preview of the key. Specified for map-like collection entries.","optional":true,"$ref":"ObjectPreview"},{"name":"value","description":"Preview of the value.","$ref":"ObjectPreview"}]},{"id":"PropertyDescriptor","description":"Object property descriptor.","type":"object","properties":[{"name":"name","description":"Property name or symbol description.","type":"string"},{"name":"value","description":"The value associated with the property.","optional":true,"$ref":"RemoteObject"},{"name":"writable","description":"True if the value associated with the property may be changed (data descriptors only).","optional":true,"type":"boolean"},{"name":"get","description":"A function which serves as a getter for the property, or `undefined` if there is no getter\n(accessor descriptors only).","optional":true,"$ref":"RemoteObject"},{"name":"set","description":"A function which serves as a setter for the property, or `undefined` if there is no setter\n(accessor descriptors only).","optional":true,"$ref":"RemoteObject"},{"name":"configurable","description":"True if the type of this property descriptor may be changed and if the property may be\ndeleted from the corresponding object.","type":"boolean"},{"name":"enumerable","description":"True if this property shows up during enumeration of the properties on the corresponding\nobject.","type":"boolean"},{"name":"wasThrown","description":"True if the result was thrown during the evaluation.","optional":true,"type":"boolean"},{"name":"isOwn","description":"True if the property is owned for the object.","optional":true,"type":"boolean"},{"name":"symbol","description":"Property symbol object, if the property is of the `symbol` type.","optional":true,"$ref":"RemoteObject"}]},{"id":"InternalPropertyDescriptor","description":"Object internal property descriptor. This property isn't normally visible in JavaScript code.","type":"object","properties":[{"name":"name","description":"Conventional property name.","type":"string"},{"name":"value","description":"The value associated with the property.","optional":true,"$ref":"RemoteObject"}]},{"id":"CallArgument","description":"Represents function call argument. Either remote object id `objectId`, primitive `value`,\nunserializable primitive value or neither of (for undefined) them should be specified.","type":"object","properties":[{"name":"value","description":"Primitive value or serializable javascript object.","optional":true,"type":"any"},{"name":"unserializableValue","description":"Primitive value which can not be JSON-stringified.","optional":true,"$ref":"UnserializableValue"},{"name":"objectId","description":"Remote object handle.","optional":true,"$ref":"RemoteObjectId"}]},{"id":"ExecutionContextId","description":"Id of an execution context.","type":"integer"},{"id":"ExecutionContextDescription","description":"Description of an isolated world.","type":"object","properties":[{"name":"id","description":"Unique id of the execution context. It can be used to specify in which execution context\nscript evaluation should be performed.","$ref":"ExecutionContextId"},{"name":"origin","description":"Execution context origin.","type":"string"},{"name":"name","description":"Human readable name describing given context.","type":"string"},{"name":"auxData","description":"Embedder-specific auxiliary data.","optional":true,"type":"object"}]},{"id":"ExceptionDetails","description":"Detailed information about exception (or error) that was thrown during script compilation or\nexecution.","type":"object","properties":[{"name":"exceptionId","description":"Exception id.","type":"integer"},{"name":"text","description":"Exception text, which should be used together with exception object when available.","type":"string"},{"name":"lineNumber","description":"Line number of the exception location (0-based).","type":"integer"},{"name":"columnNumber","description":"Column number of the exception location (0-based).","type":"integer"},{"name":"scriptId","description":"Script ID of the exception location.","optional":true,"$ref":"ScriptId"},{"name":"url","description":"URL of the exception location, to be used when the script was not reported.","optional":true,"type":"string"},{"name":"stackTrace","description":"JavaScript stack trace if available.","optional":true,"$ref":"StackTrace"},{"name":"exception","description":"Exception object if available.","optional":true,"$ref":"RemoteObject"},{"name":"executionContextId","description":"Identifier of the context where exception happened.","optional":true,"$ref":"ExecutionContextId"}]},{"id":"Timestamp","description":"Number of milliseconds since epoch.","type":"number"},{"id":"TimeDelta","description":"Number of milliseconds.","type":"number"},{"id":"CallFrame","description":"Stack entry for runtime errors and assertions.","type":"object","properties":[{"name":"functionName","description":"JavaScript function name.","type":"string"},{"name":"scriptId","description":"JavaScript script id.","$ref":"ScriptId"},{"name":"url","description":"JavaScript script name or url.","type":"string"},{"name":"lineNumber","description":"JavaScript script line number (0-based).","type":"integer"},{"name":"columnNumber","description":"JavaScript script column number (0-based).","type":"integer"}]},{"id":"StackTrace","description":"Call frames for assertions or error messages.","type":"object","properties":[{"name":"description","description":"String label of this stack trace. For async traces this may be a name of the function that\ninitiated the async call.","optional":true,"type":"string"},{"name":"callFrames","description":"JavaScript function name.","type":"array","items":{"$ref":"CallFrame"}},{"name":"parent","description":"Asynchronous JavaScript stack trace that preceded this stack, if available.","optional":true,"$ref":"StackTrace"},{"name":"parentId","description":"Asynchronous JavaScript stack trace that preceded this stack, if available.","experimental":true,"optional":true,"$ref":"StackTraceId"}]},{"id":"UniqueDebuggerId","description":"Unique identifier of current debugger.","experimental":true,"type":"string"},{"id":"StackTraceId","description":"If `debuggerId` is set stack trace comes from another debugger and can be resolved there. This\nallows to track cross-debugger calls. See `Runtime.StackTrace` and `Debugger.paused` for usages.","experimental":true,"type":"object","properties":[{"name":"id","type":"string"},{"name":"debuggerId","optional":true,"$ref":"UniqueDebuggerId"}]}],"commands":[{"name":"awaitPromise","description":"Add handler to promise with given promise object id.","parameters":[{"name":"promiseObjectId","description":"Identifier of the promise.","$ref":"RemoteObjectId"},{"name":"returnByValue","description":"Whether the result is expected to be a JSON object that should be sent by value.","optional":true,"type":"boolean"},{"name":"generatePreview","description":"Whether preview should be generated for the result.","optional":true,"type":"boolean"}],"returns":[{"name":"result","description":"Promise result. Will contain rejected value if promise was rejected.","$ref":"RemoteObject"},{"name":"exceptionDetails","description":"Exception details if stack strace is available.","optional":true,"$ref":"ExceptionDetails"}]},{"name":"callFunctionOn","description":"Calls function with given declaration on the given object. Object group of the result is\ninherited from the target object.","parameters":[{"name":"functionDeclaration","description":"Declaration of the function to call.","type":"string"},{"name":"objectId","description":"Identifier of the object to call function on. Either objectId or executionContextId should\nbe specified.","optional":true,"$ref":"RemoteObjectId"},{"name":"arguments","description":"Call arguments. All call arguments must belong to the same JavaScript world as the target\nobject.","optional":true,"type":"array","items":{"$ref":"CallArgument"}},{"name":"silent","description":"In silent mode exceptions thrown during evaluation are not reported and do not pause\nexecution. Overrides `setPauseOnException` state.","optional":true,"type":"boolean"},{"name":"returnByValue","description":"Whether the result is expected to be a JSON object which should be sent by value.","optional":true,"type":"boolean"},{"name":"generatePreview","description":"Whether preview should be generated for the result.","experimental":true,"optional":true,"type":"boolean"},{"name":"userGesture","description":"Whether execution should be treated as initiated by user in the UI.","optional":true,"type":"boolean"},{"name":"awaitPromise","description":"Whether execution should `await` for resulting value and return once awaited promise is\nresolved.","optional":true,"type":"boolean"},{"name":"executionContextId","description":"Specifies execution context which global object will be used to call function on. Either\nexecutionContextId or objectId should be specified.","optional":true,"$ref":"ExecutionContextId"},{"name":"objectGroup","description":"Symbolic group name that can be used to release multiple objects. If objectGroup is not\nspecified and objectId is, objectGroup will be inherited from object.","optional":true,"type":"string"}],"returns":[{"name":"result","description":"Call result.","$ref":"RemoteObject"},{"name":"exceptionDetails","description":"Exception details.","optional":true,"$ref":"ExceptionDetails"}]},{"name":"compileScript","description":"Compiles expression.","parameters":[{"name":"expression","description":"Expression to compile.","type":"string"},{"name":"sourceURL","description":"Source url to be set for the script.","type":"string"},{"name":"persistScript","description":"Specifies whether the compiled script should be persisted.","type":"boolean"},{"name":"executionContextId","description":"Specifies in which execution context to perform script run. If the parameter is omitted the\nevaluation will be performed in the context of the inspected page.","optional":true,"$ref":"ExecutionContextId"}],"returns":[{"name":"scriptId","description":"Id of the script.","optional":true,"$ref":"ScriptId"},{"name":"exceptionDetails","description":"Exception details.","optional":true,"$ref":"ExceptionDetails"}]},{"name":"disable","description":"Disables reporting of execution contexts creation."},{"name":"discardConsoleEntries","description":"Discards collected exceptions and console API calls."},{"name":"enable","description":"Enables reporting of execution contexts creation by means of `executionContextCreated` event.\nWhen the reporting gets enabled the event will be sent immediately for each existing execution\ncontext."},{"name":"evaluate","description":"Evaluates expression on global object.","parameters":[{"name":"expression","description":"Expression to evaluate.","type":"string"},{"name":"objectGroup","description":"Symbolic group name that can be used to release multiple objects.","optional":true,"type":"string"},{"name":"includeCommandLineAPI","description":"Determines whether Command Line API should be available during the evaluation.","optional":true,"type":"boolean"},{"name":"silent","description":"In silent mode exceptions thrown during evaluation are not reported and do not pause\nexecution. Overrides `setPauseOnException` state.","optional":true,"type":"boolean"},{"name":"contextId","description":"Specifies in which execution context to perform evaluation. If the parameter is omitted the\nevaluation will be performed in the context of the inspected page.","optional":true,"$ref":"ExecutionContextId"},{"name":"returnByValue","description":"Whether the result is expected to be a JSON object that should be sent by value.","optional":true,"type":"boolean"},{"name":"generatePreview","description":"Whether preview should be generated for the result.","experimental":true,"optional":true,"type":"boolean"},{"name":"userGesture","description":"Whether execution should be treated as initiated by user in the UI.","optional":true,"type":"boolean"},{"name":"awaitPromise","description":"Whether execution should `await` for resulting value and return once awaited promise is\nresolved.","optional":true,"type":"boolean"},{"name":"throwOnSideEffect","description":"Whether to throw an exception if side effect cannot be ruled out during evaluation.","experimental":true,"optional":true,"type":"boolean"},{"name":"timeout","description":"Terminate execution after timing out (number of milliseconds).","experimental":true,"optional":true,"$ref":"TimeDelta"}],"returns":[{"name":"result","description":"Evaluation result.","$ref":"RemoteObject"},{"name":"exceptionDetails","description":"Exception details.","optional":true,"$ref":"ExceptionDetails"}]},{"name":"getIsolateId","description":"Returns the isolate id.","experimental":true,"returns":[{"name":"id","description":"The isolate id.","type":"string"}]},{"name":"getHeapUsage","description":"Returns the JavaScript heap usage.\nIt is the total usage of the corresponding isolate not scoped to a particular Runtime.","experimental":true,"returns":[{"name":"usedSize","description":"Used heap size in bytes.","type":"number"},{"name":"totalSize","description":"Allocated heap size in bytes.","type":"number"}]},{"name":"getProperties","description":"Returns properties of a given object. Object group of the result is inherited from the target\nobject.","parameters":[{"name":"objectId","description":"Identifier of the object to return properties for.","$ref":"RemoteObjectId"},{"name":"ownProperties","description":"If true, returns properties belonging only to the element itself, not to its prototype\nchain.","optional":true,"type":"boolean"},{"name":"accessorPropertiesOnly","description":"If true, returns accessor properties (with getter/setter) only; internal properties are not\nreturned either.","experimental":true,"optional":true,"type":"boolean"},{"name":"generatePreview","description":"Whether preview should be generated for the results.","experimental":true,"optional":true,"type":"boolean"}],"returns":[{"name":"result","description":"Object properties.","type":"array","items":{"$ref":"PropertyDescriptor"}},{"name":"internalProperties","description":"Internal object properties (only of the element itself).","optional":true,"type":"array","items":{"$ref":"InternalPropertyDescriptor"}},{"name":"exceptionDetails","description":"Exception details.","optional":true,"$ref":"ExceptionDetails"}]},{"name":"globalLexicalScopeNames","description":"Returns all let, const and class variables from global scope.","parameters":[{"name":"executionContextId","description":"Specifies in which execution context to lookup global scope variables.","optional":true,"$ref":"ExecutionContextId"}],"returns":[{"name":"names","type":"array","items":{"type":"string"}}]},{"name":"queryObjects","parameters":[{"name":"prototypeObjectId","description":"Identifier of the prototype to return objects for.","$ref":"RemoteObjectId"},{"name":"objectGroup","description":"Symbolic group name that can be used to release the results.","optional":true,"type":"string"}],"returns":[{"name":"objects","description":"Array with objects.","$ref":"RemoteObject"}]},{"name":"releaseObject","description":"Releases remote object with given id.","parameters":[{"name":"objectId","description":"Identifier of the object to release.","$ref":"RemoteObjectId"}]},{"name":"releaseObjectGroup","description":"Releases all remote objects that belong to a given group.","parameters":[{"name":"objectGroup","description":"Symbolic object group name.","type":"string"}]},{"name":"runIfWaitingForDebugger","description":"Tells inspected instance to run if it was waiting for debugger to attach."},{"name":"runScript","description":"Runs script with given id in a given context.","parameters":[{"name":"scriptId","description":"Id of the script to run.","$ref":"ScriptId"},{"name":"executionContextId","description":"Specifies in which execution context to perform script run. If the parameter is omitted the\nevaluation will be performed in the context of the inspected page.","optional":true,"$ref":"ExecutionContextId"},{"name":"objectGroup","description":"Symbolic group name that can be used to release multiple objects.","optional":true,"type":"string"},{"name":"silent","description":"In silent mode exceptions thrown during evaluation are not reported and do not pause\nexecution. Overrides `setPauseOnException` state.","optional":true,"type":"boolean"},{"name":"includeCommandLineAPI","description":"Determines whether Command Line API should be available during the evaluation.","optional":true,"type":"boolean"},{"name":"returnByValue","description":"Whether the result is expected to be a JSON object which should be sent by value.","optional":true,"type":"boolean"},{"name":"generatePreview","description":"Whether preview should be generated for the result.","optional":true,"type":"boolean"},{"name":"awaitPromise","description":"Whether execution should `await` for resulting value and return once awaited promise is\nresolved.","optional":true,"type":"boolean"}],"returns":[{"name":"result","description":"Run result.","$ref":"RemoteObject"},{"name":"exceptionDetails","description":"Exception details.","optional":true,"$ref":"ExceptionDetails"}]},{"name":"setAsyncCallStackDepth","description":"Enables or disables async call stacks tracking.","redirect":"Debugger","parameters":[{"name":"maxDepth","description":"Maximum depth of async call stacks. Setting to `0` will effectively disable collecting async\ncall stacks (default).","type":"integer"}]},{"name":"setCustomObjectFormatterEnabled","experimental":true,"parameters":[{"name":"enabled","type":"boolean"}]},{"name":"setMaxCallStackSizeToCapture","experimental":true,"parameters":[{"name":"size","type":"integer"}]},{"name":"terminateExecution","description":"Terminate current or next JavaScript execution.\nWill cancel the termination when the outer-most script execution ends.","experimental":true},{"name":"addBinding","description":"If executionContextId is empty, adds binding with the given name on the\nglobal objects of all inspected contexts, including those created later,\nbindings survive reloads.\nIf executionContextId is specified, adds binding only on global object of\ngiven execution context.\nBinding function takes exactly one argument, this argument should be string,\nin case of any other input, function throws an exception.\nEach binding function call produces Runtime.bindingCalled notification.","experimental":true,"parameters":[{"name":"name","type":"string"},{"name":"executionContextId","optional":true,"$ref":"ExecutionContextId"}]},{"name":"removeBinding","description":"This method does not remove binding function from global object but\nunsubscribes current runtime agent from Runtime.bindingCalled notifications.","experimental":true,"parameters":[{"name":"name","type":"string"}]}],"events":[{"name":"bindingCalled","description":"Notification is issued every time when binding is called.","experimental":true,"parameters":[{"name":"name","type":"string"},{"name":"payload","type":"string"},{"name":"executionContextId","description":"Identifier of the context where the call was made.","$ref":"ExecutionContextId"}]},{"name":"consoleAPICalled","description":"Issued when console API was called.","parameters":[{"name":"type","description":"Type of the call.","type":"string","enum":["log","debug","info","error","warning","dir","dirxml","table","trace","clear","startGroup","startGroupCollapsed","endGroup","assert","profile","profileEnd","count","timeEnd"]},{"name":"args","description":"Call arguments.","type":"array","items":{"$ref":"RemoteObject"}},{"name":"executionContextId","description":"Identifier of the context where the call was made.","$ref":"ExecutionContextId"},{"name":"timestamp","description":"Call timestamp.","$ref":"Timestamp"},{"name":"stackTrace","description":"Stack trace captured when the call was made.","optional":true,"$ref":"StackTrace"},{"name":"context","description":"Console context descriptor for calls on non-default console context (not console.*):\n'anonymous#unique-logger-id' for call on unnamed context, 'name#unique-logger-id' for call\non named context.","experimental":true,"optional":true,"type":"string"}]},{"name":"exceptionRevoked","description":"Issued when unhandled exception was revoked.","parameters":[{"name":"reason","description":"Reason describing why exception was revoked.","type":"string"},{"name":"exceptionId","description":"The id of revoked exception, as reported in `exceptionThrown`.","type":"integer"}]},{"name":"exceptionThrown","description":"Issued when exception was thrown and unhandled.","parameters":[{"name":"timestamp","description":"Timestamp of the exception.","$ref":"Timestamp"},{"name":"exceptionDetails","$ref":"ExceptionDetails"}]},{"name":"executionContextCreated","description":"Issued when new execution context is created.","parameters":[{"name":"context","description":"A newly created execution context.","$ref":"ExecutionContextDescription"}]},{"name":"executionContextDestroyed","description":"Issued when execution context is destroyed.","parameters":[{"name":"executionContextId","description":"Id of the destroyed context","$ref":"ExecutionContextId"}]},{"name":"executionContextsCleared","description":"Issued when all executionContexts were cleared in browser"},{"name":"inspectRequested","description":"Issued when object should be inspected (for example, as a result of inspect() command line API\ncall).","parameters":[{"name":"object","$ref":"RemoteObject"},{"name":"hints","type":"object"}]}]},{"domain":"Schema","description":"This domain is deprecated.","deprecated":true,"types":[{"id":"Domain","description":"Description of the protocol domain.","type":"object","properties":[{"name":"name","description":"Domain name.","type":"string"},{"name":"version","description":"Domain version.","type":"string"}]}],"commands":[{"name":"getDomains","description":"Returns supported domains.","returns":[{"name":"domains","description":"List of supported domains.","type":"array","items":{"$ref":"Domain"}}]}]}]}

/***/ }),
/* 372 */
/***/ (function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(process) {'use strict';

	var _typeof = typeof Symbol === "function" && typeof Symbol.iterator === "symbol" ? function (obj) { return typeof obj; } : function (obj) { return obj && typeof Symbol === "function" && obj.constructor === Symbol && obj !== Symbol.prototype ? "symbol" : typeof obj; };

	var _createClass = function () { function defineProperties(target, props) { for (var i = 0; i < props.length; i++) { var descriptor = props[i]; descriptor.enumerable = descriptor.enumerable || false; descriptor.configurable = true; if ("value" in descriptor) descriptor.writable = true; Object.defineProperty(target, descriptor.key, descriptor); } } return function (Constructor, protoProps, staticProps) { if (protoProps) defineProperties(Constructor.prototype, protoProps); if (staticProps) defineProperties(Constructor, staticProps); return Constructor; }; }();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } }

	function _possibleConstructorReturn(self, call) { if (!self) { throw new ReferenceError("this hasn't been initialised - super() hasn't been called"); } return call && (typeof call === "object" || typeof call === "function") ? call : self; }

	function _inherits(subClass, superClass) { if (typeof superClass !== "function" && superClass !== null) { throw new TypeError("Super expression must either be null or a function, not " + typeof superClass); } subClass.prototype = Object.create(superClass && superClass.prototype, { constructor: { value: subClass, enumerable: false, writable: true, configurable: true } }); if (superClass) Object.setPrototypeOf ? Object.setPrototypeOf(subClass, superClass) : subClass.__proto__ = superClass; }

	var EventEmitter = __webpack_require__(329);
	var util = __webpack_require__(373);
	var parseUrl = __webpack_require__(361).parse;

	var WebSocket = __webpack_require__(375);

	var api = __webpack_require__(376);
	var defaults = __webpack_require__(369);
	var devtools = __webpack_require__(330);

	var ProtocolError = function (_Error) {
	    _inherits(ProtocolError, _Error);

	    function ProtocolError(request, response) {
	        _classCallCheck(this, ProtocolError);

	        var message = response.message;

	        if (response.data) {
	            message += ' (' + response.data + ')';
	        }

	        // attach the original response as well
	        var _this = _possibleConstructorReturn(this, (ProtocolError.__proto__ || Object.getPrototypeOf(ProtocolError)).call(this, message));

	        _this.request = request;
	        _this.response = response;
	        return _this;
	    }

	    return ProtocolError;
	}(Error);

	var Chrome = function (_EventEmitter) {
	    _inherits(Chrome, _EventEmitter);

	    function Chrome(options, notifier) {
	        _classCallCheck(this, Chrome);

	        // options
	        var _this2 = _possibleConstructorReturn(this, (Chrome.__proto__ || Object.getPrototypeOf(Chrome)).call(this));

	        var defaultTarget = function defaultTarget(targets) {
	            // prefer type = 'page' inspectabe targets as they represents
	            // browser tabs (fall back to the first instectable target
	            // otherwise)
	            var backup = void 0;
	            var target = targets.find(function (target) {
	                if (target.webSocketDebuggerUrl) {
	                    backup = backup || target;
	                    return target.type === 'page';
	                } else {
	                    return false;
	                }
	            });
	            target = target || backup;
	            if (target) {
	                return target;
	            } else {
	                throw new Error('No inspectable targets');
	            }
	        };
	        options = options || {};
	        _this2.host = options.host || defaults.HOST;
	        _this2.port = options.port || defaults.PORT;
	        _this2.secure = !!options.secure;
	        _this2.useHostName = !!options.useHostName;
	        _this2.protocol = options.protocol;
	        _this2.local = !!options.local;
	        _this2.target = options.target || defaultTarget;
	        // locals
	        _this2._notifier = notifier;
	        _this2._callbacks = {};
	        _this2._nextCommandId = 1;
	        // properties
	        _this2.webSocketUrl = undefined;
	        // operations
	        _this2._start();
	        return _this2;
	    }

	    // avoid misinterpreting protocol's members as custom util.inspect functions


	    _createClass(Chrome, [{
	        key: 'inspect',
	        value: function inspect(depth, options) {
	            options.customInspect = false;
	            return util.inspect(this, options);
	        }
	    }, {
	        key: 'send',
	        value: function send(method, params, callback) {
	            var _this3 = this;

	            if (typeof params === 'function') {
	                callback = params;
	                params = undefined;
	            }
	            // return a promise when a callback is not provided
	            if (typeof callback === 'function') {
	                this._enqueueCommand(method, params, callback);
	                return undefined;
	            } else {
	                return new Promise(function (fulfill, reject) {
	                    _this3._enqueueCommand(method, params, function (error, response) {
	                        if (error) {
	                            var request = { method: method, params: params };
	                            reject(error instanceof Error ? error // low-level WebSocket error
	                            : new ProtocolError(request, response));
	                        } else {
	                            fulfill(response);
	                        }
	                    });
	                });
	            }
	        }
	    }, {
	        key: 'close',
	        value: function close(callback) {
	            var _this4 = this;

	            var closeWebSocket = function closeWebSocket(callback) {
	                // don't close if it's already closed
	                if (_this4._ws.readyState === 3) {
	                    callback();
	                } else {
	                    // don't notify on user-initiated shutdown ('disconnect' event)
	                    _this4._ws.removeAllListeners('close');
	                    _this4._ws.once('close', function () {
	                        _this4._ws.removeAllListeners();
	                        callback();
	                    });
	                    _this4._ws.close();
	                }
	            };
	            if (typeof callback === 'function') {
	                closeWebSocket(callback);
	                return undefined;
	            } else {
	                return new Promise(function (fulfill, reject) {
	                    closeWebSocket(fulfill);
	                });
	            }
	        }

	        // initiate the connection process

	    }, {
	        key: '_start',
	        value: function () {
	            var _ref = _asyncToGenerator( /*#__PURE__*/regeneratorRuntime.mark(function _callee() {
	                var _this5 = this;

	                var options, url, urlObject, protocol;
	                return regeneratorRuntime.wrap(function _callee$(_context) {
	                    while (1) {
	                        switch (_context.prev = _context.next) {
	                            case 0:
	                                options = {
	                                    host: this.host,
	                                    port: this.port,
	                                    secure: this.secure,
	                                    useHostName: this.useHostName
	                                };
	                                _context.prev = 1;
	                                _context.next = 4;
	                                return this._fetchDebuggerURL(options);

	                            case 4:
	                                url = _context.sent;

	                                this.webSocketUrl = url;
	                                // update the connection parameters using the debugging URL
	                                urlObject = parseUrl(url);

	                                options.host = urlObject.hostname;
	                                options.port = urlObject.port || options.port;
	                                // fetch the protocol and prepare the API
	                                _context.next = 11;
	                                return this._fetchProtocol(options);

	                            case 11:
	                                protocol = _context.sent;

	                                api.prepare(this, protocol);
	                                // finally connect to the WebSocket
	                                _context.next = 15;
	                                return this._connectToWebSocket();

	                            case 15:
	                                // since the handler is executed synchronously, the emit() must be
	                                // performed in the next tick so that uncaught errors in the client code
	                                // are not intercepted by the Promise mechanism and therefore reported
	                                // via the 'error' event
	                                process.nextTick(function () {
	                                    _this5._notifier.emit('connect', _this5);
	                                });
	                                _context.next = 21;
	                                break;

	                            case 18:
	                                _context.prev = 18;
	                                _context.t0 = _context['catch'](1);

	                                this._notifier.emit('error', _context.t0);

	                            case 21:
	                            case 'end':
	                                return _context.stop();
	                        }
	                    }
	                }, _callee, this, [[1, 18]]);
	            }));

	            function _start() {
	                return _ref.apply(this, arguments);
	            }

	            return _start;
	        }()

	        // fetch the WebSocket URL according to 'target'

	    }, {
	        key: '_fetchDebuggerURL',
	        value: function () {
	            var _ref2 = _asyncToGenerator( /*#__PURE__*/regeneratorRuntime.mark(function _callee2(options) {
	                var userTarget, idOrUrl, targets, object, _object, func, _targets, result, _object2;

	                return regeneratorRuntime.wrap(function _callee2$(_context2) {
	                    while (1) {
	                        switch (_context2.prev = _context2.next) {
	                            case 0:
	                                userTarget = this.target;
	                                _context2.t0 = typeof userTarget === 'undefined' ? 'undefined' : _typeof(userTarget);
	                                _context2.next = _context2.t0 === 'string' ? 4 : _context2.t0 === 'object' ? 15 : _context2.t0 === 'function' ? 17 : 24;
	                                break;

	                            case 4:
	                                idOrUrl = userTarget;
	                                // use default host and port if omitted (and a relative URL is specified)

	                                if (idOrUrl.startsWith('/')) {
	                                    idOrUrl = 'ws://' + this.host + ':' + this.port + idOrUrl;
	                                }
	                                // a WebSocket URL is specified by the user (e.g., node-inspector)

	                                if (!idOrUrl.match(/^wss?:/i)) {
	                                    _context2.next = 10;
	                                    break;
	                                }

	                                return _context2.abrupt('return', idOrUrl);

	                            case 10:
	                                _context2.next = 12;
	                                return devtools.List(options);

	                            case 12:
	                                targets = _context2.sent;
	                                object = targets.find(function (target) {
	                                    return target.id === idOrUrl;
	                                });
	                                return _context2.abrupt('return', object.webSocketDebuggerUrl);

	                            case 15:
	                                _object = userTarget;
	                                return _context2.abrupt('return', _object.webSocketDebuggerUrl);

	                            case 17:
	                                func = userTarget;
	                                _context2.next = 20;
	                                return devtools.List(options);

	                            case 20:
	                                _targets = _context2.sent;
	                                result = func(_targets);
	                                _object2 = typeof result === 'number' ? _targets[result] : result;
	                                return _context2.abrupt('return', _object2.webSocketDebuggerUrl);

	                            case 24:
	                                throw new Error('Invalid target argument "' + this.target + '"');

	                            case 25:
	                            case 'end':
	                                return _context2.stop();
	                        }
	                    }
	                }, _callee2, this);
	            }));

	            function _fetchDebuggerURL(_x) {
	                return _ref2.apply(this, arguments);
	            }

	            return _fetchDebuggerURL;
	        }()

	        // fetch the protocol according to 'protocol' and 'local'

	    }, {
	        key: '_fetchProtocol',
	        value: function () {
	            var _ref3 = _asyncToGenerator( /*#__PURE__*/regeneratorRuntime.mark(function _callee3(options) {
	                return regeneratorRuntime.wrap(function _callee3$(_context3) {
	                    while (1) {
	                        switch (_context3.prev = _context3.next) {
	                            case 0:
	                                if (!this.protocol) {
	                                    _context3.next = 4;
	                                    break;
	                                }

	                                return _context3.abrupt('return', this.protocol);

	                            case 4:
	                                options.local = this.local;
	                                _context3.next = 7;
	                                return devtools.Protocol(options);

	                            case 7:
	                                return _context3.abrupt('return', _context3.sent);

	                            case 8:
	                            case 'end':
	                                return _context3.stop();
	                        }
	                    }
	                }, _callee3, this);
	            }));

	            function _fetchProtocol(_x2) {
	                return _ref3.apply(this, arguments);
	            }

	            return _fetchProtocol;
	        }()

	        // establish the WebSocket connection and start processing user commands

	    }, {
	        key: '_connectToWebSocket',
	        value: function _connectToWebSocket() {
	            var _this6 = this;

	            return new Promise(function (fulfill, reject) {
	                // create the WebSocket
	                try {
	                    if (_this6.secure) {
	                        _this6.webSocketUrl = _this6.webSocketUrl.replace(/^ws:/i, 'wss:');
	                    }
	                    _this6._ws = new WebSocket(_this6.webSocketUrl);
	                } catch (err) {
	                    // handles bad URLs
	                    reject(err);
	                    return;
	                }
	                // set up event handlers
	                _this6._ws.on('open', function () {
	                    fulfill();
	                });
	                _this6._ws.on('message', function (data) {
	                    var message = JSON.parse(data);
	                    _this6._handleMessage(message);
	                });
	                _this6._ws.on('close', function (code) {
	                    _this6.emit('disconnect');
	                });
	                _this6._ws.on('error', function (err) {
	                    reject(err);
	                });
	            });
	        }

	        // handle the messages read from the WebSocket

	    }, {
	        key: '_handleMessage',
	        value: function _handleMessage(message) {
	            // command response
	            if (message.id) {
	                var callback = this._callbacks[message.id];
	                if (!callback) {
	                    return;
	                }
	                // interpret the lack of both 'error' and 'result' as success
	                // (this may happen with node-inspector)
	                if (message.error) {
	                    callback(true, message.error);
	                } else {
	                    callback(false, message.result || {});
	                }
	                // unregister command response callback
	                delete this._callbacks[message.id];
	                // notify when there are no more pending commands
	                if (Object.keys(this._callbacks).length === 0) {
	                    this.emit('ready');
	                }
	            }
	            // event
	            else if (message.method) {
	                    this.emit('event', message);
	                    this.emit(message.method, message.params);
	                }
	        }

	        // send a command to the remote endpoint and register a callback for the reply

	    }, {
	        key: '_enqueueCommand',
	        value: function _enqueueCommand(method, params, callback) {
	            var _this7 = this;

	            var id = this._nextCommandId++;
	            var message = {
	                id: id, method: method,
	                params: params || {}
	            };
	            this._ws.send(JSON.stringify(message), function (err) {
	                if (err) {
	                    // handle low-level WebSocket errors
	                    if (typeof callback === 'function') {
	                        callback(err);
	                    }
	                } else {
	                    _this7._callbacks[id] = callback;
	                }
	            });
	        }
	    }]);

	    return Chrome;
	}(EventEmitter);

	module.exports = Chrome;
	/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(328)))

/***/ }),
/* 373 */
/***/ (function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(global, process) {// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	var formatRegExp = /%[sdj%]/g;
	exports.format = function(f) {
	  if (!isString(f)) {
	    var objects = [];
	    for (var i = 0; i < arguments.length; i++) {
	      objects.push(inspect(arguments[i]));
	    }
	    return objects.join(' ');
	  }

	  var i = 1;
	  var args = arguments;
	  var len = args.length;
	  var str = String(f).replace(formatRegExp, function(x) {
	    if (x === '%%') return '%';
	    if (i >= len) return x;
	    switch (x) {
	      case '%s': return String(args[i++]);
	      case '%d': return Number(args[i++]);
	      case '%j':
	        try {
	          return JSON.stringify(args[i++]);
	        } catch (_) {
	          return '[Circular]';
	        }
	      default:
	        return x;
	    }
	  });
	  for (var x = args[i]; i < len; x = args[++i]) {
	    if (isNull(x) || !isObject(x)) {
	      str += ' ' + x;
	    } else {
	      str += ' ' + inspect(x);
	    }
	  }
	  return str;
	};


	// Mark that a method should not be used.
	// Returns a modified function which warns once by default.
	// If --no-deprecation is set, then it is a no-op.
	exports.deprecate = function(fn, msg) {
	  // Allow for deprecating things in the process of starting up.
	  if (isUndefined(global.process)) {
	    return function() {
	      return exports.deprecate(fn, msg).apply(this, arguments);
	    };
	  }

	  if (process.noDeprecation === true) {
	    return fn;
	  }

	  var warned = false;
	  function deprecated() {
	    if (!warned) {
	      if (process.throwDeprecation) {
	        throw new Error(msg);
	      } else if (process.traceDeprecation) {
	        console.trace(msg);
	      } else {
	        console.error(msg);
	      }
	      warned = true;
	    }
	    return fn.apply(this, arguments);
	  }

	  return deprecated;
	};


	var debugs = {};
	var debugEnviron;
	exports.debuglog = function(set) {
	  if (isUndefined(debugEnviron))
	    debugEnviron = process.env.NODE_DEBUG || '';
	  set = set.toUpperCase();
	  if (!debugs[set]) {
	    if (new RegExp('\\b' + set + '\\b', 'i').test(debugEnviron)) {
	      var pid = process.pid;
	      debugs[set] = function() {
	        var msg = exports.format.apply(exports, arguments);
	        console.error('%s %d: %s', set, pid, msg);
	      };
	    } else {
	      debugs[set] = function() {};
	    }
	  }
	  return debugs[set];
	};


	/**
	 * Echos the value of a value. Trys to print the value out
	 * in the best way possible given the different types.
	 *
	 * @param {Object} obj The object to print out.
	 * @param {Object} opts Optional options object that alters the output.
	 */
	/* legacy: obj, showHidden, depth, colors*/
	function inspect(obj, opts) {
	  // default options
	  var ctx = {
	    seen: [],
	    stylize: stylizeNoColor
	  };
	  // legacy...
	  if (arguments.length >= 3) ctx.depth = arguments[2];
	  if (arguments.length >= 4) ctx.colors = arguments[3];
	  if (isBoolean(opts)) {
	    // legacy...
	    ctx.showHidden = opts;
	  } else if (opts) {
	    // got an "options" object
	    exports._extend(ctx, opts);
	  }
	  // set default options
	  if (isUndefined(ctx.showHidden)) ctx.showHidden = false;
	  if (isUndefined(ctx.depth)) ctx.depth = 2;
	  if (isUndefined(ctx.colors)) ctx.colors = false;
	  if (isUndefined(ctx.customInspect)) ctx.customInspect = true;
	  if (ctx.colors) ctx.stylize = stylizeWithColor;
	  return formatValue(ctx, obj, ctx.depth);
	}
	exports.inspect = inspect;


	// http://en.wikipedia.org/wiki/ANSI_escape_code#graphics
	inspect.colors = {
	  'bold' : [1, 22],
	  'italic' : [3, 23],
	  'underline' : [4, 24],
	  'inverse' : [7, 27],
	  'white' : [37, 39],
	  'grey' : [90, 39],
	  'black' : [30, 39],
	  'blue' : [34, 39],
	  'cyan' : [36, 39],
	  'green' : [32, 39],
	  'magenta' : [35, 39],
	  'red' : [31, 39],
	  'yellow' : [33, 39]
	};

	// Don't use 'blue' not visible on cmd.exe
	inspect.styles = {
	  'special': 'cyan',
	  'number': 'yellow',
	  'boolean': 'yellow',
	  'undefined': 'grey',
	  'null': 'bold',
	  'string': 'green',
	  'date': 'magenta',
	  // "name": intentionally not styling
	  'regexp': 'red'
	};


	function stylizeWithColor(str, styleType) {
	  var style = inspect.styles[styleType];

	  if (style) {
	    return '\u001b[' + inspect.colors[style][0] + 'm' + str +
	           '\u001b[' + inspect.colors[style][1] + 'm';
	  } else {
	    return str;
	  }
	}


	function stylizeNoColor(str, styleType) {
	  return str;
	}


	function arrayToHash(array) {
	  var hash = {};

	  array.forEach(function(val, idx) {
	    hash[val] = true;
	  });

	  return hash;
	}


	function formatValue(ctx, value, recurseTimes) {
	  // Provide a hook for user-specified inspect functions.
	  // Check that value is an object with an inspect function on it
	  if (ctx.customInspect &&
	      value &&
	      isFunction(value.inspect) &&
	      // Filter out the util module, it's inspect function is special
	      value.inspect !== exports.inspect &&
	      // Also filter out any prototype objects using the circular check.
	      !(value.constructor && value.constructor.prototype === value)) {
	    var ret = value.inspect(recurseTimes, ctx);
	    if (!isString(ret)) {
	      ret = formatValue(ctx, ret, recurseTimes);
	    }
	    return ret;
	  }

	  // Primitive types cannot have properties
	  var primitive = formatPrimitive(ctx, value);
	  if (primitive) {
	    return primitive;
	  }

	  // Look up the keys of the object.
	  var keys = Object.keys(value);
	  var visibleKeys = arrayToHash(keys);

	  if (ctx.showHidden) {
	    keys = Object.getOwnPropertyNames(value);
	  }

	  // IE doesn't make error fields non-enumerable
	  // http://msdn.microsoft.com/en-us/library/ie/dww52sbt(v=vs.94).aspx
	  if (isError(value)
	      && (keys.indexOf('message') >= 0 || keys.indexOf('description') >= 0)) {
	    return formatError(value);
	  }

	  // Some type of object without properties can be shortcutted.
	  if (keys.length === 0) {
	    if (isFunction(value)) {
	      var name = value.name ? ': ' + value.name : '';
	      return ctx.stylize('[Function' + name + ']', 'special');
	    }
	    if (isRegExp(value)) {
	      return ctx.stylize(RegExp.prototype.toString.call(value), 'regexp');
	    }
	    if (isDate(value)) {
	      return ctx.stylize(Date.prototype.toString.call(value), 'date');
	    }
	    if (isError(value)) {
	      return formatError(value);
	    }
	  }

	  var base = '', array = false, braces = ['{', '}'];

	  // Make Array say that they are Array
	  if (isArray(value)) {
	    array = true;
	    braces = ['[', ']'];
	  }

	  // Make functions say that they are functions
	  if (isFunction(value)) {
	    var n = value.name ? ': ' + value.name : '';
	    base = ' [Function' + n + ']';
	  }

	  // Make RegExps say that they are RegExps
	  if (isRegExp(value)) {
	    base = ' ' + RegExp.prototype.toString.call(value);
	  }

	  // Make dates with properties first say the date
	  if (isDate(value)) {
	    base = ' ' + Date.prototype.toUTCString.call(value);
	  }

	  // Make error with message first say the error
	  if (isError(value)) {
	    base = ' ' + formatError(value);
	  }

	  if (keys.length === 0 && (!array || value.length == 0)) {
	    return braces[0] + base + braces[1];
	  }

	  if (recurseTimes < 0) {
	    if (isRegExp(value)) {
	      return ctx.stylize(RegExp.prototype.toString.call(value), 'regexp');
	    } else {
	      return ctx.stylize('[Object]', 'special');
	    }
	  }

	  ctx.seen.push(value);

	  var output;
	  if (array) {
	    output = formatArray(ctx, value, recurseTimes, visibleKeys, keys);
	  } else {
	    output = keys.map(function(key) {
	      return formatProperty(ctx, value, recurseTimes, visibleKeys, key, array);
	    });
	  }

	  ctx.seen.pop();

	  return reduceToSingleString(output, base, braces);
	}


	function formatPrimitive(ctx, value) {
	  if (isUndefined(value))
	    return ctx.stylize('undefined', 'undefined');
	  if (isString(value)) {
	    var simple = '\'' + JSON.stringify(value).replace(/^"|"$/g, '')
	                                             .replace(/'/g, "\\'")
	                                             .replace(/\\"/g, '"') + '\'';
	    return ctx.stylize(simple, 'string');
	  }
	  if (isNumber(value))
	    return ctx.stylize('' + value, 'number');
	  if (isBoolean(value))
	    return ctx.stylize('' + value, 'boolean');
	  // For some reason typeof null is "object", so special case here.
	  if (isNull(value))
	    return ctx.stylize('null', 'null');
	}


	function formatError(value) {
	  return '[' + Error.prototype.toString.call(value) + ']';
	}


	function formatArray(ctx, value, recurseTimes, visibleKeys, keys) {
	  var output = [];
	  for (var i = 0, l = value.length; i < l; ++i) {
	    if (hasOwnProperty(value, String(i))) {
	      output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
	          String(i), true));
	    } else {
	      output.push('');
	    }
	  }
	  keys.forEach(function(key) {
	    if (!key.match(/^\d+$/)) {
	      output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
	          key, true));
	    }
	  });
	  return output;
	}


	function formatProperty(ctx, value, recurseTimes, visibleKeys, key, array) {
	  var name, str, desc;
	  desc = Object.getOwnPropertyDescriptor(value, key) || { value: value[key] };
	  if (desc.get) {
	    if (desc.set) {
	      str = ctx.stylize('[Getter/Setter]', 'special');
	    } else {
	      str = ctx.stylize('[Getter]', 'special');
	    }
	  } else {
	    if (desc.set) {
	      str = ctx.stylize('[Setter]', 'special');
	    }
	  }
	  if (!hasOwnProperty(visibleKeys, key)) {
	    name = '[' + key + ']';
	  }
	  if (!str) {
	    if (ctx.seen.indexOf(desc.value) < 0) {
	      if (isNull(recurseTimes)) {
	        str = formatValue(ctx, desc.value, null);
	      } else {
	        str = formatValue(ctx, desc.value, recurseTimes - 1);
	      }
	      if (str.indexOf('\n') > -1) {
	        if (array) {
	          str = str.split('\n').map(function(line) {
	            return '  ' + line;
	          }).join('\n').substr(2);
	        } else {
	          str = '\n' + str.split('\n').map(function(line) {
	            return '   ' + line;
	          }).join('\n');
	        }
	      }
	    } else {
	      str = ctx.stylize('[Circular]', 'special');
	    }
	  }
	  if (isUndefined(name)) {
	    if (array && key.match(/^\d+$/)) {
	      return str;
	    }
	    name = JSON.stringify('' + key);
	    if (name.match(/^"([a-zA-Z_][a-zA-Z_0-9]*)"$/)) {
	      name = name.substr(1, name.length - 2);
	      name = ctx.stylize(name, 'name');
	    } else {
	      name = name.replace(/'/g, "\\'")
	                 .replace(/\\"/g, '"')
	                 .replace(/(^"|"$)/g, "'");
	      name = ctx.stylize(name, 'string');
	    }
	  }

	  return name + ': ' + str;
	}


	function reduceToSingleString(output, base, braces) {
	  var numLinesEst = 0;
	  var length = output.reduce(function(prev, cur) {
	    numLinesEst++;
	    if (cur.indexOf('\n') >= 0) numLinesEst++;
	    return prev + cur.replace(/\u001b\[\d\d?m/g, '').length + 1;
	  }, 0);

	  if (length > 60) {
	    return braces[0] +
	           (base === '' ? '' : base + '\n ') +
	           ' ' +
	           output.join(',\n  ') +
	           ' ' +
	           braces[1];
	  }

	  return braces[0] + base + ' ' + output.join(', ') + ' ' + braces[1];
	}


	// NOTE: These type checking functions intentionally don't use `instanceof`
	// because it is fragile and can be easily faked with `Object.create()`.
	function isArray(ar) {
	  return Array.isArray(ar);
	}
	exports.isArray = isArray;

	function isBoolean(arg) {
	  return typeof arg === 'boolean';
	}
	exports.isBoolean = isBoolean;

	function isNull(arg) {
	  return arg === null;
	}
	exports.isNull = isNull;

	function isNullOrUndefined(arg) {
	  return arg == null;
	}
	exports.isNullOrUndefined = isNullOrUndefined;

	function isNumber(arg) {
	  return typeof arg === 'number';
	}
	exports.isNumber = isNumber;

	function isString(arg) {
	  return typeof arg === 'string';
	}
	exports.isString = isString;

	function isSymbol(arg) {
	  return typeof arg === 'symbol';
	}
	exports.isSymbol = isSymbol;

	function isUndefined(arg) {
	  return arg === void 0;
	}
	exports.isUndefined = isUndefined;

	function isRegExp(re) {
	  return isObject(re) && objectToString(re) === '[object RegExp]';
	}
	exports.isRegExp = isRegExp;

	function isObject(arg) {
	  return typeof arg === 'object' && arg !== null;
	}
	exports.isObject = isObject;

	function isDate(d) {
	  return isObject(d) && objectToString(d) === '[object Date]';
	}
	exports.isDate = isDate;

	function isError(e) {
	  return isObject(e) &&
	      (objectToString(e) === '[object Error]' || e instanceof Error);
	}
	exports.isError = isError;

	function isFunction(arg) {
	  return typeof arg === 'function';
	}
	exports.isFunction = isFunction;

	function isPrimitive(arg) {
	  return arg === null ||
	         typeof arg === 'boolean' ||
	         typeof arg === 'number' ||
	         typeof arg === 'string' ||
	         typeof arg === 'symbol' ||  // ES6 symbol
	         typeof arg === 'undefined';
	}
	exports.isPrimitive = isPrimitive;

	exports.isBuffer = __webpack_require__(374);

	function objectToString(o) {
	  return Object.prototype.toString.call(o);
	}


	function pad(n) {
	  return n < 10 ? '0' + n.toString(10) : n.toString(10);
	}


	var months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep',
	              'Oct', 'Nov', 'Dec'];

	// 26 Feb 16:19:34
	function timestamp() {
	  var d = new Date();
	  var time = [pad(d.getHours()),
	              pad(d.getMinutes()),
	              pad(d.getSeconds())].join(':');
	  return [d.getDate(), months[d.getMonth()], time].join(' ');
	}


	// log is just a thin wrapper to console.log that prepends a timestamp
	exports.log = function() {
	  console.log('%s - %s', timestamp(), exports.format.apply(exports, arguments));
	};


	/**
	 * Inherit the prototype methods from one constructor into another.
	 *
	 * The Function.prototype.inherits from lang.js rewritten as a standalone
	 * function (not on Function.prototype). NOTE: If this file is to be loaded
	 * during bootstrapping this function needs to be rewritten using some native
	 * functions as prototype setup using normal JavaScript does not work as
	 * expected during bootstrapping (see mirror.js in r114903).
	 *
	 * @param {function} ctor Constructor function which needs to inherit the
	 *     prototype.
	 * @param {function} superCtor Constructor function to inherit prototype from.
	 */
	exports.inherits = __webpack_require__(338);

	exports._extend = function(origin, add) {
	  // Don't do anything if add isn't an object
	  if (!add || !isObject(add)) return origin;

	  var keys = Object.keys(add);
	  var i = keys.length;
	  while (i--) {
	    origin[keys[i]] = add[keys[i]];
	  }
	  return origin;
	};

	function hasOwnProperty(obj, prop) {
	  return Object.prototype.hasOwnProperty.call(obj, prop);
	}

	/* WEBPACK VAR INJECTION */}.call(exports, (function() { return this; }()), __webpack_require__(328)))

/***/ }),
/* 374 */
/***/ (function(module, exports) {

	module.exports = function isBuffer(arg) {
	  return arg && typeof arg === 'object'
	    && typeof arg.copy === 'function'
	    && typeof arg.fill === 'function'
	    && typeof arg.readUInt8 === 'function';
	}

/***/ }),
/* 375 */
/***/ (function(module, exports, __webpack_require__) {

	'use strict';

	var _createClass = function () { function defineProperties(target, props) { for (var i = 0; i < props.length; i++) { var descriptor = props[i]; descriptor.enumerable = descriptor.enumerable || false; descriptor.configurable = true; if ("value" in descriptor) descriptor.writable = true; Object.defineProperty(target, descriptor.key, descriptor); } } return function (Constructor, protoProps, staticProps) { if (protoProps) defineProperties(Constructor.prototype, protoProps); if (staticProps) defineProperties(Constructor, staticProps); return Constructor; }; }();

	function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } }

	function _possibleConstructorReturn(self, call) { if (!self) { throw new ReferenceError("this hasn't been initialised - super() hasn't been called"); } return call && (typeof call === "object" || typeof call === "function") ? call : self; }

	function _inherits(subClass, superClass) { if (typeof superClass !== "function" && superClass !== null) { throw new TypeError("Super expression must either be null or a function, not " + typeof superClass); } subClass.prototype = Object.create(superClass && superClass.prototype, { constructor: { value: subClass, enumerable: false, writable: true, configurable: true } }); if (superClass) Object.setPrototypeOf ? Object.setPrototypeOf(subClass, superClass) : subClass.__proto__ = superClass; }

	var EventEmitter = __webpack_require__(329);

	// wrapper around the Node.js ws module
	// for use in browsers

	var WebSocketWrapper = function (_EventEmitter) {
	    _inherits(WebSocketWrapper, _EventEmitter);

	    function WebSocketWrapper(url) {
	        _classCallCheck(this, WebSocketWrapper);

	        var _this = _possibleConstructorReturn(this, (WebSocketWrapper.__proto__ || Object.getPrototypeOf(WebSocketWrapper)).call(this));

	        _this._ws = new WebSocket(url); // eslint-disable-line no-undef
	        _this._ws.onopen = function () {
	            _this.emit('open');
	        };
	        _this._ws.onclose = function () {
	            _this.emit('close');
	        };
	        _this._ws.onmessage = function (event) {
	            _this.emit('message', event.data);
	        };
	        _this._ws.onerror = function () {
	            _this.emit('error', new Error('WebSocket error'));
	        };
	        return _this;
	    }

	    _createClass(WebSocketWrapper, [{
	        key: 'close',
	        value: function close() {
	            this._ws.close();
	        }
	    }, {
	        key: 'send',
	        value: function send(data, callback) {
	            try {
	                this._ws.send(data);
	                callback();
	            } catch (err) {
	                callback(err);
	            }
	        }
	    }]);

	    return WebSocketWrapper;
	}(EventEmitter);

	module.exports = WebSocketWrapper;

/***/ }),
/* 376 */
/***/ (function(module, exports) {

	'use strict';

	function arrayToObject(parameters) {
	    var keyValue = {};
	    parameters.forEach(function (parameter) {
	        var name = parameter.name;
	        delete parameter.name;
	        keyValue[name] = parameter;
	    });
	    return keyValue;
	}

	function decorate(to, category, object) {
	    to.category = category;
	    Object.keys(object).forEach(function (field) {
	        // skip the 'name' field as it is part of the function prototype
	        if (field === 'name') {
	            return;
	        }
	        // commands and events have parameters whereas types have properties
	        if (category === 'type' && field === 'properties' || field === 'parameters') {
	            to[field] = arrayToObject(object[field]);
	        } else {
	            to[field] = object[field];
	        }
	    });
	}

	function addCommand(chrome, domainName, command) {
	    var handler = function handler(params, callback) {
	        return chrome.send(domainName + '.' + command.name, params, callback);
	    };
	    decorate(handler, 'command', command);
	    chrome[domainName][command.name] = handler;
	}

	function addEvent(chrome, domainName, event) {
	    var eventName = domainName + '.' + event.name;
	    var handler = function handler(_handler) {
	        if (typeof _handler === 'function') {
	            chrome.on(eventName, _handler);
	            return function () {
	                return chrome.removeListener(eventName, _handler);
	            };
	        } else {
	            return new Promise(function (fulfill, reject) {
	                chrome.once(eventName, fulfill);
	            });
	        }
	    };
	    decorate(handler, 'event', event);
	    chrome[domainName][event.name] = handler;
	}

	function addType(chrome, domainName, type) {
	    var help = {};
	    decorate(help, 'type', type);
	    chrome[domainName][type.id] = help;
	}

	function prepare(object, protocol) {
	    // assign the protocol and generate the shorthands
	    object.protocol = protocol;
	    protocol.domains.forEach(function (domain) {
	        var domainName = domain.domain;
	        object[domainName] = {};
	        // add commands
	        (domain.commands || []).forEach(function (command) {
	            addCommand(object, domainName, command);
	        });
	        // add events
	        (domain.events || []).forEach(function (event) {
	            addEvent(object, domainName, event);
	        });
	        // add types
	        (domain.types || []).forEach(function (type) {
	            addType(object, domainName, type);
	        });
	    });
	}

	module.exports.prepare = prepare;

/***/ })
/******/ ]);