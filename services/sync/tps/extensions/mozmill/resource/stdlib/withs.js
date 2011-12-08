/*
    Copyright (c) 2006 Lawrence Oluyede <l.oluyede@gmail.com>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

/*
 startsWith(str, prefix[, start[, end]]) -> bool

 Return true if str ends with the specified prefix, false otherwise.
 With optional start, test str beginning at that position.
 With optional end, stop comparing str at that position.
 prefix can also be an array of strings to try.
*/

var EXPORTED_SYMBOLS = ['startsWith', 'endsWith'];

function startsWith(str, prefix, start, end) {
    if (arguments.length < 2) {
        throw new TypeError('startsWith() requires at least 2 arguments');
    }

    // check if start and end are null/undefined or a 'number'
    if ((start == null) || (isNaN(new Number(start)))) {
        start = 0;
    }
    if ((end == null) || (isNaN(new Number(end)))) {
        end = Number.MAX_VALUE;
    }

    // if it's an array
    if (typeof prefix == "object") {
        for (var i = 0, j = prefix.length; i < j; i++) {
            var res = _stringTailMatch(str, prefix[i], start, end, true);
            if (res) {
                return true;
            }
        }
        return false;
    }

    return _stringTailMatch(str, prefix, start, end, true);
}

/*
 endsWith(str, suffix[, start[, end]]) -> bool

 Return true if str ends with the specified suffix, false otherwise.
 With optional start, test str beginning at that position.
 With optional end, stop comparing str at that position.
 suffix can also be an array of strings to try.
*/
function endsWith(str, suffix, start, end) {
    if (arguments.length < 2) {
        throw new TypeError('endsWith() requires at least 2 arguments');
    }

    // check if start and end are null/undefined or a 'number'
    if ((start == null) || (isNaN(new Number(start)))) {
        start = 0;
    }
    if ((end == null) || (isNaN(new Number(end)))) {
        end = Number.MAX_VALUE;
    }

    // if it's an array
    if (typeof suffix == "object") {
        for (var i = 0, j = suffix.length; i < j; i++) {
            var res = _stringTailMatch(str, suffix[i], start, end, false);
            if (res) {
                return true;
            }
        }
        return false;
    }

    return _stringTailMatch(str, suffix, start, end, false);
}

/*
 Matches the end (direction == false) or start (direction == true) of str
 against substr, using the start and end arguments. Returns false
 if not found and true if found.
*/
function _stringTailMatch(str, substr, start, end, fromStart) {
    var len = str.length;
    var slen = substr.length;

    var indices = _adjustIndices(start, end, len);
    start = indices[0]; end = indices[1]; len = indices[2];

    if (fromStart) {
        if (start + slen > len) {
            return false;
        }
    } else {
        if (end - start < slen || start > len) {
            return false;
        }
        if (end - slen > start) {
            start = end - slen;
        }
    }

    if (end - start >= slen) {
        return str.substr(start, slen) == substr;
    }
    return false;
}

function _adjustIndices(start, end, len)
{
	if (end > len) {
	    end = len;
	} else if (end < 0) {
	    end += len;
	}

    if (end < 0) {
        end = 0;
    }
	if (start < 0) {
	    start += len;
	}
	if (start < 0) {
		start = 0;
	}
	
	return [start, end, len];
}
