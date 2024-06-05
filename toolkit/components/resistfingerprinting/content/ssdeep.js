/*!
The MIT License (MIT)

Copyright (c) 2015 Hu Wenshuo

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

// Trimmed down version of ssdeep.js from https://github.com/cloudtracer/ssdeep.js

(function () {
  var ssdeep = {};
  this.ssdeep = ssdeep;

  var HASH_PRIME = 16777619;
  var HASH_INIT = 671226215;
  var ROLLING_WINDOW = 7;
  var MAX_LENGTH = 64; // Max individual hash length in characters
  var B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  /*
   * Add integers, wrapping at 2^32. This uses 16-bit operations internally
   * to work around bugs in some JS interpreters.
   */
  function safe_add(x, y) {
    var lsw = (x & 0xffff) + (y & 0xffff);
    var msw = (x >> 16) + (y >> 16) + (lsw >> 16);
    return (msw << 16) | (lsw & 0xffff);
  }

  /*
      1000 0000
      1000 0000
      0000 0001
    */

  function safe_multiply(x, y) {
    /*
  			a = a00 + a16
  			b = b00 + b16
  			a*b = (a00 + a16)(b00 + b16)
  				= a00b00 + a00b16 + a16b00 + a16b16

  			a16b16 overflows the 32bits
  		 */
    var xlsw = x & 0xffff;
    var xmsw = (x >> 16) + (xlsw >> 16);
    var ylsw = y & 0xffff;
    var ymsw = (y >> 16) + (ylsw >> 16);
    var a16 = xmsw;
    var a00 = xlsw;
    var b16 = ymsw;
    var b00 = ylsw;
    var c16, c00;
    c00 = a00 * b00;
    c16 = c00 >>> 16;

    c16 += a16 * b00;
    c16 &= 0xffff; // Not required but improves performance
    c16 += a00 * b16;

    xlsw = c00 & 0xffff;
    xmsw = c16 & 0xffff;

    return (xmsw << 16) | (xlsw & 0xffff);
  }

  //FNV-1 hash
  function fnv(h, c) {
    return (safe_multiply(h, HASH_PRIME) ^ c) >>> 0;
  }

  function RollHash() {
    this.rolling_window = new Array(ROLLING_WINDOW);
    this.h1 = 0;
    this.h2 = 0;
    this.h3 = 0;
    this.n = 0;
  }
  RollHash.prototype.update = function (c) {
    this.h2 = safe_add(this.h2, -this.h1);
    var mut = ROLLING_WINDOW * c;
    this.h2 = safe_add(this.h2, mut) >>> 0;
    this.h1 = safe_add(this.h1, c);

    var val = this.rolling_window[this.n % ROLLING_WINDOW] || 0;
    this.h1 = safe_add(this.h1, -val) >>> 0;
    this.rolling_window[this.n % ROLLING_WINDOW] = c;
    this.n++;

    this.h3 = this.h3 << 5;
    this.h3 = (this.h3 ^ c) >>> 0;
  };
  RollHash.prototype.sum = function () {
    return (this.h1 + this.h2 + this.h3) >>> 0;
  };

  function piecewiseHash(bytes, triggerValue) {
    var signatures = ["", "", triggerValue];
    if (bytes.length === 0) {
      return signatures;
    }
    var h1 = HASH_INIT;
    var h2 = HASH_INIT;
    var rh = new RollHash();
    //console.log(triggerValue)
    for (var i = 0, len = bytes.length; i < len; i++) {
      var thisByte = bytes[i];

      h1 = fnv(h1, thisByte);
      h2 = fnv(h2, thisByte);

      rh.update(thisByte);

      if (
        signatures[0].length < MAX_LENGTH - 1 &&
        rh.sum() % triggerValue === triggerValue - 1
      ) {
        signatures[0] += B64.charAt(h1 & 63);
        h1 = HASH_INIT;
      }
      if (
        signatures[1].length < MAX_LENGTH / 2 - 1 &&
        rh.sum() % (triggerValue * 2) === triggerValue * 2 - 1
      ) {
        signatures[1] += B64.charAt(h2 & 63);
        h2 = HASH_INIT;
      }
    }
    signatures[0] += B64.charAt(h1 & 63);
    signatures[1] += B64.charAt(h2 & 63);
    return signatures;
  }

  function digest(bytes) {
    var bi = 3;
    while (bi * MAX_LENGTH < bytes.length) {
      bi *= 2;
    }

    var signatures;
    do {
      signatures = piecewiseHash(bytes, bi);
      bi = ~~(bi / 2);
    } while (bi > 3 && signatures[0].length < MAX_LENGTH / 2);

    return signatures[2] + ":" + signatures[0] + ":" + signatures[1];
  }

  ssdeep.digest = function (data) {
    if (typeof data === "string") {
      data = new TextEncoder().encode(data);
    }
    return digest(data);
  };
})();
