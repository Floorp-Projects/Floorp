export namespace WasiClocksWallClock {
  export function now(): Datetime;
}
export interface Datetime {
  seconds: bigint,
  nanoseconds: number,
}
