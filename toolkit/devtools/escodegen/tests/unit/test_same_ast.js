/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can go AST -> JS -> AST via escodegen and Reflect.
 */

const escodegen = require("escodegen/escodegen");
Components.utils.import("resource://gre/modules/reflect.jsm");

const testCode = "" + function main () {
  function makeAcc(n) {
    return function () {
      return ++n;
    };
  }

  var acc = makeAcc(10);

  for (var i = 0; i < 10; i++) {
    acc();
  }

  console.log(acc());
};

function run_test() {
  const originalAST = Reflect.parse(testCode);
  const generatedCode = escodegen.generate(originalAST);
  const generatedAST = Reflect.parse(generatedCode);

  do_print("Original AST:");
  do_print(JSON.stringify(originalAST, null, 2));
  do_print("Generated AST:");
  do_print(JSON.stringify(generatedAST, null, 2));

  checkEquivalentASTs(originalAST, generatedAST);
}

const isObject = (obj) => typeof obj === "object" && obj !== null;
const zip = (a, b) => {
  let pairs = [];
  for (let i = 0; i < a.length && i < b.length; i++) {
    pairs.push([a[i], b[i]]);
  }
  return pairs;
};
const isntLoc = k => k !== "loc";

function checkEquivalentASTs(expected, actual, prop = []) {
  do_print("Checking: " + prop.join(" "));

  if (!isObject(expected)) {
    return void do_check_eq(expected, actual);
  }

  do_check_true(isObject(actual));

  if (Array.isArray(expected)) {
    do_check_true(Array.isArray(actual));
    do_check_eq(expected.length, actual.length);
    let i = 0;
    for (let [e, a] of zip(expected, actual)) {
      checkEquivalentASTs(a, e, prop.concat([i++]));
    }
    return;
  }

  const expectedKeys = Object.keys(expected).filter(isntLoc).sort();
  const actualKeys = Object.keys(actual).filter(isntLoc).sort();
  do_check_eq(expectedKeys.length, actualKeys.length);
  for (let [ek, ak] of zip(expectedKeys, actualKeys)) {
    do_check_eq(ek, ak);
    checkEquivalentASTs(expected[ek], actual[ak], prop.concat([ek]));
  }
}
