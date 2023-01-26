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
import { z } from 'zod';
export declare const zPlatform: z.ZodEnum<["win32", "linux", "darwin"]>;
export type Platform = z.infer<typeof zPlatform>;
export declare const zTestSuite: z.ZodObject<{
    id: z.ZodString;
    platforms: z.ZodArray<z.ZodEnum<["win32", "linux", "darwin"]>, "many">;
    parameters: z.ZodArray<z.ZodString, "many">;
    expectedLineCoverage: z.ZodNumber;
}, "strip", z.ZodTypeAny, {
    id: string;
    platforms: ("win32" | "linux" | "darwin")[];
    parameters: string[];
    expectedLineCoverage: number;
}, {
    id: string;
    platforms: ("win32" | "linux" | "darwin")[];
    parameters: string[];
    expectedLineCoverage: number;
}>;
export type TestSuite = z.infer<typeof zTestSuite>;
export declare const zTestSuiteFile: z.ZodObject<{
    testSuites: z.ZodArray<z.ZodObject<{
        id: z.ZodString;
        platforms: z.ZodArray<z.ZodEnum<["win32", "linux", "darwin"]>, "many">;
        parameters: z.ZodArray<z.ZodString, "many">;
        expectedLineCoverage: z.ZodNumber;
    }, "strip", z.ZodTypeAny, {
        id: string;
        platforms: ("win32" | "linux" | "darwin")[];
        parameters: string[];
        expectedLineCoverage: number;
    }, {
        id: string;
        platforms: ("win32" | "linux" | "darwin")[];
        parameters: string[];
        expectedLineCoverage: number;
    }>, "many">;
    parameterDefinitons: z.ZodRecord<z.ZodString, z.ZodAny>;
}, "strip", z.ZodTypeAny, {
    testSuites: {
        id: string;
        platforms: ("win32" | "linux" | "darwin")[];
        parameters: string[];
        expectedLineCoverage: number;
    }[];
    parameterDefinitons: Record<string, any>;
}, {
    testSuites: {
        id: string;
        platforms: ("win32" | "linux" | "darwin")[];
        parameters: string[];
        expectedLineCoverage: number;
    }[];
    parameterDefinitons: Record<string, any>;
}>;
export type TestSuiteFile = z.infer<typeof zTestSuiteFile>;
export type TestResult = 'PASS' | 'FAIL' | 'TIMEOUT' | 'SKIP';
export type TestExpectation = {
    testIdPattern: string;
    platforms: NodeJS.Platform[];
    parameters: string[];
    expectations: TestResult[];
};
export type MochaTestResult = {
    fullTitle: string;
    file: string;
    err?: {
        code: string;
    };
};
export type MochaResults = {
    stats: unknown;
    pending: MochaTestResult[];
    passes: MochaTestResult[];
    failures: MochaTestResult[];
};
//# sourceMappingURL=types.d.ts.map