#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Check shipped Web Panel settings and real HTTP headers in an isolated profile."""

import argparse
import base64
import importlib.util
import json
import logging
import os
import platform
import plistlib
import re
import subprocess
import tempfile
import threading
from pathlib import Path

from final_web_panel_http import (
    EchoServer, validate_redirect_trace, validate_ua_values,
)

MOBILE_UA = (
    "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 "
    "Mobile Safari/537.36 Edg/114.0.1823.79"
)
STATE = "window.wrappedJSObject.__floorpFinalUAProbe"
LOGGER = logging.getLogger(__name__)
PANEL_NAVIGATION_SCRIPT = f"""
    const host={STATE}.main.document.getElementById('sidebar-panel-'+arguments[0]);
    host.browsingContext.associatedWindow.floorpWebPanelContentBrowser.loadURI(
        Services.io.newURI(arguments[1]),
        {{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()}});
"""
REDIRECT_IDS = ("web-panel-ua.redirect.mobile-on", "web-panel-ua.redirect.mobile-off")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True, type=Path)
    parser.add_argument("--native-record", required=True, type=Path)
    parser.add_argument("--expected-floorp-version", required=True)
    parser.add_argument("--expected-firefox-version", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    record = json.loads(args.native_record.read_text())
    require(record["schema_version"] == 2 and record["verification"]["status"] == "verified",
            "Expected the normal pipeline's verified native record")
    require(record["firefox_version"] == args.expected_firefox_version,
            "Native record differs from the expected final Firefox version")
    expected_build_id = record["runtime"]["expected_build_id"]
    require(re.fullmatch(r"[0-9]{14}", expected_build_id), "Invalid native BuildID")
    require(record["app_build_id"] == record["platform_build_id"] == expected_build_id,
            "Native record contains inconsistent BuildIDs")
    require(not any(os.environ.get(name) for name in (
        "MOZ_DISABLE_CONTENT_SANDBOX", "MOZ_DISABLE_GMP_SANDBOX",
        "MOZ_DISABLE_RDD_SANDBOX", "MOZ_DISABLE_GPU_SANDBOX",
    )), "Refusing a sandbox-disabled test environment")

    helper = Path(__file__).parent / "app-shim/test-runtime.py"
    spec = importlib.util.spec_from_file_location("native_runtime_helpers", helper)
    h = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(h)
    browser = args.browser.resolve(strict=True)
    if browser.is_dir():
        info = plistlib.loads((browser / "Contents/Info.plist").read_bytes())
        binary = browser / "Contents/MacOS" / info["CFBundleExecutable"]
    else:
        binary = browser
    args.output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
    profile = output / "profile"
    profile.mkdir(mode=0o700)
    port = h.unused_port()
    preferences = {
        "marionette.port": port,
        "browser.shell.checkDefaultBrowser": False,
        "browser.aboutwelcome.enabled": False,
        "browser.startup.homepage_override.mstone": "ignore",
        "browser.startup.page": 0,
        "browser.startup.homepage": "about:blank",
        "browser.sessionstore.resume_from_crash": False,
        "app.update.enabled": False,
        "datareporting.policy.dataSubmissionEnabled": False,
    }
    (profile / "user.js").write_text("\n".join(
        f"user_pref({json.dumps(key)}, {json.dumps(value)});"
        for key, value in preferences.items()
    ) + "\n")
    report = {"status": "failed", "browser": str(browser), "nativeRecord": record,
              "profilePreferences": preferences, "checks": [], "method": "bundled UI and HTTP",
              "redirectChecks": [{"caseId": case_id, "status": "NOT_EXECUTED"}
                                 for case_id in REDIRECT_IDS]}
    server = EchoServer()
    base = f"http://127.0.0.1:{server.server_port}/ua"
    threading.Thread(target=server.serve_forever, daemon=True).start()
    host = client = None
    panel_id, other_id = "final-ua-mobile", "final-ua-desktop"
    try:
        with (output / "browser.log").open("w") as log:
            host = h.HostProcess(browser, binary, profile, output, log,
                                 launch_services=platform.system() == "Darwin")

        def connect():
            require(host.poll() is None, "The isolated browser exited before Marionette")
            try:
                return h.Marionette(port)
            except OSError:
                return None

        client = h.wait_for("Marionette", connect, 90)
        host.verify_session(client.session)
        report["hostProcessIdentity"] = {
            "pid": host.pid,
            "argv": host.command,
            "verified": host.verified,
        }
        report["marionetteSession"] = client.session
        client.context("chrome")
        h.wait_for("bundled sidebar", lambda: client.script(
            "return !!window.document.getElementById('panel-sidebar-select-box');"), 90)
        identity = client.script("""
            const {NoranekoConstants} = ChromeUtils.importESModule(
                'resource://noraneko/modules/NoranekoConstants.sys.mjs');
            return {version:Services.appinfo.version, appBuildID:Services.appinfo.appBuildID,
                platformBuildID:Services.appinfo.platformBuildID,
                version2:NoranekoConstants.version2, buildID2:NoranekoConstants.buildID2,
                startupMode:Services.prefs.getStringPref('nora.startup.mode',''),
                httpLoaderAllowed:Services.prefs.getBoolPref('nora.dev.allow_http_loader',false),
                sandboxLevel:Services.prefs.getIntPref('security.sandbox.content.level')};
        """)
        report["identity"] = identity
        require(identity["version"] == record["firefox_version"] == args.expected_firefox_version and
                identity["appBuildID"] == identity["platformBuildID"] == expected_build_id,
                "Running browser identity differs from the final native record")
        require(identity["version2"] == args.expected_floorp_version and
                identity["buildID2"] == record["build_id2"], "Floorp package identity mismatch")
        require(identity["startupMode"] not in ("dev", "test") and
                not identity["httpLoaderAllowed"], "Expected shipped modules without a development loader")
        client.script(f"""
            {STATE}={{main:window.wrappedJSObject, ids:{{}}, retired:[], commands:[]}};
            {STATE}.observer={{observe(subject){{
                const channel=subject.QueryInterface(Ci.nsIHttpChannel);
                if(channel.URI.spec.startsWith(argumentsBase)){{
                    {STATE}.ids[channel.URI.spec]=channel.browserId;
                }}
            }}}};
            const argumentsBase=arguments[0];
            Services.obs.addObserver({STATE}.observer,'http-on-modify-request');
            {STATE}.normal={STATE}.main.gBrowser.addTrustedTab(arguments[0]+'?normal');
            {STATE}.main.gBrowser.selectedTab={STATE}.normal;
        """, base)

        def request(key, expected=None, previous=0):
            def received():
                values = server.requests(key)[previous:]
                return values or None

            values = h.wait_for(f"HTTP {key}", received, 35)
            actual = values[0]
            expected = expected or actual
            server.expect_direct(key, expected, previous)
            validate_ua_values(values, expected, key)
            server.assert_healthy()
            return actual

        desktop_ua = request("normal")
        require("Mobile" not in desktop_ua, "Normal tab initially used a mobile UA")
        report["desktopUA"] = desktop_ua

        def reload_normal(label):
            previous = len(server.requests("normal"))
            client.script(f"{STATE}.normal.linkedBrowser.reload();")
            request("normal", desktop_ua, previous)
            normal_id = client.script(f"return {STATE}.normal.linkedBrowser.browserId;")
            observed_id = client.script(f"return {STATE}.ids[arguments[0]];", base + "?normal")
            require(observed_id == normal_id, "Normal tab HTTP channel has a different browser identity")
            report["checks"].append({"key": label, "normalBrowserId": normal_id,
                                     "httpBrowserId": observed_id})

        panels = [{"id": panel_id, "type": "web", "width": 400,
                   "url": base + "?initial-mobile", "userAgent": True, "userContextId": 0},
                  {"id": other_id, "type": "web", "width": 400,
                   "url": base + "?initial-other", "userAgent": False, "userContextId": 0}]
        client.script("""
            Services.prefs.setStringPref('floorp.panelSidebar.config',JSON.stringify({
                globalWidth:400, autoUnload:false, position_start:false,
                displayed:true, webExtensionRunningEnabled:false}));
            Services.prefs.setStringPref('floorp.panelSidebar.data',JSON.stringify({data:arguments[0]}));
        """, panels)

        def pointer(selector, button=0):
            client.context("chrome")
            point = h.wait_for(selector, lambda: client.script("""
                const element=window.document.querySelector(arguments[0]);
                if(!element)return null;
                const r=element.getBoundingClientRect();
                if(!r.width||!r.height)return null;
                return {x:Math.round(r.x+r.width/2), y:Math.round(r.y+r.height/2)};
            """, selector), 20)
            try:
                client.command("WebDriver:PerformActions", {"actions": [{
                    "type": "pointer", "id": "final-ua-ui-mouse", "parameters": {"pointerType": "mouse"},
                    "actions": [{"type": "pointerMove", "duration": 0, "origin": "viewport", **point},
                                {"type": "pointerDown", "button": button},
                                {"type": "pointerUp", "button": button}],
                }]})
            finally:
                client.command("WebDriver:ReleaseActions")

        def panel_state(identifier):
            return client.script(f"""
                const host={STATE}.main.document.getElementById('sidebar-panel-'+arguments[0]);
                const child=host?.browsingContext?.associatedWindow;
                const browser=child?.floorpWebPanelContentBrowser;
                if(!browser)return null;
                return {{id:browser.browserId, outerId:host.browserId,
                    mobile:!!child.floorpBmsUserAgent, connected:browser.isConnected,
                    uri:browser.currentURI?.spec, loading:!!browser.webProgress?.isLoadingDocument}};
            """, identifier)

        def ready_url(identifier, mobile, url):
            state = h.wait_for(f"panel {identifier} mobile={mobile}", lambda: (value
                if (value := panel_state(identifier)) and value["id"] > 0 and
                value["mobile"] == mobile and value["connected"] and not value["loading"] and
                value["uri"] == url else None), 35)
            require(state["id"] != state["outerId"], "Content browser identity equals its outer host")
            return state

        def ready(identifier, mobile, key):
            return ready_url(identifier, mobile, base + "?" + key)

        def navigate(identifier, key, expected):
            client.script(PANEL_NAVIGATION_SCRIPT, identifier, base + "?" + key)
            request(key, expected)
            state = ready(identifier, expected == MOBILE_UA, key)
            channel_id = client.script(f"return {STATE}.ids[arguments[0]];", base + "?" + key)
            require(channel_id == state["id"], f"HTTP channel did not identify the inner browser for {key}")
            report["checks"].append({"key": key, "browserId": state["id"], "httpBrowserId": channel_id})
            return state

        def redirect_panel(identifier, case_id, expected):
            case = server.register_redirect(case_id, expected)
            result = next(item for item in report["redirectChecks"] if item["caseId"] == case_id)
            result["status"] = "failed"
            result["runNonce"] = case.nonce
            client.script(PANEL_NAVIGATION_SCRIPT, identifier, case.url(0))
            h.wait_for(case_id, lambda: len(server.redirect_trace(case_id)) >= 3, 35)
            state = ready_url(identifier, expected == MOBILE_UA, case.url(2))
            trace = server.redirect_trace(case_id)
            validate_redirect_trace(case, trace)
            server.assert_healthy()
            ids = [client.script(f"return {STATE}.ids[arguments[0]];", case.url(hop))
                   for hop in range(3)]
            require(all(type(value) is int and value == state["id"] for value in ids),
                    "Redirect HTTP channels did not retain the inner panel identity")
            result.update({"status": "passed", "browserId": state["id"],
                           "httpBrowserIds": ids, "requests": trace})

        def reload_panel(identifier, key, expected):
            previous = len(server.requests(key))
            old = panel_state(identifier)
            client.script(f"""
                {STATE}.main.document.getElementById('sidebar-panel-'+arguments[0])
                    .browsingContext.associatedWindow.floorpWebPanelContentBrowser.reload();
            """, identifier)
            request(key, expected, previous)
            current = ready(identifier, expected == MOBILE_UA, key)
            require(current["id"] == old["id"], "Reload changed the content browser identity")
            report["checks"].append({"key": key + "-reload", "browserId": current["id"]})

        def menu_command(identifier, command):
            pointer(f'#{identifier}', button=2)
            h.wait_for("Web Panel context menu", lambda: client.script(
                "return window.document.getElementById('webpanel-context')?.state==='open';"), 15)
            before = client.script(f"return {STATE}.commands.length;")
            h.wait_for(f"Web Panel menu command {command}", lambda: client.script(f"""
                const popup=window.document.getElementById('webpanel-context');
                const item=window.document.getElementById(arguments[0]);
                if(!item||!popup.contains(item)||item.hidden||item.disabled)return false;
                item.addEventListener('command',event=>{{
                    {STATE}.commands.push({{id:event.target.id, trusted:event.isTrusted}});
                }},{{once:true}});
                return true;
            """, command), 15)
            if platform.system() == "Darwin":
                # Native AppKit menus have no DOM hit rectangle. Use the same
                # trusted native-popup API as the project's sidebar UI tests.
                client.script("""
                    const popup=window.document.getElementById('webpanel-context');
                    popup.activateItem(window.document.getElementById(arguments[0]));
                """, command)
                method = "XULPopupElement.activateItem on open native menu"
            else:
                pointer(f'#{command}')
                method = "WebDriver pointer click"
            observed = h.wait_for(f"trusted command {command}", lambda: client.script(f"""
                const events={STATE}.commands.slice(arguments[0]);
                return events.length?events:null;
            """, before), 15)
            require(observed == [{"id": command, "trusted": True}],
                    f"Unexpected Web Panel command events: {observed}")
            h.wait_for("Web Panel menu closure", lambda: client.script(
                "return window.document.getElementById('webpanel-context')?.state==='closed';"), 15)
            report["checks"].append({"key": "menu-command", "panel": identifier,
                                     "command": command, "method": method, "events": observed})

        pointer(f'#{panel_id}')
        request("initial-mobile", MOBILE_UA)
        initial = ready(panel_id, True, "initial-mobile")
        reload_normal("normal-while-mobile-on")
        mobile = navigate(panel_id, "mobile", MOBILE_UA)
        require(mobile["id"] == initial["id"], "Navigation changed the panel identity")
        reload_panel(panel_id, "mobile", MOBILE_UA)
        navigate(panel_id, "mobile-navigation", MOBILE_UA)
        pointer(f'#{other_id}')
        request("initial-other", desktop_ua)
        ready(other_id, False, "initial-other")
        navigate(other_id, "other-panel", desktop_ua)
        navigate(panel_id, "hidden-mobile", MOBILE_UA)
        redirect_panel(panel_id, REDIRECT_IDS[0], MOBILE_UA)
        reload_normal("normal-after-mobile-redirect")
        navigate(other_id, "other-after-mobile-redirect", desktop_ua)

        previous = len(server.requests("initial-mobile"))
        menu_command(panel_id, "changeUAWebpanelMenu")
        request("initial-mobile", desktop_ua, previous)
        disabled = ready(panel_id, False, "initial-mobile")
        require(disabled["id"] != initial["id"], "Mobile OFF did not retire the original browser")
        reload_normal("normal-while-mobile-off")
        navigate(panel_id, "disabled", desktop_ua)
        reload_panel(panel_id, "disabled", desktop_ua)
        redirect_panel(panel_id, REDIRECT_IDS[1], desktop_ua)
        reload_normal("normal-after-desktop-redirect")
        navigate(other_id, "other-after-desktop-redirect", desktop_ua)
        settings = client.script("return JSON.parse(Services.prefs.getStringPref('floorp.panelSidebar.data')).data;")
        require(next(panel for panel in settings if panel["id"] == panel_id)["userAgent"] is False,
                "Mobile OFF was not saved in real panel settings")

        previous = len(server.requests("initial-mobile"))
        menu_command(panel_id, "changeUAWebpanelMenu")
        request("initial-mobile", MOBILE_UA, previous)
        enabled = ready(panel_id, True, "initial-mobile")
        require(enabled["id"] != disabled["id"], "Mobile ON did not retire the desktop browser")
        settings = client.script("return JSON.parse(Services.prefs.getStringPref('floorp.panelSidebar.data')).data;")
        require(next(panel for panel in settings if panel["id"] == panel_id)["userAgent"] is True,
                "Mobile ON was not saved in real panel settings")
        reload_normal("normal-after-mobile-reenabled")
        navigate(panel_id, "reenabled", MOBILE_UA)
        menu_command(panel_id, "deleteWebpanelMenu")
        h.wait_for("panel removal", lambda: panel_state(panel_id) is None, 20)
        reload_normal("normal-after-panel-removal")
        navigate(other_id, "other-after-removal", desktop_ua)
        report["retiredBrowserIds"] = [initial["id"], disabled["id"], enabled["id"]]
        report["normalBrowserId"] = client.script(f"return {STATE}.normal.linkedBrowser.browserId;")
        require(report["normalBrowserId"] not in report["retiredBrowserIds"], "Normal tab reused a retired panel ID")
        settings = client.script("return JSON.parse(Services.prefs.getStringPref('floorp.panelSidebar.data')).data;")
        require(all(panel["id"] != panel_id for panel in settings), "Deleted panel remains in saved settings")
        require(host.poll() is None, "Browser exited during the HTTP regression")
        require(all(item["status"] == "passed" for item in report["redirectChecks"]),
                "Required redirect cases did not complete")
        server.assert_healthy()
        report["status"] = "passed"
    except Exception as error:
        LOGGER.exception("Final package Web Panel HTTP regression failed")
        report["error"] = str(error)
    finally:
        if client:
            try:
                client.context("chrome")
                screenshot = client.command("WebDriver:TakeScreenshot", {"full": False, "hash": False})
                data = screenshot.get("value") if isinstance(screenshot, dict) else screenshot
                (output / "browser-window.png").write_bytes(base64.b64decode(data, validate=True))
            except (OSError, RuntimeError, ValueError, TypeError) as error:
                report["screenshotError"] = str(error)
            try:
                client.script(f"if({STATE}?.observer)Services.obs.removeObserver({STATE}.observer,'http-on-modify-request');")
            except (OSError, RuntimeError) as error:
                report["cleanupError"] = str(error)
                report["status"] = "failed"
            try:
                client.close()
            except (OSError, RuntimeError) as error:
                report["sessionCleanupError"] = str(error)
                report["status"] = "failed"
        server.shutdown()
        server.server_close()
        report.update(server.snapshot())
        if report["httpFixtureError"] is not None:
            report["status"] = "failed"
        try:
            server.validate_registered_requests()
        except RuntimeError as error:
            report["redirectFinalizationError"] = str(error)
            report["status"] = "failed"
        if host:
            if host.poll() is not None:
                report["unexpectedHostExit"] = host.returncode
                report["status"] = "failed"
            try:
                host.close()
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                report["hostCleanupError"] = str(error)
                report["status"] = "failed"
            report["hostExitCode"] = host.returncode
            report["cleanupRequestedSignal"] = host.requested_signal
            report["ownedHostPid"] = host.pid
            if host.returncode not in (0, -int(host.requested_signal or 0)):
                report["hostCleanupError"] = f"Missing or abnormal browser exit: {host.returncode}"
                report["status"] = "failed"
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{report['status']}: {output / 'report.json'}", flush=True)
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
