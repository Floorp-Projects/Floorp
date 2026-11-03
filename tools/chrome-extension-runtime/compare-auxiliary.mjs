#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
import { runContractComparison } from "./compare-debugger.mjs";
await runContractComparison("auxiliary-fixture", "auxiliary-comparison-");
