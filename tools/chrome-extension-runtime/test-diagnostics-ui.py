#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Verify Chrome extension diagnostics in the real about:addons details page."""

import argparse
import base64
import importlib.util
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
from urllib.parse import unquote, urlparse
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("runtime", ROOT / "tools/app-shim/test-runtime.py")
    runtime = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runtime)
    output = Path(tempfile.mkdtemp(prefix="diagnostics-ui-", dir=ROOT / "_dist"))
    chrome_id = "c" * 32
    addon_id = chrome_id + "@chromium-extension"
    manifest = {
        "manifest_version": 3, "name": "Chrome Diagnostics Fixture", "version": "2.4.1",
        "permissions": ["storage"], "background": {"scripts": ["background.js"]},
        "browser_specific_settings": {"gecko": {
            "id": addon_id, "floorp_chrome_extension": {"schema_version": 1, "original_id": chrome_id}}},
    }
    fixture = output / "fixture.xpi"
    warning = "Service worker converted to an event page for this runtime."
    conversion = {"schemaVersion": 1, "chromeId": chrome_id, "addonId": addon_id,
                  "originalVersion": "2.4.1", "warnings": [warning], "removedPermissions": ["tabCapture"],
                  "signature": {"format": 3, "algorithm": "rsa-sha256", "publicKeySha256": "f" * 64, "unknown": "not exported"}}
    with zipfile.ZipFile(fixture, "w") as archive:
        archive.writestr("manifest.json", json.dumps(manifest))
        archive.writestr("__floorp_chrome__/conversion-report.json", json.dumps(conversion))
        archive.writestr("background.js", "document.title=crypto.randomUUID();browser.runtime.onMessage.addListener(()=>Promise.resolve('ready')); ")
    ordinary_id = "ordinary-diagnostics@example.com"
    ordinary = output / "ordinary.xpi"
    with zipfile.ZipFile(ordinary, "w") as archive:
        archive.writestr("manifest.json", json.dumps({"manifest_version": 3, "name": "Ordinary Firefox Fixture", "version": "1.0", "browser_specific_settings": {"gecko": {"id": ordinary_id}}}))
    missing_report_id = "e" * 32 + "@chromium-extension"
    missing_report = output / "missing-report.xpi"
    with zipfile.ZipFile(missing_report, "w") as archive:
        archive.writestr("manifest.json", json.dumps({
            "manifest_version": 3, "name": "Missing conversion report fixture", "version": "1.0",
            "browser_specific_settings": {"gecko": {"id": missing_report_id,
                "floorp_chrome_extension": {"schema_version": 1, "original_id": "e" * 32}}},
        }))
    legacy_id = "d" * 32 + "@chromium-extension"
    legacy_fixture = output / "legacy.xpi"
    with zipfile.ZipFile(legacy_fixture, "w") as archive:
        archive.writestr("manifest.json", json.dumps({
            "manifest_version": 3, "name": "Legacy conversion fixture", "version": "1.0",
            "permissions": ["sidePanel"], "background": {"scripts": ["background.js"]},
            "browser_specific_settings": {"gecko": {"id": legacy_id}},
        }))
        archive.writestr("background.js", "document.title='legacy-api:'+typeof browser.sidePanel;")
    profile = output / "profile"
    profile.mkdir()
    port = runtime.unused_port()
    prefs = {"marionette.port": port, "browser.shell.checkDefaultBrowser": False,
             "browser.startup.page": 0, "browser.aboutwelcome.enabled": False,
             "browser.startup.homepage_override.mstone": "ignore",
             "datareporting.policy.dataSubmissionEnabled": False,
             "floorp.releaseNotes.mode": "disabled", "floorp.releaseNotes.choiceConfirmed": True,
             "extensions.htmlaboutaddons.recommendations.enabled": False,
             "extensions.getAddons.showPane": False}
    (profile / "user.js").write_text("\n".join(f"user_pref({json.dumps(k)}, {json.dumps(v)});" for k, v in prefs.items()) + "\n")
    browser = args.browser.resolve()
    if browser.is_dir():
        browser = browser / "floorp"
    report = {"passed": False, "checks": []}
    client = host = None

    def check(condition, description):
        if not condition:
            raise AssertionError(description)
        report["checks"].append(description)

    with (output / "browser.log").open("w") as log:
        try:
            host = subprocess.Popen([str(browser), "--no-remote", "--profile", str(profile), "--marionette", "--remote-allow-system-access", "--headless", "about:blank"], stdout=log, stderr=subprocess.STDOUT, env=os.environ)

            def connect():
                if host.poll() is not None:
                    raise RuntimeError(f"Browser exited {host.returncode}")
                try:
                    return runtime.Marionette(port)
                except (ConnectionRefusedError, socket.timeout):
                    return None

            client = runtime.wait_for("Marionette", connect, 45)
            client.context("chrome")
            resource_base = client.script("""
                return Services.io.getProtocolHandler('resource').QueryInterface(Ci.nsIResProtocolHandler)
                    .resolveURI(Services.io.newURI('resource://noraneko/'));
            """)
            if not resource_base.startswith("file:"):
                raise RuntimeError("This UI fixture requires the unpacked Floorp resource tree")
            original_resources = Path(unquote(urlparse(resource_base).path))
            overlay = output / "noraneko-resources"

            def mirror(source, target, parts):
                target.mkdir(parents=True)
                for child in source.iterdir():
                    destination = target / child.name
                    if child.name == parts[0]:
                        if len(parts) > 1:
                            mirror(child, destination, parts[1:])
                    else:
                        destination.symlink_to(child.resolve(), target_is_directory=child.is_dir())

            mirror(original_resources, overlay, ["modules", "chrome-web-store", "ChromeExtensionUpdater.sys.mjs"])
            (overlay / "modules/chrome-web-store/ChromeExtensionUpdater.sys.mjs").write_text(r"""
export const ChromeExtensionUpdater = {
    fixture: true, scenario: 'available', calls: [],
    async checkForUpdate(addonId, options={}) {
        this.calls.push({method:'check',addonId,reconvert:!!options.reconvert});
        if (this.scenario==='error') throw new Error('fixture network error');
        if (this.scenario==='up-to-date') return {addonId,currentVersion:'2.4.1',status:'up-to-date',warnings:[]};
        return {addonId,currentVersion:'2.4.1',availableVersion:options.reconvert?'2.4.1':'2.5.0',
            status:'available',kind:options.reconvert?'reconvert':'update',warnings:['Prepared conversion warning']};
    },
    async installUpdate(addonId, browser) {
        this.calls.push({method:'install',addonId,browser:{tag:browser?.localName,uri:browser?.currentURI?.spec}});
        if (this.scenario==='expired') throw new Error('prepared package expired');
        return {status:this.scenario==='postponed'?'postponed':'installed',version:'2.5.0'};
    }
};
""")
            scheduler_path=overlay / "modules/chrome-web-store/ChromeExtensionUpdateScheduler.sys.mjs"
            scheduler_path.unlink(missing_ok=True)
            scheduler_path.write_text(r"""
export const ChromeExtensionUpdateScheduler={
    fixture:true,enabled:false,policyAllowed:true,state:'idle',eligibility:'verified',calls:[],refuse:false,
    async getStatus(addonId) {
        return {addonId,enabled:this.enabled,policyAllowed:this.policyAllowed,state:this.state,
            eligibility:addonId.startsWith('dddd')?'migration-required':addonId.startsWith('eeee')?'unavailable':this.eligibility};
    },
    async setAutomaticUpdates(addonId,enabled) {
        this.calls.push({addonId,enabled});
        if(this.refuse) throw new Error('fixture automatic update refusal');
        this.enabled=enabled;
        return this.getStatus(addonId);
    }
};
""")
            client.script("""
                const file=Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);file.initWithPath(arguments[0]);
                Services.io.getProtocolHandler('resource').QueryInterface(Ci.nsIResProtocolHandler)
                    .setSubstitution('noraneko',Services.io.newFileURI(file));
                const {ChromeExtensionUpdater}=ChromeUtils.importESModule('resource://noraneko/modules/chrome-web-store/ChromeExtensionUpdater.sys.mjs');
                if (!ChromeExtensionUpdater.fixture) throw new Error('Updater fixture was not loaded');
            """, str(overlay))
            client.script("""
                window.__diagnosticInstalls={done:false};
                const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');
                Promise.all(arguments[0].map(path=>{
                    const file=Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);file.initWithPath(path);
                    return AddonManager.installTemporaryAddon(file);
                })).then(addons=>window.__diagnosticInstalls={done:true,ids:addons.map(addon=>addon.id)},
                         error=>window.__diagnosticInstalls={done:true,error:String(error)});
            """, [str(fixture), str(ordinary), str(missing_report), str(legacy_fixture)])
            installed = runtime.wait_for("fixtures", lambda: client.script("return window.__diagnosticInstalls.done ? window.__diagnosticInstalls : null;"), 20)
            check(installed.get("ids") == [addon_id, ordinary_id, missing_report_id, legacy_id], f"Temporary extension fixtures install: {installed}")

            def background_instance():
                client.context("chrome")
                value = client.script("""
                    const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                    return ExtensionParent.GlobalManager.getExtension(arguments[0])?.backgroundContext?.xulBrowser?.contentTitle || null;
                """, addon_id)
                client.context("content")
                return value

            initial_instance = runtime.wait_for("background", background_instance, 15)
            client.navigate("about:addons")
            runtime.wait_for("about:addons initialized", lambda: client.script("return !!window.wrappedJSObject.gViewController;"), 20)

            def details(extension_id):
                client.script("""
                    const page=window.wrappedJSObject;page.__diagnosticsReady=false;
                    Promise.resolve(page.promiseInitialized).then(()=>page.gViewController.loadView('detail/'+arguments[0])).then(()=>page.__diagnosticsReady=true);
                """, extension_id)
                runtime.wait_for("details view", lambda: client.script("return window.wrappedJSObject.__diagnosticsReady;"), 15)
                runtime.wait_for("diagnostics settled", lambda: client.script("""
                    const element=window.wrappedJSObject.document.querySelector('floorp-chrome-extension-diagnostics');
                    return element && element.diagnostics !== undefined && element.getAttribute('aria-busy') !== 'true';
                """), 15)

            details(addon_id)
            state = client.script("""
                const element=window.wrappedJSObject.document.querySelector('floorp-chrome-extension-diagnostics');
                return {url:location.href,title:document.title,hidden:element.hidden,text:element.textContent,diagnostics:element.diagnostics};
            """)
            check(state["url"].startswith("about:addons") and bool(state["title"]), "Page identity is about:addons with a title")
            check(not state["hidden"] and chrome_id in state["text"] and "2.4.1" in state["text"], "Chrome provenance and source version render in the existing details card")
            check(warning in state["text"] and "tabCapture" in state["text"], "Recorded conversion warnings and removed permissions render")
            check(any(context["contextType"] == "BACKGROUND" for context in state["diagnostics"]["contexts"]), "Running contexts come from the actual extension runtime")
            client.script("document.querySelector('.floorp-chrome-diagnostics-contexts').open=true;")
            screenshot = client.command("WebDriver:TakeScreenshot", {"full": True})
            (output / "diagnostics-en.png").write_bytes(base64.b64decode(screenshot.get("value", screenshot) if isinstance(screenshot, dict) else screenshot))

            def click(action):
                client.script("document.querySelector('[data-chrome-action='+arguments[0]+']').click();", action)
                runtime.wait_for(action, lambda: client.script("return document.querySelector('floorp-chrome-extension-diagnostics').getAttribute('aria-busy') !== 'true';"), 20)

            def scheduler_state(**changes):
                client.context("chrome")
                result=client.script("""
                    const {ChromeExtensionUpdateScheduler:scheduler}=ChromeUtils.importESModule('resource://noraneko/modules/chrome-web-store/ChromeExtensionUpdateScheduler.sys.mjs');
                    Object.assign(scheduler,arguments[0]);
                    Services.obs.notifyObservers(null,'floorp-chrome-extension-update-status-changed',arguments[1]);
                    return scheduler.calls;
                """,changes,addon_id)
                client.context("content")
                return result
            automatic='[data-chrome-action=automatic-updates]'
            check(client.script("return !document.querySelector(arguments[0]).disabled && !document.querySelector(arguments[0]).checked;",automatic), "Verified scheduler eligibility exposes an unchecked automatic-update control")
            click("automatic-updates")
            check(client.script("return document.querySelector(arguments[0]).checked;",automatic) and scheduler_state()[-1]=={"addonId":addon_id,"enabled":True}, "Explicit checkbox clicks persist automatic-update preference through the scheduler")
            scheduler_state(policyAllowed=False)
            runtime.wait_for("automatic update paused",lambda:client.script("return document.querySelector('.floorp-chrome-diagnostics-automatic-status').textContent.includes('paused');"),10)
            check(client.script("return document.querySelector(arguments[0]).checked;",automatic), "Browser policy pauses updates without losing the user's enabled preference")
            scheduler_state(policyAllowed=True,state="waiting-for-window")
            runtime.wait_for("automatic waiting state",lambda:client.script("return document.querySelector('.floorp-chrome-diagnostics-automatic-status').textContent.includes('waiting');"),10)
            check(True,"Scheduler observer updates the existing details view with a truthful waiting-for-window state")
            scheduler_state(refuse=True)
            click("automatic-updates")
            check(client.script("return document.querySelector(arguments[0]).checked && document.querySelector('.floorp-chrome-diagnostics-automatic-status').textContent.includes('fixture automatic update refusal');",automatic), "A rejected automatic-update change restores the authoritative checkbox state and reports the error")
            scheduler_state(refuse=False,state="idle")
            click("automatic-updates")
            check(not client.script("return document.querySelector(arguments[0]).checked;",automatic), "A second explicit checkbox click disables automatic updates")
            scheduler_state(eligibility="migration-required")
            runtime.wait_for("marked migration required",lambda:client.script("return document.querySelector('[data-chrome-action=automatic-updates]').disabled;"),10)
            check(client.script("return !document.querySelector('.floorp-chrome-diagnostics-automatic-migration').hidden;"), "A compatibility marker cannot override the scheduler's explicit migration requirement")
            scheduler_state(eligibility="verified")
            click("refresh")
            check("refreshed" in client.script("return document.querySelector('.floorp-chrome-diagnostics-status').textContent;").lower(), "Refresh button updates diagnostics and reports completion")
            export_path = output / "exported-diagnostics.json"
            client.script("""
                const element=window.wrappedJSObject.document.querySelector('floorp-chrome-extension-diagnostics');
                element.chooseExportFile=async()=>({path:arguments[0]});
            """, str(export_path))
            click("export")
            check(export_path.is_file(), "Save diagnostics button writes a JSON file after choosing the destination")
            exported = json.loads(export_path.read_text())
            check(exported["extension"]["chromeId"] == chrome_id and exported["conversion"]["warnings"] == [warning], "Saved diagnostics contain real provenance and conversion results")
            def update_service(scenario=None):
                client.context("chrome")
                value = client.script("""
                    const {ChromeExtensionUpdater}=ChromeUtils.importESModule('resource://noraneko/modules/chrome-web-store/ChromeExtensionUpdater.sys.mjs');
                    if (arguments[0]) ChromeExtensionUpdater.scenario=arguments[0];
                    return ChromeExtensionUpdater.calls;
                """, scenario)
                client.context("content")
                return value

            def apply_visible():
                return client.script("return !document.querySelector('[data-chrome-action=apply-update]').hidden;")

            check(exported["conversion"].get("signature") == {"format": 3, "algorithm": "rsa-sha256", "publicKeySha256": "f" * 64}, "Saved diagnostics preserve only recognized recorded CRX signature fields")
            click("check-update")
            calls = update_service()
            check(len(calls) == 1 and calls[0]["method"] == "check" and not calls[0]["reconvert"] and apply_visible(), "Checking prepares a candidate and does not install it")
            candidate_text = client.script("return document.querySelector('.floorp-chrome-diagnostics-update-status').textContent;")
            check("2.5.0" in candidate_text and client.script("return document.querySelector('.floorp-chrome-diagnostics-update-warnings').textContent.includes('Prepared conversion warning');"), "Prepared candidate version and warnings render before applying")
            screenshot = client.command("WebDriver:TakeScreenshot", {"full": True})
            (output / "diagnostics-update-ready.png").write_bytes(base64.b64decode(screenshot.get("value", screenshot) if isinstance(screenshot, dict) else screenshot))
            click("apply-update")
            calls = update_service()
            check(calls[-1]["method"] == "install" and calls[-1]["browser"]["tag"] == "browser" and calls[-1]["browser"]["uri"].startswith("about:addons"), "Explicit Apply passes the actual about:addons browser embedder to the update service")
            check(not apply_visible(), "Applying clears the prepared candidate control")
            before = len(calls)
            click("reapply")
            calls = update_service()
            check(len(calls) == before + 1 and calls[-1]["method"] == "check" and calls[-1]["reconvert"] and apply_visible(), "Reapply only prepares a reconversion and requires a separate Apply click")
            click("apply-update")
            update_service("up-to-date")
            click("check-update")
            check(not apply_visible() and "up to date" in client.script("return document.querySelector('.floorp-chrome-diagnostics-update-status').textContent;").lower(), "Up-to-date results never expose an Apply operation")
            update_service("error")
            click("check-update")
            check(not apply_visible() and "fixture network error" in client.script("return document.querySelector('.floorp-chrome-diagnostics-update-status').textContent;"), "Check failures are visible and clear any stale candidate")
            update_service("available")
            click("check-update")
            click("refresh")
            check(not apply_visible() and not client.script("return document.querySelector('.floorp-chrome-diagnostics-update-status').textContent;"), "Refreshing diagnostics invalidates the prepared candidate and clears its summary")
            click("check-update")
            update_service("expired")
            click("apply-update")
            check(not apply_visible() and "prepared package expired" in client.script("return document.querySelector('.floorp-chrome-diagnostics-update-status').textContent;"), "Expired packages report an error and cannot be reapplied through stale controls")
            update_service("postponed")
            click("check-update")
            click("apply-update")
            check("postponed" in client.script("return document.querySelector('.floorp-chrome-diagnostics-update-status').textContent;").lower(), "Postponed installation is reported without claiming installation completed")
            click("reload")
            next_instance = runtime.wait_for("reloaded background", lambda: (value if (value := background_instance()) != initial_instance else None), 20)
            check(bool(next_instance) and next_instance != initial_instance, "Reload button restarts the actual extension background")
            details(addon_id)

            japanese = json.loads((ROOT / "i18n/ja-JP/browser-chrome.json").read_text())["addons"]["chromeDiagnostics"]
            client.context("chrome")
            client.script("""
                const {I18nUtils}=ChromeUtils.importESModule('resource://noraneko/modules/i18n/I18n-Utils.sys.mjs');
                const strings=arguments[0];I18nUtils.registerTranslationProvider({t:key=>strings[key.split('.').pop()] || key});
            """, japanese)
            client.context("content")
            click("refresh")
            japanese_text = client.script("return document.querySelector('floorp-chrome-extension-diagnostics').textContent;")
            check(japanese["heading"] in japanese_text and japanese["reload"] in japanese_text and japanese["automaticUpdates"] in japanese_text, "Japanese catalog strings render for headings and controls")
            screenshot = client.command("WebDriver:TakeScreenshot", {"full": True})
            (output / "diagnostics-ja.png").write_bytes(base64.b64decode(screenshot.get("value", screenshot) if isinstance(screenshot, dict) else screenshot))
            client.context("chrome")
            client.script("""
                window.__disabledFixture=false;
                const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');
                AddonManager.getAddonByID(arguments[0]).then(addon=>addon.disable()).then(()=>window.__disabledFixture=true);
            """, addon_id)
            runtime.wait_for("disable fixture", lambda: client.script("return window.__disabledFixture;"), 10)
            client.context("content")
            details(addon_id)
            check(client.script("return document.querySelector('[data-chrome-action=reload]').disabled;"), "Reload is disabled for inactive extensions and cannot enable them accidentally")
            check(client.script("return document.querySelector('floorp-chrome-extension-diagnostics').diagnostics.contexts.length === 0;"), "Disabled extensions report no running contexts")
            details(missing_report_id)
            check(client.script("""
                const element=document.querySelector('floorp-chrome-extension-diagnostics');
                return !element.hidden && !!element.diagnostics.conversionError && element.diagnostics.conversion === null;
            """), "A missing conversion report is shown as unavailable instead of claiming compatibility")
            details(legacy_id)
            legacy_state = client.script("""
                const element=document.querySelector('floorp-chrome-extension-diagnostics');
                return {hidden:element.hidden,legacyHidden:element.querySelector('.floorp-chrome-diagnostics-legacy').hidden,
                    provenance:element.diagnostics?.extension.provenance,schema:element.diagnostics?.extension.runtimeSchemaVersion};
            """)
            check(not legacy_state["hidden"] and not legacy_state["legacyHidden"] and legacy_state["provenance"] == "legacy-unverified" and legacy_state["schema"] is None,
                  "Legacy conversions display an unverified-source notice without claiming runtime support")
            check(client.script("return document.querySelector('[data-chrome-action=automatic-updates]').disabled && !document.querySelector('.floorp-chrome-diagnostics-automatic-migration').hidden;"), "Legacy installations require explicit verified Reapply and cannot enable automatic updates")
            client.context("chrome")
            legacy_api = client.script("""
                const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                return ExtensionParent.GlobalManager.getExtension(arguments[0])?.backgroundContext?.xulBrowser?.contentTitle;
            """, legacy_id)
            check(legacy_api == "legacy-api:undefined", "Displaying legacy diagnostics does not grant Chrome compatibility APIs")
            client.context("content")
            details(ordinary_id)
            check(client.script("return document.querySelector('floorp-chrome-extension-diagnostics').hidden;"), "Ordinary Firefox extension cards keep diagnostics hidden")
            client.context("chrome")
            errors = client.script("return Services.console.getMessageArray().map(message=>message.message).filter(message=>/ChromeExtensionDiagnostics|floorp-chrome-extension-diagnostics/.test(message));")
            expected_errors = [message for message in errors if "fixture network error" in message or "prepared package expired" in message or "fixture automatic update refusal" in message]
            unexpected_errors = [message for message in errors if message not in expected_errors]
            report["expectedConsoleErrors"] = expected_errors
            report["console"] = unexpected_errors
            check(not unexpected_errors, "No unexpected diagnostics-related runtime errors reach the browser console")
            report["passed"] = True
        except Exception as error:
            report["error"] = str(error)
            if client:
                try:
                    client.context("chrome")
                    report["console"] = client.script("return Services.console.getMessageArray().map(message=>message.message).filter(message=>/ChromeExtensionDiagnostics|Error|aboutaddons/.test(message)).slice(-30);")
                except Exception:
                    pass
        finally:
            if client:
                client.close()
            if host and host.poll() is None:
                host.terminate()
                try:
                    host.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    host.kill()
                    host.wait(timeout=5)
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    print(f"Report: {output / 'report.json'}")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
