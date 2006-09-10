
use Test;
require 'foo.pl';

var number = 1;
var regexp = /this/;
var regexpi = /InSensitive/i;
/* simple C style comment */
(void) 1;
// simple C++ style comment
(void) 2;
(void)3;
const constant = 3;
var single_quoted_string = 'singly quoted string';
var double_quoted_string = "doubly quoted string";
var array = [];
var array2 = [1, , ];

function foo(argument, argument2, argument3,

argument4) {
  if (condition) {
    expression;
  } else if (!condition2) {
    expression2;
    expression3;
  } else {
    if (other && conditions) {
      !expression;
    }
  }
}

function exception() {
  try {
    throw 1;
  } catch (e) {
    do_something;
  } finally {
    return something_else;
  }
  return not_reached;
}

function MyClass() {
  this._foo = 0;
}
MyClass.prototype = {
  constructor: MyClass,
  foo: function () {
    return this._foo++;
  }
};

var array3 = new Array("a", "big", "bird", "can't" + " fly");

function reserved_words() {
  try {} catch (e instanceof Exception) {
    var foo = new Bar;
  } finally {
    for each (var i in {});
    do { nothing; } while (false);
    if (!true || !!'' && " " | 3 & 7);
    while (false | true & false) nothing;
    a = [];
    a[3] = 0;
    a.q = RegExp("2");
  }
  null == undefined;
  null !== undefined;
  var z |= 15 ^ 29;
  return null;
}

function not_reserved_words() {
  tryThis();
  throwThis();
  catchThis();
  finallyThis();
  returnThis();
  varThis();
  constThis();
  newThis();
  voidThis();
  ifThis();
  elseThis();
  elsif;
  instanceofThis();
}
