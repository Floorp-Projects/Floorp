#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Exercise Japanese conversion through a real Shim in an isolated profile."""

import argparse
import ctypes
import http.server
import importlib.util
import json
from pathlib import Path
import plistlib
import tempfile
import threading
import time
import uuid

spec = importlib.util.spec_from_file_location("helpers", Path(__file__).with_name("test-runtime.py"))
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
STATE = "Services.appShell.hiddenDOMWindow.__pwaIMEProbe"


class Page(http.server.BaseHTTPRequestHandler):
    editor_type = "textarea"

    def do_GET(self):
        element = ('<textarea id=editor autofocus aria-label="Japanese input test"></textarea>'
                   if self.editor_type == "textarea" else
                   '<div id=editor contenteditable=true tabindex=0 aria-label="Japanese input test"></div>')
        body = """<!doctype html><meta charset=utf-8><title>PWA Japanese input probe</title>
<style>body{padding:60px;font:24px system-ui}#editor{font:24px system-ui;width:80%;height:180px;border:1px solid}</style>
""" + element + """
<script>window.probe={events:[]};
for(const type of ['keydown','keyup','beforeinput','input','compositionstart','compositionupdate','compositionend'])
editor.addEventListener(type,e=>probe.events.push({type:e.type,key:e.key,data:e.data,inputType:e.inputType,
isComposing:e.isComposing,isTrusted:e.isTrusted,value:editor.value ?? editor.textContent}));</script>"""
        body = body.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args):
        pass


