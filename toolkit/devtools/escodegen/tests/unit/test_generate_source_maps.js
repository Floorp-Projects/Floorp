/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can generate source maps via escodegen.
 */

Components.utils.import("resource://gre/modules/reflect.jsm");
Components.utils.import("resource://gre/modules/devtools/SourceMap.jsm");
const escodegen = require("escodegen/escodegen");

const testCode = "" + function main() {
  var a = 5 + 3;
  var b = 19 * 52;
  return a / b;
};

function run_test() {
  const ast = Reflect.parse(testCode);
  const { code, map } = escodegen.generate(ast, {
    format: {
      indent: {
        // Single space indents so we are mapping different locations.
        style: " "
      }
    },
    sourceMap: "testCode.js",
    sourceMapWithCode: true
  });
  const smc = new SourceMapConsumer(map.toString());

  let mapping = smc.originalPositionFor({
    line: 2,
    column: 1
  });
  do_check_eq(mapping.source, "testCode.js");
  do_check_eq(mapping.line, 2);
  do_check_eq(mapping.column, 2);

  mapping = smc.originalPositionFor({
    line: 2,
    column: 5
  });
  do_check_eq(mapping.source, "testCode.js");
  do_check_eq(mapping.line, 2);
  do_check_eq(mapping.column, 6);

  mapping = smc.originalPositionFor({
    line: 3,
    column: 1
  });
  do_check_eq(mapping.source, "testCode.js");
  do_check_eq(mapping.line, 3);
  do_check_eq(mapping.column, 2);

  mapping = smc.originalPositionFor({
    line: 3,
    column: 5
  });
  do_check_eq(mapping.source, "testCode.js");
  do_check_eq(mapping.line, 3);
  do_check_eq(mapping.column, 6);

  mapping = smc.originalPositionFor({
    line: 4,
    column: 1
  });
  do_check_eq(mapping.source, "testCode.js");
  do_check_eq(mapping.line, 4);
  do_check_eq(mapping.column, 2);
};
