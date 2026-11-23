// SPDX-License-Identifier: MPL-2.0
import { assertThrows } from "@std/assert";
import { assertPackagedTestSelection } from "../../bridge/loader-features/loader/test/packaged_contract.ts";

Deno.test("packaged QA rejects absent, empty and unavailable scoped controls", () => {
  const available = new Set(["one", "two"]);
  assertThrows(() =>
    assertPackagedTestSelection(undefined, new Set(["one"]), available)
  );
  assertThrows(() => assertPackagedTestSelection("run", new Set(), available));
  assertThrows(() =>
    assertPackagedTestSelection("run", new Set(["missing"]), available)
  );
  assertThrows(() =>
    assertPackagedTestSelection("run", new Set(["one", "missing"]), available)
  );
  assertPackagedTestSelection("run", new Set(["one"]), available);
});
