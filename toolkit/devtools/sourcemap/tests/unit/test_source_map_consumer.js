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
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	{
	  var util = __webpack_require__(1);
	  var SourceMapConsumer = __webpack_require__(3).SourceMapConsumer;
	  var IndexedSourceMapConsumer = __webpack_require__(3).IndexedSourceMapConsumer;
	  var BasicSourceMapConsumer = __webpack_require__(3).BasicSourceMapConsumer;
	  var SourceMapGenerator = __webpack_require__(9).SourceMapGenerator;
	
	  exports['test that we can instantiate with a string or an object'] = function (assert) {
	    assert.doesNotThrow(function () {
	      var map = new SourceMapConsumer(util.testMap);
	    });
	    assert.doesNotThrow(function () {
	      var map = new SourceMapConsumer(JSON.stringify(util.testMap));
	    });
	  };
	
	  exports['test that the object returned from new SourceMapConsumer inherits from SourceMapConsumer'] = function (assert) {
	    assert.ok(new SourceMapConsumer(util.testMap) instanceof SourceMapConsumer);
	  }
	
	  exports['test that a BasicSourceMapConsumer is returned for sourcemaps without sections'] = function(assert) {
	    assert.ok(new SourceMapConsumer(util.testMap) instanceof BasicSourceMapConsumer);
	  };
	
	  exports['test that an IndexedSourceMapConsumer is returned for sourcemaps with sections'] = function(assert) {
	    assert.ok(new SourceMapConsumer(util.indexedTestMap) instanceof IndexedSourceMapConsumer);
	  };
	
	  exports['test that the `sources` field has the original sources'] = function (assert) {
	    var map;
	    var sources;
	
	    map = new SourceMapConsumer(util.testMap);
	    sources = map.sources;
	    assert.equal(sources[0], '/the/root/one.js');
	    assert.equal(sources[1], '/the/root/two.js');
	    assert.equal(sources.length, 2);
	
	    map = new SourceMapConsumer(util.indexedTestMap);
	    sources = map.sources;
	    assert.equal(sources[0], '/the/root/one.js');
	    assert.equal(sources[1], '/the/root/two.js');
	    assert.equal(sources.length, 2);
	
	    map = new SourceMapConsumer(util.indexedTestMapDifferentSourceRoots);
	    sources = map.sources;
	    assert.equal(sources[0], '/the/root/one.js');
	    assert.equal(sources[1], '/different/root/two.js');
	    assert.equal(sources.length, 2);
	
	    map = new SourceMapConsumer(util.testMapNoSourceRoot);
	    sources = map.sources;
	    assert.equal(sources[0], 'one.js');
	    assert.equal(sources[1], 'two.js');
	    assert.equal(sources.length, 2);
	
	    map = new SourceMapConsumer(util.testMapEmptySourceRoot);
	    sources = map.sources;
	    assert.equal(sources[0], 'one.js');
	    assert.equal(sources[1], 'two.js');
	    assert.equal(sources.length, 2);
	  };
	
	  exports['test that the source root is reflected in a mapping\'s source field'] = function (assert) {
	    var map;
	    var mapping;
	
	    map = new SourceMapConsumer(util.testMap);
	
	    mapping = map.originalPositionFor({
	      line: 2,
	      column: 1
	    });
	    assert.equal(mapping.source, '/the/root/two.js');
	
	    mapping = map.originalPositionFor({
	      line: 1,
	      column: 1
	    });
	    assert.equal(mapping.source, '/the/root/one.js');
	
	
	    map = new SourceMapConsumer(util.testMapNoSourceRoot);
	
	    mapping = map.originalPositionFor({
	      line: 2,
	      column: 1
	    });
	    assert.equal(mapping.source, 'two.js');
	
	    mapping = map.originalPositionFor({
	      line: 1,
	      column: 1
	    });
	    assert.equal(mapping.source, 'one.js');
	
	
	    map = new SourceMapConsumer(util.testMapEmptySourceRoot);
	
	    mapping = map.originalPositionFor({
	      line: 2,
	      column: 1
	    });
	    assert.equal(mapping.source, 'two.js');
	
	    mapping = map.originalPositionFor({
	      line: 1,
	      column: 1
	    });
	    assert.equal(mapping.source, 'one.js');
	  };
	
	  exports['test mapping tokens back exactly'] = function (assert) {
	    var map = new SourceMapConsumer(util.testMap);
	
	    util.assertMapping(1, 1, '/the/root/one.js', 1, 1, null, null, map, assert);
	    util.assertMapping(1, 5, '/the/root/one.js', 1, 5, null, null, map, assert);
	    util.assertMapping(1, 9, '/the/root/one.js', 1, 11, null, null, map, assert);
	    util.assertMapping(1, 18, '/the/root/one.js', 1, 21, 'bar', null, map, assert);
	    util.assertMapping(1, 21, '/the/root/one.js', 2, 3, null, null, map, assert);
	    util.assertMapping(1, 28, '/the/root/one.js', 2, 10, 'baz', null, map, assert);
	    util.assertMapping(1, 32, '/the/root/one.js', 2, 14, 'bar', null, map, assert);
	
	    util.assertMapping(2, 1, '/the/root/two.js', 1, 1, null, null, map, assert);
	    util.assertMapping(2, 5, '/the/root/two.js', 1, 5, null, null, map, assert);
	    util.assertMapping(2, 9, '/the/root/two.js', 1, 11, null, null, map, assert);
	    util.assertMapping(2, 18, '/the/root/two.js', 1, 21, 'n', null, map, assert);
	    util.assertMapping(2, 21, '/the/root/two.js', 2, 3, null, null, map, assert);
	    util.assertMapping(2, 28, '/the/root/two.js', 2, 10, 'n', null, map, assert);
	  };
	
	  exports['test mapping tokens back exactly in indexed source map'] = function (assert) {
	    var map = new SourceMapConsumer(util.indexedTestMap);
	
	    util.assertMapping(1, 1, '/the/root/one.js', 1, 1, null, null, map, assert);
	    util.assertMapping(1, 5, '/the/root/one.js', 1, 5, null, null, map, assert);
	    util.assertMapping(1, 9, '/the/root/one.js', 1, 11, null, null, map, assert);
	    util.assertMapping(1, 18, '/the/root/one.js', 1, 21, 'bar', null, map, assert);
	    util.assertMapping(1, 21, '/the/root/one.js', 2, 3, null, null, map, assert);
	    util.assertMapping(1, 28, '/the/root/one.js', 2, 10, 'baz', null, map, assert);
	    util.assertMapping(1, 32, '/the/root/one.js', 2, 14, 'bar', null, map, assert);
	
	    util.assertMapping(2, 1, '/the/root/two.js', 1, 1, null, null, map, assert);
	    util.assertMapping(2, 5, '/the/root/two.js', 1, 5, null, null, map, assert);
	    util.assertMapping(2, 9, '/the/root/two.js', 1, 11, null, null, map, assert);
	    util.assertMapping(2, 18, '/the/root/two.js', 1, 21, 'n', null, map, assert);
	    util.assertMapping(2, 21, '/the/root/two.js', 2, 3, null, null, map, assert);
	    util.assertMapping(2, 28, '/the/root/two.js', 2, 10, 'n', null, map, assert);
	  };
	
	
	  exports['test mapping tokens back exactly'] = function (assert) {
	    var map = new SourceMapConsumer(util.testMap);
	
	    util.assertMapping(1, 1, '/the/root/one.js', 1, 1, null, null, map, assert);
	    util.assertMapping(1, 5, '/the/root/one.js', 1, 5, null, null, map, assert);
	    util.assertMapping(1, 9, '/the/root/one.js', 1, 11, null, null, map, assert);
	    util.assertMapping(1, 18, '/the/root/one.js', 1, 21, 'bar', null, map, assert);
	    util.assertMapping(1, 21, '/the/root/one.js', 2, 3, null, null, map, assert);
	    util.assertMapping(1, 28, '/the/root/one.js', 2, 10, 'baz', null, map, assert);
	    util.assertMapping(1, 32, '/the/root/one.js', 2, 14, 'bar', null, map, assert);
	
	    util.assertMapping(2, 1, '/the/root/two.js', 1, 1, null, null, map, assert);
	    util.assertMapping(2, 5, '/the/root/two.js', 1, 5, null, null, map, assert);
	    util.assertMapping(2, 9, '/the/root/two.js', 1, 11, null, null, map, assert);
	    util.assertMapping(2, 18, '/the/root/two.js', 1, 21, 'n', null, map, assert);
	    util.assertMapping(2, 21, '/the/root/two.js', 2, 3, null, null, map, assert);
	    util.assertMapping(2, 28, '/the/root/two.js', 2, 10, 'n', null, map, assert);
	  };
	
	  exports['test mapping tokens fuzzy'] = function (assert) {
	    var map = new SourceMapConsumer(util.testMap);
	
	    // Finding original positions with default (glb) bias.
	    util.assertMapping(1, 20, '/the/root/one.js', 1, 21, 'bar', null, map, assert, true);
	    util.assertMapping(1, 30, '/the/root/one.js', 2, 10, 'baz', null, map, assert, true);
	    util.assertMapping(2, 12, '/the/root/two.js', 1, 11, null, null, map, assert, true);
	
	    // Finding original positions with lub bias.
	    util.assertMapping(1, 16, '/the/root/one.js', 1, 21, 'bar', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, true);
	    util.assertMapping(1, 26, '/the/root/one.js', 2, 10, 'baz', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, true);
	    util.assertMapping(2, 6, '/the/root/two.js', 1, 11, null, SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, true);
	
	    // Finding generated positions with default (glb) bias.
	    util.assertMapping(1, 18, '/the/root/one.js', 1, 22, 'bar', null, map, assert, null, true);
	    util.assertMapping(1, 28, '/the/root/one.js', 2, 13, 'baz', null, map, assert, null, true);
	    util.assertMapping(2, 9, '/the/root/two.js', 1, 16, null, null, map, assert, null, true);
	
	    // Finding generated positions with lub bias.
	    util.assertMapping(1, 18, '/the/root/one.js', 1, 20, 'bar', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, null, true);
	    util.assertMapping(1, 28, '/the/root/one.js', 2, 7, 'baz', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, null, true);
	    util.assertMapping(2, 9, '/the/root/two.js', 1, 6, null, SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, null, true);
	  };
	
	  exports['test mapping tokens fuzzy in indexed source map'] = function (assert) {
	    var map = new SourceMapConsumer(util.indexedTestMap);
	
	    // Finding original positions with default (glb) bias.
	    util.assertMapping(1, 20, '/the/root/one.js', 1, 21, 'bar', null, map, assert, true);
	    util.assertMapping(1, 30, '/the/root/one.js', 2, 10, 'baz', null, map, assert, true);
	    util.assertMapping(2, 12, '/the/root/two.js', 1, 11, null, null, map, assert, true);
	
	    // Finding original positions with lub bias.
	    util.assertMapping(1, 16, '/the/root/one.js', 1, 21, 'bar', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, true);
	    util.assertMapping(1, 26, '/the/root/one.js', 2, 10, 'baz', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, true);
	    util.assertMapping(2, 6, '/the/root/two.js', 1, 11, null, SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, true);
	
	    // Finding generated positions with default (glb) bias.
	    util.assertMapping(1, 18, '/the/root/one.js', 1, 22, 'bar', null, map, assert, null, true);
	    util.assertMapping(1, 28, '/the/root/one.js', 2, 13, 'baz', null, map, assert, null, true);
	    util.assertMapping(2, 9, '/the/root/two.js', 1, 16, null, null, map, assert, null, true);
	
	    // Finding generated positions with lub bias.
	    util.assertMapping(1, 18, '/the/root/one.js', 1, 20, 'bar', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, null, true);
	    util.assertMapping(1, 28, '/the/root/one.js', 2, 7, 'baz', SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, null, true);
	    util.assertMapping(2, 9, '/the/root/two.js', 1, 6, null, SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, null, true);
	  };
	
	  exports['test mappings and end of lines'] = function (assert) {
	    var smg = new SourceMapGenerator({
	      file: 'foo.js'
	    });
	    smg.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 1, column: 1 },
	      source: 'bar.js'
	    });
	    smg.addMapping({
	      original: { line: 2, column: 2 },
	      generated: { line: 2, column: 2 },
	      source: 'bar.js'
	    });
	    smg.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 1, column: 1 },
	      source: 'baz.js'
	    });
	
	    var map = SourceMapConsumer.fromSourceMap(smg);
	
	    // When finding original positions, mappings end at the end of the line.
	    util.assertMapping(2, 1, null, null, null, null, null, map, assert, true)
	
	    // When finding generated positions, mappings do not end at the end of the line.
	    util.assertMapping(1, 1, 'bar.js', 2, 1, null, null, map, assert, null, true);
	
	    // When finding generated positions with, mappings end at the end of the source.
	    util.assertMapping(null, null, 'bar.js', 3, 1, null, SourceMapConsumer.LEAST_UPPER_BOUND, map, assert, null, true);
	  };
	
	  exports['test creating source map consumers with )]}\' prefix'] = function (assert) {
	    assert.doesNotThrow(function () {
	      var map = new SourceMapConsumer(")]}'" + JSON.stringify(util.testMap));
	    });
	  };
	
	  exports['test eachMapping'] = function (assert) {
	    var map;
	
	    map = new SourceMapConsumer(util.testMap);
	    var previousLine = -Infinity;
	    var previousColumn = -Infinity;
	    map.eachMapping(function (mapping) {
	      assert.ok(mapping.generatedLine >= previousLine);
	
	      assert.ok(mapping.source === '/the/root/one.js' || mapping.source === '/the/root/two.js');
	
	      if (mapping.generatedLine === previousLine) {
	        assert.ok(mapping.generatedColumn >= previousColumn);
	        previousColumn = mapping.generatedColumn;
	      }
	      else {
	        previousLine = mapping.generatedLine;
	        previousColumn = -Infinity;
	      }
	    });
	
	    map = new SourceMapConsumer(util.testMapNoSourceRoot);
	    map.eachMapping(function (mapping) {
	      assert.ok(mapping.source === 'one.js' || mapping.source === 'two.js');
	    });
	
	    map = new SourceMapConsumer(util.testMapEmptySourceRoot);
	    map.eachMapping(function (mapping) {
	      assert.ok(mapping.source === 'one.js' || mapping.source === 'two.js');
	    });
	  };
	
	  exports['test eachMapping for indexed source maps'] = function(assert) {
	    var map = new SourceMapConsumer(util.indexedTestMap);
	    var previousLine = -Infinity;
	    var previousColumn = -Infinity;
	    map.eachMapping(function (mapping) {
	      assert.ok(mapping.generatedLine >= previousLine);
	
	      if (mapping.source) {
	        assert.equal(mapping.source.indexOf(util.testMap.sourceRoot), 0);
	      }
	
	      if (mapping.generatedLine === previousLine) {
	        assert.ok(mapping.generatedColumn >= previousColumn);
	        previousColumn = mapping.generatedColumn;
	      }
	      else {
	        previousLine = mapping.generatedLine;
	        previousColumn = -Infinity;
	      }
	    });
	  };
	
	
	  exports['test iterating over mappings in a different order'] = function (assert) {
	    var map = new SourceMapConsumer(util.testMap);
	    var previousLine = -Infinity;
	    var previousColumn = -Infinity;
	    var previousSource = "";
	    map.eachMapping(function (mapping) {
	      assert.ok(mapping.source >= previousSource);
	
	      if (mapping.source === previousSource) {
	        assert.ok(mapping.originalLine >= previousLine);
	
	        if (mapping.originalLine === previousLine) {
	          assert.ok(mapping.originalColumn >= previousColumn);
	          previousColumn = mapping.originalColumn;
	        }
	        else {
	          previousLine = mapping.originalLine;
	          previousColumn = -Infinity;
	        }
	      }
	      else {
	        previousSource = mapping.source;
	        previousLine = -Infinity;
	        previousColumn = -Infinity;
	      }
	    }, null, SourceMapConsumer.ORIGINAL_ORDER);
	  };
	
	  exports['test iterating over mappings in a different order in indexed source maps'] = function (assert) {
	    var map = new SourceMapConsumer(util.indexedTestMap);
	    var previousLine = -Infinity;
	    var previousColumn = -Infinity;
	    var previousSource = "";
	    map.eachMapping(function (mapping) {
	      assert.ok(mapping.source >= previousSource);
	
	      if (mapping.source === previousSource) {
	        assert.ok(mapping.originalLine >= previousLine);
	
	        if (mapping.originalLine === previousLine) {
	          assert.ok(mapping.originalColumn >= previousColumn);
	          previousColumn = mapping.originalColumn;
	        }
	        else {
	          previousLine = mapping.originalLine;
	          previousColumn = -Infinity;
	        }
	      }
	      else {
	        previousSource = mapping.source;
	        previousLine = -Infinity;
	        previousColumn = -Infinity;
	      }
	    }, null, SourceMapConsumer.ORIGINAL_ORDER);
	  };
	
	  exports['test that we can set the context for `this` in eachMapping'] = function (assert) {
	    var map = new SourceMapConsumer(util.testMap);
	    var context = {};
	    map.eachMapping(function () {
	      assert.equal(this, context);
	    }, context);
	  };
	
	  exports['test that we can set the context for `this` in eachMapping in indexed source maps'] = function (assert) {
	    var map = new SourceMapConsumer(util.indexedTestMap);
	    var context = {};
	    map.eachMapping(function () {
	      assert.equal(this, context);
	    }, context);
	  };
	
	  exports['test that the `sourcesContent` field has the original sources'] = function (assert) {
	    var map = new SourceMapConsumer(util.testMapWithSourcesContent);
	    var sourcesContent = map.sourcesContent;
	
	    assert.equal(sourcesContent[0], ' ONE.foo = function (bar) {\n   return baz(bar);\n };');
	    assert.equal(sourcesContent[1], ' TWO.inc = function (n) {\n   return n + 1;\n };');
	    assert.equal(sourcesContent.length, 2);
	  };
	
	  exports['test that we can get the original sources for the sources'] = function (assert) {
	    var map = new SourceMapConsumer(util.testMapWithSourcesContent);
	    var sources = map.sources;
	
	    assert.equal(map.sourceContentFor(sources[0]), ' ONE.foo = function (bar) {\n   return baz(bar);\n };');
	    assert.equal(map.sourceContentFor(sources[1]), ' TWO.inc = function (n) {\n   return n + 1;\n };');
	    assert.equal(map.sourceContentFor("one.js"), ' ONE.foo = function (bar) {\n   return baz(bar);\n };');
	    assert.equal(map.sourceContentFor("two.js"), ' TWO.inc = function (n) {\n   return n + 1;\n };');
	    assert.throws(function () {
	      map.sourceContentFor("");
	    }, Error);
	    assert.throws(function () {
	      map.sourceContentFor("/the/root/three.js");
	    }, Error);
	    assert.throws(function () {
	      map.sourceContentFor("three.js");
	    }, Error);
	  };
	
	  exports['test that we can get the original source content with relative source paths'] = function (assert) {
	    var map = new SourceMapConsumer(util.testMapRelativeSources);
	    var sources = map.sources;
	
	    assert.equal(map.sourceContentFor(sources[0]), ' ONE.foo = function (bar) {\n   return baz(bar);\n };');
	    assert.equal(map.sourceContentFor(sources[1]), ' TWO.inc = function (n) {\n   return n + 1;\n };');
	    assert.equal(map.sourceContentFor("one.js"), ' ONE.foo = function (bar) {\n   return baz(bar);\n };');
	    assert.equal(map.sourceContentFor("two.js"), ' TWO.inc = function (n) {\n   return n + 1;\n };');
	    assert.throws(function () {
	      map.sourceContentFor("");
	    }, Error);
	    assert.throws(function () {
	      map.sourceContentFor("/the/root/three.js");
	    }, Error);
	    assert.throws(function () {
	      map.sourceContentFor("three.js");
	    }, Error);
	  };
	
	  exports['test that we can get the original source content for the sources on an indexed source map'] = function (assert) {
	    var map = new SourceMapConsumer(util.indexedTestMap);
	    var sources = map.sources;
	
	    assert.equal(map.sourceContentFor(sources[0]), ' ONE.foo = function (bar) {\n   return baz(bar);\n };');
	    assert.equal(map.sourceContentFor(sources[1]), ' TWO.inc = function (n) {\n   return n + 1;\n };');
	    assert.equal(map.sourceContentFor("one.js"), ' ONE.foo = function (bar) {\n   return baz(bar);\n };');
	    assert.equal(map.sourceContentFor("two.js"), ' TWO.inc = function (n) {\n   return n + 1;\n };');
	    assert.throws(function () {
	      map.sourceContentFor("");
	    }, Error);
	    assert.throws(function () {
	      map.sourceContentFor("/the/root/three.js");
	    }, Error);
	    assert.throws(function () {
	      map.sourceContentFor("three.js");
	    }, Error);
	  };
	
	  exports['test hasContentsOfAllSources, single source with contents'] = function (assert) {
	    // Has one source: foo.js (with contents).
	    var mapWithContents = new SourceMapGenerator();
	    mapWithContents.addMapping({ source: 'foo.js',
	                                 original: { line: 1, column: 10 },
	                                 generated: { line: 1, column: 10 } });
	    mapWithContents.setSourceContent('foo.js', 'content of foo.js');
	    var consumer = new SourceMapConsumer(mapWithContents.toJSON());
	    assert.ok(consumer.hasContentsOfAllSources());
	  };
	
	  exports['test hasContentsOfAllSources, single source without contents'] = function (assert) {
	    // Has one source: foo.js (without contents).
	    var mapWithoutContents = new SourceMapGenerator();
	    mapWithoutContents.addMapping({ source: 'foo.js',
	                                    original: { line: 1, column: 10 },
	                                    generated: { line: 1, column: 10 } });
	    var consumer = new SourceMapConsumer(mapWithoutContents.toJSON());
	    assert.ok(!consumer.hasContentsOfAllSources());
	  };
	
	  exports['test hasContentsOfAllSources, two sources with contents'] = function (assert) {
	    // Has two sources: foo.js (with contents) and bar.js (with contents).
	    var mapWithBothContents = new SourceMapGenerator();
	    mapWithBothContents.addMapping({ source: 'foo.js',
	                                     original: { line: 1, column: 10 },
	                                     generated: { line: 1, column: 10 } });
	    mapWithBothContents.addMapping({ source: 'bar.js',
	                                     original: { line: 1, column: 10 },
	                                     generated: { line: 1, column: 10 } });
	    mapWithBothContents.setSourceContent('foo.js', 'content of foo.js');
	    mapWithBothContents.setSourceContent('bar.js', 'content of bar.js');
	    var consumer = new SourceMapConsumer(mapWithBothContents.toJSON());
	    assert.ok(consumer.hasContentsOfAllSources());
	  };
	
	  exports['test hasContentsOfAllSources, two sources one with and one without contents'] = function (assert) {
	    // Has two sources: foo.js (with contents) and bar.js (without contents).
	    var mapWithoutSomeContents = new SourceMapGenerator();
	    mapWithoutSomeContents.addMapping({ source: 'foo.js',
	                                        original: { line: 1, column: 10 },
	                                        generated: { line: 1, column: 10 } });
	    mapWithoutSomeContents.addMapping({ source: 'bar.js',
	                                        original: { line: 1, column: 10 },
	                                        generated: { line: 1, column: 10 } });
	    mapWithoutSomeContents.setSourceContent('foo.js', 'content of foo.js');
	    var consumer = new SourceMapConsumer(mapWithoutSomeContents.toJSON());
	    assert.ok(!consumer.hasContentsOfAllSources());
	};
	
	  exports['test sourceRoot + generatedPositionFor'] = function (assert) {
	    var map = new SourceMapGenerator({
	      sourceRoot: 'foo/bar',
	      file: 'baz.js'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 2, column: 2 },
	      source: 'bang.coffee'
	    });
	    map.addMapping({
	      original: { line: 5, column: 5 },
	      generated: { line: 6, column: 6 },
	      source: 'bang.coffee'
	    });
	    map = new SourceMapConsumer(map.toString());
	
	    // Should handle without sourceRoot.
	    var pos = map.generatedPositionFor({
	      line: 1,
	      column: 1,
	      source: 'bang.coffee'
	    });
	
	    assert.equal(pos.line, 2);
	    assert.equal(pos.column, 2);
	
	    // Should handle with sourceRoot.
	    var pos = map.generatedPositionFor({
	      line: 1,
	      column: 1,
	      source: 'foo/bar/bang.coffee'
	    });
	
	    assert.equal(pos.line, 2);
	    assert.equal(pos.column, 2);
	  };
	
	  exports['test sourceRoot + generatedPositionFor for path above the root'] = function (assert) {
	    var map = new SourceMapGenerator({
	      sourceRoot: 'foo/bar',
	      file: 'baz.js'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 2, column: 2 },
	      source: '../bang.coffee'
	    });
	    map = new SourceMapConsumer(map.toString());
	
	    // Should handle with sourceRoot.
	    var pos = map.generatedPositionFor({
	      line: 1,
	      column: 1,
	      source: 'foo/bang.coffee'
	    });
	
	    assert.equal(pos.line, 2);
	    assert.equal(pos.column, 2);
	  };
	
	  exports['test allGeneratedPositionsFor for line'] = function (assert) {
	    var map = new SourceMapGenerator({
	      file: 'generated.js'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 2, column: 2 },
	      source: 'foo.coffee'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 2, column: 2 },
	      source: 'bar.coffee'
	    });
	    map.addMapping({
	      original: { line: 2, column: 1 },
	      generated: { line: 3, column: 2 },
	      source: 'bar.coffee'
	    });
	    map.addMapping({
	      original: { line: 2, column: 2 },
	      generated: { line: 3, column: 3 },
	      source: 'bar.coffee'
	    });
	    map.addMapping({
	      original: { line: 3, column: 1 },
	      generated: { line: 4, column: 2 },
	      source: 'bar.coffee'
	    });
	    map = new SourceMapConsumer(map.toString());
	
	    var mappings = map.allGeneratedPositionsFor({
	      line: 2,
	      source: 'bar.coffee'
	    });
	
	    assert.equal(mappings.length, 2);
	    assert.equal(mappings[0].line, 3);
	    assert.equal(mappings[0].column, 2);
	    assert.equal(mappings[1].line, 3);
	    assert.equal(mappings[1].column, 3);
	  };
	
	  exports['test allGeneratedPositionsFor for line fuzzy'] = function (assert) {
	    var map = new SourceMapGenerator({
	      file: 'generated.js'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 2, column: 2 },
	      source: 'foo.coffee'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 2, column: 2 },
	      source: 'bar.coffee'
	    });
	    map.addMapping({
	      original: { line: 3, column: 1 },
	      generated: { line: 4, column: 2 },
	      source: 'bar.coffee'
	    });
	    map = new SourceMapConsumer(map.toString());
	
	    var mappings = map.allGeneratedPositionsFor({
	      line: 2,
	      source: 'bar.coffee'
	    });
	
	    assert.equal(mappings.length, 1);
	    assert.equal(mappings[0].line, 4);
	    assert.equal(mappings[0].column, 2);
	  };
	
	  exports['test allGeneratedPositionsFor for empty source map'] = function (assert) {
	    var map = new SourceMapGenerator({
	      file: 'generated.js'
	    });
	    map = new SourceMapConsumer(map.toString());
	
	    var mappings = map.allGeneratedPositionsFor({
	      line: 2,
	      source: 'bar.coffee'
	    });
	
	    assert.equal(mappings.length, 0);
	  };
	
	  exports['test allGeneratedPositionsFor for column'] = function (assert) {
	    var map = new SourceMapGenerator({
	      file: 'generated.js'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 1, column: 2 },
	      source: 'foo.coffee'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 1, column: 3 },
	      source: 'foo.coffee'
	    });
	    map = new SourceMapConsumer(map.toString());
	
	    var mappings = map.allGeneratedPositionsFor({
	      line: 1,
	      column: 1,
	      source: 'foo.coffee'
	    });
	
	    assert.equal(mappings.length, 2);
	    assert.equal(mappings[0].line, 1);
	    assert.equal(mappings[0].column, 2);
	    assert.equal(mappings[1].line, 1);
	    assert.equal(mappings[1].column, 3);
	  };
	
	  exports['test allGeneratedPositionsFor for column fuzzy'] = function (assert) {
	    var map = new SourceMapGenerator({
	      file: 'generated.js'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 1, column: 2 },
	      source: 'foo.coffee'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 1, column: 3 },
	      source: 'foo.coffee'
	    });
	    map = new SourceMapConsumer(map.toString());
	
	    var mappings = map.allGeneratedPositionsFor({
	      line: 1,
	      column: 0,
	      source: 'foo.coffee'
	    });
	
	    assert.equal(mappings.length, 2);
	    assert.equal(mappings[0].line, 1);
	    assert.equal(mappings[0].column, 2);
	    assert.equal(mappings[1].line, 1);
	    assert.equal(mappings[1].column, 3);
	  };
	
	  exports['test allGeneratedPositionsFor for column on different line fuzzy'] = function (assert) {
	    var map = new SourceMapGenerator({
	      file: 'generated.js'
	    });
	    map.addMapping({
	      original: { line: 2, column: 1 },
	      generated: { line: 2, column: 2 },
	      source: 'foo.coffee'
	    });
	    map.addMapping({
	      original: { line: 2, column: 1 },
	      generated: { line: 2, column: 3 },
	      source: 'foo.coffee'
	    });
	    map = new SourceMapConsumer(map.toString());
	
	    var mappings = map.allGeneratedPositionsFor({
	      line: 1,
	      column: 0,
	      source: 'foo.coffee'
	    });
	
	    assert.equal(mappings.length, 0);
	  };
	
	  exports['test computeColumnSpans'] = function (assert) {
	    var map = new SourceMapGenerator({
	      file: 'generated.js'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 1, column: 1 },
	      source: 'foo.coffee'
	    });
	    map.addMapping({
	      original: { line: 2, column: 1 },
	      generated: { line: 2, column: 1 },
	      source: 'foo.coffee'
	    });
	    map.addMapping({
	      original: { line: 2, column: 2 },
	      generated: { line: 2, column: 10 },
	      source: 'foo.coffee'
	    });
	    map.addMapping({
	      original: { line: 2, column: 3 },
	      generated: { line: 2, column: 20 },
	      source: 'foo.coffee'
	    });
	    map.addMapping({
	      original: { line: 3, column: 1 },
	      generated: { line: 3, column: 1 },
	      source: 'foo.coffee'
	    });
	    map.addMapping({
	      original: { line: 3, column: 2 },
	      generated: { line: 3, column: 2 },
	      source: 'foo.coffee'
	    });
	    map = new SourceMapConsumer(map.toString());
	
	    map.computeColumnSpans();
	
	    var mappings = map.allGeneratedPositionsFor({
	      line: 1,
	      source: 'foo.coffee'
	    });
	
	    assert.equal(mappings.length, 1);
	    assert.equal(mappings[0].lastColumn, Infinity);
	
	    var mappings = map.allGeneratedPositionsFor({
	      line: 2,
	      source: 'foo.coffee'
	    });
	
	    assert.equal(mappings.length, 3);
	    assert.equal(mappings[0].lastColumn, 9);
	    assert.equal(mappings[1].lastColumn, 19);
	    assert.equal(mappings[2].lastColumn, Infinity);
	
	    var mappings = map.allGeneratedPositionsFor({
	      line: 3,
	      source: 'foo.coffee'
	    });
	
	    assert.equal(mappings.length, 2);
	    assert.equal(mappings[0].lastColumn, 1);
	    assert.equal(mappings[1].lastColumn, Infinity);
	  };
	
	  exports['test sourceRoot + originalPositionFor'] = function (assert) {
	    var map = new SourceMapGenerator({
	      sourceRoot: 'foo/bar',
	      file: 'baz.js'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 2, column: 2 },
	      source: 'bang.coffee'
	    });
	    map = new SourceMapConsumer(map.toString());
	
	    var pos = map.originalPositionFor({
	      line: 2,
	      column: 2,
	    });
	
	    // Should always have the prepended source root
	    assert.equal(pos.source, 'foo/bar/bang.coffee');
	    assert.equal(pos.line, 1);
	    assert.equal(pos.column, 1);
	  };
	
	  exports['test github issue #56'] = function (assert) {
	    var map = new SourceMapGenerator({
	      sourceRoot: 'http://',
	      file: 'www.example.com/foo.js'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 2, column: 2 },
	      source: 'www.example.com/original.js'
	    });
	    map = new SourceMapConsumer(map.toString());
	
	    var sources = map.sources;
	    assert.equal(sources.length, 1);
	    assert.equal(sources[0], 'http://www.example.com/original.js');
	  };
	
	  exports['test github issue #43'] = function (assert) {
	    var map = new SourceMapGenerator({
	      sourceRoot: 'http://example.com',
	      file: 'foo.js'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 2, column: 2 },
	      source: 'http://cdn.example.com/original.js'
	    });
	    map = new SourceMapConsumer(map.toString());
	
	    var sources = map.sources;
	    assert.equal(sources.length, 1,
	                 'Should only be one source.');
	    assert.equal(sources[0], 'http://cdn.example.com/original.js',
	                 'Should not be joined with the sourceRoot.');
	  };
	
	  exports['test absolute path, but same host sources'] = function (assert) {
	    var map = new SourceMapGenerator({
	      sourceRoot: 'http://example.com/foo/bar',
	      file: 'foo.js'
	    });
	    map.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 2, column: 2 },
	      source: '/original.js'
	    });
	    map = new SourceMapConsumer(map.toString());
	
	    var sources = map.sources;
	    assert.equal(sources.length, 1,
	                 'Should only be one source.');
	    assert.equal(sources[0], 'http://example.com/original.js',
	                 'Source should be relative the host of the source root.');
	  };
	
	  exports['test indexed source map errors when sections are out of order by line'] = function(assert) {
	    // Make a deep copy of the indexedTestMap
	    var misorderedIndexedTestMap = JSON.parse(JSON.stringify(util.indexedTestMap));
	
	    misorderedIndexedTestMap.sections[0].offset = {
	      line: 2,
	      column: 0
	    };
	
	    assert.throws(function() {
	      new SourceMapConsumer(misorderedIndexedTestMap);
	    }, Error);
	  };
	
	  exports['test github issue #64'] = function (assert) {
	    var map = new SourceMapConsumer({
	      "version": 3,
	      "file": "foo.js",
	      "sourceRoot": "http://example.com/",
	      "sources": ["/a"],
	      "names": [],
	      "mappings": "AACA",
	      "sourcesContent": ["foo"]
	    });
	
	    assert.equal(map.sourceContentFor("a"), "foo");
	    assert.equal(map.sourceContentFor("/a"), "foo");
	  };
	
	  exports['test bug 885597'] = function (assert) {
	    var map = new SourceMapConsumer({
	      "version": 3,
	      "file": "foo.js",
	      "sourceRoot": "file:///Users/AlGore/Invented/The/Internet/",
	      "sources": ["/a"],
	      "names": [],
	      "mappings": "AACA",
	      "sourcesContent": ["foo"]
	    });
	
	    var s = map.sources[0];
	    assert.equal(map.sourceContentFor(s), "foo");
	  };
	
	  exports['test github issue #72, duplicate sources'] = function (assert) {
	    var map = new SourceMapConsumer({
	      "version": 3,
	      "file": "foo.js",
	      "sources": ["source1.js", "source1.js", "source3.js"],
	      "names": [],
	      "mappings": ";EAAC;;IAEE;;MEEE",
	      "sourceRoot": "http://example.com"
	    });
	
	    var pos = map.originalPositionFor({
	      line: 2,
	      column: 2
	    });
	    assert.equal(pos.source, 'http://example.com/source1.js');
	    assert.equal(pos.line, 1);
	    assert.equal(pos.column, 1);
	
	    var pos = map.originalPositionFor({
	      line: 4,
	      column: 4
	    });
	    assert.equal(pos.source, 'http://example.com/source1.js');
	    assert.equal(pos.line, 3);
	    assert.equal(pos.column, 3);
	
	    var pos = map.originalPositionFor({
	      line: 6,
	      column: 6
	    });
	    assert.equal(pos.source, 'http://example.com/source3.js');
	    assert.equal(pos.line, 5);
	    assert.equal(pos.column, 5);
	  };
	
	  exports['test github issue #72, duplicate names'] = function (assert) {
	    var map = new SourceMapConsumer({
	      "version": 3,
	      "file": "foo.js",
	      "sources": ["source.js"],
	      "names": ["name1", "name1", "name3"],
	      "mappings": ";EAACA;;IAEEA;;MAEEE",
	      "sourceRoot": "http://example.com"
	    });
	
	    var pos = map.originalPositionFor({
	      line: 2,
	      column: 2
	    });
	    assert.equal(pos.name, 'name1');
	    assert.equal(pos.line, 1);
	    assert.equal(pos.column, 1);
	
	    var pos = map.originalPositionFor({
	      line: 4,
	      column: 4
	    });
	    assert.equal(pos.name, 'name1');
	    assert.equal(pos.line, 3);
	    assert.equal(pos.column, 3);
	
	    var pos = map.originalPositionFor({
	      line: 6,
	      column: 6
	    });
	    assert.equal(pos.name, 'name3');
	    assert.equal(pos.line, 5);
	    assert.equal(pos.column, 5);
	  };
	
	  exports['test SourceMapConsumer.fromSourceMap'] = function (assert) {
	    var smg = new SourceMapGenerator({
	      sourceRoot: 'http://example.com/',
	      file: 'foo.js'
	    });
	    smg.addMapping({
	      original: { line: 1, column: 1 },
	      generated: { line: 2, column: 2 },
	      source: 'bar.js'
	    });
	    smg.addMapping({
	      original: { line: 2, column: 2 },
	      generated: { line: 4, column: 4 },
	      source: 'baz.js',
	      name: 'dirtMcGirt'
	    });
	    smg.setSourceContent('baz.js', 'baz.js content');
	
	    var smc = SourceMapConsumer.fromSourceMap(smg);
	    assert.equal(smc.file, 'foo.js');
	    assert.equal(smc.sourceRoot, 'http://example.com/');
	    assert.equal(smc.sources.length, 2);
	    assert.equal(smc.sources[0], 'http://example.com/bar.js');
	    assert.equal(smc.sources[1], 'http://example.com/baz.js');
	    assert.equal(smc.sourceContentFor('baz.js'), 'baz.js content');
	
	    var pos = smc.originalPositionFor({
	      line: 2,
	      column: 2
	    });
	    assert.equal(pos.line, 1);
	    assert.equal(pos.column, 1);
	    assert.equal(pos.source, 'http://example.com/bar.js');
	    assert.equal(pos.name, null);
	
	    pos = smc.generatedPositionFor({
	      line: 1,
	      column: 1,
	      source: 'http://example.com/bar.js'
	    });
	    assert.equal(pos.line, 2);
	    assert.equal(pos.column, 2);
	
	    pos = smc.originalPositionFor({
	      line: 4,
	      column: 4
	    });
	    assert.equal(pos.line, 2);
	    assert.equal(pos.column, 2);
	    assert.equal(pos.source, 'http://example.com/baz.js');
	    assert.equal(pos.name, 'dirtMcGirt');
	
	    pos = smc.generatedPositionFor({
	      line: 2,
	      column: 2,
	      source: 'http://example.com/baz.js'
	    });
	    assert.equal(pos.line, 4);
	    assert.equal(pos.column, 4);
	  };
	
	  exports['test issue #191'] = function (assert) {
	    var generator = new SourceMapGenerator({ file: 'a.css' });
	    generator.addMapping({
	      source:   'b.css',
	      original: {
	        line:   1,
	        column: 0
	      },
	      generated: {
	        line:   1,
	        column: 0
	      }
	    });
	
	    // Create a SourceMapConsumer from the SourceMapGenerator, ...
	    var consumer  = SourceMapConsumer.fromSourceMap(generator);
	    // ... and then try and use the SourceMapGenerator again. This should not
	    // throw.
	    generator.toJSON();
	
	    assert.ok(true, "Using a SourceMapGenerator again after creating a " +
	                    "SourceMapConsumer from it should not throw");
	  };
	
	  exports['test sources where their prefix is the source root: issue #199'] = function (assert) {
	    var testSourceMap = {
	      "version": 3,
	      "sources": ["/source/app/app/app.js"],
	      "names": ["System"],
	      "mappings": "AAAAA",
	      "file": "app/app.js",
	      "sourcesContent": ["'use strict';"],
	      "sourceRoot":"/source/"
	    };
	
	    var consumer = new SourceMapConsumer(testSourceMap);
	
	    function consumerHasSource(s) {
	      assert.ok(consumer.sourceContentFor(s));
	    }
	
	    consumer.sources.forEach(consumerHasSource);
	    testSourceMap.sources.forEach(consumerHasSource);
	  };
	
	  exports['test sources where their prefix is the source root and the source root is a url: issue #199'] = function (assert) {
	    var testSourceMap = {
	      "version": 3,
	      "sources": ["http://example.com/source/app/app/app.js"],
	      "names": ["System"],
	      "mappings": "AAAAA",
	      "sourcesContent": ["'use strict';"],
	      "sourceRoot":"http://example.com/source/"
	    };
	
	    var consumer = new SourceMapConsumer(testSourceMap);
	
	    function consumerHasSource(s) {
	      assert.ok(consumer.sourceContentFor(s));
	    }
	
	    consumer.sources.forEach(consumerHasSource);
	    testSourceMap.sources.forEach(consumerHasSource);
	  };
	}


/***/ },
/* 1 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	{
	  var util = __webpack_require__(2);
	
	  // This is a test mapping which maps functions from two different files
	  // (one.js and two.js) to a minified generated source.
	  //
	  // Here is one.js:
	  //
	  //   ONE.foo = function (bar) {
	  //     return baz(bar);
	  //   };
	  //
	  // Here is two.js:
	  //
	  //   TWO.inc = function (n) {
	  //     return n + 1;
	  //   };
	  //
	  // And here is the generated code (min.js):
	  //
	  //   ONE.foo=function(a){return baz(a);};
	  //   TWO.inc=function(a){return a+1;};
	  exports.testGeneratedCode = " ONE.foo=function(a){return baz(a);};\n"+
	                              " TWO.inc=function(a){return a+1;};";
	  exports.testMap = {
	    version: 3,
	    file: 'min.js',
	    names: ['bar', 'baz', 'n'],
	    sources: ['one.js', 'two.js'],
	    sourceRoot: '/the/root',
	    mappings: 'CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID;CCDb,IAAI,IAAM,SAAUE,GAClB,OAAOA'
	  };
	  exports.testMapNoSourceRoot = {
	    version: 3,
	    file: 'min.js',
	    names: ['bar', 'baz', 'n'],
	    sources: ['one.js', 'two.js'],
	    mappings: 'CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID;CCDb,IAAI,IAAM,SAAUE,GAClB,OAAOA'
	  };
	  exports.testMapEmptySourceRoot = {
	    version: 3,
	    file: 'min.js',
	    names: ['bar', 'baz', 'n'],
	    sources: ['one.js', 'two.js'],
	    sourceRoot: '',
	    mappings: 'CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID;CCDb,IAAI,IAAM,SAAUE,GAClB,OAAOA'
	  };
	  // This mapping is identical to above, but uses the indexed format instead.
	  exports.indexedTestMap = {
	    version: 3,
	    file: 'min.js',
	    sections: [
	      {
	        offset: {
	          line: 0,
	          column: 0
	        },
	        map: {
	          version: 3,
	          sources: [
	            "one.js"
	          ],
	          sourcesContent: [
	            ' ONE.foo = function (bar) {\n' +
	            '   return baz(bar);\n' +
	            ' };',
	          ],
	          names: [
	            "bar",
	            "baz"
	          ],
	          mappings: "CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID",
	          file: "min.js",
	          sourceRoot: "/the/root"
	        }
	      },
	      {
	        offset: {
	          line: 1,
	          column: 0
	        },
	        map: {
	          version: 3,
	          sources: [
	            "two.js"
	          ],
	          sourcesContent: [
	            ' TWO.inc = function (n) {\n' +
	            '   return n + 1;\n' +
	            ' };'
	          ],
	          names: [
	            "n"
	          ],
	          mappings: "CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOA",
	          file: "min.js",
	          sourceRoot: "/the/root"
	        }
	      }
	    ]
	  };
	  exports.indexedTestMapDifferentSourceRoots = {
	    version: 3,
	    file: 'min.js',
	    sections: [
	      {
	        offset: {
	          line: 0,
	          column: 0
	        },
	        map: {
	          version: 3,
	          sources: [
	            "one.js"
	          ],
	          sourcesContent: [
	            ' ONE.foo = function (bar) {\n' +
	            '   return baz(bar);\n' +
	            ' };',
	          ],
	          names: [
	            "bar",
	            "baz"
	          ],
	          mappings: "CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID",
	          file: "min.js",
	          sourceRoot: "/the/root"
	        }
	      },
	      {
	        offset: {
	          line: 1,
	          column: 0
	        },
	        map: {
	          version: 3,
	          sources: [
	            "two.js"
	          ],
	          sourcesContent: [
	            ' TWO.inc = function (n) {\n' +
	            '   return n + 1;\n' +
	            ' };'
	          ],
	          names: [
	            "n"
	          ],
	          mappings: "CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOA",
	          file: "min.js",
	          sourceRoot: "/different/root"
	        }
	      }
	    ]
	  };
	  exports.testMapWithSourcesContent = {
	    version: 3,
	    file: 'min.js',
	    names: ['bar', 'baz', 'n'],
	    sources: ['one.js', 'two.js'],
	    sourcesContent: [
	      ' ONE.foo = function (bar) {\n' +
	      '   return baz(bar);\n' +
	      ' };',
	      ' TWO.inc = function (n) {\n' +
	      '   return n + 1;\n' +
	      ' };'
	    ],
	    sourceRoot: '/the/root',
	    mappings: 'CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID;CCDb,IAAI,IAAM,SAAUE,GAClB,OAAOA'
	  };
	  exports.testMapRelativeSources = {
	    version: 3,
	    file: 'min.js',
	    names: ['bar', 'baz', 'n'],
	    sources: ['./one.js', './two.js'],
	    sourcesContent: [
	      ' ONE.foo = function (bar) {\n' +
	      '   return baz(bar);\n' +
	      ' };',
	      ' TWO.inc = function (n) {\n' +
	      '   return n + 1;\n' +
	      ' };'
	    ],
	    sourceRoot: '/the/root',
	    mappings: 'CAAC,IAAI,IAAM,SAAUA,GAClB,OAAOC,IAAID;CCDb,IAAI,IAAM,SAAUE,GAClB,OAAOA'
	  };
	  exports.emptyMap = {
	    version: 3,
	    file: 'min.js',
	    names: [],
	    sources: [],
	    mappings: ''
	  };
	
	
	  function assertMapping(generatedLine, generatedColumn, originalSource,
	                         originalLine, originalColumn, name, bias, map, assert,
	                         dontTestGenerated, dontTestOriginal) {
	    if (!dontTestOriginal) {
	      var origMapping = map.originalPositionFor({
	        line: generatedLine,
	        column: generatedColumn,
	        bias: bias
	      });
	      assert.equal(origMapping.name, name,
	                   'Incorrect name, expected ' + JSON.stringify(name)
	                   + ', got ' + JSON.stringify(origMapping.name));
	      assert.equal(origMapping.line, originalLine,
	                   'Incorrect line, expected ' + JSON.stringify(originalLine)
	                   + ', got ' + JSON.stringify(origMapping.line));
	      assert.equal(origMapping.column, originalColumn,
	                   'Incorrect column, expected ' + JSON.stringify(originalColumn)
	                   + ', got ' + JSON.stringify(origMapping.column));
	
	      var expectedSource;
	
	      if (originalSource && map.sourceRoot && originalSource.indexOf(map.sourceRoot) === 0) {
	        expectedSource = originalSource;
	      } else if (originalSource) {
	        expectedSource = map.sourceRoot
	          ? util.join(map.sourceRoot, originalSource)
	          : originalSource;
	      } else {
	        expectedSource = null;
	      }
	
	      assert.equal(origMapping.source, expectedSource,
	                   'Incorrect source, expected ' + JSON.stringify(expectedSource)
	                   + ', got ' + JSON.stringify(origMapping.source));
	    }
	
	    if (!dontTestGenerated) {
	      var genMapping = map.generatedPositionFor({
	        source: originalSource,
	        line: originalLine,
	        column: originalColumn,
	        bias: bias
	      });
	      assert.equal(genMapping.line, generatedLine,
	                   'Incorrect line, expected ' + JSON.stringify(generatedLine)
	                   + ', got ' + JSON.stringify(genMapping.line));
	      assert.equal(genMapping.column, generatedColumn,
	                   'Incorrect column, expected ' + JSON.stringify(generatedColumn)
	                   + ', got ' + JSON.stringify(genMapping.column));
	    }
	  }
	  exports.assertMapping = assertMapping;
	
	  function assertEqualMaps(assert, actualMap, expectedMap) {
	    assert.equal(actualMap.version, expectedMap.version, "version mismatch");
	    assert.equal(actualMap.file, expectedMap.file, "file mismatch");
	    assert.equal(actualMap.names.length,
	                 expectedMap.names.length,
	                 "names length mismatch: " +
	                   actualMap.names.join(", ") + " != " + expectedMap.names.join(", "));
	    for (var i = 0; i < actualMap.names.length; i++) {
	      assert.equal(actualMap.names[i],
	                   expectedMap.names[i],
	                   "names[" + i + "] mismatch: " +
	                     actualMap.names.join(", ") + " != " + expectedMap.names.join(", "));
	    }
	    assert.equal(actualMap.sources.length,
	                 expectedMap.sources.length,
	                 "sources length mismatch: " +
	                   actualMap.sources.join(", ") + " != " + expectedMap.sources.join(", "));
	    for (var i = 0; i < actualMap.sources.length; i++) {
	      assert.equal(actualMap.sources[i],
	                   expectedMap.sources[i],
	                   "sources[" + i + "] length mismatch: " +
	                   actualMap.sources.join(", ") + " != " + expectedMap.sources.join(", "));
	    }
	    assert.equal(actualMap.sourceRoot,
	                 expectedMap.sourceRoot,
	                 "sourceRoot mismatch: " +
	                   actualMap.sourceRoot + " != " + expectedMap.sourceRoot);
	    assert.equal(actualMap.mappings, expectedMap.mappings,
	                 "mappings mismatch:\nActual:   " + actualMap.mappings + "\nExpected: " + expectedMap.mappings);
	    if (actualMap.sourcesContent) {
	      assert.equal(actualMap.sourcesContent.length,
	                   expectedMap.sourcesContent.length,
	                   "sourcesContent length mismatch");
	      for (var i = 0; i < actualMap.sourcesContent.length; i++) {
	        assert.equal(actualMap.sourcesContent[i],
	                     expectedMap.sourcesContent[i],
	                     "sourcesContent[" + i + "] mismatch");
	      }
	    }
	  }
	  exports.assertEqualMaps = assertEqualMaps;
	}


/***/ },
/* 2 */
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


/***/ },
/* 3 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	{
	  var util = __webpack_require__(2);
	  var binarySearch = __webpack_require__(4);
	  var ArraySet = __webpack_require__(5).ArraySet;
	  var base64VLQ = __webpack_require__(6);
	  var quickSort = __webpack_require__(8).quickSort;
	
	  function SourceMapConsumer(aSourceMap) {
	    var sourceMap = aSourceMap;
	    if (typeof aSourceMap === 'string') {
	      sourceMap = JSON.parse(aSourceMap.replace(/^\)\]\}'/, ''));
	    }
	
	    return sourceMap.sections != null
	      ? new IndexedSourceMapConsumer(sourceMap)
	      : new BasicSourceMapConsumer(sourceMap);
	  }
	
	  SourceMapConsumer.fromSourceMap = function(aSourceMap) {
	    return BasicSourceMapConsumer.fromSourceMap(aSourceMap);
	  }
	
	  /**
	   * The version of the source mapping spec that we are consuming.
	   */
	  SourceMapConsumer.prototype._version = 3;
	
	  // `__generatedMappings` and `__originalMappings` are arrays that hold the
	  // parsed mapping coordinates from the source map's "mappings" attribute. They
	  // are lazily instantiated, accessed via the `_generatedMappings` and
	  // `_originalMappings` getters respectively, and we only parse the mappings
	  // and create these arrays once queried for a source location. We jump through
	  // these hoops because there can be many thousands of mappings, and parsing
	  // them is expensive, so we only want to do it if we must.
	  //
	  // Each object in the arrays is of the form:
	  //
	  //     {
	  //       generatedLine: The line number in the generated code,
	  //       generatedColumn: The column number in the generated code,
	  //       source: The path to the original source file that generated this
	  //               chunk of code,
	  //       originalLine: The line number in the original source that
	  //                     corresponds to this chunk of generated code,
	  //       originalColumn: The column number in the original source that
	  //                       corresponds to this chunk of generated code,
	  //       name: The name of the original symbol which generated this chunk of
	  //             code.
	  //     }
	  //
	  // All properties except for `generatedLine` and `generatedColumn` can be
	  // `null`.
	  //
	  // `_generatedMappings` is ordered by the generated positions.
	  //
	  // `_originalMappings` is ordered by the original positions.
	
	  SourceMapConsumer.prototype.__generatedMappings = null;
	  Object.defineProperty(SourceMapConsumer.prototype, '_generatedMappings', {
	    get: function () {
	      if (!this.__generatedMappings) {
	        this._parseMappings(this._mappings, this.sourceRoot);
	      }
	
	      return this.__generatedMappings;
	    }
	  });
	
	  SourceMapConsumer.prototype.__originalMappings = null;
	  Object.defineProperty(SourceMapConsumer.prototype, '_originalMappings', {
	    get: function () {
	      if (!this.__originalMappings) {
	        this._parseMappings(this._mappings, this.sourceRoot);
	      }
	
	      return this.__originalMappings;
	    }
	  });
	
	  SourceMapConsumer.prototype._charIsMappingSeparator =
	    function SourceMapConsumer_charIsMappingSeparator(aStr, index) {
	      var c = aStr.charAt(index);
	      return c === ";" || c === ",";
	    };
	
	  /**
	   * Parse the mappings in a string in to a data structure which we can easily
	   * query (the ordered arrays in the `this.__generatedMappings` and
	   * `this.__originalMappings` properties).
	   */
	  SourceMapConsumer.prototype._parseMappings =
	    function SourceMapConsumer_parseMappings(aStr, aSourceRoot) {
	      throw new Error("Subclasses must implement _parseMappings");
	    };
	
	  SourceMapConsumer.GENERATED_ORDER = 1;
	  SourceMapConsumer.ORIGINAL_ORDER = 2;
	
	  SourceMapConsumer.GREATEST_LOWER_BOUND = 1;
	  SourceMapConsumer.LEAST_UPPER_BOUND = 2;
	
	  /**
	   * Iterate over each mapping between an original source/line/column and a
	   * generated line/column in this source map.
	   *
	   * @param Function aCallback
	   *        The function that is called with each mapping.
	   * @param Object aContext
	   *        Optional. If specified, this object will be the value of `this` every
	   *        time that `aCallback` is called.
	   * @param aOrder
	   *        Either `SourceMapConsumer.GENERATED_ORDER` or
	   *        `SourceMapConsumer.ORIGINAL_ORDER`. Specifies whether you want to
	   *        iterate over the mappings sorted by the generated file's line/column
	   *        order or the original's source/line/column order, respectively. Defaults to
	   *        `SourceMapConsumer.GENERATED_ORDER`.
	   */
	  SourceMapConsumer.prototype.eachMapping =
	    function SourceMapConsumer_eachMapping(aCallback, aContext, aOrder) {
	      var context = aContext || null;
	      var order = aOrder || SourceMapConsumer.GENERATED_ORDER;
	
	      var mappings;
	      switch (order) {
	      case SourceMapConsumer.GENERATED_ORDER:
	        mappings = this._generatedMappings;
	        break;
	      case SourceMapConsumer.ORIGINAL_ORDER:
	        mappings = this._originalMappings;
	        break;
	      default:
	        throw new Error("Unknown order of iteration.");
	      }
	
	      var sourceRoot = this.sourceRoot;
	      mappings.map(function (mapping) {
	        var source = mapping.source === null ? null : this._sources.at(mapping.source);
	        if (source != null && sourceRoot != null) {
	          source = util.join(sourceRoot, source);
	        }
	        return {
	          source: source,
	          generatedLine: mapping.generatedLine,
	          generatedColumn: mapping.generatedColumn,
	          originalLine: mapping.originalLine,
	          originalColumn: mapping.originalColumn,
	          name: mapping.name === null ? null : this._names.at(mapping.name)
	        };
	      }, this).forEach(aCallback, context);
	    };
	
	  /**
	   * Returns all generated line and column information for the original source,
	   * line, and column provided. If no column is provided, returns all mappings
	   * corresponding to a either the line we are searching for or the next
	   * closest line that has any mappings. Otherwise, returns all mappings
	   * corresponding to the given line and either the column we are searching for
	   * or the next closest column that has any offsets.
	   *
	   * The only argument is an object with the following properties:
	   *
	   *   - source: The filename of the original source.
	   *   - line: The line number in the original source.
	   *   - column: Optional. the column number in the original source.
	   *
	   * and an array of objects is returned, each with the following properties:
	   *
	   *   - line: The line number in the generated source, or null.
	   *   - column: The column number in the generated source, or null.
	   */
	  SourceMapConsumer.prototype.allGeneratedPositionsFor =
	    function SourceMapConsumer_allGeneratedPositionsFor(aArgs) {
	      var line = util.getArg(aArgs, 'line');
	
	      // When there is no exact match, BasicSourceMapConsumer.prototype._findMapping
	      // returns the index of the closest mapping less than the needle. By
	      // setting needle.originalColumn to 0, we thus find the last mapping for
	      // the given line, provided such a mapping exists.
	      var needle = {
	        source: util.getArg(aArgs, 'source'),
	        originalLine: line,
	        originalColumn: util.getArg(aArgs, 'column', 0)
	      };
	
	      if (this.sourceRoot != null) {
	        needle.source = util.relative(this.sourceRoot, needle.source);
	      }
	      if (!this._sources.has(needle.source)) {
	        return [];
	      }
	      needle.source = this._sources.indexOf(needle.source);
	
	      var mappings = [];
	
	      var index = this._findMapping(needle,
	                                    this._originalMappings,
	                                    "originalLine",
	                                    "originalColumn",
	                                    util.compareByOriginalPositions,
	                                    binarySearch.LEAST_UPPER_BOUND);
	      if (index >= 0) {
	        var mapping = this._originalMappings[index];
	
	        if (aArgs.column === undefined) {
	          var originalLine = mapping.originalLine;
	
	          // Iterate until either we run out of mappings, or we run into
	          // a mapping for a different line than the one we found. Since
	          // mappings are sorted, this is guaranteed to find all mappings for
	          // the line we found.
	          while (mapping && mapping.originalLine === originalLine) {
	            mappings.push({
	              line: util.getArg(mapping, 'generatedLine', null),
	              column: util.getArg(mapping, 'generatedColumn', null),
	              lastColumn: util.getArg(mapping, 'lastGeneratedColumn', null)
	            });
	
	            mapping = this._originalMappings[++index];
	          }
	        } else {
	          var originalColumn = mapping.originalColumn;
	
	          // Iterate until either we run out of mappings, or we run into
	          // a mapping for a different line than the one we were searching for.
	          // Since mappings are sorted, this is guaranteed to find all mappings for
	          // the line we are searching for.
	          while (mapping &&
	                 mapping.originalLine === line &&
	                 mapping.originalColumn == originalColumn) {
	            mappings.push({
	              line: util.getArg(mapping, 'generatedLine', null),
	              column: util.getArg(mapping, 'generatedColumn', null),
	              lastColumn: util.getArg(mapping, 'lastGeneratedColumn', null)
	            });
	
	            mapping = this._originalMappings[++index];
	          }
	        }
	      }
	
	      return mappings;
	    };
	
	  exports.SourceMapConsumer = SourceMapConsumer;
	
	  /**
	   * A BasicSourceMapConsumer instance represents a parsed source map which we can
	   * query for information about the original file positions by giving it a file
	   * position in the generated source.
	   *
	   * The only parameter is the raw source map (either as a JSON string, or
	   * already parsed to an object). According to the spec, source maps have the
	   * following attributes:
	   *
	   *   - version: Which version of the source map spec this map is following.
	   *   - sources: An array of URLs to the original source files.
	   *   - names: An array of identifiers which can be referrenced by individual mappings.
	   *   - sourceRoot: Optional. The URL root from which all sources are relative.
	   *   - sourcesContent: Optional. An array of contents of the original source files.
	   *   - mappings: A string of base64 VLQs which contain the actual mappings.
	   *   - file: Optional. The generated file this source map is associated with.
	   *
	   * Here is an example source map, taken from the source map spec[0]:
	   *
	   *     {
	   *       version : 3,
	   *       file: "out.js",
	   *       sourceRoot : "",
	   *       sources: ["foo.js", "bar.js"],
	   *       names: ["src", "maps", "are", "fun"],
	   *       mappings: "AA,AB;;ABCDE;"
	   *     }
	   *
	   * [0]: https://docs.google.com/document/d/1U1RGAehQwRypUTovF1KRlpiOFze0b-_2gc6fAH0KY0k/edit?pli=1#
	   */
	  function BasicSourceMapConsumer(aSourceMap) {
	    var sourceMap = aSourceMap;
	    if (typeof aSourceMap === 'string') {
	      sourceMap = JSON.parse(aSourceMap.replace(/^\)\]\}'/, ''));
	    }
	
	    var version = util.getArg(sourceMap, 'version');
	    var sources = util.getArg(sourceMap, 'sources');
	    // Sass 3.3 leaves out the 'names' array, so we deviate from the spec (which
	    // requires the array) to play nice here.
	    var names = util.getArg(sourceMap, 'names', []);
	    var sourceRoot = util.getArg(sourceMap, 'sourceRoot', null);
	    var sourcesContent = util.getArg(sourceMap, 'sourcesContent', null);
	    var mappings = util.getArg(sourceMap, 'mappings');
	    var file = util.getArg(sourceMap, 'file', null);
	
	    // Once again, Sass deviates from the spec and supplies the version as a
	    // string rather than a number, so we use loose equality checking here.
	    if (version != this._version) {
	      throw new Error('Unsupported version: ' + version);
	    }
	
	    sources = sources
	      // Some source maps produce relative source paths like "./foo.js" instead of
	      // "foo.js".  Normalize these first so that future comparisons will succeed.
	      // See bugzil.la/1090768.
	      .map(util.normalize)
	      // Always ensure that absolute sources are internally stored relative to
	      // the source root, if the source root is absolute. Not doing this would
	      // be particularly problematic when the source root is a prefix of the
	      // source (valid, but why??). See github issue #199 and bugzil.la/1188982.
	      .map(function (source) {
	        return sourceRoot && util.isAbsolute(sourceRoot) && util.isAbsolute(source)
	          ? util.relative(sourceRoot, source)
	          : source;
	      });
	
	    // Pass `true` below to allow duplicate names and sources. While source maps
	    // are intended to be compressed and deduplicated, the TypeScript compiler
	    // sometimes generates source maps with duplicates in them. See Github issue
	    // #72 and bugzil.la/889492.
	    this._names = ArraySet.fromArray(names, true);
	    this._sources = ArraySet.fromArray(sources, true);
	
	    this.sourceRoot = sourceRoot;
	    this.sourcesContent = sourcesContent;
	    this._mappings = mappings;
	    this.file = file;
	  }
	
	  BasicSourceMapConsumer.prototype = Object.create(SourceMapConsumer.prototype);
	  BasicSourceMapConsumer.prototype.consumer = SourceMapConsumer;
	
	  /**
	   * Create a BasicSourceMapConsumer from a SourceMapGenerator.
	   *
	   * @param SourceMapGenerator aSourceMap
	   *        The source map that will be consumed.
	   * @returns BasicSourceMapConsumer
	   */
	  BasicSourceMapConsumer.fromSourceMap =
	    function SourceMapConsumer_fromSourceMap(aSourceMap) {
	      var smc = Object.create(BasicSourceMapConsumer.prototype);
	
	      var names = smc._names = ArraySet.fromArray(aSourceMap._names.toArray(), true);
	      var sources = smc._sources = ArraySet.fromArray(aSourceMap._sources.toArray(), true);
	      smc.sourceRoot = aSourceMap._sourceRoot;
	      smc.sourcesContent = aSourceMap._generateSourcesContent(smc._sources.toArray(),
	                                                              smc.sourceRoot);
	      smc.file = aSourceMap._file;
	
	      // Because we are modifying the entries (by converting string sources and
	      // names to indices into the sources and names ArraySets), we have to make
	      // a copy of the entry or else bad things happen. Shared mutable state
	      // strikes again! See github issue #191.
	
	      var generatedMappings = aSourceMap._mappings.toArray().slice();
	      var destGeneratedMappings = smc.__generatedMappings = [];
	      var destOriginalMappings = smc.__originalMappings = [];
	
	      for (var i = 0, length = generatedMappings.length; i < length; i++) {
	        var srcMapping = generatedMappings[i];
	        var destMapping = new Mapping;
	        destMapping.generatedLine = srcMapping.generatedLine;
	        destMapping.generatedColumn = srcMapping.generatedColumn;
	
	        if (srcMapping.source) {
	          destMapping.source = sources.indexOf(srcMapping.source);
	          destMapping.originalLine = srcMapping.originalLine;
	          destMapping.originalColumn = srcMapping.originalColumn;
	
	          if (srcMapping.name) {
	            destMapping.name = names.indexOf(srcMapping.name);
	          }
	
	          destOriginalMappings.push(destMapping);
	        }
	
	        destGeneratedMappings.push(destMapping);
	      }
	
	      quickSort(smc.__originalMappings, util.compareByOriginalPositions);
	
	      return smc;
	    };
	
	  /**
	   * The version of the source mapping spec that we are consuming.
	   */
	  BasicSourceMapConsumer.prototype._version = 3;
	
	  /**
	   * The list of original sources.
	   */
	  Object.defineProperty(BasicSourceMapConsumer.prototype, 'sources', {
	    get: function () {
	      return this._sources.toArray().map(function (s) {
	        return this.sourceRoot != null ? util.join(this.sourceRoot, s) : s;
	      }, this);
	    }
	  });
	
	  /**
	   * Provide the JIT with a nice shape / hidden class.
	   */
	  function Mapping() {
	    this.generatedLine = 0;
	    this.generatedColumn = 0;
	    this.source = null;
	    this.originalLine = null;
	    this.originalColumn = null;
	    this.name = null;
	  }
	
	  /**
	   * Parse the mappings in a string in to a data structure which we can easily
	   * query (the ordered arrays in the `this.__generatedMappings` and
	   * `this.__originalMappings` properties).
	   */
	  BasicSourceMapConsumer.prototype._parseMappings =
	    function SourceMapConsumer_parseMappings(aStr, aSourceRoot) {
	      var generatedLine = 1;
	      var previousGeneratedColumn = 0;
	      var previousOriginalLine = 0;
	      var previousOriginalColumn = 0;
	      var previousSource = 0;
	      var previousName = 0;
	      var length = aStr.length;
	      var index = 0;
	      var cachedSegments = {};
	      var temp = {};
	      var originalMappings = [];
	      var generatedMappings = [];
	      var mapping, str, segment, end, value;
	
	      while (index < length) {
	        if (aStr.charAt(index) === ';') {
	          generatedLine++;
	          index++;
	          previousGeneratedColumn = 0;
	        }
	        else if (aStr.charAt(index) === ',') {
	          index++;
	        }
	        else {
	          mapping = new Mapping();
	          mapping.generatedLine = generatedLine;
	
	          // Because each offset is encoded relative to the previous one,
	          // many segments often have the same encoding. We can exploit this
	          // fact by caching the parsed variable length fields of each segment,
	          // allowing us to avoid a second parse if we encounter the same
	          // segment again.
	          for (end = index; end < length; end++) {
	            if (this._charIsMappingSeparator(aStr, end)) {
	              break;
	            }
	          }
	          str = aStr.slice(index, end);
	
	          segment = cachedSegments[str];
	          if (segment) {
	            index += str.length;
	          } else {
	            segment = [];
	            while (index < end) {
	              base64VLQ.decode(aStr, index, temp);
	              value = temp.value;
	              index = temp.rest;
	              segment.push(value);
	            }
	
	            if (segment.length === 2) {
	              throw new Error('Found a source, but no line and column');
	            }
	
	            if (segment.length === 3) {
	              throw new Error('Found a source and line, but no column');
	            }
	
	            cachedSegments[str] = segment;
	          }
	
	          // Generated column.
	          mapping.generatedColumn = previousGeneratedColumn + segment[0];
	          previousGeneratedColumn = mapping.generatedColumn;
	
	          if (segment.length > 1) {
	            // Original source.
	            mapping.source = previousSource + segment[1];
	            previousSource += segment[1];
	
	            // Original line.
	            mapping.originalLine = previousOriginalLine + segment[2];
	            previousOriginalLine = mapping.originalLine;
	            // Lines are stored 0-based
	            mapping.originalLine += 1;
	
	            // Original column.
	            mapping.originalColumn = previousOriginalColumn + segment[3];
	            previousOriginalColumn = mapping.originalColumn;
	
	            if (segment.length > 4) {
	              // Original name.
	              mapping.name = previousName + segment[4];
	              previousName += segment[4];
	            }
	          }
	
	          generatedMappings.push(mapping);
	          if (typeof mapping.originalLine === 'number') {
	            originalMappings.push(mapping);
	          }
	        }
	      }
	
	      quickSort(generatedMappings, util.compareByGeneratedPositionsDeflated);
	      this.__generatedMappings = generatedMappings;
	
	      quickSort(originalMappings, util.compareByOriginalPositions);
	      this.__originalMappings = originalMappings;
	    };
	
	  /**
	   * Find the mapping that best matches the hypothetical "needle" mapping that
	   * we are searching for in the given "haystack" of mappings.
	   */
	  BasicSourceMapConsumer.prototype._findMapping =
	    function SourceMapConsumer_findMapping(aNeedle, aMappings, aLineName,
	                                           aColumnName, aComparator, aBias) {
	      // To return the position we are searching for, we must first find the
	      // mapping for the given position and then return the opposite position it
	      // points to. Because the mappings are sorted, we can use binary search to
	      // find the best mapping.
	
	      if (aNeedle[aLineName] <= 0) {
	        throw new TypeError('Line must be greater than or equal to 1, got '
	                            + aNeedle[aLineName]);
	      }
	      if (aNeedle[aColumnName] < 0) {
	        throw new TypeError('Column must be greater than or equal to 0, got '
	                            + aNeedle[aColumnName]);
	      }
	
	      return binarySearch.search(aNeedle, aMappings, aComparator, aBias);
	    };
	
	  /**
	   * Compute the last column for each generated mapping. The last column is
	   * inclusive.
	   */
	  BasicSourceMapConsumer.prototype.computeColumnSpans =
	    function SourceMapConsumer_computeColumnSpans() {
	      for (var index = 0; index < this._generatedMappings.length; ++index) {
	        var mapping = this._generatedMappings[index];
	
	        // Mappings do not contain a field for the last generated columnt. We
	        // can come up with an optimistic estimate, however, by assuming that
	        // mappings are contiguous (i.e. given two consecutive mappings, the
	        // first mapping ends where the second one starts).
	        if (index + 1 < this._generatedMappings.length) {
	          var nextMapping = this._generatedMappings[index + 1];
	
	          if (mapping.generatedLine === nextMapping.generatedLine) {
	            mapping.lastGeneratedColumn = nextMapping.generatedColumn - 1;
	            continue;
	          }
	        }
	
	        // The last mapping for each line spans the entire line.
	        mapping.lastGeneratedColumn = Infinity;
	      }
	    };
	
	  /**
	   * Returns the original source, line, and column information for the generated
	   * source's line and column positions provided. The only argument is an object
	   * with the following properties:
	   *
	   *   - line: The line number in the generated source.
	   *   - column: The column number in the generated source.
	   *   - bias: Either 'SourceMapConsumer.GREATEST_LOWER_BOUND' or
	   *     'SourceMapConsumer.LEAST_UPPER_BOUND'. Specifies whether to return the
	   *     closest element that is smaller than or greater than the one we are
	   *     searching for, respectively, if the exact element cannot be found.
	   *     Defaults to 'SourceMapConsumer.GREATEST_LOWER_BOUND'.
	   *
	   * and an object is returned with the following properties:
	   *
	   *   - source: The original source file, or null.
	   *   - line: The line number in the original source, or null.
	   *   - column: The column number in the original source, or null.
	   *   - name: The original identifier, or null.
	   */
	  BasicSourceMapConsumer.prototype.originalPositionFor =
	    function SourceMapConsumer_originalPositionFor(aArgs) {
	      var needle = {
	        generatedLine: util.getArg(aArgs, 'line'),
	        generatedColumn: util.getArg(aArgs, 'column')
	      };
	
	      var index = this._findMapping(
	        needle,
	        this._generatedMappings,
	        "generatedLine",
	        "generatedColumn",
	        util.compareByGeneratedPositionsDeflated,
	        util.getArg(aArgs, 'bias', SourceMapConsumer.GREATEST_LOWER_BOUND)
	      );
	
	      if (index >= 0) {
	        var mapping = this._generatedMappings[index];
	
	        if (mapping.generatedLine === needle.generatedLine) {
	          var source = util.getArg(mapping, 'source', null);
	          if (source !== null) {
	            source = this._sources.at(source);
	            if (this.sourceRoot != null) {
	              source = util.join(this.sourceRoot, source);
	            }
	          }
	          var name = util.getArg(mapping, 'name', null);
	          if (name !== null) {
	            name = this._names.at(name);
	          }
	          return {
	            source: source,
	            line: util.getArg(mapping, 'originalLine', null),
	            column: util.getArg(mapping, 'originalColumn', null),
	            name: name
	          };
	        }
	      }
	
	      return {
	        source: null,
	        line: null,
	        column: null,
	        name: null
	      };
	    };
	
	  /**
	   * Return true if we have the source content for every source in the source
	   * map, false otherwise.
	   */
	  BasicSourceMapConsumer.prototype.hasContentsOfAllSources =
	    function BasicSourceMapConsumer_hasContentsOfAllSources() {
	      if (!this.sourcesContent) {
	        return false;
	      }
	      return this.sourcesContent.length >= this._sources.size() &&
	        !this.sourcesContent.some(function (sc) { return sc == null; });
	    };
	
	  /**
	   * Returns the original source content. The only argument is the url of the
	   * original source file. Returns null if no original source content is
	   * availible.
	   */
	  BasicSourceMapConsumer.prototype.sourceContentFor =
	    function SourceMapConsumer_sourceContentFor(aSource, nullOnMissing) {
	      if (!this.sourcesContent) {
	        return null;
	      }
	
	      if (this.sourceRoot != null) {
	        aSource = util.relative(this.sourceRoot, aSource);
	      }
	
	      if (this._sources.has(aSource)) {
	        return this.sourcesContent[this._sources.indexOf(aSource)];
	      }
	
	      var url;
	      if (this.sourceRoot != null
	          && (url = util.urlParse(this.sourceRoot))) {
	        // XXX: file:// URIs and absolute paths lead to unexpected behavior for
	        // many users. We can help them out when they expect file:// URIs to
	        // behave like it would if they were running a local HTTP server. See
	        // https://bugzilla.mozilla.org/show_bug.cgi?id=885597.
	        var fileUriAbsPath = aSource.replace(/^file:\/\//, "");
	        if (url.scheme == "file"
	            && this._sources.has(fileUriAbsPath)) {
	          return this.sourcesContent[this._sources.indexOf(fileUriAbsPath)]
	        }
	
	        if ((!url.path || url.path == "/")
	            && this._sources.has("/" + aSource)) {
	          return this.sourcesContent[this._sources.indexOf("/" + aSource)];
	        }
	      }
	
	      // This function is used recursively from
	      // IndexedSourceMapConsumer.prototype.sourceContentFor. In that case, we
	      // don't want to throw if we can't find the source - we just want to
	      // return null, so we provide a flag to exit gracefully.
	      if (nullOnMissing) {
	        return null;
	      }
	      else {
	        throw new Error('"' + aSource + '" is not in the SourceMap.');
	      }
	    };
	
	  /**
	   * Returns the generated line and column information for the original source,
	   * line, and column positions provided. The only argument is an object with
	   * the following properties:
	   *
	   *   - source: The filename of the original source.
	   *   - line: The line number in the original source.
	   *   - column: The column number in the original source.
	   *   - bias: Either 'SourceMapConsumer.GREATEST_LOWER_BOUND' or
	   *     'SourceMapConsumer.LEAST_UPPER_BOUND'. Specifies whether to return the
	   *     closest element that is smaller than or greater than the one we are
	   *     searching for, respectively, if the exact element cannot be found.
	   *     Defaults to 'SourceMapConsumer.GREATEST_LOWER_BOUND'.
	   *
	   * and an object is returned with the following properties:
	   *
	   *   - line: The line number in the generated source, or null.
	   *   - column: The column number in the generated source, or null.
	   */
	  BasicSourceMapConsumer.prototype.generatedPositionFor =
	    function SourceMapConsumer_generatedPositionFor(aArgs) {
	      var source = util.getArg(aArgs, 'source');
	      if (this.sourceRoot != null) {
	        source = util.relative(this.sourceRoot, source);
	      }
	      if (!this._sources.has(source)) {
	        return {
	          line: null,
	          column: null,
	          lastColumn: null
	        };
	      }
	      source = this._sources.indexOf(source);
	
	      var needle = {
	        source: source,
	        originalLine: util.getArg(aArgs, 'line'),
	        originalColumn: util.getArg(aArgs, 'column')
	      };
	
	      var index = this._findMapping(
	        needle,
	        this._originalMappings,
	        "originalLine",
	        "originalColumn",
	        util.compareByOriginalPositions,
	        util.getArg(aArgs, 'bias', SourceMapConsumer.GREATEST_LOWER_BOUND)
	      );
	
	      if (index >= 0) {
	        var mapping = this._originalMappings[index];
	
	        if (mapping.source === needle.source) {
	          return {
	            line: util.getArg(mapping, 'generatedLine', null),
	            column: util.getArg(mapping, 'generatedColumn', null),
	            lastColumn: util.getArg(mapping, 'lastGeneratedColumn', null)
	          };
	        }
	      }
	
	      return {
	        line: null,
	        column: null,
	        lastColumn: null
	      };
	    };
	
	  exports.BasicSourceMapConsumer = BasicSourceMapConsumer;
	
	  /**
	   * An IndexedSourceMapConsumer instance represents a parsed source map which
	   * we can query for information. It differs from BasicSourceMapConsumer in
	   * that it takes "indexed" source maps (i.e. ones with a "sections" field) as
	   * input.
	   *
	   * The only parameter is a raw source map (either as a JSON string, or already
	   * parsed to an object). According to the spec for indexed source maps, they
	   * have the following attributes:
	   *
	   *   - version: Which version of the source map spec this map is following.
	   *   - file: Optional. The generated file this source map is associated with.
	   *   - sections: A list of section definitions.
	   *
	   * Each value under the "sections" field has two fields:
	   *   - offset: The offset into the original specified at which this section
	   *       begins to apply, defined as an object with a "line" and "column"
	   *       field.
	   *   - map: A source map definition. This source map could also be indexed,
	   *       but doesn't have to be.
	   *
	   * Instead of the "map" field, it's also possible to have a "url" field
	   * specifying a URL to retrieve a source map from, but that's currently
	   * unsupported.
	   *
	   * Here's an example source map, taken from the source map spec[0], but
	   * modified to omit a section which uses the "url" field.
	   *
	   *  {
	   *    version : 3,
	   *    file: "app.js",
	   *    sections: [{
	   *      offset: {line:100, column:10},
	   *      map: {
	   *        version : 3,
	   *        file: "section.js",
	   *        sources: ["foo.js", "bar.js"],
	   *        names: ["src", "maps", "are", "fun"],
	   *        mappings: "AAAA,E;;ABCDE;"
	   *      }
	   *    }],
	   *  }
	   *
	   * [0]: https://docs.google.com/document/d/1U1RGAehQwRypUTovF1KRlpiOFze0b-_2gc6fAH0KY0k/edit#heading=h.535es3xeprgt
	   */
	  function IndexedSourceMapConsumer(aSourceMap) {
	    var sourceMap = aSourceMap;
	    if (typeof aSourceMap === 'string') {
	      sourceMap = JSON.parse(aSourceMap.replace(/^\)\]\}'/, ''));
	    }
	
	    var version = util.getArg(sourceMap, 'version');
	    var sections = util.getArg(sourceMap, 'sections');
	
	    if (version != this._version) {
	      throw new Error('Unsupported version: ' + version);
	    }
	
	    this._sources = new ArraySet();
	    this._names = new ArraySet();
	
	    var lastOffset = {
	      line: -1,
	      column: 0
	    };
	    this._sections = sections.map(function (s) {
	      if (s.url) {
	        // The url field will require support for asynchronicity.
	        // See https://github.com/mozilla/source-map/issues/16
	        throw new Error('Support for url field in sections not implemented.');
	      }
	      var offset = util.getArg(s, 'offset');
	      var offsetLine = util.getArg(offset, 'line');
	      var offsetColumn = util.getArg(offset, 'column');
	
	      if (offsetLine < lastOffset.line ||
	          (offsetLine === lastOffset.line && offsetColumn < lastOffset.column)) {
	        throw new Error('Section offsets must be ordered and non-overlapping.');
	      }
	      lastOffset = offset;
	
	      return {
	        generatedOffset: {
	          // The offset fields are 0-based, but we use 1-based indices when
	          // encoding/decoding from VLQ.
	          generatedLine: offsetLine + 1,
	          generatedColumn: offsetColumn + 1
	        },
	        consumer: new SourceMapConsumer(util.getArg(s, 'map'))
	      }
	    });
	  }
	
	  IndexedSourceMapConsumer.prototype = Object.create(SourceMapConsumer.prototype);
	  IndexedSourceMapConsumer.prototype.constructor = SourceMapConsumer;
	
	  /**
	   * The version of the source mapping spec that we are consuming.
	   */
	  IndexedSourceMapConsumer.prototype._version = 3;
	
	  /**
	   * The list of original sources.
	   */
	  Object.defineProperty(IndexedSourceMapConsumer.prototype, 'sources', {
	    get: function () {
	      var sources = [];
	      for (var i = 0; i < this._sections.length; i++) {
	        for (var j = 0; j < this._sections[i].consumer.sources.length; j++) {
	          sources.push(this._sections[i].consumer.sources[j]);
	        }
	      }
	      return sources;
	    }
	  });
	
	  /**
	   * Returns the original source, line, and column information for the generated
	   * source's line and column positions provided. The only argument is an object
	   * with the following properties:
	   *
	   *   - line: The line number in the generated source.
	   *   - column: The column number in the generated source.
	   *
	   * and an object is returned with the following properties:
	   *
	   *   - source: The original source file, or null.
	   *   - line: The line number in the original source, or null.
	   *   - column: The column number in the original source, or null.
	   *   - name: The original identifier, or null.
	   */
	  IndexedSourceMapConsumer.prototype.originalPositionFor =
	    function IndexedSourceMapConsumer_originalPositionFor(aArgs) {
	      var needle = {
	        generatedLine: util.getArg(aArgs, 'line'),
	        generatedColumn: util.getArg(aArgs, 'column')
	      };
	
	      // Find the section containing the generated position we're trying to map
	      // to an original position.
	      var sectionIndex = binarySearch.search(needle, this._sections,
	        function(needle, section) {
	          var cmp = needle.generatedLine - section.generatedOffset.generatedLine;
	          if (cmp) {
	            return cmp;
	          }
	
	          return (needle.generatedColumn -
	                  section.generatedOffset.generatedColumn);
	        });
	      var section = this._sections[sectionIndex];
	
	      if (!section) {
	        return {
	          source: null,
	          line: null,
	          column: null,
	          name: null
	        };
	      }
	
	      return section.consumer.originalPositionFor({
	        line: needle.generatedLine -
	          (section.generatedOffset.generatedLine - 1),
	        column: needle.generatedColumn -
	          (section.generatedOffset.generatedLine === needle.generatedLine
	           ? section.generatedOffset.generatedColumn - 1
	           : 0),
	        bias: aArgs.bias
	      });
	    };
	
	  /**
	   * Return true if we have the source content for every source in the source
	   * map, false otherwise.
	   */
	  IndexedSourceMapConsumer.prototype.hasContentsOfAllSources =
	    function IndexedSourceMapConsumer_hasContentsOfAllSources() {
	      return this._sections.every(function (s) {
	        return s.consumer.hasContentsOfAllSources();
	      });
	    };
	
	  /**
	   * Returns the original source content. The only argument is the url of the
	   * original source file. Returns null if no original source content is
	   * available.
	   */
	  IndexedSourceMapConsumer.prototype.sourceContentFor =
	    function IndexedSourceMapConsumer_sourceContentFor(aSource, nullOnMissing) {
	      for (var i = 0; i < this._sections.length; i++) {
	        var section = this._sections[i];
	
	        var content = section.consumer.sourceContentFor(aSource, true);
	        if (content) {
	          return content;
	        }
	      }
	      if (nullOnMissing) {
	        return null;
	      }
	      else {
	        throw new Error('"' + aSource + '" is not in the SourceMap.');
	      }
	    };
	
	  /**
	   * Returns the generated line and column information for the original source,
	   * line, and column positions provided. The only argument is an object with
	   * the following properties:
	   *
	   *   - source: The filename of the original source.
	   *   - line: The line number in the original source.
	   *   - column: The column number in the original source.
	   *
	   * and an object is returned with the following properties:
	   *
	   *   - line: The line number in the generated source, or null.
	   *   - column: The column number in the generated source, or null.
	   */
	  IndexedSourceMapConsumer.prototype.generatedPositionFor =
	    function IndexedSourceMapConsumer_generatedPositionFor(aArgs) {
	      for (var i = 0; i < this._sections.length; i++) {
	        var section = this._sections[i];
	
	        // Only consider this section if the requested source is in the list of
	        // sources of the consumer.
	        if (section.consumer.sources.indexOf(util.getArg(aArgs, 'source')) === -1) {
	          continue;
	        }
	        var generatedPosition = section.consumer.generatedPositionFor(aArgs);
	        if (generatedPosition) {
	          var ret = {
	            line: generatedPosition.line +
	              (section.generatedOffset.generatedLine - 1),
	            column: generatedPosition.column +
	              (section.generatedOffset.generatedLine === generatedPosition.line
	               ? section.generatedOffset.generatedColumn - 1
	               : 0)
	          };
	          return ret;
	        }
	      }
	
	      return {
	        line: null,
	        column: null
	      };
	    };
	
	  /**
	   * Parse the mappings in a string in to a data structure which we can easily
	   * query (the ordered arrays in the `this.__generatedMappings` and
	   * `this.__originalMappings` properties).
	   */
	  IndexedSourceMapConsumer.prototype._parseMappings =
	    function IndexedSourceMapConsumer_parseMappings(aStr, aSourceRoot) {
	      this.__generatedMappings = [];
	      this.__originalMappings = [];
	      for (var i = 0; i < this._sections.length; i++) {
	        var section = this._sections[i];
	        var sectionMappings = section.consumer._generatedMappings;
	        for (var j = 0; j < sectionMappings.length; j++) {
	          var mapping = sectionMappings[i];
	
	          var source = section.consumer._sources.at(mapping.source);
	          if (section.consumer.sourceRoot !== null) {
	            source = util.join(section.consumer.sourceRoot, source);
	          }
	          this._sources.add(source);
	          source = this._sources.indexOf(source);
	
	          var name = section.consumer._names.at(mapping.name);
	          this._names.add(name);
	          name = this._names.indexOf(name);
	
	          // The mappings coming from the consumer for the section have
	          // generated positions relative to the start of the section, so we
	          // need to offset them to be relative to the start of the concatenated
	          // generated file.
	          var adjustedMapping = {
	            source: source,
	            generatedLine: mapping.generatedLine +
	              (section.generatedOffset.generatedLine - 1),
	            generatedColumn: mapping.column +
	              (section.generatedOffset.generatedLine === mapping.generatedLine)
	              ? section.generatedOffset.generatedColumn - 1
	              : 0,
	            originalLine: mapping.originalLine,
	            originalColumn: mapping.originalColumn,
	            name: name
	          };
	
	          this.__generatedMappings.push(adjustedMapping);
	          if (typeof adjustedMapping.originalLine === 'number') {
	            this.__originalMappings.push(adjustedMapping);
	          }
	        }
	      }
	
	      quickSort(this.__generatedMappings, util.compareByGeneratedPositionsDeflated);
	      quickSort(this.__originalMappings, util.compareByOriginalPositions);
	    };
	
	  exports.IndexedSourceMapConsumer = IndexedSourceMapConsumer;
	}


/***/ },
/* 4 */
/***/ function(module, exports) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	{
	  exports.GREATEST_LOWER_BOUND = 1;
	  exports.LEAST_UPPER_BOUND = 2;
	
	  /**
	   * Recursive implementation of binary search.
	   *
	   * @param aLow Indices here and lower do not contain the needle.
	   * @param aHigh Indices here and higher do not contain the needle.
	   * @param aNeedle The element being searched for.
	   * @param aHaystack The non-empty array being searched.
	   * @param aCompare Function which takes two elements and returns -1, 0, or 1.
	   * @param aBias Either 'binarySearch.GREATEST_LOWER_BOUND' or
	   *     'binarySearch.LEAST_UPPER_BOUND'. Specifies whether to return the
	   *     closest element that is smaller than or greater than the one we are
	   *     searching for, respectively, if the exact element cannot be found.
	   */
	  function recursiveSearch(aLow, aHigh, aNeedle, aHaystack, aCompare, aBias) {
	    // This function terminates when one of the following is true:
	    //
	    //   1. We find the exact element we are looking for.
	    //
	    //   2. We did not find the exact element, but we can return the index of
	    //      the next-closest element.
	    //
	    //   3. We did not find the exact element, and there is no next-closest
	    //      element than the one we are searching for, so we return -1.
	    var mid = Math.floor((aHigh - aLow) / 2) + aLow;
	    var cmp = aCompare(aNeedle, aHaystack[mid], true);
	    if (cmp === 0) {
	      // Found the element we are looking for.
	      return mid;
	    }
	    else if (cmp > 0) {
	      // Our needle is greater than aHaystack[mid].
	      if (aHigh - mid > 1) {
	        // The element is in the upper half.
	        return recursiveSearch(mid, aHigh, aNeedle, aHaystack, aCompare, aBias);
	      }
	
	      // The exact needle element was not found in this haystack. Determine if
	      // we are in termination case (3) or (2) and return the appropriate thing.
	      if (aBias == exports.LEAST_UPPER_BOUND) {
	        return aHigh < aHaystack.length ? aHigh : -1;
	      } else {
	        return mid;
	      }
	    }
	    else {
	      // Our needle is less than aHaystack[mid].
	      if (mid - aLow > 1) {
	        // The element is in the lower half.
	        return recursiveSearch(aLow, mid, aNeedle, aHaystack, aCompare, aBias);
	      }
	
	      // we are in termination case (3) or (2) and return the appropriate thing.
	      if (aBias == exports.LEAST_UPPER_BOUND) {
	        return mid;
	      } else {
	        return aLow < 0 ? -1 : aLow;
	      }
	    }
	  }
	
	  /**
	   * This is an implementation of binary search which will always try and return
	   * the index of the closest element if there is no exact hit. This is because
	   * mappings between original and generated line/col pairs are single points,
	   * and there is an implicit region between each of them, so a miss just means
	   * that you aren't on the very start of a region.
	   *
	   * @param aNeedle The element you are looking for.
	   * @param aHaystack The array that is being searched.
	   * @param aCompare A function which takes the needle and an element in the
	   *     array and returns -1, 0, or 1 depending on whether the needle is less
	   *     than, equal to, or greater than the element, respectively.
	   * @param aBias Either 'binarySearch.GREATEST_LOWER_BOUND' or
	   *     'binarySearch.LEAST_UPPER_BOUND'. Specifies whether to return the
	   *     closest element that is smaller than or greater than the one we are
	   *     searching for, respectively, if the exact element cannot be found.
	   *     Defaults to 'binarySearch.GREATEST_LOWER_BOUND'.
	   */
	  exports.search = function search(aNeedle, aHaystack, aCompare, aBias) {
	    if (aHaystack.length === 0) {
	      return -1;
	    }
	
	    var index = recursiveSearch(-1, aHaystack.length, aNeedle, aHaystack,
	                                aCompare, aBias || exports.GREATEST_LOWER_BOUND);
	    if (index < 0) {
	      return -1;
	    }
	
	    // We have found either the exact element, or the next-closest element than
	    // the one we are searching for. However, there may be more than one such
	    // element. Make sure we always return the smallest of these.
	    while (index - 1 >= 0) {
	      if (aCompare(aHaystack[index], aHaystack[index - 1], true) !== 0) {
	        break;
	      }
	      --index;
	    }
	
	    return index;
	  };
	}


/***/ },
/* 5 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	{
	  var util = __webpack_require__(2);
	
	  /**
	   * A data structure which is a combination of an array and a set. Adding a new
	   * member is O(1), testing for membership is O(1), and finding the index of an
	   * element is O(1). Removing elements from the set is not supported. Only
	   * strings are supported for membership.
	   */
	  function ArraySet() {
	    this._array = [];
	    this._set = {};
	  }
	
	  /**
	   * Static method for creating ArraySet instances from an existing array.
	   */
	  ArraySet.fromArray = function ArraySet_fromArray(aArray, aAllowDuplicates) {
	    var set = new ArraySet();
	    for (var i = 0, len = aArray.length; i < len; i++) {
	      set.add(aArray[i], aAllowDuplicates);
	    }
	    return set;
	  };
	
	  /**
	   * Return how many unique items are in this ArraySet. If duplicates have been
	   * added, than those do not count towards the size.
	   *
	   * @returns Number
	   */
	  ArraySet.prototype.size = function ArraySet_size() {
	    return Object.getOwnPropertyNames(this._set).length;
	  };
	
	  /**
	   * Add the given string to this set.
	   *
	   * @param String aStr
	   */
	  ArraySet.prototype.add = function ArraySet_add(aStr, aAllowDuplicates) {
	    var sStr = util.toSetString(aStr);
	    var isDuplicate = this._set.hasOwnProperty(sStr);
	    var idx = this._array.length;
	    if (!isDuplicate || aAllowDuplicates) {
	      this._array.push(aStr);
	    }
	    if (!isDuplicate) {
	      this._set[sStr] = idx;
	    }
	  };
	
	  /**
	   * Is the given string a member of this set?
	   *
	   * @param String aStr
	   */
	  ArraySet.prototype.has = function ArraySet_has(aStr) {
	    var sStr = util.toSetString(aStr);
	    return this._set.hasOwnProperty(sStr);
	  };
	
	  /**
	   * What is the index of the given string in the array?
	   *
	   * @param String aStr
	   */
	  ArraySet.prototype.indexOf = function ArraySet_indexOf(aStr) {
	    var sStr = util.toSetString(aStr);
	    if (this._set.hasOwnProperty(sStr)) {
	      return this._set[sStr];
	    }
	    throw new Error('"' + aStr + '" is not in the set.');
	  };
	
	  /**
	   * What is the element at the given index?
	   *
	   * @param Number aIdx
	   */
	  ArraySet.prototype.at = function ArraySet_at(aIdx) {
	    if (aIdx >= 0 && aIdx < this._array.length) {
	      return this._array[aIdx];
	    }
	    throw new Error('No element indexed by ' + aIdx);
	  };
	
	  /**
	   * Returns the array representation of this set (which has the proper indices
	   * indicated by indexOf). Note that this is a copy of the internal array used
	   * for storing the members so that no one can mess with internal state.
	   */
	  ArraySet.prototype.toArray = function ArraySet_toArray() {
	    return this._array.slice();
	  };
	
	  exports.ArraySet = ArraySet;
	}


/***/ },
/* 6 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 *
	 * Based on the Base 64 VLQ implementation in Closure Compiler:
	 * https://code.google.com/p/closure-compiler/source/browse/trunk/src/com/google/debugging/sourcemap/Base64VLQ.java
	 *
	 * Copyright 2011 The Closure Compiler Authors. All rights reserved.
	 * Redistribution and use in source and binary forms, with or without
	 * modification, are permitted provided that the following conditions are
	 * met:
	 *
	 *  * Redistributions of source code must retain the above copyright
	 *    notice, this list of conditions and the following disclaimer.
	 *  * Redistributions in binary form must reproduce the above
	 *    copyright notice, this list of conditions and the following
	 *    disclaimer in the documentation and/or other materials provided
	 *    with the distribution.
	 *  * Neither the name of Google Inc. nor the names of its
	 *    contributors may be used to endorse or promote products derived
	 *    from this software without specific prior written permission.
	 *
	 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
	 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
	 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
	 */
	{
	  var base64 = __webpack_require__(7);
	
	  // A single base 64 digit can contain 6 bits of data. For the base 64 variable
	  // length quantities we use in the source map spec, the first bit is the sign,
	  // the next four bits are the actual value, and the 6th bit is the
	  // continuation bit. The continuation bit tells us whether there are more
	  // digits in this value following this digit.
	  //
	  //   Continuation
	  //   |    Sign
	  //   |    |
	  //   V    V
	  //   101011
	
	  var VLQ_BASE_SHIFT = 5;
	
	  // binary: 100000
	  var VLQ_BASE = 1 << VLQ_BASE_SHIFT;
	
	  // binary: 011111
	  var VLQ_BASE_MASK = VLQ_BASE - 1;
	
	  // binary: 100000
	  var VLQ_CONTINUATION_BIT = VLQ_BASE;
	
	  /**
	   * Converts from a two-complement value to a value where the sign bit is
	   * placed in the least significant bit.  For example, as decimals:
	   *   1 becomes 2 (10 binary), -1 becomes 3 (11 binary)
	   *   2 becomes 4 (100 binary), -2 becomes 5 (101 binary)
	   */
	  function toVLQSigned(aValue) {
	    return aValue < 0
	      ? ((-aValue) << 1) + 1
	      : (aValue << 1) + 0;
	  }
	
	  /**
	   * Converts to a two-complement value from a value where the sign bit is
	   * placed in the least significant bit.  For example, as decimals:
	   *   2 (10 binary) becomes 1, 3 (11 binary) becomes -1
	   *   4 (100 binary) becomes 2, 5 (101 binary) becomes -2
	   */
	  function fromVLQSigned(aValue) {
	    var isNegative = (aValue & 1) === 1;
	    var shifted = aValue >> 1;
	    return isNegative
	      ? -shifted
	      : shifted;
	  }
	
	  /**
	   * Returns the base 64 VLQ encoded value.
	   */
	  exports.encode = function base64VLQ_encode(aValue) {
	    var encoded = "";
	    var digit;
	
	    var vlq = toVLQSigned(aValue);
	
	    do {
	      digit = vlq & VLQ_BASE_MASK;
	      vlq >>>= VLQ_BASE_SHIFT;
	      if (vlq > 0) {
	        // There are still more digits in this value, so we must make sure the
	        // continuation bit is marked.
	        digit |= VLQ_CONTINUATION_BIT;
	      }
	      encoded += base64.encode(digit);
	    } while (vlq > 0);
	
	    return encoded;
	  };
	
	  /**
	   * Decodes the next base 64 VLQ value from the given string and returns the
	   * value and the rest of the string via the out parameter.
	   */
	  exports.decode = function base64VLQ_decode(aStr, aIndex, aOutParam) {
	    var strLen = aStr.length;
	    var result = 0;
	    var shift = 0;
	    var continuation, digit;
	
	    do {
	      if (aIndex >= strLen) {
	        throw new Error("Expected more digits in base 64 VLQ value.");
	      }
	
	      digit = base64.decode(aStr.charCodeAt(aIndex++));
	      if (digit === -1) {
	        throw new Error("Invalid base64 digit: " + aStr.charAt(aIndex - 1));
	      }
	
	      continuation = !!(digit & VLQ_CONTINUATION_BIT);
	      digit &= VLQ_BASE_MASK;
	      result = result + (digit << shift);
	      shift += VLQ_BASE_SHIFT;
	    } while (continuation);
	
	    aOutParam.value = fromVLQSigned(result);
	    aOutParam.rest = aIndex;
	  };
	}


/***/ },
/* 7 */
/***/ function(module, exports) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	{
	  var intToCharMap = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'.split('');
	
	  /**
	   * Encode an integer in the range of 0 to 63 to a single base 64 digit.
	   */
	  exports.encode = function (number) {
	    if (0 <= number && number < intToCharMap.length) {
	      return intToCharMap[number];
	    }
	    throw new TypeError("Must be between 0 and 63: " + number);
	  };
	
	  /**
	   * Decode a single base 64 character code digit to an integer. Returns -1 on
	   * failure.
	   */
	  exports.decode = function (charCode) {
	    var bigA = 65;     // 'A'
	    var bigZ = 90;     // 'Z'
	
	    var littleA = 97;  // 'a'
	    var littleZ = 122; // 'z'
	
	    var zero = 48;     // '0'
	    var nine = 57;     // '9'
	
	    var plus = 43;     // '+'
	    var slash = 47;    // '/'
	
	    var littleOffset = 26;
	    var numberOffset = 52;
	
	    // 0 - 25: ABCDEFGHIJKLMNOPQRSTUVWXYZ
	    if (bigA <= charCode && charCode <= bigZ) {
	      return (charCode - bigA);
	    }
	
	    // 26 - 51: abcdefghijklmnopqrstuvwxyz
	    if (littleA <= charCode && charCode <= littleZ) {
	      return (charCode - littleA + littleOffset);
	    }
	
	    // 52 - 61: 0123456789
	    if (zero <= charCode && charCode <= nine) {
	      return (charCode - zero + numberOffset);
	    }
	
	    // 62: +
	    if (charCode == plus) {
	      return 62;
	    }
	
	    // 63: /
	    if (charCode == slash) {
	      return 63;
	    }
	
	    // Invalid base64 digit.
	    return -1;
	  };
	}


/***/ },
/* 8 */
/***/ function(module, exports) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	{
	  // It turns out that some (most?) JavaScript engines don't self-host
	  // `Array.prototype.sort`. This makes sense because C++ will likely remain
	  // faster than JS when doing raw CPU-intensive sorting. However, when using a
	  // custom comparator function, calling back and forth between the VM's C++ and
	  // JIT'd JS is rather slow *and* loses JIT type information, resulting in
	  // worse generated code for the comparator function than would be optimal. In
	  // fact, when sorting with a comparator, these costs outweigh the benefits of
	  // sorting in C++. By using our own JS-implemented Quick Sort (below), we get
	  // a ~3500ms mean speed-up in `bench/bench.html`.
	
	  /**
	   * Swap the elements indexed by `x` and `y` in the array `ary`.
	   *
	   * @param {Array} ary
	   *        The array.
	   * @param {Number} x
	   *        The index of the first item.
	   * @param {Number} y
	   *        The index of the second item.
	   */
	  function swap(ary, x, y) {
	    var temp = ary[x];
	    ary[x] = ary[y];
	    ary[y] = temp;
	  }
	
	  /**
	   * Returns a random integer within the range `low .. high` inclusive.
	   *
	   * @param {Number} low
	   *        The lower bound on the range.
	   * @param {Number} high
	   *        The upper bound on the range.
	   */
	  function randomIntInRange(low, high) {
	    return Math.round(low + (Math.random() * (high - low)));
	  }
	
	  /**
	   * The Quick Sort algorithm.
	   *
	   * @param {Array} ary
	   *        An array to sort.
	   * @param {function} comparator
	   *        Function to use to compare two items.
	   * @param {Number} p
	   *        Start index of the array
	   * @param {Number} r
	   *        End index of the array
	   */
	  function doQuickSort(ary, comparator, p, r) {
	    // If our lower bound is less than our upper bound, we (1) partition the
	    // array into two pieces and (2) recurse on each half. If it is not, this is
	    // the empty array and our base case.
	
	    if (p < r) {
	      // (1) Partitioning.
	      //
	      // The partitioning chooses a pivot between `p` and `r` and moves all
	      // elements that are less than or equal to the pivot to the before it, and
	      // all the elements that are greater than it after it. The effect is that
	      // once partition is done, the pivot is in the exact place it will be when
	      // the array is put in sorted order, and it will not need to be moved
	      // again. This runs in O(n) time.
	
	      // Always choose a random pivot so that an input array which is reverse
	      // sorted does not cause O(n^2) running time.
	      var pivotIndex = randomIntInRange(p, r);
	      var i = p - 1;
	
	      swap(ary, pivotIndex, r);
	      var pivot = ary[r];
	
	      // Immediately after `j` is incremented in this loop, the following hold
	      // true:
	      //
	      //   * Every element in `ary[p .. i]` is less than or equal to the pivot.
	      //
	      //   * Every element in `ary[i+1 .. j-1]` is greater than the pivot.
	      for (var j = p; j < r; j++) {
	        if (comparator(ary[j], pivot) <= 0) {
	          i += 1;
	          swap(ary, i, j);
	        }
	      }
	
	      swap(ary, i + 1, j);
	      var q = i + 1;
	
	      // (2) Recurse on each half.
	
	      doQuickSort(ary, comparator, p, q - 1);
	      doQuickSort(ary, comparator, q + 1, r);
	    }
	  }
	
	  /**
	   * Sort the given array in-place with the given comparator function.
	   *
	   * @param {Array} ary
	   *        An array to sort.
	   * @param {function} comparator
	   *        Function to use to compare two items.
	   */
	  exports.quickSort = function (ary, comparator) {
	    doQuickSort(ary, comparator, 0, ary.length - 1);
	  };
	}


/***/ },
/* 9 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	{
	  var base64VLQ = __webpack_require__(6);
	  var util = __webpack_require__(2);
	  var ArraySet = __webpack_require__(5).ArraySet;
	  var MappingList = __webpack_require__(10).MappingList;
	
	  /**
	   * An instance of the SourceMapGenerator represents a source map which is
	   * being built incrementally. You may pass an object with the following
	   * properties:
	   *
	   *   - file: The filename of the generated source.
	   *   - sourceRoot: A root for all relative URLs in this source map.
	   */
	  function SourceMapGenerator(aArgs) {
	    if (!aArgs) {
	      aArgs = {};
	    }
	    this._file = util.getArg(aArgs, 'file', null);
	    this._sourceRoot = util.getArg(aArgs, 'sourceRoot', null);
	    this._skipValidation = util.getArg(aArgs, 'skipValidation', false);
	    this._sources = new ArraySet();
	    this._names = new ArraySet();
	    this._mappings = new MappingList();
	    this._sourcesContents = null;
	  }
	
	  SourceMapGenerator.prototype._version = 3;
	
	  /**
	   * Creates a new SourceMapGenerator based on a SourceMapConsumer
	   *
	   * @param aSourceMapConsumer The SourceMap.
	   */
	  SourceMapGenerator.fromSourceMap =
	    function SourceMapGenerator_fromSourceMap(aSourceMapConsumer) {
	      var sourceRoot = aSourceMapConsumer.sourceRoot;
	      var generator = new SourceMapGenerator({
	        file: aSourceMapConsumer.file,
	        sourceRoot: sourceRoot
	      });
	      aSourceMapConsumer.eachMapping(function (mapping) {
	        var newMapping = {
	          generated: {
	            line: mapping.generatedLine,
	            column: mapping.generatedColumn
	          }
	        };
	
	        if (mapping.source != null) {
	          newMapping.source = mapping.source;
	          if (sourceRoot != null) {
	            newMapping.source = util.relative(sourceRoot, newMapping.source);
	          }
	
	          newMapping.original = {
	            line: mapping.originalLine,
	            column: mapping.originalColumn
	          };
	
	          if (mapping.name != null) {
	            newMapping.name = mapping.name;
	          }
	        }
	
	        generator.addMapping(newMapping);
	      });
	      aSourceMapConsumer.sources.forEach(function (sourceFile) {
	        var content = aSourceMapConsumer.sourceContentFor(sourceFile);
	        if (content != null) {
	          generator.setSourceContent(sourceFile, content);
	        }
	      });
	      return generator;
	    };
	
	  /**
	   * Add a single mapping from original source line and column to the generated
	   * source's line and column for this source map being created. The mapping
	   * object should have the following properties:
	   *
	   *   - generated: An object with the generated line and column positions.
	   *   - original: An object with the original line and column positions.
	   *   - source: The original source file (relative to the sourceRoot).
	   *   - name: An optional original token name for this mapping.
	   */
	  SourceMapGenerator.prototype.addMapping =
	    function SourceMapGenerator_addMapping(aArgs) {
	      var generated = util.getArg(aArgs, 'generated');
	      var original = util.getArg(aArgs, 'original', null);
	      var source = util.getArg(aArgs, 'source', null);
	      var name = util.getArg(aArgs, 'name', null);
	
	      if (!this._skipValidation) {
	        this._validateMapping(generated, original, source, name);
	      }
	
	      if (source != null && !this._sources.has(source)) {
	        this._sources.add(source);
	      }
	
	      if (name != null && !this._names.has(name)) {
	        this._names.add(name);
	      }
	
	      this._mappings.add({
	        generatedLine: generated.line,
	        generatedColumn: generated.column,
	        originalLine: original != null && original.line,
	        originalColumn: original != null && original.column,
	        source: source,
	        name: name
	      });
	    };
	
	  /**
	   * Set the source content for a source file.
	   */
	  SourceMapGenerator.prototype.setSourceContent =
	    function SourceMapGenerator_setSourceContent(aSourceFile, aSourceContent) {
	      var source = aSourceFile;
	      if (this._sourceRoot != null) {
	        source = util.relative(this._sourceRoot, source);
	      }
	
	      if (aSourceContent != null) {
	        // Add the source content to the _sourcesContents map.
	        // Create a new _sourcesContents map if the property is null.
	        if (!this._sourcesContents) {
	          this._sourcesContents = {};
	        }
	        this._sourcesContents[util.toSetString(source)] = aSourceContent;
	      } else if (this._sourcesContents) {
	        // Remove the source file from the _sourcesContents map.
	        // If the _sourcesContents map is empty, set the property to null.
	        delete this._sourcesContents[util.toSetString(source)];
	        if (Object.keys(this._sourcesContents).length === 0) {
	          this._sourcesContents = null;
	        }
	      }
	    };
	
	  /**
	   * Applies the mappings of a sub-source-map for a specific source file to the
	   * source map being generated. Each mapping to the supplied source file is
	   * rewritten using the supplied source map. Note: The resolution for the
	   * resulting mappings is the minimium of this map and the supplied map.
	   *
	   * @param aSourceMapConsumer The source map to be applied.
	   * @param aSourceFile Optional. The filename of the source file.
	   *        If omitted, SourceMapConsumer's file property will be used.
	   * @param aSourceMapPath Optional. The dirname of the path to the source map
	   *        to be applied. If relative, it is relative to the SourceMapConsumer.
	   *        This parameter is needed when the two source maps aren't in the same
	   *        directory, and the source map to be applied contains relative source
	   *        paths. If so, those relative source paths need to be rewritten
	   *        relative to the SourceMapGenerator.
	   */
	  SourceMapGenerator.prototype.applySourceMap =
	    function SourceMapGenerator_applySourceMap(aSourceMapConsumer, aSourceFile, aSourceMapPath) {
	      var sourceFile = aSourceFile;
	      // If aSourceFile is omitted, we will use the file property of the SourceMap
	      if (aSourceFile == null) {
	        if (aSourceMapConsumer.file == null) {
	          throw new Error(
	            'SourceMapGenerator.prototype.applySourceMap requires either an explicit source file, ' +
	            'or the source map\'s "file" property. Both were omitted.'
	          );
	        }
	        sourceFile = aSourceMapConsumer.file;
	      }
	      var sourceRoot = this._sourceRoot;
	      // Make "sourceFile" relative if an absolute Url is passed.
	      if (sourceRoot != null) {
	        sourceFile = util.relative(sourceRoot, sourceFile);
	      }
	      // Applying the SourceMap can add and remove items from the sources and
	      // the names array.
	      var newSources = new ArraySet();
	      var newNames = new ArraySet();
	
	      // Find mappings for the "sourceFile"
	      this._mappings.unsortedForEach(function (mapping) {
	        if (mapping.source === sourceFile && mapping.originalLine != null) {
	          // Check if it can be mapped by the source map, then update the mapping.
	          var original = aSourceMapConsumer.originalPositionFor({
	            line: mapping.originalLine,
	            column: mapping.originalColumn
	          });
	          if (original.source != null) {
	            // Copy mapping
	            mapping.source = original.source;
	            if (aSourceMapPath != null) {
	              mapping.source = util.join(aSourceMapPath, mapping.source)
	            }
	            if (sourceRoot != null) {
	              mapping.source = util.relative(sourceRoot, mapping.source);
	            }
	            mapping.originalLine = original.line;
	            mapping.originalColumn = original.column;
	            if (original.name != null) {
	              mapping.name = original.name;
	            }
	          }
	        }
	
	        var source = mapping.source;
	        if (source != null && !newSources.has(source)) {
	          newSources.add(source);
	        }
	
	        var name = mapping.name;
	        if (name != null && !newNames.has(name)) {
	          newNames.add(name);
	        }
	
	      }, this);
	      this._sources = newSources;
	      this._names = newNames;
	
	      // Copy sourcesContents of applied map.
	      aSourceMapConsumer.sources.forEach(function (sourceFile) {
	        var content = aSourceMapConsumer.sourceContentFor(sourceFile);
	        if (content != null) {
	          if (aSourceMapPath != null) {
	            sourceFile = util.join(aSourceMapPath, sourceFile);
	          }
	          if (sourceRoot != null) {
	            sourceFile = util.relative(sourceRoot, sourceFile);
	          }
	          this.setSourceContent(sourceFile, content);
	        }
	      }, this);
	    };
	
	  /**
	   * A mapping can have one of the three levels of data:
	   *
	   *   1. Just the generated position.
	   *   2. The Generated position, original position, and original source.
	   *   3. Generated and original position, original source, as well as a name
	   *      token.
	   *
	   * To maintain consistency, we validate that any new mapping being added falls
	   * in to one of these categories.
	   */
	  SourceMapGenerator.prototype._validateMapping =
	    function SourceMapGenerator_validateMapping(aGenerated, aOriginal, aSource,
	                                                aName) {
	      if (aGenerated && 'line' in aGenerated && 'column' in aGenerated
	          && aGenerated.line > 0 && aGenerated.column >= 0
	          && !aOriginal && !aSource && !aName) {
	        // Case 1.
	        return;
	      }
	      else if (aGenerated && 'line' in aGenerated && 'column' in aGenerated
	               && aOriginal && 'line' in aOriginal && 'column' in aOriginal
	               && aGenerated.line > 0 && aGenerated.column >= 0
	               && aOriginal.line > 0 && aOriginal.column >= 0
	               && aSource) {
	        // Cases 2 and 3.
	        return;
	      }
	      else {
	        throw new Error('Invalid mapping: ' + JSON.stringify({
	          generated: aGenerated,
	          source: aSource,
	          original: aOriginal,
	          name: aName
	        }));
	      }
	    };
	
	  /**
	   * Serialize the accumulated mappings in to the stream of base 64 VLQs
	   * specified by the source map format.
	   */
	  SourceMapGenerator.prototype._serializeMappings =
	    function SourceMapGenerator_serializeMappings() {
	      var previousGeneratedColumn = 0;
	      var previousGeneratedLine = 1;
	      var previousOriginalColumn = 0;
	      var previousOriginalLine = 0;
	      var previousName = 0;
	      var previousSource = 0;
	      var result = '';
	      var mapping;
	      var nameIdx;
	      var sourceIdx;
	
	      var mappings = this._mappings.toArray();
	      for (var i = 0, len = mappings.length; i < len; i++) {
	        mapping = mappings[i];
	
	        if (mapping.generatedLine !== previousGeneratedLine) {
	          previousGeneratedColumn = 0;
	          while (mapping.generatedLine !== previousGeneratedLine) {
	            result += ';';
	            previousGeneratedLine++;
	          }
	        }
	        else {
	          if (i > 0) {
	            if (!util.compareByGeneratedPositionsInflated(mapping, mappings[i - 1])) {
	              continue;
	            }
	            result += ',';
	          }
	        }
	
	        result += base64VLQ.encode(mapping.generatedColumn
	                                   - previousGeneratedColumn);
	        previousGeneratedColumn = mapping.generatedColumn;
	
	        if (mapping.source != null) {
	          sourceIdx = this._sources.indexOf(mapping.source);
	          result += base64VLQ.encode(sourceIdx - previousSource);
	          previousSource = sourceIdx;
	
	          // lines are stored 0-based in SourceMap spec version 3
	          result += base64VLQ.encode(mapping.originalLine - 1
	                                     - previousOriginalLine);
	          previousOriginalLine = mapping.originalLine - 1;
	
	          result += base64VLQ.encode(mapping.originalColumn
	                                     - previousOriginalColumn);
	          previousOriginalColumn = mapping.originalColumn;
	
	          if (mapping.name != null) {
	            nameIdx = this._names.indexOf(mapping.name);
	            result += base64VLQ.encode(nameIdx - previousName);
	            previousName = nameIdx;
	          }
	        }
	      }
	
	      return result;
	    };
	
	  SourceMapGenerator.prototype._generateSourcesContent =
	    function SourceMapGenerator_generateSourcesContent(aSources, aSourceRoot) {
	      return aSources.map(function (source) {
	        if (!this._sourcesContents) {
	          return null;
	        }
	        if (aSourceRoot != null) {
	          source = util.relative(aSourceRoot, source);
	        }
	        var key = util.toSetString(source);
	        return Object.prototype.hasOwnProperty.call(this._sourcesContents,
	                                                    key)
	          ? this._sourcesContents[key]
	          : null;
	      }, this);
	    };
	
	  /**
	   * Externalize the source map.
	   */
	  SourceMapGenerator.prototype.toJSON =
	    function SourceMapGenerator_toJSON() {
	      var map = {
	        version: this._version,
	        sources: this._sources.toArray(),
	        names: this._names.toArray(),
	        mappings: this._serializeMappings()
	      };
	      if (this._file != null) {
	        map.file = this._file;
	      }
	      if (this._sourceRoot != null) {
	        map.sourceRoot = this._sourceRoot;
	      }
	      if (this._sourcesContents) {
	        map.sourcesContent = this._generateSourcesContent(map.sources, map.sourceRoot);
	      }
	
	      return map;
	    };
	
	  /**
	   * Render the source map being generated to a string.
	   */
	  SourceMapGenerator.prototype.toString =
	    function SourceMapGenerator_toString() {
	      return JSON.stringify(this.toJSON());
	    };
	
	  exports.SourceMapGenerator = SourceMapGenerator;
	}


/***/ },
/* 10 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2014 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	{
	  var util = __webpack_require__(2);
	
	  /**
	   * Determine whether mappingB is after mappingA with respect to generated
	   * position.
	   */
	  function generatedPositionAfter(mappingA, mappingB) {
	    // Optimized for most common case
	    var lineA = mappingA.generatedLine;
	    var lineB = mappingB.generatedLine;
	    var columnA = mappingA.generatedColumn;
	    var columnB = mappingB.generatedColumn;
	    return lineB > lineA || lineB == lineA && columnB >= columnA ||
	           util.compareByGeneratedPositionsInflated(mappingA, mappingB) <= 0;
	  }
	
	  /**
	   * A data structure to provide a sorted view of accumulated mappings in a
	   * performance conscious manner. It trades a neglibable overhead in general
	   * case for a large speedup in case of mappings being added in order.
	   */
	  function MappingList() {
	    this._array = [];
	    this._sorted = true;
	    // Serves as infimum
	    this._last = {generatedLine: -1, generatedColumn: 0};
	  }
	
	  /**
	   * Iterate through internal items. This method takes the same arguments that
	   * `Array.prototype.forEach` takes.
	   *
	   * NOTE: The order of the mappings is NOT guaranteed.
	   */
	  MappingList.prototype.unsortedForEach =
	    function MappingList_forEach(aCallback, aThisArg) {
	      this._array.forEach(aCallback, aThisArg);
	    };
	
	  /**
	   * Add the given source mapping.
	   *
	   * @param Object aMapping
	   */
	  MappingList.prototype.add = function MappingList_add(aMapping) {
	    if (generatedPositionAfter(this._last, aMapping)) {
	      this._last = aMapping;
	      this._array.push(aMapping);
	    } else {
	      this._sorted = false;
	      this._array.push(aMapping);
	    }
	  };
	
	  /**
	   * Returns the flat, sorted array of mappings. The mappings are sorted by
	   * generated position.
	   *
	   * WARNING: This method returns internal data without copying, for
	   * performance. The return value must NOT be mutated, and should be treated as
	   * an immutable borrow. If you want to take ownership, you must make your own
	   * copy.
	   */
	  MappingList.prototype.toArray = function MappingList_toArray() {
	    if (!this._sorted) {
	      this._array.sort(util.compareByGeneratedPositionsInflated);
	      this._sorted = true;
	    }
	    return this._array;
	  };
	
	  exports.MappingList = MappingList;
	}


/***/ }
/******/ ]);
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbIndlYnBhY2s6Ly8vd2VicGFjay9ib290c3RyYXAgZGExYmM2OWM5ZmUyOGIwN2RiNzIiLCJ3ZWJwYWNrOi8vLy4vdGVzdC90ZXN0LXNvdXJjZS1tYXAtY29uc3VtZXIuanMiLCJ3ZWJwYWNrOi8vLy4vdGVzdC91dGlsLmpzIiwid2VicGFjazovLy8uL2xpYi91dGlsLmpzIiwid2VicGFjazovLy8uL2xpYi9zb3VyY2UtbWFwLWNvbnN1bWVyLmpzIiwid2VicGFjazovLy8uL2xpYi9iaW5hcnktc2VhcmNoLmpzIiwid2VicGFjazovLy8uL2xpYi9hcnJheS1zZXQuanMiLCJ3ZWJwYWNrOi8vLy4vbGliL2Jhc2U2NC12bHEuanMiLCJ3ZWJwYWNrOi8vLy4vbGliL2Jhc2U2NC5qcyIsIndlYnBhY2s6Ly8vLi9saWIvcXVpY2stc29ydC5qcyIsIndlYnBhY2s6Ly8vLi9saWIvc291cmNlLW1hcC1nZW5lcmF0b3IuanMiLCJ3ZWJwYWNrOi8vLy4vbGliL21hcHBpbmctbGlzdC5qcyJdLCJuYW1lcyI6W10sIm1hcHBpbmdzIjoiOzs7Ozs7Ozs7OztBQUFBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBLHVCQUFlO0FBQ2Y7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7OztBQUdBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7Ozs7Ozs7QUN0Q0EsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBO0FBQ0EsTUFBSztBQUNMOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMOztBQUVBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDs7O0FBR0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMOztBQUVBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDs7O0FBR0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMOztBQUVBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7O0FBR0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7O0FBRUw7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQSx1REFBc0Q7QUFDdEQ7QUFDQSwyQ0FBMEM7QUFDMUMsTUFBSztBQUNMOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSzs7QUFFTDtBQUNBO0FBQ0E7QUFDQSxNQUFLOztBQUVMO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7OztBQUdBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMOztBQUVBO0FBQ0E7QUFDQTs7QUFFQSxpRUFBZ0UscUJBQXFCLEtBQUs7QUFDMUYsK0RBQThELGtCQUFrQixLQUFLO0FBQ3JGO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBLGdGQUErRSxxQkFBcUIsS0FBSztBQUN6Ryw4RUFBNkUsa0JBQWtCLEtBQUs7QUFDcEcsOEVBQTZFLHFCQUFxQixLQUFLO0FBQ3ZHLDRFQUEyRSxrQkFBa0IsS0FBSztBQUNsRztBQUNBO0FBQ0EsTUFBSztBQUNMO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQTtBQUNBLE1BQUs7QUFDTDs7QUFFQTtBQUNBO0FBQ0E7O0FBRUEsZ0ZBQStFLHFCQUFxQixLQUFLO0FBQ3pHLDhFQUE2RSxrQkFBa0IsS0FBSztBQUNwRyw4RUFBNkUscUJBQXFCLEtBQUs7QUFDdkcsNEVBQTJFLGtCQUFrQixLQUFLO0FBQ2xHO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBO0FBQ0EsTUFBSztBQUNMOztBQUVBO0FBQ0E7QUFDQTs7QUFFQSxnRkFBK0UscUJBQXFCLEtBQUs7QUFDekcsOEVBQTZFLGtCQUFrQixLQUFLO0FBQ3BHLDhFQUE2RSxxQkFBcUIsS0FBSztBQUN2Ryw0RUFBMkUsa0JBQWtCLEtBQUs7QUFDbEc7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBO0FBQ0EsTUFBSztBQUNMO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsaUNBQWdDO0FBQ2hDLDZDQUE0QyxzQkFBc0I7QUFDbEUsOENBQTZDLHNCQUFzQixFQUFFO0FBQ3JFO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLG9DQUFtQztBQUNuQyxnREFBK0Msc0JBQXNCO0FBQ3JFLGlEQUFnRCxzQkFBc0IsRUFBRTtBQUN4RTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EscUNBQW9DO0FBQ3BDLGlEQUFnRCxzQkFBc0I7QUFDdEUsa0RBQWlELHNCQUFzQixFQUFFO0FBQ3pFLHFDQUFvQztBQUNwQyxpREFBZ0Qsc0JBQXNCO0FBQ3RFLGtEQUFpRCxzQkFBc0IsRUFBRTtBQUN6RTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLHdDQUF1QztBQUN2QyxvREFBbUQsc0JBQXNCO0FBQ3pFLHFEQUFvRCxzQkFBc0IsRUFBRTtBQUM1RSx3Q0FBdUM7QUFDdkMsb0RBQW1ELHNCQUFzQjtBQUN6RSxxREFBb0Qsc0JBQXNCLEVBQUU7QUFDNUU7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixxQkFBcUI7QUFDdkM7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixxQkFBcUI7QUFDdkM7QUFDQSxNQUFLO0FBQ0w7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7O0FBRUw7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSzs7QUFFTDtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixxQkFBcUI7QUFDdkM7QUFDQSxNQUFLO0FBQ0w7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7O0FBRUw7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7QUFDTDs7QUFFQTtBQUNBO0FBQ0E7QUFDQSxNQUFLOztBQUVMO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixxQkFBcUI7QUFDdkM7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixxQkFBcUI7QUFDdkM7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixxQkFBcUI7QUFDdkM7QUFDQSxNQUFLO0FBQ0w7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsTUFBSzs7QUFFTDtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsTUFBSzs7QUFFTDtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7QUFDTDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7O0FBRUw7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7QUFDTDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7O0FBRUw7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7QUFDTDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7O0FBRUw7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixxQkFBcUI7QUFDdkM7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixxQkFBcUI7QUFDdkM7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixzQkFBc0I7QUFDeEM7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixzQkFBc0I7QUFDeEM7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixxQkFBcUI7QUFDdkM7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixxQkFBcUI7QUFDdkM7QUFDQSxNQUFLO0FBQ0w7O0FBRUE7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsTUFBSzs7QUFFTDtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7O0FBRUw7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsTUFBSzs7QUFFTDtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBLGtCQUFpQixxQkFBcUI7QUFDdEMsbUJBQWtCLHFCQUFxQjtBQUN2QztBQUNBLE1BQUs7QUFDTDs7QUFFQTtBQUNBO0FBQ0E7QUFDQSxNQUFLOztBQUVMO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixxQkFBcUI7QUFDdkM7QUFDQSxNQUFLO0FBQ0w7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixxQkFBcUI7QUFDdkM7QUFDQSxNQUFLO0FBQ0w7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMO0FBQ0Esa0JBQWlCLHFCQUFxQjtBQUN0QyxtQkFBa0IscUJBQXFCO0FBQ3ZDO0FBQ0EsTUFBSztBQUNMOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBLE1BQUs7QUFDTDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLOztBQUVMO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLOztBQUVMO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxxQkFBb0IsTUFBTSxNQUFNO0FBQ2hDO0FBQ0EsTUFBSzs7QUFFTDtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLHFCQUFvQixPQUFPLE9BQU87QUFDbEM7QUFDQSxNQUFLOztBQUVMO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsTUFBSztBQUNMO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixxQkFBcUI7QUFDdkM7QUFDQSxNQUFLO0FBQ0w7QUFDQSxrQkFBaUIscUJBQXFCO0FBQ3RDLG1CQUFrQixxQkFBcUI7QUFDdkM7QUFDQTtBQUNBLE1BQUs7QUFDTDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQTtBQUNBOztBQUVBO0FBQ0EsNkNBQTRDLGdCQUFnQjtBQUM1RDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSzs7QUFFTDtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLHdDQUF1QztBQUN2QztBQUNBOztBQUVBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0Esd0NBQXVDO0FBQ3ZDO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOzs7Ozs7O0FDaG1DQSxpQkFBZ0Isb0JBQW9CO0FBQ3BDO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSw0QkFBMkI7QUFDM0IsNEJBQTJCO0FBQzNCLHFEQUFvRCxnQkFBZ0I7QUFDcEUscURBQW9ELGFBQWE7QUFDakU7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsdURBQXNEO0FBQ3REO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLHVEQUFzRDtBQUN0RDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLHVEQUFzRDtBQUN0RDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLFVBQVM7QUFDVDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSx5Q0FBd0M7QUFDeEMsaUNBQWdDO0FBQ2hDLGlCQUFnQjtBQUNoQjtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxRQUFPO0FBQ1A7QUFDQTtBQUNBO0FBQ0E7QUFDQSxVQUFTO0FBQ1Q7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsdUNBQXNDO0FBQ3RDLDhCQUE2QjtBQUM3QixpQkFBZ0I7QUFDaEI7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxVQUFTO0FBQ1Q7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EseUNBQXdDO0FBQ3hDLGlDQUFnQztBQUNoQyxpQkFBZ0I7QUFDaEI7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsVUFBUztBQUNUO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLHVDQUFzQztBQUN0Qyw4QkFBNkI7QUFDN0IsaUJBQWdCO0FBQ2hCO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxtQ0FBa0M7QUFDbEMsMkJBQTBCO0FBQzFCLFdBQVU7QUFDVixpQ0FBZ0M7QUFDaEMsd0JBQXVCO0FBQ3ZCLFdBQVU7QUFDVjtBQUNBO0FBQ0EsdURBQXNEO0FBQ3REO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsbUNBQWtDO0FBQ2xDLDJCQUEwQjtBQUMxQixXQUFVO0FBQ1YsaUNBQWdDO0FBQ2hDLHdCQUF1QjtBQUN2QixXQUFVO0FBQ1Y7QUFDQTtBQUNBLHVEQUFzRDtBQUN0RDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOzs7QUFHQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTs7QUFFQTtBQUNBO0FBQ0EsUUFBTztBQUNQO0FBQ0E7QUFDQTtBQUNBLFFBQU87QUFDUDtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLFFBQU87QUFDUDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxvQkFBbUIsNEJBQTRCO0FBQy9DO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLG9CQUFtQiw4QkFBOEI7QUFDakQ7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0Esc0JBQXFCLHFDQUFxQztBQUMxRDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOzs7Ozs7O0FDdlNBLGlCQUFnQixvQkFBb0I7QUFDcEM7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQSxNQUFLO0FBQ0w7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQSxpREFBZ0QsUUFBUTtBQUN4RDtBQUNBO0FBQ0E7QUFDQSxRQUFPO0FBQ1A7QUFDQSxRQUFPO0FBQ1A7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsVUFBUztBQUNUO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7Ozs7Ozs7QUNoWEEsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLHlEQUF3RDtBQUN4RDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBLElBQUc7O0FBRUg7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQSxJQUFHOztBQUVIO0FBQ0E7QUFDQTtBQUNBLHNCQUFxQjtBQUNyQjs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsY0FBYTs7QUFFYjtBQUNBO0FBQ0EsVUFBUztBQUNUOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxjQUFhOztBQUViO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSw4QkFBNkIsTUFBTTtBQUNuQztBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLHlEQUF3RDtBQUN4RDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLFFBQU87O0FBRVA7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBLHlEQUF3RCxZQUFZO0FBQ3BFO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLFFBQU87QUFDUDtBQUNBLElBQUc7O0FBRUg7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0Esc0NBQXFDO0FBQ3JDO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSw0QkFBMkIsY0FBYztBQUN6QztBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLFlBQVc7QUFDWDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsMEJBQXlCLHdDQUF3QztBQUNqRTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0Esa0RBQWlELG1CQUFtQixFQUFFO0FBQ3RFOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLG9CQUFtQixvQkFBb0I7QUFDdkM7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLGdDQUErQixNQUFNO0FBQ3JDO0FBQ0EsVUFBUztBQUNUO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EseURBQXdEO0FBQ3hEOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxVQUFTO0FBQ1Q7QUFDQTtBQUNBLE1BQUs7QUFDTDs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLHNCQUFxQiwyQkFBMkI7QUFDaEQsd0JBQXVCLCtDQUErQztBQUN0RTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsSUFBRzs7QUFFSDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0EsVUFBUztBQUNUOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLFFBQU87QUFDUDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0Esc0JBQXFCLDJCQUEyQjtBQUNoRDs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxzQkFBcUIsMkJBQTJCO0FBQ2hEOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLHNCQUFxQiwyQkFBMkI7QUFDaEQ7QUFDQTtBQUNBLHdCQUF1Qiw0QkFBNEI7QUFDbkQ7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7Ozs7Ozs7QUN6akNBLGlCQUFnQixvQkFBb0I7QUFDcEM7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQSxRQUFPO0FBQ1A7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOzs7Ozs7O0FDL0dBLGlCQUFnQixvQkFBb0I7QUFDcEM7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLHlDQUF3QyxTQUFTO0FBQ2pEO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOzs7Ozs7O0FDdkdBLGlCQUFnQixvQkFBb0I7QUFDcEM7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLDREQUEyRDtBQUMzRCxxQkFBb0I7QUFDcEI7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsTUFBSzs7QUFFTDtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7O0FBRUw7QUFDQTtBQUNBO0FBQ0E7Ozs7Ozs7QUM1SUEsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLG1CQUFrQjtBQUNsQixtQkFBa0I7O0FBRWxCLHNCQUFxQjtBQUNyQix1QkFBc0I7O0FBRXRCLG1CQUFrQjtBQUNsQixtQkFBa0I7O0FBRWxCLG1CQUFrQjtBQUNsQixvQkFBbUI7O0FBRW5CO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7Ozs7Ozs7QUNuRUEsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0EsY0FBYSxNQUFNO0FBQ25CO0FBQ0EsY0FBYSxPQUFPO0FBQ3BCO0FBQ0EsY0FBYSxPQUFPO0FBQ3BCO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLGNBQWEsT0FBTztBQUNwQjtBQUNBLGNBQWEsT0FBTztBQUNwQjtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLGNBQWEsTUFBTTtBQUNuQjtBQUNBLGNBQWEsU0FBUztBQUN0QjtBQUNBLGNBQWEsT0FBTztBQUNwQjtBQUNBLGNBQWEsT0FBTztBQUNwQjtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0Esc0JBQXFCLE9BQU87QUFDNUI7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLGNBQWEsTUFBTTtBQUNuQjtBQUNBLGNBQWEsU0FBUztBQUN0QjtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7Ozs7Ozs7QUNsSEEsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxRQUFPO0FBQ1A7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBLFFBQU87QUFDUDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxRQUFPO0FBQ1A7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLFlBQVc7QUFDWDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQSxRQUFPO0FBQ1A7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxRQUFPO0FBQ1A7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxVQUFTO0FBQ1Q7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBLDZDQUE0QyxTQUFTO0FBQ3JEOztBQUVBO0FBQ0E7QUFDQTtBQUNBLHlCQUF3QjtBQUN4QjtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOzs7Ozs7O0FDM1lBLGlCQUFnQixvQkFBb0I7QUFDcEM7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsbUJBQWtCO0FBQ2xCOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBIiwiZmlsZSI6InRlc3Rfc291cmNlX21hcF9jb25zdW1lci5qcyIsInNvdXJjZXNDb250ZW50IjpbIiBcdC8vIFRoZSBtb2R1bGUgY2FjaGVcbiBcdHZhciBpbnN0YWxsZWRNb2R1bGVzID0ge307XG5cbiBcdC8vIFRoZSByZXF1aXJlIGZ1bmN0aW9uXG4gXHRmdW5jdGlvbiBfX3dlYnBhY2tfcmVxdWlyZV9fKG1vZHVsZUlkKSB7XG5cbiBcdFx0Ly8gQ2hlY2sgaWYgbW9kdWxlIGlzIGluIGNhY2hlXG4gXHRcdGlmKGluc3RhbGxlZE1vZHVsZXNbbW9kdWxlSWRdKVxuIFx0XHRcdHJldHVybiBpbnN0YWxsZWRNb2R1bGVzW21vZHVsZUlkXS5leHBvcnRzO1xuXG4gXHRcdC8vIENyZWF0ZSBhIG5ldyBtb2R1bGUgKGFuZCBwdXQgaXQgaW50byB0aGUgY2FjaGUpXG4gXHRcdHZhciBtb2R1bGUgPSBpbnN0YWxsZWRNb2R1bGVzW21vZHVsZUlkXSA9IHtcbiBcdFx0XHRleHBvcnRzOiB7fSxcbiBcdFx0XHRpZDogbW9kdWxlSWQsXG4gXHRcdFx0bG9hZGVkOiBmYWxzZVxuIFx0XHR9O1xuXG4gXHRcdC8vIEV4ZWN1dGUgdGhlIG1vZHVsZSBmdW5jdGlvblxuIFx0XHRtb2R1bGVzW21vZHVsZUlkXS5jYWxsKG1vZHVsZS5leHBvcnRzLCBtb2R1bGUsIG1vZHVsZS5leHBvcnRzLCBfX3dlYnBhY2tfcmVxdWlyZV9fKTtcblxuIFx0XHQvLyBGbGFnIHRoZSBtb2R1bGUgYXMgbG9hZGVkXG4gXHRcdG1vZHVsZS5sb2FkZWQgPSB0cnVlO1xuXG4gXHRcdC8vIFJldHVybiB0aGUgZXhwb3J0cyBvZiB0aGUgbW9kdWxlXG4gXHRcdHJldHVybiBtb2R1bGUuZXhwb3J0cztcbiBcdH1cblxuXG4gXHQvLyBleHBvc2UgdGhlIG1vZHVsZXMgb2JqZWN0IChfX3dlYnBhY2tfbW9kdWxlc19fKVxuIFx0X193ZWJwYWNrX3JlcXVpcmVfXy5tID0gbW9kdWxlcztcblxuIFx0Ly8gZXhwb3NlIHRoZSBtb2R1bGUgY2FjaGVcbiBcdF9fd2VicGFja19yZXF1aXJlX18uYyA9IGluc3RhbGxlZE1vZHVsZXM7XG5cbiBcdC8vIF9fd2VicGFja19wdWJsaWNfcGF0aF9fXG4gXHRfX3dlYnBhY2tfcmVxdWlyZV9fLnAgPSBcIlwiO1xuXG4gXHQvLyBMb2FkIGVudHJ5IG1vZHVsZSBhbmQgcmV0dXJuIGV4cG9ydHNcbiBcdHJldHVybiBfX3dlYnBhY2tfcmVxdWlyZV9fKDApO1xuXG5cblxuLyoqIFdFQlBBQ0sgRk9PVEVSICoqXG4gKiogd2VicGFjay9ib290c3RyYXAgZGExYmM2OWM5ZmUyOGIwN2RiNzJcbiAqKi8iLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG57XG4gIHZhciB1dGlsID0gcmVxdWlyZShcIi4vdXRpbFwiKTtcbiAgdmFyIFNvdXJjZU1hcENvbnN1bWVyID0gcmVxdWlyZSgnLi4vbGliL3NvdXJjZS1tYXAtY29uc3VtZXInKS5Tb3VyY2VNYXBDb25zdW1lcjtcbiAgdmFyIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lciA9IHJlcXVpcmUoJy4uL2xpYi9zb3VyY2UtbWFwLWNvbnN1bWVyJykuSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyO1xuICB2YXIgQmFzaWNTb3VyY2VNYXBDb25zdW1lciA9IHJlcXVpcmUoJy4uL2xpYi9zb3VyY2UtbWFwLWNvbnN1bWVyJykuQmFzaWNTb3VyY2VNYXBDb25zdW1lcjtcbiAgdmFyIFNvdXJjZU1hcEdlbmVyYXRvciA9IHJlcXVpcmUoJy4uL2xpYi9zb3VyY2UtbWFwLWdlbmVyYXRvcicpLlNvdXJjZU1hcEdlbmVyYXRvcjtcblxuICBleHBvcnRzWyd0ZXN0IHRoYXQgd2UgY2FuIGluc3RhbnRpYXRlIHdpdGggYSBzdHJpbmcgb3IgYW4gb2JqZWN0J10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgYXNzZXJ0LmRvZXNOb3RUaHJvdyhmdW5jdGlvbiAoKSB7XG4gICAgICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcCk7XG4gICAgfSk7XG4gICAgYXNzZXJ0LmRvZXNOb3RUaHJvdyhmdW5jdGlvbiAoKSB7XG4gICAgICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKEpTT04uc3RyaW5naWZ5KHV0aWwudGVzdE1hcCkpO1xuICAgIH0pO1xuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3QgdGhhdCB0aGUgb2JqZWN0IHJldHVybmVkIGZyb20gbmV3IFNvdXJjZU1hcENvbnN1bWVyIGluaGVyaXRzIGZyb20gU291cmNlTWFwQ29uc3VtZXInXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICBhc3NlcnQub2sobmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcCkgaW5zdGFuY2VvZiBTb3VyY2VNYXBDb25zdW1lcik7XG4gIH1cblxuICBleHBvcnRzWyd0ZXN0IHRoYXQgYSBCYXNpY1NvdXJjZU1hcENvbnN1bWVyIGlzIHJldHVybmVkIGZvciBzb3VyY2VtYXBzIHdpdGhvdXQgc2VjdGlvbnMnXSA9IGZ1bmN0aW9uKGFzc2VydCkge1xuICAgIGFzc2VydC5vayhuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC50ZXN0TWFwKSBpbnN0YW5jZW9mIEJhc2ljU291cmNlTWFwQ29uc3VtZXIpO1xuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3QgdGhhdCBhbiBJbmRleGVkU291cmNlTWFwQ29uc3VtZXIgaXMgcmV0dXJuZWQgZm9yIHNvdXJjZW1hcHMgd2l0aCBzZWN0aW9ucyddID0gZnVuY3Rpb24oYXNzZXJ0KSB7XG4gICAgYXNzZXJ0Lm9rKG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLmluZGV4ZWRUZXN0TWFwKSBpbnN0YW5jZW9mIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lcik7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCB0aGF0IHRoZSBgc291cmNlc2AgZmllbGQgaGFzIHRoZSBvcmlnaW5hbCBzb3VyY2VzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG1hcDtcbiAgICB2YXIgc291cmNlcztcblxuICAgIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLnRlc3RNYXApO1xuICAgIHNvdXJjZXMgPSBtYXAuc291cmNlcztcbiAgICBhc3NlcnQuZXF1YWwoc291cmNlc1swXSwgJy90aGUvcm9vdC9vbmUuanMnKTtcbiAgICBhc3NlcnQuZXF1YWwoc291cmNlc1sxXSwgJy90aGUvcm9vdC90d28uanMnKTtcbiAgICBhc3NlcnQuZXF1YWwoc291cmNlcy5sZW5ndGgsIDIpO1xuXG4gICAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwuaW5kZXhlZFRlc3RNYXApO1xuICAgIHNvdXJjZXMgPSBtYXAuc291cmNlcztcbiAgICBhc3NlcnQuZXF1YWwoc291cmNlc1swXSwgJy90aGUvcm9vdC9vbmUuanMnKTtcbiAgICBhc3NlcnQuZXF1YWwoc291cmNlc1sxXSwgJy90aGUvcm9vdC90d28uanMnKTtcbiAgICBhc3NlcnQuZXF1YWwoc291cmNlcy5sZW5ndGgsIDIpO1xuXG4gICAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwuaW5kZXhlZFRlc3RNYXBEaWZmZXJlbnRTb3VyY2VSb290cyk7XG4gICAgc291cmNlcyA9IG1hcC5zb3VyY2VzO1xuICAgIGFzc2VydC5lcXVhbChzb3VyY2VzWzBdLCAnL3RoZS9yb290L29uZS5qcycpO1xuICAgIGFzc2VydC5lcXVhbChzb3VyY2VzWzFdLCAnL2RpZmZlcmVudC9yb290L3R3by5qcycpO1xuICAgIGFzc2VydC5lcXVhbChzb3VyY2VzLmxlbmd0aCwgMik7XG5cbiAgICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC50ZXN0TWFwTm9Tb3VyY2VSb290KTtcbiAgICBzb3VyY2VzID0gbWFwLnNvdXJjZXM7XG4gICAgYXNzZXJ0LmVxdWFsKHNvdXJjZXNbMF0sICdvbmUuanMnKTtcbiAgICBhc3NlcnQuZXF1YWwoc291cmNlc1sxXSwgJ3R3by5qcycpO1xuICAgIGFzc2VydC5lcXVhbChzb3VyY2VzLmxlbmd0aCwgMik7XG5cbiAgICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC50ZXN0TWFwRW1wdHlTb3VyY2VSb290KTtcbiAgICBzb3VyY2VzID0gbWFwLnNvdXJjZXM7XG4gICAgYXNzZXJ0LmVxdWFsKHNvdXJjZXNbMF0sICdvbmUuanMnKTtcbiAgICBhc3NlcnQuZXF1YWwoc291cmNlc1sxXSwgJ3R3by5qcycpO1xuICAgIGFzc2VydC5lcXVhbChzb3VyY2VzLmxlbmd0aCwgMik7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCB0aGF0IHRoZSBzb3VyY2Ugcm9vdCBpcyByZWZsZWN0ZWQgaW4gYSBtYXBwaW5nXFwncyBzb3VyY2UgZmllbGQnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgbWFwO1xuICAgIHZhciBtYXBwaW5nO1xuXG4gICAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcCk7XG5cbiAgICBtYXBwaW5nID0gbWFwLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgICAgbGluZTogMixcbiAgICAgIGNvbHVtbjogMVxuICAgIH0pO1xuICAgIGFzc2VydC5lcXVhbChtYXBwaW5nLnNvdXJjZSwgJy90aGUvcm9vdC90d28uanMnKTtcblxuICAgIG1hcHBpbmcgPSBtYXAub3JpZ2luYWxQb3NpdGlvbkZvcih7XG4gICAgICBsaW5lOiAxLFxuICAgICAgY29sdW1uOiAxXG4gICAgfSk7XG4gICAgYXNzZXJ0LmVxdWFsKG1hcHBpbmcuc291cmNlLCAnL3RoZS9yb290L29uZS5qcycpO1xuXG5cbiAgICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC50ZXN0TWFwTm9Tb3VyY2VSb290KTtcblxuICAgIG1hcHBpbmcgPSBtYXAub3JpZ2luYWxQb3NpdGlvbkZvcih7XG4gICAgICBsaW5lOiAyLFxuICAgICAgY29sdW1uOiAxXG4gICAgfSk7XG4gICAgYXNzZXJ0LmVxdWFsKG1hcHBpbmcuc291cmNlLCAndHdvLmpzJyk7XG5cbiAgICBtYXBwaW5nID0gbWFwLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgICAgbGluZTogMSxcbiAgICAgIGNvbHVtbjogMVxuICAgIH0pO1xuICAgIGFzc2VydC5lcXVhbChtYXBwaW5nLnNvdXJjZSwgJ29uZS5qcycpO1xuXG5cbiAgICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC50ZXN0TWFwRW1wdHlTb3VyY2VSb290KTtcblxuICAgIG1hcHBpbmcgPSBtYXAub3JpZ2luYWxQb3NpdGlvbkZvcih7XG4gICAgICBsaW5lOiAyLFxuICAgICAgY29sdW1uOiAxXG4gICAgfSk7XG4gICAgYXNzZXJ0LmVxdWFsKG1hcHBpbmcuc291cmNlLCAndHdvLmpzJyk7XG5cbiAgICBtYXBwaW5nID0gbWFwLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgICAgbGluZTogMSxcbiAgICAgIGNvbHVtbjogMVxuICAgIH0pO1xuICAgIGFzc2VydC5lcXVhbChtYXBwaW5nLnNvdXJjZSwgJ29uZS5qcycpO1xuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3QgbWFwcGluZyB0b2tlbnMgYmFjayBleGFjdGx5J10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLnRlc3RNYXApO1xuXG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDEsICcvdGhlL3Jvb3Qvb25lLmpzJywgMSwgMSwgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygxLCA1LCAnL3RoZS9yb290L29uZS5qcycsIDEsIDUsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgOSwgJy90aGUvcm9vdC9vbmUuanMnLCAxLCAxMSwgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAxOCwgJy90aGUvcm9vdC9vbmUuanMnLCAxLCAyMSwgJ2JhcicsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMjEsICcvdGhlL3Jvb3Qvb25lLmpzJywgMiwgMywgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAyOCwgJy90aGUvcm9vdC9vbmUuanMnLCAyLCAxMCwgJ2JheicsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMzIsICcvdGhlL3Jvb3Qvb25lLmpzJywgMiwgMTQsICdiYXInLCBudWxsLCBtYXAsIGFzc2VydCk7XG5cbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMiwgMSwgJy90aGUvcm9vdC90d28uanMnLCAxLCAxLCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDUsICcvdGhlL3Jvb3QvdHdvLmpzJywgMSwgNSwgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygyLCA5LCAnL3RoZS9yb290L3R3by5qcycsIDEsIDExLCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDE4LCAnL3RoZS9yb290L3R3by5qcycsIDEsIDIxLCAnbicsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMiwgMjEsICcvdGhlL3Jvb3QvdHdvLmpzJywgMiwgMywgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygyLCAyOCwgJy90aGUvcm9vdC90d28uanMnLCAyLCAxMCwgJ24nLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBtYXBwaW5nIHRva2VucyBiYWNrIGV4YWN0bHkgaW4gaW5kZXhlZCBzb3VyY2UgbWFwJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLmluZGV4ZWRUZXN0TWFwKTtcblxuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAxLCAnL3RoZS9yb290L29uZS5qcycsIDEsIDEsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgNSwgJy90aGUvcm9vdC9vbmUuanMnLCAxLCA1LCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDksICcvdGhlL3Jvb3Qvb25lLmpzJywgMSwgMTEsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMTgsICcvdGhlL3Jvb3Qvb25lLmpzJywgMSwgMjEsICdiYXInLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDIxLCAnL3RoZS9yb290L29uZS5qcycsIDIsIDMsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMjgsICcvdGhlL3Jvb3Qvb25lLmpzJywgMiwgMTAsICdiYXonLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDMyLCAnL3RoZS9yb290L29uZS5qcycsIDIsIDE0LCAnYmFyJywgbnVsbCwgbWFwLCBhc3NlcnQpO1xuXG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDEsICcvdGhlL3Jvb3QvdHdvLmpzJywgMSwgMSwgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygyLCA1LCAnL3RoZS9yb290L3R3by5qcycsIDEsIDUsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMiwgOSwgJy90aGUvcm9vdC90d28uanMnLCAxLCAxMSwgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygyLCAxOCwgJy90aGUvcm9vdC90d28uanMnLCAxLCAyMSwgJ24nLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDIxLCAnL3RoZS9yb290L3R3by5qcycsIDIsIDMsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMiwgMjgsICcvdGhlL3Jvb3QvdHdvLmpzJywgMiwgMTAsICduJywgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICB9O1xuXG5cbiAgZXhwb3J0c1sndGVzdCBtYXBwaW5nIHRva2VucyBiYWNrIGV4YWN0bHknXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcCk7XG5cbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMSwgJy90aGUvcm9vdC9vbmUuanMnLCAxLCAxLCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDUsICcvdGhlL3Jvb3Qvb25lLmpzJywgMSwgNSwgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygxLCA5LCAnL3RoZS9yb290L29uZS5qcycsIDEsIDExLCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDE4LCAnL3RoZS9yb290L29uZS5qcycsIDEsIDIxLCAnYmFyJywgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAyMSwgJy90aGUvcm9vdC9vbmUuanMnLCAyLCAzLCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDI4LCAnL3RoZS9yb290L29uZS5qcycsIDIsIDEwLCAnYmF6JywgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAzMiwgJy90aGUvcm9vdC9vbmUuanMnLCAyLCAxNCwgJ2JhcicsIG51bGwsIG1hcCwgYXNzZXJ0KTtcblxuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygyLCAxLCAnL3RoZS9yb290L3R3by5qcycsIDEsIDEsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMiwgNSwgJy90aGUvcm9vdC90d28uanMnLCAxLCA1LCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDksICcvdGhlL3Jvb3QvdHdvLmpzJywgMSwgMTEsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMiwgMTgsICcvdGhlL3Jvb3QvdHdvLmpzJywgMSwgMjEsICduJywgbnVsbCwgbWFwLCBhc3NlcnQpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygyLCAyMSwgJy90aGUvcm9vdC90d28uanMnLCAyLCAzLCBudWxsLCBudWxsLCBtYXAsIGFzc2VydCk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDI4LCAnL3RoZS9yb290L3R3by5qcycsIDIsIDEwLCAnbicsIG51bGwsIG1hcCwgYXNzZXJ0KTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IG1hcHBpbmcgdG9rZW5zIGZ1enp5J10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLnRlc3RNYXApO1xuXG4gICAgLy8gRmluZGluZyBvcmlnaW5hbCBwb3NpdGlvbnMgd2l0aCBkZWZhdWx0IChnbGIpIGJpYXMuXG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDIwLCAnL3RoZS9yb290L29uZS5qcycsIDEsIDIxLCAnYmFyJywgbnVsbCwgbWFwLCBhc3NlcnQsIHRydWUpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAzMCwgJy90aGUvcm9vdC9vbmUuanMnLCAyLCAxMCwgJ2JheicsIG51bGwsIG1hcCwgYXNzZXJ0LCB0cnVlKTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMiwgMTIsICcvdGhlL3Jvb3QvdHdvLmpzJywgMSwgMTEsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0LCB0cnVlKTtcblxuICAgIC8vIEZpbmRpbmcgb3JpZ2luYWwgcG9zaXRpb25zIHdpdGggbHViIGJpYXMuXG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDE2LCAnL3RoZS9yb290L29uZS5qcycsIDEsIDIxLCAnYmFyJywgU291cmNlTWFwQ29uc3VtZXIuTEVBU1RfVVBQRVJfQk9VTkQsIG1hcCwgYXNzZXJ0LCB0cnVlKTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMjYsICcvdGhlL3Jvb3Qvb25lLmpzJywgMiwgMTAsICdiYXonLCBTb3VyY2VNYXBDb25zdW1lci5MRUFTVF9VUFBFUl9CT1VORCwgbWFwLCBhc3NlcnQsIHRydWUpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygyLCA2LCAnL3RoZS9yb290L3R3by5qcycsIDEsIDExLCBudWxsLCBTb3VyY2VNYXBDb25zdW1lci5MRUFTVF9VUFBFUl9CT1VORCwgbWFwLCBhc3NlcnQsIHRydWUpO1xuXG4gICAgLy8gRmluZGluZyBnZW5lcmF0ZWQgcG9zaXRpb25zIHdpdGggZGVmYXVsdCAoZ2xiKSBiaWFzLlxuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAxOCwgJy90aGUvcm9vdC9vbmUuanMnLCAxLCAyMiwgJ2JhcicsIG51bGwsIG1hcCwgYXNzZXJ0LCBudWxsLCB0cnVlKTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMjgsICcvdGhlL3Jvb3Qvb25lLmpzJywgMiwgMTMsICdiYXonLCBudWxsLCBtYXAsIGFzc2VydCwgbnVsbCwgdHJ1ZSk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDksICcvdGhlL3Jvb3QvdHdvLmpzJywgMSwgMTYsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0LCBudWxsLCB0cnVlKTtcblxuICAgIC8vIEZpbmRpbmcgZ2VuZXJhdGVkIHBvc2l0aW9ucyB3aXRoIGx1YiBiaWFzLlxuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAxOCwgJy90aGUvcm9vdC9vbmUuanMnLCAxLCAyMCwgJ2JhcicsIFNvdXJjZU1hcENvbnN1bWVyLkxFQVNUX1VQUEVSX0JPVU5ELCBtYXAsIGFzc2VydCwgbnVsbCwgdHJ1ZSk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDI4LCAnL3RoZS9yb290L29uZS5qcycsIDIsIDcsICdiYXonLCBTb3VyY2VNYXBDb25zdW1lci5MRUFTVF9VUFBFUl9CT1VORCwgbWFwLCBhc3NlcnQsIG51bGwsIHRydWUpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygyLCA5LCAnL3RoZS9yb290L3R3by5qcycsIDEsIDYsIG51bGwsIFNvdXJjZU1hcENvbnN1bWVyLkxFQVNUX1VQUEVSX0JPVU5ELCBtYXAsIGFzc2VydCwgbnVsbCwgdHJ1ZSk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBtYXBwaW5nIHRva2VucyBmdXp6eSBpbiBpbmRleGVkIHNvdXJjZSBtYXAnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwuaW5kZXhlZFRlc3RNYXApO1xuXG4gICAgLy8gRmluZGluZyBvcmlnaW5hbCBwb3NpdGlvbnMgd2l0aCBkZWZhdWx0IChnbGIpIGJpYXMuXG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDIwLCAnL3RoZS9yb290L29uZS5qcycsIDEsIDIxLCAnYmFyJywgbnVsbCwgbWFwLCBhc3NlcnQsIHRydWUpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAzMCwgJy90aGUvcm9vdC9vbmUuanMnLCAyLCAxMCwgJ2JheicsIG51bGwsIG1hcCwgYXNzZXJ0LCB0cnVlKTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMiwgMTIsICcvdGhlL3Jvb3QvdHdvLmpzJywgMSwgMTEsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0LCB0cnVlKTtcblxuICAgIC8vIEZpbmRpbmcgb3JpZ2luYWwgcG9zaXRpb25zIHdpdGggbHViIGJpYXMuXG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDE2LCAnL3RoZS9yb290L29uZS5qcycsIDEsIDIxLCAnYmFyJywgU291cmNlTWFwQ29uc3VtZXIuTEVBU1RfVVBQRVJfQk9VTkQsIG1hcCwgYXNzZXJ0LCB0cnVlKTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMjYsICcvdGhlL3Jvb3Qvb25lLmpzJywgMiwgMTAsICdiYXonLCBTb3VyY2VNYXBDb25zdW1lci5MRUFTVF9VUFBFUl9CT1VORCwgbWFwLCBhc3NlcnQsIHRydWUpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygyLCA2LCAnL3RoZS9yb290L3R3by5qcycsIDEsIDExLCBudWxsLCBTb3VyY2VNYXBDb25zdW1lci5MRUFTVF9VUFBFUl9CT1VORCwgbWFwLCBhc3NlcnQsIHRydWUpO1xuXG4gICAgLy8gRmluZGluZyBnZW5lcmF0ZWQgcG9zaXRpb25zIHdpdGggZGVmYXVsdCAoZ2xiKSBiaWFzLlxuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAxOCwgJy90aGUvcm9vdC9vbmUuanMnLCAxLCAyMiwgJ2JhcicsIG51bGwsIG1hcCwgYXNzZXJ0LCBudWxsLCB0cnVlKTtcbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMSwgMjgsICcvdGhlL3Jvb3Qvb25lLmpzJywgMiwgMTMsICdiYXonLCBudWxsLCBtYXAsIGFzc2VydCwgbnVsbCwgdHJ1ZSk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDIsIDksICcvdGhlL3Jvb3QvdHdvLmpzJywgMSwgMTYsIG51bGwsIG51bGwsIG1hcCwgYXNzZXJ0LCBudWxsLCB0cnVlKTtcblxuICAgIC8vIEZpbmRpbmcgZ2VuZXJhdGVkIHBvc2l0aW9ucyB3aXRoIGx1YiBiaWFzLlxuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAxOCwgJy90aGUvcm9vdC9vbmUuanMnLCAxLCAyMCwgJ2JhcicsIFNvdXJjZU1hcENvbnN1bWVyLkxFQVNUX1VQUEVSX0JPVU5ELCBtYXAsIGFzc2VydCwgbnVsbCwgdHJ1ZSk7XG4gICAgdXRpbC5hc3NlcnRNYXBwaW5nKDEsIDI4LCAnL3RoZS9yb290L29uZS5qcycsIDIsIDcsICdiYXonLCBTb3VyY2VNYXBDb25zdW1lci5MRUFTVF9VUFBFUl9CT1VORCwgbWFwLCBhc3NlcnQsIG51bGwsIHRydWUpO1xuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygyLCA5LCAnL3RoZS9yb290L3R3by5qcycsIDEsIDYsIG51bGwsIFNvdXJjZU1hcENvbnN1bWVyLkxFQVNUX1VQUEVSX0JPVU5ELCBtYXAsIGFzc2VydCwgbnVsbCwgdHJ1ZSk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBtYXBwaW5ncyBhbmQgZW5kIG9mIGxpbmVzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIHNtZyA9IG5ldyBTb3VyY2VNYXBHZW5lcmF0b3Ioe1xuICAgICAgZmlsZTogJ2Zvby5qcydcbiAgICB9KTtcbiAgICBzbWcuYWRkTWFwcGluZyh7XG4gICAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICAgIHNvdXJjZTogJ2Jhci5qcydcbiAgICB9KTtcbiAgICBzbWcuYWRkTWFwcGluZyh7XG4gICAgICBvcmlnaW5hbDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICAgIHNvdXJjZTogJ2Jhci5qcydcbiAgICB9KTtcbiAgICBzbWcuYWRkTWFwcGluZyh7XG4gICAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICAgIHNvdXJjZTogJ2Jhei5qcydcbiAgICB9KTtcblxuICAgIHZhciBtYXAgPSBTb3VyY2VNYXBDb25zdW1lci5mcm9tU291cmNlTWFwKHNtZyk7XG5cbiAgICAvLyBXaGVuIGZpbmRpbmcgb3JpZ2luYWwgcG9zaXRpb25zLCBtYXBwaW5ncyBlbmQgYXQgdGhlIGVuZCBvZiB0aGUgbGluZS5cbiAgICB1dGlsLmFzc2VydE1hcHBpbmcoMiwgMSwgbnVsbCwgbnVsbCwgbnVsbCwgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQsIHRydWUpXG5cbiAgICAvLyBXaGVuIGZpbmRpbmcgZ2VuZXJhdGVkIHBvc2l0aW9ucywgbWFwcGluZ3MgZG8gbm90IGVuZCBhdCB0aGUgZW5kIG9mIHRoZSBsaW5lLlxuICAgIHV0aWwuYXNzZXJ0TWFwcGluZygxLCAxLCAnYmFyLmpzJywgMiwgMSwgbnVsbCwgbnVsbCwgbWFwLCBhc3NlcnQsIG51bGwsIHRydWUpO1xuXG4gICAgLy8gV2hlbiBmaW5kaW5nIGdlbmVyYXRlZCBwb3NpdGlvbnMgd2l0aCwgbWFwcGluZ3MgZW5kIGF0IHRoZSBlbmQgb2YgdGhlIHNvdXJjZS5cbiAgICB1dGlsLmFzc2VydE1hcHBpbmcobnVsbCwgbnVsbCwgJ2Jhci5qcycsIDMsIDEsIG51bGwsIFNvdXJjZU1hcENvbnN1bWVyLkxFQVNUX1VQUEVSX0JPVU5ELCBtYXAsIGFzc2VydCwgbnVsbCwgdHJ1ZSk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBjcmVhdGluZyBzb3VyY2UgbWFwIGNvbnN1bWVycyB3aXRoICldfVxcJyBwcmVmaXgnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICBhc3NlcnQuZG9lc05vdFRocm93KGZ1bmN0aW9uICgpIHtcbiAgICAgIHZhciBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIoXCIpXX0nXCIgKyBKU09OLnN0cmluZ2lmeSh1dGlsLnRlc3RNYXApKTtcbiAgICB9KTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IGVhY2hNYXBwaW5nJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG1hcDtcblxuICAgIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLnRlc3RNYXApO1xuICAgIHZhciBwcmV2aW91c0xpbmUgPSAtSW5maW5pdHk7XG4gICAgdmFyIHByZXZpb3VzQ29sdW1uID0gLUluZmluaXR5O1xuICAgIG1hcC5lYWNoTWFwcGluZyhmdW5jdGlvbiAobWFwcGluZykge1xuICAgICAgYXNzZXJ0Lm9rKG1hcHBpbmcuZ2VuZXJhdGVkTGluZSA+PSBwcmV2aW91c0xpbmUpO1xuXG4gICAgICBhc3NlcnQub2sobWFwcGluZy5zb3VyY2UgPT09ICcvdGhlL3Jvb3Qvb25lLmpzJyB8fCBtYXBwaW5nLnNvdXJjZSA9PT0gJy90aGUvcm9vdC90d28uanMnKTtcblxuICAgICAgaWYgKG1hcHBpbmcuZ2VuZXJhdGVkTGluZSA9PT0gcHJldmlvdXNMaW5lKSB7XG4gICAgICAgIGFzc2VydC5vayhtYXBwaW5nLmdlbmVyYXRlZENvbHVtbiA+PSBwcmV2aW91c0NvbHVtbik7XG4gICAgICAgIHByZXZpb3VzQ29sdW1uID0gbWFwcGluZy5nZW5lcmF0ZWRDb2x1bW47XG4gICAgICB9XG4gICAgICBlbHNlIHtcbiAgICAgICAgcHJldmlvdXNMaW5lID0gbWFwcGluZy5nZW5lcmF0ZWRMaW5lO1xuICAgICAgICBwcmV2aW91c0NvbHVtbiA9IC1JbmZpbml0eTtcbiAgICAgIH1cbiAgICB9KTtcblxuICAgIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLnRlc3RNYXBOb1NvdXJjZVJvb3QpO1xuICAgIG1hcC5lYWNoTWFwcGluZyhmdW5jdGlvbiAobWFwcGluZykge1xuICAgICAgYXNzZXJ0Lm9rKG1hcHBpbmcuc291cmNlID09PSAnb25lLmpzJyB8fCBtYXBwaW5nLnNvdXJjZSA9PT0gJ3R3by5qcycpO1xuICAgIH0pO1xuXG4gICAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcEVtcHR5U291cmNlUm9vdCk7XG4gICAgbWFwLmVhY2hNYXBwaW5nKGZ1bmN0aW9uIChtYXBwaW5nKSB7XG4gICAgICBhc3NlcnQub2sobWFwcGluZy5zb3VyY2UgPT09ICdvbmUuanMnIHx8IG1hcHBpbmcuc291cmNlID09PSAndHdvLmpzJyk7XG4gICAgfSk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBlYWNoTWFwcGluZyBmb3IgaW5kZXhlZCBzb3VyY2UgbWFwcyddID0gZnVuY3Rpb24oYXNzZXJ0KSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLmluZGV4ZWRUZXN0TWFwKTtcbiAgICB2YXIgcHJldmlvdXNMaW5lID0gLUluZmluaXR5O1xuICAgIHZhciBwcmV2aW91c0NvbHVtbiA9IC1JbmZpbml0eTtcbiAgICBtYXAuZWFjaE1hcHBpbmcoZnVuY3Rpb24gKG1hcHBpbmcpIHtcbiAgICAgIGFzc2VydC5vayhtYXBwaW5nLmdlbmVyYXRlZExpbmUgPj0gcHJldmlvdXNMaW5lKTtcblxuICAgICAgaWYgKG1hcHBpbmcuc291cmNlKSB7XG4gICAgICAgIGFzc2VydC5lcXVhbChtYXBwaW5nLnNvdXJjZS5pbmRleE9mKHV0aWwudGVzdE1hcC5zb3VyY2VSb290KSwgMCk7XG4gICAgICB9XG5cbiAgICAgIGlmIChtYXBwaW5nLmdlbmVyYXRlZExpbmUgPT09IHByZXZpb3VzTGluZSkge1xuICAgICAgICBhc3NlcnQub2sobWFwcGluZy5nZW5lcmF0ZWRDb2x1bW4gPj0gcHJldmlvdXNDb2x1bW4pO1xuICAgICAgICBwcmV2aW91c0NvbHVtbiA9IG1hcHBpbmcuZ2VuZXJhdGVkQ29sdW1uO1xuICAgICAgfVxuICAgICAgZWxzZSB7XG4gICAgICAgIHByZXZpb3VzTGluZSA9IG1hcHBpbmcuZ2VuZXJhdGVkTGluZTtcbiAgICAgICAgcHJldmlvdXNDb2x1bW4gPSAtSW5maW5pdHk7XG4gICAgICB9XG4gICAgfSk7XG4gIH07XG5cblxuICBleHBvcnRzWyd0ZXN0IGl0ZXJhdGluZyBvdmVyIG1hcHBpbmdzIGluIGEgZGlmZmVyZW50IG9yZGVyJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLnRlc3RNYXApO1xuICAgIHZhciBwcmV2aW91c0xpbmUgPSAtSW5maW5pdHk7XG4gICAgdmFyIHByZXZpb3VzQ29sdW1uID0gLUluZmluaXR5O1xuICAgIHZhciBwcmV2aW91c1NvdXJjZSA9IFwiXCI7XG4gICAgbWFwLmVhY2hNYXBwaW5nKGZ1bmN0aW9uIChtYXBwaW5nKSB7XG4gICAgICBhc3NlcnQub2sobWFwcGluZy5zb3VyY2UgPj0gcHJldmlvdXNTb3VyY2UpO1xuXG4gICAgICBpZiAobWFwcGluZy5zb3VyY2UgPT09IHByZXZpb3VzU291cmNlKSB7XG4gICAgICAgIGFzc2VydC5vayhtYXBwaW5nLm9yaWdpbmFsTGluZSA+PSBwcmV2aW91c0xpbmUpO1xuXG4gICAgICAgIGlmIChtYXBwaW5nLm9yaWdpbmFsTGluZSA9PT0gcHJldmlvdXNMaW5lKSB7XG4gICAgICAgICAgYXNzZXJ0Lm9rKG1hcHBpbmcub3JpZ2luYWxDb2x1bW4gPj0gcHJldmlvdXNDb2x1bW4pO1xuICAgICAgICAgIHByZXZpb3VzQ29sdW1uID0gbWFwcGluZy5vcmlnaW5hbENvbHVtbjtcbiAgICAgICAgfVxuICAgICAgICBlbHNlIHtcbiAgICAgICAgICBwcmV2aW91c0xpbmUgPSBtYXBwaW5nLm9yaWdpbmFsTGluZTtcbiAgICAgICAgICBwcmV2aW91c0NvbHVtbiA9IC1JbmZpbml0eTtcbiAgICAgICAgfVxuICAgICAgfVxuICAgICAgZWxzZSB7XG4gICAgICAgIHByZXZpb3VzU291cmNlID0gbWFwcGluZy5zb3VyY2U7XG4gICAgICAgIHByZXZpb3VzTGluZSA9IC1JbmZpbml0eTtcbiAgICAgICAgcHJldmlvdXNDb2x1bW4gPSAtSW5maW5pdHk7XG4gICAgICB9XG4gICAgfSwgbnVsbCwgU291cmNlTWFwQ29uc3VtZXIuT1JJR0lOQUxfT1JERVIpO1xuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3QgaXRlcmF0aW5nIG92ZXIgbWFwcGluZ3MgaW4gYSBkaWZmZXJlbnQgb3JkZXIgaW4gaW5kZXhlZCBzb3VyY2UgbWFwcyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC5pbmRleGVkVGVzdE1hcCk7XG4gICAgdmFyIHByZXZpb3VzTGluZSA9IC1JbmZpbml0eTtcbiAgICB2YXIgcHJldmlvdXNDb2x1bW4gPSAtSW5maW5pdHk7XG4gICAgdmFyIHByZXZpb3VzU291cmNlID0gXCJcIjtcbiAgICBtYXAuZWFjaE1hcHBpbmcoZnVuY3Rpb24gKG1hcHBpbmcpIHtcbiAgICAgIGFzc2VydC5vayhtYXBwaW5nLnNvdXJjZSA+PSBwcmV2aW91c1NvdXJjZSk7XG5cbiAgICAgIGlmIChtYXBwaW5nLnNvdXJjZSA9PT0gcHJldmlvdXNTb3VyY2UpIHtcbiAgICAgICAgYXNzZXJ0Lm9rKG1hcHBpbmcub3JpZ2luYWxMaW5lID49IHByZXZpb3VzTGluZSk7XG5cbiAgICAgICAgaWYgKG1hcHBpbmcub3JpZ2luYWxMaW5lID09PSBwcmV2aW91c0xpbmUpIHtcbiAgICAgICAgICBhc3NlcnQub2sobWFwcGluZy5vcmlnaW5hbENvbHVtbiA+PSBwcmV2aW91c0NvbHVtbik7XG4gICAgICAgICAgcHJldmlvdXNDb2x1bW4gPSBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uO1xuICAgICAgICB9XG4gICAgICAgIGVsc2Uge1xuICAgICAgICAgIHByZXZpb3VzTGluZSA9IG1hcHBpbmcub3JpZ2luYWxMaW5lO1xuICAgICAgICAgIHByZXZpb3VzQ29sdW1uID0gLUluZmluaXR5O1xuICAgICAgICB9XG4gICAgICB9XG4gICAgICBlbHNlIHtcbiAgICAgICAgcHJldmlvdXNTb3VyY2UgPSBtYXBwaW5nLnNvdXJjZTtcbiAgICAgICAgcHJldmlvdXNMaW5lID0gLUluZmluaXR5O1xuICAgICAgICBwcmV2aW91c0NvbHVtbiA9IC1JbmZpbml0eTtcbiAgICAgIH1cbiAgICB9LCBudWxsLCBTb3VyY2VNYXBDb25zdW1lci5PUklHSU5BTF9PUkRFUik7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCB0aGF0IHdlIGNhbiBzZXQgdGhlIGNvbnRleHQgZm9yIGB0aGlzYCBpbiBlYWNoTWFwcGluZyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC50ZXN0TWFwKTtcbiAgICB2YXIgY29udGV4dCA9IHt9O1xuICAgIG1hcC5lYWNoTWFwcGluZyhmdW5jdGlvbiAoKSB7XG4gICAgICBhc3NlcnQuZXF1YWwodGhpcywgY29udGV4dCk7XG4gICAgfSwgY29udGV4dCk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCB0aGF0IHdlIGNhbiBzZXQgdGhlIGNvbnRleHQgZm9yIGB0aGlzYCBpbiBlYWNoTWFwcGluZyBpbiBpbmRleGVkIHNvdXJjZSBtYXBzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLmluZGV4ZWRUZXN0TWFwKTtcbiAgICB2YXIgY29udGV4dCA9IHt9O1xuICAgIG1hcC5lYWNoTWFwcGluZyhmdW5jdGlvbiAoKSB7XG4gICAgICBhc3NlcnQuZXF1YWwodGhpcywgY29udGV4dCk7XG4gICAgfSwgY29udGV4dCk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCB0aGF0IHRoZSBgc291cmNlc0NvbnRlbnRgIGZpZWxkIGhhcyB0aGUgb3JpZ2luYWwgc291cmNlcyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC50ZXN0TWFwV2l0aFNvdXJjZXNDb250ZW50KTtcbiAgICB2YXIgc291cmNlc0NvbnRlbnQgPSBtYXAuc291cmNlc0NvbnRlbnQ7XG5cbiAgICBhc3NlcnQuZXF1YWwoc291cmNlc0NvbnRlbnRbMF0sICcgT05FLmZvbyA9IGZ1bmN0aW9uIChiYXIpIHtcXG4gICByZXR1cm4gYmF6KGJhcik7XFxuIH07Jyk7XG4gICAgYXNzZXJ0LmVxdWFsKHNvdXJjZXNDb250ZW50WzFdLCAnIFRXTy5pbmMgPSBmdW5jdGlvbiAobikge1xcbiAgIHJldHVybiBuICsgMTtcXG4gfTsnKTtcbiAgICBhc3NlcnQuZXF1YWwoc291cmNlc0NvbnRlbnQubGVuZ3RoLCAyKTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IHRoYXQgd2UgY2FuIGdldCB0aGUgb3JpZ2luYWwgc291cmNlcyBmb3IgdGhlIHNvdXJjZXMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHV0aWwudGVzdE1hcFdpdGhTb3VyY2VzQ29udGVudCk7XG4gICAgdmFyIHNvdXJjZXMgPSBtYXAuc291cmNlcztcblxuICAgIGFzc2VydC5lcXVhbChtYXAuc291cmNlQ29udGVudEZvcihzb3VyY2VzWzBdKSwgJyBPTkUuZm9vID0gZnVuY3Rpb24gKGJhcikge1xcbiAgIHJldHVybiBiYXooYmFyKTtcXG4gfTsnKTtcbiAgICBhc3NlcnQuZXF1YWwobWFwLnNvdXJjZUNvbnRlbnRGb3Ioc291cmNlc1sxXSksICcgVFdPLmluYyA9IGZ1bmN0aW9uIChuKSB7XFxuICAgcmV0dXJuIG4gKyAxO1xcbiB9OycpO1xuICAgIGFzc2VydC5lcXVhbChtYXAuc291cmNlQ29udGVudEZvcihcIm9uZS5qc1wiKSwgJyBPTkUuZm9vID0gZnVuY3Rpb24gKGJhcikge1xcbiAgIHJldHVybiBiYXooYmFyKTtcXG4gfTsnKTtcbiAgICBhc3NlcnQuZXF1YWwobWFwLnNvdXJjZUNvbnRlbnRGb3IoXCJ0d28uanNcIiksICcgVFdPLmluYyA9IGZ1bmN0aW9uIChuKSB7XFxuICAgcmV0dXJuIG4gKyAxO1xcbiB9OycpO1xuICAgIGFzc2VydC50aHJvd3MoZnVuY3Rpb24gKCkge1xuICAgICAgbWFwLnNvdXJjZUNvbnRlbnRGb3IoXCJcIik7XG4gICAgfSwgRXJyb3IpO1xuICAgIGFzc2VydC50aHJvd3MoZnVuY3Rpb24gKCkge1xuICAgICAgbWFwLnNvdXJjZUNvbnRlbnRGb3IoXCIvdGhlL3Jvb3QvdGhyZWUuanNcIik7XG4gICAgfSwgRXJyb3IpO1xuICAgIGFzc2VydC50aHJvd3MoZnVuY3Rpb24gKCkge1xuICAgICAgbWFwLnNvdXJjZUNvbnRlbnRGb3IoXCJ0aHJlZS5qc1wiKTtcbiAgICB9LCBFcnJvcik7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCB0aGF0IHdlIGNhbiBnZXQgdGhlIG9yaWdpbmFsIHNvdXJjZSBjb250ZW50IHdpdGggcmVsYXRpdmUgc291cmNlIHBhdGhzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLnRlc3RNYXBSZWxhdGl2ZVNvdXJjZXMpO1xuICAgIHZhciBzb3VyY2VzID0gbWFwLnNvdXJjZXM7XG5cbiAgICBhc3NlcnQuZXF1YWwobWFwLnNvdXJjZUNvbnRlbnRGb3Ioc291cmNlc1swXSksICcgT05FLmZvbyA9IGZ1bmN0aW9uIChiYXIpIHtcXG4gICByZXR1cm4gYmF6KGJhcik7XFxuIH07Jyk7XG4gICAgYXNzZXJ0LmVxdWFsKG1hcC5zb3VyY2VDb250ZW50Rm9yKHNvdXJjZXNbMV0pLCAnIFRXTy5pbmMgPSBmdW5jdGlvbiAobikge1xcbiAgIHJldHVybiBuICsgMTtcXG4gfTsnKTtcbiAgICBhc3NlcnQuZXF1YWwobWFwLnNvdXJjZUNvbnRlbnRGb3IoXCJvbmUuanNcIiksICcgT05FLmZvbyA9IGZ1bmN0aW9uIChiYXIpIHtcXG4gICByZXR1cm4gYmF6KGJhcik7XFxuIH07Jyk7XG4gICAgYXNzZXJ0LmVxdWFsKG1hcC5zb3VyY2VDb250ZW50Rm9yKFwidHdvLmpzXCIpLCAnIFRXTy5pbmMgPSBmdW5jdGlvbiAobikge1xcbiAgIHJldHVybiBuICsgMTtcXG4gfTsnKTtcbiAgICBhc3NlcnQudGhyb3dzKGZ1bmN0aW9uICgpIHtcbiAgICAgIG1hcC5zb3VyY2VDb250ZW50Rm9yKFwiXCIpO1xuICAgIH0sIEVycm9yKTtcbiAgICBhc3NlcnQudGhyb3dzKGZ1bmN0aW9uICgpIHtcbiAgICAgIG1hcC5zb3VyY2VDb250ZW50Rm9yKFwiL3RoZS9yb290L3RocmVlLmpzXCIpO1xuICAgIH0sIEVycm9yKTtcbiAgICBhc3NlcnQudGhyb3dzKGZ1bmN0aW9uICgpIHtcbiAgICAgIG1hcC5zb3VyY2VDb250ZW50Rm9yKFwidGhyZWUuanNcIik7XG4gICAgfSwgRXJyb3IpO1xuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3QgdGhhdCB3ZSBjYW4gZ2V0IHRoZSBvcmlnaW5hbCBzb3VyY2UgY29udGVudCBmb3IgdGhlIHNvdXJjZXMgb24gYW4gaW5kZXhlZCBzb3VyY2UgbWFwJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih1dGlsLmluZGV4ZWRUZXN0TWFwKTtcbiAgICB2YXIgc291cmNlcyA9IG1hcC5zb3VyY2VzO1xuXG4gICAgYXNzZXJ0LmVxdWFsKG1hcC5zb3VyY2VDb250ZW50Rm9yKHNvdXJjZXNbMF0pLCAnIE9ORS5mb28gPSBmdW5jdGlvbiAoYmFyKSB7XFxuICAgcmV0dXJuIGJheihiYXIpO1xcbiB9OycpO1xuICAgIGFzc2VydC5lcXVhbChtYXAuc291cmNlQ29udGVudEZvcihzb3VyY2VzWzFdKSwgJyBUV08uaW5jID0gZnVuY3Rpb24gKG4pIHtcXG4gICByZXR1cm4gbiArIDE7XFxuIH07Jyk7XG4gICAgYXNzZXJ0LmVxdWFsKG1hcC5zb3VyY2VDb250ZW50Rm9yKFwib25lLmpzXCIpLCAnIE9ORS5mb28gPSBmdW5jdGlvbiAoYmFyKSB7XFxuICAgcmV0dXJuIGJheihiYXIpO1xcbiB9OycpO1xuICAgIGFzc2VydC5lcXVhbChtYXAuc291cmNlQ29udGVudEZvcihcInR3by5qc1wiKSwgJyBUV08uaW5jID0gZnVuY3Rpb24gKG4pIHtcXG4gICByZXR1cm4gbiArIDE7XFxuIH07Jyk7XG4gICAgYXNzZXJ0LnRocm93cyhmdW5jdGlvbiAoKSB7XG4gICAgICBtYXAuc291cmNlQ29udGVudEZvcihcIlwiKTtcbiAgICB9LCBFcnJvcik7XG4gICAgYXNzZXJ0LnRocm93cyhmdW5jdGlvbiAoKSB7XG4gICAgICBtYXAuc291cmNlQ29udGVudEZvcihcIi90aGUvcm9vdC90aHJlZS5qc1wiKTtcbiAgICB9LCBFcnJvcik7XG4gICAgYXNzZXJ0LnRocm93cyhmdW5jdGlvbiAoKSB7XG4gICAgICBtYXAuc291cmNlQ29udGVudEZvcihcInRocmVlLmpzXCIpO1xuICAgIH0sIEVycm9yKTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IGhhc0NvbnRlbnRzT2ZBbGxTb3VyY2VzLCBzaW5nbGUgc291cmNlIHdpdGggY29udGVudHMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICAvLyBIYXMgb25lIHNvdXJjZTogZm9vLmpzICh3aXRoIGNvbnRlbnRzKS5cbiAgICB2YXIgbWFwV2l0aENvbnRlbnRzID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcigpO1xuICAgIG1hcFdpdGhDb250ZW50cy5hZGRNYXBwaW5nKHsgc291cmNlOiAnZm9vLmpzJyxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMTAgfSxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAxLCBjb2x1bW46IDEwIH0gfSk7XG4gICAgbWFwV2l0aENvbnRlbnRzLnNldFNvdXJjZUNvbnRlbnQoJ2Zvby5qcycsICdjb250ZW50IG9mIGZvby5qcycpO1xuICAgIHZhciBjb25zdW1lciA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcihtYXBXaXRoQ29udGVudHMudG9KU09OKCkpO1xuICAgIGFzc2VydC5vayhjb25zdW1lci5oYXNDb250ZW50c09mQWxsU291cmNlcygpKTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IGhhc0NvbnRlbnRzT2ZBbGxTb3VyY2VzLCBzaW5nbGUgc291cmNlIHdpdGhvdXQgY29udGVudHMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICAvLyBIYXMgb25lIHNvdXJjZTogZm9vLmpzICh3aXRob3V0IGNvbnRlbnRzKS5cbiAgICB2YXIgbWFwV2l0aG91dENvbnRlbnRzID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcigpO1xuICAgIG1hcFdpdGhvdXRDb250ZW50cy5hZGRNYXBwaW5nKHsgc291cmNlOiAnZm9vLmpzJyxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMTAgfSxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAxLCBjb2x1bW46IDEwIH0gfSk7XG4gICAgdmFyIGNvbnN1bWVyID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcFdpdGhvdXRDb250ZW50cy50b0pTT04oKSk7XG4gICAgYXNzZXJ0Lm9rKCFjb25zdW1lci5oYXNDb250ZW50c09mQWxsU291cmNlcygpKTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IGhhc0NvbnRlbnRzT2ZBbGxTb3VyY2VzLCB0d28gc291cmNlcyB3aXRoIGNvbnRlbnRzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgLy8gSGFzIHR3byBzb3VyY2VzOiBmb28uanMgKHdpdGggY29udGVudHMpIGFuZCBiYXIuanMgKHdpdGggY29udGVudHMpLlxuICAgIHZhciBtYXBXaXRoQm90aENvbnRlbnRzID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcigpO1xuICAgIG1hcFdpdGhCb3RoQ29udGVudHMuYWRkTWFwcGluZyh7IHNvdXJjZTogJ2Zvby5qcycsXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgb3JpZ2luYWw6IHsgbGluZTogMSwgY29sdW1uOiAxMCB9LFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAxLCBjb2x1bW46IDEwIH0gfSk7XG4gICAgbWFwV2l0aEJvdGhDb250ZW50cy5hZGRNYXBwaW5nKHsgc291cmNlOiAnYmFyLmpzJyxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEwIH0sXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDEsIGNvbHVtbjogMTAgfSB9KTtcbiAgICBtYXBXaXRoQm90aENvbnRlbnRzLnNldFNvdXJjZUNvbnRlbnQoJ2Zvby5qcycsICdjb250ZW50IG9mIGZvby5qcycpO1xuICAgIG1hcFdpdGhCb3RoQ29udGVudHMuc2V0U291cmNlQ29udGVudCgnYmFyLmpzJywgJ2NvbnRlbnQgb2YgYmFyLmpzJyk7XG4gICAgdmFyIGNvbnN1bWVyID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcFdpdGhCb3RoQ29udGVudHMudG9KU09OKCkpO1xuICAgIGFzc2VydC5vayhjb25zdW1lci5oYXNDb250ZW50c09mQWxsU291cmNlcygpKTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IGhhc0NvbnRlbnRzT2ZBbGxTb3VyY2VzLCB0d28gc291cmNlcyBvbmUgd2l0aCBhbmQgb25lIHdpdGhvdXQgY29udGVudHMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICAvLyBIYXMgdHdvIHNvdXJjZXM6IGZvby5qcyAod2l0aCBjb250ZW50cykgYW5kIGJhci5qcyAod2l0aG91dCBjb250ZW50cykuXG4gICAgdmFyIG1hcFdpdGhvdXRTb21lQ29udGVudHMgPSBuZXcgU291cmNlTWFwR2VuZXJhdG9yKCk7XG4gICAgbWFwV2l0aG91dFNvbWVDb250ZW50cy5hZGRNYXBwaW5nKHsgc291cmNlOiAnZm9vLmpzJyxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEwIH0sXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDEsIGNvbHVtbjogMTAgfSB9KTtcbiAgICBtYXBXaXRob3V0U29tZUNvbnRlbnRzLmFkZE1hcHBpbmcoeyBzb3VyY2U6ICdiYXIuanMnLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMTAgfSxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMSwgY29sdW1uOiAxMCB9IH0pO1xuICAgIG1hcFdpdGhvdXRTb21lQ29udGVudHMuc2V0U291cmNlQ29udGVudCgnZm9vLmpzJywgJ2NvbnRlbnQgb2YgZm9vLmpzJyk7XG4gICAgdmFyIGNvbnN1bWVyID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcFdpdGhvdXRTb21lQ29udGVudHMudG9KU09OKCkpO1xuICAgIGFzc2VydC5vayghY29uc3VtZXIuaGFzQ29udGVudHNPZkFsbFNvdXJjZXMoKSk7XG59O1xuXG4gIGV4cG9ydHNbJ3Rlc3Qgc291cmNlUm9vdCArIGdlbmVyYXRlZFBvc2l0aW9uRm9yJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBHZW5lcmF0b3Ioe1xuICAgICAgc291cmNlUm9vdDogJ2Zvby9iYXInLFxuICAgICAgZmlsZTogJ2Jhei5qcydcbiAgICB9KTtcbiAgICBtYXAuYWRkTWFwcGluZyh7XG4gICAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICAgIHNvdXJjZTogJ2JhbmcuY29mZmVlJ1xuICAgIH0pO1xuICAgIG1hcC5hZGRNYXBwaW5nKHtcbiAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDUsIGNvbHVtbjogNSB9LFxuICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDYsIGNvbHVtbjogNiB9LFxuICAgICAgc291cmNlOiAnYmFuZy5jb2ZmZWUnXG4gICAgfSk7XG4gICAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcC50b1N0cmluZygpKTtcblxuICAgIC8vIFNob3VsZCBoYW5kbGUgd2l0aG91dCBzb3VyY2VSb290LlxuICAgIHZhciBwb3MgPSBtYXAuZ2VuZXJhdGVkUG9zaXRpb25Gb3Ioe1xuICAgICAgbGluZTogMSxcbiAgICAgIGNvbHVtbjogMSxcbiAgICAgIHNvdXJjZTogJ2JhbmcuY29mZmVlJ1xuICAgIH0pO1xuXG4gICAgYXNzZXJ0LmVxdWFsKHBvcy5saW5lLCAyKTtcbiAgICBhc3NlcnQuZXF1YWwocG9zLmNvbHVtbiwgMik7XG5cbiAgICAvLyBTaG91bGQgaGFuZGxlIHdpdGggc291cmNlUm9vdC5cbiAgICB2YXIgcG9zID0gbWFwLmdlbmVyYXRlZFBvc2l0aW9uRm9yKHtcbiAgICAgIGxpbmU6IDEsXG4gICAgICBjb2x1bW46IDEsXG4gICAgICBzb3VyY2U6ICdmb28vYmFyL2JhbmcuY29mZmVlJ1xuICAgIH0pO1xuXG4gICAgYXNzZXJ0LmVxdWFsKHBvcy5saW5lLCAyKTtcbiAgICBhc3NlcnQuZXF1YWwocG9zLmNvbHVtbiwgMik7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBzb3VyY2VSb290ICsgZ2VuZXJhdGVkUG9zaXRpb25Gb3IgZm9yIHBhdGggYWJvdmUgdGhlIHJvb3QnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7XG4gICAgICBzb3VyY2VSb290OiAnZm9vL2JhcicsXG4gICAgICBmaWxlOiAnYmF6LmpzJ1xuICAgIH0pO1xuICAgIG1hcC5hZGRNYXBwaW5nKHtcbiAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDIsIGNvbHVtbjogMiB9LFxuICAgICAgc291cmNlOiAnLi4vYmFuZy5jb2ZmZWUnXG4gICAgfSk7XG4gICAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcC50b1N0cmluZygpKTtcblxuICAgIC8vIFNob3VsZCBoYW5kbGUgd2l0aCBzb3VyY2VSb290LlxuICAgIHZhciBwb3MgPSBtYXAuZ2VuZXJhdGVkUG9zaXRpb25Gb3Ioe1xuICAgICAgbGluZTogMSxcbiAgICAgIGNvbHVtbjogMSxcbiAgICAgIHNvdXJjZTogJ2Zvby9iYW5nLmNvZmZlZSdcbiAgICB9KTtcblxuICAgIGFzc2VydC5lcXVhbChwb3MubGluZSwgMik7XG4gICAgYXNzZXJ0LmVxdWFsKHBvcy5jb2x1bW4sIDIpO1xuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3QgYWxsR2VuZXJhdGVkUG9zaXRpb25zRm9yIGZvciBsaW5lJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBHZW5lcmF0b3Ioe1xuICAgICAgZmlsZTogJ2dlbmVyYXRlZC5qcydcbiAgICB9KTtcbiAgICBtYXAuYWRkTWFwcGluZyh7XG4gICAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICAgIHNvdXJjZTogJ2Zvby5jb2ZmZWUnXG4gICAgfSk7XG4gICAgbWFwLmFkZE1hcHBpbmcoe1xuICAgICAgb3JpZ2luYWw6IHsgbGluZTogMSwgY29sdW1uOiAxIH0sXG4gICAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMiwgY29sdW1uOiAyIH0sXG4gICAgICBzb3VyY2U6ICdiYXIuY29mZmVlJ1xuICAgIH0pO1xuICAgIG1hcC5hZGRNYXBwaW5nKHtcbiAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDIsIGNvbHVtbjogMSB9LFxuICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDMsIGNvbHVtbjogMiB9LFxuICAgICAgc291cmNlOiAnYmFyLmNvZmZlZSdcbiAgICB9KTtcbiAgICBtYXAuYWRkTWFwcGluZyh7XG4gICAgICBvcmlnaW5hbDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAzLCBjb2x1bW46IDMgfSxcbiAgICAgIHNvdXJjZTogJ2Jhci5jb2ZmZWUnXG4gICAgfSk7XG4gICAgbWFwLmFkZE1hcHBpbmcoe1xuICAgICAgb3JpZ2luYWw6IHsgbGluZTogMywgY29sdW1uOiAxIH0sXG4gICAgICBnZW5lcmF0ZWQ6IHsgbGluZTogNCwgY29sdW1uOiAyIH0sXG4gICAgICBzb3VyY2U6ICdiYXIuY29mZmVlJ1xuICAgIH0pO1xuICAgIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcihtYXAudG9TdHJpbmcoKSk7XG5cbiAgICB2YXIgbWFwcGluZ3MgPSBtYXAuYWxsR2VuZXJhdGVkUG9zaXRpb25zRm9yKHtcbiAgICAgIGxpbmU6IDIsXG4gICAgICBzb3VyY2U6ICdiYXIuY29mZmVlJ1xuICAgIH0pO1xuXG4gICAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzLmxlbmd0aCwgMik7XG4gICAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzBdLmxpbmUsIDMpO1xuICAgIGFzc2VydC5lcXVhbChtYXBwaW5nc1swXS5jb2x1bW4sIDIpO1xuICAgIGFzc2VydC5lcXVhbChtYXBwaW5nc1sxXS5saW5lLCAzKTtcbiAgICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMV0uY29sdW1uLCAzKTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IGFsbEdlbmVyYXRlZFBvc2l0aW9uc0ZvciBmb3IgbGluZSBmdXp6eSddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBtYXAgPSBuZXcgU291cmNlTWFwR2VuZXJhdG9yKHtcbiAgICAgIGZpbGU6ICdnZW5lcmF0ZWQuanMnXG4gICAgfSk7XG4gICAgbWFwLmFkZE1hcHBpbmcoe1xuICAgICAgb3JpZ2luYWw6IHsgbGluZTogMSwgY29sdW1uOiAxIH0sXG4gICAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMiwgY29sdW1uOiAyIH0sXG4gICAgICBzb3VyY2U6ICdmb28uY29mZmVlJ1xuICAgIH0pO1xuICAgIG1hcC5hZGRNYXBwaW5nKHtcbiAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDIsIGNvbHVtbjogMiB9LFxuICAgICAgc291cmNlOiAnYmFyLmNvZmZlZSdcbiAgICB9KTtcbiAgICBtYXAuYWRkTWFwcGluZyh7XG4gICAgICBvcmlnaW5hbDogeyBsaW5lOiAzLCBjb2x1bW46IDEgfSxcbiAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiA0LCBjb2x1bW46IDIgfSxcbiAgICAgIHNvdXJjZTogJ2Jhci5jb2ZmZWUnXG4gICAgfSk7XG4gICAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcC50b1N0cmluZygpKTtcblxuICAgIHZhciBtYXBwaW5ncyA9IG1hcC5hbGxHZW5lcmF0ZWRQb3NpdGlvbnNGb3Ioe1xuICAgICAgbGluZTogMixcbiAgICAgIHNvdXJjZTogJ2Jhci5jb2ZmZWUnXG4gICAgfSk7XG5cbiAgICBhc3NlcnQuZXF1YWwobWFwcGluZ3MubGVuZ3RoLCAxKTtcbiAgICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMF0ubGluZSwgNCk7XG4gICAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzBdLmNvbHVtbiwgMik7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBhbGxHZW5lcmF0ZWRQb3NpdGlvbnNGb3IgZm9yIGVtcHR5IHNvdXJjZSBtYXAnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7XG4gICAgICBmaWxlOiAnZ2VuZXJhdGVkLmpzJ1xuICAgIH0pO1xuICAgIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcihtYXAudG9TdHJpbmcoKSk7XG5cbiAgICB2YXIgbWFwcGluZ3MgPSBtYXAuYWxsR2VuZXJhdGVkUG9zaXRpb25zRm9yKHtcbiAgICAgIGxpbmU6IDIsXG4gICAgICBzb3VyY2U6ICdiYXIuY29mZmVlJ1xuICAgIH0pO1xuXG4gICAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzLmxlbmd0aCwgMCk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBhbGxHZW5lcmF0ZWRQb3NpdGlvbnNGb3IgZm9yIGNvbHVtbiddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBtYXAgPSBuZXcgU291cmNlTWFwR2VuZXJhdG9yKHtcbiAgICAgIGZpbGU6ICdnZW5lcmF0ZWQuanMnXG4gICAgfSk7XG4gICAgbWFwLmFkZE1hcHBpbmcoe1xuICAgICAgb3JpZ2luYWw6IHsgbGluZTogMSwgY29sdW1uOiAxIH0sXG4gICAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMSwgY29sdW1uOiAyIH0sXG4gICAgICBzb3VyY2U6ICdmb28uY29mZmVlJ1xuICAgIH0pO1xuICAgIG1hcC5hZGRNYXBwaW5nKHtcbiAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDEsIGNvbHVtbjogMyB9LFxuICAgICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgICB9KTtcbiAgICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIobWFwLnRvU3RyaW5nKCkpO1xuXG4gICAgdmFyIG1hcHBpbmdzID0gbWFwLmFsbEdlbmVyYXRlZFBvc2l0aW9uc0Zvcih7XG4gICAgICBsaW5lOiAxLFxuICAgICAgY29sdW1uOiAxLFxuICAgICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgICB9KTtcblxuICAgIGFzc2VydC5lcXVhbChtYXBwaW5ncy5sZW5ndGgsIDIpO1xuICAgIGFzc2VydC5lcXVhbChtYXBwaW5nc1swXS5saW5lLCAxKTtcbiAgICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMF0uY29sdW1uLCAyKTtcbiAgICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMV0ubGluZSwgMSk7XG4gICAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzFdLmNvbHVtbiwgMyk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBhbGxHZW5lcmF0ZWRQb3NpdGlvbnNGb3IgZm9yIGNvbHVtbiBmdXp6eSddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBtYXAgPSBuZXcgU291cmNlTWFwR2VuZXJhdG9yKHtcbiAgICAgIGZpbGU6ICdnZW5lcmF0ZWQuanMnXG4gICAgfSk7XG4gICAgbWFwLmFkZE1hcHBpbmcoe1xuICAgICAgb3JpZ2luYWw6IHsgbGluZTogMSwgY29sdW1uOiAxIH0sXG4gICAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMSwgY29sdW1uOiAyIH0sXG4gICAgICBzb3VyY2U6ICdmb28uY29mZmVlJ1xuICAgIH0pO1xuICAgIG1hcC5hZGRNYXBwaW5nKHtcbiAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDEsIGNvbHVtbjogMyB9LFxuICAgICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgICB9KTtcbiAgICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIobWFwLnRvU3RyaW5nKCkpO1xuXG4gICAgdmFyIG1hcHBpbmdzID0gbWFwLmFsbEdlbmVyYXRlZFBvc2l0aW9uc0Zvcih7XG4gICAgICBsaW5lOiAxLFxuICAgICAgY29sdW1uOiAwLFxuICAgICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgICB9KTtcblxuICAgIGFzc2VydC5lcXVhbChtYXBwaW5ncy5sZW5ndGgsIDIpO1xuICAgIGFzc2VydC5lcXVhbChtYXBwaW5nc1swXS5saW5lLCAxKTtcbiAgICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMF0uY29sdW1uLCAyKTtcbiAgICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMV0ubGluZSwgMSk7XG4gICAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzFdLmNvbHVtbiwgMyk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBhbGxHZW5lcmF0ZWRQb3NpdGlvbnNGb3IgZm9yIGNvbHVtbiBvbiBkaWZmZXJlbnQgbGluZSBmdXp6eSddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBtYXAgPSBuZXcgU291cmNlTWFwR2VuZXJhdG9yKHtcbiAgICAgIGZpbGU6ICdnZW5lcmF0ZWQuanMnXG4gICAgfSk7XG4gICAgbWFwLmFkZE1hcHBpbmcoe1xuICAgICAgb3JpZ2luYWw6IHsgbGluZTogMiwgY29sdW1uOiAxIH0sXG4gICAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMiwgY29sdW1uOiAyIH0sXG4gICAgICBzb3VyY2U6ICdmb28uY29mZmVlJ1xuICAgIH0pO1xuICAgIG1hcC5hZGRNYXBwaW5nKHtcbiAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDIsIGNvbHVtbjogMSB9LFxuICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDIsIGNvbHVtbjogMyB9LFxuICAgICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgICB9KTtcbiAgICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIobWFwLnRvU3RyaW5nKCkpO1xuXG4gICAgdmFyIG1hcHBpbmdzID0gbWFwLmFsbEdlbmVyYXRlZFBvc2l0aW9uc0Zvcih7XG4gICAgICBsaW5lOiAxLFxuICAgICAgY29sdW1uOiAwLFxuICAgICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgICB9KTtcblxuICAgIGFzc2VydC5lcXVhbChtYXBwaW5ncy5sZW5ndGgsIDApO1xuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3QgY29tcHV0ZUNvbHVtblNwYW5zJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBHZW5lcmF0b3Ioe1xuICAgICAgZmlsZTogJ2dlbmVyYXRlZC5qcydcbiAgICB9KTtcbiAgICBtYXAuYWRkTWFwcGluZyh7XG4gICAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICAgIHNvdXJjZTogJ2Zvby5jb2ZmZWUnXG4gICAgfSk7XG4gICAgbWFwLmFkZE1hcHBpbmcoe1xuICAgICAgb3JpZ2luYWw6IHsgbGluZTogMiwgY29sdW1uOiAxIH0sXG4gICAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMiwgY29sdW1uOiAxIH0sXG4gICAgICBzb3VyY2U6ICdmb28uY29mZmVlJ1xuICAgIH0pO1xuICAgIG1hcC5hZGRNYXBwaW5nKHtcbiAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDIsIGNvbHVtbjogMiB9LFxuICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDIsIGNvbHVtbjogMTAgfSxcbiAgICAgIHNvdXJjZTogJ2Zvby5jb2ZmZWUnXG4gICAgfSk7XG4gICAgbWFwLmFkZE1hcHBpbmcoe1xuICAgICAgb3JpZ2luYWw6IHsgbGluZTogMiwgY29sdW1uOiAzIH0sXG4gICAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMiwgY29sdW1uOiAyMCB9LFxuICAgICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgICB9KTtcbiAgICBtYXAuYWRkTWFwcGluZyh7XG4gICAgICBvcmlnaW5hbDogeyBsaW5lOiAzLCBjb2x1bW46IDEgfSxcbiAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAzLCBjb2x1bW46IDEgfSxcbiAgICAgIHNvdXJjZTogJ2Zvby5jb2ZmZWUnXG4gICAgfSk7XG4gICAgbWFwLmFkZE1hcHBpbmcoe1xuICAgICAgb3JpZ2luYWw6IHsgbGluZTogMywgY29sdW1uOiAyIH0sXG4gICAgICBnZW5lcmF0ZWQ6IHsgbGluZTogMywgY29sdW1uOiAyIH0sXG4gICAgICBzb3VyY2U6ICdmb28uY29mZmVlJ1xuICAgIH0pO1xuICAgIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcihtYXAudG9TdHJpbmcoKSk7XG5cbiAgICBtYXAuY29tcHV0ZUNvbHVtblNwYW5zKCk7XG5cbiAgICB2YXIgbWFwcGluZ3MgPSBtYXAuYWxsR2VuZXJhdGVkUG9zaXRpb25zRm9yKHtcbiAgICAgIGxpbmU6IDEsXG4gICAgICBzb3VyY2U6ICdmb28uY29mZmVlJ1xuICAgIH0pO1xuXG4gICAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzLmxlbmd0aCwgMSk7XG4gICAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzBdLmxhc3RDb2x1bW4sIEluZmluaXR5KTtcblxuICAgIHZhciBtYXBwaW5ncyA9IG1hcC5hbGxHZW5lcmF0ZWRQb3NpdGlvbnNGb3Ioe1xuICAgICAgbGluZTogMixcbiAgICAgIHNvdXJjZTogJ2Zvby5jb2ZmZWUnXG4gICAgfSk7XG5cbiAgICBhc3NlcnQuZXF1YWwobWFwcGluZ3MubGVuZ3RoLCAzKTtcbiAgICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMF0ubGFzdENvbHVtbiwgOSk7XG4gICAgYXNzZXJ0LmVxdWFsKG1hcHBpbmdzWzFdLmxhc3RDb2x1bW4sIDE5KTtcbiAgICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMl0ubGFzdENvbHVtbiwgSW5maW5pdHkpO1xuXG4gICAgdmFyIG1hcHBpbmdzID0gbWFwLmFsbEdlbmVyYXRlZFBvc2l0aW9uc0Zvcih7XG4gICAgICBsaW5lOiAzLFxuICAgICAgc291cmNlOiAnZm9vLmNvZmZlZSdcbiAgICB9KTtcblxuICAgIGFzc2VydC5lcXVhbChtYXBwaW5ncy5sZW5ndGgsIDIpO1xuICAgIGFzc2VydC5lcXVhbChtYXBwaW5nc1swXS5sYXN0Q29sdW1uLCAxKTtcbiAgICBhc3NlcnQuZXF1YWwobWFwcGluZ3NbMV0ubGFzdENvbHVtbiwgSW5maW5pdHkpO1xuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3Qgc291cmNlUm9vdCArIG9yaWdpbmFsUG9zaXRpb25Gb3InXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7XG4gICAgICBzb3VyY2VSb290OiAnZm9vL2JhcicsXG4gICAgICBmaWxlOiAnYmF6LmpzJ1xuICAgIH0pO1xuICAgIG1hcC5hZGRNYXBwaW5nKHtcbiAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDIsIGNvbHVtbjogMiB9LFxuICAgICAgc291cmNlOiAnYmFuZy5jb2ZmZWUnXG4gICAgfSk7XG4gICAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcC50b1N0cmluZygpKTtcblxuICAgIHZhciBwb3MgPSBtYXAub3JpZ2luYWxQb3NpdGlvbkZvcih7XG4gICAgICBsaW5lOiAyLFxuICAgICAgY29sdW1uOiAyLFxuICAgIH0pO1xuXG4gICAgLy8gU2hvdWxkIGFsd2F5cyBoYXZlIHRoZSBwcmVwZW5kZWQgc291cmNlIHJvb3RcbiAgICBhc3NlcnQuZXF1YWwocG9zLnNvdXJjZSwgJ2Zvby9iYXIvYmFuZy5jb2ZmZWUnKTtcbiAgICBhc3NlcnQuZXF1YWwocG9zLmxpbmUsIDEpO1xuICAgIGFzc2VydC5lcXVhbChwb3MuY29sdW1uLCAxKTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IGdpdGh1YiBpc3N1ZSAjNTYnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7XG4gICAgICBzb3VyY2VSb290OiAnaHR0cDovLycsXG4gICAgICBmaWxlOiAnd3d3LmV4YW1wbGUuY29tL2Zvby5qcydcbiAgICB9KTtcbiAgICBtYXAuYWRkTWFwcGluZyh7XG4gICAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICAgIHNvdXJjZTogJ3d3dy5leGFtcGxlLmNvbS9vcmlnaW5hbC5qcydcbiAgICB9KTtcbiAgICBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIobWFwLnRvU3RyaW5nKCkpO1xuXG4gICAgdmFyIHNvdXJjZXMgPSBtYXAuc291cmNlcztcbiAgICBhc3NlcnQuZXF1YWwoc291cmNlcy5sZW5ndGgsIDEpO1xuICAgIGFzc2VydC5lcXVhbChzb3VyY2VzWzBdLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbS9vcmlnaW5hbC5qcycpO1xuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3QgZ2l0aHViIGlzc3VlICM0MyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBtYXAgPSBuZXcgU291cmNlTWFwR2VuZXJhdG9yKHtcbiAgICAgIHNvdXJjZVJvb3Q6ICdodHRwOi8vZXhhbXBsZS5jb20nLFxuICAgICAgZmlsZTogJ2Zvby5qcydcbiAgICB9KTtcbiAgICBtYXAuYWRkTWFwcGluZyh7XG4gICAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICAgIHNvdXJjZTogJ2h0dHA6Ly9jZG4uZXhhbXBsZS5jb20vb3JpZ2luYWwuanMnXG4gICAgfSk7XG4gICAgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKG1hcC50b1N0cmluZygpKTtcblxuICAgIHZhciBzb3VyY2VzID0gbWFwLnNvdXJjZXM7XG4gICAgYXNzZXJ0LmVxdWFsKHNvdXJjZXMubGVuZ3RoLCAxLFxuICAgICAgICAgICAgICAgICAnU2hvdWxkIG9ubHkgYmUgb25lIHNvdXJjZS4nKTtcbiAgICBhc3NlcnQuZXF1YWwoc291cmNlc1swXSwgJ2h0dHA6Ly9jZG4uZXhhbXBsZS5jb20vb3JpZ2luYWwuanMnLFxuICAgICAgICAgICAgICAgICAnU2hvdWxkIG5vdCBiZSBqb2luZWQgd2l0aCB0aGUgc291cmNlUm9vdC4nKTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IGFic29sdXRlIHBhdGgsIGJ1dCBzYW1lIGhvc3Qgc291cmNlcyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBtYXAgPSBuZXcgU291cmNlTWFwR2VuZXJhdG9yKHtcbiAgICAgIHNvdXJjZVJvb3Q6ICdodHRwOi8vZXhhbXBsZS5jb20vZm9vL2JhcicsXG4gICAgICBmaWxlOiAnZm9vLmpzJ1xuICAgIH0pO1xuICAgIG1hcC5hZGRNYXBwaW5nKHtcbiAgICAgIG9yaWdpbmFsOiB7IGxpbmU6IDEsIGNvbHVtbjogMSB9LFxuICAgICAgZ2VuZXJhdGVkOiB7IGxpbmU6IDIsIGNvbHVtbjogMiB9LFxuICAgICAgc291cmNlOiAnL29yaWdpbmFsLmpzJ1xuICAgIH0pO1xuICAgIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcihtYXAudG9TdHJpbmcoKSk7XG5cbiAgICB2YXIgc291cmNlcyA9IG1hcC5zb3VyY2VzO1xuICAgIGFzc2VydC5lcXVhbChzb3VyY2VzLmxlbmd0aCwgMSxcbiAgICAgICAgICAgICAgICAgJ1Nob3VsZCBvbmx5IGJlIG9uZSBzb3VyY2UuJyk7XG4gICAgYXNzZXJ0LmVxdWFsKHNvdXJjZXNbMF0sICdodHRwOi8vZXhhbXBsZS5jb20vb3JpZ2luYWwuanMnLFxuICAgICAgICAgICAgICAgICAnU291cmNlIHNob3VsZCBiZSByZWxhdGl2ZSB0aGUgaG9zdCBvZiB0aGUgc291cmNlIHJvb3QuJyk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBpbmRleGVkIHNvdXJjZSBtYXAgZXJyb3JzIHdoZW4gc2VjdGlvbnMgYXJlIG91dCBvZiBvcmRlciBieSBsaW5lJ10gPSBmdW5jdGlvbihhc3NlcnQpIHtcbiAgICAvLyBNYWtlIGEgZGVlcCBjb3B5IG9mIHRoZSBpbmRleGVkVGVzdE1hcFxuICAgIHZhciBtaXNvcmRlcmVkSW5kZXhlZFRlc3RNYXAgPSBKU09OLnBhcnNlKEpTT04uc3RyaW5naWZ5KHV0aWwuaW5kZXhlZFRlc3RNYXApKTtcblxuICAgIG1pc29yZGVyZWRJbmRleGVkVGVzdE1hcC5zZWN0aW9uc1swXS5vZmZzZXQgPSB7XG4gICAgICBsaW5lOiAyLFxuICAgICAgY29sdW1uOiAwXG4gICAgfTtcblxuICAgIGFzc2VydC50aHJvd3MoZnVuY3Rpb24oKSB7XG4gICAgICBuZXcgU291cmNlTWFwQ29uc3VtZXIobWlzb3JkZXJlZEluZGV4ZWRUZXN0TWFwKTtcbiAgICB9LCBFcnJvcik7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBnaXRodWIgaXNzdWUgIzY0J10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih7XG4gICAgICBcInZlcnNpb25cIjogMyxcbiAgICAgIFwiZmlsZVwiOiBcImZvby5qc1wiLFxuICAgICAgXCJzb3VyY2VSb290XCI6IFwiaHR0cDovL2V4YW1wbGUuY29tL1wiLFxuICAgICAgXCJzb3VyY2VzXCI6IFtcIi9hXCJdLFxuICAgICAgXCJuYW1lc1wiOiBbXSxcbiAgICAgIFwibWFwcGluZ3NcIjogXCJBQUNBXCIsXG4gICAgICBcInNvdXJjZXNDb250ZW50XCI6IFtcImZvb1wiXVxuICAgIH0pO1xuXG4gICAgYXNzZXJ0LmVxdWFsKG1hcC5zb3VyY2VDb250ZW50Rm9yKFwiYVwiKSwgXCJmb29cIik7XG4gICAgYXNzZXJ0LmVxdWFsKG1hcC5zb3VyY2VDb250ZW50Rm9yKFwiL2FcIiksIFwiZm9vXCIpO1xuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3QgYnVnIDg4NTU5NyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBtYXAgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIoe1xuICAgICAgXCJ2ZXJzaW9uXCI6IDMsXG4gICAgICBcImZpbGVcIjogXCJmb28uanNcIixcbiAgICAgIFwic291cmNlUm9vdFwiOiBcImZpbGU6Ly8vVXNlcnMvQWxHb3JlL0ludmVudGVkL1RoZS9JbnRlcm5ldC9cIixcbiAgICAgIFwic291cmNlc1wiOiBbXCIvYVwiXSxcbiAgICAgIFwibmFtZXNcIjogW10sXG4gICAgICBcIm1hcHBpbmdzXCI6IFwiQUFDQVwiLFxuICAgICAgXCJzb3VyY2VzQ29udGVudFwiOiBbXCJmb29cIl1cbiAgICB9KTtcblxuICAgIHZhciBzID0gbWFwLnNvdXJjZXNbMF07XG4gICAgYXNzZXJ0LmVxdWFsKG1hcC5zb3VyY2VDb250ZW50Rm9yKHMpLCBcImZvb1wiKTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IGdpdGh1YiBpc3N1ZSAjNzIsIGR1cGxpY2F0ZSBzb3VyY2VzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG1hcCA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih7XG4gICAgICBcInZlcnNpb25cIjogMyxcbiAgICAgIFwiZmlsZVwiOiBcImZvby5qc1wiLFxuICAgICAgXCJzb3VyY2VzXCI6IFtcInNvdXJjZTEuanNcIiwgXCJzb3VyY2UxLmpzXCIsIFwic291cmNlMy5qc1wiXSxcbiAgICAgIFwibmFtZXNcIjogW10sXG4gICAgICBcIm1hcHBpbmdzXCI6IFwiO0VBQUM7O0lBRUU7O01FRUVcIixcbiAgICAgIFwic291cmNlUm9vdFwiOiBcImh0dHA6Ly9leGFtcGxlLmNvbVwiXG4gICAgfSk7XG5cbiAgICB2YXIgcG9zID0gbWFwLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgICAgbGluZTogMixcbiAgICAgIGNvbHVtbjogMlxuICAgIH0pO1xuICAgIGFzc2VydC5lcXVhbChwb3Muc291cmNlLCAnaHR0cDovL2V4YW1wbGUuY29tL3NvdXJjZTEuanMnKTtcbiAgICBhc3NlcnQuZXF1YWwocG9zLmxpbmUsIDEpO1xuICAgIGFzc2VydC5lcXVhbChwb3MuY29sdW1uLCAxKTtcblxuICAgIHZhciBwb3MgPSBtYXAub3JpZ2luYWxQb3NpdGlvbkZvcih7XG4gICAgICBsaW5lOiA0LFxuICAgICAgY29sdW1uOiA0XG4gICAgfSk7XG4gICAgYXNzZXJ0LmVxdWFsKHBvcy5zb3VyY2UsICdodHRwOi8vZXhhbXBsZS5jb20vc291cmNlMS5qcycpO1xuICAgIGFzc2VydC5lcXVhbChwb3MubGluZSwgMyk7XG4gICAgYXNzZXJ0LmVxdWFsKHBvcy5jb2x1bW4sIDMpO1xuXG4gICAgdmFyIHBvcyA9IG1hcC5vcmlnaW5hbFBvc2l0aW9uRm9yKHtcbiAgICAgIGxpbmU6IDYsXG4gICAgICBjb2x1bW46IDZcbiAgICB9KTtcbiAgICBhc3NlcnQuZXF1YWwocG9zLnNvdXJjZSwgJ2h0dHA6Ly9leGFtcGxlLmNvbS9zb3VyY2UzLmpzJyk7XG4gICAgYXNzZXJ0LmVxdWFsKHBvcy5saW5lLCA1KTtcbiAgICBhc3NlcnQuZXF1YWwocG9zLmNvbHVtbiwgNSk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBnaXRodWIgaXNzdWUgIzcyLCBkdXBsaWNhdGUgbmFtZXMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgbWFwID0gbmV3IFNvdXJjZU1hcENvbnN1bWVyKHtcbiAgICAgIFwidmVyc2lvblwiOiAzLFxuICAgICAgXCJmaWxlXCI6IFwiZm9vLmpzXCIsXG4gICAgICBcInNvdXJjZXNcIjogW1wic291cmNlLmpzXCJdLFxuICAgICAgXCJuYW1lc1wiOiBbXCJuYW1lMVwiLCBcIm5hbWUxXCIsIFwibmFtZTNcIl0sXG4gICAgICBcIm1hcHBpbmdzXCI6IFwiO0VBQUNBOztJQUVFQTs7TUFFRUVcIixcbiAgICAgIFwic291cmNlUm9vdFwiOiBcImh0dHA6Ly9leGFtcGxlLmNvbVwiXG4gICAgfSk7XG5cbiAgICB2YXIgcG9zID0gbWFwLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgICAgbGluZTogMixcbiAgICAgIGNvbHVtbjogMlxuICAgIH0pO1xuICAgIGFzc2VydC5lcXVhbChwb3MubmFtZSwgJ25hbWUxJyk7XG4gICAgYXNzZXJ0LmVxdWFsKHBvcy5saW5lLCAxKTtcbiAgICBhc3NlcnQuZXF1YWwocG9zLmNvbHVtbiwgMSk7XG5cbiAgICB2YXIgcG9zID0gbWFwLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgICAgbGluZTogNCxcbiAgICAgIGNvbHVtbjogNFxuICAgIH0pO1xuICAgIGFzc2VydC5lcXVhbChwb3MubmFtZSwgJ25hbWUxJyk7XG4gICAgYXNzZXJ0LmVxdWFsKHBvcy5saW5lLCAzKTtcbiAgICBhc3NlcnQuZXF1YWwocG9zLmNvbHVtbiwgMyk7XG5cbiAgICB2YXIgcG9zID0gbWFwLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgICAgbGluZTogNixcbiAgICAgIGNvbHVtbjogNlxuICAgIH0pO1xuICAgIGFzc2VydC5lcXVhbChwb3MubmFtZSwgJ25hbWUzJyk7XG4gICAgYXNzZXJ0LmVxdWFsKHBvcy5saW5lLCA1KTtcbiAgICBhc3NlcnQuZXF1YWwocG9zLmNvbHVtbiwgNSk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBTb3VyY2VNYXBDb25zdW1lci5mcm9tU291cmNlTWFwJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIHNtZyA9IG5ldyBTb3VyY2VNYXBHZW5lcmF0b3Ioe1xuICAgICAgc291cmNlUm9vdDogJ2h0dHA6Ly9leGFtcGxlLmNvbS8nLFxuICAgICAgZmlsZTogJ2Zvby5qcydcbiAgICB9KTtcbiAgICBzbWcuYWRkTWFwcGluZyh7XG4gICAgICBvcmlnaW5hbDogeyBsaW5lOiAxLCBjb2x1bW46IDEgfSxcbiAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICAgIHNvdXJjZTogJ2Jhci5qcydcbiAgICB9KTtcbiAgICBzbWcuYWRkTWFwcGluZyh7XG4gICAgICBvcmlnaW5hbDogeyBsaW5lOiAyLCBjb2x1bW46IDIgfSxcbiAgICAgIGdlbmVyYXRlZDogeyBsaW5lOiA0LCBjb2x1bW46IDQgfSxcbiAgICAgIHNvdXJjZTogJ2Jhei5qcycsXG4gICAgICBuYW1lOiAnZGlydE1jR2lydCdcbiAgICB9KTtcbiAgICBzbWcuc2V0U291cmNlQ29udGVudCgnYmF6LmpzJywgJ2Jhei5qcyBjb250ZW50Jyk7XG5cbiAgICB2YXIgc21jID0gU291cmNlTWFwQ29uc3VtZXIuZnJvbVNvdXJjZU1hcChzbWcpO1xuICAgIGFzc2VydC5lcXVhbChzbWMuZmlsZSwgJ2Zvby5qcycpO1xuICAgIGFzc2VydC5lcXVhbChzbWMuc291cmNlUm9vdCwgJ2h0dHA6Ly9leGFtcGxlLmNvbS8nKTtcbiAgICBhc3NlcnQuZXF1YWwoc21jLnNvdXJjZXMubGVuZ3RoLCAyKTtcbiAgICBhc3NlcnQuZXF1YWwoc21jLnNvdXJjZXNbMF0sICdodHRwOi8vZXhhbXBsZS5jb20vYmFyLmpzJyk7XG4gICAgYXNzZXJ0LmVxdWFsKHNtYy5zb3VyY2VzWzFdLCAnaHR0cDovL2V4YW1wbGUuY29tL2Jhei5qcycpO1xuICAgIGFzc2VydC5lcXVhbChzbWMuc291cmNlQ29udGVudEZvcignYmF6LmpzJyksICdiYXouanMgY29udGVudCcpO1xuXG4gICAgdmFyIHBvcyA9IHNtYy5vcmlnaW5hbFBvc2l0aW9uRm9yKHtcbiAgICAgIGxpbmU6IDIsXG4gICAgICBjb2x1bW46IDJcbiAgICB9KTtcbiAgICBhc3NlcnQuZXF1YWwocG9zLmxpbmUsIDEpO1xuICAgIGFzc2VydC5lcXVhbChwb3MuY29sdW1uLCAxKTtcbiAgICBhc3NlcnQuZXF1YWwocG9zLnNvdXJjZSwgJ2h0dHA6Ly9leGFtcGxlLmNvbS9iYXIuanMnKTtcbiAgICBhc3NlcnQuZXF1YWwocG9zLm5hbWUsIG51bGwpO1xuXG4gICAgcG9zID0gc21jLmdlbmVyYXRlZFBvc2l0aW9uRm9yKHtcbiAgICAgIGxpbmU6IDEsXG4gICAgICBjb2x1bW46IDEsXG4gICAgICBzb3VyY2U6ICdodHRwOi8vZXhhbXBsZS5jb20vYmFyLmpzJ1xuICAgIH0pO1xuICAgIGFzc2VydC5lcXVhbChwb3MubGluZSwgMik7XG4gICAgYXNzZXJ0LmVxdWFsKHBvcy5jb2x1bW4sIDIpO1xuXG4gICAgcG9zID0gc21jLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgICAgbGluZTogNCxcbiAgICAgIGNvbHVtbjogNFxuICAgIH0pO1xuICAgIGFzc2VydC5lcXVhbChwb3MubGluZSwgMik7XG4gICAgYXNzZXJ0LmVxdWFsKHBvcy5jb2x1bW4sIDIpO1xuICAgIGFzc2VydC5lcXVhbChwb3Muc291cmNlLCAnaHR0cDovL2V4YW1wbGUuY29tL2Jhei5qcycpO1xuICAgIGFzc2VydC5lcXVhbChwb3MubmFtZSwgJ2RpcnRNY0dpcnQnKTtcblxuICAgIHBvcyA9IHNtYy5nZW5lcmF0ZWRQb3NpdGlvbkZvcih7XG4gICAgICBsaW5lOiAyLFxuICAgICAgY29sdW1uOiAyLFxuICAgICAgc291cmNlOiAnaHR0cDovL2V4YW1wbGUuY29tL2Jhei5qcydcbiAgICB9KTtcbiAgICBhc3NlcnQuZXF1YWwocG9zLmxpbmUsIDQpO1xuICAgIGFzc2VydC5lcXVhbChwb3MuY29sdW1uLCA0KTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IGlzc3VlICMxOTEnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgZ2VuZXJhdG9yID0gbmV3IFNvdXJjZU1hcEdlbmVyYXRvcih7IGZpbGU6ICdhLmNzcycgfSk7XG4gICAgZ2VuZXJhdG9yLmFkZE1hcHBpbmcoe1xuICAgICAgc291cmNlOiAgICdiLmNzcycsXG4gICAgICBvcmlnaW5hbDoge1xuICAgICAgICBsaW5lOiAgIDEsXG4gICAgICAgIGNvbHVtbjogMFxuICAgICAgfSxcbiAgICAgIGdlbmVyYXRlZDoge1xuICAgICAgICBsaW5lOiAgIDEsXG4gICAgICAgIGNvbHVtbjogMFxuICAgICAgfVxuICAgIH0pO1xuXG4gICAgLy8gQ3JlYXRlIGEgU291cmNlTWFwQ29uc3VtZXIgZnJvbSB0aGUgU291cmNlTWFwR2VuZXJhdG9yLCAuLi5cbiAgICB2YXIgY29uc3VtZXIgID0gU291cmNlTWFwQ29uc3VtZXIuZnJvbVNvdXJjZU1hcChnZW5lcmF0b3IpO1xuICAgIC8vIC4uLiBhbmQgdGhlbiB0cnkgYW5kIHVzZSB0aGUgU291cmNlTWFwR2VuZXJhdG9yIGFnYWluLiBUaGlzIHNob3VsZCBub3RcbiAgICAvLyB0aHJvdy5cbiAgICBnZW5lcmF0b3IudG9KU09OKCk7XG5cbiAgICBhc3NlcnQub2sodHJ1ZSwgXCJVc2luZyBhIFNvdXJjZU1hcEdlbmVyYXRvciBhZ2FpbiBhZnRlciBjcmVhdGluZyBhIFwiICtcbiAgICAgICAgICAgICAgICAgICAgXCJTb3VyY2VNYXBDb25zdW1lciBmcm9tIGl0IHNob3VsZCBub3QgdGhyb3dcIik7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBzb3VyY2VzIHdoZXJlIHRoZWlyIHByZWZpeCBpcyB0aGUgc291cmNlIHJvb3Q6IGlzc3VlICMxOTknXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgdGVzdFNvdXJjZU1hcCA9IHtcbiAgICAgIFwidmVyc2lvblwiOiAzLFxuICAgICAgXCJzb3VyY2VzXCI6IFtcIi9zb3VyY2UvYXBwL2FwcC9hcHAuanNcIl0sXG4gICAgICBcIm5hbWVzXCI6IFtcIlN5c3RlbVwiXSxcbiAgICAgIFwibWFwcGluZ3NcIjogXCJBQUFBQVwiLFxuICAgICAgXCJmaWxlXCI6IFwiYXBwL2FwcC5qc1wiLFxuICAgICAgXCJzb3VyY2VzQ29udGVudFwiOiBbXCIndXNlIHN0cmljdCc7XCJdLFxuICAgICAgXCJzb3VyY2VSb290XCI6XCIvc291cmNlL1wiXG4gICAgfTtcblxuICAgIHZhciBjb25zdW1lciA9IG5ldyBTb3VyY2VNYXBDb25zdW1lcih0ZXN0U291cmNlTWFwKTtcblxuICAgIGZ1bmN0aW9uIGNvbnN1bWVySGFzU291cmNlKHMpIHtcbiAgICAgIGFzc2VydC5vayhjb25zdW1lci5zb3VyY2VDb250ZW50Rm9yKHMpKTtcbiAgICB9XG5cbiAgICBjb25zdW1lci5zb3VyY2VzLmZvckVhY2goY29uc3VtZXJIYXNTb3VyY2UpO1xuICAgIHRlc3RTb3VyY2VNYXAuc291cmNlcy5mb3JFYWNoKGNvbnN1bWVySGFzU291cmNlKTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IHNvdXJjZXMgd2hlcmUgdGhlaXIgcHJlZml4IGlzIHRoZSBzb3VyY2Ugcm9vdCBhbmQgdGhlIHNvdXJjZSByb290IGlzIGEgdXJsOiBpc3N1ZSAjMTk5J10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIHRlc3RTb3VyY2VNYXAgPSB7XG4gICAgICBcInZlcnNpb25cIjogMyxcbiAgICAgIFwic291cmNlc1wiOiBbXCJodHRwOi8vZXhhbXBsZS5jb20vc291cmNlL2FwcC9hcHAvYXBwLmpzXCJdLFxuICAgICAgXCJuYW1lc1wiOiBbXCJTeXN0ZW1cIl0sXG4gICAgICBcIm1hcHBpbmdzXCI6IFwiQUFBQUFcIixcbiAgICAgIFwic291cmNlc0NvbnRlbnRcIjogW1wiJ3VzZSBzdHJpY3QnO1wiXSxcbiAgICAgIFwic291cmNlUm9vdFwiOlwiaHR0cDovL2V4YW1wbGUuY29tL3NvdXJjZS9cIlxuICAgIH07XG5cbiAgICB2YXIgY29uc3VtZXIgPSBuZXcgU291cmNlTWFwQ29uc3VtZXIodGVzdFNvdXJjZU1hcCk7XG5cbiAgICBmdW5jdGlvbiBjb25zdW1lckhhc1NvdXJjZShzKSB7XG4gICAgICBhc3NlcnQub2soY29uc3VtZXIuc291cmNlQ29udGVudEZvcihzKSk7XG4gICAgfVxuXG4gICAgY29uc3VtZXIuc291cmNlcy5mb3JFYWNoKGNvbnN1bWVySGFzU291cmNlKTtcbiAgICB0ZXN0U291cmNlTWFwLnNvdXJjZXMuZm9yRWFjaChjb25zdW1lckhhc1NvdXJjZSk7XG4gIH07XG59XG5cblxuXG4vKioqKioqKioqKioqKioqKipcbiAqKiBXRUJQQUNLIEZPT1RFUlxuICoqIC4vdGVzdC90ZXN0LXNvdXJjZS1tYXAtY29uc3VtZXIuanNcbiAqKiBtb2R1bGUgaWQgPSAwXG4gKiogbW9kdWxlIGNodW5rcyA9IDBcbiAqKi8iLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG57XG4gIHZhciB1dGlsID0gcmVxdWlyZSgnLi4vbGliL3V0aWwnKTtcblxuICAvLyBUaGlzIGlzIGEgdGVzdCBtYXBwaW5nIHdoaWNoIG1hcHMgZnVuY3Rpb25zIGZyb20gdHdvIGRpZmZlcmVudCBmaWxlc1xuICAvLyAob25lLmpzIGFuZCB0d28uanMpIHRvIGEgbWluaWZpZWQgZ2VuZXJhdGVkIHNvdXJjZS5cbiAgLy9cbiAgLy8gSGVyZSBpcyBvbmUuanM6XG4gIC8vXG4gIC8vICAgT05FLmZvbyA9IGZ1bmN0aW9uIChiYXIpIHtcbiAgLy8gICAgIHJldHVybiBiYXooYmFyKTtcbiAgLy8gICB9O1xuICAvL1xuICAvLyBIZXJlIGlzIHR3by5qczpcbiAgLy9cbiAgLy8gICBUV08uaW5jID0gZnVuY3Rpb24gKG4pIHtcbiAgLy8gICAgIHJldHVybiBuICsgMTtcbiAgLy8gICB9O1xuICAvL1xuICAvLyBBbmQgaGVyZSBpcyB0aGUgZ2VuZXJhdGVkIGNvZGUgKG1pbi5qcyk6XG4gIC8vXG4gIC8vICAgT05FLmZvbz1mdW5jdGlvbihhKXtyZXR1cm4gYmF6KGEpO307XG4gIC8vICAgVFdPLmluYz1mdW5jdGlvbihhKXtyZXR1cm4gYSsxO307XG4gIGV4cG9ydHMudGVzdEdlbmVyYXRlZENvZGUgPSBcIiBPTkUuZm9vPWZ1bmN0aW9uKGEpe3JldHVybiBiYXooYSk7fTtcXG5cIitcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIFwiIFRXTy5pbmM9ZnVuY3Rpb24oYSl7cmV0dXJuIGErMTt9O1wiO1xuICBleHBvcnRzLnRlc3RNYXAgPSB7XG4gICAgdmVyc2lvbjogMyxcbiAgICBmaWxlOiAnbWluLmpzJyxcbiAgICBuYW1lczogWydiYXInLCAnYmF6JywgJ24nXSxcbiAgICBzb3VyY2VzOiBbJ29uZS5qcycsICd0d28uanMnXSxcbiAgICBzb3VyY2VSb290OiAnL3RoZS9yb290JyxcbiAgICBtYXBwaW5nczogJ0NBQUMsSUFBSSxJQUFNLFNBQVVBLEdBQ2xCLE9BQU9DLElBQUlEO0NDRGIsSUFBSSxJQUFNLFNBQVVFLEdBQ2xCLE9BQU9BJ1xuICB9O1xuICBleHBvcnRzLnRlc3RNYXBOb1NvdXJjZVJvb3QgPSB7XG4gICAgdmVyc2lvbjogMyxcbiAgICBmaWxlOiAnbWluLmpzJyxcbiAgICBuYW1lczogWydiYXInLCAnYmF6JywgJ24nXSxcbiAgICBzb3VyY2VzOiBbJ29uZS5qcycsICd0d28uanMnXSxcbiAgICBtYXBwaW5nczogJ0NBQUMsSUFBSSxJQUFNLFNBQVVBLEdBQ2xCLE9BQU9DLElBQUlEO0NDRGIsSUFBSSxJQUFNLFNBQVVFLEdBQ2xCLE9BQU9BJ1xuICB9O1xuICBleHBvcnRzLnRlc3RNYXBFbXB0eVNvdXJjZVJvb3QgPSB7XG4gICAgdmVyc2lvbjogMyxcbiAgICBmaWxlOiAnbWluLmpzJyxcbiAgICBuYW1lczogWydiYXInLCAnYmF6JywgJ24nXSxcbiAgICBzb3VyY2VzOiBbJ29uZS5qcycsICd0d28uanMnXSxcbiAgICBzb3VyY2VSb290OiAnJyxcbiAgICBtYXBwaW5nczogJ0NBQUMsSUFBSSxJQUFNLFNBQVVBLEdBQ2xCLE9BQU9DLElBQUlEO0NDRGIsSUFBSSxJQUFNLFNBQVVFLEdBQ2xCLE9BQU9BJ1xuICB9O1xuICAvLyBUaGlzIG1hcHBpbmcgaXMgaWRlbnRpY2FsIHRvIGFib3ZlLCBidXQgdXNlcyB0aGUgaW5kZXhlZCBmb3JtYXQgaW5zdGVhZC5cbiAgZXhwb3J0cy5pbmRleGVkVGVzdE1hcCA9IHtcbiAgICB2ZXJzaW9uOiAzLFxuICAgIGZpbGU6ICdtaW4uanMnLFxuICAgIHNlY3Rpb25zOiBbXG4gICAgICB7XG4gICAgICAgIG9mZnNldDoge1xuICAgICAgICAgIGxpbmU6IDAsXG4gICAgICAgICAgY29sdW1uOiAwXG4gICAgICAgIH0sXG4gICAgICAgIG1hcDoge1xuICAgICAgICAgIHZlcnNpb246IDMsXG4gICAgICAgICAgc291cmNlczogW1xuICAgICAgICAgICAgXCJvbmUuanNcIlxuICAgICAgICAgIF0sXG4gICAgICAgICAgc291cmNlc0NvbnRlbnQ6IFtcbiAgICAgICAgICAgICcgT05FLmZvbyA9IGZ1bmN0aW9uIChiYXIpIHtcXG4nICtcbiAgICAgICAgICAgICcgICByZXR1cm4gYmF6KGJhcik7XFxuJyArXG4gICAgICAgICAgICAnIH07JyxcbiAgICAgICAgICBdLFxuICAgICAgICAgIG5hbWVzOiBbXG4gICAgICAgICAgICBcImJhclwiLFxuICAgICAgICAgICAgXCJiYXpcIlxuICAgICAgICAgIF0sXG4gICAgICAgICAgbWFwcGluZ3M6IFwiQ0FBQyxJQUFJLElBQU0sU0FBVUEsR0FDbEIsT0FBT0MsSUFBSURcIixcbiAgICAgICAgICBmaWxlOiBcIm1pbi5qc1wiLFxuICAgICAgICAgIHNvdXJjZVJvb3Q6IFwiL3RoZS9yb290XCJcbiAgICAgICAgfVxuICAgICAgfSxcbiAgICAgIHtcbiAgICAgICAgb2Zmc2V0OiB7XG4gICAgICAgICAgbGluZTogMSxcbiAgICAgICAgICBjb2x1bW46IDBcbiAgICAgICAgfSxcbiAgICAgICAgbWFwOiB7XG4gICAgICAgICAgdmVyc2lvbjogMyxcbiAgICAgICAgICBzb3VyY2VzOiBbXG4gICAgICAgICAgICBcInR3by5qc1wiXG4gICAgICAgICAgXSxcbiAgICAgICAgICBzb3VyY2VzQ29udGVudDogW1xuICAgICAgICAgICAgJyBUV08uaW5jID0gZnVuY3Rpb24gKG4pIHtcXG4nICtcbiAgICAgICAgICAgICcgICByZXR1cm4gbiArIDE7XFxuJyArXG4gICAgICAgICAgICAnIH07J1xuICAgICAgICAgIF0sXG4gICAgICAgICAgbmFtZXM6IFtcbiAgICAgICAgICAgIFwiblwiXG4gICAgICAgICAgXSxcbiAgICAgICAgICBtYXBwaW5nczogXCJDQUFDLElBQUksSUFBTSxTQUFVQSxHQUNsQixPQUFPQVwiLFxuICAgICAgICAgIGZpbGU6IFwibWluLmpzXCIsXG4gICAgICAgICAgc291cmNlUm9vdDogXCIvdGhlL3Jvb3RcIlxuICAgICAgICB9XG4gICAgICB9XG4gICAgXVxuICB9O1xuICBleHBvcnRzLmluZGV4ZWRUZXN0TWFwRGlmZmVyZW50U291cmNlUm9vdHMgPSB7XG4gICAgdmVyc2lvbjogMyxcbiAgICBmaWxlOiAnbWluLmpzJyxcbiAgICBzZWN0aW9uczogW1xuICAgICAge1xuICAgICAgICBvZmZzZXQ6IHtcbiAgICAgICAgICBsaW5lOiAwLFxuICAgICAgICAgIGNvbHVtbjogMFxuICAgICAgICB9LFxuICAgICAgICBtYXA6IHtcbiAgICAgICAgICB2ZXJzaW9uOiAzLFxuICAgICAgICAgIHNvdXJjZXM6IFtcbiAgICAgICAgICAgIFwib25lLmpzXCJcbiAgICAgICAgICBdLFxuICAgICAgICAgIHNvdXJjZXNDb250ZW50OiBbXG4gICAgICAgICAgICAnIE9ORS5mb28gPSBmdW5jdGlvbiAoYmFyKSB7XFxuJyArXG4gICAgICAgICAgICAnICAgcmV0dXJuIGJheihiYXIpO1xcbicgK1xuICAgICAgICAgICAgJyB9OycsXG4gICAgICAgICAgXSxcbiAgICAgICAgICBuYW1lczogW1xuICAgICAgICAgICAgXCJiYXJcIixcbiAgICAgICAgICAgIFwiYmF6XCJcbiAgICAgICAgICBdLFxuICAgICAgICAgIG1hcHBpbmdzOiBcIkNBQUMsSUFBSSxJQUFNLFNBQVVBLEdBQ2xCLE9BQU9DLElBQUlEXCIsXG4gICAgICAgICAgZmlsZTogXCJtaW4uanNcIixcbiAgICAgICAgICBzb3VyY2VSb290OiBcIi90aGUvcm9vdFwiXG4gICAgICAgIH1cbiAgICAgIH0sXG4gICAgICB7XG4gICAgICAgIG9mZnNldDoge1xuICAgICAgICAgIGxpbmU6IDEsXG4gICAgICAgICAgY29sdW1uOiAwXG4gICAgICAgIH0sXG4gICAgICAgIG1hcDoge1xuICAgICAgICAgIHZlcnNpb246IDMsXG4gICAgICAgICAgc291cmNlczogW1xuICAgICAgICAgICAgXCJ0d28uanNcIlxuICAgICAgICAgIF0sXG4gICAgICAgICAgc291cmNlc0NvbnRlbnQ6IFtcbiAgICAgICAgICAgICcgVFdPLmluYyA9IGZ1bmN0aW9uIChuKSB7XFxuJyArXG4gICAgICAgICAgICAnICAgcmV0dXJuIG4gKyAxO1xcbicgK1xuICAgICAgICAgICAgJyB9OydcbiAgICAgICAgICBdLFxuICAgICAgICAgIG5hbWVzOiBbXG4gICAgICAgICAgICBcIm5cIlxuICAgICAgICAgIF0sXG4gICAgICAgICAgbWFwcGluZ3M6IFwiQ0FBQyxJQUFJLElBQU0sU0FBVUEsR0FDbEIsT0FBT0FcIixcbiAgICAgICAgICBmaWxlOiBcIm1pbi5qc1wiLFxuICAgICAgICAgIHNvdXJjZVJvb3Q6IFwiL2RpZmZlcmVudC9yb290XCJcbiAgICAgICAgfVxuICAgICAgfVxuICAgIF1cbiAgfTtcbiAgZXhwb3J0cy50ZXN0TWFwV2l0aFNvdXJjZXNDb250ZW50ID0ge1xuICAgIHZlcnNpb246IDMsXG4gICAgZmlsZTogJ21pbi5qcycsXG4gICAgbmFtZXM6IFsnYmFyJywgJ2JheicsICduJ10sXG4gICAgc291cmNlczogWydvbmUuanMnLCAndHdvLmpzJ10sXG4gICAgc291cmNlc0NvbnRlbnQ6IFtcbiAgICAgICcgT05FLmZvbyA9IGZ1bmN0aW9uIChiYXIpIHtcXG4nICtcbiAgICAgICcgICByZXR1cm4gYmF6KGJhcik7XFxuJyArXG4gICAgICAnIH07JyxcbiAgICAgICcgVFdPLmluYyA9IGZ1bmN0aW9uIChuKSB7XFxuJyArXG4gICAgICAnICAgcmV0dXJuIG4gKyAxO1xcbicgK1xuICAgICAgJyB9OydcbiAgICBdLFxuICAgIHNvdXJjZVJvb3Q6ICcvdGhlL3Jvb3QnLFxuICAgIG1hcHBpbmdzOiAnQ0FBQyxJQUFJLElBQU0sU0FBVUEsR0FDbEIsT0FBT0MsSUFBSUQ7Q0NEYixJQUFJLElBQU0sU0FBVUUsR0FDbEIsT0FBT0EnXG4gIH07XG4gIGV4cG9ydHMudGVzdE1hcFJlbGF0aXZlU291cmNlcyA9IHtcbiAgICB2ZXJzaW9uOiAzLFxuICAgIGZpbGU6ICdtaW4uanMnLFxuICAgIG5hbWVzOiBbJ2JhcicsICdiYXonLCAnbiddLFxuICAgIHNvdXJjZXM6IFsnLi9vbmUuanMnLCAnLi90d28uanMnXSxcbiAgICBzb3VyY2VzQ29udGVudDogW1xuICAgICAgJyBPTkUuZm9vID0gZnVuY3Rpb24gKGJhcikge1xcbicgK1xuICAgICAgJyAgIHJldHVybiBiYXooYmFyKTtcXG4nICtcbiAgICAgICcgfTsnLFxuICAgICAgJyBUV08uaW5jID0gZnVuY3Rpb24gKG4pIHtcXG4nICtcbiAgICAgICcgICByZXR1cm4gbiArIDE7XFxuJyArXG4gICAgICAnIH07J1xuICAgIF0sXG4gICAgc291cmNlUm9vdDogJy90aGUvcm9vdCcsXG4gICAgbWFwcGluZ3M6ICdDQUFDLElBQUksSUFBTSxTQUFVQSxHQUNsQixPQUFPQyxJQUFJRDtDQ0RiLElBQUksSUFBTSxTQUFVRSxHQUNsQixPQUFPQSdcbiAgfTtcbiAgZXhwb3J0cy5lbXB0eU1hcCA9IHtcbiAgICB2ZXJzaW9uOiAzLFxuICAgIGZpbGU6ICdtaW4uanMnLFxuICAgIG5hbWVzOiBbXSxcbiAgICBzb3VyY2VzOiBbXSxcbiAgICBtYXBwaW5nczogJydcbiAgfTtcblxuXG4gIGZ1bmN0aW9uIGFzc2VydE1hcHBpbmcoZ2VuZXJhdGVkTGluZSwgZ2VuZXJhdGVkQ29sdW1uLCBvcmlnaW5hbFNvdXJjZSxcbiAgICAgICAgICAgICAgICAgICAgICAgICBvcmlnaW5hbExpbmUsIG9yaWdpbmFsQ29sdW1uLCBuYW1lLCBiaWFzLCBtYXAsIGFzc2VydCxcbiAgICAgICAgICAgICAgICAgICAgICAgICBkb250VGVzdEdlbmVyYXRlZCwgZG9udFRlc3RPcmlnaW5hbCkge1xuICAgIGlmICghZG9udFRlc3RPcmlnaW5hbCkge1xuICAgICAgdmFyIG9yaWdNYXBwaW5nID0gbWFwLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgICAgICBsaW5lOiBnZW5lcmF0ZWRMaW5lLFxuICAgICAgICBjb2x1bW46IGdlbmVyYXRlZENvbHVtbixcbiAgICAgICAgYmlhczogYmlhc1xuICAgICAgfSk7XG4gICAgICBhc3NlcnQuZXF1YWwob3JpZ01hcHBpbmcubmFtZSwgbmFtZSxcbiAgICAgICAgICAgICAgICAgICAnSW5jb3JyZWN0IG5hbWUsIGV4cGVjdGVkICcgKyBKU09OLnN0cmluZ2lmeShuYW1lKVxuICAgICAgICAgICAgICAgICAgICsgJywgZ290ICcgKyBKU09OLnN0cmluZ2lmeShvcmlnTWFwcGluZy5uYW1lKSk7XG4gICAgICBhc3NlcnQuZXF1YWwob3JpZ01hcHBpbmcubGluZSwgb3JpZ2luYWxMaW5lLFxuICAgICAgICAgICAgICAgICAgICdJbmNvcnJlY3QgbGluZSwgZXhwZWN0ZWQgJyArIEpTT04uc3RyaW5naWZ5KG9yaWdpbmFsTGluZSlcbiAgICAgICAgICAgICAgICAgICArICcsIGdvdCAnICsgSlNPTi5zdHJpbmdpZnkob3JpZ01hcHBpbmcubGluZSkpO1xuICAgICAgYXNzZXJ0LmVxdWFsKG9yaWdNYXBwaW5nLmNvbHVtbiwgb3JpZ2luYWxDb2x1bW4sXG4gICAgICAgICAgICAgICAgICAgJ0luY29ycmVjdCBjb2x1bW4sIGV4cGVjdGVkICcgKyBKU09OLnN0cmluZ2lmeShvcmlnaW5hbENvbHVtbilcbiAgICAgICAgICAgICAgICAgICArICcsIGdvdCAnICsgSlNPTi5zdHJpbmdpZnkob3JpZ01hcHBpbmcuY29sdW1uKSk7XG5cbiAgICAgIHZhciBleHBlY3RlZFNvdXJjZTtcblxuICAgICAgaWYgKG9yaWdpbmFsU291cmNlICYmIG1hcC5zb3VyY2VSb290ICYmIG9yaWdpbmFsU291cmNlLmluZGV4T2YobWFwLnNvdXJjZVJvb3QpID09PSAwKSB7XG4gICAgICAgIGV4cGVjdGVkU291cmNlID0gb3JpZ2luYWxTb3VyY2U7XG4gICAgICB9IGVsc2UgaWYgKG9yaWdpbmFsU291cmNlKSB7XG4gICAgICAgIGV4cGVjdGVkU291cmNlID0gbWFwLnNvdXJjZVJvb3RcbiAgICAgICAgICA/IHV0aWwuam9pbihtYXAuc291cmNlUm9vdCwgb3JpZ2luYWxTb3VyY2UpXG4gICAgICAgICAgOiBvcmlnaW5hbFNvdXJjZTtcbiAgICAgIH0gZWxzZSB7XG4gICAgICAgIGV4cGVjdGVkU291cmNlID0gbnVsbDtcbiAgICAgIH1cblxuICAgICAgYXNzZXJ0LmVxdWFsKG9yaWdNYXBwaW5nLnNvdXJjZSwgZXhwZWN0ZWRTb3VyY2UsXG4gICAgICAgICAgICAgICAgICAgJ0luY29ycmVjdCBzb3VyY2UsIGV4cGVjdGVkICcgKyBKU09OLnN0cmluZ2lmeShleHBlY3RlZFNvdXJjZSlcbiAgICAgICAgICAgICAgICAgICArICcsIGdvdCAnICsgSlNPTi5zdHJpbmdpZnkob3JpZ01hcHBpbmcuc291cmNlKSk7XG4gICAgfVxuXG4gICAgaWYgKCFkb250VGVzdEdlbmVyYXRlZCkge1xuICAgICAgdmFyIGdlbk1hcHBpbmcgPSBtYXAuZ2VuZXJhdGVkUG9zaXRpb25Gb3Ioe1xuICAgICAgICBzb3VyY2U6IG9yaWdpbmFsU291cmNlLFxuICAgICAgICBsaW5lOiBvcmlnaW5hbExpbmUsXG4gICAgICAgIGNvbHVtbjogb3JpZ2luYWxDb2x1bW4sXG4gICAgICAgIGJpYXM6IGJpYXNcbiAgICAgIH0pO1xuICAgICAgYXNzZXJ0LmVxdWFsKGdlbk1hcHBpbmcubGluZSwgZ2VuZXJhdGVkTGluZSxcbiAgICAgICAgICAgICAgICAgICAnSW5jb3JyZWN0IGxpbmUsIGV4cGVjdGVkICcgKyBKU09OLnN0cmluZ2lmeShnZW5lcmF0ZWRMaW5lKVxuICAgICAgICAgICAgICAgICAgICsgJywgZ290ICcgKyBKU09OLnN0cmluZ2lmeShnZW5NYXBwaW5nLmxpbmUpKTtcbiAgICAgIGFzc2VydC5lcXVhbChnZW5NYXBwaW5nLmNvbHVtbiwgZ2VuZXJhdGVkQ29sdW1uLFxuICAgICAgICAgICAgICAgICAgICdJbmNvcnJlY3QgY29sdW1uLCBleHBlY3RlZCAnICsgSlNPTi5zdHJpbmdpZnkoZ2VuZXJhdGVkQ29sdW1uKVxuICAgICAgICAgICAgICAgICAgICsgJywgZ290ICcgKyBKU09OLnN0cmluZ2lmeShnZW5NYXBwaW5nLmNvbHVtbikpO1xuICAgIH1cbiAgfVxuICBleHBvcnRzLmFzc2VydE1hcHBpbmcgPSBhc3NlcnRNYXBwaW5nO1xuXG4gIGZ1bmN0aW9uIGFzc2VydEVxdWFsTWFwcyhhc3NlcnQsIGFjdHVhbE1hcCwgZXhwZWN0ZWRNYXApIHtcbiAgICBhc3NlcnQuZXF1YWwoYWN0dWFsTWFwLnZlcnNpb24sIGV4cGVjdGVkTWFwLnZlcnNpb24sIFwidmVyc2lvbiBtaXNtYXRjaFwiKTtcbiAgICBhc3NlcnQuZXF1YWwoYWN0dWFsTWFwLmZpbGUsIGV4cGVjdGVkTWFwLmZpbGUsIFwiZmlsZSBtaXNtYXRjaFwiKTtcbiAgICBhc3NlcnQuZXF1YWwoYWN0dWFsTWFwLm5hbWVzLmxlbmd0aCxcbiAgICAgICAgICAgICAgICAgZXhwZWN0ZWRNYXAubmFtZXMubGVuZ3RoLFxuICAgICAgICAgICAgICAgICBcIm5hbWVzIGxlbmd0aCBtaXNtYXRjaDogXCIgK1xuICAgICAgICAgICAgICAgICAgIGFjdHVhbE1hcC5uYW1lcy5qb2luKFwiLCBcIikgKyBcIiAhPSBcIiArIGV4cGVjdGVkTWFwLm5hbWVzLmpvaW4oXCIsIFwiKSk7XG4gICAgZm9yICh2YXIgaSA9IDA7IGkgPCBhY3R1YWxNYXAubmFtZXMubGVuZ3RoOyBpKyspIHtcbiAgICAgIGFzc2VydC5lcXVhbChhY3R1YWxNYXAubmFtZXNbaV0sXG4gICAgICAgICAgICAgICAgICAgZXhwZWN0ZWRNYXAubmFtZXNbaV0sXG4gICAgICAgICAgICAgICAgICAgXCJuYW1lc1tcIiArIGkgKyBcIl0gbWlzbWF0Y2g6IFwiICtcbiAgICAgICAgICAgICAgICAgICAgIGFjdHVhbE1hcC5uYW1lcy5qb2luKFwiLCBcIikgKyBcIiAhPSBcIiArIGV4cGVjdGVkTWFwLm5hbWVzLmpvaW4oXCIsIFwiKSk7XG4gICAgfVxuICAgIGFzc2VydC5lcXVhbChhY3R1YWxNYXAuc291cmNlcy5sZW5ndGgsXG4gICAgICAgICAgICAgICAgIGV4cGVjdGVkTWFwLnNvdXJjZXMubGVuZ3RoLFxuICAgICAgICAgICAgICAgICBcInNvdXJjZXMgbGVuZ3RoIG1pc21hdGNoOiBcIiArXG4gICAgICAgICAgICAgICAgICAgYWN0dWFsTWFwLnNvdXJjZXMuam9pbihcIiwgXCIpICsgXCIgIT0gXCIgKyBleHBlY3RlZE1hcC5zb3VyY2VzLmpvaW4oXCIsIFwiKSk7XG4gICAgZm9yICh2YXIgaSA9IDA7IGkgPCBhY3R1YWxNYXAuc291cmNlcy5sZW5ndGg7IGkrKykge1xuICAgICAgYXNzZXJ0LmVxdWFsKGFjdHVhbE1hcC5zb3VyY2VzW2ldLFxuICAgICAgICAgICAgICAgICAgIGV4cGVjdGVkTWFwLnNvdXJjZXNbaV0sXG4gICAgICAgICAgICAgICAgICAgXCJzb3VyY2VzW1wiICsgaSArIFwiXSBsZW5ndGggbWlzbWF0Y2g6IFwiICtcbiAgICAgICAgICAgICAgICAgICBhY3R1YWxNYXAuc291cmNlcy5qb2luKFwiLCBcIikgKyBcIiAhPSBcIiArIGV4cGVjdGVkTWFwLnNvdXJjZXMuam9pbihcIiwgXCIpKTtcbiAgICB9XG4gICAgYXNzZXJ0LmVxdWFsKGFjdHVhbE1hcC5zb3VyY2VSb290LFxuICAgICAgICAgICAgICAgICBleHBlY3RlZE1hcC5zb3VyY2VSb290LFxuICAgICAgICAgICAgICAgICBcInNvdXJjZVJvb3QgbWlzbWF0Y2g6IFwiICtcbiAgICAgICAgICAgICAgICAgICBhY3R1YWxNYXAuc291cmNlUm9vdCArIFwiICE9IFwiICsgZXhwZWN0ZWRNYXAuc291cmNlUm9vdCk7XG4gICAgYXNzZXJ0LmVxdWFsKGFjdHVhbE1hcC5tYXBwaW5ncywgZXhwZWN0ZWRNYXAubWFwcGluZ3MsXG4gICAgICAgICAgICAgICAgIFwibWFwcGluZ3MgbWlzbWF0Y2g6XFxuQWN0dWFsOiAgIFwiICsgYWN0dWFsTWFwLm1hcHBpbmdzICsgXCJcXG5FeHBlY3RlZDogXCIgKyBleHBlY3RlZE1hcC5tYXBwaW5ncyk7XG4gICAgaWYgKGFjdHVhbE1hcC5zb3VyY2VzQ29udGVudCkge1xuICAgICAgYXNzZXJ0LmVxdWFsKGFjdHVhbE1hcC5zb3VyY2VzQ29udGVudC5sZW5ndGgsXG4gICAgICAgICAgICAgICAgICAgZXhwZWN0ZWRNYXAuc291cmNlc0NvbnRlbnQubGVuZ3RoLFxuICAgICAgICAgICAgICAgICAgIFwic291cmNlc0NvbnRlbnQgbGVuZ3RoIG1pc21hdGNoXCIpO1xuICAgICAgZm9yICh2YXIgaSA9IDA7IGkgPCBhY3R1YWxNYXAuc291cmNlc0NvbnRlbnQubGVuZ3RoOyBpKyspIHtcbiAgICAgICAgYXNzZXJ0LmVxdWFsKGFjdHVhbE1hcC5zb3VyY2VzQ29udGVudFtpXSxcbiAgICAgICAgICAgICAgICAgICAgIGV4cGVjdGVkTWFwLnNvdXJjZXNDb250ZW50W2ldLFxuICAgICAgICAgICAgICAgICAgICAgXCJzb3VyY2VzQ29udGVudFtcIiArIGkgKyBcIl0gbWlzbWF0Y2hcIik7XG4gICAgICB9XG4gICAgfVxuICB9XG4gIGV4cG9ydHMuYXNzZXJ0RXF1YWxNYXBzID0gYXNzZXJ0RXF1YWxNYXBzO1xufVxuXG5cblxuLyoqKioqKioqKioqKioqKioqXG4gKiogV0VCUEFDSyBGT09URVJcbiAqKiAuL3Rlc3QvdXRpbC5qc1xuICoqIG1vZHVsZSBpZCA9IDFcbiAqKiBtb2R1bGUgY2h1bmtzID0gMFxuICoqLyIsIi8qIC0qLSBNb2RlOiBqczsganMtaW5kZW50LWxldmVsOiAyOyAtKi0gKi9cbi8qXG4gKiBDb3B5cmlnaHQgMjAxMSBNb3ppbGxhIEZvdW5kYXRpb24gYW5kIGNvbnRyaWJ1dG9yc1xuICogTGljZW5zZWQgdW5kZXIgdGhlIE5ldyBCU0QgbGljZW5zZS4gU2VlIExJQ0VOU0Ugb3I6XG4gKiBodHRwOi8vb3BlbnNvdXJjZS5vcmcvbGljZW5zZXMvQlNELTMtQ2xhdXNlXG4gKi9cbntcbiAgLyoqXG4gICAqIFRoaXMgaXMgYSBoZWxwZXIgZnVuY3Rpb24gZm9yIGdldHRpbmcgdmFsdWVzIGZyb20gcGFyYW1ldGVyL29wdGlvbnNcbiAgICogb2JqZWN0cy5cbiAgICpcbiAgICogQHBhcmFtIGFyZ3MgVGhlIG9iamVjdCB3ZSBhcmUgZXh0cmFjdGluZyB2YWx1ZXMgZnJvbVxuICAgKiBAcGFyYW0gbmFtZSBUaGUgbmFtZSBvZiB0aGUgcHJvcGVydHkgd2UgYXJlIGdldHRpbmcuXG4gICAqIEBwYXJhbSBkZWZhdWx0VmFsdWUgQW4gb3B0aW9uYWwgdmFsdWUgdG8gcmV0dXJuIGlmIHRoZSBwcm9wZXJ0eSBpcyBtaXNzaW5nXG4gICAqIGZyb20gdGhlIG9iamVjdC4gSWYgdGhpcyBpcyBub3Qgc3BlY2lmaWVkIGFuZCB0aGUgcHJvcGVydHkgaXMgbWlzc2luZywgYW5cbiAgICogZXJyb3Igd2lsbCBiZSB0aHJvd24uXG4gICAqL1xuICBmdW5jdGlvbiBnZXRBcmcoYUFyZ3MsIGFOYW1lLCBhRGVmYXVsdFZhbHVlKSB7XG4gICAgaWYgKGFOYW1lIGluIGFBcmdzKSB7XG4gICAgICByZXR1cm4gYUFyZ3NbYU5hbWVdO1xuICAgIH0gZWxzZSBpZiAoYXJndW1lbnRzLmxlbmd0aCA9PT0gMykge1xuICAgICAgcmV0dXJuIGFEZWZhdWx0VmFsdWU7XG4gICAgfSBlbHNlIHtcbiAgICAgIHRocm93IG5ldyBFcnJvcignXCInICsgYU5hbWUgKyAnXCIgaXMgYSByZXF1aXJlZCBhcmd1bWVudC4nKTtcbiAgICB9XG4gIH1cbiAgZXhwb3J0cy5nZXRBcmcgPSBnZXRBcmc7XG5cbiAgdmFyIHVybFJlZ2V4cCA9IC9eKD86KFtcXHcrXFwtLl0rKTopP1xcL1xcLyg/OihcXHcrOlxcdyspQCk/KFtcXHcuXSopKD86OihcXGQrKSk/KFxcUyopJC87XG4gIHZhciBkYXRhVXJsUmVnZXhwID0gL15kYXRhOi4rXFwsLiskLztcblxuICBmdW5jdGlvbiB1cmxQYXJzZShhVXJsKSB7XG4gICAgdmFyIG1hdGNoID0gYVVybC5tYXRjaCh1cmxSZWdleHApO1xuICAgIGlmICghbWF0Y2gpIHtcbiAgICAgIHJldHVybiBudWxsO1xuICAgIH1cbiAgICByZXR1cm4ge1xuICAgICAgc2NoZW1lOiBtYXRjaFsxXSxcbiAgICAgIGF1dGg6IG1hdGNoWzJdLFxuICAgICAgaG9zdDogbWF0Y2hbM10sXG4gICAgICBwb3J0OiBtYXRjaFs0XSxcbiAgICAgIHBhdGg6IG1hdGNoWzVdXG4gICAgfTtcbiAgfVxuICBleHBvcnRzLnVybFBhcnNlID0gdXJsUGFyc2U7XG5cbiAgZnVuY3Rpb24gdXJsR2VuZXJhdGUoYVBhcnNlZFVybCkge1xuICAgIHZhciB1cmwgPSAnJztcbiAgICBpZiAoYVBhcnNlZFVybC5zY2hlbWUpIHtcbiAgICAgIHVybCArPSBhUGFyc2VkVXJsLnNjaGVtZSArICc6JztcbiAgICB9XG4gICAgdXJsICs9ICcvLyc7XG4gICAgaWYgKGFQYXJzZWRVcmwuYXV0aCkge1xuICAgICAgdXJsICs9IGFQYXJzZWRVcmwuYXV0aCArICdAJztcbiAgICB9XG4gICAgaWYgKGFQYXJzZWRVcmwuaG9zdCkge1xuICAgICAgdXJsICs9IGFQYXJzZWRVcmwuaG9zdDtcbiAgICB9XG4gICAgaWYgKGFQYXJzZWRVcmwucG9ydCkge1xuICAgICAgdXJsICs9IFwiOlwiICsgYVBhcnNlZFVybC5wb3J0XG4gICAgfVxuICAgIGlmIChhUGFyc2VkVXJsLnBhdGgpIHtcbiAgICAgIHVybCArPSBhUGFyc2VkVXJsLnBhdGg7XG4gICAgfVxuICAgIHJldHVybiB1cmw7XG4gIH1cbiAgZXhwb3J0cy51cmxHZW5lcmF0ZSA9IHVybEdlbmVyYXRlO1xuXG4gIC8qKlxuICAgKiBOb3JtYWxpemVzIGEgcGF0aCwgb3IgdGhlIHBhdGggcG9ydGlvbiBvZiBhIFVSTDpcbiAgICpcbiAgICogLSBSZXBsYWNlcyBjb25zZXF1dGl2ZSBzbGFzaGVzIHdpdGggb25lIHNsYXNoLlxuICAgKiAtIFJlbW92ZXMgdW5uZWNlc3NhcnkgJy4nIHBhcnRzLlxuICAgKiAtIFJlbW92ZXMgdW5uZWNlc3NhcnkgJzxkaXI+Ly4uJyBwYXJ0cy5cbiAgICpcbiAgICogQmFzZWQgb24gY29kZSBpbiB0aGUgTm9kZS5qcyAncGF0aCcgY29yZSBtb2R1bGUuXG4gICAqXG4gICAqIEBwYXJhbSBhUGF0aCBUaGUgcGF0aCBvciB1cmwgdG8gbm9ybWFsaXplLlxuICAgKi9cbiAgZnVuY3Rpb24gbm9ybWFsaXplKGFQYXRoKSB7XG4gICAgdmFyIHBhdGggPSBhUGF0aDtcbiAgICB2YXIgdXJsID0gdXJsUGFyc2UoYVBhdGgpO1xuICAgIGlmICh1cmwpIHtcbiAgICAgIGlmICghdXJsLnBhdGgpIHtcbiAgICAgICAgcmV0dXJuIGFQYXRoO1xuICAgICAgfVxuICAgICAgcGF0aCA9IHVybC5wYXRoO1xuICAgIH1cbiAgICB2YXIgaXNBYnNvbHV0ZSA9IGV4cG9ydHMuaXNBYnNvbHV0ZShwYXRoKTtcblxuICAgIHZhciBwYXJ0cyA9IHBhdGguc3BsaXQoL1xcLysvKTtcbiAgICBmb3IgKHZhciBwYXJ0LCB1cCA9IDAsIGkgPSBwYXJ0cy5sZW5ndGggLSAxOyBpID49IDA7IGktLSkge1xuICAgICAgcGFydCA9IHBhcnRzW2ldO1xuICAgICAgaWYgKHBhcnQgPT09ICcuJykge1xuICAgICAgICBwYXJ0cy5zcGxpY2UoaSwgMSk7XG4gICAgICB9IGVsc2UgaWYgKHBhcnQgPT09ICcuLicpIHtcbiAgICAgICAgdXArKztcbiAgICAgIH0gZWxzZSBpZiAodXAgPiAwKSB7XG4gICAgICAgIGlmIChwYXJ0ID09PSAnJykge1xuICAgICAgICAgIC8vIFRoZSBmaXJzdCBwYXJ0IGlzIGJsYW5rIGlmIHRoZSBwYXRoIGlzIGFic29sdXRlLiBUcnlpbmcgdG8gZ29cbiAgICAgICAgICAvLyBhYm92ZSB0aGUgcm9vdCBpcyBhIG5vLW9wLiBUaGVyZWZvcmUgd2UgY2FuIHJlbW92ZSBhbGwgJy4uJyBwYXJ0c1xuICAgICAgICAgIC8vIGRpcmVjdGx5IGFmdGVyIHRoZSByb290LlxuICAgICAgICAgIHBhcnRzLnNwbGljZShpICsgMSwgdXApO1xuICAgICAgICAgIHVwID0gMDtcbiAgICAgICAgfSBlbHNlIHtcbiAgICAgICAgICBwYXJ0cy5zcGxpY2UoaSwgMik7XG4gICAgICAgICAgdXAtLTtcbiAgICAgICAgfVxuICAgICAgfVxuICAgIH1cbiAgICBwYXRoID0gcGFydHMuam9pbignLycpO1xuXG4gICAgaWYgKHBhdGggPT09ICcnKSB7XG4gICAgICBwYXRoID0gaXNBYnNvbHV0ZSA/ICcvJyA6ICcuJztcbiAgICB9XG5cbiAgICBpZiAodXJsKSB7XG4gICAgICB1cmwucGF0aCA9IHBhdGg7XG4gICAgICByZXR1cm4gdXJsR2VuZXJhdGUodXJsKTtcbiAgICB9XG4gICAgcmV0dXJuIHBhdGg7XG4gIH1cbiAgZXhwb3J0cy5ub3JtYWxpemUgPSBub3JtYWxpemU7XG5cbiAgLyoqXG4gICAqIEpvaW5zIHR3byBwYXRocy9VUkxzLlxuICAgKlxuICAgKiBAcGFyYW0gYVJvb3QgVGhlIHJvb3QgcGF0aCBvciBVUkwuXG4gICAqIEBwYXJhbSBhUGF0aCBUaGUgcGF0aCBvciBVUkwgdG8gYmUgam9pbmVkIHdpdGggdGhlIHJvb3QuXG4gICAqXG4gICAqIC0gSWYgYVBhdGggaXMgYSBVUkwgb3IgYSBkYXRhIFVSSSwgYVBhdGggaXMgcmV0dXJuZWQsIHVubGVzcyBhUGF0aCBpcyBhXG4gICAqICAgc2NoZW1lLXJlbGF0aXZlIFVSTDogVGhlbiB0aGUgc2NoZW1lIG9mIGFSb290LCBpZiBhbnksIGlzIHByZXBlbmRlZFxuICAgKiAgIGZpcnN0LlxuICAgKiAtIE90aGVyd2lzZSBhUGF0aCBpcyBhIHBhdGguIElmIGFSb290IGlzIGEgVVJMLCB0aGVuIGl0cyBwYXRoIHBvcnRpb25cbiAgICogICBpcyB1cGRhdGVkIHdpdGggdGhlIHJlc3VsdCBhbmQgYVJvb3QgaXMgcmV0dXJuZWQuIE90aGVyd2lzZSB0aGUgcmVzdWx0XG4gICAqICAgaXMgcmV0dXJuZWQuXG4gICAqICAgLSBJZiBhUGF0aCBpcyBhYnNvbHV0ZSwgdGhlIHJlc3VsdCBpcyBhUGF0aC5cbiAgICogICAtIE90aGVyd2lzZSB0aGUgdHdvIHBhdGhzIGFyZSBqb2luZWQgd2l0aCBhIHNsYXNoLlxuICAgKiAtIEpvaW5pbmcgZm9yIGV4YW1wbGUgJ2h0dHA6Ly8nIGFuZCAnd3d3LmV4YW1wbGUuY29tJyBpcyBhbHNvIHN1cHBvcnRlZC5cbiAgICovXG4gIGZ1bmN0aW9uIGpvaW4oYVJvb3QsIGFQYXRoKSB7XG4gICAgaWYgKGFSb290ID09PSBcIlwiKSB7XG4gICAgICBhUm9vdCA9IFwiLlwiO1xuICAgIH1cbiAgICBpZiAoYVBhdGggPT09IFwiXCIpIHtcbiAgICAgIGFQYXRoID0gXCIuXCI7XG4gICAgfVxuICAgIHZhciBhUGF0aFVybCA9IHVybFBhcnNlKGFQYXRoKTtcbiAgICB2YXIgYVJvb3RVcmwgPSB1cmxQYXJzZShhUm9vdCk7XG4gICAgaWYgKGFSb290VXJsKSB7XG4gICAgICBhUm9vdCA9IGFSb290VXJsLnBhdGggfHwgJy8nO1xuICAgIH1cblxuICAgIC8vIGBqb2luKGZvbywgJy8vd3d3LmV4YW1wbGUub3JnJylgXG4gICAgaWYgKGFQYXRoVXJsICYmICFhUGF0aFVybC5zY2hlbWUpIHtcbiAgICAgIGlmIChhUm9vdFVybCkge1xuICAgICAgICBhUGF0aFVybC5zY2hlbWUgPSBhUm9vdFVybC5zY2hlbWU7XG4gICAgICB9XG4gICAgICByZXR1cm4gdXJsR2VuZXJhdGUoYVBhdGhVcmwpO1xuICAgIH1cblxuICAgIGlmIChhUGF0aFVybCB8fCBhUGF0aC5tYXRjaChkYXRhVXJsUmVnZXhwKSkge1xuICAgICAgcmV0dXJuIGFQYXRoO1xuICAgIH1cblxuICAgIC8vIGBqb2luKCdodHRwOi8vJywgJ3d3dy5leGFtcGxlLmNvbScpYFxuICAgIGlmIChhUm9vdFVybCAmJiAhYVJvb3RVcmwuaG9zdCAmJiAhYVJvb3RVcmwucGF0aCkge1xuICAgICAgYVJvb3RVcmwuaG9zdCA9IGFQYXRoO1xuICAgICAgcmV0dXJuIHVybEdlbmVyYXRlKGFSb290VXJsKTtcbiAgICB9XG5cbiAgICB2YXIgam9pbmVkID0gYVBhdGguY2hhckF0KDApID09PSAnLydcbiAgICAgID8gYVBhdGhcbiAgICAgIDogbm9ybWFsaXplKGFSb290LnJlcGxhY2UoL1xcLyskLywgJycpICsgJy8nICsgYVBhdGgpO1xuXG4gICAgaWYgKGFSb290VXJsKSB7XG4gICAgICBhUm9vdFVybC5wYXRoID0gam9pbmVkO1xuICAgICAgcmV0dXJuIHVybEdlbmVyYXRlKGFSb290VXJsKTtcbiAgICB9XG4gICAgcmV0dXJuIGpvaW5lZDtcbiAgfVxuICBleHBvcnRzLmpvaW4gPSBqb2luO1xuXG4gIGV4cG9ydHMuaXNBYnNvbHV0ZSA9IGZ1bmN0aW9uIChhUGF0aCkge1xuICAgIHJldHVybiBhUGF0aC5jaGFyQXQoMCkgPT09ICcvJyB8fCAhIWFQYXRoLm1hdGNoKHVybFJlZ2V4cCk7XG4gIH07XG5cbiAgLyoqXG4gICAqIE1ha2UgYSBwYXRoIHJlbGF0aXZlIHRvIGEgVVJMIG9yIGFub3RoZXIgcGF0aC5cbiAgICpcbiAgICogQHBhcmFtIGFSb290IFRoZSByb290IHBhdGggb3IgVVJMLlxuICAgKiBAcGFyYW0gYVBhdGggVGhlIHBhdGggb3IgVVJMIHRvIGJlIG1hZGUgcmVsYXRpdmUgdG8gYVJvb3QuXG4gICAqL1xuICBmdW5jdGlvbiByZWxhdGl2ZShhUm9vdCwgYVBhdGgpIHtcbiAgICBpZiAoYVJvb3QgPT09IFwiXCIpIHtcbiAgICAgIGFSb290ID0gXCIuXCI7XG4gICAgfVxuXG4gICAgYVJvb3QgPSBhUm9vdC5yZXBsYWNlKC9cXC8kLywgJycpO1xuXG4gICAgLy8gSXQgaXMgcG9zc2libGUgZm9yIHRoZSBwYXRoIHRvIGJlIGFib3ZlIHRoZSByb290LiBJbiB0aGlzIGNhc2UsIHNpbXBseVxuICAgIC8vIGNoZWNraW5nIHdoZXRoZXIgdGhlIHJvb3QgaXMgYSBwcmVmaXggb2YgdGhlIHBhdGggd29uJ3Qgd29yay4gSW5zdGVhZCwgd2VcbiAgICAvLyBuZWVkIHRvIHJlbW92ZSBjb21wb25lbnRzIGZyb20gdGhlIHJvb3Qgb25lIGJ5IG9uZSwgdW50aWwgZWl0aGVyIHdlIGZpbmRcbiAgICAvLyBhIHByZWZpeCB0aGF0IGZpdHMsIG9yIHdlIHJ1biBvdXQgb2YgY29tcG9uZW50cyB0byByZW1vdmUuXG4gICAgdmFyIGxldmVsID0gMDtcbiAgICB3aGlsZSAoYVBhdGguaW5kZXhPZihhUm9vdCArICcvJykgIT09IDApIHtcbiAgICAgIHZhciBpbmRleCA9IGFSb290Lmxhc3RJbmRleE9mKFwiL1wiKTtcbiAgICAgIGlmIChpbmRleCA8IDApIHtcbiAgICAgICAgcmV0dXJuIGFQYXRoO1xuICAgICAgfVxuXG4gICAgICAvLyBJZiB0aGUgb25seSBwYXJ0IG9mIHRoZSByb290IHRoYXQgaXMgbGVmdCBpcyB0aGUgc2NoZW1lIChpLmUuIGh0dHA6Ly8sXG4gICAgICAvLyBmaWxlOi8vLywgZXRjLiksIG9uZSBvciBtb3JlIHNsYXNoZXMgKC8pLCBvciBzaW1wbHkgbm90aGluZyBhdCBhbGwsIHdlXG4gICAgICAvLyBoYXZlIGV4aGF1c3RlZCBhbGwgY29tcG9uZW50cywgc28gdGhlIHBhdGggaXMgbm90IHJlbGF0aXZlIHRvIHRoZSByb290LlxuICAgICAgYVJvb3QgPSBhUm9vdC5zbGljZSgwLCBpbmRleCk7XG4gICAgICBpZiAoYVJvb3QubWF0Y2goL14oW15cXC9dKzpcXC8pP1xcLyokLykpIHtcbiAgICAgICAgcmV0dXJuIGFQYXRoO1xuICAgICAgfVxuXG4gICAgICArK2xldmVsO1xuICAgIH1cblxuICAgIC8vIE1ha2Ugc3VyZSB3ZSBhZGQgYSBcIi4uL1wiIGZvciBlYWNoIGNvbXBvbmVudCB3ZSByZW1vdmVkIGZyb20gdGhlIHJvb3QuXG4gICAgcmV0dXJuIEFycmF5KGxldmVsICsgMSkuam9pbihcIi4uL1wiKSArIGFQYXRoLnN1YnN0cihhUm9vdC5sZW5ndGggKyAxKTtcbiAgfVxuICBleHBvcnRzLnJlbGF0aXZlID0gcmVsYXRpdmU7XG5cbiAgLyoqXG4gICAqIEJlY2F1c2UgYmVoYXZpb3IgZ29lcyB3YWNreSB3aGVuIHlvdSBzZXQgYF9fcHJvdG9fX2Agb24gb2JqZWN0cywgd2VcbiAgICogaGF2ZSB0byBwcmVmaXggYWxsIHRoZSBzdHJpbmdzIGluIG91ciBzZXQgd2l0aCBhbiBhcmJpdHJhcnkgY2hhcmFjdGVyLlxuICAgKlxuICAgKiBTZWUgaHR0cHM6Ly9naXRodWIuY29tL21vemlsbGEvc291cmNlLW1hcC9wdWxsLzMxIGFuZFxuICAgKiBodHRwczovL2dpdGh1Yi5jb20vbW96aWxsYS9zb3VyY2UtbWFwL2lzc3Vlcy8zMFxuICAgKlxuICAgKiBAcGFyYW0gU3RyaW5nIGFTdHJcbiAgICovXG4gIGZ1bmN0aW9uIHRvU2V0U3RyaW5nKGFTdHIpIHtcbiAgICByZXR1cm4gJyQnICsgYVN0cjtcbiAgfVxuICBleHBvcnRzLnRvU2V0U3RyaW5nID0gdG9TZXRTdHJpbmc7XG5cbiAgZnVuY3Rpb24gZnJvbVNldFN0cmluZyhhU3RyKSB7XG4gICAgcmV0dXJuIGFTdHIuc3Vic3RyKDEpO1xuICB9XG4gIGV4cG9ydHMuZnJvbVNldFN0cmluZyA9IGZyb21TZXRTdHJpbmc7XG5cbiAgLyoqXG4gICAqIENvbXBhcmF0b3IgYmV0d2VlbiB0d28gbWFwcGluZ3Mgd2hlcmUgdGhlIG9yaWdpbmFsIHBvc2l0aW9ucyBhcmUgY29tcGFyZWQuXG4gICAqXG4gICAqIE9wdGlvbmFsbHkgcGFzcyBpbiBgdHJ1ZWAgYXMgYG9ubHlDb21wYXJlR2VuZXJhdGVkYCB0byBjb25zaWRlciB0d29cbiAgICogbWFwcGluZ3Mgd2l0aCB0aGUgc2FtZSBvcmlnaW5hbCBzb3VyY2UvbGluZS9jb2x1bW4sIGJ1dCBkaWZmZXJlbnQgZ2VuZXJhdGVkXG4gICAqIGxpbmUgYW5kIGNvbHVtbiB0aGUgc2FtZS4gVXNlZnVsIHdoZW4gc2VhcmNoaW5nIGZvciBhIG1hcHBpbmcgd2l0aCBhXG4gICAqIHN0dWJiZWQgb3V0IG1hcHBpbmcuXG4gICAqL1xuICBmdW5jdGlvbiBjb21wYXJlQnlPcmlnaW5hbFBvc2l0aW9ucyhtYXBwaW5nQSwgbWFwcGluZ0IsIG9ubHlDb21wYXJlT3JpZ2luYWwpIHtcbiAgICB2YXIgY21wID0gbWFwcGluZ0Euc291cmNlIC0gbWFwcGluZ0Iuc291cmNlO1xuICAgIGlmIChjbXAgIT09IDApIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgY21wID0gbWFwcGluZ0Eub3JpZ2luYWxMaW5lIC0gbWFwcGluZ0Iub3JpZ2luYWxMaW5lO1xuICAgIGlmIChjbXAgIT09IDApIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgY21wID0gbWFwcGluZ0Eub3JpZ2luYWxDb2x1bW4gLSBtYXBwaW5nQi5vcmlnaW5hbENvbHVtbjtcbiAgICBpZiAoY21wICE9PSAwIHx8IG9ubHlDb21wYXJlT3JpZ2luYWwpIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkQ29sdW1uIC0gbWFwcGluZ0IuZ2VuZXJhdGVkQ29sdW1uO1xuICAgIGlmIChjbXAgIT09IDApIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkTGluZSAtIG1hcHBpbmdCLmdlbmVyYXRlZExpbmU7XG4gICAgaWYgKGNtcCAhPT0gMCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICByZXR1cm4gbWFwcGluZ0EubmFtZSAtIG1hcHBpbmdCLm5hbWU7XG4gIH1cbiAgZXhwb3J0cy5jb21wYXJlQnlPcmlnaW5hbFBvc2l0aW9ucyA9IGNvbXBhcmVCeU9yaWdpbmFsUG9zaXRpb25zO1xuXG4gIC8qKlxuICAgKiBDb21wYXJhdG9yIGJldHdlZW4gdHdvIG1hcHBpbmdzIHdpdGggZGVmbGF0ZWQgc291cmNlIGFuZCBuYW1lIGluZGljZXMgd2hlcmVcbiAgICogdGhlIGdlbmVyYXRlZCBwb3NpdGlvbnMgYXJlIGNvbXBhcmVkLlxuICAgKlxuICAgKiBPcHRpb25hbGx5IHBhc3MgaW4gYHRydWVgIGFzIGBvbmx5Q29tcGFyZUdlbmVyYXRlZGAgdG8gY29uc2lkZXIgdHdvXG4gICAqIG1hcHBpbmdzIHdpdGggdGhlIHNhbWUgZ2VuZXJhdGVkIGxpbmUgYW5kIGNvbHVtbiwgYnV0IGRpZmZlcmVudFxuICAgKiBzb3VyY2UvbmFtZS9vcmlnaW5hbCBsaW5lIGFuZCBjb2x1bW4gdGhlIHNhbWUuIFVzZWZ1bCB3aGVuIHNlYXJjaGluZyBmb3IgYVxuICAgKiBtYXBwaW5nIHdpdGggYSBzdHViYmVkIG91dCBtYXBwaW5nLlxuICAgKi9cbiAgZnVuY3Rpb24gY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zRGVmbGF0ZWQobWFwcGluZ0EsIG1hcHBpbmdCLCBvbmx5Q29tcGFyZUdlbmVyYXRlZCkge1xuICAgIHZhciBjbXAgPSBtYXBwaW5nQS5nZW5lcmF0ZWRMaW5lIC0gbWFwcGluZ0IuZ2VuZXJhdGVkTGluZTtcbiAgICBpZiAoY21wICE9PSAwKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIGNtcCA9IG1hcHBpbmdBLmdlbmVyYXRlZENvbHVtbiAtIG1hcHBpbmdCLmdlbmVyYXRlZENvbHVtbjtcbiAgICBpZiAoY21wICE9PSAwIHx8IG9ubHlDb21wYXJlR2VuZXJhdGVkKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIGNtcCA9IG1hcHBpbmdBLnNvdXJjZSAtIG1hcHBpbmdCLnNvdXJjZTtcbiAgICBpZiAoY21wICE9PSAwKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsTGluZSAtIG1hcHBpbmdCLm9yaWdpbmFsTGluZTtcbiAgICBpZiAoY21wICE9PSAwKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsQ29sdW1uIC0gbWFwcGluZ0Iub3JpZ2luYWxDb2x1bW47XG4gICAgaWYgKGNtcCAhPT0gMCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICByZXR1cm4gbWFwcGluZ0EubmFtZSAtIG1hcHBpbmdCLm5hbWU7XG4gIH1cbiAgZXhwb3J0cy5jb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNEZWZsYXRlZCA9IGNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0RlZmxhdGVkO1xuXG4gIGZ1bmN0aW9uIHN0cmNtcChhU3RyMSwgYVN0cjIpIHtcbiAgICBpZiAoYVN0cjEgPT09IGFTdHIyKSB7XG4gICAgICByZXR1cm4gMDtcbiAgICB9XG5cbiAgICBpZiAoYVN0cjEgPiBhU3RyMikge1xuICAgICAgcmV0dXJuIDE7XG4gICAgfVxuXG4gICAgcmV0dXJuIC0xO1xuICB9XG5cbiAgLyoqXG4gICAqIENvbXBhcmF0b3IgYmV0d2VlbiB0d28gbWFwcGluZ3Mgd2l0aCBpbmZsYXRlZCBzb3VyY2UgYW5kIG5hbWUgc3RyaW5ncyB3aGVyZVxuICAgKiB0aGUgZ2VuZXJhdGVkIHBvc2l0aW9ucyBhcmUgY29tcGFyZWQuXG4gICAqL1xuICBmdW5jdGlvbiBjb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNJbmZsYXRlZChtYXBwaW5nQSwgbWFwcGluZ0IpIHtcbiAgICB2YXIgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkTGluZSAtIG1hcHBpbmdCLmdlbmVyYXRlZExpbmU7XG4gICAgaWYgKGNtcCAhPT0gMCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICBjbXAgPSBtYXBwaW5nQS5nZW5lcmF0ZWRDb2x1bW4gLSBtYXBwaW5nQi5nZW5lcmF0ZWRDb2x1bW47XG4gICAgaWYgKGNtcCAhPT0gMCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICBjbXAgPSBzdHJjbXAobWFwcGluZ0Euc291cmNlLCBtYXBwaW5nQi5zb3VyY2UpO1xuICAgIGlmIChjbXAgIT09IDApIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgY21wID0gbWFwcGluZ0Eub3JpZ2luYWxMaW5lIC0gbWFwcGluZ0Iub3JpZ2luYWxMaW5lO1xuICAgIGlmIChjbXAgIT09IDApIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgY21wID0gbWFwcGluZ0Eub3JpZ2luYWxDb2x1bW4gLSBtYXBwaW5nQi5vcmlnaW5hbENvbHVtbjtcbiAgICBpZiAoY21wICE9PSAwKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIHJldHVybiBzdHJjbXAobWFwcGluZ0EubmFtZSwgbWFwcGluZ0IubmFtZSk7XG4gIH1cbiAgZXhwb3J0cy5jb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNJbmZsYXRlZCA9IGNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0luZmxhdGVkO1xufVxuXG5cblxuLyoqKioqKioqKioqKioqKioqXG4gKiogV0VCUEFDSyBGT09URVJcbiAqKiAuL2xpYi91dGlsLmpzXG4gKiogbW9kdWxlIGlkID0gMlxuICoqIG1vZHVsZSBjaHVua3MgPSAwXG4gKiovIiwiLyogLSotIE1vZGU6IGpzOyBqcy1pbmRlbnQtbGV2ZWw6IDI7IC0qLSAqL1xuLypcbiAqIENvcHlyaWdodCAyMDExIE1vemlsbGEgRm91bmRhdGlvbiBhbmQgY29udHJpYnV0b3JzXG4gKiBMaWNlbnNlZCB1bmRlciB0aGUgTmV3IEJTRCBsaWNlbnNlLiBTZWUgTElDRU5TRSBvcjpcbiAqIGh0dHA6Ly9vcGVuc291cmNlLm9yZy9saWNlbnNlcy9CU0QtMy1DbGF1c2VcbiAqL1xue1xuICB2YXIgdXRpbCA9IHJlcXVpcmUoJy4vdXRpbCcpO1xuICB2YXIgYmluYXJ5U2VhcmNoID0gcmVxdWlyZSgnLi9iaW5hcnktc2VhcmNoJyk7XG4gIHZhciBBcnJheVNldCA9IHJlcXVpcmUoJy4vYXJyYXktc2V0JykuQXJyYXlTZXQ7XG4gIHZhciBiYXNlNjRWTFEgPSByZXF1aXJlKCcuL2Jhc2U2NC12bHEnKTtcbiAgdmFyIHF1aWNrU29ydCA9IHJlcXVpcmUoJy4vcXVpY2stc29ydCcpLnF1aWNrU29ydDtcblxuICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcihhU291cmNlTWFwKSB7XG4gICAgdmFyIHNvdXJjZU1hcCA9IGFTb3VyY2VNYXA7XG4gICAgaWYgKHR5cGVvZiBhU291cmNlTWFwID09PSAnc3RyaW5nJykge1xuICAgICAgc291cmNlTWFwID0gSlNPTi5wYXJzZShhU291cmNlTWFwLnJlcGxhY2UoL15cXClcXF1cXH0nLywgJycpKTtcbiAgICB9XG5cbiAgICByZXR1cm4gc291cmNlTWFwLnNlY3Rpb25zICE9IG51bGxcbiAgICAgID8gbmV3IEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lcihzb3VyY2VNYXApXG4gICAgICA6IG5ldyBCYXNpY1NvdXJjZU1hcENvbnN1bWVyKHNvdXJjZU1hcCk7XG4gIH1cblxuICBTb3VyY2VNYXBDb25zdW1lci5mcm9tU291cmNlTWFwID0gZnVuY3Rpb24oYVNvdXJjZU1hcCkge1xuICAgIHJldHVybiBCYXNpY1NvdXJjZU1hcENvbnN1bWVyLmZyb21Tb3VyY2VNYXAoYVNvdXJjZU1hcCk7XG4gIH1cblxuICAvKipcbiAgICogVGhlIHZlcnNpb24gb2YgdGhlIHNvdXJjZSBtYXBwaW5nIHNwZWMgdGhhdCB3ZSBhcmUgY29uc3VtaW5nLlxuICAgKi9cbiAgU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLl92ZXJzaW9uID0gMztcblxuICAvLyBgX19nZW5lcmF0ZWRNYXBwaW5nc2AgYW5kIGBfX29yaWdpbmFsTWFwcGluZ3NgIGFyZSBhcnJheXMgdGhhdCBob2xkIHRoZVxuICAvLyBwYXJzZWQgbWFwcGluZyBjb29yZGluYXRlcyBmcm9tIHRoZSBzb3VyY2UgbWFwJ3MgXCJtYXBwaW5nc1wiIGF0dHJpYnV0ZS4gVGhleVxuICAvLyBhcmUgbGF6aWx5IGluc3RhbnRpYXRlZCwgYWNjZXNzZWQgdmlhIHRoZSBgX2dlbmVyYXRlZE1hcHBpbmdzYCBhbmRcbiAgLy8gYF9vcmlnaW5hbE1hcHBpbmdzYCBnZXR0ZXJzIHJlc3BlY3RpdmVseSwgYW5kIHdlIG9ubHkgcGFyc2UgdGhlIG1hcHBpbmdzXG4gIC8vIGFuZCBjcmVhdGUgdGhlc2UgYXJyYXlzIG9uY2UgcXVlcmllZCBmb3IgYSBzb3VyY2UgbG9jYXRpb24uIFdlIGp1bXAgdGhyb3VnaFxuICAvLyB0aGVzZSBob29wcyBiZWNhdXNlIHRoZXJlIGNhbiBiZSBtYW55IHRob3VzYW5kcyBvZiBtYXBwaW5ncywgYW5kIHBhcnNpbmdcbiAgLy8gdGhlbSBpcyBleHBlbnNpdmUsIHNvIHdlIG9ubHkgd2FudCB0byBkbyBpdCBpZiB3ZSBtdXN0LlxuICAvL1xuICAvLyBFYWNoIG9iamVjdCBpbiB0aGUgYXJyYXlzIGlzIG9mIHRoZSBmb3JtOlxuICAvL1xuICAvLyAgICAge1xuICAvLyAgICAgICBnZW5lcmF0ZWRMaW5lOiBUaGUgbGluZSBudW1iZXIgaW4gdGhlIGdlbmVyYXRlZCBjb2RlLFxuICAvLyAgICAgICBnZW5lcmF0ZWRDb2x1bW46IFRoZSBjb2x1bW4gbnVtYmVyIGluIHRoZSBnZW5lcmF0ZWQgY29kZSxcbiAgLy8gICAgICAgc291cmNlOiBUaGUgcGF0aCB0byB0aGUgb3JpZ2luYWwgc291cmNlIGZpbGUgdGhhdCBnZW5lcmF0ZWQgdGhpc1xuICAvLyAgICAgICAgICAgICAgIGNodW5rIG9mIGNvZGUsXG4gIC8vICAgICAgIG9yaWdpbmFsTGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBvcmlnaW5hbCBzb3VyY2UgdGhhdFxuICAvLyAgICAgICAgICAgICAgICAgICAgIGNvcnJlc3BvbmRzIHRvIHRoaXMgY2h1bmsgb2YgZ2VuZXJhdGVkIGNvZGUsXG4gIC8vICAgICAgIG9yaWdpbmFsQ29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlIHRoYXRcbiAgLy8gICAgICAgICAgICAgICAgICAgICAgIGNvcnJlc3BvbmRzIHRvIHRoaXMgY2h1bmsgb2YgZ2VuZXJhdGVkIGNvZGUsXG4gIC8vICAgICAgIG5hbWU6IFRoZSBuYW1lIG9mIHRoZSBvcmlnaW5hbCBzeW1ib2wgd2hpY2ggZ2VuZXJhdGVkIHRoaXMgY2h1bmsgb2ZcbiAgLy8gICAgICAgICAgICAgY29kZS5cbiAgLy8gICAgIH1cbiAgLy9cbiAgLy8gQWxsIHByb3BlcnRpZXMgZXhjZXB0IGZvciBgZ2VuZXJhdGVkTGluZWAgYW5kIGBnZW5lcmF0ZWRDb2x1bW5gIGNhbiBiZVxuICAvLyBgbnVsbGAuXG4gIC8vXG4gIC8vIGBfZ2VuZXJhdGVkTWFwcGluZ3NgIGlzIG9yZGVyZWQgYnkgdGhlIGdlbmVyYXRlZCBwb3NpdGlvbnMuXG4gIC8vXG4gIC8vIGBfb3JpZ2luYWxNYXBwaW5nc2AgaXMgb3JkZXJlZCBieSB0aGUgb3JpZ2luYWwgcG9zaXRpb25zLlxuXG4gIFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5fX2dlbmVyYXRlZE1hcHBpbmdzID0gbnVsbDtcbiAgT2JqZWN0LmRlZmluZVByb3BlcnR5KFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZSwgJ19nZW5lcmF0ZWRNYXBwaW5ncycsIHtcbiAgICBnZXQ6IGZ1bmN0aW9uICgpIHtcbiAgICAgIGlmICghdGhpcy5fX2dlbmVyYXRlZE1hcHBpbmdzKSB7XG4gICAgICAgIHRoaXMuX3BhcnNlTWFwcGluZ3ModGhpcy5fbWFwcGluZ3MsIHRoaXMuc291cmNlUm9vdCk7XG4gICAgICB9XG5cbiAgICAgIHJldHVybiB0aGlzLl9fZ2VuZXJhdGVkTWFwcGluZ3M7XG4gICAgfVxuICB9KTtcblxuICBTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuX19vcmlnaW5hbE1hcHBpbmdzID0gbnVsbDtcbiAgT2JqZWN0LmRlZmluZVByb3BlcnR5KFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZSwgJ19vcmlnaW5hbE1hcHBpbmdzJywge1xuICAgIGdldDogZnVuY3Rpb24gKCkge1xuICAgICAgaWYgKCF0aGlzLl9fb3JpZ2luYWxNYXBwaW5ncykge1xuICAgICAgICB0aGlzLl9wYXJzZU1hcHBpbmdzKHRoaXMuX21hcHBpbmdzLCB0aGlzLnNvdXJjZVJvb3QpO1xuICAgICAgfVxuXG4gICAgICByZXR1cm4gdGhpcy5fX29yaWdpbmFsTWFwcGluZ3M7XG4gICAgfVxuICB9KTtcblxuICBTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuX2NoYXJJc01hcHBpbmdTZXBhcmF0b3IgPVxuICAgIGZ1bmN0aW9uIFNvdXJjZU1hcENvbnN1bWVyX2NoYXJJc01hcHBpbmdTZXBhcmF0b3IoYVN0ciwgaW5kZXgpIHtcbiAgICAgIHZhciBjID0gYVN0ci5jaGFyQXQoaW5kZXgpO1xuICAgICAgcmV0dXJuIGMgPT09IFwiO1wiIHx8IGMgPT09IFwiLFwiO1xuICAgIH07XG5cbiAgLyoqXG4gICAqIFBhcnNlIHRoZSBtYXBwaW5ncyBpbiBhIHN0cmluZyBpbiB0byBhIGRhdGEgc3RydWN0dXJlIHdoaWNoIHdlIGNhbiBlYXNpbHlcbiAgICogcXVlcnkgKHRoZSBvcmRlcmVkIGFycmF5cyBpbiB0aGUgYHRoaXMuX19nZW5lcmF0ZWRNYXBwaW5nc2AgYW5kXG4gICAqIGB0aGlzLl9fb3JpZ2luYWxNYXBwaW5nc2AgcHJvcGVydGllcykuXG4gICAqL1xuICBTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuX3BhcnNlTWFwcGluZ3MgPVxuICAgIGZ1bmN0aW9uIFNvdXJjZU1hcENvbnN1bWVyX3BhcnNlTWFwcGluZ3MoYVN0ciwgYVNvdXJjZVJvb3QpIHtcbiAgICAgIHRocm93IG5ldyBFcnJvcihcIlN1YmNsYXNzZXMgbXVzdCBpbXBsZW1lbnQgX3BhcnNlTWFwcGluZ3NcIik7XG4gICAgfTtcblxuICBTb3VyY2VNYXBDb25zdW1lci5HRU5FUkFURURfT1JERVIgPSAxO1xuICBTb3VyY2VNYXBDb25zdW1lci5PUklHSU5BTF9PUkRFUiA9IDI7XG5cbiAgU291cmNlTWFwQ29uc3VtZXIuR1JFQVRFU1RfTE9XRVJfQk9VTkQgPSAxO1xuICBTb3VyY2VNYXBDb25zdW1lci5MRUFTVF9VUFBFUl9CT1VORCA9IDI7XG5cbiAgLyoqXG4gICAqIEl0ZXJhdGUgb3ZlciBlYWNoIG1hcHBpbmcgYmV0d2VlbiBhbiBvcmlnaW5hbCBzb3VyY2UvbGluZS9jb2x1bW4gYW5kIGFcbiAgICogZ2VuZXJhdGVkIGxpbmUvY29sdW1uIGluIHRoaXMgc291cmNlIG1hcC5cbiAgICpcbiAgICogQHBhcmFtIEZ1bmN0aW9uIGFDYWxsYmFja1xuICAgKiAgICAgICAgVGhlIGZ1bmN0aW9uIHRoYXQgaXMgY2FsbGVkIHdpdGggZWFjaCBtYXBwaW5nLlxuICAgKiBAcGFyYW0gT2JqZWN0IGFDb250ZXh0XG4gICAqICAgICAgICBPcHRpb25hbC4gSWYgc3BlY2lmaWVkLCB0aGlzIG9iamVjdCB3aWxsIGJlIHRoZSB2YWx1ZSBvZiBgdGhpc2AgZXZlcnlcbiAgICogICAgICAgIHRpbWUgdGhhdCBgYUNhbGxiYWNrYCBpcyBjYWxsZWQuXG4gICAqIEBwYXJhbSBhT3JkZXJcbiAgICogICAgICAgIEVpdGhlciBgU291cmNlTWFwQ29uc3VtZXIuR0VORVJBVEVEX09SREVSYCBvclxuICAgKiAgICAgICAgYFNvdXJjZU1hcENvbnN1bWVyLk9SSUdJTkFMX09SREVSYC4gU3BlY2lmaWVzIHdoZXRoZXIgeW91IHdhbnQgdG9cbiAgICogICAgICAgIGl0ZXJhdGUgb3ZlciB0aGUgbWFwcGluZ3Mgc29ydGVkIGJ5IHRoZSBnZW5lcmF0ZWQgZmlsZSdzIGxpbmUvY29sdW1uXG4gICAqICAgICAgICBvcmRlciBvciB0aGUgb3JpZ2luYWwncyBzb3VyY2UvbGluZS9jb2x1bW4gb3JkZXIsIHJlc3BlY3RpdmVseS4gRGVmYXVsdHMgdG9cbiAgICogICAgICAgIGBTb3VyY2VNYXBDb25zdW1lci5HRU5FUkFURURfT1JERVJgLlxuICAgKi9cbiAgU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLmVhY2hNYXBwaW5nID1cbiAgICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcl9lYWNoTWFwcGluZyhhQ2FsbGJhY2ssIGFDb250ZXh0LCBhT3JkZXIpIHtcbiAgICAgIHZhciBjb250ZXh0ID0gYUNvbnRleHQgfHwgbnVsbDtcbiAgICAgIHZhciBvcmRlciA9IGFPcmRlciB8fCBTb3VyY2VNYXBDb25zdW1lci5HRU5FUkFURURfT1JERVI7XG5cbiAgICAgIHZhciBtYXBwaW5ncztcbiAgICAgIHN3aXRjaCAob3JkZXIpIHtcbiAgICAgIGNhc2UgU291cmNlTWFwQ29uc3VtZXIuR0VORVJBVEVEX09SREVSOlxuICAgICAgICBtYXBwaW5ncyA9IHRoaXMuX2dlbmVyYXRlZE1hcHBpbmdzO1xuICAgICAgICBicmVhaztcbiAgICAgIGNhc2UgU291cmNlTWFwQ29uc3VtZXIuT1JJR0lOQUxfT1JERVI6XG4gICAgICAgIG1hcHBpbmdzID0gdGhpcy5fb3JpZ2luYWxNYXBwaW5ncztcbiAgICAgICAgYnJlYWs7XG4gICAgICBkZWZhdWx0OlxuICAgICAgICB0aHJvdyBuZXcgRXJyb3IoXCJVbmtub3duIG9yZGVyIG9mIGl0ZXJhdGlvbi5cIik7XG4gICAgICB9XG5cbiAgICAgIHZhciBzb3VyY2VSb290ID0gdGhpcy5zb3VyY2VSb290O1xuICAgICAgbWFwcGluZ3MubWFwKGZ1bmN0aW9uIChtYXBwaW5nKSB7XG4gICAgICAgIHZhciBzb3VyY2UgPSBtYXBwaW5nLnNvdXJjZSA9PT0gbnVsbCA/IG51bGwgOiB0aGlzLl9zb3VyY2VzLmF0KG1hcHBpbmcuc291cmNlKTtcbiAgICAgICAgaWYgKHNvdXJjZSAhPSBudWxsICYmIHNvdXJjZVJvb3QgIT0gbnVsbCkge1xuICAgICAgICAgIHNvdXJjZSA9IHV0aWwuam9pbihzb3VyY2VSb290LCBzb3VyY2UpO1xuICAgICAgICB9XG4gICAgICAgIHJldHVybiB7XG4gICAgICAgICAgc291cmNlOiBzb3VyY2UsXG4gICAgICAgICAgZ2VuZXJhdGVkTGluZTogbWFwcGluZy5nZW5lcmF0ZWRMaW5lLFxuICAgICAgICAgIGdlbmVyYXRlZENvbHVtbjogbWFwcGluZy5nZW5lcmF0ZWRDb2x1bW4sXG4gICAgICAgICAgb3JpZ2luYWxMaW5lOiBtYXBwaW5nLm9yaWdpbmFsTGluZSxcbiAgICAgICAgICBvcmlnaW5hbENvbHVtbjogbWFwcGluZy5vcmlnaW5hbENvbHVtbixcbiAgICAgICAgICBuYW1lOiBtYXBwaW5nLm5hbWUgPT09IG51bGwgPyBudWxsIDogdGhpcy5fbmFtZXMuYXQobWFwcGluZy5uYW1lKVxuICAgICAgICB9O1xuICAgICAgfSwgdGhpcykuZm9yRWFjaChhQ2FsbGJhY2ssIGNvbnRleHQpO1xuICAgIH07XG5cbiAgLyoqXG4gICAqIFJldHVybnMgYWxsIGdlbmVyYXRlZCBsaW5lIGFuZCBjb2x1bW4gaW5mb3JtYXRpb24gZm9yIHRoZSBvcmlnaW5hbCBzb3VyY2UsXG4gICAqIGxpbmUsIGFuZCBjb2x1bW4gcHJvdmlkZWQuIElmIG5vIGNvbHVtbiBpcyBwcm92aWRlZCwgcmV0dXJucyBhbGwgbWFwcGluZ3NcbiAgICogY29ycmVzcG9uZGluZyB0byBhIGVpdGhlciB0aGUgbGluZSB3ZSBhcmUgc2VhcmNoaW5nIGZvciBvciB0aGUgbmV4dFxuICAgKiBjbG9zZXN0IGxpbmUgdGhhdCBoYXMgYW55IG1hcHBpbmdzLiBPdGhlcndpc2UsIHJldHVybnMgYWxsIG1hcHBpbmdzXG4gICAqIGNvcnJlc3BvbmRpbmcgdG8gdGhlIGdpdmVuIGxpbmUgYW5kIGVpdGhlciB0aGUgY29sdW1uIHdlIGFyZSBzZWFyY2hpbmcgZm9yXG4gICAqIG9yIHRoZSBuZXh0IGNsb3Nlc3QgY29sdW1uIHRoYXQgaGFzIGFueSBvZmZzZXRzLlxuICAgKlxuICAgKiBUaGUgb25seSBhcmd1bWVudCBpcyBhbiBvYmplY3Qgd2l0aCB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gICAqXG4gICAqICAgLSBzb3VyY2U6IFRoZSBmaWxlbmFtZSBvZiB0aGUgb3JpZ2luYWwgc291cmNlLlxuICAgKiAgIC0gbGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBvcmlnaW5hbCBzb3VyY2UuXG4gICAqICAgLSBjb2x1bW46IE9wdGlvbmFsLiB0aGUgY29sdW1uIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLlxuICAgKlxuICAgKiBhbmQgYW4gYXJyYXkgb2Ygb2JqZWN0cyBpcyByZXR1cm5lZCwgZWFjaCB3aXRoIHRoZSBmb2xsb3dpbmcgcHJvcGVydGllczpcbiAgICpcbiAgICogICAtIGxpbmU6IFRoZSBsaW5lIG51bWJlciBpbiB0aGUgZ2VuZXJhdGVkIHNvdXJjZSwgb3IgbnVsbC5cbiAgICogICAtIGNvbHVtbjogVGhlIGNvbHVtbiBudW1iZXIgaW4gdGhlIGdlbmVyYXRlZCBzb3VyY2UsIG9yIG51bGwuXG4gICAqL1xuICBTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuYWxsR2VuZXJhdGVkUG9zaXRpb25zRm9yID1cbiAgICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcl9hbGxHZW5lcmF0ZWRQb3NpdGlvbnNGb3IoYUFyZ3MpIHtcbiAgICAgIHZhciBsaW5lID0gdXRpbC5nZXRBcmcoYUFyZ3MsICdsaW5lJyk7XG5cbiAgICAgIC8vIFdoZW4gdGhlcmUgaXMgbm8gZXhhY3QgbWF0Y2gsIEJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLl9maW5kTWFwcGluZ1xuICAgICAgLy8gcmV0dXJucyB0aGUgaW5kZXggb2YgdGhlIGNsb3Nlc3QgbWFwcGluZyBsZXNzIHRoYW4gdGhlIG5lZWRsZS4gQnlcbiAgICAgIC8vIHNldHRpbmcgbmVlZGxlLm9yaWdpbmFsQ29sdW1uIHRvIDAsIHdlIHRodXMgZmluZCB0aGUgbGFzdCBtYXBwaW5nIGZvclxuICAgICAgLy8gdGhlIGdpdmVuIGxpbmUsIHByb3ZpZGVkIHN1Y2ggYSBtYXBwaW5nIGV4aXN0cy5cbiAgICAgIHZhciBuZWVkbGUgPSB7XG4gICAgICAgIHNvdXJjZTogdXRpbC5nZXRBcmcoYUFyZ3MsICdzb3VyY2UnKSxcbiAgICAgICAgb3JpZ2luYWxMaW5lOiBsaW5lLFxuICAgICAgICBvcmlnaW5hbENvbHVtbjogdXRpbC5nZXRBcmcoYUFyZ3MsICdjb2x1bW4nLCAwKVxuICAgICAgfTtcblxuICAgICAgaWYgKHRoaXMuc291cmNlUm9vdCAhPSBudWxsKSB7XG4gICAgICAgIG5lZWRsZS5zb3VyY2UgPSB1dGlsLnJlbGF0aXZlKHRoaXMuc291cmNlUm9vdCwgbmVlZGxlLnNvdXJjZSk7XG4gICAgICB9XG4gICAgICBpZiAoIXRoaXMuX3NvdXJjZXMuaGFzKG5lZWRsZS5zb3VyY2UpKSB7XG4gICAgICAgIHJldHVybiBbXTtcbiAgICAgIH1cbiAgICAgIG5lZWRsZS5zb3VyY2UgPSB0aGlzLl9zb3VyY2VzLmluZGV4T2YobmVlZGxlLnNvdXJjZSk7XG5cbiAgICAgIHZhciBtYXBwaW5ncyA9IFtdO1xuXG4gICAgICB2YXIgaW5kZXggPSB0aGlzLl9maW5kTWFwcGluZyhuZWVkbGUsXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICB0aGlzLl9vcmlnaW5hbE1hcHBpbmdzLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgXCJvcmlnaW5hbExpbmVcIixcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIFwib3JpZ2luYWxDb2x1bW5cIixcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIHV0aWwuY29tcGFyZUJ5T3JpZ2luYWxQb3NpdGlvbnMsXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICBiaW5hcnlTZWFyY2guTEVBU1RfVVBQRVJfQk9VTkQpO1xuICAgICAgaWYgKGluZGV4ID49IDApIHtcbiAgICAgICAgdmFyIG1hcHBpbmcgPSB0aGlzLl9vcmlnaW5hbE1hcHBpbmdzW2luZGV4XTtcblxuICAgICAgICBpZiAoYUFyZ3MuY29sdW1uID09PSB1bmRlZmluZWQpIHtcbiAgICAgICAgICB2YXIgb3JpZ2luYWxMaW5lID0gbWFwcGluZy5vcmlnaW5hbExpbmU7XG5cbiAgICAgICAgICAvLyBJdGVyYXRlIHVudGlsIGVpdGhlciB3ZSBydW4gb3V0IG9mIG1hcHBpbmdzLCBvciB3ZSBydW4gaW50b1xuICAgICAgICAgIC8vIGEgbWFwcGluZyBmb3IgYSBkaWZmZXJlbnQgbGluZSB0aGFuIHRoZSBvbmUgd2UgZm91bmQuIFNpbmNlXG4gICAgICAgICAgLy8gbWFwcGluZ3MgYXJlIHNvcnRlZCwgdGhpcyBpcyBndWFyYW50ZWVkIHRvIGZpbmQgYWxsIG1hcHBpbmdzIGZvclxuICAgICAgICAgIC8vIHRoZSBsaW5lIHdlIGZvdW5kLlxuICAgICAgICAgIHdoaWxlIChtYXBwaW5nICYmIG1hcHBpbmcub3JpZ2luYWxMaW5lID09PSBvcmlnaW5hbExpbmUpIHtcbiAgICAgICAgICAgIG1hcHBpbmdzLnB1c2goe1xuICAgICAgICAgICAgICBsaW5lOiB1dGlsLmdldEFyZyhtYXBwaW5nLCAnZ2VuZXJhdGVkTGluZScsIG51bGwpLFxuICAgICAgICAgICAgICBjb2x1bW46IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICdnZW5lcmF0ZWRDb2x1bW4nLCBudWxsKSxcbiAgICAgICAgICAgICAgbGFzdENvbHVtbjogdXRpbC5nZXRBcmcobWFwcGluZywgJ2xhc3RHZW5lcmF0ZWRDb2x1bW4nLCBudWxsKVxuICAgICAgICAgICAgfSk7XG5cbiAgICAgICAgICAgIG1hcHBpbmcgPSB0aGlzLl9vcmlnaW5hbE1hcHBpbmdzWysraW5kZXhdO1xuICAgICAgICAgIH1cbiAgICAgICAgfSBlbHNlIHtcbiAgICAgICAgICB2YXIgb3JpZ2luYWxDb2x1bW4gPSBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uO1xuXG4gICAgICAgICAgLy8gSXRlcmF0ZSB1bnRpbCBlaXRoZXIgd2UgcnVuIG91dCBvZiBtYXBwaW5ncywgb3Igd2UgcnVuIGludG9cbiAgICAgICAgICAvLyBhIG1hcHBpbmcgZm9yIGEgZGlmZmVyZW50IGxpbmUgdGhhbiB0aGUgb25lIHdlIHdlcmUgc2VhcmNoaW5nIGZvci5cbiAgICAgICAgICAvLyBTaW5jZSBtYXBwaW5ncyBhcmUgc29ydGVkLCB0aGlzIGlzIGd1YXJhbnRlZWQgdG8gZmluZCBhbGwgbWFwcGluZ3MgZm9yXG4gICAgICAgICAgLy8gdGhlIGxpbmUgd2UgYXJlIHNlYXJjaGluZyBmb3IuXG4gICAgICAgICAgd2hpbGUgKG1hcHBpbmcgJiZcbiAgICAgICAgICAgICAgICAgbWFwcGluZy5vcmlnaW5hbExpbmUgPT09IGxpbmUgJiZcbiAgICAgICAgICAgICAgICAgbWFwcGluZy5vcmlnaW5hbENvbHVtbiA9PSBvcmlnaW5hbENvbHVtbikge1xuICAgICAgICAgICAgbWFwcGluZ3MucHVzaCh7XG4gICAgICAgICAgICAgIGxpbmU6IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICdnZW5lcmF0ZWRMaW5lJywgbnVsbCksXG4gICAgICAgICAgICAgIGNvbHVtbjogdXRpbC5nZXRBcmcobWFwcGluZywgJ2dlbmVyYXRlZENvbHVtbicsIG51bGwpLFxuICAgICAgICAgICAgICBsYXN0Q29sdW1uOiB1dGlsLmdldEFyZyhtYXBwaW5nLCAnbGFzdEdlbmVyYXRlZENvbHVtbicsIG51bGwpXG4gICAgICAgICAgICB9KTtcblxuICAgICAgICAgICAgbWFwcGluZyA9IHRoaXMuX29yaWdpbmFsTWFwcGluZ3NbKytpbmRleF07XG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICB9XG5cbiAgICAgIHJldHVybiBtYXBwaW5ncztcbiAgICB9O1xuXG4gIGV4cG9ydHMuU291cmNlTWFwQ29uc3VtZXIgPSBTb3VyY2VNYXBDb25zdW1lcjtcblxuICAvKipcbiAgICogQSBCYXNpY1NvdXJjZU1hcENvbnN1bWVyIGluc3RhbmNlIHJlcHJlc2VudHMgYSBwYXJzZWQgc291cmNlIG1hcCB3aGljaCB3ZSBjYW5cbiAgICogcXVlcnkgZm9yIGluZm9ybWF0aW9uIGFib3V0IHRoZSBvcmlnaW5hbCBmaWxlIHBvc2l0aW9ucyBieSBnaXZpbmcgaXQgYSBmaWxlXG4gICAqIHBvc2l0aW9uIGluIHRoZSBnZW5lcmF0ZWQgc291cmNlLlxuICAgKlxuICAgKiBUaGUgb25seSBwYXJhbWV0ZXIgaXMgdGhlIHJhdyBzb3VyY2UgbWFwIChlaXRoZXIgYXMgYSBKU09OIHN0cmluZywgb3JcbiAgICogYWxyZWFkeSBwYXJzZWQgdG8gYW4gb2JqZWN0KS4gQWNjb3JkaW5nIHRvIHRoZSBzcGVjLCBzb3VyY2UgbWFwcyBoYXZlIHRoZVxuICAgKiBmb2xsb3dpbmcgYXR0cmlidXRlczpcbiAgICpcbiAgICogICAtIHZlcnNpb246IFdoaWNoIHZlcnNpb24gb2YgdGhlIHNvdXJjZSBtYXAgc3BlYyB0aGlzIG1hcCBpcyBmb2xsb3dpbmcuXG4gICAqICAgLSBzb3VyY2VzOiBBbiBhcnJheSBvZiBVUkxzIHRvIHRoZSBvcmlnaW5hbCBzb3VyY2UgZmlsZXMuXG4gICAqICAgLSBuYW1lczogQW4gYXJyYXkgb2YgaWRlbnRpZmllcnMgd2hpY2ggY2FuIGJlIHJlZmVycmVuY2VkIGJ5IGluZGl2aWR1YWwgbWFwcGluZ3MuXG4gICAqICAgLSBzb3VyY2VSb290OiBPcHRpb25hbC4gVGhlIFVSTCByb290IGZyb20gd2hpY2ggYWxsIHNvdXJjZXMgYXJlIHJlbGF0aXZlLlxuICAgKiAgIC0gc291cmNlc0NvbnRlbnQ6IE9wdGlvbmFsLiBBbiBhcnJheSBvZiBjb250ZW50cyBvZiB0aGUgb3JpZ2luYWwgc291cmNlIGZpbGVzLlxuICAgKiAgIC0gbWFwcGluZ3M6IEEgc3RyaW5nIG9mIGJhc2U2NCBWTFFzIHdoaWNoIGNvbnRhaW4gdGhlIGFjdHVhbCBtYXBwaW5ncy5cbiAgICogICAtIGZpbGU6IE9wdGlvbmFsLiBUaGUgZ2VuZXJhdGVkIGZpbGUgdGhpcyBzb3VyY2UgbWFwIGlzIGFzc29jaWF0ZWQgd2l0aC5cbiAgICpcbiAgICogSGVyZSBpcyBhbiBleGFtcGxlIHNvdXJjZSBtYXAsIHRha2VuIGZyb20gdGhlIHNvdXJjZSBtYXAgc3BlY1swXTpcbiAgICpcbiAgICogICAgIHtcbiAgICogICAgICAgdmVyc2lvbiA6IDMsXG4gICAqICAgICAgIGZpbGU6IFwib3V0LmpzXCIsXG4gICAqICAgICAgIHNvdXJjZVJvb3QgOiBcIlwiLFxuICAgKiAgICAgICBzb3VyY2VzOiBbXCJmb28uanNcIiwgXCJiYXIuanNcIl0sXG4gICAqICAgICAgIG5hbWVzOiBbXCJzcmNcIiwgXCJtYXBzXCIsIFwiYXJlXCIsIFwiZnVuXCJdLFxuICAgKiAgICAgICBtYXBwaW5nczogXCJBQSxBQjs7QUJDREU7XCJcbiAgICogICAgIH1cbiAgICpcbiAgICogWzBdOiBodHRwczovL2RvY3MuZ29vZ2xlLmNvbS9kb2N1bWVudC9kLzFVMVJHQWVoUXdSeXBVVG92RjFLUmxwaU9GemUwYi1fMmdjNmZBSDBLWTBrL2VkaXQ/cGxpPTEjXG4gICAqL1xuICBmdW5jdGlvbiBCYXNpY1NvdXJjZU1hcENvbnN1bWVyKGFTb3VyY2VNYXApIHtcbiAgICB2YXIgc291cmNlTWFwID0gYVNvdXJjZU1hcDtcbiAgICBpZiAodHlwZW9mIGFTb3VyY2VNYXAgPT09ICdzdHJpbmcnKSB7XG4gICAgICBzb3VyY2VNYXAgPSBKU09OLnBhcnNlKGFTb3VyY2VNYXAucmVwbGFjZSgvXlxcKVxcXVxcfScvLCAnJykpO1xuICAgIH1cblxuICAgIHZhciB2ZXJzaW9uID0gdXRpbC5nZXRBcmcoc291cmNlTWFwLCAndmVyc2lvbicpO1xuICAgIHZhciBzb3VyY2VzID0gdXRpbC5nZXRBcmcoc291cmNlTWFwLCAnc291cmNlcycpO1xuICAgIC8vIFNhc3MgMy4zIGxlYXZlcyBvdXQgdGhlICduYW1lcycgYXJyYXksIHNvIHdlIGRldmlhdGUgZnJvbSB0aGUgc3BlYyAod2hpY2hcbiAgICAvLyByZXF1aXJlcyB0aGUgYXJyYXkpIHRvIHBsYXkgbmljZSBoZXJlLlxuICAgIHZhciBuYW1lcyA9IHV0aWwuZ2V0QXJnKHNvdXJjZU1hcCwgJ25hbWVzJywgW10pO1xuICAgIHZhciBzb3VyY2VSb290ID0gdXRpbC5nZXRBcmcoc291cmNlTWFwLCAnc291cmNlUm9vdCcsIG51bGwpO1xuICAgIHZhciBzb3VyY2VzQ29udGVudCA9IHV0aWwuZ2V0QXJnKHNvdXJjZU1hcCwgJ3NvdXJjZXNDb250ZW50JywgbnVsbCk7XG4gICAgdmFyIG1hcHBpbmdzID0gdXRpbC5nZXRBcmcoc291cmNlTWFwLCAnbWFwcGluZ3MnKTtcbiAgICB2YXIgZmlsZSA9IHV0aWwuZ2V0QXJnKHNvdXJjZU1hcCwgJ2ZpbGUnLCBudWxsKTtcblxuICAgIC8vIE9uY2UgYWdhaW4sIFNhc3MgZGV2aWF0ZXMgZnJvbSB0aGUgc3BlYyBhbmQgc3VwcGxpZXMgdGhlIHZlcnNpb24gYXMgYVxuICAgIC8vIHN0cmluZyByYXRoZXIgdGhhbiBhIG51bWJlciwgc28gd2UgdXNlIGxvb3NlIGVxdWFsaXR5IGNoZWNraW5nIGhlcmUuXG4gICAgaWYgKHZlcnNpb24gIT0gdGhpcy5fdmVyc2lvbikge1xuICAgICAgdGhyb3cgbmV3IEVycm9yKCdVbnN1cHBvcnRlZCB2ZXJzaW9uOiAnICsgdmVyc2lvbik7XG4gICAgfVxuXG4gICAgc291cmNlcyA9IHNvdXJjZXNcbiAgICAgIC8vIFNvbWUgc291cmNlIG1hcHMgcHJvZHVjZSByZWxhdGl2ZSBzb3VyY2UgcGF0aHMgbGlrZSBcIi4vZm9vLmpzXCIgaW5zdGVhZCBvZlxuICAgICAgLy8gXCJmb28uanNcIi4gIE5vcm1hbGl6ZSB0aGVzZSBmaXJzdCBzbyB0aGF0IGZ1dHVyZSBjb21wYXJpc29ucyB3aWxsIHN1Y2NlZWQuXG4gICAgICAvLyBTZWUgYnVnemlsLmxhLzEwOTA3NjguXG4gICAgICAubWFwKHV0aWwubm9ybWFsaXplKVxuICAgICAgLy8gQWx3YXlzIGVuc3VyZSB0aGF0IGFic29sdXRlIHNvdXJjZXMgYXJlIGludGVybmFsbHkgc3RvcmVkIHJlbGF0aXZlIHRvXG4gICAgICAvLyB0aGUgc291cmNlIHJvb3QsIGlmIHRoZSBzb3VyY2Ugcm9vdCBpcyBhYnNvbHV0ZS4gTm90IGRvaW5nIHRoaXMgd291bGRcbiAgICAgIC8vIGJlIHBhcnRpY3VsYXJseSBwcm9ibGVtYXRpYyB3aGVuIHRoZSBzb3VyY2Ugcm9vdCBpcyBhIHByZWZpeCBvZiB0aGVcbiAgICAgIC8vIHNvdXJjZSAodmFsaWQsIGJ1dCB3aHk/PykuIFNlZSBnaXRodWIgaXNzdWUgIzE5OSBhbmQgYnVnemlsLmxhLzExODg5ODIuXG4gICAgICAubWFwKGZ1bmN0aW9uIChzb3VyY2UpIHtcbiAgICAgICAgcmV0dXJuIHNvdXJjZVJvb3QgJiYgdXRpbC5pc0Fic29sdXRlKHNvdXJjZVJvb3QpICYmIHV0aWwuaXNBYnNvbHV0ZShzb3VyY2UpXG4gICAgICAgICAgPyB1dGlsLnJlbGF0aXZlKHNvdXJjZVJvb3QsIHNvdXJjZSlcbiAgICAgICAgICA6IHNvdXJjZTtcbiAgICAgIH0pO1xuXG4gICAgLy8gUGFzcyBgdHJ1ZWAgYmVsb3cgdG8gYWxsb3cgZHVwbGljYXRlIG5hbWVzIGFuZCBzb3VyY2VzLiBXaGlsZSBzb3VyY2UgbWFwc1xuICAgIC8vIGFyZSBpbnRlbmRlZCB0byBiZSBjb21wcmVzc2VkIGFuZCBkZWR1cGxpY2F0ZWQsIHRoZSBUeXBlU2NyaXB0IGNvbXBpbGVyXG4gICAgLy8gc29tZXRpbWVzIGdlbmVyYXRlcyBzb3VyY2UgbWFwcyB3aXRoIGR1cGxpY2F0ZXMgaW4gdGhlbS4gU2VlIEdpdGh1YiBpc3N1ZVxuICAgIC8vICM3MiBhbmQgYnVnemlsLmxhLzg4OTQ5Mi5cbiAgICB0aGlzLl9uYW1lcyA9IEFycmF5U2V0LmZyb21BcnJheShuYW1lcywgdHJ1ZSk7XG4gICAgdGhpcy5fc291cmNlcyA9IEFycmF5U2V0LmZyb21BcnJheShzb3VyY2VzLCB0cnVlKTtcblxuICAgIHRoaXMuc291cmNlUm9vdCA9IHNvdXJjZVJvb3Q7XG4gICAgdGhpcy5zb3VyY2VzQ29udGVudCA9IHNvdXJjZXNDb250ZW50O1xuICAgIHRoaXMuX21hcHBpbmdzID0gbWFwcGluZ3M7XG4gICAgdGhpcy5maWxlID0gZmlsZTtcbiAgfVxuXG4gIEJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlID0gT2JqZWN0LmNyZWF0ZShTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUpO1xuICBCYXNpY1NvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5jb25zdW1lciA9IFNvdXJjZU1hcENvbnN1bWVyO1xuXG4gIC8qKlxuICAgKiBDcmVhdGUgYSBCYXNpY1NvdXJjZU1hcENvbnN1bWVyIGZyb20gYSBTb3VyY2VNYXBHZW5lcmF0b3IuXG4gICAqXG4gICAqIEBwYXJhbSBTb3VyY2VNYXBHZW5lcmF0b3IgYVNvdXJjZU1hcFxuICAgKiAgICAgICAgVGhlIHNvdXJjZSBtYXAgdGhhdCB3aWxsIGJlIGNvbnN1bWVkLlxuICAgKiBAcmV0dXJucyBCYXNpY1NvdXJjZU1hcENvbnN1bWVyXG4gICAqL1xuICBCYXNpY1NvdXJjZU1hcENvbnN1bWVyLmZyb21Tb3VyY2VNYXAgPVxuICAgIGZ1bmN0aW9uIFNvdXJjZU1hcENvbnN1bWVyX2Zyb21Tb3VyY2VNYXAoYVNvdXJjZU1hcCkge1xuICAgICAgdmFyIHNtYyA9IE9iamVjdC5jcmVhdGUoQmFzaWNTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUpO1xuXG4gICAgICB2YXIgbmFtZXMgPSBzbWMuX25hbWVzID0gQXJyYXlTZXQuZnJvbUFycmF5KGFTb3VyY2VNYXAuX25hbWVzLnRvQXJyYXkoKSwgdHJ1ZSk7XG4gICAgICB2YXIgc291cmNlcyA9IHNtYy5fc291cmNlcyA9IEFycmF5U2V0LmZyb21BcnJheShhU291cmNlTWFwLl9zb3VyY2VzLnRvQXJyYXkoKSwgdHJ1ZSk7XG4gICAgICBzbWMuc291cmNlUm9vdCA9IGFTb3VyY2VNYXAuX3NvdXJjZVJvb3Q7XG4gICAgICBzbWMuc291cmNlc0NvbnRlbnQgPSBhU291cmNlTWFwLl9nZW5lcmF0ZVNvdXJjZXNDb250ZW50KHNtYy5fc291cmNlcy50b0FycmF5KCksXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIHNtYy5zb3VyY2VSb290KTtcbiAgICAgIHNtYy5maWxlID0gYVNvdXJjZU1hcC5fZmlsZTtcblxuICAgICAgLy8gQmVjYXVzZSB3ZSBhcmUgbW9kaWZ5aW5nIHRoZSBlbnRyaWVzIChieSBjb252ZXJ0aW5nIHN0cmluZyBzb3VyY2VzIGFuZFxuICAgICAgLy8gbmFtZXMgdG8gaW5kaWNlcyBpbnRvIHRoZSBzb3VyY2VzIGFuZCBuYW1lcyBBcnJheVNldHMpLCB3ZSBoYXZlIHRvIG1ha2VcbiAgICAgIC8vIGEgY29weSBvZiB0aGUgZW50cnkgb3IgZWxzZSBiYWQgdGhpbmdzIGhhcHBlbi4gU2hhcmVkIG11dGFibGUgc3RhdGVcbiAgICAgIC8vIHN0cmlrZXMgYWdhaW4hIFNlZSBnaXRodWIgaXNzdWUgIzE5MS5cblxuICAgICAgdmFyIGdlbmVyYXRlZE1hcHBpbmdzID0gYVNvdXJjZU1hcC5fbWFwcGluZ3MudG9BcnJheSgpLnNsaWNlKCk7XG4gICAgICB2YXIgZGVzdEdlbmVyYXRlZE1hcHBpbmdzID0gc21jLl9fZ2VuZXJhdGVkTWFwcGluZ3MgPSBbXTtcbiAgICAgIHZhciBkZXN0T3JpZ2luYWxNYXBwaW5ncyA9IHNtYy5fX29yaWdpbmFsTWFwcGluZ3MgPSBbXTtcblxuICAgICAgZm9yICh2YXIgaSA9IDAsIGxlbmd0aCA9IGdlbmVyYXRlZE1hcHBpbmdzLmxlbmd0aDsgaSA8IGxlbmd0aDsgaSsrKSB7XG4gICAgICAgIHZhciBzcmNNYXBwaW5nID0gZ2VuZXJhdGVkTWFwcGluZ3NbaV07XG4gICAgICAgIHZhciBkZXN0TWFwcGluZyA9IG5ldyBNYXBwaW5nO1xuICAgICAgICBkZXN0TWFwcGluZy5nZW5lcmF0ZWRMaW5lID0gc3JjTWFwcGluZy5nZW5lcmF0ZWRMaW5lO1xuICAgICAgICBkZXN0TWFwcGluZy5nZW5lcmF0ZWRDb2x1bW4gPSBzcmNNYXBwaW5nLmdlbmVyYXRlZENvbHVtbjtcblxuICAgICAgICBpZiAoc3JjTWFwcGluZy5zb3VyY2UpIHtcbiAgICAgICAgICBkZXN0TWFwcGluZy5zb3VyY2UgPSBzb3VyY2VzLmluZGV4T2Yoc3JjTWFwcGluZy5zb3VyY2UpO1xuICAgICAgICAgIGRlc3RNYXBwaW5nLm9yaWdpbmFsTGluZSA9IHNyY01hcHBpbmcub3JpZ2luYWxMaW5lO1xuICAgICAgICAgIGRlc3RNYXBwaW5nLm9yaWdpbmFsQ29sdW1uID0gc3JjTWFwcGluZy5vcmlnaW5hbENvbHVtbjtcblxuICAgICAgICAgIGlmIChzcmNNYXBwaW5nLm5hbWUpIHtcbiAgICAgICAgICAgIGRlc3RNYXBwaW5nLm5hbWUgPSBuYW1lcy5pbmRleE9mKHNyY01hcHBpbmcubmFtZSk7XG4gICAgICAgICAgfVxuXG4gICAgICAgICAgZGVzdE9yaWdpbmFsTWFwcGluZ3MucHVzaChkZXN0TWFwcGluZyk7XG4gICAgICAgIH1cblxuICAgICAgICBkZXN0R2VuZXJhdGVkTWFwcGluZ3MucHVzaChkZXN0TWFwcGluZyk7XG4gICAgICB9XG5cbiAgICAgIHF1aWNrU29ydChzbWMuX19vcmlnaW5hbE1hcHBpbmdzLCB1dGlsLmNvbXBhcmVCeU9yaWdpbmFsUG9zaXRpb25zKTtcblxuICAgICAgcmV0dXJuIHNtYztcbiAgICB9O1xuXG4gIC8qKlxuICAgKiBUaGUgdmVyc2lvbiBvZiB0aGUgc291cmNlIG1hcHBpbmcgc3BlYyB0aGF0IHdlIGFyZSBjb25zdW1pbmcuXG4gICAqL1xuICBCYXNpY1NvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5fdmVyc2lvbiA9IDM7XG5cbiAgLyoqXG4gICAqIFRoZSBsaXN0IG9mIG9yaWdpbmFsIHNvdXJjZXMuXG4gICAqL1xuICBPYmplY3QuZGVmaW5lUHJvcGVydHkoQmFzaWNTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUsICdzb3VyY2VzJywge1xuICAgIGdldDogZnVuY3Rpb24gKCkge1xuICAgICAgcmV0dXJuIHRoaXMuX3NvdXJjZXMudG9BcnJheSgpLm1hcChmdW5jdGlvbiAocykge1xuICAgICAgICByZXR1cm4gdGhpcy5zb3VyY2VSb290ICE9IG51bGwgPyB1dGlsLmpvaW4odGhpcy5zb3VyY2VSb290LCBzKSA6IHM7XG4gICAgICB9LCB0aGlzKTtcbiAgICB9XG4gIH0pO1xuXG4gIC8qKlxuICAgKiBQcm92aWRlIHRoZSBKSVQgd2l0aCBhIG5pY2Ugc2hhcGUgLyBoaWRkZW4gY2xhc3MuXG4gICAqL1xuICBmdW5jdGlvbiBNYXBwaW5nKCkge1xuICAgIHRoaXMuZ2VuZXJhdGVkTGluZSA9IDA7XG4gICAgdGhpcy5nZW5lcmF0ZWRDb2x1bW4gPSAwO1xuICAgIHRoaXMuc291cmNlID0gbnVsbDtcbiAgICB0aGlzLm9yaWdpbmFsTGluZSA9IG51bGw7XG4gICAgdGhpcy5vcmlnaW5hbENvbHVtbiA9IG51bGw7XG4gICAgdGhpcy5uYW1lID0gbnVsbDtcbiAgfVxuXG4gIC8qKlxuICAgKiBQYXJzZSB0aGUgbWFwcGluZ3MgaW4gYSBzdHJpbmcgaW4gdG8gYSBkYXRhIHN0cnVjdHVyZSB3aGljaCB3ZSBjYW4gZWFzaWx5XG4gICAqIHF1ZXJ5ICh0aGUgb3JkZXJlZCBhcnJheXMgaW4gdGhlIGB0aGlzLl9fZ2VuZXJhdGVkTWFwcGluZ3NgIGFuZFxuICAgKiBgdGhpcy5fX29yaWdpbmFsTWFwcGluZ3NgIHByb3BlcnRpZXMpLlxuICAgKi9cbiAgQmFzaWNTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuX3BhcnNlTWFwcGluZ3MgPVxuICAgIGZ1bmN0aW9uIFNvdXJjZU1hcENvbnN1bWVyX3BhcnNlTWFwcGluZ3MoYVN0ciwgYVNvdXJjZVJvb3QpIHtcbiAgICAgIHZhciBnZW5lcmF0ZWRMaW5lID0gMTtcbiAgICAgIHZhciBwcmV2aW91c0dlbmVyYXRlZENvbHVtbiA9IDA7XG4gICAgICB2YXIgcHJldmlvdXNPcmlnaW5hbExpbmUgPSAwO1xuICAgICAgdmFyIHByZXZpb3VzT3JpZ2luYWxDb2x1bW4gPSAwO1xuICAgICAgdmFyIHByZXZpb3VzU291cmNlID0gMDtcbiAgICAgIHZhciBwcmV2aW91c05hbWUgPSAwO1xuICAgICAgdmFyIGxlbmd0aCA9IGFTdHIubGVuZ3RoO1xuICAgICAgdmFyIGluZGV4ID0gMDtcbiAgICAgIHZhciBjYWNoZWRTZWdtZW50cyA9IHt9O1xuICAgICAgdmFyIHRlbXAgPSB7fTtcbiAgICAgIHZhciBvcmlnaW5hbE1hcHBpbmdzID0gW107XG4gICAgICB2YXIgZ2VuZXJhdGVkTWFwcGluZ3MgPSBbXTtcbiAgICAgIHZhciBtYXBwaW5nLCBzdHIsIHNlZ21lbnQsIGVuZCwgdmFsdWU7XG5cbiAgICAgIHdoaWxlIChpbmRleCA8IGxlbmd0aCkge1xuICAgICAgICBpZiAoYVN0ci5jaGFyQXQoaW5kZXgpID09PSAnOycpIHtcbiAgICAgICAgICBnZW5lcmF0ZWRMaW5lKys7XG4gICAgICAgICAgaW5kZXgrKztcbiAgICAgICAgICBwcmV2aW91c0dlbmVyYXRlZENvbHVtbiA9IDA7XG4gICAgICAgIH1cbiAgICAgICAgZWxzZSBpZiAoYVN0ci5jaGFyQXQoaW5kZXgpID09PSAnLCcpIHtcbiAgICAgICAgICBpbmRleCsrO1xuICAgICAgICB9XG4gICAgICAgIGVsc2Uge1xuICAgICAgICAgIG1hcHBpbmcgPSBuZXcgTWFwcGluZygpO1xuICAgICAgICAgIG1hcHBpbmcuZ2VuZXJhdGVkTGluZSA9IGdlbmVyYXRlZExpbmU7XG5cbiAgICAgICAgICAvLyBCZWNhdXNlIGVhY2ggb2Zmc2V0IGlzIGVuY29kZWQgcmVsYXRpdmUgdG8gdGhlIHByZXZpb3VzIG9uZSxcbiAgICAgICAgICAvLyBtYW55IHNlZ21lbnRzIG9mdGVuIGhhdmUgdGhlIHNhbWUgZW5jb2RpbmcuIFdlIGNhbiBleHBsb2l0IHRoaXNcbiAgICAgICAgICAvLyBmYWN0IGJ5IGNhY2hpbmcgdGhlIHBhcnNlZCB2YXJpYWJsZSBsZW5ndGggZmllbGRzIG9mIGVhY2ggc2VnbWVudCxcbiAgICAgICAgICAvLyBhbGxvd2luZyB1cyB0byBhdm9pZCBhIHNlY29uZCBwYXJzZSBpZiB3ZSBlbmNvdW50ZXIgdGhlIHNhbWVcbiAgICAgICAgICAvLyBzZWdtZW50IGFnYWluLlxuICAgICAgICAgIGZvciAoZW5kID0gaW5kZXg7IGVuZCA8IGxlbmd0aDsgZW5kKyspIHtcbiAgICAgICAgICAgIGlmICh0aGlzLl9jaGFySXNNYXBwaW5nU2VwYXJhdG9yKGFTdHIsIGVuZCkpIHtcbiAgICAgICAgICAgICAgYnJlYWs7XG4gICAgICAgICAgICB9XG4gICAgICAgICAgfVxuICAgICAgICAgIHN0ciA9IGFTdHIuc2xpY2UoaW5kZXgsIGVuZCk7XG5cbiAgICAgICAgICBzZWdtZW50ID0gY2FjaGVkU2VnbWVudHNbc3RyXTtcbiAgICAgICAgICBpZiAoc2VnbWVudCkge1xuICAgICAgICAgICAgaW5kZXggKz0gc3RyLmxlbmd0aDtcbiAgICAgICAgICB9IGVsc2Uge1xuICAgICAgICAgICAgc2VnbWVudCA9IFtdO1xuICAgICAgICAgICAgd2hpbGUgKGluZGV4IDwgZW5kKSB7XG4gICAgICAgICAgICAgIGJhc2U2NFZMUS5kZWNvZGUoYVN0ciwgaW5kZXgsIHRlbXApO1xuICAgICAgICAgICAgICB2YWx1ZSA9IHRlbXAudmFsdWU7XG4gICAgICAgICAgICAgIGluZGV4ID0gdGVtcC5yZXN0O1xuICAgICAgICAgICAgICBzZWdtZW50LnB1c2godmFsdWUpO1xuICAgICAgICAgICAgfVxuXG4gICAgICAgICAgICBpZiAoc2VnbWVudC5sZW5ndGggPT09IDIpIHtcbiAgICAgICAgICAgICAgdGhyb3cgbmV3IEVycm9yKCdGb3VuZCBhIHNvdXJjZSwgYnV0IG5vIGxpbmUgYW5kIGNvbHVtbicpO1xuICAgICAgICAgICAgfVxuXG4gICAgICAgICAgICBpZiAoc2VnbWVudC5sZW5ndGggPT09IDMpIHtcbiAgICAgICAgICAgICAgdGhyb3cgbmV3IEVycm9yKCdGb3VuZCBhIHNvdXJjZSBhbmQgbGluZSwgYnV0IG5vIGNvbHVtbicpO1xuICAgICAgICAgICAgfVxuXG4gICAgICAgICAgICBjYWNoZWRTZWdtZW50c1tzdHJdID0gc2VnbWVudDtcbiAgICAgICAgICB9XG5cbiAgICAgICAgICAvLyBHZW5lcmF0ZWQgY29sdW1uLlxuICAgICAgICAgIG1hcHBpbmcuZ2VuZXJhdGVkQ29sdW1uID0gcHJldmlvdXNHZW5lcmF0ZWRDb2x1bW4gKyBzZWdtZW50WzBdO1xuICAgICAgICAgIHByZXZpb3VzR2VuZXJhdGVkQ29sdW1uID0gbWFwcGluZy5nZW5lcmF0ZWRDb2x1bW47XG5cbiAgICAgICAgICBpZiAoc2VnbWVudC5sZW5ndGggPiAxKSB7XG4gICAgICAgICAgICAvLyBPcmlnaW5hbCBzb3VyY2UuXG4gICAgICAgICAgICBtYXBwaW5nLnNvdXJjZSA9IHByZXZpb3VzU291cmNlICsgc2VnbWVudFsxXTtcbiAgICAgICAgICAgIHByZXZpb3VzU291cmNlICs9IHNlZ21lbnRbMV07XG5cbiAgICAgICAgICAgIC8vIE9yaWdpbmFsIGxpbmUuXG4gICAgICAgICAgICBtYXBwaW5nLm9yaWdpbmFsTGluZSA9IHByZXZpb3VzT3JpZ2luYWxMaW5lICsgc2VnbWVudFsyXTtcbiAgICAgICAgICAgIHByZXZpb3VzT3JpZ2luYWxMaW5lID0gbWFwcGluZy5vcmlnaW5hbExpbmU7XG4gICAgICAgICAgICAvLyBMaW5lcyBhcmUgc3RvcmVkIDAtYmFzZWRcbiAgICAgICAgICAgIG1hcHBpbmcub3JpZ2luYWxMaW5lICs9IDE7XG5cbiAgICAgICAgICAgIC8vIE9yaWdpbmFsIGNvbHVtbi5cbiAgICAgICAgICAgIG1hcHBpbmcub3JpZ2luYWxDb2x1bW4gPSBwcmV2aW91c09yaWdpbmFsQ29sdW1uICsgc2VnbWVudFszXTtcbiAgICAgICAgICAgIHByZXZpb3VzT3JpZ2luYWxDb2x1bW4gPSBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uO1xuXG4gICAgICAgICAgICBpZiAoc2VnbWVudC5sZW5ndGggPiA0KSB7XG4gICAgICAgICAgICAgIC8vIE9yaWdpbmFsIG5hbWUuXG4gICAgICAgICAgICAgIG1hcHBpbmcubmFtZSA9IHByZXZpb3VzTmFtZSArIHNlZ21lbnRbNF07XG4gICAgICAgICAgICAgIHByZXZpb3VzTmFtZSArPSBzZWdtZW50WzRdO1xuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cblxuICAgICAgICAgIGdlbmVyYXRlZE1hcHBpbmdzLnB1c2gobWFwcGluZyk7XG4gICAgICAgICAgaWYgKHR5cGVvZiBtYXBwaW5nLm9yaWdpbmFsTGluZSA9PT0gJ251bWJlcicpIHtcbiAgICAgICAgICAgIG9yaWdpbmFsTWFwcGluZ3MucHVzaChtYXBwaW5nKTtcbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIH1cblxuICAgICAgcXVpY2tTb3J0KGdlbmVyYXRlZE1hcHBpbmdzLCB1dGlsLmNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0RlZmxhdGVkKTtcbiAgICAgIHRoaXMuX19nZW5lcmF0ZWRNYXBwaW5ncyA9IGdlbmVyYXRlZE1hcHBpbmdzO1xuXG4gICAgICBxdWlja1NvcnQob3JpZ2luYWxNYXBwaW5ncywgdXRpbC5jb21wYXJlQnlPcmlnaW5hbFBvc2l0aW9ucyk7XG4gICAgICB0aGlzLl9fb3JpZ2luYWxNYXBwaW5ncyA9IG9yaWdpbmFsTWFwcGluZ3M7XG4gICAgfTtcblxuICAvKipcbiAgICogRmluZCB0aGUgbWFwcGluZyB0aGF0IGJlc3QgbWF0Y2hlcyB0aGUgaHlwb3RoZXRpY2FsIFwibmVlZGxlXCIgbWFwcGluZyB0aGF0XG4gICAqIHdlIGFyZSBzZWFyY2hpbmcgZm9yIGluIHRoZSBnaXZlbiBcImhheXN0YWNrXCIgb2YgbWFwcGluZ3MuXG4gICAqL1xuICBCYXNpY1NvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5fZmluZE1hcHBpbmcgPVxuICAgIGZ1bmN0aW9uIFNvdXJjZU1hcENvbnN1bWVyX2ZpbmRNYXBwaW5nKGFOZWVkbGUsIGFNYXBwaW5ncywgYUxpbmVOYW1lLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGFDb2x1bW5OYW1lLCBhQ29tcGFyYXRvciwgYUJpYXMpIHtcbiAgICAgIC8vIFRvIHJldHVybiB0aGUgcG9zaXRpb24gd2UgYXJlIHNlYXJjaGluZyBmb3IsIHdlIG11c3QgZmlyc3QgZmluZCB0aGVcbiAgICAgIC8vIG1hcHBpbmcgZm9yIHRoZSBnaXZlbiBwb3NpdGlvbiBhbmQgdGhlbiByZXR1cm4gdGhlIG9wcG9zaXRlIHBvc2l0aW9uIGl0XG4gICAgICAvLyBwb2ludHMgdG8uIEJlY2F1c2UgdGhlIG1hcHBpbmdzIGFyZSBzb3J0ZWQsIHdlIGNhbiB1c2UgYmluYXJ5IHNlYXJjaCB0b1xuICAgICAgLy8gZmluZCB0aGUgYmVzdCBtYXBwaW5nLlxuXG4gICAgICBpZiAoYU5lZWRsZVthTGluZU5hbWVdIDw9IDApIHtcbiAgICAgICAgdGhyb3cgbmV3IFR5cGVFcnJvcignTGluZSBtdXN0IGJlIGdyZWF0ZXIgdGhhbiBvciBlcXVhbCB0byAxLCBnb3QgJ1xuICAgICAgICAgICAgICAgICAgICAgICAgICAgICsgYU5lZWRsZVthTGluZU5hbWVdKTtcbiAgICAgIH1cbiAgICAgIGlmIChhTmVlZGxlW2FDb2x1bW5OYW1lXSA8IDApIHtcbiAgICAgICAgdGhyb3cgbmV3IFR5cGVFcnJvcignQ29sdW1uIG11c3QgYmUgZ3JlYXRlciB0aGFuIG9yIGVxdWFsIHRvIDAsIGdvdCAnXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgKyBhTmVlZGxlW2FDb2x1bW5OYW1lXSk7XG4gICAgICB9XG5cbiAgICAgIHJldHVybiBiaW5hcnlTZWFyY2guc2VhcmNoKGFOZWVkbGUsIGFNYXBwaW5ncywgYUNvbXBhcmF0b3IsIGFCaWFzKTtcbiAgICB9O1xuXG4gIC8qKlxuICAgKiBDb21wdXRlIHRoZSBsYXN0IGNvbHVtbiBmb3IgZWFjaCBnZW5lcmF0ZWQgbWFwcGluZy4gVGhlIGxhc3QgY29sdW1uIGlzXG4gICAqIGluY2x1c2l2ZS5cbiAgICovXG4gIEJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLmNvbXB1dGVDb2x1bW5TcGFucyA9XG4gICAgZnVuY3Rpb24gU291cmNlTWFwQ29uc3VtZXJfY29tcHV0ZUNvbHVtblNwYW5zKCkge1xuICAgICAgZm9yICh2YXIgaW5kZXggPSAwOyBpbmRleCA8IHRoaXMuX2dlbmVyYXRlZE1hcHBpbmdzLmxlbmd0aDsgKytpbmRleCkge1xuICAgICAgICB2YXIgbWFwcGluZyA9IHRoaXMuX2dlbmVyYXRlZE1hcHBpbmdzW2luZGV4XTtcblxuICAgICAgICAvLyBNYXBwaW5ncyBkbyBub3QgY29udGFpbiBhIGZpZWxkIGZvciB0aGUgbGFzdCBnZW5lcmF0ZWQgY29sdW1udC4gV2VcbiAgICAgICAgLy8gY2FuIGNvbWUgdXAgd2l0aCBhbiBvcHRpbWlzdGljIGVzdGltYXRlLCBob3dldmVyLCBieSBhc3N1bWluZyB0aGF0XG4gICAgICAgIC8vIG1hcHBpbmdzIGFyZSBjb250aWd1b3VzIChpLmUuIGdpdmVuIHR3byBjb25zZWN1dGl2ZSBtYXBwaW5ncywgdGhlXG4gICAgICAgIC8vIGZpcnN0IG1hcHBpbmcgZW5kcyB3aGVyZSB0aGUgc2Vjb25kIG9uZSBzdGFydHMpLlxuICAgICAgICBpZiAoaW5kZXggKyAxIDwgdGhpcy5fZ2VuZXJhdGVkTWFwcGluZ3MubGVuZ3RoKSB7XG4gICAgICAgICAgdmFyIG5leHRNYXBwaW5nID0gdGhpcy5fZ2VuZXJhdGVkTWFwcGluZ3NbaW5kZXggKyAxXTtcblxuICAgICAgICAgIGlmIChtYXBwaW5nLmdlbmVyYXRlZExpbmUgPT09IG5leHRNYXBwaW5nLmdlbmVyYXRlZExpbmUpIHtcbiAgICAgICAgICAgIG1hcHBpbmcubGFzdEdlbmVyYXRlZENvbHVtbiA9IG5leHRNYXBwaW5nLmdlbmVyYXRlZENvbHVtbiAtIDE7XG4gICAgICAgICAgICBjb250aW51ZTtcbiAgICAgICAgICB9XG4gICAgICAgIH1cblxuICAgICAgICAvLyBUaGUgbGFzdCBtYXBwaW5nIGZvciBlYWNoIGxpbmUgc3BhbnMgdGhlIGVudGlyZSBsaW5lLlxuICAgICAgICBtYXBwaW5nLmxhc3RHZW5lcmF0ZWRDb2x1bW4gPSBJbmZpbml0eTtcbiAgICAgIH1cbiAgICB9O1xuXG4gIC8qKlxuICAgKiBSZXR1cm5zIHRoZSBvcmlnaW5hbCBzb3VyY2UsIGxpbmUsIGFuZCBjb2x1bW4gaW5mb3JtYXRpb24gZm9yIHRoZSBnZW5lcmF0ZWRcbiAgICogc291cmNlJ3MgbGluZSBhbmQgY29sdW1uIHBvc2l0aW9ucyBwcm92aWRlZC4gVGhlIG9ubHkgYXJndW1lbnQgaXMgYW4gb2JqZWN0XG4gICAqIHdpdGggdGhlIGZvbGxvd2luZyBwcm9wZXJ0aWVzOlxuICAgKlxuICAgKiAgIC0gbGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBnZW5lcmF0ZWQgc291cmNlLlxuICAgKiAgIC0gY29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgZ2VuZXJhdGVkIHNvdXJjZS5cbiAgICogICAtIGJpYXM6IEVpdGhlciAnU291cmNlTWFwQ29uc3VtZXIuR1JFQVRFU1RfTE9XRVJfQk9VTkQnIG9yXG4gICAqICAgICAnU291cmNlTWFwQ29uc3VtZXIuTEVBU1RfVVBQRVJfQk9VTkQnLiBTcGVjaWZpZXMgd2hldGhlciB0byByZXR1cm4gdGhlXG4gICAqICAgICBjbG9zZXN0IGVsZW1lbnQgdGhhdCBpcyBzbWFsbGVyIHRoYW4gb3IgZ3JlYXRlciB0aGFuIHRoZSBvbmUgd2UgYXJlXG4gICAqICAgICBzZWFyY2hpbmcgZm9yLCByZXNwZWN0aXZlbHksIGlmIHRoZSBleGFjdCBlbGVtZW50IGNhbm5vdCBiZSBmb3VuZC5cbiAgICogICAgIERlZmF1bHRzIHRvICdTb3VyY2VNYXBDb25zdW1lci5HUkVBVEVTVF9MT1dFUl9CT1VORCcuXG4gICAqXG4gICAqIGFuZCBhbiBvYmplY3QgaXMgcmV0dXJuZWQgd2l0aCB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gICAqXG4gICAqICAgLSBzb3VyY2U6IFRoZSBvcmlnaW5hbCBzb3VyY2UgZmlsZSwgb3IgbnVsbC5cbiAgICogICAtIGxpbmU6IFRoZSBsaW5lIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLCBvciBudWxsLlxuICAgKiAgIC0gY29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLCBvciBudWxsLlxuICAgKiAgIC0gbmFtZTogVGhlIG9yaWdpbmFsIGlkZW50aWZpZXIsIG9yIG51bGwuXG4gICAqL1xuICBCYXNpY1NvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5vcmlnaW5hbFBvc2l0aW9uRm9yID1cbiAgICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcl9vcmlnaW5hbFBvc2l0aW9uRm9yKGFBcmdzKSB7XG4gICAgICB2YXIgbmVlZGxlID0ge1xuICAgICAgICBnZW5lcmF0ZWRMaW5lOiB1dGlsLmdldEFyZyhhQXJncywgJ2xpbmUnKSxcbiAgICAgICAgZ2VuZXJhdGVkQ29sdW1uOiB1dGlsLmdldEFyZyhhQXJncywgJ2NvbHVtbicpXG4gICAgICB9O1xuXG4gICAgICB2YXIgaW5kZXggPSB0aGlzLl9maW5kTWFwcGluZyhcbiAgICAgICAgbmVlZGxlLFxuICAgICAgICB0aGlzLl9nZW5lcmF0ZWRNYXBwaW5ncyxcbiAgICAgICAgXCJnZW5lcmF0ZWRMaW5lXCIsXG4gICAgICAgIFwiZ2VuZXJhdGVkQ29sdW1uXCIsXG4gICAgICAgIHV0aWwuY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zRGVmbGF0ZWQsXG4gICAgICAgIHV0aWwuZ2V0QXJnKGFBcmdzLCAnYmlhcycsIFNvdXJjZU1hcENvbnN1bWVyLkdSRUFURVNUX0xPV0VSX0JPVU5EKVxuICAgICAgKTtcblxuICAgICAgaWYgKGluZGV4ID49IDApIHtcbiAgICAgICAgdmFyIG1hcHBpbmcgPSB0aGlzLl9nZW5lcmF0ZWRNYXBwaW5nc1tpbmRleF07XG5cbiAgICAgICAgaWYgKG1hcHBpbmcuZ2VuZXJhdGVkTGluZSA9PT0gbmVlZGxlLmdlbmVyYXRlZExpbmUpIHtcbiAgICAgICAgICB2YXIgc291cmNlID0gdXRpbC5nZXRBcmcobWFwcGluZywgJ3NvdXJjZScsIG51bGwpO1xuICAgICAgICAgIGlmIChzb3VyY2UgIT09IG51bGwpIHtcbiAgICAgICAgICAgIHNvdXJjZSA9IHRoaXMuX3NvdXJjZXMuYXQoc291cmNlKTtcbiAgICAgICAgICAgIGlmICh0aGlzLnNvdXJjZVJvb3QgIT0gbnVsbCkge1xuICAgICAgICAgICAgICBzb3VyY2UgPSB1dGlsLmpvaW4odGhpcy5zb3VyY2VSb290LCBzb3VyY2UpO1xuICAgICAgICAgICAgfVxuICAgICAgICAgIH1cbiAgICAgICAgICB2YXIgbmFtZSA9IHV0aWwuZ2V0QXJnKG1hcHBpbmcsICduYW1lJywgbnVsbCk7XG4gICAgICAgICAgaWYgKG5hbWUgIT09IG51bGwpIHtcbiAgICAgICAgICAgIG5hbWUgPSB0aGlzLl9uYW1lcy5hdChuYW1lKTtcbiAgICAgICAgICB9XG4gICAgICAgICAgcmV0dXJuIHtcbiAgICAgICAgICAgIHNvdXJjZTogc291cmNlLFxuICAgICAgICAgICAgbGluZTogdXRpbC5nZXRBcmcobWFwcGluZywgJ29yaWdpbmFsTGluZScsIG51bGwpLFxuICAgICAgICAgICAgY29sdW1uOiB1dGlsLmdldEFyZyhtYXBwaW5nLCAnb3JpZ2luYWxDb2x1bW4nLCBudWxsKSxcbiAgICAgICAgICAgIG5hbWU6IG5hbWVcbiAgICAgICAgICB9O1xuICAgICAgICB9XG4gICAgICB9XG5cbiAgICAgIHJldHVybiB7XG4gICAgICAgIHNvdXJjZTogbnVsbCxcbiAgICAgICAgbGluZTogbnVsbCxcbiAgICAgICAgY29sdW1uOiBudWxsLFxuICAgICAgICBuYW1lOiBudWxsXG4gICAgICB9O1xuICAgIH07XG5cbiAgLyoqXG4gICAqIFJldHVybiB0cnVlIGlmIHdlIGhhdmUgdGhlIHNvdXJjZSBjb250ZW50IGZvciBldmVyeSBzb3VyY2UgaW4gdGhlIHNvdXJjZVxuICAgKiBtYXAsIGZhbHNlIG90aGVyd2lzZS5cbiAgICovXG4gIEJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLmhhc0NvbnRlbnRzT2ZBbGxTb3VyY2VzID1cbiAgICBmdW5jdGlvbiBCYXNpY1NvdXJjZU1hcENvbnN1bWVyX2hhc0NvbnRlbnRzT2ZBbGxTb3VyY2VzKCkge1xuICAgICAgaWYgKCF0aGlzLnNvdXJjZXNDb250ZW50KSB7XG4gICAgICAgIHJldHVybiBmYWxzZTtcbiAgICAgIH1cbiAgICAgIHJldHVybiB0aGlzLnNvdXJjZXNDb250ZW50Lmxlbmd0aCA+PSB0aGlzLl9zb3VyY2VzLnNpemUoKSAmJlxuICAgICAgICAhdGhpcy5zb3VyY2VzQ29udGVudC5zb21lKGZ1bmN0aW9uIChzYykgeyByZXR1cm4gc2MgPT0gbnVsbDsgfSk7XG4gICAgfTtcblxuICAvKipcbiAgICogUmV0dXJucyB0aGUgb3JpZ2luYWwgc291cmNlIGNvbnRlbnQuIFRoZSBvbmx5IGFyZ3VtZW50IGlzIHRoZSB1cmwgb2YgdGhlXG4gICAqIG9yaWdpbmFsIHNvdXJjZSBmaWxlLiBSZXR1cm5zIG51bGwgaWYgbm8gb3JpZ2luYWwgc291cmNlIGNvbnRlbnQgaXNcbiAgICogYXZhaWxpYmxlLlxuICAgKi9cbiAgQmFzaWNTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuc291cmNlQ29udGVudEZvciA9XG4gICAgZnVuY3Rpb24gU291cmNlTWFwQ29uc3VtZXJfc291cmNlQ29udGVudEZvcihhU291cmNlLCBudWxsT25NaXNzaW5nKSB7XG4gICAgICBpZiAoIXRoaXMuc291cmNlc0NvbnRlbnQpIHtcbiAgICAgICAgcmV0dXJuIG51bGw7XG4gICAgICB9XG5cbiAgICAgIGlmICh0aGlzLnNvdXJjZVJvb3QgIT0gbnVsbCkge1xuICAgICAgICBhU291cmNlID0gdXRpbC5yZWxhdGl2ZSh0aGlzLnNvdXJjZVJvb3QsIGFTb3VyY2UpO1xuICAgICAgfVxuXG4gICAgICBpZiAodGhpcy5fc291cmNlcy5oYXMoYVNvdXJjZSkpIHtcbiAgICAgICAgcmV0dXJuIHRoaXMuc291cmNlc0NvbnRlbnRbdGhpcy5fc291cmNlcy5pbmRleE9mKGFTb3VyY2UpXTtcbiAgICAgIH1cblxuICAgICAgdmFyIHVybDtcbiAgICAgIGlmICh0aGlzLnNvdXJjZVJvb3QgIT0gbnVsbFxuICAgICAgICAgICYmICh1cmwgPSB1dGlsLnVybFBhcnNlKHRoaXMuc291cmNlUm9vdCkpKSB7XG4gICAgICAgIC8vIFhYWDogZmlsZTovLyBVUklzIGFuZCBhYnNvbHV0ZSBwYXRocyBsZWFkIHRvIHVuZXhwZWN0ZWQgYmVoYXZpb3IgZm9yXG4gICAgICAgIC8vIG1hbnkgdXNlcnMuIFdlIGNhbiBoZWxwIHRoZW0gb3V0IHdoZW4gdGhleSBleHBlY3QgZmlsZTovLyBVUklzIHRvXG4gICAgICAgIC8vIGJlaGF2ZSBsaWtlIGl0IHdvdWxkIGlmIHRoZXkgd2VyZSBydW5uaW5nIGEgbG9jYWwgSFRUUCBzZXJ2ZXIuIFNlZVxuICAgICAgICAvLyBodHRwczovL2J1Z3ppbGxhLm1vemlsbGEub3JnL3Nob3dfYnVnLmNnaT9pZD04ODU1OTcuXG4gICAgICAgIHZhciBmaWxlVXJpQWJzUGF0aCA9IGFTb3VyY2UucmVwbGFjZSgvXmZpbGU6XFwvXFwvLywgXCJcIik7XG4gICAgICAgIGlmICh1cmwuc2NoZW1lID09IFwiZmlsZVwiXG4gICAgICAgICAgICAmJiB0aGlzLl9zb3VyY2VzLmhhcyhmaWxlVXJpQWJzUGF0aCkpIHtcbiAgICAgICAgICByZXR1cm4gdGhpcy5zb3VyY2VzQ29udGVudFt0aGlzLl9zb3VyY2VzLmluZGV4T2YoZmlsZVVyaUFic1BhdGgpXVxuICAgICAgICB9XG5cbiAgICAgICAgaWYgKCghdXJsLnBhdGggfHwgdXJsLnBhdGggPT0gXCIvXCIpXG4gICAgICAgICAgICAmJiB0aGlzLl9zb3VyY2VzLmhhcyhcIi9cIiArIGFTb3VyY2UpKSB7XG4gICAgICAgICAgcmV0dXJuIHRoaXMuc291cmNlc0NvbnRlbnRbdGhpcy5fc291cmNlcy5pbmRleE9mKFwiL1wiICsgYVNvdXJjZSldO1xuICAgICAgICB9XG4gICAgICB9XG5cbiAgICAgIC8vIFRoaXMgZnVuY3Rpb24gaXMgdXNlZCByZWN1cnNpdmVseSBmcm9tXG4gICAgICAvLyBJbmRleGVkU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLnNvdXJjZUNvbnRlbnRGb3IuIEluIHRoYXQgY2FzZSwgd2VcbiAgICAgIC8vIGRvbid0IHdhbnQgdG8gdGhyb3cgaWYgd2UgY2FuJ3QgZmluZCB0aGUgc291cmNlIC0gd2UganVzdCB3YW50IHRvXG4gICAgICAvLyByZXR1cm4gbnVsbCwgc28gd2UgcHJvdmlkZSBhIGZsYWcgdG8gZXhpdCBncmFjZWZ1bGx5LlxuICAgICAgaWYgKG51bGxPbk1pc3NpbmcpIHtcbiAgICAgICAgcmV0dXJuIG51bGw7XG4gICAgICB9XG4gICAgICBlbHNlIHtcbiAgICAgICAgdGhyb3cgbmV3IEVycm9yKCdcIicgKyBhU291cmNlICsgJ1wiIGlzIG5vdCBpbiB0aGUgU291cmNlTWFwLicpO1xuICAgICAgfVxuICAgIH07XG5cbiAgLyoqXG4gICAqIFJldHVybnMgdGhlIGdlbmVyYXRlZCBsaW5lIGFuZCBjb2x1bW4gaW5mb3JtYXRpb24gZm9yIHRoZSBvcmlnaW5hbCBzb3VyY2UsXG4gICAqIGxpbmUsIGFuZCBjb2x1bW4gcG9zaXRpb25zIHByb3ZpZGVkLiBUaGUgb25seSBhcmd1bWVudCBpcyBhbiBvYmplY3Qgd2l0aFxuICAgKiB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gICAqXG4gICAqICAgLSBzb3VyY2U6IFRoZSBmaWxlbmFtZSBvZiB0aGUgb3JpZ2luYWwgc291cmNlLlxuICAgKiAgIC0gbGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBvcmlnaW5hbCBzb3VyY2UuXG4gICAqICAgLSBjb2x1bW46IFRoZSBjb2x1bW4gbnVtYmVyIGluIHRoZSBvcmlnaW5hbCBzb3VyY2UuXG4gICAqICAgLSBiaWFzOiBFaXRoZXIgJ1NvdXJjZU1hcENvbnN1bWVyLkdSRUFURVNUX0xPV0VSX0JPVU5EJyBvclxuICAgKiAgICAgJ1NvdXJjZU1hcENvbnN1bWVyLkxFQVNUX1VQUEVSX0JPVU5EJy4gU3BlY2lmaWVzIHdoZXRoZXIgdG8gcmV0dXJuIHRoZVxuICAgKiAgICAgY2xvc2VzdCBlbGVtZW50IHRoYXQgaXMgc21hbGxlciB0aGFuIG9yIGdyZWF0ZXIgdGhhbiB0aGUgb25lIHdlIGFyZVxuICAgKiAgICAgc2VhcmNoaW5nIGZvciwgcmVzcGVjdGl2ZWx5LCBpZiB0aGUgZXhhY3QgZWxlbWVudCBjYW5ub3QgYmUgZm91bmQuXG4gICAqICAgICBEZWZhdWx0cyB0byAnU291cmNlTWFwQ29uc3VtZXIuR1JFQVRFU1RfTE9XRVJfQk9VTkQnLlxuICAgKlxuICAgKiBhbmQgYW4gb2JqZWN0IGlzIHJldHVybmVkIHdpdGggdGhlIGZvbGxvd2luZyBwcm9wZXJ0aWVzOlxuICAgKlxuICAgKiAgIC0gbGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBnZW5lcmF0ZWQgc291cmNlLCBvciBudWxsLlxuICAgKiAgIC0gY29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgZ2VuZXJhdGVkIHNvdXJjZSwgb3IgbnVsbC5cbiAgICovXG4gIEJhc2ljU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLmdlbmVyYXRlZFBvc2l0aW9uRm9yID1cbiAgICBmdW5jdGlvbiBTb3VyY2VNYXBDb25zdW1lcl9nZW5lcmF0ZWRQb3NpdGlvbkZvcihhQXJncykge1xuICAgICAgdmFyIHNvdXJjZSA9IHV0aWwuZ2V0QXJnKGFBcmdzLCAnc291cmNlJyk7XG4gICAgICBpZiAodGhpcy5zb3VyY2VSb290ICE9IG51bGwpIHtcbiAgICAgICAgc291cmNlID0gdXRpbC5yZWxhdGl2ZSh0aGlzLnNvdXJjZVJvb3QsIHNvdXJjZSk7XG4gICAgICB9XG4gICAgICBpZiAoIXRoaXMuX3NvdXJjZXMuaGFzKHNvdXJjZSkpIHtcbiAgICAgICAgcmV0dXJuIHtcbiAgICAgICAgICBsaW5lOiBudWxsLFxuICAgICAgICAgIGNvbHVtbjogbnVsbCxcbiAgICAgICAgICBsYXN0Q29sdW1uOiBudWxsXG4gICAgICAgIH07XG4gICAgICB9XG4gICAgICBzb3VyY2UgPSB0aGlzLl9zb3VyY2VzLmluZGV4T2Yoc291cmNlKTtcblxuICAgICAgdmFyIG5lZWRsZSA9IHtcbiAgICAgICAgc291cmNlOiBzb3VyY2UsXG4gICAgICAgIG9yaWdpbmFsTGluZTogdXRpbC5nZXRBcmcoYUFyZ3MsICdsaW5lJyksXG4gICAgICAgIG9yaWdpbmFsQ29sdW1uOiB1dGlsLmdldEFyZyhhQXJncywgJ2NvbHVtbicpXG4gICAgICB9O1xuXG4gICAgICB2YXIgaW5kZXggPSB0aGlzLl9maW5kTWFwcGluZyhcbiAgICAgICAgbmVlZGxlLFxuICAgICAgICB0aGlzLl9vcmlnaW5hbE1hcHBpbmdzLFxuICAgICAgICBcIm9yaWdpbmFsTGluZVwiLFxuICAgICAgICBcIm9yaWdpbmFsQ29sdW1uXCIsXG4gICAgICAgIHV0aWwuY29tcGFyZUJ5T3JpZ2luYWxQb3NpdGlvbnMsXG4gICAgICAgIHV0aWwuZ2V0QXJnKGFBcmdzLCAnYmlhcycsIFNvdXJjZU1hcENvbnN1bWVyLkdSRUFURVNUX0xPV0VSX0JPVU5EKVxuICAgICAgKTtcblxuICAgICAgaWYgKGluZGV4ID49IDApIHtcbiAgICAgICAgdmFyIG1hcHBpbmcgPSB0aGlzLl9vcmlnaW5hbE1hcHBpbmdzW2luZGV4XTtcblxuICAgICAgICBpZiAobWFwcGluZy5zb3VyY2UgPT09IG5lZWRsZS5zb3VyY2UpIHtcbiAgICAgICAgICByZXR1cm4ge1xuICAgICAgICAgICAgbGluZTogdXRpbC5nZXRBcmcobWFwcGluZywgJ2dlbmVyYXRlZExpbmUnLCBudWxsKSxcbiAgICAgICAgICAgIGNvbHVtbjogdXRpbC5nZXRBcmcobWFwcGluZywgJ2dlbmVyYXRlZENvbHVtbicsIG51bGwpLFxuICAgICAgICAgICAgbGFzdENvbHVtbjogdXRpbC5nZXRBcmcobWFwcGluZywgJ2xhc3RHZW5lcmF0ZWRDb2x1bW4nLCBudWxsKVxuICAgICAgICAgIH07XG4gICAgICAgIH1cbiAgICAgIH1cblxuICAgICAgcmV0dXJuIHtcbiAgICAgICAgbGluZTogbnVsbCxcbiAgICAgICAgY29sdW1uOiBudWxsLFxuICAgICAgICBsYXN0Q29sdW1uOiBudWxsXG4gICAgICB9O1xuICAgIH07XG5cbiAgZXhwb3J0cy5CYXNpY1NvdXJjZU1hcENvbnN1bWVyID0gQmFzaWNTb3VyY2VNYXBDb25zdW1lcjtcblxuICAvKipcbiAgICogQW4gSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyIGluc3RhbmNlIHJlcHJlc2VudHMgYSBwYXJzZWQgc291cmNlIG1hcCB3aGljaFxuICAgKiB3ZSBjYW4gcXVlcnkgZm9yIGluZm9ybWF0aW9uLiBJdCBkaWZmZXJzIGZyb20gQmFzaWNTb3VyY2VNYXBDb25zdW1lciBpblxuICAgKiB0aGF0IGl0IHRha2VzIFwiaW5kZXhlZFwiIHNvdXJjZSBtYXBzIChpLmUuIG9uZXMgd2l0aCBhIFwic2VjdGlvbnNcIiBmaWVsZCkgYXNcbiAgICogaW5wdXQuXG4gICAqXG4gICAqIFRoZSBvbmx5IHBhcmFtZXRlciBpcyBhIHJhdyBzb3VyY2UgbWFwIChlaXRoZXIgYXMgYSBKU09OIHN0cmluZywgb3IgYWxyZWFkeVxuICAgKiBwYXJzZWQgdG8gYW4gb2JqZWN0KS4gQWNjb3JkaW5nIHRvIHRoZSBzcGVjIGZvciBpbmRleGVkIHNvdXJjZSBtYXBzLCB0aGV5XG4gICAqIGhhdmUgdGhlIGZvbGxvd2luZyBhdHRyaWJ1dGVzOlxuICAgKlxuICAgKiAgIC0gdmVyc2lvbjogV2hpY2ggdmVyc2lvbiBvZiB0aGUgc291cmNlIG1hcCBzcGVjIHRoaXMgbWFwIGlzIGZvbGxvd2luZy5cbiAgICogICAtIGZpbGU6IE9wdGlvbmFsLiBUaGUgZ2VuZXJhdGVkIGZpbGUgdGhpcyBzb3VyY2UgbWFwIGlzIGFzc29jaWF0ZWQgd2l0aC5cbiAgICogICAtIHNlY3Rpb25zOiBBIGxpc3Qgb2Ygc2VjdGlvbiBkZWZpbml0aW9ucy5cbiAgICpcbiAgICogRWFjaCB2YWx1ZSB1bmRlciB0aGUgXCJzZWN0aW9uc1wiIGZpZWxkIGhhcyB0d28gZmllbGRzOlxuICAgKiAgIC0gb2Zmc2V0OiBUaGUgb2Zmc2V0IGludG8gdGhlIG9yaWdpbmFsIHNwZWNpZmllZCBhdCB3aGljaCB0aGlzIHNlY3Rpb25cbiAgICogICAgICAgYmVnaW5zIHRvIGFwcGx5LCBkZWZpbmVkIGFzIGFuIG9iamVjdCB3aXRoIGEgXCJsaW5lXCIgYW5kIFwiY29sdW1uXCJcbiAgICogICAgICAgZmllbGQuXG4gICAqICAgLSBtYXA6IEEgc291cmNlIG1hcCBkZWZpbml0aW9uLiBUaGlzIHNvdXJjZSBtYXAgY291bGQgYWxzbyBiZSBpbmRleGVkLFxuICAgKiAgICAgICBidXQgZG9lc24ndCBoYXZlIHRvIGJlLlxuICAgKlxuICAgKiBJbnN0ZWFkIG9mIHRoZSBcIm1hcFwiIGZpZWxkLCBpdCdzIGFsc28gcG9zc2libGUgdG8gaGF2ZSBhIFwidXJsXCIgZmllbGRcbiAgICogc3BlY2lmeWluZyBhIFVSTCB0byByZXRyaWV2ZSBhIHNvdXJjZSBtYXAgZnJvbSwgYnV0IHRoYXQncyBjdXJyZW50bHlcbiAgICogdW5zdXBwb3J0ZWQuXG4gICAqXG4gICAqIEhlcmUncyBhbiBleGFtcGxlIHNvdXJjZSBtYXAsIHRha2VuIGZyb20gdGhlIHNvdXJjZSBtYXAgc3BlY1swXSwgYnV0XG4gICAqIG1vZGlmaWVkIHRvIG9taXQgYSBzZWN0aW9uIHdoaWNoIHVzZXMgdGhlIFwidXJsXCIgZmllbGQuXG4gICAqXG4gICAqICB7XG4gICAqICAgIHZlcnNpb24gOiAzLFxuICAgKiAgICBmaWxlOiBcImFwcC5qc1wiLFxuICAgKiAgICBzZWN0aW9uczogW3tcbiAgICogICAgICBvZmZzZXQ6IHtsaW5lOjEwMCwgY29sdW1uOjEwfSxcbiAgICogICAgICBtYXA6IHtcbiAgICogICAgICAgIHZlcnNpb24gOiAzLFxuICAgKiAgICAgICAgZmlsZTogXCJzZWN0aW9uLmpzXCIsXG4gICAqICAgICAgICBzb3VyY2VzOiBbXCJmb28uanNcIiwgXCJiYXIuanNcIl0sXG4gICAqICAgICAgICBuYW1lczogW1wic3JjXCIsIFwibWFwc1wiLCBcImFyZVwiLCBcImZ1blwiXSxcbiAgICogICAgICAgIG1hcHBpbmdzOiBcIkFBQUEsRTs7QUJDREU7XCJcbiAgICogICAgICB9XG4gICAqICAgIH1dLFxuICAgKiAgfVxuICAgKlxuICAgKiBbMF06IGh0dHBzOi8vZG9jcy5nb29nbGUuY29tL2RvY3VtZW50L2QvMVUxUkdBZWhRd1J5cFVUb3ZGMUtSbHBpT0Z6ZTBiLV8yZ2M2ZkFIMEtZMGsvZWRpdCNoZWFkaW5nPWguNTM1ZXMzeGVwcmd0XG4gICAqL1xuICBmdW5jdGlvbiBJbmRleGVkU291cmNlTWFwQ29uc3VtZXIoYVNvdXJjZU1hcCkge1xuICAgIHZhciBzb3VyY2VNYXAgPSBhU291cmNlTWFwO1xuICAgIGlmICh0eXBlb2YgYVNvdXJjZU1hcCA9PT0gJ3N0cmluZycpIHtcbiAgICAgIHNvdXJjZU1hcCA9IEpTT04ucGFyc2UoYVNvdXJjZU1hcC5yZXBsYWNlKC9eXFwpXFxdXFx9Jy8sICcnKSk7XG4gICAgfVxuXG4gICAgdmFyIHZlcnNpb24gPSB1dGlsLmdldEFyZyhzb3VyY2VNYXAsICd2ZXJzaW9uJyk7XG4gICAgdmFyIHNlY3Rpb25zID0gdXRpbC5nZXRBcmcoc291cmNlTWFwLCAnc2VjdGlvbnMnKTtcblxuICAgIGlmICh2ZXJzaW9uICE9IHRoaXMuX3ZlcnNpb24pIHtcbiAgICAgIHRocm93IG5ldyBFcnJvcignVW5zdXBwb3J0ZWQgdmVyc2lvbjogJyArIHZlcnNpb24pO1xuICAgIH1cblxuICAgIHRoaXMuX3NvdXJjZXMgPSBuZXcgQXJyYXlTZXQoKTtcbiAgICB0aGlzLl9uYW1lcyA9IG5ldyBBcnJheVNldCgpO1xuXG4gICAgdmFyIGxhc3RPZmZzZXQgPSB7XG4gICAgICBsaW5lOiAtMSxcbiAgICAgIGNvbHVtbjogMFxuICAgIH07XG4gICAgdGhpcy5fc2VjdGlvbnMgPSBzZWN0aW9ucy5tYXAoZnVuY3Rpb24gKHMpIHtcbiAgICAgIGlmIChzLnVybCkge1xuICAgICAgICAvLyBUaGUgdXJsIGZpZWxkIHdpbGwgcmVxdWlyZSBzdXBwb3J0IGZvciBhc3luY2hyb25pY2l0eS5cbiAgICAgICAgLy8gU2VlIGh0dHBzOi8vZ2l0aHViLmNvbS9tb3ppbGxhL3NvdXJjZS1tYXAvaXNzdWVzLzE2XG4gICAgICAgIHRocm93IG5ldyBFcnJvcignU3VwcG9ydCBmb3IgdXJsIGZpZWxkIGluIHNlY3Rpb25zIG5vdCBpbXBsZW1lbnRlZC4nKTtcbiAgICAgIH1cbiAgICAgIHZhciBvZmZzZXQgPSB1dGlsLmdldEFyZyhzLCAnb2Zmc2V0Jyk7XG4gICAgICB2YXIgb2Zmc2V0TGluZSA9IHV0aWwuZ2V0QXJnKG9mZnNldCwgJ2xpbmUnKTtcbiAgICAgIHZhciBvZmZzZXRDb2x1bW4gPSB1dGlsLmdldEFyZyhvZmZzZXQsICdjb2x1bW4nKTtcblxuICAgICAgaWYgKG9mZnNldExpbmUgPCBsYXN0T2Zmc2V0LmxpbmUgfHxcbiAgICAgICAgICAob2Zmc2V0TGluZSA9PT0gbGFzdE9mZnNldC5saW5lICYmIG9mZnNldENvbHVtbiA8IGxhc3RPZmZzZXQuY29sdW1uKSkge1xuICAgICAgICB0aHJvdyBuZXcgRXJyb3IoJ1NlY3Rpb24gb2Zmc2V0cyBtdXN0IGJlIG9yZGVyZWQgYW5kIG5vbi1vdmVybGFwcGluZy4nKTtcbiAgICAgIH1cbiAgICAgIGxhc3RPZmZzZXQgPSBvZmZzZXQ7XG5cbiAgICAgIHJldHVybiB7XG4gICAgICAgIGdlbmVyYXRlZE9mZnNldDoge1xuICAgICAgICAgIC8vIFRoZSBvZmZzZXQgZmllbGRzIGFyZSAwLWJhc2VkLCBidXQgd2UgdXNlIDEtYmFzZWQgaW5kaWNlcyB3aGVuXG4gICAgICAgICAgLy8gZW5jb2RpbmcvZGVjb2RpbmcgZnJvbSBWTFEuXG4gICAgICAgICAgZ2VuZXJhdGVkTGluZTogb2Zmc2V0TGluZSArIDEsXG4gICAgICAgICAgZ2VuZXJhdGVkQ29sdW1uOiBvZmZzZXRDb2x1bW4gKyAxXG4gICAgICAgIH0sXG4gICAgICAgIGNvbnN1bWVyOiBuZXcgU291cmNlTWFwQ29uc3VtZXIodXRpbC5nZXRBcmcocywgJ21hcCcpKVxuICAgICAgfVxuICAgIH0pO1xuICB9XG5cbiAgSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZSA9IE9iamVjdC5jcmVhdGUoU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlKTtcbiAgSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5jb25zdHJ1Y3RvciA9IFNvdXJjZU1hcENvbnN1bWVyO1xuXG4gIC8qKlxuICAgKiBUaGUgdmVyc2lvbiBvZiB0aGUgc291cmNlIG1hcHBpbmcgc3BlYyB0aGF0IHdlIGFyZSBjb25zdW1pbmcuXG4gICAqL1xuICBJbmRleGVkU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLl92ZXJzaW9uID0gMztcblxuICAvKipcbiAgICogVGhlIGxpc3Qgb2Ygb3JpZ2luYWwgc291cmNlcy5cbiAgICovXG4gIE9iamVjdC5kZWZpbmVQcm9wZXJ0eShJbmRleGVkU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLCAnc291cmNlcycsIHtcbiAgICBnZXQ6IGZ1bmN0aW9uICgpIHtcbiAgICAgIHZhciBzb3VyY2VzID0gW107XG4gICAgICBmb3IgKHZhciBpID0gMDsgaSA8IHRoaXMuX3NlY3Rpb25zLmxlbmd0aDsgaSsrKSB7XG4gICAgICAgIGZvciAodmFyIGogPSAwOyBqIDwgdGhpcy5fc2VjdGlvbnNbaV0uY29uc3VtZXIuc291cmNlcy5sZW5ndGg7IGorKykge1xuICAgICAgICAgIHNvdXJjZXMucHVzaCh0aGlzLl9zZWN0aW9uc1tpXS5jb25zdW1lci5zb3VyY2VzW2pdKTtcbiAgICAgICAgfVxuICAgICAgfVxuICAgICAgcmV0dXJuIHNvdXJjZXM7XG4gICAgfVxuICB9KTtcblxuICAvKipcbiAgICogUmV0dXJucyB0aGUgb3JpZ2luYWwgc291cmNlLCBsaW5lLCBhbmQgY29sdW1uIGluZm9ybWF0aW9uIGZvciB0aGUgZ2VuZXJhdGVkXG4gICAqIHNvdXJjZSdzIGxpbmUgYW5kIGNvbHVtbiBwb3NpdGlvbnMgcHJvdmlkZWQuIFRoZSBvbmx5IGFyZ3VtZW50IGlzIGFuIG9iamVjdFxuICAgKiB3aXRoIHRoZSBmb2xsb3dpbmcgcHJvcGVydGllczpcbiAgICpcbiAgICogICAtIGxpbmU6IFRoZSBsaW5lIG51bWJlciBpbiB0aGUgZ2VuZXJhdGVkIHNvdXJjZS5cbiAgICogICAtIGNvbHVtbjogVGhlIGNvbHVtbiBudW1iZXIgaW4gdGhlIGdlbmVyYXRlZCBzb3VyY2UuXG4gICAqXG4gICAqIGFuZCBhbiBvYmplY3QgaXMgcmV0dXJuZWQgd2l0aCB0aGUgZm9sbG93aW5nIHByb3BlcnRpZXM6XG4gICAqXG4gICAqICAgLSBzb3VyY2U6IFRoZSBvcmlnaW5hbCBzb3VyY2UgZmlsZSwgb3IgbnVsbC5cbiAgICogICAtIGxpbmU6IFRoZSBsaW5lIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLCBvciBudWxsLlxuICAgKiAgIC0gY29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLCBvciBudWxsLlxuICAgKiAgIC0gbmFtZTogVGhlIG9yaWdpbmFsIGlkZW50aWZpZXIsIG9yIG51bGwuXG4gICAqL1xuICBJbmRleGVkU291cmNlTWFwQ29uc3VtZXIucHJvdG90eXBlLm9yaWdpbmFsUG9zaXRpb25Gb3IgPVxuICAgIGZ1bmN0aW9uIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lcl9vcmlnaW5hbFBvc2l0aW9uRm9yKGFBcmdzKSB7XG4gICAgICB2YXIgbmVlZGxlID0ge1xuICAgICAgICBnZW5lcmF0ZWRMaW5lOiB1dGlsLmdldEFyZyhhQXJncywgJ2xpbmUnKSxcbiAgICAgICAgZ2VuZXJhdGVkQ29sdW1uOiB1dGlsLmdldEFyZyhhQXJncywgJ2NvbHVtbicpXG4gICAgICB9O1xuXG4gICAgICAvLyBGaW5kIHRoZSBzZWN0aW9uIGNvbnRhaW5pbmcgdGhlIGdlbmVyYXRlZCBwb3NpdGlvbiB3ZSdyZSB0cnlpbmcgdG8gbWFwXG4gICAgICAvLyB0byBhbiBvcmlnaW5hbCBwb3NpdGlvbi5cbiAgICAgIHZhciBzZWN0aW9uSW5kZXggPSBiaW5hcnlTZWFyY2guc2VhcmNoKG5lZWRsZSwgdGhpcy5fc2VjdGlvbnMsXG4gICAgICAgIGZ1bmN0aW9uKG5lZWRsZSwgc2VjdGlvbikge1xuICAgICAgICAgIHZhciBjbXAgPSBuZWVkbGUuZ2VuZXJhdGVkTGluZSAtIHNlY3Rpb24uZ2VuZXJhdGVkT2Zmc2V0LmdlbmVyYXRlZExpbmU7XG4gICAgICAgICAgaWYgKGNtcCkge1xuICAgICAgICAgICAgcmV0dXJuIGNtcDtcbiAgICAgICAgICB9XG5cbiAgICAgICAgICByZXR1cm4gKG5lZWRsZS5nZW5lcmF0ZWRDb2x1bW4gLVxuICAgICAgICAgICAgICAgICAgc2VjdGlvbi5nZW5lcmF0ZWRPZmZzZXQuZ2VuZXJhdGVkQ29sdW1uKTtcbiAgICAgICAgfSk7XG4gICAgICB2YXIgc2VjdGlvbiA9IHRoaXMuX3NlY3Rpb25zW3NlY3Rpb25JbmRleF07XG5cbiAgICAgIGlmICghc2VjdGlvbikge1xuICAgICAgICByZXR1cm4ge1xuICAgICAgICAgIHNvdXJjZTogbnVsbCxcbiAgICAgICAgICBsaW5lOiBudWxsLFxuICAgICAgICAgIGNvbHVtbjogbnVsbCxcbiAgICAgICAgICBuYW1lOiBudWxsXG4gICAgICAgIH07XG4gICAgICB9XG5cbiAgICAgIHJldHVybiBzZWN0aW9uLmNvbnN1bWVyLm9yaWdpbmFsUG9zaXRpb25Gb3Ioe1xuICAgICAgICBsaW5lOiBuZWVkbGUuZ2VuZXJhdGVkTGluZSAtXG4gICAgICAgICAgKHNlY3Rpb24uZ2VuZXJhdGVkT2Zmc2V0LmdlbmVyYXRlZExpbmUgLSAxKSxcbiAgICAgICAgY29sdW1uOiBuZWVkbGUuZ2VuZXJhdGVkQ29sdW1uIC1cbiAgICAgICAgICAoc2VjdGlvbi5nZW5lcmF0ZWRPZmZzZXQuZ2VuZXJhdGVkTGluZSA9PT0gbmVlZGxlLmdlbmVyYXRlZExpbmVcbiAgICAgICAgICAgPyBzZWN0aW9uLmdlbmVyYXRlZE9mZnNldC5nZW5lcmF0ZWRDb2x1bW4gLSAxXG4gICAgICAgICAgIDogMCksXG4gICAgICAgIGJpYXM6IGFBcmdzLmJpYXNcbiAgICAgIH0pO1xuICAgIH07XG5cbiAgLyoqXG4gICAqIFJldHVybiB0cnVlIGlmIHdlIGhhdmUgdGhlIHNvdXJjZSBjb250ZW50IGZvciBldmVyeSBzb3VyY2UgaW4gdGhlIHNvdXJjZVxuICAgKiBtYXAsIGZhbHNlIG90aGVyd2lzZS5cbiAgICovXG4gIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuaGFzQ29udGVudHNPZkFsbFNvdXJjZXMgPVxuICAgIGZ1bmN0aW9uIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lcl9oYXNDb250ZW50c09mQWxsU291cmNlcygpIHtcbiAgICAgIHJldHVybiB0aGlzLl9zZWN0aW9ucy5ldmVyeShmdW5jdGlvbiAocykge1xuICAgICAgICByZXR1cm4gcy5jb25zdW1lci5oYXNDb250ZW50c09mQWxsU291cmNlcygpO1xuICAgICAgfSk7XG4gICAgfTtcblxuICAvKipcbiAgICogUmV0dXJucyB0aGUgb3JpZ2luYWwgc291cmNlIGNvbnRlbnQuIFRoZSBvbmx5IGFyZ3VtZW50IGlzIHRoZSB1cmwgb2YgdGhlXG4gICAqIG9yaWdpbmFsIHNvdXJjZSBmaWxlLiBSZXR1cm5zIG51bGwgaWYgbm8gb3JpZ2luYWwgc291cmNlIGNvbnRlbnQgaXNcbiAgICogYXZhaWxhYmxlLlxuICAgKi9cbiAgSW5kZXhlZFNvdXJjZU1hcENvbnN1bWVyLnByb3RvdHlwZS5zb3VyY2VDb250ZW50Rm9yID1cbiAgICBmdW5jdGlvbiBJbmRleGVkU291cmNlTWFwQ29uc3VtZXJfc291cmNlQ29udGVudEZvcihhU291cmNlLCBudWxsT25NaXNzaW5nKSB7XG4gICAgICBmb3IgKHZhciBpID0gMDsgaSA8IHRoaXMuX3NlY3Rpb25zLmxlbmd0aDsgaSsrKSB7XG4gICAgICAgIHZhciBzZWN0aW9uID0gdGhpcy5fc2VjdGlvbnNbaV07XG5cbiAgICAgICAgdmFyIGNvbnRlbnQgPSBzZWN0aW9uLmNvbnN1bWVyLnNvdXJjZUNvbnRlbnRGb3IoYVNvdXJjZSwgdHJ1ZSk7XG4gICAgICAgIGlmIChjb250ZW50KSB7XG4gICAgICAgICAgcmV0dXJuIGNvbnRlbnQ7XG4gICAgICAgIH1cbiAgICAgIH1cbiAgICAgIGlmIChudWxsT25NaXNzaW5nKSB7XG4gICAgICAgIHJldHVybiBudWxsO1xuICAgICAgfVxuICAgICAgZWxzZSB7XG4gICAgICAgIHRocm93IG5ldyBFcnJvcignXCInICsgYVNvdXJjZSArICdcIiBpcyBub3QgaW4gdGhlIFNvdXJjZU1hcC4nKTtcbiAgICAgIH1cbiAgICB9O1xuXG4gIC8qKlxuICAgKiBSZXR1cm5zIHRoZSBnZW5lcmF0ZWQgbGluZSBhbmQgY29sdW1uIGluZm9ybWF0aW9uIGZvciB0aGUgb3JpZ2luYWwgc291cmNlLFxuICAgKiBsaW5lLCBhbmQgY29sdW1uIHBvc2l0aW9ucyBwcm92aWRlZC4gVGhlIG9ubHkgYXJndW1lbnQgaXMgYW4gb2JqZWN0IHdpdGhcbiAgICogdGhlIGZvbGxvd2luZyBwcm9wZXJ0aWVzOlxuICAgKlxuICAgKiAgIC0gc291cmNlOiBUaGUgZmlsZW5hbWUgb2YgdGhlIG9yaWdpbmFsIHNvdXJjZS5cbiAgICogICAtIGxpbmU6IFRoZSBsaW5lIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLlxuICAgKiAgIC0gY29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgb3JpZ2luYWwgc291cmNlLlxuICAgKlxuICAgKiBhbmQgYW4gb2JqZWN0IGlzIHJldHVybmVkIHdpdGggdGhlIGZvbGxvd2luZyBwcm9wZXJ0aWVzOlxuICAgKlxuICAgKiAgIC0gbGluZTogVGhlIGxpbmUgbnVtYmVyIGluIHRoZSBnZW5lcmF0ZWQgc291cmNlLCBvciBudWxsLlxuICAgKiAgIC0gY29sdW1uOiBUaGUgY29sdW1uIG51bWJlciBpbiB0aGUgZ2VuZXJhdGVkIHNvdXJjZSwgb3IgbnVsbC5cbiAgICovXG4gIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuZ2VuZXJhdGVkUG9zaXRpb25Gb3IgPVxuICAgIGZ1bmN0aW9uIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lcl9nZW5lcmF0ZWRQb3NpdGlvbkZvcihhQXJncykge1xuICAgICAgZm9yICh2YXIgaSA9IDA7IGkgPCB0aGlzLl9zZWN0aW9ucy5sZW5ndGg7IGkrKykge1xuICAgICAgICB2YXIgc2VjdGlvbiA9IHRoaXMuX3NlY3Rpb25zW2ldO1xuXG4gICAgICAgIC8vIE9ubHkgY29uc2lkZXIgdGhpcyBzZWN0aW9uIGlmIHRoZSByZXF1ZXN0ZWQgc291cmNlIGlzIGluIHRoZSBsaXN0IG9mXG4gICAgICAgIC8vIHNvdXJjZXMgb2YgdGhlIGNvbnN1bWVyLlxuICAgICAgICBpZiAoc2VjdGlvbi5jb25zdW1lci5zb3VyY2VzLmluZGV4T2YodXRpbC5nZXRBcmcoYUFyZ3MsICdzb3VyY2UnKSkgPT09IC0xKSB7XG4gICAgICAgICAgY29udGludWU7XG4gICAgICAgIH1cbiAgICAgICAgdmFyIGdlbmVyYXRlZFBvc2l0aW9uID0gc2VjdGlvbi5jb25zdW1lci5nZW5lcmF0ZWRQb3NpdGlvbkZvcihhQXJncyk7XG4gICAgICAgIGlmIChnZW5lcmF0ZWRQb3NpdGlvbikge1xuICAgICAgICAgIHZhciByZXQgPSB7XG4gICAgICAgICAgICBsaW5lOiBnZW5lcmF0ZWRQb3NpdGlvbi5saW5lICtcbiAgICAgICAgICAgICAgKHNlY3Rpb24uZ2VuZXJhdGVkT2Zmc2V0LmdlbmVyYXRlZExpbmUgLSAxKSxcbiAgICAgICAgICAgIGNvbHVtbjogZ2VuZXJhdGVkUG9zaXRpb24uY29sdW1uICtcbiAgICAgICAgICAgICAgKHNlY3Rpb24uZ2VuZXJhdGVkT2Zmc2V0LmdlbmVyYXRlZExpbmUgPT09IGdlbmVyYXRlZFBvc2l0aW9uLmxpbmVcbiAgICAgICAgICAgICAgID8gc2VjdGlvbi5nZW5lcmF0ZWRPZmZzZXQuZ2VuZXJhdGVkQ29sdW1uIC0gMVxuICAgICAgICAgICAgICAgOiAwKVxuICAgICAgICAgIH07XG4gICAgICAgICAgcmV0dXJuIHJldDtcbiAgICAgICAgfVxuICAgICAgfVxuXG4gICAgICByZXR1cm4ge1xuICAgICAgICBsaW5lOiBudWxsLFxuICAgICAgICBjb2x1bW46IG51bGxcbiAgICAgIH07XG4gICAgfTtcblxuICAvKipcbiAgICogUGFyc2UgdGhlIG1hcHBpbmdzIGluIGEgc3RyaW5nIGluIHRvIGEgZGF0YSBzdHJ1Y3R1cmUgd2hpY2ggd2UgY2FuIGVhc2lseVxuICAgKiBxdWVyeSAodGhlIG9yZGVyZWQgYXJyYXlzIGluIHRoZSBgdGhpcy5fX2dlbmVyYXRlZE1hcHBpbmdzYCBhbmRcbiAgICogYHRoaXMuX19vcmlnaW5hbE1hcHBpbmdzYCBwcm9wZXJ0aWVzKS5cbiAgICovXG4gIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lci5wcm90b3R5cGUuX3BhcnNlTWFwcGluZ3MgPVxuICAgIGZ1bmN0aW9uIEluZGV4ZWRTb3VyY2VNYXBDb25zdW1lcl9wYXJzZU1hcHBpbmdzKGFTdHIsIGFTb3VyY2VSb290KSB7XG4gICAgICB0aGlzLl9fZ2VuZXJhdGVkTWFwcGluZ3MgPSBbXTtcbiAgICAgIHRoaXMuX19vcmlnaW5hbE1hcHBpbmdzID0gW107XG4gICAgICBmb3IgKHZhciBpID0gMDsgaSA8IHRoaXMuX3NlY3Rpb25zLmxlbmd0aDsgaSsrKSB7XG4gICAgICAgIHZhciBzZWN0aW9uID0gdGhpcy5fc2VjdGlvbnNbaV07XG4gICAgICAgIHZhciBzZWN0aW9uTWFwcGluZ3MgPSBzZWN0aW9uLmNvbnN1bWVyLl9nZW5lcmF0ZWRNYXBwaW5ncztcbiAgICAgICAgZm9yICh2YXIgaiA9IDA7IGogPCBzZWN0aW9uTWFwcGluZ3MubGVuZ3RoOyBqKyspIHtcbiAgICAgICAgICB2YXIgbWFwcGluZyA9IHNlY3Rpb25NYXBwaW5nc1tpXTtcblxuICAgICAgICAgIHZhciBzb3VyY2UgPSBzZWN0aW9uLmNvbnN1bWVyLl9zb3VyY2VzLmF0KG1hcHBpbmcuc291cmNlKTtcbiAgICAgICAgICBpZiAoc2VjdGlvbi5jb25zdW1lci5zb3VyY2VSb290ICE9PSBudWxsKSB7XG4gICAgICAgICAgICBzb3VyY2UgPSB1dGlsLmpvaW4oc2VjdGlvbi5jb25zdW1lci5zb3VyY2VSb290LCBzb3VyY2UpO1xuICAgICAgICAgIH1cbiAgICAgICAgICB0aGlzLl9zb3VyY2VzLmFkZChzb3VyY2UpO1xuICAgICAgICAgIHNvdXJjZSA9IHRoaXMuX3NvdXJjZXMuaW5kZXhPZihzb3VyY2UpO1xuXG4gICAgICAgICAgdmFyIG5hbWUgPSBzZWN0aW9uLmNvbnN1bWVyLl9uYW1lcy5hdChtYXBwaW5nLm5hbWUpO1xuICAgICAgICAgIHRoaXMuX25hbWVzLmFkZChuYW1lKTtcbiAgICAgICAgICBuYW1lID0gdGhpcy5fbmFtZXMuaW5kZXhPZihuYW1lKTtcblxuICAgICAgICAgIC8vIFRoZSBtYXBwaW5ncyBjb21pbmcgZnJvbSB0aGUgY29uc3VtZXIgZm9yIHRoZSBzZWN0aW9uIGhhdmVcbiAgICAgICAgICAvLyBnZW5lcmF0ZWQgcG9zaXRpb25zIHJlbGF0aXZlIHRvIHRoZSBzdGFydCBvZiB0aGUgc2VjdGlvbiwgc28gd2VcbiAgICAgICAgICAvLyBuZWVkIHRvIG9mZnNldCB0aGVtIHRvIGJlIHJlbGF0aXZlIHRvIHRoZSBzdGFydCBvZiB0aGUgY29uY2F0ZW5hdGVkXG4gICAgICAgICAgLy8gZ2VuZXJhdGVkIGZpbGUuXG4gICAgICAgICAgdmFyIGFkanVzdGVkTWFwcGluZyA9IHtcbiAgICAgICAgICAgIHNvdXJjZTogc291cmNlLFxuICAgICAgICAgICAgZ2VuZXJhdGVkTGluZTogbWFwcGluZy5nZW5lcmF0ZWRMaW5lICtcbiAgICAgICAgICAgICAgKHNlY3Rpb24uZ2VuZXJhdGVkT2Zmc2V0LmdlbmVyYXRlZExpbmUgLSAxKSxcbiAgICAgICAgICAgIGdlbmVyYXRlZENvbHVtbjogbWFwcGluZy5jb2x1bW4gK1xuICAgICAgICAgICAgICAoc2VjdGlvbi5nZW5lcmF0ZWRPZmZzZXQuZ2VuZXJhdGVkTGluZSA9PT0gbWFwcGluZy5nZW5lcmF0ZWRMaW5lKVxuICAgICAgICAgICAgICA/IHNlY3Rpb24uZ2VuZXJhdGVkT2Zmc2V0LmdlbmVyYXRlZENvbHVtbiAtIDFcbiAgICAgICAgICAgICAgOiAwLFxuICAgICAgICAgICAgb3JpZ2luYWxMaW5lOiBtYXBwaW5nLm9yaWdpbmFsTGluZSxcbiAgICAgICAgICAgIG9yaWdpbmFsQ29sdW1uOiBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uLFxuICAgICAgICAgICAgbmFtZTogbmFtZVxuICAgICAgICAgIH07XG5cbiAgICAgICAgICB0aGlzLl9fZ2VuZXJhdGVkTWFwcGluZ3MucHVzaChhZGp1c3RlZE1hcHBpbmcpO1xuICAgICAgICAgIGlmICh0eXBlb2YgYWRqdXN0ZWRNYXBwaW5nLm9yaWdpbmFsTGluZSA9PT0gJ251bWJlcicpIHtcbiAgICAgICAgICAgIHRoaXMuX19vcmlnaW5hbE1hcHBpbmdzLnB1c2goYWRqdXN0ZWRNYXBwaW5nKTtcbiAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICAgIH1cblxuICAgICAgcXVpY2tTb3J0KHRoaXMuX19nZW5lcmF0ZWRNYXBwaW5ncywgdXRpbC5jb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNEZWZsYXRlZCk7XG4gICAgICBxdWlja1NvcnQodGhpcy5fX29yaWdpbmFsTWFwcGluZ3MsIHV0aWwuY29tcGFyZUJ5T3JpZ2luYWxQb3NpdGlvbnMpO1xuICAgIH07XG5cbiAgZXhwb3J0cy5JbmRleGVkU291cmNlTWFwQ29uc3VtZXIgPSBJbmRleGVkU291cmNlTWFwQ29uc3VtZXI7XG59XG5cblxuXG4vKioqKioqKioqKioqKioqKipcbiAqKiBXRUJQQUNLIEZPT1RFUlxuICoqIC4vbGliL3NvdXJjZS1tYXAtY29uc3VtZXIuanNcbiAqKiBtb2R1bGUgaWQgPSAzXG4gKiogbW9kdWxlIGNodW5rcyA9IDBcbiAqKi8iLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG57XG4gIGV4cG9ydHMuR1JFQVRFU1RfTE9XRVJfQk9VTkQgPSAxO1xuICBleHBvcnRzLkxFQVNUX1VQUEVSX0JPVU5EID0gMjtcblxuICAvKipcbiAgICogUmVjdXJzaXZlIGltcGxlbWVudGF0aW9uIG9mIGJpbmFyeSBzZWFyY2guXG4gICAqXG4gICAqIEBwYXJhbSBhTG93IEluZGljZXMgaGVyZSBhbmQgbG93ZXIgZG8gbm90IGNvbnRhaW4gdGhlIG5lZWRsZS5cbiAgICogQHBhcmFtIGFIaWdoIEluZGljZXMgaGVyZSBhbmQgaGlnaGVyIGRvIG5vdCBjb250YWluIHRoZSBuZWVkbGUuXG4gICAqIEBwYXJhbSBhTmVlZGxlIFRoZSBlbGVtZW50IGJlaW5nIHNlYXJjaGVkIGZvci5cbiAgICogQHBhcmFtIGFIYXlzdGFjayBUaGUgbm9uLWVtcHR5IGFycmF5IGJlaW5nIHNlYXJjaGVkLlxuICAgKiBAcGFyYW0gYUNvbXBhcmUgRnVuY3Rpb24gd2hpY2ggdGFrZXMgdHdvIGVsZW1lbnRzIGFuZCByZXR1cm5zIC0xLCAwLCBvciAxLlxuICAgKiBAcGFyYW0gYUJpYXMgRWl0aGVyICdiaW5hcnlTZWFyY2guR1JFQVRFU1RfTE9XRVJfQk9VTkQnIG9yXG4gICAqICAgICAnYmluYXJ5U2VhcmNoLkxFQVNUX1VQUEVSX0JPVU5EJy4gU3BlY2lmaWVzIHdoZXRoZXIgdG8gcmV0dXJuIHRoZVxuICAgKiAgICAgY2xvc2VzdCBlbGVtZW50IHRoYXQgaXMgc21hbGxlciB0aGFuIG9yIGdyZWF0ZXIgdGhhbiB0aGUgb25lIHdlIGFyZVxuICAgKiAgICAgc2VhcmNoaW5nIGZvciwgcmVzcGVjdGl2ZWx5LCBpZiB0aGUgZXhhY3QgZWxlbWVudCBjYW5ub3QgYmUgZm91bmQuXG4gICAqL1xuICBmdW5jdGlvbiByZWN1cnNpdmVTZWFyY2goYUxvdywgYUhpZ2gsIGFOZWVkbGUsIGFIYXlzdGFjaywgYUNvbXBhcmUsIGFCaWFzKSB7XG4gICAgLy8gVGhpcyBmdW5jdGlvbiB0ZXJtaW5hdGVzIHdoZW4gb25lIG9mIHRoZSBmb2xsb3dpbmcgaXMgdHJ1ZTpcbiAgICAvL1xuICAgIC8vICAgMS4gV2UgZmluZCB0aGUgZXhhY3QgZWxlbWVudCB3ZSBhcmUgbG9va2luZyBmb3IuXG4gICAgLy9cbiAgICAvLyAgIDIuIFdlIGRpZCBub3QgZmluZCB0aGUgZXhhY3QgZWxlbWVudCwgYnV0IHdlIGNhbiByZXR1cm4gdGhlIGluZGV4IG9mXG4gICAgLy8gICAgICB0aGUgbmV4dC1jbG9zZXN0IGVsZW1lbnQuXG4gICAgLy9cbiAgICAvLyAgIDMuIFdlIGRpZCBub3QgZmluZCB0aGUgZXhhY3QgZWxlbWVudCwgYW5kIHRoZXJlIGlzIG5vIG5leHQtY2xvc2VzdFxuICAgIC8vICAgICAgZWxlbWVudCB0aGFuIHRoZSBvbmUgd2UgYXJlIHNlYXJjaGluZyBmb3IsIHNvIHdlIHJldHVybiAtMS5cbiAgICB2YXIgbWlkID0gTWF0aC5mbG9vcigoYUhpZ2ggLSBhTG93KSAvIDIpICsgYUxvdztcbiAgICB2YXIgY21wID0gYUNvbXBhcmUoYU5lZWRsZSwgYUhheXN0YWNrW21pZF0sIHRydWUpO1xuICAgIGlmIChjbXAgPT09IDApIHtcbiAgICAgIC8vIEZvdW5kIHRoZSBlbGVtZW50IHdlIGFyZSBsb29raW5nIGZvci5cbiAgICAgIHJldHVybiBtaWQ7XG4gICAgfVxuICAgIGVsc2UgaWYgKGNtcCA+IDApIHtcbiAgICAgIC8vIE91ciBuZWVkbGUgaXMgZ3JlYXRlciB0aGFuIGFIYXlzdGFja1ttaWRdLlxuICAgICAgaWYgKGFIaWdoIC0gbWlkID4gMSkge1xuICAgICAgICAvLyBUaGUgZWxlbWVudCBpcyBpbiB0aGUgdXBwZXIgaGFsZi5cbiAgICAgICAgcmV0dXJuIHJlY3Vyc2l2ZVNlYXJjaChtaWQsIGFIaWdoLCBhTmVlZGxlLCBhSGF5c3RhY2ssIGFDb21wYXJlLCBhQmlhcyk7XG4gICAgICB9XG5cbiAgICAgIC8vIFRoZSBleGFjdCBuZWVkbGUgZWxlbWVudCB3YXMgbm90IGZvdW5kIGluIHRoaXMgaGF5c3RhY2suIERldGVybWluZSBpZlxuICAgICAgLy8gd2UgYXJlIGluIHRlcm1pbmF0aW9uIGNhc2UgKDMpIG9yICgyKSBhbmQgcmV0dXJuIHRoZSBhcHByb3ByaWF0ZSB0aGluZy5cbiAgICAgIGlmIChhQmlhcyA9PSBleHBvcnRzLkxFQVNUX1VQUEVSX0JPVU5EKSB7XG4gICAgICAgIHJldHVybiBhSGlnaCA8IGFIYXlzdGFjay5sZW5ndGggPyBhSGlnaCA6IC0xO1xuICAgICAgfSBlbHNlIHtcbiAgICAgICAgcmV0dXJuIG1pZDtcbiAgICAgIH1cbiAgICB9XG4gICAgZWxzZSB7XG4gICAgICAvLyBPdXIgbmVlZGxlIGlzIGxlc3MgdGhhbiBhSGF5c3RhY2tbbWlkXS5cbiAgICAgIGlmIChtaWQgLSBhTG93ID4gMSkge1xuICAgICAgICAvLyBUaGUgZWxlbWVudCBpcyBpbiB0aGUgbG93ZXIgaGFsZi5cbiAgICAgICAgcmV0dXJuIHJlY3Vyc2l2ZVNlYXJjaChhTG93LCBtaWQsIGFOZWVkbGUsIGFIYXlzdGFjaywgYUNvbXBhcmUsIGFCaWFzKTtcbiAgICAgIH1cblxuICAgICAgLy8gd2UgYXJlIGluIHRlcm1pbmF0aW9uIGNhc2UgKDMpIG9yICgyKSBhbmQgcmV0dXJuIHRoZSBhcHByb3ByaWF0ZSB0aGluZy5cbiAgICAgIGlmIChhQmlhcyA9PSBleHBvcnRzLkxFQVNUX1VQUEVSX0JPVU5EKSB7XG4gICAgICAgIHJldHVybiBtaWQ7XG4gICAgICB9IGVsc2Uge1xuICAgICAgICByZXR1cm4gYUxvdyA8IDAgPyAtMSA6IGFMb3c7XG4gICAgICB9XG4gICAgfVxuICB9XG5cbiAgLyoqXG4gICAqIFRoaXMgaXMgYW4gaW1wbGVtZW50YXRpb24gb2YgYmluYXJ5IHNlYXJjaCB3aGljaCB3aWxsIGFsd2F5cyB0cnkgYW5kIHJldHVyblxuICAgKiB0aGUgaW5kZXggb2YgdGhlIGNsb3Nlc3QgZWxlbWVudCBpZiB0aGVyZSBpcyBubyBleGFjdCBoaXQuIFRoaXMgaXMgYmVjYXVzZVxuICAgKiBtYXBwaW5ncyBiZXR3ZWVuIG9yaWdpbmFsIGFuZCBnZW5lcmF0ZWQgbGluZS9jb2wgcGFpcnMgYXJlIHNpbmdsZSBwb2ludHMsXG4gICAqIGFuZCB0aGVyZSBpcyBhbiBpbXBsaWNpdCByZWdpb24gYmV0d2VlbiBlYWNoIG9mIHRoZW0sIHNvIGEgbWlzcyBqdXN0IG1lYW5zXG4gICAqIHRoYXQgeW91IGFyZW4ndCBvbiB0aGUgdmVyeSBzdGFydCBvZiBhIHJlZ2lvbi5cbiAgICpcbiAgICogQHBhcmFtIGFOZWVkbGUgVGhlIGVsZW1lbnQgeW91IGFyZSBsb29raW5nIGZvci5cbiAgICogQHBhcmFtIGFIYXlzdGFjayBUaGUgYXJyYXkgdGhhdCBpcyBiZWluZyBzZWFyY2hlZC5cbiAgICogQHBhcmFtIGFDb21wYXJlIEEgZnVuY3Rpb24gd2hpY2ggdGFrZXMgdGhlIG5lZWRsZSBhbmQgYW4gZWxlbWVudCBpbiB0aGVcbiAgICogICAgIGFycmF5IGFuZCByZXR1cm5zIC0xLCAwLCBvciAxIGRlcGVuZGluZyBvbiB3aGV0aGVyIHRoZSBuZWVkbGUgaXMgbGVzc1xuICAgKiAgICAgdGhhbiwgZXF1YWwgdG8sIG9yIGdyZWF0ZXIgdGhhbiB0aGUgZWxlbWVudCwgcmVzcGVjdGl2ZWx5LlxuICAgKiBAcGFyYW0gYUJpYXMgRWl0aGVyICdiaW5hcnlTZWFyY2guR1JFQVRFU1RfTE9XRVJfQk9VTkQnIG9yXG4gICAqICAgICAnYmluYXJ5U2VhcmNoLkxFQVNUX1VQUEVSX0JPVU5EJy4gU3BlY2lmaWVzIHdoZXRoZXIgdG8gcmV0dXJuIHRoZVxuICAgKiAgICAgY2xvc2VzdCBlbGVtZW50IHRoYXQgaXMgc21hbGxlciB0aGFuIG9yIGdyZWF0ZXIgdGhhbiB0aGUgb25lIHdlIGFyZVxuICAgKiAgICAgc2VhcmNoaW5nIGZvciwgcmVzcGVjdGl2ZWx5LCBpZiB0aGUgZXhhY3QgZWxlbWVudCBjYW5ub3QgYmUgZm91bmQuXG4gICAqICAgICBEZWZhdWx0cyB0byAnYmluYXJ5U2VhcmNoLkdSRUFURVNUX0xPV0VSX0JPVU5EJy5cbiAgICovXG4gIGV4cG9ydHMuc2VhcmNoID0gZnVuY3Rpb24gc2VhcmNoKGFOZWVkbGUsIGFIYXlzdGFjaywgYUNvbXBhcmUsIGFCaWFzKSB7XG4gICAgaWYgKGFIYXlzdGFjay5sZW5ndGggPT09IDApIHtcbiAgICAgIHJldHVybiAtMTtcbiAgICB9XG5cbiAgICB2YXIgaW5kZXggPSByZWN1cnNpdmVTZWFyY2goLTEsIGFIYXlzdGFjay5sZW5ndGgsIGFOZWVkbGUsIGFIYXlzdGFjayxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgYUNvbXBhcmUsIGFCaWFzIHx8IGV4cG9ydHMuR1JFQVRFU1RfTE9XRVJfQk9VTkQpO1xuICAgIGlmIChpbmRleCA8IDApIHtcbiAgICAgIHJldHVybiAtMTtcbiAgICB9XG5cbiAgICAvLyBXZSBoYXZlIGZvdW5kIGVpdGhlciB0aGUgZXhhY3QgZWxlbWVudCwgb3IgdGhlIG5leHQtY2xvc2VzdCBlbGVtZW50IHRoYW5cbiAgICAvLyB0aGUgb25lIHdlIGFyZSBzZWFyY2hpbmcgZm9yLiBIb3dldmVyLCB0aGVyZSBtYXkgYmUgbW9yZSB0aGFuIG9uZSBzdWNoXG4gICAgLy8gZWxlbWVudC4gTWFrZSBzdXJlIHdlIGFsd2F5cyByZXR1cm4gdGhlIHNtYWxsZXN0IG9mIHRoZXNlLlxuICAgIHdoaWxlIChpbmRleCAtIDEgPj0gMCkge1xuICAgICAgaWYgKGFDb21wYXJlKGFIYXlzdGFja1tpbmRleF0sIGFIYXlzdGFja1tpbmRleCAtIDFdLCB0cnVlKSAhPT0gMCkge1xuICAgICAgICBicmVhaztcbiAgICAgIH1cbiAgICAgIC0taW5kZXg7XG4gICAgfVxuXG4gICAgcmV0dXJuIGluZGV4O1xuICB9O1xufVxuXG5cblxuLyoqKioqKioqKioqKioqKioqXG4gKiogV0VCUEFDSyBGT09URVJcbiAqKiAuL2xpYi9iaW5hcnktc2VhcmNoLmpzXG4gKiogbW9kdWxlIGlkID0gNFxuICoqIG1vZHVsZSBjaHVua3MgPSAwXG4gKiovIiwiLyogLSotIE1vZGU6IGpzOyBqcy1pbmRlbnQtbGV2ZWw6IDI7IC0qLSAqL1xuLypcbiAqIENvcHlyaWdodCAyMDExIE1vemlsbGEgRm91bmRhdGlvbiBhbmQgY29udHJpYnV0b3JzXG4gKiBMaWNlbnNlZCB1bmRlciB0aGUgTmV3IEJTRCBsaWNlbnNlLiBTZWUgTElDRU5TRSBvcjpcbiAqIGh0dHA6Ly9vcGVuc291cmNlLm9yZy9saWNlbnNlcy9CU0QtMy1DbGF1c2VcbiAqL1xue1xuICB2YXIgdXRpbCA9IHJlcXVpcmUoJy4vdXRpbCcpO1xuXG4gIC8qKlxuICAgKiBBIGRhdGEgc3RydWN0dXJlIHdoaWNoIGlzIGEgY29tYmluYXRpb24gb2YgYW4gYXJyYXkgYW5kIGEgc2V0LiBBZGRpbmcgYSBuZXdcbiAgICogbWVtYmVyIGlzIE8oMSksIHRlc3RpbmcgZm9yIG1lbWJlcnNoaXAgaXMgTygxKSwgYW5kIGZpbmRpbmcgdGhlIGluZGV4IG9mIGFuXG4gICAqIGVsZW1lbnQgaXMgTygxKS4gUmVtb3ZpbmcgZWxlbWVudHMgZnJvbSB0aGUgc2V0IGlzIG5vdCBzdXBwb3J0ZWQuIE9ubHlcbiAgICogc3RyaW5ncyBhcmUgc3VwcG9ydGVkIGZvciBtZW1iZXJzaGlwLlxuICAgKi9cbiAgZnVuY3Rpb24gQXJyYXlTZXQoKSB7XG4gICAgdGhpcy5fYXJyYXkgPSBbXTtcbiAgICB0aGlzLl9zZXQgPSB7fTtcbiAgfVxuXG4gIC8qKlxuICAgKiBTdGF0aWMgbWV0aG9kIGZvciBjcmVhdGluZyBBcnJheVNldCBpbnN0YW5jZXMgZnJvbSBhbiBleGlzdGluZyBhcnJheS5cbiAgICovXG4gIEFycmF5U2V0LmZyb21BcnJheSA9IGZ1bmN0aW9uIEFycmF5U2V0X2Zyb21BcnJheShhQXJyYXksIGFBbGxvd0R1cGxpY2F0ZXMpIHtcbiAgICB2YXIgc2V0ID0gbmV3IEFycmF5U2V0KCk7XG4gICAgZm9yICh2YXIgaSA9IDAsIGxlbiA9IGFBcnJheS5sZW5ndGg7IGkgPCBsZW47IGkrKykge1xuICAgICAgc2V0LmFkZChhQXJyYXlbaV0sIGFBbGxvd0R1cGxpY2F0ZXMpO1xuICAgIH1cbiAgICByZXR1cm4gc2V0O1xuICB9O1xuXG4gIC8qKlxuICAgKiBSZXR1cm4gaG93IG1hbnkgdW5pcXVlIGl0ZW1zIGFyZSBpbiB0aGlzIEFycmF5U2V0LiBJZiBkdXBsaWNhdGVzIGhhdmUgYmVlblxuICAgKiBhZGRlZCwgdGhhbiB0aG9zZSBkbyBub3QgY291bnQgdG93YXJkcyB0aGUgc2l6ZS5cbiAgICpcbiAgICogQHJldHVybnMgTnVtYmVyXG4gICAqL1xuICBBcnJheVNldC5wcm90b3R5cGUuc2l6ZSA9IGZ1bmN0aW9uIEFycmF5U2V0X3NpemUoKSB7XG4gICAgcmV0dXJuIE9iamVjdC5nZXRPd25Qcm9wZXJ0eU5hbWVzKHRoaXMuX3NldCkubGVuZ3RoO1xuICB9O1xuXG4gIC8qKlxuICAgKiBBZGQgdGhlIGdpdmVuIHN0cmluZyB0byB0aGlzIHNldC5cbiAgICpcbiAgICogQHBhcmFtIFN0cmluZyBhU3RyXG4gICAqL1xuICBBcnJheVNldC5wcm90b3R5cGUuYWRkID0gZnVuY3Rpb24gQXJyYXlTZXRfYWRkKGFTdHIsIGFBbGxvd0R1cGxpY2F0ZXMpIHtcbiAgICB2YXIgc1N0ciA9IHV0aWwudG9TZXRTdHJpbmcoYVN0cik7XG4gICAgdmFyIGlzRHVwbGljYXRlID0gdGhpcy5fc2V0Lmhhc093blByb3BlcnR5KHNTdHIpO1xuICAgIHZhciBpZHggPSB0aGlzLl9hcnJheS5sZW5ndGg7XG4gICAgaWYgKCFpc0R1cGxpY2F0ZSB8fCBhQWxsb3dEdXBsaWNhdGVzKSB7XG4gICAgICB0aGlzLl9hcnJheS5wdXNoKGFTdHIpO1xuICAgIH1cbiAgICBpZiAoIWlzRHVwbGljYXRlKSB7XG4gICAgICB0aGlzLl9zZXRbc1N0cl0gPSBpZHg7XG4gICAgfVxuICB9O1xuXG4gIC8qKlxuICAgKiBJcyB0aGUgZ2l2ZW4gc3RyaW5nIGEgbWVtYmVyIG9mIHRoaXMgc2V0P1xuICAgKlxuICAgKiBAcGFyYW0gU3RyaW5nIGFTdHJcbiAgICovXG4gIEFycmF5U2V0LnByb3RvdHlwZS5oYXMgPSBmdW5jdGlvbiBBcnJheVNldF9oYXMoYVN0cikge1xuICAgIHZhciBzU3RyID0gdXRpbC50b1NldFN0cmluZyhhU3RyKTtcbiAgICByZXR1cm4gdGhpcy5fc2V0Lmhhc093blByb3BlcnR5KHNTdHIpO1xuICB9O1xuXG4gIC8qKlxuICAgKiBXaGF0IGlzIHRoZSBpbmRleCBvZiB0aGUgZ2l2ZW4gc3RyaW5nIGluIHRoZSBhcnJheT9cbiAgICpcbiAgICogQHBhcmFtIFN0cmluZyBhU3RyXG4gICAqL1xuICBBcnJheVNldC5wcm90b3R5cGUuaW5kZXhPZiA9IGZ1bmN0aW9uIEFycmF5U2V0X2luZGV4T2YoYVN0cikge1xuICAgIHZhciBzU3RyID0gdXRpbC50b1NldFN0cmluZyhhU3RyKTtcbiAgICBpZiAodGhpcy5fc2V0Lmhhc093blByb3BlcnR5KHNTdHIpKSB7XG4gICAgICByZXR1cm4gdGhpcy5fc2V0W3NTdHJdO1xuICAgIH1cbiAgICB0aHJvdyBuZXcgRXJyb3IoJ1wiJyArIGFTdHIgKyAnXCIgaXMgbm90IGluIHRoZSBzZXQuJyk7XG4gIH07XG5cbiAgLyoqXG4gICAqIFdoYXQgaXMgdGhlIGVsZW1lbnQgYXQgdGhlIGdpdmVuIGluZGV4P1xuICAgKlxuICAgKiBAcGFyYW0gTnVtYmVyIGFJZHhcbiAgICovXG4gIEFycmF5U2V0LnByb3RvdHlwZS5hdCA9IGZ1bmN0aW9uIEFycmF5U2V0X2F0KGFJZHgpIHtcbiAgICBpZiAoYUlkeCA+PSAwICYmIGFJZHggPCB0aGlzLl9hcnJheS5sZW5ndGgpIHtcbiAgICAgIHJldHVybiB0aGlzLl9hcnJheVthSWR4XTtcbiAgICB9XG4gICAgdGhyb3cgbmV3IEVycm9yKCdObyBlbGVtZW50IGluZGV4ZWQgYnkgJyArIGFJZHgpO1xuICB9O1xuXG4gIC8qKlxuICAgKiBSZXR1cm5zIHRoZSBhcnJheSByZXByZXNlbnRhdGlvbiBvZiB0aGlzIHNldCAod2hpY2ggaGFzIHRoZSBwcm9wZXIgaW5kaWNlc1xuICAgKiBpbmRpY2F0ZWQgYnkgaW5kZXhPZikuIE5vdGUgdGhhdCB0aGlzIGlzIGEgY29weSBvZiB0aGUgaW50ZXJuYWwgYXJyYXkgdXNlZFxuICAgKiBmb3Igc3RvcmluZyB0aGUgbWVtYmVycyBzbyB0aGF0IG5vIG9uZSBjYW4gbWVzcyB3aXRoIGludGVybmFsIHN0YXRlLlxuICAgKi9cbiAgQXJyYXlTZXQucHJvdG90eXBlLnRvQXJyYXkgPSBmdW5jdGlvbiBBcnJheVNldF90b0FycmF5KCkge1xuICAgIHJldHVybiB0aGlzLl9hcnJheS5zbGljZSgpO1xuICB9O1xuXG4gIGV4cG9ydHMuQXJyYXlTZXQgPSBBcnJheVNldDtcbn1cblxuXG5cbi8qKioqKioqKioqKioqKioqKlxuICoqIFdFQlBBQ0sgRk9PVEVSXG4gKiogLi9saWIvYXJyYXktc2V0LmpzXG4gKiogbW9kdWxlIGlkID0gNVxuICoqIG1vZHVsZSBjaHVua3MgPSAwXG4gKiovIiwiLyogLSotIE1vZGU6IGpzOyBqcy1pbmRlbnQtbGV2ZWw6IDI7IC0qLSAqL1xuLypcbiAqIENvcHlyaWdodCAyMDExIE1vemlsbGEgRm91bmRhdGlvbiBhbmQgY29udHJpYnV0b3JzXG4gKiBMaWNlbnNlZCB1bmRlciB0aGUgTmV3IEJTRCBsaWNlbnNlLiBTZWUgTElDRU5TRSBvcjpcbiAqIGh0dHA6Ly9vcGVuc291cmNlLm9yZy9saWNlbnNlcy9CU0QtMy1DbGF1c2VcbiAqXG4gKiBCYXNlZCBvbiB0aGUgQmFzZSA2NCBWTFEgaW1wbGVtZW50YXRpb24gaW4gQ2xvc3VyZSBDb21waWxlcjpcbiAqIGh0dHBzOi8vY29kZS5nb29nbGUuY29tL3AvY2xvc3VyZS1jb21waWxlci9zb3VyY2UvYnJvd3NlL3RydW5rL3NyYy9jb20vZ29vZ2xlL2RlYnVnZ2luZy9zb3VyY2VtYXAvQmFzZTY0VkxRLmphdmFcbiAqXG4gKiBDb3B5cmlnaHQgMjAxMSBUaGUgQ2xvc3VyZSBDb21waWxlciBBdXRob3JzLiBBbGwgcmlnaHRzIHJlc2VydmVkLlxuICogUmVkaXN0cmlidXRpb24gYW5kIHVzZSBpbiBzb3VyY2UgYW5kIGJpbmFyeSBmb3Jtcywgd2l0aCBvciB3aXRob3V0XG4gKiBtb2RpZmljYXRpb24sIGFyZSBwZXJtaXR0ZWQgcHJvdmlkZWQgdGhhdCB0aGUgZm9sbG93aW5nIGNvbmRpdGlvbnMgYXJlXG4gKiBtZXQ6XG4gKlxuICogICogUmVkaXN0cmlidXRpb25zIG9mIHNvdXJjZSBjb2RlIG11c3QgcmV0YWluIHRoZSBhYm92ZSBjb3B5cmlnaHRcbiAqICAgIG5vdGljZSwgdGhpcyBsaXN0IG9mIGNvbmRpdGlvbnMgYW5kIHRoZSBmb2xsb3dpbmcgZGlzY2xhaW1lci5cbiAqICAqIFJlZGlzdHJpYnV0aW9ucyBpbiBiaW5hcnkgZm9ybSBtdXN0IHJlcHJvZHVjZSB0aGUgYWJvdmVcbiAqICAgIGNvcHlyaWdodCBub3RpY2UsIHRoaXMgbGlzdCBvZiBjb25kaXRpb25zIGFuZCB0aGUgZm9sbG93aW5nXG4gKiAgICBkaXNjbGFpbWVyIGluIHRoZSBkb2N1bWVudGF0aW9uIGFuZC9vciBvdGhlciBtYXRlcmlhbHMgcHJvdmlkZWRcbiAqICAgIHdpdGggdGhlIGRpc3RyaWJ1dGlvbi5cbiAqICAqIE5laXRoZXIgdGhlIG5hbWUgb2YgR29vZ2xlIEluYy4gbm9yIHRoZSBuYW1lcyBvZiBpdHNcbiAqICAgIGNvbnRyaWJ1dG9ycyBtYXkgYmUgdXNlZCB0byBlbmRvcnNlIG9yIHByb21vdGUgcHJvZHVjdHMgZGVyaXZlZFxuICogICAgZnJvbSB0aGlzIHNvZnR3YXJlIHdpdGhvdXQgc3BlY2lmaWMgcHJpb3Igd3JpdHRlbiBwZXJtaXNzaW9uLlxuICpcbiAqIFRISVMgU09GVFdBUkUgSVMgUFJPVklERUQgQlkgVEhFIENPUFlSSUdIVCBIT0xERVJTIEFORCBDT05UUklCVVRPUlNcbiAqIFwiQVMgSVNcIiBBTkQgQU5ZIEVYUFJFU1MgT1IgSU1QTElFRCBXQVJSQU5USUVTLCBJTkNMVURJTkcsIEJVVCBOT1RcbiAqIExJTUlURUQgVE8sIFRIRSBJTVBMSUVEIFdBUlJBTlRJRVMgT0YgTUVSQ0hBTlRBQklMSVRZIEFORCBGSVRORVNTIEZPUlxuICogQSBQQVJUSUNVTEFSIFBVUlBPU0UgQVJFIERJU0NMQUlNRUQuIElOIE5PIEVWRU5UIFNIQUxMIFRIRSBDT1BZUklHSFRcbiAqIE9XTkVSIE9SIENPTlRSSUJVVE9SUyBCRSBMSUFCTEUgRk9SIEFOWSBESVJFQ1QsIElORElSRUNULCBJTkNJREVOVEFMLFxuICogU1BFQ0lBTCwgRVhFTVBMQVJZLCBPUiBDT05TRVFVRU5USUFMIERBTUFHRVMgKElOQ0xVRElORywgQlVUIE5PVFxuICogTElNSVRFRCBUTywgUFJPQ1VSRU1FTlQgT0YgU1VCU1RJVFVURSBHT09EUyBPUiBTRVJWSUNFUzsgTE9TUyBPRiBVU0UsXG4gKiBEQVRBLCBPUiBQUk9GSVRTOyBPUiBCVVNJTkVTUyBJTlRFUlJVUFRJT04pIEhPV0VWRVIgQ0FVU0VEIEFORCBPTiBBTllcbiAqIFRIRU9SWSBPRiBMSUFCSUxJVFksIFdIRVRIRVIgSU4gQ09OVFJBQ1QsIFNUUklDVCBMSUFCSUxJVFksIE9SIFRPUlRcbiAqIChJTkNMVURJTkcgTkVHTElHRU5DRSBPUiBPVEhFUldJU0UpIEFSSVNJTkcgSU4gQU5ZIFdBWSBPVVQgT0YgVEhFIFVTRVxuICogT0YgVEhJUyBTT0ZUV0FSRSwgRVZFTiBJRiBBRFZJU0VEIE9GIFRIRSBQT1NTSUJJTElUWSBPRiBTVUNIIERBTUFHRS5cbiAqL1xue1xuICB2YXIgYmFzZTY0ID0gcmVxdWlyZSgnLi9iYXNlNjQnKTtcblxuICAvLyBBIHNpbmdsZSBiYXNlIDY0IGRpZ2l0IGNhbiBjb250YWluIDYgYml0cyBvZiBkYXRhLiBGb3IgdGhlIGJhc2UgNjQgdmFyaWFibGVcbiAgLy8gbGVuZ3RoIHF1YW50aXRpZXMgd2UgdXNlIGluIHRoZSBzb3VyY2UgbWFwIHNwZWMsIHRoZSBmaXJzdCBiaXQgaXMgdGhlIHNpZ24sXG4gIC8vIHRoZSBuZXh0IGZvdXIgYml0cyBhcmUgdGhlIGFjdHVhbCB2YWx1ZSwgYW5kIHRoZSA2dGggYml0IGlzIHRoZVxuICAvLyBjb250aW51YXRpb24gYml0LiBUaGUgY29udGludWF0aW9uIGJpdCB0ZWxscyB1cyB3aGV0aGVyIHRoZXJlIGFyZSBtb3JlXG4gIC8vIGRpZ2l0cyBpbiB0aGlzIHZhbHVlIGZvbGxvd2luZyB0aGlzIGRpZ2l0LlxuICAvL1xuICAvLyAgIENvbnRpbnVhdGlvblxuICAvLyAgIHwgICAgU2lnblxuICAvLyAgIHwgICAgfFxuICAvLyAgIFYgICAgVlxuICAvLyAgIDEwMTAxMVxuXG4gIHZhciBWTFFfQkFTRV9TSElGVCA9IDU7XG5cbiAgLy8gYmluYXJ5OiAxMDAwMDBcbiAgdmFyIFZMUV9CQVNFID0gMSA8PCBWTFFfQkFTRV9TSElGVDtcblxuICAvLyBiaW5hcnk6IDAxMTExMVxuICB2YXIgVkxRX0JBU0VfTUFTSyA9IFZMUV9CQVNFIC0gMTtcblxuICAvLyBiaW5hcnk6IDEwMDAwMFxuICB2YXIgVkxRX0NPTlRJTlVBVElPTl9CSVQgPSBWTFFfQkFTRTtcblxuICAvKipcbiAgICogQ29udmVydHMgZnJvbSBhIHR3by1jb21wbGVtZW50IHZhbHVlIHRvIGEgdmFsdWUgd2hlcmUgdGhlIHNpZ24gYml0IGlzXG4gICAqIHBsYWNlZCBpbiB0aGUgbGVhc3Qgc2lnbmlmaWNhbnQgYml0LiAgRm9yIGV4YW1wbGUsIGFzIGRlY2ltYWxzOlxuICAgKiAgIDEgYmVjb21lcyAyICgxMCBiaW5hcnkpLCAtMSBiZWNvbWVzIDMgKDExIGJpbmFyeSlcbiAgICogICAyIGJlY29tZXMgNCAoMTAwIGJpbmFyeSksIC0yIGJlY29tZXMgNSAoMTAxIGJpbmFyeSlcbiAgICovXG4gIGZ1bmN0aW9uIHRvVkxRU2lnbmVkKGFWYWx1ZSkge1xuICAgIHJldHVybiBhVmFsdWUgPCAwXG4gICAgICA/ICgoLWFWYWx1ZSkgPDwgMSkgKyAxXG4gICAgICA6IChhVmFsdWUgPDwgMSkgKyAwO1xuICB9XG5cbiAgLyoqXG4gICAqIENvbnZlcnRzIHRvIGEgdHdvLWNvbXBsZW1lbnQgdmFsdWUgZnJvbSBhIHZhbHVlIHdoZXJlIHRoZSBzaWduIGJpdCBpc1xuICAgKiBwbGFjZWQgaW4gdGhlIGxlYXN0IHNpZ25pZmljYW50IGJpdC4gIEZvciBleGFtcGxlLCBhcyBkZWNpbWFsczpcbiAgICogICAyICgxMCBiaW5hcnkpIGJlY29tZXMgMSwgMyAoMTEgYmluYXJ5KSBiZWNvbWVzIC0xXG4gICAqICAgNCAoMTAwIGJpbmFyeSkgYmVjb21lcyAyLCA1ICgxMDEgYmluYXJ5KSBiZWNvbWVzIC0yXG4gICAqL1xuICBmdW5jdGlvbiBmcm9tVkxRU2lnbmVkKGFWYWx1ZSkge1xuICAgIHZhciBpc05lZ2F0aXZlID0gKGFWYWx1ZSAmIDEpID09PSAxO1xuICAgIHZhciBzaGlmdGVkID0gYVZhbHVlID4+IDE7XG4gICAgcmV0dXJuIGlzTmVnYXRpdmVcbiAgICAgID8gLXNoaWZ0ZWRcbiAgICAgIDogc2hpZnRlZDtcbiAgfVxuXG4gIC8qKlxuICAgKiBSZXR1cm5zIHRoZSBiYXNlIDY0IFZMUSBlbmNvZGVkIHZhbHVlLlxuICAgKi9cbiAgZXhwb3J0cy5lbmNvZGUgPSBmdW5jdGlvbiBiYXNlNjRWTFFfZW5jb2RlKGFWYWx1ZSkge1xuICAgIHZhciBlbmNvZGVkID0gXCJcIjtcbiAgICB2YXIgZGlnaXQ7XG5cbiAgICB2YXIgdmxxID0gdG9WTFFTaWduZWQoYVZhbHVlKTtcblxuICAgIGRvIHtcbiAgICAgIGRpZ2l0ID0gdmxxICYgVkxRX0JBU0VfTUFTSztcbiAgICAgIHZscSA+Pj49IFZMUV9CQVNFX1NISUZUO1xuICAgICAgaWYgKHZscSA+IDApIHtcbiAgICAgICAgLy8gVGhlcmUgYXJlIHN0aWxsIG1vcmUgZGlnaXRzIGluIHRoaXMgdmFsdWUsIHNvIHdlIG11c3QgbWFrZSBzdXJlIHRoZVxuICAgICAgICAvLyBjb250aW51YXRpb24gYml0IGlzIG1hcmtlZC5cbiAgICAgICAgZGlnaXQgfD0gVkxRX0NPTlRJTlVBVElPTl9CSVQ7XG4gICAgICB9XG4gICAgICBlbmNvZGVkICs9IGJhc2U2NC5lbmNvZGUoZGlnaXQpO1xuICAgIH0gd2hpbGUgKHZscSA+IDApO1xuXG4gICAgcmV0dXJuIGVuY29kZWQ7XG4gIH07XG5cbiAgLyoqXG4gICAqIERlY29kZXMgdGhlIG5leHQgYmFzZSA2NCBWTFEgdmFsdWUgZnJvbSB0aGUgZ2l2ZW4gc3RyaW5nIGFuZCByZXR1cm5zIHRoZVxuICAgKiB2YWx1ZSBhbmQgdGhlIHJlc3Qgb2YgdGhlIHN0cmluZyB2aWEgdGhlIG91dCBwYXJhbWV0ZXIuXG4gICAqL1xuICBleHBvcnRzLmRlY29kZSA9IGZ1bmN0aW9uIGJhc2U2NFZMUV9kZWNvZGUoYVN0ciwgYUluZGV4LCBhT3V0UGFyYW0pIHtcbiAgICB2YXIgc3RyTGVuID0gYVN0ci5sZW5ndGg7XG4gICAgdmFyIHJlc3VsdCA9IDA7XG4gICAgdmFyIHNoaWZ0ID0gMDtcbiAgICB2YXIgY29udGludWF0aW9uLCBkaWdpdDtcblxuICAgIGRvIHtcbiAgICAgIGlmIChhSW5kZXggPj0gc3RyTGVuKSB7XG4gICAgICAgIHRocm93IG5ldyBFcnJvcihcIkV4cGVjdGVkIG1vcmUgZGlnaXRzIGluIGJhc2UgNjQgVkxRIHZhbHVlLlwiKTtcbiAgICAgIH1cblxuICAgICAgZGlnaXQgPSBiYXNlNjQuZGVjb2RlKGFTdHIuY2hhckNvZGVBdChhSW5kZXgrKykpO1xuICAgICAgaWYgKGRpZ2l0ID09PSAtMSkge1xuICAgICAgICB0aHJvdyBuZXcgRXJyb3IoXCJJbnZhbGlkIGJhc2U2NCBkaWdpdDogXCIgKyBhU3RyLmNoYXJBdChhSW5kZXggLSAxKSk7XG4gICAgICB9XG5cbiAgICAgIGNvbnRpbnVhdGlvbiA9ICEhKGRpZ2l0ICYgVkxRX0NPTlRJTlVBVElPTl9CSVQpO1xuICAgICAgZGlnaXQgJj0gVkxRX0JBU0VfTUFTSztcbiAgICAgIHJlc3VsdCA9IHJlc3VsdCArIChkaWdpdCA8PCBzaGlmdCk7XG4gICAgICBzaGlmdCArPSBWTFFfQkFTRV9TSElGVDtcbiAgICB9IHdoaWxlIChjb250aW51YXRpb24pO1xuXG4gICAgYU91dFBhcmFtLnZhbHVlID0gZnJvbVZMUVNpZ25lZChyZXN1bHQpO1xuICAgIGFPdXRQYXJhbS5yZXN0ID0gYUluZGV4O1xuICB9O1xufVxuXG5cblxuLyoqKioqKioqKioqKioqKioqXG4gKiogV0VCUEFDSyBGT09URVJcbiAqKiAuL2xpYi9iYXNlNjQtdmxxLmpzXG4gKiogbW9kdWxlIGlkID0gNlxuICoqIG1vZHVsZSBjaHVua3MgPSAwXG4gKiovIiwiLyogLSotIE1vZGU6IGpzOyBqcy1pbmRlbnQtbGV2ZWw6IDI7IC0qLSAqL1xuLypcbiAqIENvcHlyaWdodCAyMDExIE1vemlsbGEgRm91bmRhdGlvbiBhbmQgY29udHJpYnV0b3JzXG4gKiBMaWNlbnNlZCB1bmRlciB0aGUgTmV3IEJTRCBsaWNlbnNlLiBTZWUgTElDRU5TRSBvcjpcbiAqIGh0dHA6Ly9vcGVuc291cmNlLm9yZy9saWNlbnNlcy9CU0QtMy1DbGF1c2VcbiAqL1xue1xuICB2YXIgaW50VG9DaGFyTWFwID0gJ0FCQ0RFRkdISUpLTE1OT1BRUlNUVVZXWFlaYWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXowMTIzNDU2Nzg5Ky8nLnNwbGl0KCcnKTtcblxuICAvKipcbiAgICogRW5jb2RlIGFuIGludGVnZXIgaW4gdGhlIHJhbmdlIG9mIDAgdG8gNjMgdG8gYSBzaW5nbGUgYmFzZSA2NCBkaWdpdC5cbiAgICovXG4gIGV4cG9ydHMuZW5jb2RlID0gZnVuY3Rpb24gKG51bWJlcikge1xuICAgIGlmICgwIDw9IG51bWJlciAmJiBudW1iZXIgPCBpbnRUb0NoYXJNYXAubGVuZ3RoKSB7XG4gICAgICByZXR1cm4gaW50VG9DaGFyTWFwW251bWJlcl07XG4gICAgfVxuICAgIHRocm93IG5ldyBUeXBlRXJyb3IoXCJNdXN0IGJlIGJldHdlZW4gMCBhbmQgNjM6IFwiICsgbnVtYmVyKTtcbiAgfTtcblxuICAvKipcbiAgICogRGVjb2RlIGEgc2luZ2xlIGJhc2UgNjQgY2hhcmFjdGVyIGNvZGUgZGlnaXQgdG8gYW4gaW50ZWdlci4gUmV0dXJucyAtMSBvblxuICAgKiBmYWlsdXJlLlxuICAgKi9cbiAgZXhwb3J0cy5kZWNvZGUgPSBmdW5jdGlvbiAoY2hhckNvZGUpIHtcbiAgICB2YXIgYmlnQSA9IDY1OyAgICAgLy8gJ0EnXG4gICAgdmFyIGJpZ1ogPSA5MDsgICAgIC8vICdaJ1xuXG4gICAgdmFyIGxpdHRsZUEgPSA5NzsgIC8vICdhJ1xuICAgIHZhciBsaXR0bGVaID0gMTIyOyAvLyAneidcblxuICAgIHZhciB6ZXJvID0gNDg7ICAgICAvLyAnMCdcbiAgICB2YXIgbmluZSA9IDU3OyAgICAgLy8gJzknXG5cbiAgICB2YXIgcGx1cyA9IDQzOyAgICAgLy8gJysnXG4gICAgdmFyIHNsYXNoID0gNDc7ICAgIC8vICcvJ1xuXG4gICAgdmFyIGxpdHRsZU9mZnNldCA9IDI2O1xuICAgIHZhciBudW1iZXJPZmZzZXQgPSA1MjtcblxuICAgIC8vIDAgLSAyNTogQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVpcbiAgICBpZiAoYmlnQSA8PSBjaGFyQ29kZSAmJiBjaGFyQ29kZSA8PSBiaWdaKSB7XG4gICAgICByZXR1cm4gKGNoYXJDb2RlIC0gYmlnQSk7XG4gICAgfVxuXG4gICAgLy8gMjYgLSA1MTogYWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXpcbiAgICBpZiAobGl0dGxlQSA8PSBjaGFyQ29kZSAmJiBjaGFyQ29kZSA8PSBsaXR0bGVaKSB7XG4gICAgICByZXR1cm4gKGNoYXJDb2RlIC0gbGl0dGxlQSArIGxpdHRsZU9mZnNldCk7XG4gICAgfVxuXG4gICAgLy8gNTIgLSA2MTogMDEyMzQ1Njc4OVxuICAgIGlmICh6ZXJvIDw9IGNoYXJDb2RlICYmIGNoYXJDb2RlIDw9IG5pbmUpIHtcbiAgICAgIHJldHVybiAoY2hhckNvZGUgLSB6ZXJvICsgbnVtYmVyT2Zmc2V0KTtcbiAgICB9XG5cbiAgICAvLyA2MjogK1xuICAgIGlmIChjaGFyQ29kZSA9PSBwbHVzKSB7XG4gICAgICByZXR1cm4gNjI7XG4gICAgfVxuXG4gICAgLy8gNjM6IC9cbiAgICBpZiAoY2hhckNvZGUgPT0gc2xhc2gpIHtcbiAgICAgIHJldHVybiA2MztcbiAgICB9XG5cbiAgICAvLyBJbnZhbGlkIGJhc2U2NCBkaWdpdC5cbiAgICByZXR1cm4gLTE7XG4gIH07XG59XG5cblxuXG4vKioqKioqKioqKioqKioqKipcbiAqKiBXRUJQQUNLIEZPT1RFUlxuICoqIC4vbGliL2Jhc2U2NC5qc1xuICoqIG1vZHVsZSBpZCA9IDdcbiAqKiBtb2R1bGUgY2h1bmtzID0gMFxuICoqLyIsIi8qIC0qLSBNb2RlOiBqczsganMtaW5kZW50LWxldmVsOiAyOyAtKi0gKi9cbi8qXG4gKiBDb3B5cmlnaHQgMjAxMSBNb3ppbGxhIEZvdW5kYXRpb24gYW5kIGNvbnRyaWJ1dG9yc1xuICogTGljZW5zZWQgdW5kZXIgdGhlIE5ldyBCU0QgbGljZW5zZS4gU2VlIExJQ0VOU0Ugb3I6XG4gKiBodHRwOi8vb3BlbnNvdXJjZS5vcmcvbGljZW5zZXMvQlNELTMtQ2xhdXNlXG4gKi9cbntcbiAgLy8gSXQgdHVybnMgb3V0IHRoYXQgc29tZSAobW9zdD8pIEphdmFTY3JpcHQgZW5naW5lcyBkb24ndCBzZWxmLWhvc3RcbiAgLy8gYEFycmF5LnByb3RvdHlwZS5zb3J0YC4gVGhpcyBtYWtlcyBzZW5zZSBiZWNhdXNlIEMrKyB3aWxsIGxpa2VseSByZW1haW5cbiAgLy8gZmFzdGVyIHRoYW4gSlMgd2hlbiBkb2luZyByYXcgQ1BVLWludGVuc2l2ZSBzb3J0aW5nLiBIb3dldmVyLCB3aGVuIHVzaW5nIGFcbiAgLy8gY3VzdG9tIGNvbXBhcmF0b3IgZnVuY3Rpb24sIGNhbGxpbmcgYmFjayBhbmQgZm9ydGggYmV0d2VlbiB0aGUgVk0ncyBDKysgYW5kXG4gIC8vIEpJVCdkIEpTIGlzIHJhdGhlciBzbG93ICphbmQqIGxvc2VzIEpJVCB0eXBlIGluZm9ybWF0aW9uLCByZXN1bHRpbmcgaW5cbiAgLy8gd29yc2UgZ2VuZXJhdGVkIGNvZGUgZm9yIHRoZSBjb21wYXJhdG9yIGZ1bmN0aW9uIHRoYW4gd291bGQgYmUgb3B0aW1hbC4gSW5cbiAgLy8gZmFjdCwgd2hlbiBzb3J0aW5nIHdpdGggYSBjb21wYXJhdG9yLCB0aGVzZSBjb3N0cyBvdXR3ZWlnaCB0aGUgYmVuZWZpdHMgb2ZcbiAgLy8gc29ydGluZyBpbiBDKysuIEJ5IHVzaW5nIG91ciBvd24gSlMtaW1wbGVtZW50ZWQgUXVpY2sgU29ydCAoYmVsb3cpLCB3ZSBnZXRcbiAgLy8gYSB+MzUwMG1zIG1lYW4gc3BlZWQtdXAgaW4gYGJlbmNoL2JlbmNoLmh0bWxgLlxuXG4gIC8qKlxuICAgKiBTd2FwIHRoZSBlbGVtZW50cyBpbmRleGVkIGJ5IGB4YCBhbmQgYHlgIGluIHRoZSBhcnJheSBgYXJ5YC5cbiAgICpcbiAgICogQHBhcmFtIHtBcnJheX0gYXJ5XG4gICAqICAgICAgICBUaGUgYXJyYXkuXG4gICAqIEBwYXJhbSB7TnVtYmVyfSB4XG4gICAqICAgICAgICBUaGUgaW5kZXggb2YgdGhlIGZpcnN0IGl0ZW0uXG4gICAqIEBwYXJhbSB7TnVtYmVyfSB5XG4gICAqICAgICAgICBUaGUgaW5kZXggb2YgdGhlIHNlY29uZCBpdGVtLlxuICAgKi9cbiAgZnVuY3Rpb24gc3dhcChhcnksIHgsIHkpIHtcbiAgICB2YXIgdGVtcCA9IGFyeVt4XTtcbiAgICBhcnlbeF0gPSBhcnlbeV07XG4gICAgYXJ5W3ldID0gdGVtcDtcbiAgfVxuXG4gIC8qKlxuICAgKiBSZXR1cm5zIGEgcmFuZG9tIGludGVnZXIgd2l0aGluIHRoZSByYW5nZSBgbG93IC4uIGhpZ2hgIGluY2x1c2l2ZS5cbiAgICpcbiAgICogQHBhcmFtIHtOdW1iZXJ9IGxvd1xuICAgKiAgICAgICAgVGhlIGxvd2VyIGJvdW5kIG9uIHRoZSByYW5nZS5cbiAgICogQHBhcmFtIHtOdW1iZXJ9IGhpZ2hcbiAgICogICAgICAgIFRoZSB1cHBlciBib3VuZCBvbiB0aGUgcmFuZ2UuXG4gICAqL1xuICBmdW5jdGlvbiByYW5kb21JbnRJblJhbmdlKGxvdywgaGlnaCkge1xuICAgIHJldHVybiBNYXRoLnJvdW5kKGxvdyArIChNYXRoLnJhbmRvbSgpICogKGhpZ2ggLSBsb3cpKSk7XG4gIH1cblxuICAvKipcbiAgICogVGhlIFF1aWNrIFNvcnQgYWxnb3JpdGhtLlxuICAgKlxuICAgKiBAcGFyYW0ge0FycmF5fSBhcnlcbiAgICogICAgICAgIEFuIGFycmF5IHRvIHNvcnQuXG4gICAqIEBwYXJhbSB7ZnVuY3Rpb259IGNvbXBhcmF0b3JcbiAgICogICAgICAgIEZ1bmN0aW9uIHRvIHVzZSB0byBjb21wYXJlIHR3byBpdGVtcy5cbiAgICogQHBhcmFtIHtOdW1iZXJ9IHBcbiAgICogICAgICAgIFN0YXJ0IGluZGV4IG9mIHRoZSBhcnJheVxuICAgKiBAcGFyYW0ge051bWJlcn0gclxuICAgKiAgICAgICAgRW5kIGluZGV4IG9mIHRoZSBhcnJheVxuICAgKi9cbiAgZnVuY3Rpb24gZG9RdWlja1NvcnQoYXJ5LCBjb21wYXJhdG9yLCBwLCByKSB7XG4gICAgLy8gSWYgb3VyIGxvd2VyIGJvdW5kIGlzIGxlc3MgdGhhbiBvdXIgdXBwZXIgYm91bmQsIHdlICgxKSBwYXJ0aXRpb24gdGhlXG4gICAgLy8gYXJyYXkgaW50byB0d28gcGllY2VzIGFuZCAoMikgcmVjdXJzZSBvbiBlYWNoIGhhbGYuIElmIGl0IGlzIG5vdCwgdGhpcyBpc1xuICAgIC8vIHRoZSBlbXB0eSBhcnJheSBhbmQgb3VyIGJhc2UgY2FzZS5cblxuICAgIGlmIChwIDwgcikge1xuICAgICAgLy8gKDEpIFBhcnRpdGlvbmluZy5cbiAgICAgIC8vXG4gICAgICAvLyBUaGUgcGFydGl0aW9uaW5nIGNob29zZXMgYSBwaXZvdCBiZXR3ZWVuIGBwYCBhbmQgYHJgIGFuZCBtb3ZlcyBhbGxcbiAgICAgIC8vIGVsZW1lbnRzIHRoYXQgYXJlIGxlc3MgdGhhbiBvciBlcXVhbCB0byB0aGUgcGl2b3QgdG8gdGhlIGJlZm9yZSBpdCwgYW5kXG4gICAgICAvLyBhbGwgdGhlIGVsZW1lbnRzIHRoYXQgYXJlIGdyZWF0ZXIgdGhhbiBpdCBhZnRlciBpdC4gVGhlIGVmZmVjdCBpcyB0aGF0XG4gICAgICAvLyBvbmNlIHBhcnRpdGlvbiBpcyBkb25lLCB0aGUgcGl2b3QgaXMgaW4gdGhlIGV4YWN0IHBsYWNlIGl0IHdpbGwgYmUgd2hlblxuICAgICAgLy8gdGhlIGFycmF5IGlzIHB1dCBpbiBzb3J0ZWQgb3JkZXIsIGFuZCBpdCB3aWxsIG5vdCBuZWVkIHRvIGJlIG1vdmVkXG4gICAgICAvLyBhZ2Fpbi4gVGhpcyBydW5zIGluIE8obikgdGltZS5cblxuICAgICAgLy8gQWx3YXlzIGNob29zZSBhIHJhbmRvbSBwaXZvdCBzbyB0aGF0IGFuIGlucHV0IGFycmF5IHdoaWNoIGlzIHJldmVyc2VcbiAgICAgIC8vIHNvcnRlZCBkb2VzIG5vdCBjYXVzZSBPKG5eMikgcnVubmluZyB0aW1lLlxuICAgICAgdmFyIHBpdm90SW5kZXggPSByYW5kb21JbnRJblJhbmdlKHAsIHIpO1xuICAgICAgdmFyIGkgPSBwIC0gMTtcblxuICAgICAgc3dhcChhcnksIHBpdm90SW5kZXgsIHIpO1xuICAgICAgdmFyIHBpdm90ID0gYXJ5W3JdO1xuXG4gICAgICAvLyBJbW1lZGlhdGVseSBhZnRlciBgamAgaXMgaW5jcmVtZW50ZWQgaW4gdGhpcyBsb29wLCB0aGUgZm9sbG93aW5nIGhvbGRcbiAgICAgIC8vIHRydWU6XG4gICAgICAvL1xuICAgICAgLy8gICAqIEV2ZXJ5IGVsZW1lbnQgaW4gYGFyeVtwIC4uIGldYCBpcyBsZXNzIHRoYW4gb3IgZXF1YWwgdG8gdGhlIHBpdm90LlxuICAgICAgLy9cbiAgICAgIC8vICAgKiBFdmVyeSBlbGVtZW50IGluIGBhcnlbaSsxIC4uIGotMV1gIGlzIGdyZWF0ZXIgdGhhbiB0aGUgcGl2b3QuXG4gICAgICBmb3IgKHZhciBqID0gcDsgaiA8IHI7IGorKykge1xuICAgICAgICBpZiAoY29tcGFyYXRvcihhcnlbal0sIHBpdm90KSA8PSAwKSB7XG4gICAgICAgICAgaSArPSAxO1xuICAgICAgICAgIHN3YXAoYXJ5LCBpLCBqKTtcbiAgICAgICAgfVxuICAgICAgfVxuXG4gICAgICBzd2FwKGFyeSwgaSArIDEsIGopO1xuICAgICAgdmFyIHEgPSBpICsgMTtcblxuICAgICAgLy8gKDIpIFJlY3Vyc2Ugb24gZWFjaCBoYWxmLlxuXG4gICAgICBkb1F1aWNrU29ydChhcnksIGNvbXBhcmF0b3IsIHAsIHEgLSAxKTtcbiAgICAgIGRvUXVpY2tTb3J0KGFyeSwgY29tcGFyYXRvciwgcSArIDEsIHIpO1xuICAgIH1cbiAgfVxuXG4gIC8qKlxuICAgKiBTb3J0IHRoZSBnaXZlbiBhcnJheSBpbi1wbGFjZSB3aXRoIHRoZSBnaXZlbiBjb21wYXJhdG9yIGZ1bmN0aW9uLlxuICAgKlxuICAgKiBAcGFyYW0ge0FycmF5fSBhcnlcbiAgICogICAgICAgIEFuIGFycmF5IHRvIHNvcnQuXG4gICAqIEBwYXJhbSB7ZnVuY3Rpb259IGNvbXBhcmF0b3JcbiAgICogICAgICAgIEZ1bmN0aW9uIHRvIHVzZSB0byBjb21wYXJlIHR3byBpdGVtcy5cbiAgICovXG4gIGV4cG9ydHMucXVpY2tTb3J0ID0gZnVuY3Rpb24gKGFyeSwgY29tcGFyYXRvcikge1xuICAgIGRvUXVpY2tTb3J0KGFyeSwgY29tcGFyYXRvciwgMCwgYXJ5Lmxlbmd0aCAtIDEpO1xuICB9O1xufVxuXG5cblxuLyoqKioqKioqKioqKioqKioqXG4gKiogV0VCUEFDSyBGT09URVJcbiAqKiAuL2xpYi9xdWljay1zb3J0LmpzXG4gKiogbW9kdWxlIGlkID0gOFxuICoqIG1vZHVsZSBjaHVua3MgPSAwXG4gKiovIiwiLyogLSotIE1vZGU6IGpzOyBqcy1pbmRlbnQtbGV2ZWw6IDI7IC0qLSAqL1xuLypcbiAqIENvcHlyaWdodCAyMDExIE1vemlsbGEgRm91bmRhdGlvbiBhbmQgY29udHJpYnV0b3JzXG4gKiBMaWNlbnNlZCB1bmRlciB0aGUgTmV3IEJTRCBsaWNlbnNlLiBTZWUgTElDRU5TRSBvcjpcbiAqIGh0dHA6Ly9vcGVuc291cmNlLm9yZy9saWNlbnNlcy9CU0QtMy1DbGF1c2VcbiAqL1xue1xuICB2YXIgYmFzZTY0VkxRID0gcmVxdWlyZSgnLi9iYXNlNjQtdmxxJyk7XG4gIHZhciB1dGlsID0gcmVxdWlyZSgnLi91dGlsJyk7XG4gIHZhciBBcnJheVNldCA9IHJlcXVpcmUoJy4vYXJyYXktc2V0JykuQXJyYXlTZXQ7XG4gIHZhciBNYXBwaW5nTGlzdCA9IHJlcXVpcmUoJy4vbWFwcGluZy1saXN0JykuTWFwcGluZ0xpc3Q7XG5cbiAgLyoqXG4gICAqIEFuIGluc3RhbmNlIG9mIHRoZSBTb3VyY2VNYXBHZW5lcmF0b3IgcmVwcmVzZW50cyBhIHNvdXJjZSBtYXAgd2hpY2ggaXNcbiAgICogYmVpbmcgYnVpbHQgaW5jcmVtZW50YWxseS4gWW91IG1heSBwYXNzIGFuIG9iamVjdCB3aXRoIHRoZSBmb2xsb3dpbmdcbiAgICogcHJvcGVydGllczpcbiAgICpcbiAgICogICAtIGZpbGU6IFRoZSBmaWxlbmFtZSBvZiB0aGUgZ2VuZXJhdGVkIHNvdXJjZS5cbiAgICogICAtIHNvdXJjZVJvb3Q6IEEgcm9vdCBmb3IgYWxsIHJlbGF0aXZlIFVSTHMgaW4gdGhpcyBzb3VyY2UgbWFwLlxuICAgKi9cbiAgZnVuY3Rpb24gU291cmNlTWFwR2VuZXJhdG9yKGFBcmdzKSB7XG4gICAgaWYgKCFhQXJncykge1xuICAgICAgYUFyZ3MgPSB7fTtcbiAgICB9XG4gICAgdGhpcy5fZmlsZSA9IHV0aWwuZ2V0QXJnKGFBcmdzLCAnZmlsZScsIG51bGwpO1xuICAgIHRoaXMuX3NvdXJjZVJvb3QgPSB1dGlsLmdldEFyZyhhQXJncywgJ3NvdXJjZVJvb3QnLCBudWxsKTtcbiAgICB0aGlzLl9za2lwVmFsaWRhdGlvbiA9IHV0aWwuZ2V0QXJnKGFBcmdzLCAnc2tpcFZhbGlkYXRpb24nLCBmYWxzZSk7XG4gICAgdGhpcy5fc291cmNlcyA9IG5ldyBBcnJheVNldCgpO1xuICAgIHRoaXMuX25hbWVzID0gbmV3IEFycmF5U2V0KCk7XG4gICAgdGhpcy5fbWFwcGluZ3MgPSBuZXcgTWFwcGluZ0xpc3QoKTtcbiAgICB0aGlzLl9zb3VyY2VzQ29udGVudHMgPSBudWxsO1xuICB9XG5cbiAgU291cmNlTWFwR2VuZXJhdG9yLnByb3RvdHlwZS5fdmVyc2lvbiA9IDM7XG5cbiAgLyoqXG4gICAqIENyZWF0ZXMgYSBuZXcgU291cmNlTWFwR2VuZXJhdG9yIGJhc2VkIG9uIGEgU291cmNlTWFwQ29uc3VtZXJcbiAgICpcbiAgICogQHBhcmFtIGFTb3VyY2VNYXBDb25zdW1lciBUaGUgU291cmNlTWFwLlxuICAgKi9cbiAgU291cmNlTWFwR2VuZXJhdG9yLmZyb21Tb3VyY2VNYXAgPVxuICAgIGZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcl9mcm9tU291cmNlTWFwKGFTb3VyY2VNYXBDb25zdW1lcikge1xuICAgICAgdmFyIHNvdXJjZVJvb3QgPSBhU291cmNlTWFwQ29uc3VtZXIuc291cmNlUm9vdDtcbiAgICAgIHZhciBnZW5lcmF0b3IgPSBuZXcgU291cmNlTWFwR2VuZXJhdG9yKHtcbiAgICAgICAgZmlsZTogYVNvdXJjZU1hcENvbnN1bWVyLmZpbGUsXG4gICAgICAgIHNvdXJjZVJvb3Q6IHNvdXJjZVJvb3RcbiAgICAgIH0pO1xuICAgICAgYVNvdXJjZU1hcENvbnN1bWVyLmVhY2hNYXBwaW5nKGZ1bmN0aW9uIChtYXBwaW5nKSB7XG4gICAgICAgIHZhciBuZXdNYXBwaW5nID0ge1xuICAgICAgICAgIGdlbmVyYXRlZDoge1xuICAgICAgICAgICAgbGluZTogbWFwcGluZy5nZW5lcmF0ZWRMaW5lLFxuICAgICAgICAgICAgY29sdW1uOiBtYXBwaW5nLmdlbmVyYXRlZENvbHVtblxuICAgICAgICAgIH1cbiAgICAgICAgfTtcblxuICAgICAgICBpZiAobWFwcGluZy5zb3VyY2UgIT0gbnVsbCkge1xuICAgICAgICAgIG5ld01hcHBpbmcuc291cmNlID0gbWFwcGluZy5zb3VyY2U7XG4gICAgICAgICAgaWYgKHNvdXJjZVJvb3QgIT0gbnVsbCkge1xuICAgICAgICAgICAgbmV3TWFwcGluZy5zb3VyY2UgPSB1dGlsLnJlbGF0aXZlKHNvdXJjZVJvb3QsIG5ld01hcHBpbmcuc291cmNlKTtcbiAgICAgICAgICB9XG5cbiAgICAgICAgICBuZXdNYXBwaW5nLm9yaWdpbmFsID0ge1xuICAgICAgICAgICAgbGluZTogbWFwcGluZy5vcmlnaW5hbExpbmUsXG4gICAgICAgICAgICBjb2x1bW46IG1hcHBpbmcub3JpZ2luYWxDb2x1bW5cbiAgICAgICAgICB9O1xuXG4gICAgICAgICAgaWYgKG1hcHBpbmcubmFtZSAhPSBudWxsKSB7XG4gICAgICAgICAgICBuZXdNYXBwaW5nLm5hbWUgPSBtYXBwaW5nLm5hbWU7XG4gICAgICAgICAgfVxuICAgICAgICB9XG5cbiAgICAgICAgZ2VuZXJhdG9yLmFkZE1hcHBpbmcobmV3TWFwcGluZyk7XG4gICAgICB9KTtcbiAgICAgIGFTb3VyY2VNYXBDb25zdW1lci5zb3VyY2VzLmZvckVhY2goZnVuY3Rpb24gKHNvdXJjZUZpbGUpIHtcbiAgICAgICAgdmFyIGNvbnRlbnQgPSBhU291cmNlTWFwQ29uc3VtZXIuc291cmNlQ29udGVudEZvcihzb3VyY2VGaWxlKTtcbiAgICAgICAgaWYgKGNvbnRlbnQgIT0gbnVsbCkge1xuICAgICAgICAgIGdlbmVyYXRvci5zZXRTb3VyY2VDb250ZW50KHNvdXJjZUZpbGUsIGNvbnRlbnQpO1xuICAgICAgICB9XG4gICAgICB9KTtcbiAgICAgIHJldHVybiBnZW5lcmF0b3I7XG4gICAgfTtcblxuICAvKipcbiAgICogQWRkIGEgc2luZ2xlIG1hcHBpbmcgZnJvbSBvcmlnaW5hbCBzb3VyY2UgbGluZSBhbmQgY29sdW1uIHRvIHRoZSBnZW5lcmF0ZWRcbiAgICogc291cmNlJ3MgbGluZSBhbmQgY29sdW1uIGZvciB0aGlzIHNvdXJjZSBtYXAgYmVpbmcgY3JlYXRlZC4gVGhlIG1hcHBpbmdcbiAgICogb2JqZWN0IHNob3VsZCBoYXZlIHRoZSBmb2xsb3dpbmcgcHJvcGVydGllczpcbiAgICpcbiAgICogICAtIGdlbmVyYXRlZDogQW4gb2JqZWN0IHdpdGggdGhlIGdlbmVyYXRlZCBsaW5lIGFuZCBjb2x1bW4gcG9zaXRpb25zLlxuICAgKiAgIC0gb3JpZ2luYWw6IEFuIG9iamVjdCB3aXRoIHRoZSBvcmlnaW5hbCBsaW5lIGFuZCBjb2x1bW4gcG9zaXRpb25zLlxuICAgKiAgIC0gc291cmNlOiBUaGUgb3JpZ2luYWwgc291cmNlIGZpbGUgKHJlbGF0aXZlIHRvIHRoZSBzb3VyY2VSb290KS5cbiAgICogICAtIG5hbWU6IEFuIG9wdGlvbmFsIG9yaWdpbmFsIHRva2VuIG5hbWUgZm9yIHRoaXMgbWFwcGluZy5cbiAgICovXG4gIFNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuYWRkTWFwcGluZyA9XG4gICAgZnVuY3Rpb24gU291cmNlTWFwR2VuZXJhdG9yX2FkZE1hcHBpbmcoYUFyZ3MpIHtcbiAgICAgIHZhciBnZW5lcmF0ZWQgPSB1dGlsLmdldEFyZyhhQXJncywgJ2dlbmVyYXRlZCcpO1xuICAgICAgdmFyIG9yaWdpbmFsID0gdXRpbC5nZXRBcmcoYUFyZ3MsICdvcmlnaW5hbCcsIG51bGwpO1xuICAgICAgdmFyIHNvdXJjZSA9IHV0aWwuZ2V0QXJnKGFBcmdzLCAnc291cmNlJywgbnVsbCk7XG4gICAgICB2YXIgbmFtZSA9IHV0aWwuZ2V0QXJnKGFBcmdzLCAnbmFtZScsIG51bGwpO1xuXG4gICAgICBpZiAoIXRoaXMuX3NraXBWYWxpZGF0aW9uKSB7XG4gICAgICAgIHRoaXMuX3ZhbGlkYXRlTWFwcGluZyhnZW5lcmF0ZWQsIG9yaWdpbmFsLCBzb3VyY2UsIG5hbWUpO1xuICAgICAgfVxuXG4gICAgICBpZiAoc291cmNlICE9IG51bGwgJiYgIXRoaXMuX3NvdXJjZXMuaGFzKHNvdXJjZSkpIHtcbiAgICAgICAgdGhpcy5fc291cmNlcy5hZGQoc291cmNlKTtcbiAgICAgIH1cblxuICAgICAgaWYgKG5hbWUgIT0gbnVsbCAmJiAhdGhpcy5fbmFtZXMuaGFzKG5hbWUpKSB7XG4gICAgICAgIHRoaXMuX25hbWVzLmFkZChuYW1lKTtcbiAgICAgIH1cblxuICAgICAgdGhpcy5fbWFwcGluZ3MuYWRkKHtcbiAgICAgICAgZ2VuZXJhdGVkTGluZTogZ2VuZXJhdGVkLmxpbmUsXG4gICAgICAgIGdlbmVyYXRlZENvbHVtbjogZ2VuZXJhdGVkLmNvbHVtbixcbiAgICAgICAgb3JpZ2luYWxMaW5lOiBvcmlnaW5hbCAhPSBudWxsICYmIG9yaWdpbmFsLmxpbmUsXG4gICAgICAgIG9yaWdpbmFsQ29sdW1uOiBvcmlnaW5hbCAhPSBudWxsICYmIG9yaWdpbmFsLmNvbHVtbixcbiAgICAgICAgc291cmNlOiBzb3VyY2UsXG4gICAgICAgIG5hbWU6IG5hbWVcbiAgICAgIH0pO1xuICAgIH07XG5cbiAgLyoqXG4gICAqIFNldCB0aGUgc291cmNlIGNvbnRlbnQgZm9yIGEgc291cmNlIGZpbGUuXG4gICAqL1xuICBTb3VyY2VNYXBHZW5lcmF0b3IucHJvdG90eXBlLnNldFNvdXJjZUNvbnRlbnQgPVxuICAgIGZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcl9zZXRTb3VyY2VDb250ZW50KGFTb3VyY2VGaWxlLCBhU291cmNlQ29udGVudCkge1xuICAgICAgdmFyIHNvdXJjZSA9IGFTb3VyY2VGaWxlO1xuICAgICAgaWYgKHRoaXMuX3NvdXJjZVJvb3QgIT0gbnVsbCkge1xuICAgICAgICBzb3VyY2UgPSB1dGlsLnJlbGF0aXZlKHRoaXMuX3NvdXJjZVJvb3QsIHNvdXJjZSk7XG4gICAgICB9XG5cbiAgICAgIGlmIChhU291cmNlQ29udGVudCAhPSBudWxsKSB7XG4gICAgICAgIC8vIEFkZCB0aGUgc291cmNlIGNvbnRlbnQgdG8gdGhlIF9zb3VyY2VzQ29udGVudHMgbWFwLlxuICAgICAgICAvLyBDcmVhdGUgYSBuZXcgX3NvdXJjZXNDb250ZW50cyBtYXAgaWYgdGhlIHByb3BlcnR5IGlzIG51bGwuXG4gICAgICAgIGlmICghdGhpcy5fc291cmNlc0NvbnRlbnRzKSB7XG4gICAgICAgICAgdGhpcy5fc291cmNlc0NvbnRlbnRzID0ge307XG4gICAgICAgIH1cbiAgICAgICAgdGhpcy5fc291cmNlc0NvbnRlbnRzW3V0aWwudG9TZXRTdHJpbmcoc291cmNlKV0gPSBhU291cmNlQ29udGVudDtcbiAgICAgIH0gZWxzZSBpZiAodGhpcy5fc291cmNlc0NvbnRlbnRzKSB7XG4gICAgICAgIC8vIFJlbW92ZSB0aGUgc291cmNlIGZpbGUgZnJvbSB0aGUgX3NvdXJjZXNDb250ZW50cyBtYXAuXG4gICAgICAgIC8vIElmIHRoZSBfc291cmNlc0NvbnRlbnRzIG1hcCBpcyBlbXB0eSwgc2V0IHRoZSBwcm9wZXJ0eSB0byBudWxsLlxuICAgICAgICBkZWxldGUgdGhpcy5fc291cmNlc0NvbnRlbnRzW3V0aWwudG9TZXRTdHJpbmcoc291cmNlKV07XG4gICAgICAgIGlmIChPYmplY3Qua2V5cyh0aGlzLl9zb3VyY2VzQ29udGVudHMpLmxlbmd0aCA9PT0gMCkge1xuICAgICAgICAgIHRoaXMuX3NvdXJjZXNDb250ZW50cyA9IG51bGw7XG4gICAgICAgIH1cbiAgICAgIH1cbiAgICB9O1xuXG4gIC8qKlxuICAgKiBBcHBsaWVzIHRoZSBtYXBwaW5ncyBvZiBhIHN1Yi1zb3VyY2UtbWFwIGZvciBhIHNwZWNpZmljIHNvdXJjZSBmaWxlIHRvIHRoZVxuICAgKiBzb3VyY2UgbWFwIGJlaW5nIGdlbmVyYXRlZC4gRWFjaCBtYXBwaW5nIHRvIHRoZSBzdXBwbGllZCBzb3VyY2UgZmlsZSBpc1xuICAgKiByZXdyaXR0ZW4gdXNpbmcgdGhlIHN1cHBsaWVkIHNvdXJjZSBtYXAuIE5vdGU6IFRoZSByZXNvbHV0aW9uIGZvciB0aGVcbiAgICogcmVzdWx0aW5nIG1hcHBpbmdzIGlzIHRoZSBtaW5pbWl1bSBvZiB0aGlzIG1hcCBhbmQgdGhlIHN1cHBsaWVkIG1hcC5cbiAgICpcbiAgICogQHBhcmFtIGFTb3VyY2VNYXBDb25zdW1lciBUaGUgc291cmNlIG1hcCB0byBiZSBhcHBsaWVkLlxuICAgKiBAcGFyYW0gYVNvdXJjZUZpbGUgT3B0aW9uYWwuIFRoZSBmaWxlbmFtZSBvZiB0aGUgc291cmNlIGZpbGUuXG4gICAqICAgICAgICBJZiBvbWl0dGVkLCBTb3VyY2VNYXBDb25zdW1lcidzIGZpbGUgcHJvcGVydHkgd2lsbCBiZSB1c2VkLlxuICAgKiBAcGFyYW0gYVNvdXJjZU1hcFBhdGggT3B0aW9uYWwuIFRoZSBkaXJuYW1lIG9mIHRoZSBwYXRoIHRvIHRoZSBzb3VyY2UgbWFwXG4gICAqICAgICAgICB0byBiZSBhcHBsaWVkLiBJZiByZWxhdGl2ZSwgaXQgaXMgcmVsYXRpdmUgdG8gdGhlIFNvdXJjZU1hcENvbnN1bWVyLlxuICAgKiAgICAgICAgVGhpcyBwYXJhbWV0ZXIgaXMgbmVlZGVkIHdoZW4gdGhlIHR3byBzb3VyY2UgbWFwcyBhcmVuJ3QgaW4gdGhlIHNhbWVcbiAgICogICAgICAgIGRpcmVjdG9yeSwgYW5kIHRoZSBzb3VyY2UgbWFwIHRvIGJlIGFwcGxpZWQgY29udGFpbnMgcmVsYXRpdmUgc291cmNlXG4gICAqICAgICAgICBwYXRocy4gSWYgc28sIHRob3NlIHJlbGF0aXZlIHNvdXJjZSBwYXRocyBuZWVkIHRvIGJlIHJld3JpdHRlblxuICAgKiAgICAgICAgcmVsYXRpdmUgdG8gdGhlIFNvdXJjZU1hcEdlbmVyYXRvci5cbiAgICovXG4gIFNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuYXBwbHlTb3VyY2VNYXAgPVxuICAgIGZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcl9hcHBseVNvdXJjZU1hcChhU291cmNlTWFwQ29uc3VtZXIsIGFTb3VyY2VGaWxlLCBhU291cmNlTWFwUGF0aCkge1xuICAgICAgdmFyIHNvdXJjZUZpbGUgPSBhU291cmNlRmlsZTtcbiAgICAgIC8vIElmIGFTb3VyY2VGaWxlIGlzIG9taXR0ZWQsIHdlIHdpbGwgdXNlIHRoZSBmaWxlIHByb3BlcnR5IG9mIHRoZSBTb3VyY2VNYXBcbiAgICAgIGlmIChhU291cmNlRmlsZSA9PSBudWxsKSB7XG4gICAgICAgIGlmIChhU291cmNlTWFwQ29uc3VtZXIuZmlsZSA9PSBudWxsKSB7XG4gICAgICAgICAgdGhyb3cgbmV3IEVycm9yKFxuICAgICAgICAgICAgJ1NvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuYXBwbHlTb3VyY2VNYXAgcmVxdWlyZXMgZWl0aGVyIGFuIGV4cGxpY2l0IHNvdXJjZSBmaWxlLCAnICtcbiAgICAgICAgICAgICdvciB0aGUgc291cmNlIG1hcFxcJ3MgXCJmaWxlXCIgcHJvcGVydHkuIEJvdGggd2VyZSBvbWl0dGVkLidcbiAgICAgICAgICApO1xuICAgICAgICB9XG4gICAgICAgIHNvdXJjZUZpbGUgPSBhU291cmNlTWFwQ29uc3VtZXIuZmlsZTtcbiAgICAgIH1cbiAgICAgIHZhciBzb3VyY2VSb290ID0gdGhpcy5fc291cmNlUm9vdDtcbiAgICAgIC8vIE1ha2UgXCJzb3VyY2VGaWxlXCIgcmVsYXRpdmUgaWYgYW4gYWJzb2x1dGUgVXJsIGlzIHBhc3NlZC5cbiAgICAgIGlmIChzb3VyY2VSb290ICE9IG51bGwpIHtcbiAgICAgICAgc291cmNlRmlsZSA9IHV0aWwucmVsYXRpdmUoc291cmNlUm9vdCwgc291cmNlRmlsZSk7XG4gICAgICB9XG4gICAgICAvLyBBcHBseWluZyB0aGUgU291cmNlTWFwIGNhbiBhZGQgYW5kIHJlbW92ZSBpdGVtcyBmcm9tIHRoZSBzb3VyY2VzIGFuZFxuICAgICAgLy8gdGhlIG5hbWVzIGFycmF5LlxuICAgICAgdmFyIG5ld1NvdXJjZXMgPSBuZXcgQXJyYXlTZXQoKTtcbiAgICAgIHZhciBuZXdOYW1lcyA9IG5ldyBBcnJheVNldCgpO1xuXG4gICAgICAvLyBGaW5kIG1hcHBpbmdzIGZvciB0aGUgXCJzb3VyY2VGaWxlXCJcbiAgICAgIHRoaXMuX21hcHBpbmdzLnVuc29ydGVkRm9yRWFjaChmdW5jdGlvbiAobWFwcGluZykge1xuICAgICAgICBpZiAobWFwcGluZy5zb3VyY2UgPT09IHNvdXJjZUZpbGUgJiYgbWFwcGluZy5vcmlnaW5hbExpbmUgIT0gbnVsbCkge1xuICAgICAgICAgIC8vIENoZWNrIGlmIGl0IGNhbiBiZSBtYXBwZWQgYnkgdGhlIHNvdXJjZSBtYXAsIHRoZW4gdXBkYXRlIHRoZSBtYXBwaW5nLlxuICAgICAgICAgIHZhciBvcmlnaW5hbCA9IGFTb3VyY2VNYXBDb25zdW1lci5vcmlnaW5hbFBvc2l0aW9uRm9yKHtcbiAgICAgICAgICAgIGxpbmU6IG1hcHBpbmcub3JpZ2luYWxMaW5lLFxuICAgICAgICAgICAgY29sdW1uOiBtYXBwaW5nLm9yaWdpbmFsQ29sdW1uXG4gICAgICAgICAgfSk7XG4gICAgICAgICAgaWYgKG9yaWdpbmFsLnNvdXJjZSAhPSBudWxsKSB7XG4gICAgICAgICAgICAvLyBDb3B5IG1hcHBpbmdcbiAgICAgICAgICAgIG1hcHBpbmcuc291cmNlID0gb3JpZ2luYWwuc291cmNlO1xuICAgICAgICAgICAgaWYgKGFTb3VyY2VNYXBQYXRoICE9IG51bGwpIHtcbiAgICAgICAgICAgICAgbWFwcGluZy5zb3VyY2UgPSB1dGlsLmpvaW4oYVNvdXJjZU1hcFBhdGgsIG1hcHBpbmcuc291cmNlKVxuICAgICAgICAgICAgfVxuICAgICAgICAgICAgaWYgKHNvdXJjZVJvb3QgIT0gbnVsbCkge1xuICAgICAgICAgICAgICBtYXBwaW5nLnNvdXJjZSA9IHV0aWwucmVsYXRpdmUoc291cmNlUm9vdCwgbWFwcGluZy5zb3VyY2UpO1xuICAgICAgICAgICAgfVxuICAgICAgICAgICAgbWFwcGluZy5vcmlnaW5hbExpbmUgPSBvcmlnaW5hbC5saW5lO1xuICAgICAgICAgICAgbWFwcGluZy5vcmlnaW5hbENvbHVtbiA9IG9yaWdpbmFsLmNvbHVtbjtcbiAgICAgICAgICAgIGlmIChvcmlnaW5hbC5uYW1lICE9IG51bGwpIHtcbiAgICAgICAgICAgICAgbWFwcGluZy5uYW1lID0gb3JpZ2luYWwubmFtZTtcbiAgICAgICAgICAgIH1cbiAgICAgICAgICB9XG4gICAgICAgIH1cblxuICAgICAgICB2YXIgc291cmNlID0gbWFwcGluZy5zb3VyY2U7XG4gICAgICAgIGlmIChzb3VyY2UgIT0gbnVsbCAmJiAhbmV3U291cmNlcy5oYXMoc291cmNlKSkge1xuICAgICAgICAgIG5ld1NvdXJjZXMuYWRkKHNvdXJjZSk7XG4gICAgICAgIH1cblxuICAgICAgICB2YXIgbmFtZSA9IG1hcHBpbmcubmFtZTtcbiAgICAgICAgaWYgKG5hbWUgIT0gbnVsbCAmJiAhbmV3TmFtZXMuaGFzKG5hbWUpKSB7XG4gICAgICAgICAgbmV3TmFtZXMuYWRkKG5hbWUpO1xuICAgICAgICB9XG5cbiAgICAgIH0sIHRoaXMpO1xuICAgICAgdGhpcy5fc291cmNlcyA9IG5ld1NvdXJjZXM7XG4gICAgICB0aGlzLl9uYW1lcyA9IG5ld05hbWVzO1xuXG4gICAgICAvLyBDb3B5IHNvdXJjZXNDb250ZW50cyBvZiBhcHBsaWVkIG1hcC5cbiAgICAgIGFTb3VyY2VNYXBDb25zdW1lci5zb3VyY2VzLmZvckVhY2goZnVuY3Rpb24gKHNvdXJjZUZpbGUpIHtcbiAgICAgICAgdmFyIGNvbnRlbnQgPSBhU291cmNlTWFwQ29uc3VtZXIuc291cmNlQ29udGVudEZvcihzb3VyY2VGaWxlKTtcbiAgICAgICAgaWYgKGNvbnRlbnQgIT0gbnVsbCkge1xuICAgICAgICAgIGlmIChhU291cmNlTWFwUGF0aCAhPSBudWxsKSB7XG4gICAgICAgICAgICBzb3VyY2VGaWxlID0gdXRpbC5qb2luKGFTb3VyY2VNYXBQYXRoLCBzb3VyY2VGaWxlKTtcbiAgICAgICAgICB9XG4gICAgICAgICAgaWYgKHNvdXJjZVJvb3QgIT0gbnVsbCkge1xuICAgICAgICAgICAgc291cmNlRmlsZSA9IHV0aWwucmVsYXRpdmUoc291cmNlUm9vdCwgc291cmNlRmlsZSk7XG4gICAgICAgICAgfVxuICAgICAgICAgIHRoaXMuc2V0U291cmNlQ29udGVudChzb3VyY2VGaWxlLCBjb250ZW50KTtcbiAgICAgICAgfVxuICAgICAgfSwgdGhpcyk7XG4gICAgfTtcblxuICAvKipcbiAgICogQSBtYXBwaW5nIGNhbiBoYXZlIG9uZSBvZiB0aGUgdGhyZWUgbGV2ZWxzIG9mIGRhdGE6XG4gICAqXG4gICAqICAgMS4gSnVzdCB0aGUgZ2VuZXJhdGVkIHBvc2l0aW9uLlxuICAgKiAgIDIuIFRoZSBHZW5lcmF0ZWQgcG9zaXRpb24sIG9yaWdpbmFsIHBvc2l0aW9uLCBhbmQgb3JpZ2luYWwgc291cmNlLlxuICAgKiAgIDMuIEdlbmVyYXRlZCBhbmQgb3JpZ2luYWwgcG9zaXRpb24sIG9yaWdpbmFsIHNvdXJjZSwgYXMgd2VsbCBhcyBhIG5hbWVcbiAgICogICAgICB0b2tlbi5cbiAgICpcbiAgICogVG8gbWFpbnRhaW4gY29uc2lzdGVuY3ksIHdlIHZhbGlkYXRlIHRoYXQgYW55IG5ldyBtYXBwaW5nIGJlaW5nIGFkZGVkIGZhbGxzXG4gICAqIGluIHRvIG9uZSBvZiB0aGVzZSBjYXRlZ29yaWVzLlxuICAgKi9cbiAgU291cmNlTWFwR2VuZXJhdG9yLnByb3RvdHlwZS5fdmFsaWRhdGVNYXBwaW5nID1cbiAgICBmdW5jdGlvbiBTb3VyY2VNYXBHZW5lcmF0b3JfdmFsaWRhdGVNYXBwaW5nKGFHZW5lcmF0ZWQsIGFPcmlnaW5hbCwgYVNvdXJjZSxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGFOYW1lKSB7XG4gICAgICBpZiAoYUdlbmVyYXRlZCAmJiAnbGluZScgaW4gYUdlbmVyYXRlZCAmJiAnY29sdW1uJyBpbiBhR2VuZXJhdGVkXG4gICAgICAgICAgJiYgYUdlbmVyYXRlZC5saW5lID4gMCAmJiBhR2VuZXJhdGVkLmNvbHVtbiA+PSAwXG4gICAgICAgICAgJiYgIWFPcmlnaW5hbCAmJiAhYVNvdXJjZSAmJiAhYU5hbWUpIHtcbiAgICAgICAgLy8gQ2FzZSAxLlxuICAgICAgICByZXR1cm47XG4gICAgICB9XG4gICAgICBlbHNlIGlmIChhR2VuZXJhdGVkICYmICdsaW5lJyBpbiBhR2VuZXJhdGVkICYmICdjb2x1bW4nIGluIGFHZW5lcmF0ZWRcbiAgICAgICAgICAgICAgICYmIGFPcmlnaW5hbCAmJiAnbGluZScgaW4gYU9yaWdpbmFsICYmICdjb2x1bW4nIGluIGFPcmlnaW5hbFxuICAgICAgICAgICAgICAgJiYgYUdlbmVyYXRlZC5saW5lID4gMCAmJiBhR2VuZXJhdGVkLmNvbHVtbiA+PSAwXG4gICAgICAgICAgICAgICAmJiBhT3JpZ2luYWwubGluZSA+IDAgJiYgYU9yaWdpbmFsLmNvbHVtbiA+PSAwXG4gICAgICAgICAgICAgICAmJiBhU291cmNlKSB7XG4gICAgICAgIC8vIENhc2VzIDIgYW5kIDMuXG4gICAgICAgIHJldHVybjtcbiAgICAgIH1cbiAgICAgIGVsc2Uge1xuICAgICAgICB0aHJvdyBuZXcgRXJyb3IoJ0ludmFsaWQgbWFwcGluZzogJyArIEpTT04uc3RyaW5naWZ5KHtcbiAgICAgICAgICBnZW5lcmF0ZWQ6IGFHZW5lcmF0ZWQsXG4gICAgICAgICAgc291cmNlOiBhU291cmNlLFxuICAgICAgICAgIG9yaWdpbmFsOiBhT3JpZ2luYWwsXG4gICAgICAgICAgbmFtZTogYU5hbWVcbiAgICAgICAgfSkpO1xuICAgICAgfVxuICAgIH07XG5cbiAgLyoqXG4gICAqIFNlcmlhbGl6ZSB0aGUgYWNjdW11bGF0ZWQgbWFwcGluZ3MgaW4gdG8gdGhlIHN0cmVhbSBvZiBiYXNlIDY0IFZMUXNcbiAgICogc3BlY2lmaWVkIGJ5IHRoZSBzb3VyY2UgbWFwIGZvcm1hdC5cbiAgICovXG4gIFNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUuX3NlcmlhbGl6ZU1hcHBpbmdzID1cbiAgICBmdW5jdGlvbiBTb3VyY2VNYXBHZW5lcmF0b3Jfc2VyaWFsaXplTWFwcGluZ3MoKSB7XG4gICAgICB2YXIgcHJldmlvdXNHZW5lcmF0ZWRDb2x1bW4gPSAwO1xuICAgICAgdmFyIHByZXZpb3VzR2VuZXJhdGVkTGluZSA9IDE7XG4gICAgICB2YXIgcHJldmlvdXNPcmlnaW5hbENvbHVtbiA9IDA7XG4gICAgICB2YXIgcHJldmlvdXNPcmlnaW5hbExpbmUgPSAwO1xuICAgICAgdmFyIHByZXZpb3VzTmFtZSA9IDA7XG4gICAgICB2YXIgcHJldmlvdXNTb3VyY2UgPSAwO1xuICAgICAgdmFyIHJlc3VsdCA9ICcnO1xuICAgICAgdmFyIG1hcHBpbmc7XG4gICAgICB2YXIgbmFtZUlkeDtcbiAgICAgIHZhciBzb3VyY2VJZHg7XG5cbiAgICAgIHZhciBtYXBwaW5ncyA9IHRoaXMuX21hcHBpbmdzLnRvQXJyYXkoKTtcbiAgICAgIGZvciAodmFyIGkgPSAwLCBsZW4gPSBtYXBwaW5ncy5sZW5ndGg7IGkgPCBsZW47IGkrKykge1xuICAgICAgICBtYXBwaW5nID0gbWFwcGluZ3NbaV07XG5cbiAgICAgICAgaWYgKG1hcHBpbmcuZ2VuZXJhdGVkTGluZSAhPT0gcHJldmlvdXNHZW5lcmF0ZWRMaW5lKSB7XG4gICAgICAgICAgcHJldmlvdXNHZW5lcmF0ZWRDb2x1bW4gPSAwO1xuICAgICAgICAgIHdoaWxlIChtYXBwaW5nLmdlbmVyYXRlZExpbmUgIT09IHByZXZpb3VzR2VuZXJhdGVkTGluZSkge1xuICAgICAgICAgICAgcmVzdWx0ICs9ICc7JztcbiAgICAgICAgICAgIHByZXZpb3VzR2VuZXJhdGVkTGluZSsrO1xuICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgICBlbHNlIHtcbiAgICAgICAgICBpZiAoaSA+IDApIHtcbiAgICAgICAgICAgIGlmICghdXRpbC5jb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNJbmZsYXRlZChtYXBwaW5nLCBtYXBwaW5nc1tpIC0gMV0pKSB7XG4gICAgICAgICAgICAgIGNvbnRpbnVlO1xuICAgICAgICAgICAgfVxuICAgICAgICAgICAgcmVzdWx0ICs9ICcsJztcbiAgICAgICAgICB9XG4gICAgICAgIH1cblxuICAgICAgICByZXN1bHQgKz0gYmFzZTY0VkxRLmVuY29kZShtYXBwaW5nLmdlbmVyYXRlZENvbHVtblxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAtIHByZXZpb3VzR2VuZXJhdGVkQ29sdW1uKTtcbiAgICAgICAgcHJldmlvdXNHZW5lcmF0ZWRDb2x1bW4gPSBtYXBwaW5nLmdlbmVyYXRlZENvbHVtbjtcblxuICAgICAgICBpZiAobWFwcGluZy5zb3VyY2UgIT0gbnVsbCkge1xuICAgICAgICAgIHNvdXJjZUlkeCA9IHRoaXMuX3NvdXJjZXMuaW5kZXhPZihtYXBwaW5nLnNvdXJjZSk7XG4gICAgICAgICAgcmVzdWx0ICs9IGJhc2U2NFZMUS5lbmNvZGUoc291cmNlSWR4IC0gcHJldmlvdXNTb3VyY2UpO1xuICAgICAgICAgIHByZXZpb3VzU291cmNlID0gc291cmNlSWR4O1xuXG4gICAgICAgICAgLy8gbGluZXMgYXJlIHN0b3JlZCAwLWJhc2VkIGluIFNvdXJjZU1hcCBzcGVjIHZlcnNpb24gM1xuICAgICAgICAgIHJlc3VsdCArPSBiYXNlNjRWTFEuZW5jb2RlKG1hcHBpbmcub3JpZ2luYWxMaW5lIC0gMVxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIC0gcHJldmlvdXNPcmlnaW5hbExpbmUpO1xuICAgICAgICAgIHByZXZpb3VzT3JpZ2luYWxMaW5lID0gbWFwcGluZy5vcmlnaW5hbExpbmUgLSAxO1xuXG4gICAgICAgICAgcmVzdWx0ICs9IGJhc2U2NFZMUS5lbmNvZGUobWFwcGluZy5vcmlnaW5hbENvbHVtblxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIC0gcHJldmlvdXNPcmlnaW5hbENvbHVtbik7XG4gICAgICAgICAgcHJldmlvdXNPcmlnaW5hbENvbHVtbiA9IG1hcHBpbmcub3JpZ2luYWxDb2x1bW47XG5cbiAgICAgICAgICBpZiAobWFwcGluZy5uYW1lICE9IG51bGwpIHtcbiAgICAgICAgICAgIG5hbWVJZHggPSB0aGlzLl9uYW1lcy5pbmRleE9mKG1hcHBpbmcubmFtZSk7XG4gICAgICAgICAgICByZXN1bHQgKz0gYmFzZTY0VkxRLmVuY29kZShuYW1lSWR4IC0gcHJldmlvdXNOYW1lKTtcbiAgICAgICAgICAgIHByZXZpb3VzTmFtZSA9IG5hbWVJZHg7XG4gICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICB9XG5cbiAgICAgIHJldHVybiByZXN1bHQ7XG4gICAgfTtcblxuICBTb3VyY2VNYXBHZW5lcmF0b3IucHJvdG90eXBlLl9nZW5lcmF0ZVNvdXJjZXNDb250ZW50ID1cbiAgICBmdW5jdGlvbiBTb3VyY2VNYXBHZW5lcmF0b3JfZ2VuZXJhdGVTb3VyY2VzQ29udGVudChhU291cmNlcywgYVNvdXJjZVJvb3QpIHtcbiAgICAgIHJldHVybiBhU291cmNlcy5tYXAoZnVuY3Rpb24gKHNvdXJjZSkge1xuICAgICAgICBpZiAoIXRoaXMuX3NvdXJjZXNDb250ZW50cykge1xuICAgICAgICAgIHJldHVybiBudWxsO1xuICAgICAgICB9XG4gICAgICAgIGlmIChhU291cmNlUm9vdCAhPSBudWxsKSB7XG4gICAgICAgICAgc291cmNlID0gdXRpbC5yZWxhdGl2ZShhU291cmNlUm9vdCwgc291cmNlKTtcbiAgICAgICAgfVxuICAgICAgICB2YXIga2V5ID0gdXRpbC50b1NldFN0cmluZyhzb3VyY2UpO1xuICAgICAgICByZXR1cm4gT2JqZWN0LnByb3RvdHlwZS5oYXNPd25Qcm9wZXJ0eS5jYWxsKHRoaXMuX3NvdXJjZXNDb250ZW50cyxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICBrZXkpXG4gICAgICAgICAgPyB0aGlzLl9zb3VyY2VzQ29udGVudHNba2V5XVxuICAgICAgICAgIDogbnVsbDtcbiAgICAgIH0sIHRoaXMpO1xuICAgIH07XG5cbiAgLyoqXG4gICAqIEV4dGVybmFsaXplIHRoZSBzb3VyY2UgbWFwLlxuICAgKi9cbiAgU291cmNlTWFwR2VuZXJhdG9yLnByb3RvdHlwZS50b0pTT04gPVxuICAgIGZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcl90b0pTT04oKSB7XG4gICAgICB2YXIgbWFwID0ge1xuICAgICAgICB2ZXJzaW9uOiB0aGlzLl92ZXJzaW9uLFxuICAgICAgICBzb3VyY2VzOiB0aGlzLl9zb3VyY2VzLnRvQXJyYXkoKSxcbiAgICAgICAgbmFtZXM6IHRoaXMuX25hbWVzLnRvQXJyYXkoKSxcbiAgICAgICAgbWFwcGluZ3M6IHRoaXMuX3NlcmlhbGl6ZU1hcHBpbmdzKClcbiAgICAgIH07XG4gICAgICBpZiAodGhpcy5fZmlsZSAhPSBudWxsKSB7XG4gICAgICAgIG1hcC5maWxlID0gdGhpcy5fZmlsZTtcbiAgICAgIH1cbiAgICAgIGlmICh0aGlzLl9zb3VyY2VSb290ICE9IG51bGwpIHtcbiAgICAgICAgbWFwLnNvdXJjZVJvb3QgPSB0aGlzLl9zb3VyY2VSb290O1xuICAgICAgfVxuICAgICAgaWYgKHRoaXMuX3NvdXJjZXNDb250ZW50cykge1xuICAgICAgICBtYXAuc291cmNlc0NvbnRlbnQgPSB0aGlzLl9nZW5lcmF0ZVNvdXJjZXNDb250ZW50KG1hcC5zb3VyY2VzLCBtYXAuc291cmNlUm9vdCk7XG4gICAgICB9XG5cbiAgICAgIHJldHVybiBtYXA7XG4gICAgfTtcblxuICAvKipcbiAgICogUmVuZGVyIHRoZSBzb3VyY2UgbWFwIGJlaW5nIGdlbmVyYXRlZCB0byBhIHN0cmluZy5cbiAgICovXG4gIFNvdXJjZU1hcEdlbmVyYXRvci5wcm90b3R5cGUudG9TdHJpbmcgPVxuICAgIGZ1bmN0aW9uIFNvdXJjZU1hcEdlbmVyYXRvcl90b1N0cmluZygpIHtcbiAgICAgIHJldHVybiBKU09OLnN0cmluZ2lmeSh0aGlzLnRvSlNPTigpKTtcbiAgICB9O1xuXG4gIGV4cG9ydHMuU291cmNlTWFwR2VuZXJhdG9yID0gU291cmNlTWFwR2VuZXJhdG9yO1xufVxuXG5cblxuLyoqKioqKioqKioqKioqKioqXG4gKiogV0VCUEFDSyBGT09URVJcbiAqKiAuL2xpYi9zb3VyY2UtbWFwLWdlbmVyYXRvci5qc1xuICoqIG1vZHVsZSBpZCA9IDlcbiAqKiBtb2R1bGUgY2h1bmtzID0gMFxuICoqLyIsIi8qIC0qLSBNb2RlOiBqczsganMtaW5kZW50LWxldmVsOiAyOyAtKi0gKi9cbi8qXG4gKiBDb3B5cmlnaHQgMjAxNCBNb3ppbGxhIEZvdW5kYXRpb24gYW5kIGNvbnRyaWJ1dG9yc1xuICogTGljZW5zZWQgdW5kZXIgdGhlIE5ldyBCU0QgbGljZW5zZS4gU2VlIExJQ0VOU0Ugb3I6XG4gKiBodHRwOi8vb3BlbnNvdXJjZS5vcmcvbGljZW5zZXMvQlNELTMtQ2xhdXNlXG4gKi9cbntcbiAgdmFyIHV0aWwgPSByZXF1aXJlKCcuL3V0aWwnKTtcblxuICAvKipcbiAgICogRGV0ZXJtaW5lIHdoZXRoZXIgbWFwcGluZ0IgaXMgYWZ0ZXIgbWFwcGluZ0Egd2l0aCByZXNwZWN0IHRvIGdlbmVyYXRlZFxuICAgKiBwb3NpdGlvbi5cbiAgICovXG4gIGZ1bmN0aW9uIGdlbmVyYXRlZFBvc2l0aW9uQWZ0ZXIobWFwcGluZ0EsIG1hcHBpbmdCKSB7XG4gICAgLy8gT3B0aW1pemVkIGZvciBtb3N0IGNvbW1vbiBjYXNlXG4gICAgdmFyIGxpbmVBID0gbWFwcGluZ0EuZ2VuZXJhdGVkTGluZTtcbiAgICB2YXIgbGluZUIgPSBtYXBwaW5nQi5nZW5lcmF0ZWRMaW5lO1xuICAgIHZhciBjb2x1bW5BID0gbWFwcGluZ0EuZ2VuZXJhdGVkQ29sdW1uO1xuICAgIHZhciBjb2x1bW5CID0gbWFwcGluZ0IuZ2VuZXJhdGVkQ29sdW1uO1xuICAgIHJldHVybiBsaW5lQiA+IGxpbmVBIHx8IGxpbmVCID09IGxpbmVBICYmIGNvbHVtbkIgPj0gY29sdW1uQSB8fFxuICAgICAgICAgICB1dGlsLmNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0luZmxhdGVkKG1hcHBpbmdBLCBtYXBwaW5nQikgPD0gMDtcbiAgfVxuXG4gIC8qKlxuICAgKiBBIGRhdGEgc3RydWN0dXJlIHRvIHByb3ZpZGUgYSBzb3J0ZWQgdmlldyBvZiBhY2N1bXVsYXRlZCBtYXBwaW5ncyBpbiBhXG4gICAqIHBlcmZvcm1hbmNlIGNvbnNjaW91cyBtYW5uZXIuIEl0IHRyYWRlcyBhIG5lZ2xpYmFibGUgb3ZlcmhlYWQgaW4gZ2VuZXJhbFxuICAgKiBjYXNlIGZvciBhIGxhcmdlIHNwZWVkdXAgaW4gY2FzZSBvZiBtYXBwaW5ncyBiZWluZyBhZGRlZCBpbiBvcmRlci5cbiAgICovXG4gIGZ1bmN0aW9uIE1hcHBpbmdMaXN0KCkge1xuICAgIHRoaXMuX2FycmF5ID0gW107XG4gICAgdGhpcy5fc29ydGVkID0gdHJ1ZTtcbiAgICAvLyBTZXJ2ZXMgYXMgaW5maW11bVxuICAgIHRoaXMuX2xhc3QgPSB7Z2VuZXJhdGVkTGluZTogLTEsIGdlbmVyYXRlZENvbHVtbjogMH07XG4gIH1cblxuICAvKipcbiAgICogSXRlcmF0ZSB0aHJvdWdoIGludGVybmFsIGl0ZW1zLiBUaGlzIG1ldGhvZCB0YWtlcyB0aGUgc2FtZSBhcmd1bWVudHMgdGhhdFxuICAgKiBgQXJyYXkucHJvdG90eXBlLmZvckVhY2hgIHRha2VzLlxuICAgKlxuICAgKiBOT1RFOiBUaGUgb3JkZXIgb2YgdGhlIG1hcHBpbmdzIGlzIE5PVCBndWFyYW50ZWVkLlxuICAgKi9cbiAgTWFwcGluZ0xpc3QucHJvdG90eXBlLnVuc29ydGVkRm9yRWFjaCA9XG4gICAgZnVuY3Rpb24gTWFwcGluZ0xpc3RfZm9yRWFjaChhQ2FsbGJhY2ssIGFUaGlzQXJnKSB7XG4gICAgICB0aGlzLl9hcnJheS5mb3JFYWNoKGFDYWxsYmFjaywgYVRoaXNBcmcpO1xuICAgIH07XG5cbiAgLyoqXG4gICAqIEFkZCB0aGUgZ2l2ZW4gc291cmNlIG1hcHBpbmcuXG4gICAqXG4gICAqIEBwYXJhbSBPYmplY3QgYU1hcHBpbmdcbiAgICovXG4gIE1hcHBpbmdMaXN0LnByb3RvdHlwZS5hZGQgPSBmdW5jdGlvbiBNYXBwaW5nTGlzdF9hZGQoYU1hcHBpbmcpIHtcbiAgICBpZiAoZ2VuZXJhdGVkUG9zaXRpb25BZnRlcih0aGlzLl9sYXN0LCBhTWFwcGluZykpIHtcbiAgICAgIHRoaXMuX2xhc3QgPSBhTWFwcGluZztcbiAgICAgIHRoaXMuX2FycmF5LnB1c2goYU1hcHBpbmcpO1xuICAgIH0gZWxzZSB7XG4gICAgICB0aGlzLl9zb3J0ZWQgPSBmYWxzZTtcbiAgICAgIHRoaXMuX2FycmF5LnB1c2goYU1hcHBpbmcpO1xuICAgIH1cbiAgfTtcblxuICAvKipcbiAgICogUmV0dXJucyB0aGUgZmxhdCwgc29ydGVkIGFycmF5IG9mIG1hcHBpbmdzLiBUaGUgbWFwcGluZ3MgYXJlIHNvcnRlZCBieVxuICAgKiBnZW5lcmF0ZWQgcG9zaXRpb24uXG4gICAqXG4gICAqIFdBUk5JTkc6IFRoaXMgbWV0aG9kIHJldHVybnMgaW50ZXJuYWwgZGF0YSB3aXRob3V0IGNvcHlpbmcsIGZvclxuICAgKiBwZXJmb3JtYW5jZS4gVGhlIHJldHVybiB2YWx1ZSBtdXN0IE5PVCBiZSBtdXRhdGVkLCBhbmQgc2hvdWxkIGJlIHRyZWF0ZWQgYXNcbiAgICogYW4gaW1tdXRhYmxlIGJvcnJvdy4gSWYgeW91IHdhbnQgdG8gdGFrZSBvd25lcnNoaXAsIHlvdSBtdXN0IG1ha2UgeW91ciBvd25cbiAgICogY29weS5cbiAgICovXG4gIE1hcHBpbmdMaXN0LnByb3RvdHlwZS50b0FycmF5ID0gZnVuY3Rpb24gTWFwcGluZ0xpc3RfdG9BcnJheSgpIHtcbiAgICBpZiAoIXRoaXMuX3NvcnRlZCkge1xuICAgICAgdGhpcy5fYXJyYXkuc29ydCh1dGlsLmNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0luZmxhdGVkKTtcbiAgICAgIHRoaXMuX3NvcnRlZCA9IHRydWU7XG4gICAgfVxuICAgIHJldHVybiB0aGlzLl9hcnJheTtcbiAgfTtcblxuICBleHBvcnRzLk1hcHBpbmdMaXN0ID0gTWFwcGluZ0xpc3Q7XG59XG5cblxuXG4vKioqKioqKioqKioqKioqKipcbiAqKiBXRUJQQUNLIEZPT1RFUlxuICoqIC4vbGliL21hcHBpbmctbGlzdC5qc1xuICoqIG1vZHVsZSBpZCA9IDEwXG4gKiogbW9kdWxlIGNodW5rcyA9IDBcbiAqKi8iXSwic291cmNlUm9vdCI6IiJ9