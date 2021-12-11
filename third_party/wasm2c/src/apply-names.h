/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WABT_APPLY_NAMES_H_
#define WABT_APPLY_NAMES_H_

#include "src/common.h"

namespace wabt {

struct Module;

/* Use function, import, function type, parameter and local names in Vars
 * that reference them.
 *
 * e.g. transform this:
 *
 *    (func $foo ...)
 *    ...
 *    (call 0 ...)
 *
 * to this:
 *
 *    (func $foo ...)
 *    ...
 *    (call $foo ...)
 */
Result ApplyNames(struct Module*);

}  // namespace wabt

#endif /* WABT_APPLY_NAMES_H_ */
