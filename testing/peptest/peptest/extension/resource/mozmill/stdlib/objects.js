/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ['getLength', ];//'compare'];

var getLength = function (obj) {
  var len = 0;
  for (i in obj) {
    len++;
  }

  return len;
}

// var logging = {}; Components.utils.import('resource://mozmill/stdlib/logging.js', logging);

// var objectsLogger = logging.getLogger('objectsLogger');

// var compare = function (obj1, obj2, depth, recursion) {
//   if (depth == undefined) {
//     var depth = 4;
//   }
//   if (recursion == undefined) {
//     var recursion = 0;
//   }
//   
//   if (recursion > depth) {
//     return true;
//   }
//   
//   if (typeof(obj1) != typeof(obj2)) {
//     return false;
//   }
//   
//   if (typeof(obj1) == "object" && typeof(obj2) == "object") {
//     if ([x for (x in obj1)].length != [x for (x in obj2)].length) {
//       return false;
//     }
//     for (i in obj1) {
//       recursion++;
//       var result = compare(obj1[i], obj2[i], depth, recursion);
//       objectsLogger.info(i+' in recursion '+result);
//       if (result == false) {
//         return false;
//       }
//     }
//   } else {
//     if (obj1 != obj2) {
//       return false;
//     }
//   }
//   return true;
// }
