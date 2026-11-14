// SPDX-License-Identifier: MPL-2.0
// Real macOS fixture only: no app execution, OS registration or xattr mutation.
import { bounded, ExposedSupport, ID, installAdapter, join, privateRoot, SIGN } from "./fixture.ts";
import { relative, sep } from "node:path";
import { textByte } from "./macho.ts";
if (Deno.build.os !== "darwin") throw new Error("native fixture requires macOS");
const root = await privateRoot(Deno.env.get("PWA_FIXTURE_ROOT") ?? "");
const path = join(root, "fixture.app");
let lastVerification: {code: number; output: string} | undefined;
const restore = await installAdapter(root, async (command, args) => {
  const result = await bounded(command, args);
  if (command === SIGN && args[0] === "--verify") lastVerification = result;
  return result;
});
const check = async (path: string, kind: "file" | "directory") => {
  const stat = await Deno.lstat(path);
  if (stat.isSymlink || (kind === "file" ? !stat.isFile : !stat.isDirectory)) throw new Error("unexpected fixture entry: " + path);
  const canonical = await Deno.realPath(path); const rel = relative(root, canonical);
  if (!rel || rel === ".." || rel.startsWith(".." + sep)) throw new Error("fixture path escaped root");
};
try {
  const support = new ExposedSupport();
  await support.compile(path);
  for (const directory of [path, join(path,"Contents"), join(path,"Contents","MacOS"), join(path,"Contents","Resources"), join(path,"Contents","Resources","Scripts")]) await check(directory, "directory");
  const applet = join(path,"Contents","MacOS","applet"); const script = join(path,"Contents","Resources","Scripts","main.scpt");
  await check(applet,"file"); await check(script,"file");
  const appletInfo = await Deno.stat(applet); if (!appletInfo.size || appletInfo.mode === null || !(appletInfo.mode & 0o111) || !(await Deno.stat(script)).size) throw new Error("invalid compiler output");
  const plist = join(path,"Contents","Info.plist"); await check(plist,"file");
  const launcher = join(path,"Contents","Resources","launcher.sh");
  await Deno.writeTextFile(launcher, "#!/bin/sh\nexit 0\n", { createNew: true, mode: 0o700 });
  await Deno.writeTextFile(plist, '<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd"><plist version="1.0"><dict><key>CFBundleIdentifier</key><string>' + ID + '</string><key>CFBundleExecutable</key><string>applet</string><key>CFBundlePackageType</key><string>APPL</string></dict></plist>');
  const icon = join(path,"Contents","Resources","app.icns");
  await Deno.writeFile(icon, new Uint8Array([0x69,0x63,0x6e,0x73,0,0,0,8]), {createNew:true});
  await support.sign(path);
  const verify = async () => await bounded(SIGN, ["--verify", "--deep", "--strict", "--verbose=2", path]);
  const valid = await verify(); if (valid.code !== 0) throw new Error("signed fixture verification failed: " + valid.output);
  const details = await bounded(SIGN, ["--display", "--verbose=4", path]);
  if (details.code !== 0 || !details.output.includes("Identifier=" + ID) || !/Signature=adhoc|flags=.*adhoc/i.test(details.output)) throw new Error("signature identity/ad-hoc not proven: " + details.output);
  if (await support.verify(path) !== "valid") throw new Error("production classifier did not accept signed fixture");
  const evidence: unknown[] = []; const failures: string[] = [];
  for (const [item, target] of [["applet", applet], ["main.scpt", script], ["icon", icon]] as const) {
    await check(target, "file");
    const bytes = await Deno.readFile(target);
    if (bytes.length === 0) throw new Error("empty target " + item);
    let selected: number;
    try { selected = item === "applet" ? textByte(bytes) : bytes.length - 1; }
    catch (error) { const observation = {item, selectorError:String(error)}; evidence.push(observation); console.log(JSON.stringify(observation)); failures.push(item + ": no safe byte"); continue; }
    const changed = bytes.slice(); changed[selected] ^= 1;
    let direct: {code:number;output:string} | undefined; let directError: string | undefined;
    let classifier: string | undefined; let classifierError: string | undefined;
    lastVerification = undefined;
    try {
      await Deno.writeFile(target, changed);
      try { direct = await verify(); } catch (error) { directError = String(error); }
      try { classifier = await support.verify(path); } catch (error) { classifierError = String(error); }
      const observation = {item, selectedOffset:selected, direct, directError, classifier, classifierError, production:lastVerification};
      evidence.push(observation); console.log(JSON.stringify(observation));
      if (!direct || direct.code === 0 || classifier !== "invalid" || !lastVerification) failures.push(item + ": invalidity or classification not established");
    } finally { await Deno.writeFile(target, bytes); }
    // A failed restore or unknown verification ends the experiment immediately.
    if (await support.verify(path) !== "valid") throw new Error("restored fixture did not validate: " + item);
  }
  if (failures.length) throw new Error("fixture observations failed: " + failures.join("; "));
  class OperationalErrorSupport extends ExposedSupport {
    protected override runVerificationCommand(_path: string): Promise<{exitCode:number;output:string}> { return Promise.resolve({exitCode:2,output:"simulated operational failure"}); }
  }
  let errorRejected = false; try { await new OperationalErrorSupport().verify(path); } catch { errorRejected = true; }
  if (!errorRejected) throw new Error("simulated operational failure was accepted");
  console.log(JSON.stringify({mode:"native-macos-source-method-fixture", compiled:true, validSignature:true, identifier:ID, evidence, simulatedOperationalFailureRejected:true, noAppLaunch:true}));
} finally { restore(); }