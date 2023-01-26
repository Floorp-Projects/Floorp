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
Object.defineProperty(exports, "__esModule", { value: true });
exports.zTestSuiteFile = exports.zTestSuite = exports.zPlatform = void 0;
const zod_1 = require("zod");
exports.zPlatform = zod_1.z.enum(['win32', 'linux', 'darwin']);
exports.zTestSuite = zod_1.z.object({
    id: zod_1.z.string(),
    platforms: zod_1.z.array(exports.zPlatform),
    parameters: zod_1.z.array(zod_1.z.string()),
    expectedLineCoverage: zod_1.z.number(),
});
exports.zTestSuiteFile = zod_1.z.object({
    testSuites: zod_1.z.array(exports.zTestSuite),
    parameterDefinitons: zod_1.z.record(zod_1.z.any()),
});
//# sourceMappingURL=types.js.map