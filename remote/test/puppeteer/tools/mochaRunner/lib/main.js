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
const types_js_1 = require("./types.js");
const path_1 = __importDefault(require("path"));
const fs_1 = __importDefault(require("fs"));
const os_1 = __importDefault(require("os"));
const node_child_process_1 = require("node:child_process");
const utils_js_1 = require("./utils.js");
function getApplicableTestSuites(parsedSuitesFile, platform) {
    const testSuiteArgIdx = process.argv.indexOf('--test-suite');
    let applicableSuites = [];
    if (testSuiteArgIdx === -1) {
        applicableSuites = (0, utils_js_1.filterByPlatform)(parsedSuitesFile.testSuites, platform);
    }
    else {
        const testSuiteId = process.argv[testSuiteArgIdx + 1];
        const testSuite = parsedSuitesFile.testSuites.find(suite => {
            return suite.id === testSuiteId;
        });
        if (!testSuite) {
            console.error(`Test suite ${testSuiteId} is not defined`);
            process.exit(1);
        }
        if (!testSuite.platforms.includes(platform)) {
            console.warn(`Test suite ${testSuiteId} is not enabled for your platform. Running it anyway.`);
        }
        applicableSuites = [testSuite];
    }
    return applicableSuites;
}
async function main() {
    const noCoverage = process.argv.indexOf('--no-coverage') !== -1;
    const statsFilenameIdx = process.argv.indexOf('--save-stats-to');
    let statsFilename = '';
    if (statsFilenameIdx !== -1) {
        statsFilename = process.argv[statsFilenameIdx + 1];
    }
    const platform = types_js_1.zPlatform.parse(os_1.default.platform());
    const expectations = (0, utils_js_1.readJSON)(path_1.default.join(process.cwd(), 'test', 'TestExpectations.json'));
    const parsedSuitesFile = types_js_1.zTestSuiteFile.parse((0, utils_js_1.readJSON)(path_1.default.join(process.cwd(), 'test', 'TestSuites.json')));
    const applicableSuites = getApplicableTestSuites(parsedSuitesFile, platform);
    console.log('Planning to run the following test suites', applicableSuites);
    if (statsFilename) {
        console.log('Test stats will be saved to', statsFilename);
    }
    let fail = false;
    const recommendations = [];
    try {
        for (const suite of applicableSuites) {
            const parameters = suite.parameters;
            const applicableExpectations = (0, utils_js_1.filterByParameters)((0, utils_js_1.filterByPlatform)(expectations, platform), parameters);
            const env = (0, utils_js_1.extendProcessEnv)([
                ...parameters.map(param => {
                    return parsedSuitesFile.parameterDefinitons[param];
                }),
                {
                    PUPPETEER_SKIPPED_TEST_CONFIG: JSON.stringify(applicableExpectations.map(ex => {
                        return {
                            testIdPattern: ex.testIdPattern,
                            skip: ex.expectations.includes('SKIP'),
                        };
                    })),
                },
            ]);
            const tmpDir = fs_1.default.mkdtempSync(path_1.default.join(os_1.default.tmpdir(), 'puppeteer-test-runner-'));
            const tmpFilename = statsFilename
                ? statsFilename
                : path_1.default.join(tmpDir, 'output.json');
            console.log('Running', JSON.stringify(parameters), tmpFilename);
            const reporterArgumentIndex = process.argv.indexOf('--reporter');
            const args = [
                '-u',
                path_1.default.join(__dirname, 'interface.js'),
                '-R',
                reporterArgumentIndex === -1
                    ? path_1.default.join(__dirname, 'reporter.js')
                    : process.argv[reporterArgumentIndex + 1] || '',
                '-O',
                'output=' + tmpFilename,
            ];
            const retriesArgumentIndex = process.argv.indexOf('--retries');
            const timeoutArgumentIndex = process.argv.indexOf('--timeout');
            if (retriesArgumentIndex > -1) {
                args.push('--retries', process.argv[retriesArgumentIndex + 1] || '');
            }
            if (timeoutArgumentIndex > -1) {
                args.push('--timeout', process.argv[timeoutArgumentIndex + 1] || '');
            }
            if (process.argv.indexOf('--no-parallel')) {
                args.push('--no-parallel');
            }
            if (process.argv.indexOf('--fullTrace')) {
                args.push('--fullTrace');
            }
            const spawnArgs = {
                shell: true,
                cwd: process.cwd(),
                stdio: 'inherit',
                env,
            };
            const handle = noCoverage
                ? (0, node_child_process_1.spawn)('npx', ['mocha', ...args], spawnArgs)
                : (0, node_child_process_1.spawn)('npx', [
                    'c8',
                    '--check-coverage',
                    '--lines',
                    String(suite.expectedLineCoverage),
                    'npx mocha',
                    ...args,
                ], spawnArgs);
            await new Promise((resolve, reject) => {
                handle.on('error', err => {
                    reject(err);
                });
                handle.on('close', () => {
                    resolve();
                });
            });
            console.log('Finished', JSON.stringify(parameters));
            try {
                const results = (0, utils_js_1.readJSON)(tmpFilename);
                const recommendation = (0, utils_js_1.getExpectationUpdates)(results, applicableExpectations, {
                    platforms: [os_1.default.platform()],
                    parameters,
                });
                if (recommendation.length > 0) {
                    fail = true;
                    recommendations.push(...recommendation);
                }
                else {
                    console.log('Test run matches expectations');
                    continue;
                }
            }
            catch (err) {
                fail = true;
                console.error(err);
            }
        }
    }
    catch (err) {
        fail = true;
        console.error(err);
    }
    finally {
        const toAdd = recommendations.filter(item => {
            return item.action === 'add';
        });
        if (toAdd.length) {
            console.log('Add the following to TestExpectations.json to ignore the error:');
            (0, utils_js_1.prettyPrintJSON)(toAdd.map(item => {
                return item.expectation;
            }));
        }
        const toRemove = recommendations.filter(item => {
            return item.action === 'remove';
        });
        if (toRemove.length) {
            console.log('Remove the following from the TestExpectations.json to ignore the error:');
            (0, utils_js_1.prettyPrintJSON)(toRemove.map(item => {
                return item.expectation;
            }));
        }
        const toUpdate = recommendations.filter(item => {
            return item.action === 'update';
        });
        if (toUpdate.length) {
            console.log('Update the following expectations in the TestExpecations.json to ignore the error:');
            (0, utils_js_1.prettyPrintJSON)(toUpdate.map(item => {
                return item.expectation;
            }));
        }
        process.exit(fail ? 1 : 0);
    }
}
main().catch(error => {
    console.error(error);
    process.exit(1);
});
//# sourceMappingURL=main.js.map