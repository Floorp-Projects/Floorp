function run_test() {
  for (var k in SOURCE_MAP_TEST_MODULE) {
    if (/^test/.test(k)) {
      SOURCE_MAP_TEST_MODULE[k](assert);
    }
  }
}


var SOURCE_MAP_TEST_MODULE =
/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;
/******/
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			exports: {},
/******/ 			id: moduleId,
/******/ 			loaded: false
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2014 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	{
	  var libUtil = __webpack_require__(1);
	
	  exports['test urls'] = function (assert) {
	    var assertUrl = function (url) {
	      assert.equal(url, libUtil.urlGenerate(libUtil.urlParse(url)));
	    };
	    assertUrl('http://');
	    assertUrl('http://www.example.com');
	    assertUrl('http://user:pass@www.example.com');
	    assertUrl('http://www.example.com:80');
	    assertUrl('http://www.example.com/');
	    assertUrl('http://www.example.com/foo/bar');
	    assertUrl('http://www.example.com/foo/bar/');
	    assertUrl('http://user:pass@www.example.com:80/foo/bar/');
	
	    assertUrl('//');
	    assertUrl('//www.example.com');
	    assertUrl('file:///www.example.com');
	
	    assert.equal(libUtil.urlParse(''), null);
	    assert.equal(libUtil.urlParse('.'), null);
	    assert.equal(libUtil.urlParse('..'), null);
	    assert.equal(libUtil.urlParse('a'), null);
	    assert.equal(libUtil.urlParse('a/b'), null);
	    assert.equal(libUtil.urlParse('a//b'), null);
	    assert.equal(libUtil.urlParse('/a'), null);
	    assert.equal(libUtil.urlParse('data:foo,bar'), null);
	  };
	
	  exports['test normalize()'] = function (assert) {
	    assert.equal(libUtil.normalize('/..'), '/');
	    assert.equal(libUtil.normalize('/../'), '/');
	    assert.equal(libUtil.normalize('/../../../..'), '/');
	    assert.equal(libUtil.normalize('/../../../../a/b/c'), '/a/b/c');
	    assert.equal(libUtil.normalize('/a/b/c/../../../d/../../e'), '/e');
	
	    assert.equal(libUtil.normalize('..'), '..');
	    assert.equal(libUtil.normalize('../'), '../');
	    assert.equal(libUtil.normalize('../../a/'), '../../a/');
	    assert.equal(libUtil.normalize('a/..'), '.');
	    assert.equal(libUtil.normalize('a/../../..'), '../..');
	
	    assert.equal(libUtil.normalize('/.'), '/');
	    assert.equal(libUtil.normalize('/./'), '/');
	    assert.equal(libUtil.normalize('/./././.'), '/');
	    assert.equal(libUtil.normalize('/././././a/b/c'), '/a/b/c');
	    assert.equal(libUtil.normalize('/a/b/c/./././d/././e'), '/a/b/c/d/e');
	
	    assert.equal(libUtil.normalize(''), '.');
	    assert.equal(libUtil.normalize('.'), '.');
	    assert.equal(libUtil.normalize('./'), '.');
	    assert.equal(libUtil.normalize('././a'), 'a');
	    assert.equal(libUtil.normalize('a/./'), 'a/');
	    assert.equal(libUtil.normalize('a/././.'), 'a');
	
	    assert.equal(libUtil.normalize('/a/b//c////d/////'), '/a/b/c/d/');
	    assert.equal(libUtil.normalize('///a/b//c////d/////'), '///a/b/c/d/');
	    assert.equal(libUtil.normalize('a/b//c////d'), 'a/b/c/d');
	
	    assert.equal(libUtil.normalize('.///.././../a/b//./..'), '../../a')
	
	    assert.equal(libUtil.normalize('http://www.example.com'), 'http://www.example.com');
	    assert.equal(libUtil.normalize('http://www.example.com/'), 'http://www.example.com/');
	    assert.equal(libUtil.normalize('http://www.example.com/./..//a/b/c/.././d//'), 'http://www.example.com/a/b/d/');
	  };
	
	  exports['test join()'] = function (assert) {
	    assert.equal(libUtil.join('a', 'b'), 'a/b');
	    assert.equal(libUtil.join('a/', 'b'), 'a/b');
	    assert.equal(libUtil.join('a//', 'b'), 'a/b');
	    assert.equal(libUtil.join('a', 'b/'), 'a/b/');
	    assert.equal(libUtil.join('a', 'b//'), 'a/b/');
	    assert.equal(libUtil.join('a/', '/b'), '/b');
	    assert.equal(libUtil.join('a//', '//b'), '//b');
	
	    assert.equal(libUtil.join('a', '..'), '.');
	    assert.equal(libUtil.join('a', '../b'), 'b');
	    assert.equal(libUtil.join('a/b', '../c'), 'a/c');
	
	    assert.equal(libUtil.join('a', '.'), 'a');
	    assert.equal(libUtil.join('a', './b'), 'a/b');
	    assert.equal(libUtil.join('a/b', './c'), 'a/b/c');
	
	    assert.equal(libUtil.join('a', 'http://www.example.com'), 'http://www.example.com');
	    assert.equal(libUtil.join('a', 'data:foo,bar'), 'data:foo,bar');
	
	
	    assert.equal(libUtil.join('', 'b'), 'b');
	    assert.equal(libUtil.join('.', 'b'), 'b');
	    assert.equal(libUtil.join('', 'b/'), 'b/');
	    assert.equal(libUtil.join('.', 'b/'), 'b/');
	    assert.equal(libUtil.join('', 'b//'), 'b/');
	    assert.equal(libUtil.join('.', 'b//'), 'b/');
	
	    assert.equal(libUtil.join('', '..'), '..');
	    assert.equal(libUtil.join('.', '..'), '..');
	    assert.equal(libUtil.join('', '../b'), '../b');
	    assert.equal(libUtil.join('.', '../b'), '../b');
	
	    assert.equal(libUtil.join('', '.'), '.');
	    assert.equal(libUtil.join('.', '.'), '.');
	    assert.equal(libUtil.join('', './b'), 'b');
	    assert.equal(libUtil.join('.', './b'), 'b');
	
	    assert.equal(libUtil.join('', 'http://www.example.com'), 'http://www.example.com');
	    assert.equal(libUtil.join('.', 'http://www.example.com'), 'http://www.example.com');
	    assert.equal(libUtil.join('', 'data:foo,bar'), 'data:foo,bar');
	    assert.equal(libUtil.join('.', 'data:foo,bar'), 'data:foo,bar');
	
	
	    assert.equal(libUtil.join('..', 'b'), '../b');
	    assert.equal(libUtil.join('..', 'b/'), '../b/');
	    assert.equal(libUtil.join('..', 'b//'), '../b/');
	
	    assert.equal(libUtil.join('..', '..'), '../..');
	    assert.equal(libUtil.join('..', '../b'), '../../b');
	
	    assert.equal(libUtil.join('..', '.'), '..');
	    assert.equal(libUtil.join('..', './b'), '../b');
	
	    assert.equal(libUtil.join('..', 'http://www.example.com'), 'http://www.example.com');
	    assert.equal(libUtil.join('..', 'data:foo,bar'), 'data:foo,bar');
	
	
	    assert.equal(libUtil.join('a', ''), 'a');
	    assert.equal(libUtil.join('a', '.'), 'a');
	    assert.equal(libUtil.join('a/', ''), 'a');
	    assert.equal(libUtil.join('a/', '.'), 'a');
	    assert.equal(libUtil.join('a//', ''), 'a');
	    assert.equal(libUtil.join('a//', '.'), 'a');
	    assert.equal(libUtil.join('/a', ''), '/a');
	    assert.equal(libUtil.join('/a', '.'), '/a');
	    assert.equal(libUtil.join('', ''), '.');
	    assert.equal(libUtil.join('.', ''), '.');
	    assert.equal(libUtil.join('.', ''), '.');
	    assert.equal(libUtil.join('.', '.'), '.');
	    assert.equal(libUtil.join('..', ''), '..');
	    assert.equal(libUtil.join('..', '.'), '..');
	    assert.equal(libUtil.join('http://foo.org/a', ''), 'http://foo.org/a');
	    assert.equal(libUtil.join('http://foo.org/a', '.'), 'http://foo.org/a');
	    assert.equal(libUtil.join('http://foo.org/a/', ''), 'http://foo.org/a');
	    assert.equal(libUtil.join('http://foo.org/a/', '.'), 'http://foo.org/a');
	    assert.equal(libUtil.join('http://foo.org/a//', ''), 'http://foo.org/a');
	    assert.equal(libUtil.join('http://foo.org/a//', '.'), 'http://foo.org/a');
	    assert.equal(libUtil.join('http://foo.org', ''), 'http://foo.org/');
	    assert.equal(libUtil.join('http://foo.org', '.'), 'http://foo.org/');
	    assert.equal(libUtil.join('http://foo.org/', ''), 'http://foo.org/');
	    assert.equal(libUtil.join('http://foo.org/', '.'), 'http://foo.org/');
	    assert.equal(libUtil.join('http://foo.org//', ''), 'http://foo.org/');
	    assert.equal(libUtil.join('http://foo.org//', '.'), 'http://foo.org/');
	    assert.equal(libUtil.join('//www.example.com', ''), '//www.example.com/');
	    assert.equal(libUtil.join('//www.example.com', '.'), '//www.example.com/');
	
	
	    assert.equal(libUtil.join('http://foo.org/a', 'b'), 'http://foo.org/a/b');
	    assert.equal(libUtil.join('http://foo.org/a/', 'b'), 'http://foo.org/a/b');
	    assert.equal(libUtil.join('http://foo.org/a//', 'b'), 'http://foo.org/a/b');
	    assert.equal(libUtil.join('http://foo.org/a', 'b/'), 'http://foo.org/a/b/');
	    assert.equal(libUtil.join('http://foo.org/a', 'b//'), 'http://foo.org/a/b/');
	    assert.equal(libUtil.join('http://foo.org/a/', '/b'), 'http://foo.org/b');
	    assert.equal(libUtil.join('http://foo.org/a//', '//b'), 'http://b');
	
	    assert.equal(libUtil.join('http://foo.org/a', '..'), 'http://foo.org/');
	    assert.equal(libUtil.join('http://foo.org/a', '../b'), 'http://foo.org/b');
	    assert.equal(libUtil.join('http://foo.org/a/b', '../c'), 'http://foo.org/a/c');
	
	    assert.equal(libUtil.join('http://foo.org/a', '.'), 'http://foo.org/a');
	    assert.equal(libUtil.join('http://foo.org/a', './b'), 'http://foo.org/a/b');
	    assert.equal(libUtil.join('http://foo.org/a/b', './c'), 'http://foo.org/a/b/c');
	
	    assert.equal(libUtil.join('http://foo.org/a', 'http://www.example.com'), 'http://www.example.com');
	    assert.equal(libUtil.join('http://foo.org/a', 'data:foo,bar'), 'data:foo,bar');
	
	
	    assert.equal(libUtil.join('http://foo.org', 'a'), 'http://foo.org/a');
	    assert.equal(libUtil.join('http://foo.org/', 'a'), 'http://foo.org/a');
	    assert.equal(libUtil.join('http://foo.org//', 'a'), 'http://foo.org/a');
	    assert.equal(libUtil.join('http://foo.org', '/a'), 'http://foo.org/a');
	    assert.equal(libUtil.join('http://foo.org/', '/a'), 'http://foo.org/a');
	    assert.equal(libUtil.join('http://foo.org//', '/a'), 'http://foo.org/a');
	
	
	    assert.equal(libUtil.join('http://', 'www.example.com'), 'http://www.example.com');
	    assert.equal(libUtil.join('file:///', 'www.example.com'), 'file:///www.example.com');
	    assert.equal(libUtil.join('http://', 'ftp://example.com'), 'ftp://example.com');
	
	    assert.equal(libUtil.join('http://www.example.com', '//foo.org/bar'), 'http://foo.org/bar');
	    assert.equal(libUtil.join('//www.example.com', '//foo.org/bar'), '//foo.org/bar');
	  };
	
	  // TODO Issue #128: Define and test this function properly.
	  exports['test relative()'] = function (assert) {
	    assert.equal(libUtil.relative('/the/root', '/the/root/one.js'), 'one.js');
	    assert.equal(libUtil.relative('http://the/root', 'http://the/root/one.js'), 'one.js');
	    assert.equal(libUtil.relative('/the/root', '/the/rootone.js'), '../rootone.js');
	    assert.equal(libUtil.relative('http://the/root', 'http://the/rootone.js'), '../rootone.js');
	    assert.equal(libUtil.relative('/the/root', '/therootone.js'), '/therootone.js');
	    assert.equal(libUtil.relative('http://the/root', '/therootone.js'), '/therootone.js');
	
	    assert.equal(libUtil.relative('', '/the/root/one.js'), '/the/root/one.js');
	    assert.equal(libUtil.relative('.', '/the/root/one.js'), '/the/root/one.js');
	    assert.equal(libUtil.relative('', 'the/root/one.js'), 'the/root/one.js');
	    assert.equal(libUtil.relative('.', 'the/root/one.js'), 'the/root/one.js');
	
	    assert.equal(libUtil.relative('/', '/the/root/one.js'), 'the/root/one.js');
	    assert.equal(libUtil.relative('/', 'the/root/one.js'), 'the/root/one.js');
	  };
	}