def native_key(pid, keycode):
    graphics = ctypes.CDLL("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
    core = ctypes.CDLL("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")
    graphics.CGPreflightPostEventAccess.restype = ctypes.c_bool
    if not graphics.CGPreflightPostEventAccess():
        raise RuntimeError("Existing macOS event-posting permission is required")
    graphics.CGEventCreateKeyboardEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint16, ctypes.c_bool]
    graphics.CGEventCreateKeyboardEvent.restype = ctypes.c_void_p
    graphics.CGEventSetFlags.argtypes = [ctypes.c_void_p, ctypes.c_uint64]
    graphics.CGEventPostToPid.argtypes = [ctypes.c_int, ctypes.c_void_p]
    core.CFRelease.argtypes = [ctypes.c_void_p]
    for down in (True, False):
        event = graphics.CGEventCreateKeyboardEvent(None, keycode, down)
        if not event:
            raise RuntimeError("Cannot create native keyboard event")
        graphics.CGEventSetFlags(event, 0)
        graphics.CGEventPostToPid(pid, event)
        core.CFRelease(event)
    time.sleep(0.12)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", type=Path, required=True, help="Signed test copy of a browser .app")
    parser.add_argument("--shim", type=Path, help="Override the packaged Shim executable")
    parser.add_argument("--output", type=Path, default=Path("_dist/app-shim-ime"))
    parser.add_argument("--iterations", type=int, default=3)
    parser.add_argument("--editor", choices=("textarea", "contenteditable"), default="textarea")
    args = parser.parse_args()
    if args.iterations < 1:
        parser.error("--iterations must be positive")
    Page.editor_type = args.editor
    browser = args.browser.resolve(strict=True)
    shim = (args.shim or browser / "Contents/MacOS/floorp-app-shim").resolve(strict=True)
    h.run("/usr/bin/codesign", "--verify", "--strict", str(browser))
    args.output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
    profile = output / "profile"
    profile.mkdir(mode=0o700)
    exe = browser / "Contents/MacOS" / plistlib.loads((browser / "Contents/Info.plist").read_bytes())["CFBundleExecutable"]
    port = h.unused_port()
    prefs = {"marionette.port": port, "browser.shell.checkDefaultBrowser": False, "browser.aboutwelcome.enabled": False,
             "browser.startup.homepage_override.mstone": "ignore", "browser.startup.page": 0,
             "browser.startup.homepage": "about:blank", "floorp.browser.nativeApp.appShim.enabled": True,
             "browser.sessionstore.resume_from_crash": False, "app.update.enabled": False,
             "datareporting.policy.dataSubmissionEnabled": False}
    (profile / "user.js").write_text("\n".join(f"user_pref({json.dumps(k)}, {json.dumps(v)});" for k, v in prefs.items()) + "\n")
    report = {"status": "failed", "browser": str(browser), "shim": str(shim), "editor": args.editor,
              "output": str(output), "steps": []}
    app_id = "{" + str(uuid.uuid4()) + "}"
    host = client = server = None
    log = (output / "browser.log").open("w")
    code = 1
    try:
        host = h.HostProcess(browser, exe, profile, output, log, launch_services=True)

        def connect():
            if host.poll() is not None:
                raise RuntimeError("Test host exited")
            try:
                return h.Marionette(port)
            except OSError:
                return None

        client = h.wait_for("Marionette", connect, 35)
        host.verify_session(client.session)
        client.context("chrome")
        identity = json.loads(client.script(f"return {h.SERVICE}.hostIdentityJSON;"))
        profile_id = "ime-" + uuid.uuid4().hex
        bundle = h.seal_bundle(output, shim, identity, profile, app_id, profile_id)
        client.script(f"""{STATE}={{events:[]}};
{STATE}.observer={{observe(s,t,d){{const e=JSON.parse(d);if(e.type!==111){STATE}.events.push(e);}}}};
Services.obs.addObserver({STATE}.observer,'floorp-web-app-shim-event');
{h.SERVICE}.configure(arguments[1]);{h.SERVICE}.registerApp(arguments[0],arguments[2]);
{h.SERVICE}.launchApp(arguments[0]);""", app_id, profile_id, str(bundle))
        connected = h.wait_for("Shim authentication", lambda: next((e for e in client.script(f"return {STATE}.events;") if e["type"] == "connected"), None), 20)
        pid = connected["payload"]["pid"]
        report.update({"hostPid": host.pid, "shimPid": pid})
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Page)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        client.script(f"""const {{BrowserWindowTracker}}=ChromeUtils.importESModule('resource:///modules/BrowserWindowTracker.sys.mjs');
const args=Cc['@mozilla.org/supports-string;1'].createInstance(Ci.nsISupportsString);args.data=arguments[1];
{h.SERVICE}.withWindowContext(arguments[0],{{createWindow(){{{STATE}.window=BrowserWindowTracker.openWindow(
{{args,features:'width=900,height=700',remote:true,fission:true}});return {STATE}.window;}}}});""", app_id, f"http://127.0.0.1:{server.server_port}/")
        created = h.wait_for("Native PWA window", lambda: next((e for e in client.script(f"return {STATE}.events;") if e["type"] == 101 and e["payload"].get("kind") == "created"), None), 20)
        handle = h.wait_for("PWA browsing context", lambda: client.script(f"""const w={STATE}.window;
if(!w?.gBrowserInit?.delayedStartupFinished)return null;
const {{NavigableManager}}=ChromeUtils.importESModule('chrome://remote/content/shared/NavigableManager.sys.mjs');
return NavigableManager.getIdForBrowser(w.gBrowser.selectedBrowser);"""), 20)
        client.switch(handle)
        client.context("content")
        h.wait_for("Fixture", lambda: client.script("return !!window.probe;"), 20)
        client.script("editor.focus();")
        client.context("chrome")
        client.script(f"{h.SERVICE}.sendControl(arguments[0],13,JSON.stringify({{windowId:arguments[1]}}));", app_id, created["payload"]["windowId"])
        time.sleep(0.8)
        native_key(pid, 104)  # JIS Kana: use the installed Japanese input method.

        def snapshot(label):
            client.context("content")
            data = client.script("return {value:editor.value ?? editor.textContent,events:probe.events};")
            report["steps"].append({"label": label, "value": data["value"], "eventCount": len(data["events"])})
            report["events"] = data["events"]
            print(json.dumps({"label": label, "value": data["value"]}, ensure_ascii=False), flush=True)
            return data

        for iteration in range(args.iterations):
            prefix = report["steps"][-1]["value"] if iteration else ""
            for key in (40, 0, 45, 38, 34):  # kanji
                native_key(pid, key)
            snapshot(f"{iteration}:typed")
            candidates = []
            for conversion in range(3):
                native_key(pid, 49)  # Space advances the conversion candidate.
                candidate = snapshot(f"{iteration}:space:{conversion}")
                if any(char in candidate["value"] for char in " \u3000\n"):
                    raise RuntimeError("Conversion inserted whitespace into the editor")
                if sum(e["type"] == "compositionend" for e in candidate["events"]) != iteration:
                    raise RuntimeError("Space prematurely committed the composition")
                if not candidate["value"].startswith(prefix):
                    raise RuntimeError("Conversion replaced previously committed text")
                candidates.append(candidate["value"])
            if len(set(candidates)) < 2:
                raise RuntimeError("Repeated Space did not advance the Japanese conversion candidate")
            native_key(pid, 36)  # Return selects a candidate or commits.
            client.context("content")
            ended = client.script("return probe.events.filter(e=>e.type==='compositionend').length;")
            if ended == iteration:
                native_key(pid, 36)  # The candidate panel consumed the first Return.
            final = snapshot(f"{iteration}:committed")
            if sum(e["type"] == "compositionend" for e in final["events"]) != iteration + 1:
                raise RuntimeError("Return did not commit the Japanese conversion")
        client.context("chrome")
        report["shimEvents"] = client.script(f"return {STATE}.events.filter(e=>e.type===100);")
        compositions = [e for e in final["events"] if e["type"] == "compositionstart"]
        commits = [e for e in final["events"] if e["type"] == "compositionend"]
        if len(compositions) != args.iterations or len(commits) != args.iterations:
            raise RuntimeError("Japanese input did not complete each composition")
        if any(char in final["value"] for char in " \u3000\n"):
            raise RuntimeError("Conversion inserted whitespace into the editor")
        if any(not e["isTrusted"] for e in final["events"]):
            raise RuntimeError("Native input must produce trusted DOM events")
        native_key(pid, 49)
        ordinary_space = snapshot("committed:ordinary-space")
        if ordinary_space["value"] not in (final["value"] + " ", final["value"] + "\u3000"):
            raise RuntimeError("Ordinary Space did not insert exactly one space after commit")

        def event_count(event_type):
            client.context("content")
            return client.script("return probe.events.filter(e=>e.type===arguments[0]).length;", event_type)

        for label, cancel_key, limit in (
            ("escape", 53, 3), ("backspace", 51, 8), ("empty-backspace", 51, 8)
        ):
            if label == "empty-backspace":
                client.context("content")
                client.script("""if(editor instanceof HTMLTextAreaElement)editor.select();
else getSelection().selectAllChildren(editor);""")
                native_key(pid, 51)
                cleared = snapshot("committed:all-deleted")
                if cleared["value"] != "":
                    raise RuntimeError("Deleting all committed text left characters in the editor")
            prefix = report["steps"][-1]["value"]
            started = event_count("compositionstart")
            ended = event_count("compositionend")
            for key in (40, 0, 45, 38, 34):
                native_key(pid, key)
            native_key(pid, 49)
            composing = snapshot(f"{label}:composing")
            if event_count("compositionstart") != started + 1 or composing["value"] == prefix:
                raise RuntimeError(f"{label}: input did not start a visible composition")
            for _ in range(limit):
                native_key(pid, cancel_key)
                if event_count("compositionend") == ended + 1:
                    break
            cancelled = snapshot(f"{label}:cancelled")
            if event_count("compositionend") != ended + 1:
                raise RuntimeError(f"{label}: empty marked text did not end the composition")
            if cancelled["value"] != prefix:
                raise RuntimeError(f"{label}: cancellation changed previously committed text")
            for key in (40, 0, 45, 38, 34):
                native_key(pid, key)
            resumed = snapshot(f"{label}:resumed")
            if event_count("compositionstart") != started + 2 or resumed["value"] == prefix:
                raise RuntimeError(f"{label}: typing did not restart a visible composition")
            candidates = []
            for conversion in range(3):
                native_key(pid, 49)
                candidate = snapshot(f"{label}:converted:{conversion}")
                if event_count("compositionend") != ended + 1:
                    raise RuntimeError(f"{label}: Space prematurely committed the restarted composition")
                if (not candidate["value"].startswith(prefix) or
                    any(char in candidate["value"][len(prefix):] for char in " \u3000\n")):
                    raise RuntimeError(f"{label}: conversion changed prior text or inserted whitespace")
                candidates.append(candidate["value"])
            if len(set(candidates)) < 2:
                raise RuntimeError(f"{label}: restarted conversion did not advance the candidate")
            native_key(pid, 36)
            if event_count("compositionend") == ended + 1:
                native_key(pid, 36)
            committed = snapshot(f"{label}:committed")
            if event_count("compositionend") != ended + 2 or not committed["value"].startswith(prefix):
                raise RuntimeError(f"{label}: restarted composition did not commit correctly")
            if any(not e["isTrusted"] for e in committed["events"]):
                raise RuntimeError("Cancellation and resumed input must produce trusted DOM events")
        report["status"] = "passed"
        code = 0
    except Exception as error:
        report["error"] = str(error)
    finally:
        if client:
            try:
                client.context("chrome")
                report["shimEvents"] = client.script(f"return {STATE}?.events.filter(e=>e.type===100);")
                client.script(f"""if({STATE}?.observer)Services.obs.removeObserver({STATE}.observer,'floorp-web-app-shim-event');
if({STATE}?.window&&!{STATE}.window.closed){STATE}.window.close();{h.SERVICE}.sendControl(arguments[0],40,'{{}}');""", app_id)
            except Exception as error:
                report["cleanupError"] = str(error)
                report["status"] = "failed"
                code = 1
            client.close()
        if server:
            server.shutdown()
            server.server_close()
        if host:
            host.close()
        log.close()
        (output / "result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
        print(json.dumps({"status": report["status"], "error": report.get("error"), "report": str(output / "result.json")}, ensure_ascii=False))
    return code


if __name__ == "__main__":
    raise SystemExit(main())
