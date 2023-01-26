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
exports.getTestId = exports.getTestResultForFailure = exports.getExpectationUpdates = exports.findEffectiveExpectationForTest = exports.filterByParameters = exports.prettyPrintJSON = exports.filterByPlatform = exports.readJSON = exports.getFilename = exports.extendProcessEnv = void 0;
const path_1 = __importDefault(require("path"));
const fs_1 = __importDefault(require("fs"));
function extendProcessEnv(envs) {
    return envs.reduce((acc, item) => {
        Object.assign(acc, item);
        return acc;
    }, {
        ...process.env,
    });
}
exports.extendProcessEnv = extendProcessEnv;
function getFilename(file) {
    return path_1.default.basename(file).replace(path_1.default.extname(file), '');
}
exports.getFilename = getFilename;
function readJSON(path) {
    return JSON.parse(fs_1.default.readFileSync(path, 'utf-8'));
}
exports.readJSON = readJSON;
function filterByPlatform(items, platform) {
    return items.filter(item => {
        return item.platforms.includes(platform);
    });
}
exports.filterByPlatform = filterByPlatform;
function prettyPrintJSON(json) {
    console.log(JSON.stringify(json, null, 2));
}
exports.prettyPrintJSON = prettyPrintJSON;
function filterByParameters(expectations, parameters) {
    const querySet = new Set(parameters);
    return expectations.filter(ex => {
        return ex.parameters.every(param => {
            return querySet.has(param);
        });
    });
}
exports.filterByParameters = filterByParameters;
/**
 * The last expectation that matches an empty string as all tests pattern
 * or the name of the file or the whole name of the test the filter wins.
 */
function findEffectiveExpectationForTest(expectations, result) {
    return expectations
        .filter(expectation => {
        return ('' === expectation.testIdPattern ||
            getTestId(result.file) === expectation.testIdPattern ||
            getTestId(result.file, result.fullTitle) === expectation.testIdPattern);
    })
        .pop();
}
exports.findEffectiveExpectationForTest = findEffectiveExpectationForTest;
function getExpectationUpdates(results, expecations, context) {
    const output = [];
    for (const pass of results.passes) {
        const expectation = findEffectiveExpectationForTest(expecations, pass);
        if (expectation && !expectation.expectations.includes('PASS')) {
            output.push({
                expectation,
                test: pass,
                action: 'remove',
            });
        }
    }
    for (const failure of results.failures) {
        const expectation = findEffectiveExpectationForTest(expecations, failure);
        if (expectation) {
            if (!expectation.expectations.includes(getTestResultForFailure(failure))) {
                output.push({
                    expectation: {
                        ...expectation,
                        expectations: [
                            ...expectation.expectations,
                            getTestResultForFailure(failure),
                        ],
                    },
                    test: failure,
                    action: 'update',
                });
            }
        }
        else {
            output.push({
                expectation: {
                    testIdPattern: getTestId(failure.file, failure.fullTitle),
                    platforms: context.platforms,
                    parameters: context.parameters,
                    expectations: [getTestResultForFailure(failure)],
                },
                test: failure,
                action: 'add',
            });
        }
    }
    return output;
}
exports.getExpectationUpdates = getExpectationUpdates;
function getTestResultForFailure(test) {
    var _a;
    return ((_a = test.err) === null || _a === void 0 ? void 0 : _a.code) === 'ERR_MOCHA_TIMEOUT' ? 'TIMEOUT' : 'FAIL';
}
exports.getTestResultForFailure = getTestResultForFailure;
function getTestId(file, fullTitle) {
    return fullTitle
        ? `[${getFilename(file)}] ${fullTitle}`
        : `[${getFilename(file)}]`;
}
exports.getTestId = getTestId;
//# sourceMappingURL=utils.js.map