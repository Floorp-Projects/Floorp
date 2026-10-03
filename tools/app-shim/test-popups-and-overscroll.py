#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Check remote PWA popup geometry and phased scrolling in an isolated Runtime."""

import argparse
import http.server
import importlib.util
import json
from pathlib import Path
import plistlib
import tempfile
import threading
import time
import uuid

spec = importlib.util.spec_from_file_location("runtime_helpers", Path(__file__).with_name("test-runtime.py"))
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
STATE = "Services.appShell.hiddenDOMWindow.__popupScrollTest"


class Fixture(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = b'''<!doctype html><meta charset=utf-8><title>PWA popup and scroll regression</title>
<style>body{margin:0;height:8000px;background:#ddd;font:20px system-ui}
header{height:96px;background:#16324f;color:white;padding:0 40px;display:flex;align-items:center}
section{margin:44px 110px}select{font:20px system-ui;width:280px;height:44px}</style>
<header>Native PWA popup and scroll test</header><section><label for=choice>Select an option</label>
<p><select id=choice><option value=alpha>Alpha</option><option value=beta>Beta</option>
<option value=gamma>Gamma</option><option value=delta>Delta</option></select></p></section>'''
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True, type=Path, help="Freshly built, signed test .app")
    parser.add_argument("--output", type=Path, default=Path("_dist/app-shim-popup-scroll"))
    parser.add_argument("--timeout", type=float, default=30)
    args = parser.parse_args()
    browser = args.browser.resolve(strict=True)
    h.run("/usr/bin/codesign", "--verify", "--strict", str(browser))
    args.output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
    profile = output / "profile"
    profile.mkdir(mode=0o700)
    binary = browser / "Contents/MacOS" / plistlib.loads((browser / "Contents/Info.plist").read_bytes())["CFBundleExecutable"]
    port = h.unused_port()
    prefs = {"marionette.port": port, "browser.shell.checkDefaultBrowser": False,
             "browser.aboutwelcome.enabled": False, "browser.startup.homepage_override.mstone": "ignore",
             "browser.startup.page": 0, "browser.startup.homepage": "about:blank",
             "floorp.browser.nativeApp.appShim.enabled": True,
             "browser.sessionstore.resume_from_crash": False, "app.update.enabled": False,
             "datareporting.policy.dataSubmissionEnabled": False, "apz.test.logging_enabled": True}
    (profile / "user.js").write_text("\n".join(f"user_pref({json.dumps(k)}, {json.dumps(v)});" for k, v in prefs.items()) + "\n")
    report = {"status": "failed", "browser": str(browser), "output": str(output)}
    host = client = server = None
    app_id = "{" + str(uuid.uuid4()) + "}"
    log = (output / "browser.log").open("w")
    exit_code = 1
    try:
        host = h.HostProcess(browser, binary, profile, output, log, launch_services=True)

        def connect():
            if host.poll() is not None:
                raise RuntimeError("Test browser exited before Marionette was ready")
            try:
                return h.Marionette(port)
            except OSError:
                return None

        client = h.wait_for("Marionette", connect, args.timeout)
        host.verify_session(client.session)
        client.context("chrome")
        identity = json.loads(client.script(f"return {h.SERVICE}.hostIdentityJSON;"))
        profile_id = "popup-scroll-" + uuid.uuid4().hex
        bundle = h.seal_bundle(output, browser / "Contents/MacOS/floorp-app-shim", identity, profile, app_id, profile_id)
        client.script(f'''{STATE}={{events:[],frames:{{}},failures:[]}};
{STATE}.observer={{observe(s,t,d){{if(t==='floorp-web-app-shim-event'){{const e=JSON.parse(d);
if(e.type===111){STATE}.frames[e.payload.windowId]=({STATE}.frames[e.payload.windowId]??0)+1;
else {STATE}.events.push(e);}}else {STATE}.failures.push(d);}}}};
Services.obs.addObserver({STATE}.observer,'floorp-web-app-shim-event');
Services.obs.addObserver({STATE}.observer,'floorp-web-app-presentation-failed');
{h.SERVICE}.configure(arguments[1]);{h.SERVICE}.registerApp(arguments[0],arguments[2]);
{h.SERVICE}.launchApp(arguments[0]);''', app_id, profile_id, str(bundle))

        def events():
            client.context("chrome")
            values = client.script(f"return {STATE}.events;")
            if any(event["type"] == "disconnected" for event in values):
                raise RuntimeError("Shim disconnected during popup/scroll regression")
            return values

        connected = h.wait_for("Authenticated Shim", lambda: next((e for e in events() if e["type"] == "connected"), None), args.timeout)
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        client.script(f'''const {{BrowserWindowTracker}}=ChromeUtils.importESModule('resource:///modules/BrowserWindowTracker.sys.mjs');
const args=Cc['@mozilla.org/supports-string;1'].createInstance(Ci.nsISupportsString);args.data=arguments[1];
{h.SERVICE}.withWindowContext(arguments[0],{{createWindow(){{{STATE}.window=BrowserWindowTracker.openWindow(
{{args,features:'width=900,height=700',remote:true,fission:true}});return {STATE}.window;}}}});''', app_id, f"http://127.0.0.1:{server.server_port}/")
        created = h.wait_for("Native window", lambda: next((e for e in events() if e["type"] == 101 and e["payload"].get("kind") == "created"), None), args.timeout)
        root_id = created["payload"]["windowId"]
        handle = h.wait_for("Browsing context", lambda: client.script(f'''const w={STATE}.window;
if(!w?.gBrowserInit?.delayedStartupFinished)return null;
const {{NavigableManager}}=ChromeUtils.importESModule('chrome://remote/content/shared/NavigableManager.sys.mjs');
return NavigableManager.getIdForBrowser(w.gBrowser.selectedBrowser);'''), args.timeout)
        client.switch(handle)
        client.context("content")
        h.wait_for("Select fixture", lambda: client.script("return !!document.querySelector('#choice');"), args.timeout)
        client.context("chrome")
        report["preferences"] = client.script("return {nativeSelect:Services.prefs.getBoolPref('widget.macos.allow-native-select'),nativeMenus:Services.prefs.getBoolPref('widget.macos.native-anchored-menus'),overscroll:Services.prefs.getBoolPref('apz.overscroll.enabled'),apz:Services.prefs.getBoolPref('layers.async-pan-zoom.enabled')};")
        if not all(report["preferences"].values()):
            raise RuntimeError("Native menu and APZ preferences must remain enabled")
        report["popups"] = []
        for zoom, x, y in ((1, 160, 120), (1.5, 260, 180)):
            client.context("chrome")
            client.script(f"const w={STATE}.window;w.moveTo(arguments[1],arguments[2]);w.gBrowser.selectedBrowser.browsingContext.fullZoom=arguments[0];", zoom, x, y)

            def window_geometry():
                client.context("chrome")
                return client.script(f'''const w={STATE}.window;
const native={STATE}.events.filter(e=>e.type===101&&e.payload.windowId===arguments[0]).at(-1)?.payload;
if(!native?.visible||native.width<800||native.height<600)return null;
if(Math.abs(w.screenX-native.x)>1||Math.abs(w.screenY-native.y)>1)return null;
const r=w.gBrowser.selectedBrowser.getBoundingClientRect();
return {{x:w.screenX+r.x,y:w.screenY+r.y,scale:native.scale,native}};''', root_id)

            geometry = h.wait_for("Native parent window geometry", window_geometry, args.timeout)

            def content_geometry():
                client.context("content")
                value = client.script("return {x:window.mozInnerScreenX,y:window.mozInnerScreenY,scale:devicePixelRatio};")
                if (abs(value["scale"] - geometry["scale"] * zoom) < 0.01
                        and abs(value["x"] * zoom - geometry["x"]) < 1
                        and abs(value["y"] * zoom - geometry["y"]) < 1):
                    return value
                return None

            h.wait_for("Content geometry after moving and zooming", content_geometry, args.timeout)
            client.context("content")
            rect = client.script("const select=document.querySelector('#choice');select.value='alpha';const r=select.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};")
            client.command("WebDriver:PerformActions", {"actions": [{"type": "pointer", "id": "mouse", "parameters": {"pointerType": "mouse"}, "actions": [{"type": "pointerMove", "duration": 0, "origin": "viewport", "x": round(rect["x"]), "y": round(rect["y"])}, {"type": "pointerDown", "button": 0}, {"type": "pointerUp", "button": 0}]}]})
            client.context("chrome")

            def popup_snapshot():
                return client.script(f'''const s={STATE},p=s.window.document.querySelector('#ContentSelectDropdown')?.menupopup;
if(p?.state!=='open')return null;
const {{SelectParentHelper}}=ChromeUtils.importESModule('moz-src:///toolkit/actors/SelectParent.sys.mjs');
const states=s.events.filter(e=>e.type===101&&e.payload.windowId!==arguments[0]).map(e=>e.payload);
const native=states.at(-1);if(!native?.visible||!s.frames[native.windowId])return null;
return {{rect:p.getOuterScreenRect().toJSON(),anchor:SelectParentHelper._selectRect,native,
frames:s.frames[native.windowId],items:p.children.length}};''', root_id)

            popup = h.wait_for("Shim-owned select popup frame", popup_snapshot, args.timeout)
            rect, anchor, native = popup["rect"], popup["anchor"], popup["native"]
            if abs(rect["width"] - anchor["width"]) > 2 or rect["height"] <= anchor["height"]:
                raise RuntimeError(f"Select popup dimensions disagree with its anchor: {popup}")
            if abs(rect["x"] - anchor["left"]) > 2 or abs(rect["y"] - anchor["top"]) > anchor["height"]:
                raise RuntimeError(f"Select popup is not aligned with its anchor: {popup}")
            if any(abs(native[key] - rect[key]) > 1 for key in ("x", "y", "width", "height")):
                raise RuntimeError(f"Native popup geometry disagrees with Gecko: {popup}")
            report["popups"].append({"zoom": zoom, "parent": geometry["native"], **popup})
            client.script(f"const p={STATE}.window.document.querySelector('#ContentSelectDropdown').menupopup;p.children[1].doCommand();p.hidePopup();")
            client.context("content")
            h.wait_for("Select command reaching the page", lambda: client.script("return document.querySelector('#choice').value==='beta';"), args.timeout)

        client.context("chrome")
        client.script(f"{STATE}.window.gBrowser.selectedBrowser.browsingContext.fullZoom=1;")

        def scroll_state():
            client.context("content")
            response = client.command("WebDriver:ExecuteScript", {"sandbox": "system", "script": "const u=window.windowUtils;return {rootId:u.getViewId(document.scrollingElement),scrollY:window.scrollY,data:u.getCompositorAPZTestData().additionalData};", "args": []})
            return response["value"]

        def overscrolled(value):
            return any(str(item["key"]) == str(value["rootId"]) and "overscrolled" in item["value"].split(",") for item in value["data"])

        def pan(phase, delta):
            client.context("chrome")
            client.script("Services.obs.notifyObservers(null,'floorp-web-app-shim-event',JSON.stringify({appId:arguments[0],type:100,payload:arguments[1]}));", app_id, {"windowId": root_id, "kind": "scroll", "x": 600, "y": 500, "modifiers": 0, "deltaX": 0, "deltaY": delta, "precise": True, "phase": phase, "momentumPhase": 0, "swipeEnabled": True, "timestamp": time.monotonic()})

        client.context("content")
        client.script("window.scrollTo(0,0);")
        report["scrollBefore"] = scroll_state()
        client.context("chrome")
        frames_before = client.script(f"return {STATE}.frames[arguments[0]]??0;", root_id)
        pan(1, 40)
        for _ in range(4):
            time.sleep(0.05)
            pan(4, 40)
        report["overscroll"] = h.wait_for("Overscroll at the page top", lambda: (value if overscrolled(value := scroll_state()) else None), args.timeout)

        def next_frame():
            client.context("chrome")
            frames = client.script(f"return {STATE}.frames[arguments[0]]??0;", root_id)
            return frames if frames > frames_before else None

        frames_before = h.wait_for("Overscroll compositor frame", next_frame, args.timeout)
        pan(8, 0)
        report["scrollSettled"] = h.wait_for("Overscroll animation settling", lambda: (value if not overscrolled(value := scroll_state()) else None), args.timeout)
        h.wait_for("Snap-back compositor frame", next_frame, args.timeout)
        if report["scrollSettled"]["scrollY"] != 0:
            raise RuntimeError("Top-edge overscroll changed the page's final scroll position")
        client.context("chrome")
        report["frames"] = client.script(f"return {STATE}.frames;")
        report["presentationFailures"] = client.script(f"return {STATE}.failures;")
        events()
        if report["presentationFailures"] or host.poll() is not None:
            raise RuntimeError("Presentation or host failed during the regression")
        report.update({"status": "passed", "hostPid": host.pid, "shimPid": connected["payload"]["pid"]})
        exit_code = 0
    except Exception as error:
        report["error"] = str(error)
    finally:
        if client:
            try:
                client.context("chrome")
                client.script(f"if({STATE}?.observer){{Services.obs.removeObserver({STATE}.observer,'floorp-web-app-shim-event');Services.obs.removeObserver({STATE}.observer,'floorp-web-app-presentation-failed');}}if({STATE}?.window&&!{STATE}.window.closed){STATE}.window.close();{h.SERVICE}.sendControl(arguments[0],40,'{{}}');", app_id)
            except Exception as error:
                report["cleanupError"] = str(error)
                exit_code = 1
                report["status"] = "failed"
            client.close()
        if server:
            server.shutdown()
            server.server_close()
        if host:
            host.close()
        log.close()
        (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
