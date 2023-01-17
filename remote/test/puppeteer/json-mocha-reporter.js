const mocha = require('mocha');
module.exports = JSONExtra;

const constants = mocha.Runner.constants;

/*

This is a copy of
https://github.com/mochajs/mocha/blob/master/lib/reporters/json-stream.js
with more event hooks. mocha does not support extending reporters or using
multiple reporters so a custom reporter is needed and it must be local
to the project.

*/

function JSONExtra(runner, options) {
  mocha.reporters.Base.call(this, runner, options);
  mocha.reporters.JSON.call(this, runner, options);
  const self = this;

  runner.once(constants.EVENT_RUN_BEGIN, function () {
    writeEvent(['start', {total: runner.total}]);
  });

  runner.on(constants.EVENT_TEST_PASS, function (test) {
    writeEvent(['pass', clean(test)]);
  });

  runner.on(constants.EVENT_TEST_FAIL, function (test, err) {
    test = clean(test);
    test.err = err.message;
    test.stack = err.stack || null;
    writeEvent(['fail', test]);
  });

  runner.once(constants.EVENT_RUN_END, function () {
    writeEvent(['end', self.stats]);
  });

  runner.on(constants.EVENT_TEST_BEGIN, function (test) {
    writeEvent(['test-start', clean(test)]);
  });

  runner.on(constants.EVENT_TEST_PENDING, function (test) {
    writeEvent(['pending', clean(test)]);
  });
}

function writeEvent(event) {
  process.stdout.write(JSON.stringify(event) + '\n');
}

/**
 * Returns an object literal representation of `test`
 * free of cyclic properties, etc.
 *
 * @private
 * @param {Object} test - Instance used as data source.
 * @return {Object} object containing pared-down test instance data
 */
function clean(test) {
  return {
    title: test.title,
    fullTitle: test.fullTitle(),
    file: test.file,
    duration: test.duration,
    currentRetry: test.currentRetry(),
  };
}
