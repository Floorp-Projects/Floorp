/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["assert", "assertTrue", "assertFalse", "assertEquals", "assertNotEquals",
                        "assertNull", "assertNotNull", "assertUndefined", "assertNotUndefined",
                        "assertNaN", "assertNotNaN", "assertArrayContains", "fail", "pass"];


// Array.isArray comes with JavaScript 1.8.5 (Firefox 4)
// cf. https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Array/isArray
Array.isArray = Array.isArray || function(o) { return Object.prototype.toString.call(o) === '[object Array]'; };

var frame = {}; Components.utils.import("resource://mozmill/modules/frame.js", frame);

var ifJSONable = function (v) {
  if (typeof(v) == 'function') {
    return undefined;
  } else {
    return v;
  }
}

var assert = function (booleanValue, comment) {
  if (booleanValue) {
    frame.events.pass({'function':'jum.assert', 'value':ifJSONable(booleanValue), 'comment':comment});
    return true;
  } else {
    frame.events.fail({'function':'jum.assert', 'value':ifJSONable(booleanValue), 'comment':comment});
    return false;
  }
}

var assertTrue = function (booleanValue, comment) {
  if (typeof(booleanValue) != 'boolean') {
    frame.events.fail({'function':'jum.assertTrue', 'value':ifJSONable(booleanValue),
                       'message':'Bad argument, value type '+typeof(booleanValue)+' !=  "boolean"',
                       'comment':comment});
    return false;
  }

  if (booleanValue) {
    frame.events.pass({'function':'jum.assertTrue', 'value':ifJSONable(booleanValue),
                       'comment':comment});
    return true;
  } else {
    frame.events.fail({'function':'jum.assertTrue', 'value':ifJSONable(booleanValue),
                       'comment':comment});
    return false;
  }
}

var assertFalse = function (booleanValue, comment) {
  if (typeof(booleanValue) != 'boolean') {
    frame.events.fail({'function':'jum.assertFalse', 'value':ifJSONable(booleanValue),
                       'message':'Bad argument, value type '+typeof(booleanValue)+' !=  "boolean"',
                       'comment':comment});
    return false;
  }

  if (!booleanValue) {
    frame.events.pass({'function':'jum.assertFalse', 'value':ifJSONable(booleanValue),
                       'comment':comment});
    return true;
  } else {
    frame.events.fail({'function':'jum.assertFalse', 'value':ifJSONable(booleanValue),
                       'comment':comment});
    return false;
  }
}

var assertEquals = function (value1, value2, comment) {
  // Case where value1 is an array
  if (Array.isArray(value1)) {

    if (!Array.isArray(value2)) {
      frame.events.fail({'function':'jum.assertEquals', 'comment':comment,
                         'message':'Bad argument, value1 is an array and value2 type ' +
                         typeof(value2)+' != "array"',
                         'value2':ifJSONable(value2)});
      return false;
    }

    if (value1.length != value2.length) {
      frame.events.fail({'function':'jum.assertEquals', 'comment':comment,
                         'message':"The arrays do not have the same length",
                         'value1':ifJSONable(value1), 'value2':ifJSONable(value2)});
      return false;
    }

    for (var i = 0; i < value1.length; i++) {
      if (value1[i] !== value2[i]) {
        frame.events.fail(
          {'function':'jum.assertEquals', 'comment':comment,
           'message':"The element of the arrays are different at index " + i,
           'value1':ifJSONable(value1), 'value2':ifJSONable(value2)});
        return false;
      }
    }
    frame.events.pass({'function':'jum.assertEquals', 'comment':comment,
                       'value1':ifJSONable(value1), 'value2':ifJSONable(value2)});
    return true;
  }

  // Case where value1 is not an array
  if (value1 == value2) {
    frame.events.pass({'function':'jum.assertEquals', 'comment':comment,
                       'value1':ifJSONable(value1), 'value2':ifJSONable(value2)});
    return true;
  } else {
    frame.events.fail({'function':'jum.assertEquals', 'comment':comment,
                       'value1':ifJSONable(value1), 'value2':ifJSONable(value2)});
    return false;
  }
}

