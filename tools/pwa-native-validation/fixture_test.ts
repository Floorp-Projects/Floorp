// SPDX-License-Identifier: MPL-2.0
import { adapter, bounded, COMPILE, ExposedSupport, installAdapter, join, privateRoot, SCRIPT, SIGN } from "./fixture.ts";
const assert = (value: unknown, message: string) => { if (!value) throw new Error(message); };
async function rejects(fn: () => Promise<unknown>, contains: string) { try { await fn(); } catch (e) { assert(String(e).includes(contains), String(e)); return; } throw new Error("expected failure: " + contains); }
const base = Deno.env.get("PWA_FIXTURE_TEST_ROOT") ?? "";
// fileURLToPath is required for Windows drive paths.
import { fileURLToPath } from "node:url";
const fakePath = fileURLToPath(new URL("./fake.ts", import.meta.url));
async function root() { const parent = await privateRoot(base); return await Deno.makeTempDir({ dir: parent, prefix: "host-" }); }
function fakeSpawn(root: string, mode: string, timeout = 5000) {
  return (_command: string, args: string[]) => bounded(Deno.execPath(), ["run", "--no-config", "--allow-read=" + fakePath + "," + root, "--allow-write=" + root, fakePath, mode, args[1] ?? ""], timeout);
}
Deno.test("production methods load and use adapter with controlled child", async () => {
  const r = await root(); const path = join(r, "fixture.app"); let calls: string[] = [];
  const restore = await installAdapter(r, async (command, args) => { calls.push(command); return await fakeSpawn(r, command === COMPILE ? "compile" : "sign")(command, args); });
  try { const support = new ExposedSupport(); await support.compile(path); await support.sign(path); assert(calls.join(",") === [COMPILE, SIGN].join(","), "unexpected method dispatch"); }
  finally { restore(); }
});
Deno.test("adapter fails closed on URI, command, args, existing and escaped paths", async () => {
  const r = await root(); let spawned = false;
  const transport = adapter(r, async () => { spawned = true; return {code: 0, output: ""}; }).Subprocess;
  const path = join(r, "fixture.app");
  await rejects(() => transport.call({command: "/bin/sh", arguments: [], stderr: "stdout"}), "allowlisted");
  await rejects(() => transport.call({command: COMPILE, arguments: ["-o", path, "-e", "bad"], stderr: "stdout"}), "allowlisted");
  await rejects(() => transport.call({command: COMPILE, arguments: ["-o", join(r, "..", "fixture.app"), "-e", SCRIPT], stderr: "stdout"}), "escapes");
  await rejects(() => transport.call({command: SIGN, arguments: [], stderr: "stdout"}), "allowlisted");
  await rejects(() => transport.call({command: COMPILE, arguments: ["-o", path, "-e", SCRIPT], stderr: "pipe"}), "invalid subprocess");
  await Deno.mkdir(path);
  await rejects(() => transport.call({command: SIGN, arguments: ["--force", "--deep", "--sign", "Developer ID", "--identifier", "one.ablaze.floorp.pwa.fixture", "--timestamp=none", path], stderr: "stdout"}), "allowlisted");
  await rejects(() => transport.call({command: SIGN, arguments: ["--force", "--deep", "--sign", "-", "--identifier", "wrong.identity", "--timestamp=none", path], stderr: "stdout"}), "allowlisted");
  await rejects(() => transport.call({command: COMPILE, arguments: ["-o", path, "-e", SCRIPT], stderr: "stdout"}), "already exists");
  assert(!spawned, "unsafe request spawned a child");
  const restore = await installAdapter(r, async () => ({code:0,output:""}));
  try { const g = globalThis as unknown as {ChromeUtils: {importESModule(uri: string): unknown}}; let failed = false; try { g.ChromeUtils.importESModule("resource://other"); } catch { failed = true; } assert(failed, "unexpected URI accepted"); } finally { restore(); }
});
Deno.test("nonzero process is surfaced by production method", async () => {
  const r = await root(); const restore = await installAdapter(r, fakeSpawn(r, "fail"));
  try { await rejects(() => new ExposedSupport().compile(join(r,"fixture.app")), "failed (42)"); } finally { restore(); }
});
Deno.test("bounded child timeout and output overflow fail", async () => {
  const r = await root();
  let reaped: {pid: number; success: boolean} | undefined;
  await rejects(() => bounded(Deno.execPath(), ["run", "--no-config", "--allow-read=" + fakePath + "," + r, "--allow-write=" + r, fakePath, "slow", join(r,"fixture.app")], 300, (pid, status) => { reaped = {pid, success: status.success}; }), "timed out");
  assert(reaped !== undefined && reaped.pid > 0 && reaped.success === false, "timed out child was not awaited and reaped");
  let overflowReaped = false;
  await rejects(() => bounded(Deno.execPath(), ["run", "--no-config", "--allow-read=" + fakePath + "," + r, "--allow-write=" + r, fakePath, "overflow", join(r,"fixture.app")], 5000, (_pid, _status) => { overflowReaped = true; }), "output exceeded");
  assert(overflowReaped, "overflow child was not awaited and reaped");
  const previous = Deno.env.get("PWA_TEST_SENTINEL");
  Deno.env.set("PWA_TEST_SENTINEL", "must-not-inherit");
  try {
    const result = await bounded(Deno.execPath(), ["run", "--no-config", "--allow-read=" + fakePath + "," + r, "--allow-write=" + r, "--allow-env=PWA_TEST_SENTINEL", fakePath, "env", join(r,"fixture.app")]);
    assert(result.code === 0 && result.output.trim() === "absent", "child inherited an unrelated environment variable");
  } finally { if (previous === undefined) Deno.env.delete("PWA_TEST_SENTINEL"); else Deno.env.set("PWA_TEST_SENTINEL", previous); }
});
Deno.test("adapter preserves stream and exit semantics", async () => {
  const r = await root();
  const call = adapter(r, async () => ({code: 7, output: "stderr-and-stdout"})).Subprocess.call;
  const process = await call({command: COMPILE, arguments: ["-o", join(r,"fixture.app"), "-e", SCRIPT], stderr: "stdout"});
  assert(await process.stdout.readString() === "stderr-and-stdout", "missing output");
  assert(await process.stdout.readString() === "", "stream did not end");
  assert((await process.wait()).exitCode === 7, "exit code changed");
});
Deno.test("spawn errors propagate and relative roots fail", async () => {
  await rejects(() => privateRoot(""), "absolute");
  await rejects(() => privateRoot("relative"), "absolute");
  const r = await root(); const restore = await installAdapter(r, async () => { throw new Error("spawn unavailable"); });
  try { await rejects(() => new ExposedSupport().compile(join(r,"fixture.app")), "spawn unavailable"); } finally { restore(); }
});
Deno.test("precreated symlink root and fixture are rejected", async () => {
  const r = join(await privateRoot(base), "symlink-case");
  await privateRoot(r);
  const link = join(r,"fixture.app");
  assert((await Deno.lstat(link)).isSymlink, "preflight symlink missing or not a symlink");
  await rejects(() => privateRoot(link), "real directory");
  const call = adapter(r, async () => ({code:0,output:""})).Subprocess.call;
  await rejects(() => call({command:SIGN, arguments:["--force","--deep","--sign","-","--identifier","one.ablaze.floorp.pwa.fixture","--timestamp=none",link], stderr:"stdout"}), "unsafe fixture");
});
Deno.test("production verifier classifies simulated output and rejects unknown failures", async () => {
  const r = await root(); const path = join(r,"fixture.app"); await Deno.mkdir(path);
  let outcome = {code: 0, output: ""}; let seen: string[] = [];
  const restore = await installAdapter(r, async (command, args) => { seen = [command, ...args]; return outcome; });
  try {
    const support = new ExposedSupport();
    assert(await support.verify(path) === "valid", "valid result was not classified");
    assert(seen.join("|") === [SIGN,"--verify","--deep","--strict",path].join("|"), "unexpected verify argv");
    outcome = {code: 1, output: path + ": a sealed resource is missing or invalid\n"};
    assert(await support.verify(path) === "invalid", "seal failure not classified");
    outcome = {code: 2, output: "simulated operational error"};
    await rejects(() => support.verify(path), "verification failed");
    outcome = {code: 1, output: "unknown diagnostic"};
    await rejects(() => support.verify(path), "verification failed");
    const call = adapter(r, async () => { throw new Error("unsafe spawn"); }).Subprocess.call;
    await rejects(() => call({command:SIGN,arguments:["--verify","--deep","--strict","--verbose",path],stderr:"stdout"}), "allowlisted");
    await rejects(() => call({command:SIGN,arguments:["--verify","--deep","--strict",join(r,"..","fixture.app")],stderr:"stdout"}), "escapes");
  } finally { restore(); }
});
import { textByte } from "./macho.ts";
function makeThin(le = true): Uint8Array {
  const b = new Uint8Array(320), v = new DataView(b.buffer);
  v.setUint32(0,0xfeedfacf,le); v.setUint32(4,0x01000007,le); v.setUint32(16,1,le); v.setUint32(20,152,le);
  v.setUint32(32,0x19,le); v.setUint32(36,152,le);
  const put = (at:number,s:string) => { for(let i=0;i<s.length;i++) b[at+i]=s.charCodeAt(i); };
  put(40,"__TEXT"); v.setBigUint64(72,0n,le); v.setBigUint64(80,320n,le); v.setUint32(96,1,le);
  put(104,"__text"); put(120,"__TEXT"); v.setBigUint64(144,16n,le); v.setUint32(152,256,le); v.setUint32(168,0x80000400,le); b[256]=0x90; return b;
}
function makeFat(wide:boolean): Uint8Array {
  const thin=makeThin(), start=256, b=new Uint8Array(start+thin.length),v=new DataView(b.buffer);
  v.setUint32(0,wide?0xcafebabf:0xcafebabe);v.setUint32(4,1);v.setUint32(8,0x01000007);
  if(wide){v.setBigUint64(16,BigInt(start));v.setBigUint64(24,BigInt(thin.length));}
  else {v.setUint32(16,start);v.setUint32(20,thin.length);} b.set(thin,start); return b;
}
Deno.test("Mach-O text selection handles thin and universal 32/64 headers", () => {
  assert(textByte(makeThin())===256,"thin LE"); assert(textByte(makeThin(false))===256,"thin BE");
  assert(textByte(makeFat(false))===512,"fat32"); assert(textByte(makeFat(true))===512,"fat64");
});
Deno.test("Mach-O selector fails closed on truncated, escaped, invalid structures", () => {
  const cases: Uint8Array[] = [new Uint8Array(), makeThin().slice(0,100), makeFat(false).slice(0,300)];
  const edit=(source:Uint8Array, fn:(v:DataView,b:Uint8Array)=>void) => {const b=source.slice();fn(new DataView(b.buffer),b);return b;};
  cases.push(edit(makeThin(),v=>v.setUint32(152,319,true)));
  cases.push(edit(makeThin(),v=>v.setBigUint64(144,0n,true)));
  cases.push(edit(makeThin(),v=>v.setUint32(36,8,true)));
  cases.push(edit(makeThin(),v=>v.setBigUint64(80,200n,true)));
  cases.push(edit(makeThin(),v=>v.setUint32(168,0,true)));
  cases.push(edit(makeThin(),(_v,b)=>{b[104]=0;}));
  cases.push(edit(makeFat(false),v=>v.setUint32(16,8)));
  cases.push(edit(makeFat(false),v=>v.setUint32(8,1)));
  cases.push(edit(makeFat(true),v=>v.setBigUint64(16,0xffffffffffffffffn)));
  for(const [index,b] of cases.entries()) { let failed=false; try{textByte(b);}catch{failed=true;} assert(failed,"malformed case accepted: "+index); }
});
Deno.test("universal multi-slice parsing rejects overlap and bad second slice", () => {
  const first=makeThin(), second=makeThin(), b=new Uint8Array(128+first.length+second.length), v=new DataView(b.buffer);
  v.setUint32(0,0xcafebabe); v.setUint32(4,2);
  v.setUint32(8,0x01000007); v.setUint32(16,128); v.setUint32(20,first.length);
  v.setUint32(28,0x01000007); v.setUint32(36,128+first.length); v.setUint32(40,second.length);
  b.set(first,128); b.set(second,128+first.length); assert(textByte(b)===384,"first slice text offset");
  const overlap=b.slice(); new DataView(overlap.buffer).setUint32(36,200);
  const invalid=b.slice(); invalid[128+first.length]=0;
  for (const [i,value] of [overlap,invalid].entries()) { let failed=false;try{textByte(value);}catch{failed=true;}assert(failed,"unsafe universal slice "+i); }
});
