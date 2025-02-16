import { $, Options } from "zx";

const $$ = $({preferLocal:true} as Options);

$$`pnpm vitest`
$$`node --experimental-strip-types ./scripts/runClient.ts`