var assertNotEquals = function (value1, value2, comment) {
  if (value1 != value2) {
    frame.events.pass({'function':'jum.assertNotEquals', 'comment':comment,
                       'value1':ifJSONable(value1), 'value2':ifJSONable(value2)});
    return true;
  } else {
    frame.events.fail({'function':'jum.assertNotEquals', 'comment':comment,
                       'value1':ifJSONable(value1), 'value2':ifJSONable(value2)});
    return false;
  }
}

var assertNull = function (value, comment) {
  if (value == null) {
    frame.events.pass({'function':'jum.assertNull', 'comment':comment,
                       'value':ifJSONable(value)});
    return true;
  } else {
    frame.events.fail({'function':'jum.assertNull', 'comment':comment,
                       'value':ifJSONable(value)});
    return false;
  }
}

var assertNotNull = function (value, comment) {
  if (value != null) {
    frame.events.pass({'function':'jum.assertNotNull', 'comment':comment,
                       'value':ifJSONable(value)});
    return true;
  } else {
    frame.events.fail({'function':'jum.assertNotNull', 'comment':comment,
                       'value':ifJSONable(value)});
    return false;
  }
}

var assertUndefined = function (value, comment) {
  if (value == undefined) {
    frame.events.pass({'function':'jum.assertUndefined', 'comment':comment,
                       'value':ifJSONable(value)});
    return true;
  } else {
    frame.events.fail({'function':'jum.assertUndefined', 'comment':comment,
                       'value':ifJSONable(value)});
    return false;
  }
}

var assertNotUndefined = function (value, comment) {
  if (value != undefined) {
    frame.events.pass({'function':'jum.assertNotUndefined', 'comment':comment,
                       'value':ifJSONable(value)});
    return true;
  } else {
    frame.events.fail({'function':'jum.assertNotUndefined', 'comment':comment,
                       'value':ifJSONable(value)});
    return false;
  }
}

var assertNaN = function (value, comment) {
  if (isNaN(value)) {
    frame.events.pass({'function':'jum.assertNaN', 'comment':comment,
                       'value':ifJSONable(value)});
    return true;
  } else {
    frame.events.fail({'function':'jum.assertNaN', 'comment':comment,
                       'value':ifJSONable(value)});
    return false;
  }
}

var assertNotNaN = function (value, comment) {
  if (!isNaN(value)) {
    frame.events.pass({'function':'jum.assertNotNaN', 'comment':comment,
                       'value':ifJSONable(value)});
    return true;
  } else {
    frame.events.fail({'function':'jum.assertNotNaN', 'comment':comment,
                       'value':ifJSONable(value)});
    return false;
  }
}

var assertArrayContains = function(array, value, comment) {
  if (!Array.isArray(array)) {
    frame.events.fail({'function':'jum.assertArrayContains', 'comment':comment,
                       'message':'Bad argument, value type '+typeof(array)+' != "array"',
                       'value':ifJSONable(array)});
    return false;
  }

  for (var i = 0; i < array.length; i++) {
    if (array[i] === value) {
      frame.events.pass({'function':'jum.assertArrayContains', 'comment':comment,
                         'value1':ifJSONable(array), 'value2':ifJSONable(value)});
      return true;
    }
  }
  frame.events.fail({'function':'jum.assertArrayContains', 'comment':comment,
                     'value1':ifJSONable(array), 'value2':ifJSONable(value)});
  return false;
}

var fail = function (comment) {
  frame.events.fail({'function':'jum.fail', 'comment':comment});
  return false;
}

var pass = function (comment) {
  frame.events.pass({'function':'jum.pass', 'comment':comment});
  return true;
}


