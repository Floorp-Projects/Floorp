/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { getGlobalsForCode } = require("../lib/globals");
var assert = require("assert");

/* global describe, it */

describe("globals", function() {
  it("should reflect top-level this property assignment", function() {
    const globals = getGlobalsForCode(`
this.foo = 10;
`);
    assert.deepEqual(globals, [{ name: "foo", writable: true }]);
  });

  it("should reflect this property assignment inside block", function() {
    const globals = getGlobalsForCode(`
{
  this.foo = 10;
}
`);
    assert.deepEqual(globals, [{ name: "foo", writable: true }]);
  });

  it("should ignore this property assignment inside function declaration", function() {
    const globals = getGlobalsForCode(`
function f() {
  this.foo = 10;
}
`);
    assert.deepEqual(globals, [{ name: "f", writable: true }]);
  });

  it("should ignore this property assignment inside function expression", function() {
    const globals = getGlobalsForCode(`
(function f() {
  this.foo = 10;
});
`);
    assert.deepEqual(globals, []);
  });

  it("should ignore this property assignment inside method", function() {
    const globals = getGlobalsForCode(`
({
  method() {
    this.foo = 10;
  }
});
`);
    assert.deepEqual(globals, []);
  });

  it("should ignore this property assignment inside accessor", function() {
    const globals = getGlobalsForCode(`
({
  get x() {
    this.foo = 10;
  },
  set x(v) {
    this.bar = 10;
  }
});
`);
    assert.deepEqual(globals, []);
  });

  it("should reflect this property assignment inside arrow function", function() {
    const globals = getGlobalsForCode(`
() => {
  this.foo = 10;
};
`);
    assert.deepEqual(globals, [{ name: "foo", writable: true }]);
  });

  it("should ignore this property assignment inside arrow function inside function expression", function() {
    const globals = getGlobalsForCode(`
(function f() {
  () => {
    this.foo = 10;
  };
});
`);
    assert.deepEqual(globals, []);
  });

  it("should ignore this property assignment inside class static", function() {
    const globals = getGlobalsForCode(`
class A {
  static {
    this.foo = 10;
    (() => {
      this.bar = 10;
    })();
  }
}
`);
    assert.deepEqual(globals, [{ name: "A", writable: true }]);
  });

  it("should ignore this property assignment inside class property", function() {
    const globals = getGlobalsForCode(`
class A {
  a = this.foo = 10;
  b = (() => {
    this.bar = 10;
  })();
}
`);
    assert.deepEqual(globals, [{ name: "A", writable: true }]);
  });

  it("should ignore this property assignment inside class static property", function() {
    const globals = getGlobalsForCode(`
class A {
  static a = this.foo = 10;
  static b = (() => {
    this.bar = 10;
  })();
}
`);
    assert.deepEqual(globals, [{ name: "A", writable: true }]);
  });

  it("should ignore this property assignment inside class private property", function() {
    const globals = getGlobalsForCode(`
class A {
  #a = this.foo = 10;
  #b = (() => {
    this.bar = 10;
  })();
}
`);
    assert.deepEqual(globals, [{ name: "A", writable: true }]);
  });

  it("should ignore this property assignment inside class static private property", function() {
    const globals = getGlobalsForCode(`
class A {
  static #a = this.foo = 10;
  static #b = (() => {
    this.bar = 10;
  })();
}
`);
    assert.deepEqual(globals, [{ name: "A", writable: true }]);
  });

  it("should reflect lazy getters", function() {
    const globals = getGlobalsForCode(`
ChromeUtils.defineESModuleGetters(this, {
  A: "B",
});
`);
    assert.deepEqual(globals, [{ name: "A", writable: true, explicit: true }]);
  });
});
