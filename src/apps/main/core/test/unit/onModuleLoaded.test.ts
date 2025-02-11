import { assertRejects } from "@jsr/std__assert";
import { onModuleLoaded } from "../../modules-hooks.ts";

await assertRejects(() => onModuleLoaded("module-that-could-not-be-found"));
await onModuleLoaded("__init_all__");
await assertRejects(() => onModuleLoaded("module-that-could-not-be-found"));
