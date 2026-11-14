// SPDX-License-Identifier: MPL-2.0
// Real macOS fixture only: no app execution, OS registration or xattr mutation.
import { bounded, ExposedSupport, ID, installAdapter, join, privateRoot, SIGN } from "./fixture.ts";
import { relative, sep } from "node:path";
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
  const evidence: {item: string; directExit: number; directOutput: string; classifier: string; productionExit: number; productionOutput: string}[] = [];
  for (const [item, target] of [["applet", applet], ["main.scpt", script], ["icon", icon]] as const) {
    await check(target, "file");
    const bytes = await Deno.readFile(target);
    if (bytes.length === 0) throw new Error("empty target " + item);
    const changed = bytes.slice(); changed[changed.length - 1] ^= 1;
    await Deno.writeFile(target, changed);
    try {
      const direct = await verify();
      if (direct.code === 0) throw new Error("direct verification accepted tampered " + item);
      lastVerification = undefined;
      let classifier: string;
      try { classifier = await support.verify(path); } catch (error) {
        console.error(JSON.stringify({item, directExit:direct.code, directOutput:direct.output, production:lastVerification, classifierError:String(error)}));
        throw error;
      }
      if (!lastVerification) throw new Error("missing verifier output");
      const observation = {item, directExit:direct.code, directOutput:direct.output, classifier, productionExit:lastVerification.code, productionOutput:lastVerification.output};
      evidence.push(observation);
      console.log(JSON.stringify(observation));
      if (classifier !== "invalid") throw new Error("production classifier failed to reject tampered " + item);
    } finally { await Deno.writeFile(target, bytes); }
    if (await support.verify(path) !== "valid") throw new Error("restored fixture did not validate: " + item);
  }
  class OperationalErrorSupport extends ExposedSupport {
    protected override runVerificationCommand(_path: string): Promise<{exitCode:number;output:string}> { return Promise.resolve({exitCode:2,output:"simulated operational failure"}); }
  }
  let errorRejected = false; try { await new OperationalErrorSupport().verify(path); } catch { errorRejected = true; }
  if (!errorRejected) throw new Error("simulated operational failure was accepted");
  console.log(JSON.stringify({mode:"native-macos-source-method-fixture", compiled:true, validSignature:true, identifier:ID, evidence, simulatedOperationalFailureRejected:true, noAppLaunch:true}));
} finally { restore(); }
