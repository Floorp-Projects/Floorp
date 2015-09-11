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
	  var binarySearch = __webpack_require__(1);
	
	  function numberCompare(a, b) {
	    return a - b;
	  }
	
	  exports['test too high with default (glb) bias'] = function (assert) {
	    var needle = 30;
	    var haystack = [2,4,6,8,10,12,14,16,18,20];
	
	    assert.doesNotThrow(function () {
	      binarySearch.search(needle, haystack, numberCompare);
	    });
	
	    assert.equal(haystack[binarySearch.search(needle, haystack, numberCompare)], 20);
	  };
	
	  exports['test too low with default (glb) bias'] = function (assert) {
	    var needle = 1;
	    var haystack = [2,4,6,8,10,12,14,16,18,20];
	
	    assert.doesNotThrow(function () {
	      binarySearch.search(needle, haystack, numberCompare);
	    });
	
	    assert.equal(binarySearch.search(needle, haystack, numberCompare), -1);
	  };
	
	  exports['test too high with lub bias'] = function (assert) {
	    var needle = 30;
	    var haystack = [2,4,6,8,10,12,14,16,18,20];
	
	    assert.doesNotThrow(function () {
	      binarySearch.search(needle, haystack, numberCompare);
	    });
	
	    assert.equal(binarySearch.search(needle, haystack, numberCompare,
	                                     binarySearch.LEAST_UPPER_BOUND), -1);
	  };
	
	  exports['test too low with lub bias'] = function (assert) {
	    var needle = 1;
	    var haystack = [2,4,6,8,10,12,14,16,18,20];
	
	    assert.doesNotThrow(function () {
	      binarySearch.search(needle, haystack, numberCompare);
	    });
	
	    assert.equal(haystack[binarySearch.search(needle, haystack, numberCompare,
	                                              binarySearch.LEAST_UPPER_BOUND)], 2);
	  };
	
	  exports['test exact search'] = function (assert) {
	    var needle = 4;
	    var haystack = [2,4,6,8,10,12,14,16,18,20];
	
	    assert.equal(haystack[binarySearch.search(needle, haystack, numberCompare)], 4);
	  };
	
	  exports['test fuzzy search with default (glb) bias'] = function (assert) {
	    var needle = 19;
	    var haystack = [2,4,6,8,10,12,14,16,18,20];
	
	    assert.equal(haystack[binarySearch.search(needle, haystack, numberCompare)], 18);
	  };
	
	  exports['test fuzzy search with lub bias'] = function (assert) {
	    var needle = 19;
	    var haystack = [2,4,6,8,10,12,14,16,18,20];
	
	    assert.equal(haystack[binarySearch.search(needle, haystack, numberCompare,
	                                              binarySearch.LEAST_UPPER_BOUND)], 20);
	  };
	
	  exports['test multiple matches'] = function (assert) {
	    var needle = 5;
	    var haystack = [1, 1, 2, 5, 5, 5, 13, 21];
	
	    assert.equal(binarySearch.search(needle, haystack, numberCompare,
	                                     binarySearch.LEAST_UPPER_BOUND), 3);
	  };
	
	  exports['test multiple matches at the beginning'] = function (assert) {
	    var needle = 1;
	    var haystack = [1, 1, 2, 5, 5, 5, 13, 21];
	
	    assert.equal(binarySearch.search(needle, haystack, numberCompare,
	                                     binarySearch.LEAST_UPPER_BOUND), 0);
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


/***/ }
/******/ ]);
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbIndlYnBhY2s6Ly8vd2VicGFjay9ib290c3RyYXAgYmI3MjVjOTVmZTk3YzY1OTQ3OGMiLCJ3ZWJwYWNrOi8vLy4vdGVzdC90ZXN0LWJpbmFyeS1zZWFyY2guanMiLCJ3ZWJwYWNrOi8vLy4vbGliL2JpbmFyeS1zZWFyY2guanMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6Ijs7Ozs7Ozs7Ozs7QUFBQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQSx1QkFBZTtBQUNmO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOzs7QUFHQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOzs7Ozs7O0FDdENBLGlCQUFnQixvQkFBb0I7QUFDcEM7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0EsTUFBSzs7QUFFTDtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0EsTUFBSzs7QUFFTDtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0EsTUFBSzs7QUFFTDtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQSxNQUFLOztBQUVMO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOzs7Ozs7O0FDaEdBLGlCQUFnQixvQkFBb0I7QUFDcEM7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQSxRQUFPO0FBQ1A7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBIiwiZmlsZSI6InRlc3RfYmluYXJ5X3NlYXJjaC5qcyIsInNvdXJjZXNDb250ZW50IjpbIiBcdC8vIFRoZSBtb2R1bGUgY2FjaGVcbiBcdHZhciBpbnN0YWxsZWRNb2R1bGVzID0ge307XG5cbiBcdC8vIFRoZSByZXF1aXJlIGZ1bmN0aW9uXG4gXHRmdW5jdGlvbiBfX3dlYnBhY2tfcmVxdWlyZV9fKG1vZHVsZUlkKSB7XG5cbiBcdFx0Ly8gQ2hlY2sgaWYgbW9kdWxlIGlzIGluIGNhY2hlXG4gXHRcdGlmKGluc3RhbGxlZE1vZHVsZXNbbW9kdWxlSWRdKVxuIFx0XHRcdHJldHVybiBpbnN0YWxsZWRNb2R1bGVzW21vZHVsZUlkXS5leHBvcnRzO1xuXG4gXHRcdC8vIENyZWF0ZSBhIG5ldyBtb2R1bGUgKGFuZCBwdXQgaXQgaW50byB0aGUgY2FjaGUpXG4gXHRcdHZhciBtb2R1bGUgPSBpbnN0YWxsZWRNb2R1bGVzW21vZHVsZUlkXSA9IHtcbiBcdFx0XHRleHBvcnRzOiB7fSxcbiBcdFx0XHRpZDogbW9kdWxlSWQsXG4gXHRcdFx0bG9hZGVkOiBmYWxzZVxuIFx0XHR9O1xuXG4gXHRcdC8vIEV4ZWN1dGUgdGhlIG1vZHVsZSBmdW5jdGlvblxuIFx0XHRtb2R1bGVzW21vZHVsZUlkXS5jYWxsKG1vZHVsZS5leHBvcnRzLCBtb2R1bGUsIG1vZHVsZS5leHBvcnRzLCBfX3dlYnBhY2tfcmVxdWlyZV9fKTtcblxuIFx0XHQvLyBGbGFnIHRoZSBtb2R1bGUgYXMgbG9hZGVkXG4gXHRcdG1vZHVsZS5sb2FkZWQgPSB0cnVlO1xuXG4gXHRcdC8vIFJldHVybiB0aGUgZXhwb3J0cyBvZiB0aGUgbW9kdWxlXG4gXHRcdHJldHVybiBtb2R1bGUuZXhwb3J0cztcbiBcdH1cblxuXG4gXHQvLyBleHBvc2UgdGhlIG1vZHVsZXMgb2JqZWN0IChfX3dlYnBhY2tfbW9kdWxlc19fKVxuIFx0X193ZWJwYWNrX3JlcXVpcmVfXy5tID0gbW9kdWxlcztcblxuIFx0Ly8gZXhwb3NlIHRoZSBtb2R1bGUgY2FjaGVcbiBcdF9fd2VicGFja19yZXF1aXJlX18uYyA9IGluc3RhbGxlZE1vZHVsZXM7XG5cbiBcdC8vIF9fd2VicGFja19wdWJsaWNfcGF0aF9fXG4gXHRfX3dlYnBhY2tfcmVxdWlyZV9fLnAgPSBcIlwiO1xuXG4gXHQvLyBMb2FkIGVudHJ5IG1vZHVsZSBhbmQgcmV0dXJuIGV4cG9ydHNcbiBcdHJldHVybiBfX3dlYnBhY2tfcmVxdWlyZV9fKDApO1xuXG5cblxuLyoqIFdFQlBBQ0sgRk9PVEVSICoqXG4gKiogd2VicGFjay9ib290c3RyYXAgYmI3MjVjOTVmZTk3YzY1OTQ3OGNcbiAqKi8iLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG57XG4gIHZhciBiaW5hcnlTZWFyY2ggPSByZXF1aXJlKCcuLi9saWIvYmluYXJ5LXNlYXJjaCcpO1xuXG4gIGZ1bmN0aW9uIG51bWJlckNvbXBhcmUoYSwgYikge1xuICAgIHJldHVybiBhIC0gYjtcbiAgfVxuXG4gIGV4cG9ydHNbJ3Rlc3QgdG9vIGhpZ2ggd2l0aCBkZWZhdWx0IChnbGIpIGJpYXMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgbmVlZGxlID0gMzA7XG4gICAgdmFyIGhheXN0YWNrID0gWzIsNCw2LDgsMTAsMTIsMTQsMTYsMTgsMjBdO1xuXG4gICAgYXNzZXJ0LmRvZXNOb3RUaHJvdyhmdW5jdGlvbiAoKSB7XG4gICAgICBiaW5hcnlTZWFyY2guc2VhcmNoKG5lZWRsZSwgaGF5c3RhY2ssIG51bWJlckNvbXBhcmUpO1xuICAgIH0pO1xuXG4gICAgYXNzZXJ0LmVxdWFsKGhheXN0YWNrW2JpbmFyeVNlYXJjaC5zZWFyY2gobmVlZGxlLCBoYXlzdGFjaywgbnVtYmVyQ29tcGFyZSldLCAyMCk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCB0b28gbG93IHdpdGggZGVmYXVsdCAoZ2xiKSBiaWFzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG5lZWRsZSA9IDE7XG4gICAgdmFyIGhheXN0YWNrID0gWzIsNCw2LDgsMTAsMTIsMTQsMTYsMTgsMjBdO1xuXG4gICAgYXNzZXJ0LmRvZXNOb3RUaHJvdyhmdW5jdGlvbiAoKSB7XG4gICAgICBiaW5hcnlTZWFyY2guc2VhcmNoKG5lZWRsZSwgaGF5c3RhY2ssIG51bWJlckNvbXBhcmUpO1xuICAgIH0pO1xuXG4gICAgYXNzZXJ0LmVxdWFsKGJpbmFyeVNlYXJjaC5zZWFyY2gobmVlZGxlLCBoYXlzdGFjaywgbnVtYmVyQ29tcGFyZSksIC0xKTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IHRvbyBoaWdoIHdpdGggbHViIGJpYXMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgbmVlZGxlID0gMzA7XG4gICAgdmFyIGhheXN0YWNrID0gWzIsNCw2LDgsMTAsMTIsMTQsMTYsMTgsMjBdO1xuXG4gICAgYXNzZXJ0LmRvZXNOb3RUaHJvdyhmdW5jdGlvbiAoKSB7XG4gICAgICBiaW5hcnlTZWFyY2guc2VhcmNoKG5lZWRsZSwgaGF5c3RhY2ssIG51bWJlckNvbXBhcmUpO1xuICAgIH0pO1xuXG4gICAgYXNzZXJ0LmVxdWFsKGJpbmFyeVNlYXJjaC5zZWFyY2gobmVlZGxlLCBoYXlzdGFjaywgbnVtYmVyQ29tcGFyZSxcbiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICBiaW5hcnlTZWFyY2guTEVBU1RfVVBQRVJfQk9VTkQpLCAtMSk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCB0b28gbG93IHdpdGggbHViIGJpYXMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgbmVlZGxlID0gMTtcbiAgICB2YXIgaGF5c3RhY2sgPSBbMiw0LDYsOCwxMCwxMiwxNCwxNiwxOCwyMF07XG5cbiAgICBhc3NlcnQuZG9lc05vdFRocm93KGZ1bmN0aW9uICgpIHtcbiAgICAgIGJpbmFyeVNlYXJjaC5zZWFyY2gobmVlZGxlLCBoYXlzdGFjaywgbnVtYmVyQ29tcGFyZSk7XG4gICAgfSk7XG5cbiAgICBhc3NlcnQuZXF1YWwoaGF5c3RhY2tbYmluYXJ5U2VhcmNoLnNlYXJjaChuZWVkbGUsIGhheXN0YWNrLCBudW1iZXJDb21wYXJlLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGJpbmFyeVNlYXJjaC5MRUFTVF9VUFBFUl9CT1VORCldLCAyKTtcbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IGV4YWN0IHNlYXJjaCddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBuZWVkbGUgPSA0O1xuICAgIHZhciBoYXlzdGFjayA9IFsyLDQsNiw4LDEwLDEyLDE0LDE2LDE4LDIwXTtcblxuICAgIGFzc2VydC5lcXVhbChoYXlzdGFja1tiaW5hcnlTZWFyY2guc2VhcmNoKG5lZWRsZSwgaGF5c3RhY2ssIG51bWJlckNvbXBhcmUpXSwgNCk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBmdXp6eSBzZWFyY2ggd2l0aCBkZWZhdWx0IChnbGIpIGJpYXMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgbmVlZGxlID0gMTk7XG4gICAgdmFyIGhheXN0YWNrID0gWzIsNCw2LDgsMTAsMTIsMTQsMTYsMTgsMjBdO1xuXG4gICAgYXNzZXJ0LmVxdWFsKGhheXN0YWNrW2JpbmFyeVNlYXJjaC5zZWFyY2gobmVlZGxlLCBoYXlzdGFjaywgbnVtYmVyQ29tcGFyZSldLCAxOCk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBmdXp6eSBzZWFyY2ggd2l0aCBsdWIgYmlhcyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBuZWVkbGUgPSAxOTtcbiAgICB2YXIgaGF5c3RhY2sgPSBbMiw0LDYsOCwxMCwxMiwxNCwxNiwxOCwyMF07XG5cbiAgICBhc3NlcnQuZXF1YWwoaGF5c3RhY2tbYmluYXJ5U2VhcmNoLnNlYXJjaChuZWVkbGUsIGhheXN0YWNrLCBudW1iZXJDb21wYXJlLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGJpbmFyeVNlYXJjaC5MRUFTVF9VUFBFUl9CT1VORCldLCAyMCk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCBtdWx0aXBsZSBtYXRjaGVzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG5lZWRsZSA9IDU7XG4gICAgdmFyIGhheXN0YWNrID0gWzEsIDEsIDIsIDUsIDUsIDUsIDEzLCAyMV07XG5cbiAgICBhc3NlcnQuZXF1YWwoYmluYXJ5U2VhcmNoLnNlYXJjaChuZWVkbGUsIGhheXN0YWNrLCBudW1iZXJDb21wYXJlLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGJpbmFyeVNlYXJjaC5MRUFTVF9VUFBFUl9CT1VORCksIDMpO1xuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3QgbXVsdGlwbGUgbWF0Y2hlcyBhdCB0aGUgYmVnaW5uaW5nJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIG5lZWRsZSA9IDE7XG4gICAgdmFyIGhheXN0YWNrID0gWzEsIDEsIDIsIDUsIDUsIDUsIDEzLCAyMV07XG5cbiAgICBhc3NlcnQuZXF1YWwoYmluYXJ5U2VhcmNoLnNlYXJjaChuZWVkbGUsIGhheXN0YWNrLCBudW1iZXJDb21wYXJlLFxuICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGJpbmFyeVNlYXJjaC5MRUFTVF9VUFBFUl9CT1VORCksIDApO1xuICB9O1xufVxuXG5cblxuLyoqKioqKioqKioqKioqKioqXG4gKiogV0VCUEFDSyBGT09URVJcbiAqKiAuL3Rlc3QvdGVzdC1iaW5hcnktc2VhcmNoLmpzXG4gKiogbW9kdWxlIGlkID0gMFxuICoqIG1vZHVsZSBjaHVua3MgPSAwXG4gKiovIiwiLyogLSotIE1vZGU6IGpzOyBqcy1pbmRlbnQtbGV2ZWw6IDI7IC0qLSAqL1xuLypcbiAqIENvcHlyaWdodCAyMDExIE1vemlsbGEgRm91bmRhdGlvbiBhbmQgY29udHJpYnV0b3JzXG4gKiBMaWNlbnNlZCB1bmRlciB0aGUgTmV3IEJTRCBsaWNlbnNlLiBTZWUgTElDRU5TRSBvcjpcbiAqIGh0dHA6Ly9vcGVuc291cmNlLm9yZy9saWNlbnNlcy9CU0QtMy1DbGF1c2VcbiAqL1xue1xuICBleHBvcnRzLkdSRUFURVNUX0xPV0VSX0JPVU5EID0gMTtcbiAgZXhwb3J0cy5MRUFTVF9VUFBFUl9CT1VORCA9IDI7XG5cbiAgLyoqXG4gICAqIFJlY3Vyc2l2ZSBpbXBsZW1lbnRhdGlvbiBvZiBiaW5hcnkgc2VhcmNoLlxuICAgKlxuICAgKiBAcGFyYW0gYUxvdyBJbmRpY2VzIGhlcmUgYW5kIGxvd2VyIGRvIG5vdCBjb250YWluIHRoZSBuZWVkbGUuXG4gICAqIEBwYXJhbSBhSGlnaCBJbmRpY2VzIGhlcmUgYW5kIGhpZ2hlciBkbyBub3QgY29udGFpbiB0aGUgbmVlZGxlLlxuICAgKiBAcGFyYW0gYU5lZWRsZSBUaGUgZWxlbWVudCBiZWluZyBzZWFyY2hlZCBmb3IuXG4gICAqIEBwYXJhbSBhSGF5c3RhY2sgVGhlIG5vbi1lbXB0eSBhcnJheSBiZWluZyBzZWFyY2hlZC5cbiAgICogQHBhcmFtIGFDb21wYXJlIEZ1bmN0aW9uIHdoaWNoIHRha2VzIHR3byBlbGVtZW50cyBhbmQgcmV0dXJucyAtMSwgMCwgb3IgMS5cbiAgICogQHBhcmFtIGFCaWFzIEVpdGhlciAnYmluYXJ5U2VhcmNoLkdSRUFURVNUX0xPV0VSX0JPVU5EJyBvclxuICAgKiAgICAgJ2JpbmFyeVNlYXJjaC5MRUFTVF9VUFBFUl9CT1VORCcuIFNwZWNpZmllcyB3aGV0aGVyIHRvIHJldHVybiB0aGVcbiAgICogICAgIGNsb3Nlc3QgZWxlbWVudCB0aGF0IGlzIHNtYWxsZXIgdGhhbiBvciBncmVhdGVyIHRoYW4gdGhlIG9uZSB3ZSBhcmVcbiAgICogICAgIHNlYXJjaGluZyBmb3IsIHJlc3BlY3RpdmVseSwgaWYgdGhlIGV4YWN0IGVsZW1lbnQgY2Fubm90IGJlIGZvdW5kLlxuICAgKi9cbiAgZnVuY3Rpb24gcmVjdXJzaXZlU2VhcmNoKGFMb3csIGFIaWdoLCBhTmVlZGxlLCBhSGF5c3RhY2ssIGFDb21wYXJlLCBhQmlhcykge1xuICAgIC8vIFRoaXMgZnVuY3Rpb24gdGVybWluYXRlcyB3aGVuIG9uZSBvZiB0aGUgZm9sbG93aW5nIGlzIHRydWU6XG4gICAgLy9cbiAgICAvLyAgIDEuIFdlIGZpbmQgdGhlIGV4YWN0IGVsZW1lbnQgd2UgYXJlIGxvb2tpbmcgZm9yLlxuICAgIC8vXG4gICAgLy8gICAyLiBXZSBkaWQgbm90IGZpbmQgdGhlIGV4YWN0IGVsZW1lbnQsIGJ1dCB3ZSBjYW4gcmV0dXJuIHRoZSBpbmRleCBvZlxuICAgIC8vICAgICAgdGhlIG5leHQtY2xvc2VzdCBlbGVtZW50LlxuICAgIC8vXG4gICAgLy8gICAzLiBXZSBkaWQgbm90IGZpbmQgdGhlIGV4YWN0IGVsZW1lbnQsIGFuZCB0aGVyZSBpcyBubyBuZXh0LWNsb3Nlc3RcbiAgICAvLyAgICAgIGVsZW1lbnQgdGhhbiB0aGUgb25lIHdlIGFyZSBzZWFyY2hpbmcgZm9yLCBzbyB3ZSByZXR1cm4gLTEuXG4gICAgdmFyIG1pZCA9IE1hdGguZmxvb3IoKGFIaWdoIC0gYUxvdykgLyAyKSArIGFMb3c7XG4gICAgdmFyIGNtcCA9IGFDb21wYXJlKGFOZWVkbGUsIGFIYXlzdGFja1ttaWRdLCB0cnVlKTtcbiAgICBpZiAoY21wID09PSAwKSB7XG4gICAgICAvLyBGb3VuZCB0aGUgZWxlbWVudCB3ZSBhcmUgbG9va2luZyBmb3IuXG4gICAgICByZXR1cm4gbWlkO1xuICAgIH1cbiAgICBlbHNlIGlmIChjbXAgPiAwKSB7XG4gICAgICAvLyBPdXIgbmVlZGxlIGlzIGdyZWF0ZXIgdGhhbiBhSGF5c3RhY2tbbWlkXS5cbiAgICAgIGlmIChhSGlnaCAtIG1pZCA+IDEpIHtcbiAgICAgICAgLy8gVGhlIGVsZW1lbnQgaXMgaW4gdGhlIHVwcGVyIGhhbGYuXG4gICAgICAgIHJldHVybiByZWN1cnNpdmVTZWFyY2gobWlkLCBhSGlnaCwgYU5lZWRsZSwgYUhheXN0YWNrLCBhQ29tcGFyZSwgYUJpYXMpO1xuICAgICAgfVxuXG4gICAgICAvLyBUaGUgZXhhY3QgbmVlZGxlIGVsZW1lbnQgd2FzIG5vdCBmb3VuZCBpbiB0aGlzIGhheXN0YWNrLiBEZXRlcm1pbmUgaWZcbiAgICAgIC8vIHdlIGFyZSBpbiB0ZXJtaW5hdGlvbiBjYXNlICgzKSBvciAoMikgYW5kIHJldHVybiB0aGUgYXBwcm9wcmlhdGUgdGhpbmcuXG4gICAgICBpZiAoYUJpYXMgPT0gZXhwb3J0cy5MRUFTVF9VUFBFUl9CT1VORCkge1xuICAgICAgICByZXR1cm4gYUhpZ2ggPCBhSGF5c3RhY2subGVuZ3RoID8gYUhpZ2ggOiAtMTtcbiAgICAgIH0gZWxzZSB7XG4gICAgICAgIHJldHVybiBtaWQ7XG4gICAgICB9XG4gICAgfVxuICAgIGVsc2Uge1xuICAgICAgLy8gT3VyIG5lZWRsZSBpcyBsZXNzIHRoYW4gYUhheXN0YWNrW21pZF0uXG4gICAgICBpZiAobWlkIC0gYUxvdyA+IDEpIHtcbiAgICAgICAgLy8gVGhlIGVsZW1lbnQgaXMgaW4gdGhlIGxvd2VyIGhhbGYuXG4gICAgICAgIHJldHVybiByZWN1cnNpdmVTZWFyY2goYUxvdywgbWlkLCBhTmVlZGxlLCBhSGF5c3RhY2ssIGFDb21wYXJlLCBhQmlhcyk7XG4gICAgICB9XG5cbiAgICAgIC8vIHdlIGFyZSBpbiB0ZXJtaW5hdGlvbiBjYXNlICgzKSBvciAoMikgYW5kIHJldHVybiB0aGUgYXBwcm9wcmlhdGUgdGhpbmcuXG4gICAgICBpZiAoYUJpYXMgPT0gZXhwb3J0cy5MRUFTVF9VUFBFUl9CT1VORCkge1xuICAgICAgICByZXR1cm4gbWlkO1xuICAgICAgfSBlbHNlIHtcbiAgICAgICAgcmV0dXJuIGFMb3cgPCAwID8gLTEgOiBhTG93O1xuICAgICAgfVxuICAgIH1cbiAgfVxuXG4gIC8qKlxuICAgKiBUaGlzIGlzIGFuIGltcGxlbWVudGF0aW9uIG9mIGJpbmFyeSBzZWFyY2ggd2hpY2ggd2lsbCBhbHdheXMgdHJ5IGFuZCByZXR1cm5cbiAgICogdGhlIGluZGV4IG9mIHRoZSBjbG9zZXN0IGVsZW1lbnQgaWYgdGhlcmUgaXMgbm8gZXhhY3QgaGl0LiBUaGlzIGlzIGJlY2F1c2VcbiAgICogbWFwcGluZ3MgYmV0d2VlbiBvcmlnaW5hbCBhbmQgZ2VuZXJhdGVkIGxpbmUvY29sIHBhaXJzIGFyZSBzaW5nbGUgcG9pbnRzLFxuICAgKiBhbmQgdGhlcmUgaXMgYW4gaW1wbGljaXQgcmVnaW9uIGJldHdlZW4gZWFjaCBvZiB0aGVtLCBzbyBhIG1pc3MganVzdCBtZWFuc1xuICAgKiB0aGF0IHlvdSBhcmVuJ3Qgb24gdGhlIHZlcnkgc3RhcnQgb2YgYSByZWdpb24uXG4gICAqXG4gICAqIEBwYXJhbSBhTmVlZGxlIFRoZSBlbGVtZW50IHlvdSBhcmUgbG9va2luZyBmb3IuXG4gICAqIEBwYXJhbSBhSGF5c3RhY2sgVGhlIGFycmF5IHRoYXQgaXMgYmVpbmcgc2VhcmNoZWQuXG4gICAqIEBwYXJhbSBhQ29tcGFyZSBBIGZ1bmN0aW9uIHdoaWNoIHRha2VzIHRoZSBuZWVkbGUgYW5kIGFuIGVsZW1lbnQgaW4gdGhlXG4gICAqICAgICBhcnJheSBhbmQgcmV0dXJucyAtMSwgMCwgb3IgMSBkZXBlbmRpbmcgb24gd2hldGhlciB0aGUgbmVlZGxlIGlzIGxlc3NcbiAgICogICAgIHRoYW4sIGVxdWFsIHRvLCBvciBncmVhdGVyIHRoYW4gdGhlIGVsZW1lbnQsIHJlc3BlY3RpdmVseS5cbiAgICogQHBhcmFtIGFCaWFzIEVpdGhlciAnYmluYXJ5U2VhcmNoLkdSRUFURVNUX0xPV0VSX0JPVU5EJyBvclxuICAgKiAgICAgJ2JpbmFyeVNlYXJjaC5MRUFTVF9VUFBFUl9CT1VORCcuIFNwZWNpZmllcyB3aGV0aGVyIHRvIHJldHVybiB0aGVcbiAgICogICAgIGNsb3Nlc3QgZWxlbWVudCB0aGF0IGlzIHNtYWxsZXIgdGhhbiBvciBncmVhdGVyIHRoYW4gdGhlIG9uZSB3ZSBhcmVcbiAgICogICAgIHNlYXJjaGluZyBmb3IsIHJlc3BlY3RpdmVseSwgaWYgdGhlIGV4YWN0IGVsZW1lbnQgY2Fubm90IGJlIGZvdW5kLlxuICAgKiAgICAgRGVmYXVsdHMgdG8gJ2JpbmFyeVNlYXJjaC5HUkVBVEVTVF9MT1dFUl9CT1VORCcuXG4gICAqL1xuICBleHBvcnRzLnNlYXJjaCA9IGZ1bmN0aW9uIHNlYXJjaChhTmVlZGxlLCBhSGF5c3RhY2ssIGFDb21wYXJlLCBhQmlhcykge1xuICAgIGlmIChhSGF5c3RhY2subGVuZ3RoID09PSAwKSB7XG4gICAgICByZXR1cm4gLTE7XG4gICAgfVxuXG4gICAgdmFyIGluZGV4ID0gcmVjdXJzaXZlU2VhcmNoKC0xLCBhSGF5c3RhY2subGVuZ3RoLCBhTmVlZGxlLCBhSGF5c3RhY2ssXG4gICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIGFDb21wYXJlLCBhQmlhcyB8fCBleHBvcnRzLkdSRUFURVNUX0xPV0VSX0JPVU5EKTtcbiAgICBpZiAoaW5kZXggPCAwKSB7XG4gICAgICByZXR1cm4gLTE7XG4gICAgfVxuXG4gICAgLy8gV2UgaGF2ZSBmb3VuZCBlaXRoZXIgdGhlIGV4YWN0IGVsZW1lbnQsIG9yIHRoZSBuZXh0LWNsb3Nlc3QgZWxlbWVudCB0aGFuXG4gICAgLy8gdGhlIG9uZSB3ZSBhcmUgc2VhcmNoaW5nIGZvci4gSG93ZXZlciwgdGhlcmUgbWF5IGJlIG1vcmUgdGhhbiBvbmUgc3VjaFxuICAgIC8vIGVsZW1lbnQuIE1ha2Ugc3VyZSB3ZSBhbHdheXMgcmV0dXJuIHRoZSBzbWFsbGVzdCBvZiB0aGVzZS5cbiAgICB3aGlsZSAoaW5kZXggLSAxID49IDApIHtcbiAgICAgIGlmIChhQ29tcGFyZShhSGF5c3RhY2tbaW5kZXhdLCBhSGF5c3RhY2tbaW5kZXggLSAxXSwgdHJ1ZSkgIT09IDApIHtcbiAgICAgICAgYnJlYWs7XG4gICAgICB9XG4gICAgICAtLWluZGV4O1xuICAgIH1cblxuICAgIHJldHVybiBpbmRleDtcbiAgfTtcbn1cblxuXG5cbi8qKioqKioqKioqKioqKioqKlxuICoqIFdFQlBBQ0sgRk9PVEVSXG4gKiogLi9saWIvYmluYXJ5LXNlYXJjaC5qc1xuICoqIG1vZHVsZSBpZCA9IDFcbiAqKiBtb2R1bGUgY2h1bmtzID0gMFxuICoqLyJdLCJzb3VyY2VSb290IjoiIn0=