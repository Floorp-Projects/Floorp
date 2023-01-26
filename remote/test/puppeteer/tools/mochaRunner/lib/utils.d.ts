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
/// <reference types="node" />
import { MochaTestResult, TestExpectation, MochaResults, TestResult } from './types.js';
export declare function extendProcessEnv(envs: object[]): NodeJS.ProcessEnv;
export declare function getFilename(file: string): string;
export declare function readJSON(path: string): unknown;
export declare function filterByPlatform<T extends {
    platforms: NodeJS.Platform[];
}>(items: T[], platform: NodeJS.Platform): T[];
export declare function prettyPrintJSON(json: unknown): void;
export declare function filterByParameters(expectations: TestExpectation[], parameters: string[]): TestExpectation[];
/**
 * The last expectation that matches an empty string as all tests pattern
 * or the name of the file or the whole name of the test the filter wins.
 */
export declare function findEffectiveExpectationForTest(expectations: TestExpectation[], result: MochaTestResult): TestExpectation | undefined;
type RecommendedExpecation = {
    expectation: TestExpectation;
    test: MochaTestResult;
    action: 'remove' | 'add' | 'update';
};
export declare function getExpectationUpdates(results: MochaResults, expecations: TestExpectation[], context: {
    platforms: NodeJS.Platform[];
    parameters: string[];
}): RecommendedExpecation[];
export declare function getTestResultForFailure(test: Pick<MochaTestResult, 'err'>): TestResult;
export declare function getTestId(file: string, fullTitle?: string): string;
export {};
//# sourceMappingURL=utils.d.ts.map