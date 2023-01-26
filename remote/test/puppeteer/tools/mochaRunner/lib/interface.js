"use strict";
/**
 * Copyright 2022 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const mocha_1 = __importDefault(require("mocha"));
const common_1 = __importDefault(require("mocha/lib/interfaces/common"));
const utils_js_1 = require("./utils.js");
const skippedTests = process.env['PUPPETEER_SKIPPED_TEST_CONFIG']
    ? JSON.parse(process.env['PUPPETEER_SKIPPED_TEST_CONFIG'])
    : [];
skippedTests.reverse();
function shouldSkipTest(test) {
    const testIdForFileName = (0, utils_js_1.getTestId)(test.file);
    const testIdForTestName = (0, utils_js_1.getTestId)(test.file, test.fullTitle());
    // TODO: more efficient lookup.
    const definition = skippedTests.find(skippedTest => {
        return ('' === skippedTest.testIdPattern ||
            testIdForFileName === skippedTest.testIdPattern ||
            testIdForTestName === skippedTest.testIdPattern);
    });
    if (definition && definition.skip) {
        return true;
    }
    return false;
}
function customBDDInterface(suite) {
    const suites = [suite];
    suite.on(mocha_1.default.Suite.constants.EVENT_FILE_PRE_REQUIRE, function (context, file, mocha) {
        const common = (0, common_1.default)(suites, context, mocha);
        context['before'] = common.before;
        context['after'] = common.after;
        context['beforeEach'] = common.beforeEach;
        context['afterEach'] = common.afterEach;
        if (mocha.options.delay) {
            context['run'] = common.runWithSuite(suite);
        }
        function describe(title, fn) {
            return common.suite.create({
                title: title,
                file: file,
                fn: fn,
            });
        }
        describe.only = function (title, fn) {
            return common.suite.only({
                title: title,
                file: file,
                fn: fn,
            });
        };
        describe.skip = function (title, fn) {
            return common.suite.skip({
                title: title,
                file: file,
                fn: fn,
            });
        };
        // eslint-disable-next-line @typescript-eslint/ban-ts-comment
        // @ts-ignore
        context['describe'] = describe;
        function it(title, fn) {
            const suite = suites[0];
            const test = new mocha_1.default.Test(title, suite.isPending() ? undefined : fn);
            test.file = file;
            test.parent = suite;
            if (shouldSkipTest(test)) {
                const test = new mocha_1.default.Test(title);
                test.file = file;
                suite.addTest(test);
                return test;
            }
            else {
                suite.addTest(test);
                return test;
            }
        }
        it.only = function (title, fn) {
            return common.test.only(mocha, context['it'](title, fn));
        };
        it.skip = function (title) {
            return context['it'](title);
        };
        // eslint-disable-next-line @typescript-eslint/ban-ts-comment
        // @ts-ignore
        context.it = it;
    });
}
customBDDInterface.description = 'Custom BDD';
module.exports = customBDDInterface;
//# sourceMappingURL=interface.js.map