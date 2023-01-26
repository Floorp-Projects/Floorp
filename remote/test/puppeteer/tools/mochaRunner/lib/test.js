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
const strict_1 = __importDefault(require("assert/strict"));
const node_test_1 = __importDefault(require("node:test"));
const utils_js_1 = require("./utils.js");
const utils_js_2 = require("./utils.js");
(0, node_test_1.default)('extendProcessEnv', () => {
    const env = (0, utils_js_2.extendProcessEnv)([{ TEST: 'TEST' }, { TEST2: 'TEST2' }]);
    strict_1.default.equal(env['TEST'], 'TEST');
    strict_1.default.equal(env['TEST2'], 'TEST2');
});
(0, node_test_1.default)('getFilename', () => {
    strict_1.default.equal((0, utils_js_2.getFilename)('/etc/test.ts'), 'test');
    strict_1.default.equal((0, utils_js_2.getFilename)('/etc/test.js'), 'test');
});
(0, node_test_1.default)('getTestResultForFailure', () => {
    strict_1.default.equal((0, utils_js_1.getTestResultForFailure)({ err: { code: 'ERR_MOCHA_TIMEOUT' } }), 'TIMEOUT');
    strict_1.default.equal((0, utils_js_1.getTestResultForFailure)({ err: { code: 'ERROR' } }), 'FAIL');
});
(0, node_test_1.default)('filterByParameters', () => {
    const expectations = [
        {
            testIdPattern: '[oopif.spec] OOPIF "after all" hook for "should keep track of a frames OOP state"',
            platforms: ['darwin'],
            parameters: ['firefox', 'headless'],
            expectations: ['FAIL'],
        },
    ];
    strict_1.default.equal((0, utils_js_1.filterByParameters)(expectations, ['firefox', 'headless']).length, 1);
    strict_1.default.equal((0, utils_js_1.filterByParameters)(expectations, ['firefox']).length, 0);
    strict_1.default.equal((0, utils_js_1.filterByParameters)(expectations, ['firefox', 'headless', 'other']).length, 1);
    strict_1.default.equal((0, utils_js_1.filterByParameters)(expectations, ['other']).length, 0);
});
//# sourceMappingURL=test.js.map