/***/ },
/* 1 */
/***/ function(module, exports) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	{
	  /**
	   * This is a helper function for getting values from parameter/options
	   * objects.
	   *
	   * @param args The object we are extracting values from
	   * @param name The name of the property we are getting.
	   * @param defaultValue An optional value to return if the property is missing
	   * from the object. If this is not specified and the property is missing, an
	   * error will be thrown.
	   */
	  function getArg(aArgs, aName, aDefaultValue) {
	    if (aName in aArgs) {
	      return aArgs[aName];
	    } else if (arguments.length === 3) {
	      return aDefaultValue;
	    } else {
	      throw new Error('"' + aName + '" is a required argument.');
	    }
	  }
	  exports.getArg = getArg;
	
	  var urlRegexp = /^(?:([\w+\-.]+):)?\/\/(?:(\w+:\w+)@)?([\w.]*)(?::(\d+))?(\S*)$/;
	  var dataUrlRegexp = /^data:.+\,.+$/;
	
	  function urlParse(aUrl) {
	    var match = aUrl.match(urlRegexp);
	    if (!match) {
	      return null;
	    }
	    return {
	      scheme: match[1],
	      auth: match[2],
	      host: match[3],
	      port: match[4],
	      path: match[5]
	    };
	  }
	  exports.urlParse = urlParse;
	
	  function urlGenerate(aParsedUrl) {
	    var url = '';
	    if (aParsedUrl.scheme) {
	      url += aParsedUrl.scheme + ':';
	    }
	    url += '//';
	    if (aParsedUrl.auth) {
	      url += aParsedUrl.auth + '@';
	    }
	    if (aParsedUrl.host) {
	      url += aParsedUrl.host;
	    }
	    if (aParsedUrl.port) {
	      url += ":" + aParsedUrl.port
	    }
	    if (aParsedUrl.path) {
	      url += aParsedUrl.path;
	    }
	    return url;
	  }
	  exports.urlGenerate = urlGenerate;
	
	  /**
	   * Normalizes a path, or the path portion of a URL:
	   *
	   * - Replaces consequtive slashes with one slash.
	   * - Removes unnecessary '.' parts.
	   * - Removes unnecessary '<dir>/..' parts.
	   *
	   * Based on code in the Node.js 'path' core module.
	   *
	   * @param aPath The path or url to normalize.
	   */
	  function normalize(aPath) {
	    var path = aPath;
	    var url = urlParse(aPath);
	    if (url) {
	      if (!url.path) {
	        return aPath;
	      }
	      path = url.path;
	    }
	    var isAbsolute = exports.isAbsolute(path);
	
	    var parts = path.split(/\/+/);
	    for (var part, up = 0, i = parts.length - 1; i >= 0; i--) {
	      part = parts[i];
	      if (part === '.') {
	        parts.splice(i, 1);
	      } else if (part === '..') {
	        up++;
	      } else if (up > 0) {
	        if (part === '') {
	          // The first part is blank if the path is absolute. Trying to go
	          // above the root is a no-op. Therefore we can remove all '..' parts
	          // directly after the root.
	          parts.splice(i + 1, up);
	          up = 0;
	        } else {
	          parts.splice(i, 2);
	          up--;
	        }
	      }
	    }
	    path = parts.join('/');
	
	    if (path === '') {
	      path = isAbsolute ? '/' : '.';
	    }
	
	    if (url) {
	      url.path = path;
	      return urlGenerate(url);
	    }
	    return path;
	  }
	  exports.normalize = normalize;
	
	  /**
	   * Joins two paths/URLs.
	   *
	   * @param aRoot The root path or URL.
	   * @param aPath The path or URL to be joined with the root.
	   *
	   * - If aPath is a URL or a data URI, aPath is returned, unless aPath is a
	   *   scheme-relative URL: Then the scheme of aRoot, if any, is prepended
	   *   first.
	   * - Otherwise aPath is a path. If aRoot is a URL, then its path portion
	   *   is updated with the result and aRoot is returned. Otherwise the result
	   *   is returned.
	   *   - If aPath is absolute, the result is aPath.
	   *   - Otherwise the two paths are joined with a slash.
	   * - Joining for example 'http://' and 'www.example.com' is also supported.
	   */
	  function join(aRoot, aPath) {
	    if (aRoot === "") {
	      aRoot = ".";
	    }
	    if (aPath === "") {
	      aPath = ".";
	    }
	    var aPathUrl = urlParse(aPath);
	    var aRootUrl = urlParse(aRoot);
	    if (aRootUrl) {
	      aRoot = aRootUrl.path || '/';
	    }
	
	    // `join(foo, '//www.example.org')`
	    if (aPathUrl && !aPathUrl.scheme) {
	      if (aRootUrl) {
	        aPathUrl.scheme = aRootUrl.scheme;
	      }
	      return urlGenerate(aPathUrl);
	    }
	
	    if (aPathUrl || aPath.match(dataUrlRegexp)) {
	      return aPath;
	    }
	
	    // `join('http://', 'www.example.com')`
	    if (aRootUrl && !aRootUrl.host && !aRootUrl.path) {
	      aRootUrl.host = aPath;
	      return urlGenerate(aRootUrl);
	    }
	
	    var joined = aPath.charAt(0) === '/'
	      ? aPath
	      : normalize(aRoot.replace(/\/+$/, '') + '/' + aPath);
	
	    if (aRootUrl) {
	      aRootUrl.path = joined;
	      return urlGenerate(aRootUrl);
	    }
	    return joined;
	  }
	  exports.join = join;
	
	  exports.isAbsolute = function (aPath) {
	    return aPath.charAt(0) === '/' || !!aPath.match(urlRegexp);
	  };
	
	  /**
	   * Make a path relative to a URL or another path.
	   *
	   * @param aRoot The root path or URL.
	   * @param aPath The path or URL to be made relative to aRoot.
	   */
	  function relative(aRoot, aPath) {
	    if (aRoot === "") {
	      aRoot = ".";
	    }
	
	    aRoot = aRoot.replace(/\/$/, '');
	
	    // It is possible for the path to be above the root. In this case, simply
	    // checking whether the root is a prefix of the path won't work. Instead, we
	    // need to remove components from the root one by one, until either we find
	    // a prefix that fits, or we run out of components to remove.
	    var level = 0;
	    while (aPath.indexOf(aRoot + '/') !== 0) {
	      var index = aRoot.lastIndexOf("/");
	      if (index < 0) {
	        return aPath;
	      }
	
	      // If the only part of the root that is left is the scheme (i.e. http://,
	      // file:///, etc.), one or more slashes (/), or simply nothing at all, we
	      // have exhausted all components, so the path is not relative to the root.
	      aRoot = aRoot.slice(0, index);
	      if (aRoot.match(/^([^\/]+:\/)?\/*$/)) {
	        return aPath;
	      }
	
	      ++level;
	    }
	
	    // Make sure we add a "../" for each component we removed from the root.
	    return Array(level + 1).join("../") + aPath.substr(aRoot.length + 1);
	  }
	  exports.relative = relative;
	
	  /**
	   * Because behavior goes wacky when you set `__proto__` on objects, we
	   * have to prefix all the strings in our set with an arbitrary character.
	   *
	   * See https://github.com/mozilla/source-map/pull/31 and
	   * https://github.com/mozilla/source-map/issues/30
	   *
	   * @param String aStr
	   */
	  function toSetString(aStr) {
	    return '$' + aStr;
	  }
	  exports.toSetString = toSetString;
	
	  function fromSetString(aStr) {
	    return aStr.substr(1);
	  }
	  exports.fromSetString = fromSetString;
	
	  /**
	   * Comparator between two mappings where the original positions are compared.
	   *
	   * Optionally pass in `true` as `onlyCompareGenerated` to consider two
	   * mappings with the same original source/line/column, but different generated
	   * line and column the same. Useful when searching for a mapping with a
	   * stubbed out mapping.
	   */
	  function compareByOriginalPositions(mappingA, mappingB, onlyCompareOriginal) {
	    var cmp = mappingA.source - mappingB.source;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.originalLine - mappingB.originalLine;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.originalColumn - mappingB.originalColumn;
	    if (cmp !== 0 || onlyCompareOriginal) {
	      return cmp;
	    }
	
	    cmp = mappingA.generatedColumn - mappingB.generatedColumn;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.generatedLine - mappingB.generatedLine;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    return mappingA.name - mappingB.name;
	  }
	  exports.compareByOriginalPositions = compareByOriginalPositions;
	
	  /**
	   * Comparator between two mappings with deflated source and name indices where
	   * the generated positions are compared.
	   *
	   * Optionally pass in `true` as `onlyCompareGenerated` to consider two
	   * mappings with the same generated line and column, but different
	   * source/name/original line and column the same. Useful when searching for a
	   * mapping with a stubbed out mapping.
	   */
	  function compareByGeneratedPositionsDeflated(mappingA, mappingB, onlyCompareGenerated) {
	    var cmp = mappingA.generatedLine - mappingB.generatedLine;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.generatedColumn - mappingB.generatedColumn;
	    if (cmp !== 0 || onlyCompareGenerated) {
	      return cmp;
	    }
	
	    cmp = mappingA.source - mappingB.source;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.originalLine - mappingB.originalLine;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.originalColumn - mappingB.originalColumn;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    return mappingA.name - mappingB.name;
	  }
	  exports.compareByGeneratedPositionsDeflated = compareByGeneratedPositionsDeflated;
	
	  function strcmp(aStr1, aStr2) {
	    if (aStr1 === aStr2) {
	      return 0;
	    }
	
	    if (aStr1 > aStr2) {
	      return 1;
	    }
	
	    return -1;
	  }
	
	  /**
	   * Comparator between two mappings with inflated source and name strings where
	   * the generated positions are compared.
	   */
	  function compareByGeneratedPositionsInflated(mappingA, mappingB) {
	    var cmp = mappingA.generatedLine - mappingB.generatedLine;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.generatedColumn - mappingB.generatedColumn;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = strcmp(mappingA.source, mappingB.source);
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.originalLine - mappingB.originalLine;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.originalColumn - mappingB.originalColumn;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    return strcmp(mappingA.name, mappingB.name);
	  }
	  exports.compareByGeneratedPositionsInflated = compareByGeneratedPositionsInflated;
	}


/***/ }
/******/ ]);
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbIndlYnBhY2s6Ly8vd2VicGFjay9ib290c3RyYXAgYWVhNDgxM2ZiZGRjOWE3MDI3MTgiLCJ3ZWJwYWNrOi8vLy4vdGVzdC90ZXN0LXV0aWwuanMiLCJ3ZWJwYWNrOi8vLy4vbGliL3V0aWwuanMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6Ijs7Ozs7Ozs7Ozs7QUFBQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQSx1QkFBZTtBQUNmO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOzs7QUFHQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOzs7Ozs7O0FDdENBLGlCQUFnQixvQkFBb0I7QUFDcEM7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOzs7QUFHQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7OztBQUdBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7O0FBR0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7OztBQUdBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7O0FBR0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOzs7QUFHQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7Ozs7Ozs7QUN0TkEsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBLE1BQUs7QUFDTDtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBLGlEQUFnRCxRQUFRO0FBQ3hEO0FBQ0E7QUFDQTtBQUNBLFFBQU87QUFDUDtBQUNBLFFBQU87QUFDUDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxVQUFTO0FBQ1Q7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQSIsImZpbGUiOiJ0ZXN0X3V0aWwuanMiLCJzb3VyY2VzQ29udGVudCI6WyIgXHQvLyBUaGUgbW9kdWxlIGNhY2hlXG4gXHR2YXIgaW5zdGFsbGVkTW9kdWxlcyA9IHt9O1xuXG4gXHQvLyBUaGUgcmVxdWlyZSBmdW5jdGlvblxuIFx0ZnVuY3Rpb24gX193ZWJwYWNrX3JlcXVpcmVfXyhtb2R1bGVJZCkge1xuXG4gXHRcdC8vIENoZWNrIGlmIG1vZHVsZSBpcyBpbiBjYWNoZVxuIFx0XHRpZihpbnN0YWxsZWRNb2R1bGVzW21vZHVsZUlkXSlcbiBcdFx0XHRyZXR1cm4gaW5zdGFsbGVkTW9kdWxlc1ttb2R1bGVJZF0uZXhwb3J0cztcblxuIFx0XHQvLyBDcmVhdGUgYSBuZXcgbW9kdWxlIChhbmQgcHV0IGl0IGludG8gdGhlIGNhY2hlKVxuIFx0XHR2YXIgbW9kdWxlID0gaW5zdGFsbGVkTW9kdWxlc1ttb2R1bGVJZF0gPSB7XG4gXHRcdFx0ZXhwb3J0czoge30sXG4gXHRcdFx0aWQ6IG1vZHVsZUlkLFxuIFx0XHRcdGxvYWRlZDogZmFsc2VcbiBcdFx0fTtcblxuIFx0XHQvLyBFeGVjdXRlIHRoZSBtb2R1bGUgZnVuY3Rpb25cbiBcdFx0bW9kdWxlc1ttb2R1bGVJZF0uY2FsbChtb2R1bGUuZXhwb3J0cywgbW9kdWxlLCBtb2R1bGUuZXhwb3J0cywgX193ZWJwYWNrX3JlcXVpcmVfXyk7XG5cbiBcdFx0Ly8gRmxhZyB0aGUgbW9kdWxlIGFzIGxvYWRlZFxuIFx0XHRtb2R1bGUubG9hZGVkID0gdHJ1ZTtcblxuIFx0XHQvLyBSZXR1cm4gdGhlIGV4cG9ydHMgb2YgdGhlIG1vZHVsZVxuIFx0XHRyZXR1cm4gbW9kdWxlLmV4cG9ydHM7XG4gXHR9XG5cblxuIFx0Ly8gZXhwb3NlIHRoZSBtb2R1bGVzIG9iamVjdCAoX193ZWJwYWNrX21vZHVsZXNfXylcbiBcdF9fd2VicGFja19yZXF1aXJlX18ubSA9IG1vZHVsZXM7XG5cbiBcdC8vIGV4cG9zZSB0aGUgbW9kdWxlIGNhY2hlXG4gXHRfX3dlYnBhY2tfcmVxdWlyZV9fLmMgPSBpbnN0YWxsZWRNb2R1bGVzO1xuXG4gXHQvLyBfX3dlYnBhY2tfcHVibGljX3BhdGhfX1xuIFx0X193ZWJwYWNrX3JlcXVpcmVfXy5wID0gXCJcIjtcblxuIFx0Ly8gTG9hZCBlbnRyeSBtb2R1bGUgYW5kIHJldHVybiBleHBvcnRzXG4gXHRyZXR1cm4gX193ZWJwYWNrX3JlcXVpcmVfXygwKTtcblxuXG5cbi8qKiBXRUJQQUNLIEZPT1RFUiAqKlxuICoqIHdlYnBhY2svYm9vdHN0cmFwIGFlYTQ4MTNmYmRkYzlhNzAyNzE4XG4gKiovIiwiLyogLSotIE1vZGU6IGpzOyBqcy1pbmRlbnQtbGV2ZWw6IDI7IC0qLSAqL1xuLypcbiAqIENvcHlyaWdodCAyMDE0IE1vemlsbGEgRm91bmRhdGlvbiBhbmQgY29udHJpYnV0b3JzXG4gKiBMaWNlbnNlZCB1bmRlciB0aGUgTmV3IEJTRCBsaWNlbnNlLiBTZWUgTElDRU5TRSBvcjpcbiAqIGh0dHA6Ly9vcGVuc291cmNlLm9yZy9saWNlbnNlcy9CU0QtMy1DbGF1c2VcbiAqL1xue1xuICB2YXIgbGliVXRpbCA9IHJlcXVpcmUoJy4uL2xpYi91dGlsJyk7XG5cbiAgZXhwb3J0c1sndGVzdCB1cmxzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIGFzc2VydFVybCA9IGZ1bmN0aW9uICh1cmwpIHtcbiAgICAgIGFzc2VydC5lcXVhbCh1cmwsIGxpYlV0aWwudXJsR2VuZXJhdGUobGliVXRpbC51cmxQYXJzZSh1cmwpKSk7XG4gICAgfTtcbiAgICBhc3NlcnRVcmwoJ2h0dHA6Ly8nKTtcbiAgICBhc3NlcnRVcmwoJ2h0dHA6Ly93d3cuZXhhbXBsZS5jb20nKTtcbiAgICBhc3NlcnRVcmwoJ2h0dHA6Ly91c2VyOnBhc3NAd3d3LmV4YW1wbGUuY29tJyk7XG4gICAgYXNzZXJ0VXJsKCdodHRwOi8vd3d3LmV4YW1wbGUuY29tOjgwJyk7XG4gICAgYXNzZXJ0VXJsKCdodHRwOi8vd3d3LmV4YW1wbGUuY29tLycpO1xuICAgIGFzc2VydFVybCgnaHR0cDovL3d3dy5leGFtcGxlLmNvbS9mb28vYmFyJyk7XG4gICAgYXNzZXJ0VXJsKCdodHRwOi8vd3d3LmV4YW1wbGUuY29tL2Zvby9iYXIvJyk7XG4gICAgYXNzZXJ0VXJsKCdodHRwOi8vdXNlcjpwYXNzQHd3dy5leGFtcGxlLmNvbTo4MC9mb28vYmFyLycpO1xuXG4gICAgYXNzZXJ0VXJsKCcvLycpO1xuICAgIGFzc2VydFVybCgnLy93d3cuZXhhbXBsZS5jb20nKTtcbiAgICBhc3NlcnRVcmwoJ2ZpbGU6Ly8vd3d3LmV4YW1wbGUuY29tJyk7XG5cbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC51cmxQYXJzZSgnJyksIG51bGwpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLnVybFBhcnNlKCcuJyksIG51bGwpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLnVybFBhcnNlKCcuLicpLCBudWxsKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC51cmxQYXJzZSgnYScpLCBudWxsKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC51cmxQYXJzZSgnYS9iJyksIG51bGwpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLnVybFBhcnNlKCdhLy9iJyksIG51bGwpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLnVybFBhcnNlKCcvYScpLCBudWxsKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC51cmxQYXJzZSgnZGF0YTpmb28sYmFyJyksIG51bGwpO1xuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3Qgbm9ybWFsaXplKCknXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJy8uLicpLCAnLycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnLy4uLycpLCAnLycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnLy4uLy4uLy4uLy4uJyksICcvJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcvLi4vLi4vLi4vLi4vYS9iL2MnKSwgJy9hL2IvYycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnL2EvYi9jLy4uLy4uLy4uL2QvLi4vLi4vZScpLCAnL2UnKTtcblxuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnLi4nKSwgJy4uJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcuLi8nKSwgJy4uLycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnLi4vLi4vYS8nKSwgJy4uLy4uL2EvJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCdhLy4uJyksICcuJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCdhLy4uLy4uLy4uJyksICcuLi8uLicpO1xuXG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcvLicpLCAnLycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnLy4vJyksICcvJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcvLi8uLy4vLicpLCAnLycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnLy4vLi8uLy4vYS9iL2MnKSwgJy9hL2IvYycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnL2EvYi9jLy4vLi8uL2QvLi8uL2UnKSwgJy9hL2IvYy9kL2UnKTtcblxuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnJyksICcuJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcuJyksICcuJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcuLycpLCAnLicpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnLi8uL2EnKSwgJ2EnKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJ2EvLi8nKSwgJ2EvJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCdhLy4vLi8uJyksICdhJyk7XG5cbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJy9hL2IvL2MvLy8vZC8vLy8vJyksICcvYS9iL2MvZC8nKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJy8vL2EvYi8vYy8vLy9kLy8vLy8nKSwgJy8vL2EvYi9jL2QvJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCdhL2IvL2MvLy8vZCcpLCAnYS9iL2MvZCcpO1xuXG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcuLy8vLi4vLi8uLi9hL2IvLy4vLi4nKSwgJy4uLy4uL2EnKVxuXG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCdodHRwOi8vd3d3LmV4YW1wbGUuY29tJyksICdodHRwOi8vd3d3LmV4YW1wbGUuY29tJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCdodHRwOi8vd3d3LmV4YW1wbGUuY29tLycpLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbS8nKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJ2h0dHA6Ly93d3cuZXhhbXBsZS5jb20vLi8uLi8vYS9iL2MvLi4vLi9kLy8nKSwgJ2h0dHA6Ly93d3cuZXhhbXBsZS5jb20vYS9iL2QvJyk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBqb2luKCknXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhJywgJ2InKSwgJ2EvYicpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2EvJywgJ2InKSwgJ2EvYicpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2EvLycsICdiJyksICdhL2InKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhJywgJ2IvJyksICdhL2IvJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignYScsICdiLy8nKSwgJ2EvYi8nKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhLycsICcvYicpLCAnL2InKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhLy8nLCAnLy9iJyksICcvL2InKTtcblxuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2EnLCAnLi4nKSwgJy4nKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhJywgJy4uL2InKSwgJ2InKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhL2InLCAnLi4vYycpLCAnYS9jJyk7XG5cbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhJywgJy4nKSwgJ2EnKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhJywgJy4vYicpLCAnYS9iJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignYS9iJywgJy4vYycpLCAnYS9iL2MnKTtcblxuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2EnLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2EnLCAnZGF0YTpmb28sYmFyJyksICdkYXRhOmZvbyxiYXInKTtcblxuXG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignJywgJ2InKSwgJ2InKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcuJywgJ2InKSwgJ2InKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcnLCAnYi8nKSwgJ2IvJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLicsICdiLycpLCAnYi8nKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcnLCAnYi8vJyksICdiLycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4nLCAnYi8vJyksICdiLycpO1xuXG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignJywgJy4uJyksICcuLicpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4nLCAnLi4nKSwgJy4uJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignJywgJy4uL2InKSwgJy4uL2InKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcuJywgJy4uL2InKSwgJy4uL2InKTtcblxuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJycsICcuJyksICcuJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLicsICcuJyksICcuJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignJywgJy4vYicpLCAnYicpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4nLCAnLi9iJyksICdiJyk7XG5cbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcnLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4nLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJycsICdkYXRhOmZvbyxiYXInKSwgJ2RhdGE6Zm9vLGJhcicpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4nLCAnZGF0YTpmb28sYmFyJyksICdkYXRhOmZvbyxiYXInKTtcblxuXG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLi4nLCAnYicpLCAnLi4vYicpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4uJywgJ2IvJyksICcuLi9iLycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4uJywgJ2IvLycpLCAnLi4vYi8nKTtcblxuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4uJywgJy4uJyksICcuLi8uLicpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4uJywgJy4uL2InKSwgJy4uLy4uL2InKTtcblxuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4uJywgJy4nKSwgJy4uJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLi4nLCAnLi9iJyksICcuLi9iJyk7XG5cbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcuLicsICdodHRwOi8vd3d3LmV4YW1wbGUuY29tJyksICdodHRwOi8vd3d3LmV4YW1wbGUuY29tJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLi4nLCAnZGF0YTpmb28sYmFyJyksICdkYXRhOmZvbyxiYXInKTtcblxuXG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignYScsICcnKSwgJ2EnKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhJywgJy4nKSwgJ2EnKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhLycsICcnKSwgJ2EnKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhLycsICcuJyksICdhJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignYS8vJywgJycpLCAnYScpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2EvLycsICcuJyksICdhJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignL2EnLCAnJyksICcvYScpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy9hJywgJy4nKSwgJy9hJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignJywgJycpLCAnLicpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4nLCAnJyksICcuJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLicsICcnKSwgJy4nKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcuJywgJy4nKSwgJy4nKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcuLicsICcnKSwgJy4uJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLi4nLCAnLicpLCAnLi4nKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hJywgJycpLCAnaHR0cDovL2Zvby5vcmcvYScpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EnLCAnLicpLCAnaHR0cDovL2Zvby5vcmcvYScpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EvJywgJycpLCAnaHR0cDovL2Zvby5vcmcvYScpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EvJywgJy4nKSwgJ2h0dHA6Ly9mb28ub3JnL2EnKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hLy8nLCAnJyksICdodHRwOi8vZm9vLm9yZy9hJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcvYS8vJywgJy4nKSwgJ2h0dHA6Ly9mb28ub3JnL2EnKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZycsICcnKSwgJ2h0dHA6Ly9mb28ub3JnLycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnJywgJy4nKSwgJ2h0dHA6Ly9mb28ub3JnLycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnLycsICcnKSwgJ2h0dHA6Ly9mb28ub3JnLycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnLycsICcuJyksICdodHRwOi8vZm9vLm9yZy8nKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy8vJywgJycpLCAnaHR0cDovL2Zvby5vcmcvJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcvLycsICcuJyksICdodHRwOi8vZm9vLm9yZy8nKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcvL3d3dy5leGFtcGxlLmNvbScsICcnKSwgJy8vd3d3LmV4YW1wbGUuY29tLycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy8vd3d3LmV4YW1wbGUuY29tJywgJy4nKSwgJy8vd3d3LmV4YW1wbGUuY29tLycpO1xuXG5cbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hJywgJ2InKSwgJ2h0dHA6Ly9mb28ub3JnL2EvYicpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EvJywgJ2InKSwgJ2h0dHA6Ly9mb28ub3JnL2EvYicpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EvLycsICdiJyksICdodHRwOi8vZm9vLm9yZy9hL2InKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hJywgJ2IvJyksICdodHRwOi8vZm9vLm9yZy9hL2IvJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcvYScsICdiLy8nKSwgJ2h0dHA6Ly9mb28ub3JnL2EvYi8nKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hLycsICcvYicpLCAnaHR0cDovL2Zvby5vcmcvYicpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EvLycsICcvL2InKSwgJ2h0dHA6Ly9iJyk7XG5cbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hJywgJy4uJyksICdodHRwOi8vZm9vLm9yZy8nKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hJywgJy4uL2InKSwgJ2h0dHA6Ly9mb28ub3JnL2InKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hL2InLCAnLi4vYycpLCAnaHR0cDovL2Zvby5vcmcvYS9jJyk7XG5cbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hJywgJy4nKSwgJ2h0dHA6Ly9mb28ub3JnL2EnKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hJywgJy4vYicpLCAnaHR0cDovL2Zvby5vcmcvYS9iJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcvYS9iJywgJy4vYycpLCAnaHR0cDovL2Zvby5vcmcvYS9iL2MnKTtcblxuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EnLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EnLCAnZGF0YTpmb28sYmFyJyksICdkYXRhOmZvbyxiYXInKTtcblxuXG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcnLCAnYScpLCAnaHR0cDovL2Zvby5vcmcvYScpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnLycsICdhJyksICdodHRwOi8vZm9vLm9yZy9hJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcvLycsICdhJyksICdodHRwOi8vZm9vLm9yZy9hJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcnLCAnL2EnKSwgJ2h0dHA6Ly9mb28ub3JnL2EnKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy8nLCAnL2EnKSwgJ2h0dHA6Ly9mb28ub3JnL2EnKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy8vJywgJy9hJyksICdodHRwOi8vZm9vLm9yZy9hJyk7XG5cblxuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly8nLCAnd3d3LmV4YW1wbGUuY29tJyksICdodHRwOi8vd3d3LmV4YW1wbGUuY29tJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignZmlsZTovLy8nLCAnd3d3LmV4YW1wbGUuY29tJyksICdmaWxlOi8vL3d3dy5leGFtcGxlLmNvbScpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly8nLCAnZnRwOi8vZXhhbXBsZS5jb20nKSwgJ2Z0cDovL2V4YW1wbGUuY29tJyk7XG5cbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vd3d3LmV4YW1wbGUuY29tJywgJy8vZm9vLm9yZy9iYXInKSwgJ2h0dHA6Ly9mb28ub3JnL2JhcicpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy8vd3d3LmV4YW1wbGUuY29tJywgJy8vZm9vLm9yZy9iYXInKSwgJy8vZm9vLm9yZy9iYXInKTtcbiAgfTtcblxuICAvLyBUT0RPIElzc3VlICMxMjg6IERlZmluZSBhbmQgdGVzdCB0aGlzIGZ1bmN0aW9uIHByb3Blcmx5LlxuICBleHBvcnRzWyd0ZXN0IHJlbGF0aXZlKCknXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5yZWxhdGl2ZSgnL3RoZS9yb290JywgJy90aGUvcm9vdC9vbmUuanMnKSwgJ29uZS5qcycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLnJlbGF0aXZlKCdodHRwOi8vdGhlL3Jvb3QnLCAnaHR0cDovL3RoZS9yb290L29uZS5qcycpLCAnb25lLmpzJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwucmVsYXRpdmUoJy90aGUvcm9vdCcsICcvdGhlL3Jvb3RvbmUuanMnKSwgJy4uL3Jvb3RvbmUuanMnKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5yZWxhdGl2ZSgnaHR0cDovL3RoZS9yb290JywgJ2h0dHA6Ly90aGUvcm9vdG9uZS5qcycpLCAnLi4vcm9vdG9uZS5qcycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLnJlbGF0aXZlKCcvdGhlL3Jvb3QnLCAnL3RoZXJvb3RvbmUuanMnKSwgJy90aGVyb290b25lLmpzJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwucmVsYXRpdmUoJ2h0dHA6Ly90aGUvcm9vdCcsICcvdGhlcm9vdG9uZS5qcycpLCAnL3RoZXJvb3RvbmUuanMnKTtcblxuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLnJlbGF0aXZlKCcnLCAnL3RoZS9yb290L29uZS5qcycpLCAnL3RoZS9yb290L29uZS5qcycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLnJlbGF0aXZlKCcuJywgJy90aGUvcm9vdC9vbmUuanMnKSwgJy90aGUvcm9vdC9vbmUuanMnKTtcbiAgICBhc3NlcnQuZXF1YWwobGliVXRpbC5yZWxhdGl2ZSgnJywgJ3RoZS9yb290L29uZS5qcycpLCAndGhlL3Jvb3Qvb25lLmpzJyk7XG4gICAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwucmVsYXRpdmUoJy4nLCAndGhlL3Jvb3Qvb25lLmpzJyksICd0aGUvcm9vdC9vbmUuanMnKTtcblxuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLnJlbGF0aXZlKCcvJywgJy90aGUvcm9vdC9vbmUuanMnKSwgJ3RoZS9yb290L29uZS5qcycpO1xuICAgIGFzc2VydC5lcXVhbChsaWJVdGlsLnJlbGF0aXZlKCcvJywgJ3RoZS9yb290L29uZS5qcycpLCAndGhlL3Jvb3Qvb25lLmpzJyk7XG4gIH07XG59XG5cblxuXG4vKioqKioqKioqKioqKioqKipcbiAqKiBXRUJQQUNLIEZPT1RFUlxuICoqIC4vdGVzdC90ZXN0LXV0aWwuanNcbiAqKiBtb2R1bGUgaWQgPSAwXG4gKiogbW9kdWxlIGNodW5rcyA9IDBcbiAqKi8iLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG57XG4gIC8qKlxuICAgKiBUaGlzIGlzIGEgaGVscGVyIGZ1bmN0aW9uIGZvciBnZXR0aW5nIHZhbHVlcyBmcm9tIHBhcmFtZXRlci9vcHRpb25zXG4gICAqIG9iamVjdHMuXG4gICAqXG4gICAqIEBwYXJhbSBhcmdzIFRoZSBvYmplY3Qgd2UgYXJlIGV4dHJhY3RpbmcgdmFsdWVzIGZyb21cbiAgICogQHBhcmFtIG5hbWUgVGhlIG5hbWUgb2YgdGhlIHByb3BlcnR5IHdlIGFyZSBnZXR0aW5nLlxuICAgKiBAcGFyYW0gZGVmYXVsdFZhbHVlIEFuIG9wdGlvbmFsIHZhbHVlIHRvIHJldHVybiBpZiB0aGUgcHJvcGVydHkgaXMgbWlzc2luZ1xuICAgKiBmcm9tIHRoZSBvYmplY3QuIElmIHRoaXMgaXMgbm90IHNwZWNpZmllZCBhbmQgdGhlIHByb3BlcnR5IGlzIG1pc3NpbmcsIGFuXG4gICAqIGVycm9yIHdpbGwgYmUgdGhyb3duLlxuICAgKi9cbiAgZnVuY3Rpb24gZ2V0QXJnKGFBcmdzLCBhTmFtZSwgYURlZmF1bHRWYWx1ZSkge1xuICAgIGlmIChhTmFtZSBpbiBhQXJncykge1xuICAgICAgcmV0dXJuIGFBcmdzW2FOYW1lXTtcbiAgICB9IGVsc2UgaWYgKGFyZ3VtZW50cy5sZW5ndGggPT09IDMpIHtcbiAgICAgIHJldHVybiBhRGVmYXVsdFZhbHVlO1xuICAgIH0gZWxzZSB7XG4gICAgICB0aHJvdyBuZXcgRXJyb3IoJ1wiJyArIGFOYW1lICsgJ1wiIGlzIGEgcmVxdWlyZWQgYXJndW1lbnQuJyk7XG4gICAgfVxuICB9XG4gIGV4cG9ydHMuZ2V0QXJnID0gZ2V0QXJnO1xuXG4gIHZhciB1cmxSZWdleHAgPSAvXig/OihbXFx3K1xcLS5dKyk6KT9cXC9cXC8oPzooXFx3KzpcXHcrKUApPyhbXFx3Ll0qKSg/OjooXFxkKykpPyhcXFMqKSQvO1xuICB2YXIgZGF0YVVybFJlZ2V4cCA9IC9eZGF0YTouK1xcLC4rJC87XG5cbiAgZnVuY3Rpb24gdXJsUGFyc2UoYVVybCkge1xuICAgIHZhciBtYXRjaCA9IGFVcmwubWF0Y2godXJsUmVnZXhwKTtcbiAgICBpZiAoIW1hdGNoKSB7XG4gICAgICByZXR1cm4gbnVsbDtcbiAgICB9XG4gICAgcmV0dXJuIHtcbiAgICAgIHNjaGVtZTogbWF0Y2hbMV0sXG4gICAgICBhdXRoOiBtYXRjaFsyXSxcbiAgICAgIGhvc3Q6IG1hdGNoWzNdLFxuICAgICAgcG9ydDogbWF0Y2hbNF0sXG4gICAgICBwYXRoOiBtYXRjaFs1XVxuICAgIH07XG4gIH1cbiAgZXhwb3J0cy51cmxQYXJzZSA9IHVybFBhcnNlO1xuXG4gIGZ1bmN0aW9uIHVybEdlbmVyYXRlKGFQYXJzZWRVcmwpIHtcbiAgICB2YXIgdXJsID0gJyc7XG4gICAgaWYgKGFQYXJzZWRVcmwuc2NoZW1lKSB7XG4gICAgICB1cmwgKz0gYVBhcnNlZFVybC5zY2hlbWUgKyAnOic7XG4gICAgfVxuICAgIHVybCArPSAnLy8nO1xuICAgIGlmIChhUGFyc2VkVXJsLmF1dGgpIHtcbiAgICAgIHVybCArPSBhUGFyc2VkVXJsLmF1dGggKyAnQCc7XG4gICAgfVxuICAgIGlmIChhUGFyc2VkVXJsLmhvc3QpIHtcbiAgICAgIHVybCArPSBhUGFyc2VkVXJsLmhvc3Q7XG4gICAgfVxuICAgIGlmIChhUGFyc2VkVXJsLnBvcnQpIHtcbiAgICAgIHVybCArPSBcIjpcIiArIGFQYXJzZWRVcmwucG9ydFxuICAgIH1cbiAgICBpZiAoYVBhcnNlZFVybC5wYXRoKSB7XG4gICAgICB1cmwgKz0gYVBhcnNlZFVybC5wYXRoO1xuICAgIH1cbiAgICByZXR1cm4gdXJsO1xuICB9XG4gIGV4cG9ydHMudXJsR2VuZXJhdGUgPSB1cmxHZW5lcmF0ZTtcblxuICAvKipcbiAgICogTm9ybWFsaXplcyBhIHBhdGgsIG9yIHRoZSBwYXRoIHBvcnRpb24gb2YgYSBVUkw6XG4gICAqXG4gICAqIC0gUmVwbGFjZXMgY29uc2VxdXRpdmUgc2xhc2hlcyB3aXRoIG9uZSBzbGFzaC5cbiAgICogLSBSZW1vdmVzIHVubmVjZXNzYXJ5ICcuJyBwYXJ0cy5cbiAgICogLSBSZW1vdmVzIHVubmVjZXNzYXJ5ICc8ZGlyPi8uLicgcGFydHMuXG4gICAqXG4gICAqIEJhc2VkIG9uIGNvZGUgaW4gdGhlIE5vZGUuanMgJ3BhdGgnIGNvcmUgbW9kdWxlLlxuICAgKlxuICAgKiBAcGFyYW0gYVBhdGggVGhlIHBhdGggb3IgdXJsIHRvIG5vcm1hbGl6ZS5cbiAgICovXG4gIGZ1bmN0aW9uIG5vcm1hbGl6ZShhUGF0aCkge1xuICAgIHZhciBwYXRoID0gYVBhdGg7XG4gICAgdmFyIHVybCA9IHVybFBhcnNlKGFQYXRoKTtcbiAgICBpZiAodXJsKSB7XG4gICAgICBpZiAoIXVybC5wYXRoKSB7XG4gICAgICAgIHJldHVybiBhUGF0aDtcbiAgICAgIH1cbiAgICAgIHBhdGggPSB1cmwucGF0aDtcbiAgICB9XG4gICAgdmFyIGlzQWJzb2x1dGUgPSBleHBvcnRzLmlzQWJzb2x1dGUocGF0aCk7XG5cbiAgICB2YXIgcGFydHMgPSBwYXRoLnNwbGl0KC9cXC8rLyk7XG4gICAgZm9yICh2YXIgcGFydCwgdXAgPSAwLCBpID0gcGFydHMubGVuZ3RoIC0gMTsgaSA+PSAwOyBpLS0pIHtcbiAgICAgIHBhcnQgPSBwYXJ0c1tpXTtcbiAgICAgIGlmIChwYXJ0ID09PSAnLicpIHtcbiAgICAgICAgcGFydHMuc3BsaWNlKGksIDEpO1xuICAgICAgfSBlbHNlIGlmIChwYXJ0ID09PSAnLi4nKSB7XG4gICAgICAgIHVwKys7XG4gICAgICB9IGVsc2UgaWYgKHVwID4gMCkge1xuICAgICAgICBpZiAocGFydCA9PT0gJycpIHtcbiAgICAgICAgICAvLyBUaGUgZmlyc3QgcGFydCBpcyBibGFuayBpZiB0aGUgcGF0aCBpcyBhYnNvbHV0ZS4gVHJ5aW5nIHRvIGdvXG4gICAgICAgICAgLy8gYWJvdmUgdGhlIHJvb3QgaXMgYSBuby1vcC4gVGhlcmVmb3JlIHdlIGNhbiByZW1vdmUgYWxsICcuLicgcGFydHNcbiAgICAgICAgICAvLyBkaXJlY3RseSBhZnRlciB0aGUgcm9vdC5cbiAgICAgICAgICBwYXJ0cy5zcGxpY2UoaSArIDEsIHVwKTtcbiAgICAgICAgICB1cCA9IDA7XG4gICAgICAgIH0gZWxzZSB7XG4gICAgICAgICAgcGFydHMuc3BsaWNlKGksIDIpO1xuICAgICAgICAgIHVwLS07XG4gICAgICAgIH1cbiAgICAgIH1cbiAgICB9XG4gICAgcGF0aCA9IHBhcnRzLmpvaW4oJy8nKTtcblxuICAgIGlmIChwYXRoID09PSAnJykge1xuICAgICAgcGF0aCA9IGlzQWJzb2x1dGUgPyAnLycgOiAnLic7XG4gICAgfVxuXG4gICAgaWYgKHVybCkge1xuICAgICAgdXJsLnBhdGggPSBwYXRoO1xuICAgICAgcmV0dXJuIHVybEdlbmVyYXRlKHVybCk7XG4gICAgfVxuICAgIHJldHVybiBwYXRoO1xuICB9XG4gIGV4cG9ydHMubm9ybWFsaXplID0gbm9ybWFsaXplO1xuXG4gIC8qKlxuICAgKiBKb2lucyB0d28gcGF0aHMvVVJMcy5cbiAgICpcbiAgICogQHBhcmFtIGFSb290IFRoZSByb290IHBhdGggb3IgVVJMLlxuICAgKiBAcGFyYW0gYVBhdGggVGhlIHBhdGggb3IgVVJMIHRvIGJlIGpvaW5lZCB3aXRoIHRoZSByb290LlxuICAgKlxuICAgKiAtIElmIGFQYXRoIGlzIGEgVVJMIG9yIGEgZGF0YSBVUkksIGFQYXRoIGlzIHJldHVybmVkLCB1bmxlc3MgYVBhdGggaXMgYVxuICAgKiAgIHNjaGVtZS1yZWxhdGl2ZSBVUkw6IFRoZW4gdGhlIHNjaGVtZSBvZiBhUm9vdCwgaWYgYW55LCBpcyBwcmVwZW5kZWRcbiAgICogICBmaXJzdC5cbiAgICogLSBPdGhlcndpc2UgYVBhdGggaXMgYSBwYXRoLiBJZiBhUm9vdCBpcyBhIFVSTCwgdGhlbiBpdHMgcGF0aCBwb3J0aW9uXG4gICAqICAgaXMgdXBkYXRlZCB3aXRoIHRoZSByZXN1bHQgYW5kIGFSb290IGlzIHJldHVybmVkLiBPdGhlcndpc2UgdGhlIHJlc3VsdFxuICAgKiAgIGlzIHJldHVybmVkLlxuICAgKiAgIC0gSWYgYVBhdGggaXMgYWJzb2x1dGUsIHRoZSByZXN1bHQgaXMgYVBhdGguXG4gICAqICAgLSBPdGhlcndpc2UgdGhlIHR3byBwYXRocyBhcmUgam9pbmVkIHdpdGggYSBzbGFzaC5cbiAgICogLSBKb2luaW5nIGZvciBleGFtcGxlICdodHRwOi8vJyBhbmQgJ3d3dy5leGFtcGxlLmNvbScgaXMgYWxzbyBzdXBwb3J0ZWQuXG4gICAqL1xuICBmdW5jdGlvbiBqb2luKGFSb290LCBhUGF0aCkge1xuICAgIGlmIChhUm9vdCA9PT0gXCJcIikge1xuICAgICAgYVJvb3QgPSBcIi5cIjtcbiAgICB9XG4gICAgaWYgKGFQYXRoID09PSBcIlwiKSB7XG4gICAgICBhUGF0aCA9IFwiLlwiO1xuICAgIH1cbiAgICB2YXIgYVBhdGhVcmwgPSB1cmxQYXJzZShhUGF0aCk7XG4gICAgdmFyIGFSb290VXJsID0gdXJsUGFyc2UoYVJvb3QpO1xuICAgIGlmIChhUm9vdFVybCkge1xuICAgICAgYVJvb3QgPSBhUm9vdFVybC5wYXRoIHx8ICcvJztcbiAgICB9XG5cbiAgICAvLyBgam9pbihmb28sICcvL3d3dy5leGFtcGxlLm9yZycpYFxuICAgIGlmIChhUGF0aFVybCAmJiAhYVBhdGhVcmwuc2NoZW1lKSB7XG4gICAgICBpZiAoYVJvb3RVcmwpIHtcbiAgICAgICAgYVBhdGhVcmwuc2NoZW1lID0gYVJvb3RVcmwuc2NoZW1lO1xuICAgICAgfVxuICAgICAgcmV0dXJuIHVybEdlbmVyYXRlKGFQYXRoVXJsKTtcbiAgICB9XG5cbiAgICBpZiAoYVBhdGhVcmwgfHwgYVBhdGgubWF0Y2goZGF0YVVybFJlZ2V4cCkpIHtcbiAgICAgIHJldHVybiBhUGF0aDtcbiAgICB9XG5cbiAgICAvLyBgam9pbignaHR0cDovLycsICd3d3cuZXhhbXBsZS5jb20nKWBcbiAgICBpZiAoYVJvb3RVcmwgJiYgIWFSb290VXJsLmhvc3QgJiYgIWFSb290VXJsLnBhdGgpIHtcbiAgICAgIGFSb290VXJsLmhvc3QgPSBhUGF0aDtcbiAgICAgIHJldHVybiB1cmxHZW5lcmF0ZShhUm9vdFVybCk7XG4gICAgfVxuXG4gICAgdmFyIGpvaW5lZCA9IGFQYXRoLmNoYXJBdCgwKSA9PT0gJy8nXG4gICAgICA/IGFQYXRoXG4gICAgICA6IG5vcm1hbGl6ZShhUm9vdC5yZXBsYWNlKC9cXC8rJC8sICcnKSArICcvJyArIGFQYXRoKTtcblxuICAgIGlmIChhUm9vdFVybCkge1xuICAgICAgYVJvb3RVcmwucGF0aCA9IGpvaW5lZDtcbiAgICAgIHJldHVybiB1cmxHZW5lcmF0ZShhUm9vdFVybCk7XG4gICAgfVxuICAgIHJldHVybiBqb2luZWQ7XG4gIH1cbiAgZXhwb3J0cy5qb2luID0gam9pbjtcblxuICBleHBvcnRzLmlzQWJzb2x1dGUgPSBmdW5jdGlvbiAoYVBhdGgpIHtcbiAgICByZXR1cm4gYVBhdGguY2hhckF0KDApID09PSAnLycgfHwgISFhUGF0aC5tYXRjaCh1cmxSZWdleHApO1xuICB9O1xuXG4gIC8qKlxuICAgKiBNYWtlIGEgcGF0aCByZWxhdGl2ZSB0byBhIFVSTCBvciBhbm90aGVyIHBhdGguXG4gICAqXG4gICAqIEBwYXJhbSBhUm9vdCBUaGUgcm9vdCBwYXRoIG9yIFVSTC5cbiAgICogQHBhcmFtIGFQYXRoIFRoZSBwYXRoIG9yIFVSTCB0byBiZSBtYWRlIHJlbGF0aXZlIHRvIGFSb290LlxuICAgKi9cbiAgZnVuY3Rpb24gcmVsYXRpdmUoYVJvb3QsIGFQYXRoKSB7XG4gICAgaWYgKGFSb290ID09PSBcIlwiKSB7XG4gICAgICBhUm9vdCA9IFwiLlwiO1xuICAgIH1cblxuICAgIGFSb290ID0gYVJvb3QucmVwbGFjZSgvXFwvJC8sICcnKTtcblxuICAgIC8vIEl0IGlzIHBvc3NpYmxlIGZvciB0aGUgcGF0aCB0byBiZSBhYm92ZSB0aGUgcm9vdC4gSW4gdGhpcyBjYXNlLCBzaW1wbHlcbiAgICAvLyBjaGVja2luZyB3aGV0aGVyIHRoZSByb290IGlzIGEgcHJlZml4IG9mIHRoZSBwYXRoIHdvbid0IHdvcmsuIEluc3RlYWQsIHdlXG4gICAgLy8gbmVlZCB0byByZW1vdmUgY29tcG9uZW50cyBmcm9tIHRoZSByb290IG9uZSBieSBvbmUsIHVudGlsIGVpdGhlciB3ZSBmaW5kXG4gICAgLy8gYSBwcmVmaXggdGhhdCBmaXRzLCBvciB3ZSBydW4gb3V0IG9mIGNvbXBvbmVudHMgdG8gcmVtb3ZlLlxuICAgIHZhciBsZXZlbCA9IDA7XG4gICAgd2hpbGUgKGFQYXRoLmluZGV4T2YoYVJvb3QgKyAnLycpICE9PSAwKSB7XG4gICAgICB2YXIgaW5kZXggPSBhUm9vdC5sYXN0SW5kZXhPZihcIi9cIik7XG4gICAgICBpZiAoaW5kZXggPCAwKSB7XG4gICAgICAgIHJldHVybiBhUGF0aDtcbiAgICAgIH1cblxuICAgICAgLy8gSWYgdGhlIG9ubHkgcGFydCBvZiB0aGUgcm9vdCB0aGF0IGlzIGxlZnQgaXMgdGhlIHNjaGVtZSAoaS5lLiBodHRwOi8vLFxuICAgICAgLy8gZmlsZTovLy8sIGV0Yy4pLCBvbmUgb3IgbW9yZSBzbGFzaGVzICgvKSwgb3Igc2ltcGx5IG5vdGhpbmcgYXQgYWxsLCB3ZVxuICAgICAgLy8gaGF2ZSBleGhhdXN0ZWQgYWxsIGNvbXBvbmVudHMsIHNvIHRoZSBwYXRoIGlzIG5vdCByZWxhdGl2ZSB0byB0aGUgcm9vdC5cbiAgICAgIGFSb290ID0gYVJvb3Quc2xpY2UoMCwgaW5kZXgpO1xuICAgICAgaWYgKGFSb290Lm1hdGNoKC9eKFteXFwvXSs6XFwvKT9cXC8qJC8pKSB7XG4gICAgICAgIHJldHVybiBhUGF0aDtcbiAgICAgIH1cblxuICAgICAgKytsZXZlbDtcbiAgICB9XG5cbiAgICAvLyBNYWtlIHN1cmUgd2UgYWRkIGEgXCIuLi9cIiBmb3IgZWFjaCBjb21wb25lbnQgd2UgcmVtb3ZlZCBmcm9tIHRoZSByb290LlxuICAgIHJldHVybiBBcnJheShsZXZlbCArIDEpLmpvaW4oXCIuLi9cIikgKyBhUGF0aC5zdWJzdHIoYVJvb3QubGVuZ3RoICsgMSk7XG4gIH1cbiAgZXhwb3J0cy5yZWxhdGl2ZSA9IHJlbGF0aXZlO1xuXG4gIC8qKlxuICAgKiBCZWNhdXNlIGJlaGF2aW9yIGdvZXMgd2Fja3kgd2hlbiB5b3Ugc2V0IGBfX3Byb3RvX19gIG9uIG9iamVjdHMsIHdlXG4gICAqIGhhdmUgdG8gcHJlZml4IGFsbCB0aGUgc3RyaW5ncyBpbiBvdXIgc2V0IHdpdGggYW4gYXJiaXRyYXJ5IGNoYXJhY3Rlci5cbiAgICpcbiAgICogU2VlIGh0dHBzOi8vZ2l0aHViLmNvbS9tb3ppbGxhL3NvdXJjZS1tYXAvcHVsbC8zMSBhbmRcbiAgICogaHR0cHM6Ly9naXRodWIuY29tL21vemlsbGEvc291cmNlLW1hcC9pc3N1ZXMvMzBcbiAgICpcbiAgICogQHBhcmFtIFN0cmluZyBhU3RyXG4gICAqL1xuICBmdW5jdGlvbiB0b1NldFN0cmluZyhhU3RyKSB7XG4gICAgcmV0dXJuICckJyArIGFTdHI7XG4gIH1cbiAgZXhwb3J0cy50b1NldFN0cmluZyA9IHRvU2V0U3RyaW5nO1xuXG4gIGZ1bmN0aW9uIGZyb21TZXRTdHJpbmcoYVN0cikge1xuICAgIHJldHVybiBhU3RyLnN1YnN0cigxKTtcbiAgfVxuICBleHBvcnRzLmZyb21TZXRTdHJpbmcgPSBmcm9tU2V0U3RyaW5nO1xuXG4gIC8qKlxuICAgKiBDb21wYXJhdG9yIGJldHdlZW4gdHdvIG1hcHBpbmdzIHdoZXJlIHRoZSBvcmlnaW5hbCBwb3NpdGlvbnMgYXJlIGNvbXBhcmVkLlxuICAgKlxuICAgKiBPcHRpb25hbGx5IHBhc3MgaW4gYHRydWVgIGFzIGBvbmx5Q29tcGFyZUdlbmVyYXRlZGAgdG8gY29uc2lkZXIgdHdvXG4gICAqIG1hcHBpbmdzIHdpdGggdGhlIHNhbWUgb3JpZ2luYWwgc291cmNlL2xpbmUvY29sdW1uLCBidXQgZGlmZmVyZW50IGdlbmVyYXRlZFxuICAgKiBsaW5lIGFuZCBjb2x1bW4gdGhlIHNhbWUuIFVzZWZ1bCB3aGVuIHNlYXJjaGluZyBmb3IgYSBtYXBwaW5nIHdpdGggYVxuICAgKiBzdHViYmVkIG91dCBtYXBwaW5nLlxuICAgKi9cbiAgZnVuY3Rpb24gY29tcGFyZUJ5T3JpZ2luYWxQb3NpdGlvbnMobWFwcGluZ0EsIG1hcHBpbmdCLCBvbmx5Q29tcGFyZU9yaWdpbmFsKSB7XG4gICAgdmFyIGNtcCA9IG1hcHBpbmdBLnNvdXJjZSAtIG1hcHBpbmdCLnNvdXJjZTtcbiAgICBpZiAoY21wICE9PSAwKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsTGluZSAtIG1hcHBpbmdCLm9yaWdpbmFsTGluZTtcbiAgICBpZiAoY21wICE9PSAwKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsQ29sdW1uIC0gbWFwcGluZ0Iub3JpZ2luYWxDb2x1bW47XG4gICAgaWYgKGNtcCAhPT0gMCB8fCBvbmx5Q29tcGFyZU9yaWdpbmFsKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIGNtcCA9IG1hcHBpbmdBLmdlbmVyYXRlZENvbHVtbiAtIG1hcHBpbmdCLmdlbmVyYXRlZENvbHVtbjtcbiAgICBpZiAoY21wICE9PSAwKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIGNtcCA9IG1hcHBpbmdBLmdlbmVyYXRlZExpbmUgLSBtYXBwaW5nQi5nZW5lcmF0ZWRMaW5lO1xuICAgIGlmIChjbXAgIT09IDApIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgcmV0dXJuIG1hcHBpbmdBLm5hbWUgLSBtYXBwaW5nQi5uYW1lO1xuICB9XG4gIGV4cG9ydHMuY29tcGFyZUJ5T3JpZ2luYWxQb3NpdGlvbnMgPSBjb21wYXJlQnlPcmlnaW5hbFBvc2l0aW9ucztcblxuICAvKipcbiAgICogQ29tcGFyYXRvciBiZXR3ZWVuIHR3byBtYXBwaW5ncyB3aXRoIGRlZmxhdGVkIHNvdXJjZSBhbmQgbmFtZSBpbmRpY2VzIHdoZXJlXG4gICAqIHRoZSBnZW5lcmF0ZWQgcG9zaXRpb25zIGFyZSBjb21wYXJlZC5cbiAgICpcbiAgICogT3B0aW9uYWxseSBwYXNzIGluIGB0cnVlYCBhcyBgb25seUNvbXBhcmVHZW5lcmF0ZWRgIHRvIGNvbnNpZGVyIHR3b1xuICAgKiBtYXBwaW5ncyB3aXRoIHRoZSBzYW1lIGdlbmVyYXRlZCBsaW5lIGFuZCBjb2x1bW4sIGJ1dCBkaWZmZXJlbnRcbiAgICogc291cmNlL25hbWUvb3JpZ2luYWwgbGluZSBhbmQgY29sdW1uIHRoZSBzYW1lLiBVc2VmdWwgd2hlbiBzZWFyY2hpbmcgZm9yIGFcbiAgICogbWFwcGluZyB3aXRoIGEgc3R1YmJlZCBvdXQgbWFwcGluZy5cbiAgICovXG4gIGZ1bmN0aW9uIGNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0RlZmxhdGVkKG1hcHBpbmdBLCBtYXBwaW5nQiwgb25seUNvbXBhcmVHZW5lcmF0ZWQpIHtcbiAgICB2YXIgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkTGluZSAtIG1hcHBpbmdCLmdlbmVyYXRlZExpbmU7XG4gICAgaWYgKGNtcCAhPT0gMCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICBjbXAgPSBtYXBwaW5nQS5nZW5lcmF0ZWRDb2x1bW4gLSBtYXBwaW5nQi5nZW5lcmF0ZWRDb2x1bW47XG4gICAgaWYgKGNtcCAhPT0gMCB8fCBvbmx5Q29tcGFyZUdlbmVyYXRlZCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICBjbXAgPSBtYXBwaW5nQS5zb3VyY2UgLSBtYXBwaW5nQi5zb3VyY2U7XG4gICAgaWYgKGNtcCAhPT0gMCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICBjbXAgPSBtYXBwaW5nQS5vcmlnaW5hbExpbmUgLSBtYXBwaW5nQi5vcmlnaW5hbExpbmU7XG4gICAgaWYgKGNtcCAhPT0gMCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICBjbXAgPSBtYXBwaW5nQS5vcmlnaW5hbENvbHVtbiAtIG1hcHBpbmdCLm9yaWdpbmFsQ29sdW1uO1xuICAgIGlmIChjbXAgIT09IDApIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgcmV0dXJuIG1hcHBpbmdBLm5hbWUgLSBtYXBwaW5nQi5uYW1lO1xuICB9XG4gIGV4cG9ydHMuY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zRGVmbGF0ZWQgPSBjb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNEZWZsYXRlZDtcblxuICBmdW5jdGlvbiBzdHJjbXAoYVN0cjEsIGFTdHIyKSB7XG4gICAgaWYgKGFTdHIxID09PSBhU3RyMikge1xuICAgICAgcmV0dXJuIDA7XG4gICAgfVxuXG4gICAgaWYgKGFTdHIxID4gYVN0cjIpIHtcbiAgICAgIHJldHVybiAxO1xuICAgIH1cblxuICAgIHJldHVybiAtMTtcbiAgfVxuXG4gIC8qKlxuICAgKiBDb21wYXJhdG9yIGJldHdlZW4gdHdvIG1hcHBpbmdzIHdpdGggaW5mbGF0ZWQgc291cmNlIGFuZCBuYW1lIHN0cmluZ3Mgd2hlcmVcbiAgICogdGhlIGdlbmVyYXRlZCBwb3NpdGlvbnMgYXJlIGNvbXBhcmVkLlxuICAgKi9cbiAgZnVuY3Rpb24gY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zSW5mbGF0ZWQobWFwcGluZ0EsIG1hcHBpbmdCKSB7XG4gICAgdmFyIGNtcCA9IG1hcHBpbmdBLmdlbmVyYXRlZExpbmUgLSBtYXBwaW5nQi5nZW5lcmF0ZWRMaW5lO1xuICAgIGlmIChjbXAgIT09IDApIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkQ29sdW1uIC0gbWFwcGluZ0IuZ2VuZXJhdGVkQ29sdW1uO1xuICAgIGlmIChjbXAgIT09IDApIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgY21wID0gc3RyY21wKG1hcHBpbmdBLnNvdXJjZSwgbWFwcGluZ0Iuc291cmNlKTtcbiAgICBpZiAoY21wICE9PSAwKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsTGluZSAtIG1hcHBpbmdCLm9yaWdpbmFsTGluZTtcbiAgICBpZiAoY21wICE9PSAwKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsQ29sdW1uIC0gbWFwcGluZ0Iub3JpZ2luYWxDb2x1bW47XG4gICAgaWYgKGNtcCAhPT0gMCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICByZXR1cm4gc3RyY21wKG1hcHBpbmdBLm5hbWUsIG1hcHBpbmdCLm5hbWUpO1xuICB9XG4gIGV4cG9ydHMuY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zSW5mbGF0ZWQgPSBjb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNJbmZsYXRlZDtcbn1cblxuXG5cbi8qKioqKioqKioqKioqKioqKlxuICoqIFdFQlBBQ0sgRk9PVEVSXG4gKiogLi9saWIvdXRpbC5qc1xuICoqIG1vZHVsZSBpZCA9IDFcbiAqKiBtb2R1bGUgY2h1bmtzID0gMFxuICoqLyJdLCJzb3VyY2VSb290IjoiIn0=