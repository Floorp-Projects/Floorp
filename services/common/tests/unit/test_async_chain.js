/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/async.js");

function run_test() {
  _("Chain a few async methods, making sure the 'this' object is correct.");

  let methods = {
    save(x, callback) {
      this.x = x;
      callback(x);
    },
    addX(x, callback) {
      callback(x + this.x);
    },
    double(x, callback) {
      callback(x * 2);
    },
    neg(x, callback) {
      callback(-x);
    }
  };
  methods.chain = Async.chain;

  // ((1 + 1 + 1) * (-1) + 1) * 2 + 1 = -3
  methods.chain(methods.save, methods.addX, methods.addX, methods.neg,
                methods.addX, methods.double, methods.addX, methods.save)(1);
  Assert.equal(methods.x, -3);
}
