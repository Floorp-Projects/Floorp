// SPDX-License-Identifier: MPL-2.0
// Identify a real Mach-O 64-bit __TEXT,__text file byte, never an arbitrary tail byte.
export function textBytes(bytes: Uint8Array): number[] {
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const length = bytes.byteLength;
  const fail = (): never => { throw new Error("unsupported or malformed Mach-O"); };
  function range(offset: number, size: number, end: number): void {
    if (!Number.isSafeInteger(offset) || !Number.isSafeInteger(size) || offset < 0 || size < 0 || offset > end || size > end - offset) fail();
  }
  function u64(offset: number, le: boolean): number {
    range(offset, 8, length); const n = v.getBigUint64(offset, le);
    if (n > BigInt(Number.MAX_SAFE_INTEGER)) return fail();
    return Number(n);
  }
  function name(offset: number, expected: string): boolean {
    range(offset, 16, length);
    const b = new Uint8Array(16); for (let i = 0; i < expected.length; i++) b[i] = expected.charCodeAt(i);
    return b.every((x, i) => bytes[offset + i] === x);
  }
  function thin(start: number, size: number, cpu?: number): number {
    range(start, size, length); const end = start + size; range(start, 32, end);
    const magic = v.getUint32(start, false);
    const le = magic === 0xcffaedfe ? true : magic === 0xfeedfacf ? false : fail();
    if (cpu !== undefined && v.getUint32(start + 4, le) !== cpu) fail();
    const count = v.getUint32(start + 16, le), commandSize = v.getUint32(start + 20, le);
    if (count > 4096 || commandSize > size - 32) fail();
    const commandsEnd = start + 32 + commandSize; let pos = start + 32; let found: number | undefined;
    for (let n = 0; n < count; n++) {
      range(pos, 8, commandsEnd); const cmd = v.getUint32(pos, le), cmdsize = v.getUint32(pos + 4, le);
      if (cmdsize < 8 || cmdsize % 8 !== 0) fail(); range(pos, cmdsize, commandsEnd);
      if (cmd === 0x19) {
        range(pos, 72, pos + cmdsize); const sections = v.getUint32(pos + 64, le);
        if (sections > Math.floor((cmdsize - 72) / 80)) fail();
        const fileoff = u64(pos + 40, le), filesize = u64(pos + 48, le);
        range(fileoff, filesize, size);
        for (let i = 0; i < sections; i++) {
          const section = pos + 72 + i * 80;
          if (name(pos + 8, "__TEXT") && name(section, "__text") && name(section + 16, "__TEXT")) {
            const sectionSize = u64(section + 40, le), offset = v.getUint32(section + 48, le);
            const flags = v.getUint32(section + 64, le);
            if ((flags & 0x80000400) === 0) fail(); // must contain machine instructions
            if (!sectionSize || offset < 32 + commandSize || offset < fileoff || offset - fileoff > filesize || sectionSize > filesize - (offset - fileoff)) fail();
            range(offset, sectionSize, size); if (found !== undefined) fail(); found = start + offset;
          }
        }
      }
      pos += cmdsize;
    }
    if (pos !== commandsEnd || found === undefined) return fail();
    return found;
  }
  range(0, 4, length); const magic = v.getUint32(0, false);
  if (magic === 0xfeedfacf || magic === 0xcffaedfe) return [thin(0, length)];
  if (magic !== 0xcafebabe && magic !== 0xcafebabf) return fail();
  range(0, 8, length); const count = v.getUint32(4, false), wide = magic === 0xcafebabf;
  const entrySize = wide ? 32 : 20; if (!count || count > 64) fail(); range(8, count * entrySize, length);
  const slices: {start:number; size:number; cpu:number}[] = [];
  for (let i = 0; i < count; i++) {
    const p = 8 + i * entrySize, cpu = v.getUint32(p, false);
    const start = wide ? u64(p + 8, false) : v.getUint32(p + 8, false);
    const size = wide ? u64(p + 16, false) : v.getUint32(p + 12, false);
    if (start < 8 + count * entrySize || !size) fail(); range(start, size, length);
    for (const prior of slices) if (start < prior.start + prior.size && prior.start < start + size) fail();
    slices.push({start,size,cpu});
  }
  const choices = slices.map((slice) => thin(slice.start, slice.size, slice.cpu));
  return choices;
}

export function textByte(bytes: Uint8Array): number { return textBytes(bytes)[0]; }
