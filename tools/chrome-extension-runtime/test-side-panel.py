#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Exercise sidePanel child APIs and native sidebar effects in a private profile."""

import argparse
import importlib.util
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", type=Path, required=True)
    parser.add_argument("--floorp-ui", action="store_true", help="Build and exercise existing Floorp pin and keyboard controls")
    args = parser.parse_args()
    if args.floorp_ui:
        subprocess.run(["deno", "run", "-A", "npm:vite", "build", "--config",
                        "tools/chrome-extension-runtime/vite-side-panel-test.mts"], check=True, cwd=ROOT)
    spec = importlib.util.spec_from_file_location("runtime", ROOT / "tools/app-shim/test-runtime.py")
    runtime = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runtime)
    output = Path(tempfile.mkdtemp(prefix="side-panel-", dir=ROOT / "_dist"))
    original_id = "a" * 32
    addon_id = original_id + "@chromium-extension"
    manifest = {
        "manifest_version": 3, "name": "Floorp Side Panel Fixture", "version": "1.0",
        "permissions": ["sidePanel", "tabs", "activeTab", "tabCapture", "storage"],
        "background": {"scripts": ["background.js"]}, "action": {},
        "side_panel": {"default_path": "global.html"},
        "sidebar_action": {"default_panel": "ignored.html", "open_at_install": False},
        "browser_specific_settings": {"gecko": {
            "id": addon_id, "floorp_chrome_extension": {
                "schema_version": 1, "original_id": original_id}}},
    }
    background = r"""
const report = async data => {document.title=JSON.stringify(data);};
let tab, inactive, stage=0;
const panelEvents=[];
browser.sidePanel.onOpened.addListener(info=>panelEvents.push({event:'opened',info}));
browser.sidePanel.onClosed.addListener(info=>panelEvents.push({event:'closed',info}));
window.addEventListener('message',event=>{
    if(event.data==='floorp-sidepanel-events') report({type:'panel-events',events:panelEvents,time:Date.now()});
});
const check = (condition, message) => {if (!condition) throw new Error(message);};
async function rejected(fn, pattern) {
    try { await fn(); } catch(error) { check(pattern.test(String(error)), String(error)); return; }
    throw new Error('Expected rejection '+pattern);
}
browser.action.onClicked.addListener(async () => {
    try {
        stage++;
        if (stage===1) await browser.sidePanel.open({windowId:tab.windowId});
        if (stage===2) await browser.sidePanel.open({tabId:tab.id});
        if (stage===3) await browser.sidePanel.setOptions({tabId:tab.id,enabled:false});
        if (stage===4) {
            await browser.sidePanel.setOptions({tabId:tab.id,enabled:true});
            await browser.sidePanel.setPanelBehavior({openPanelOnActionClick:true});
            check((await browser.sidePanel.getPanelBehavior()).openPanelOnActionClick, 'behavior');
        }
        if (stage===5) throw new Error('Action should open panel without onClicked');
        await report({type:'stage',stage});
    } catch(error) {await report({type:'error',error:String(error),stack:error.stack});}
});
(async () => {
    try {
        const {initialized}=await browser.storage.session.get('initialized');
        if(initialized){await report({type:'restarted'});return;}
        await browser.storage.session.set({initialized:true});
        [tab]=await browser.tabs.query({active:true,currentWindow:true});
        inactive=await browser.tabs.create({active:false});
        const fallback=await browser.sidePanel.getOptions({tabId:inactive.id});
        check(fallback.path==='global.html' && !Object.hasOwn(fallback,'tabId'),'default fallback has no tabId');
        await browser.sidePanel.setOptions({tabId:inactive.id,enabled:false});
        const specific=await browser.sidePanel.getOptions({tabId:inactive.id});
        check(!Object.hasOwn(specific,'path') && specific.tabId===inactive.id,'tab-specific options do not inherit a global path');
        await browser.sidePanel.setOptions({tabId:tab.id,path:'tab.html'});
        check((await browser.sidePanel.getOptions({})).path==='global.html','global path');
        check((await browser.sidePanel.getOptions({tabId:tab.id})).path==='tab.html','tab path');
        await rejected(()=>browser.sidePanel.setOptions({path:'https://example.com/',enabled:false}),/relative|extension|format/i);
        check((await browser.sidePanel.getOptions({})).enabled,'invalid path must not mutate');
        await rejected(()=>browser.sidePanel.open({windowId:tab.windowId}),/user input|user gesture/i);
        await rejected(()=>browser.tabCapture.getMediaStreamId({targetTabId:tab.id}),/user input|user gesture|real extension action invocation/);
        await report({type:'ready',tab,inactive});
    } catch(error) {await report({type:'error',error:String(error),stack:error.stack});}
})();
"""
    fixture = output / "fixture.xpi"
    with zipfile.ZipFile(fixture, "w") as archive:
        archive.writestr("manifest.json", json.dumps(manifest))
        archive.writestr("background.js", background)
        archive.writestr("ignored.html", "<!doctype html><title>Ignored cross-browser sidebar</title>")
        archive.writestr("global.html", '<!doctype html><script src="panel.js"></script>')
        archive.writestr("tab.html", '<!doctype html><script src="panel.js"></script>')
        archive.writestr("panel.js", """
(async()=>{
    const data={type:'panel',path:location.pathname,instance:crypto.randomUUID()};
    const [tab]=await browser.tabs.query({active:true,currentWindow:true});
    if(document.readyState==='loading') await new Promise(resolve=>document.addEventListener('DOMContentLoaded',resolve,{once:true}));
    const button=document.createElement('button');button.id='capture';button.textContent='Capture tab';document.body.appendChild(button);
    button.addEventListener('click',()=>{
        browser.tabCapture.getMediaStreamId({targetTabId:tab.id}).then(
            token=>document.title=JSON.stringify({...data,type:'capture-token',token}),
            error=>document.title=JSON.stringify({...data,type:'capture-token',error:String(error)}));
    });
    document.title=JSON.stringify(data);
})();
""")
    ordinary_fixture = output / "ordinary.xpi"
    ordinary_id = "side-panel-firefox-fixture@example.com"
    with zipfile.ZipFile(ordinary_fixture, "w") as archive:
        archive.writestr("manifest.json", json.dumps({
            "manifest_version": 3, "name": "Ordinary Firefox fixture", "version": "1.0",
            "permissions": ["sidePanel"], "background": {"scripts": ["background.js"]},
            "sidebar_action": {"default_panel": "ordinary.html", "default_title": "Firefox Sidebar", "open_at_install": False},
            "browser_specific_settings": {"gecko": {"id": ordinary_id}},
        }))
        archive.writestr("background.js", "document.title='ordinary:'+typeof browser.sidePanel;")
        archive.writestr("ordinary.html", "<!doctype html><title>Firefox sidebar</title>Firefox sidebar")
    profile = output / "profile"
    profile.mkdir()
    port = runtime.unused_port()
    prefs = {"marionette.port": port, "browser.shell.checkDefaultBrowser": False,
             "browser.startup.page": 0, "browser.aboutwelcome.enabled": False,
             "browser.startup.homepage_override.mstone": "ignore",
             "datareporting.policy.dataSubmissionEnabled": False,
             "extensions.autoDisableScopes": 0,
             "floorp.releaseNotes.mode": "disabled",
             "floorp.releaseNotes.choiceConfirmed": True}
    (profile / "user.js").write_text("\n".join(
        f"user_pref({json.dumps(k)}, {json.dumps(v)});" for k, v in prefs.items())+"\n")
    browser = args.browser.resolve()
    if browser.is_dir():
        browser = browser / "floorp"
    client = host = None
    report = {"passed": False, "checks": [], "events": []}

    seen = set()

    def receive(kind, **expected):
        def read():
            data = client.script("""
                const {ExtensionParent} = ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                const extension = ExtensionParent.GlobalManager.getExtension(arguments[0]);
                const titles = ["panel", "capture-token"].includes(arguments[1])
                    ? [window.SidebarController.browser.contentDocument?.getElementById("webext-panels-browser")?.contentTitle]
                    : [...extension.views].filter(view=>view.viewType === "background" && !view.unloaded).map(view=>view.xulBrowser?.contentTitle);
                return titles.filter(Boolean).map(title=>{try{return JSON.parse(title);}catch{return null;}}).filter(Boolean);
            """, addon_id, kind)
            for value in data:
                token = json.dumps(value, sort_keys=True)
                if value.get("type") == "error":
                    raise RuntimeError(value)
                matches_panel = kind == "panel" and value.get("type") in ["panel", "capture-token"]
                if (matches_panel or value.get("type") == kind) and (matches_panel or token not in seen):
                    if any(value.get(k) != v for k,v in expected.items()):
                        continue
                    seen.add(token)
                    return value
            return None
        message = runtime.wait_for(f"extension {kind} {expected}", read, 20)
        report["events"].append(message)
        if any(message.get(k) != v for k, v in expected.items()):
            raise AssertionError(f"Expected {kind} {expected}; got {message}")
        return message

    def check(condition, description):
        if not condition:
            raise AssertionError(description)
        report["checks"].append(description)

    with (output / "browser.log").open("w") as log:
        try:
            host = subprocess.Popen([str(browser), "--no-remote", "--profile", str(profile),
                                     "--marionette", "--remote-allow-system-access", "--headless", "about:blank"],
                                    stdout=log, stderr=subprocess.STDOUT, env=os.environ)

            def connect():
                if host.poll() is not None:
                    raise RuntimeError(f"Browser exited {host.returncode}")
                try:
                    return runtime.Marionette(port)
                except (ConnectionRefusedError, socket.timeout):
                    return None

            client = runtime.wait_for("Marionette", connect, 45)
            client.context("chrome")
            client.script("""
                window.__fixtureInstall = {done:false};
                const {AddonManager} = ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');
                const file = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);
                file.initWithPath(arguments[0]);
                AddonManager.installTemporaryAddon(file).then(
                    addon=>{window.__fixtureInstall={done:true,id:addon.id};},
                    error=>{window.__fixtureInstall={done:true,error:String(error)};});
            """, str(fixture))
            install = runtime.wait_for("fixture install", lambda: client.script(
                "return window.__fixtureInstall.done ? window.__fixtureInstall : null;"), 30)
            check(install.get("id") == addon_id, f"Fixture installs: {install}")
            ready = receive("ready")
            check(not client.script("return window.SidebarController.isOpen;"), "No panel opens on installation")

            def click():
                client.script("""
                    const {ExtensionParent} = ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                    const extension = ExtensionParent.GlobalManager.getExtension(arguments[0]);
                    window.gBrowser.selectedTab = extension.apiManager.global.tabTracker.getTab(arguments[1]);
                    extension.apiManager.getAPI('browserAction',extension,'addon_parent').triggerAction(window);
                """, addon_id, ready["tab"]["id"])

            for stage, path in [(1, "/global.html"), (2, "/tab.html")]:
                click()
                receive("stage", stage=stage)
                receive("panel", path=path)
                check(client.script("return window.SidebarController.isOpen;"), f"Stage {stage} opens a real sidebar for {path}")
            click()
            receive("stage", stage=3)
            check(not client.script("return window.SidebarController.isOpen;"), "Disabling a tab panel closes its sidebar")
            click()
            receive("stage", stage=4)
            receive("panel", path="/tab.html")
            check(client.script("return window.SidebarController.isOpen;"), "Enabling the tab restores open intent")
            click()
            runtime.wait_for("action toggles closed", lambda: not client.script("return window.SidebarController.isOpen;"), 10)
            check(True, "Second action click toggles the panel closed")
            preload_grant = client.script("""
                const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                const extension=ExtensionParent.GlobalManager.getExtension(arguments[0]);
                delete extension.floorpTabCaptureGrant;
                const action=extension.apiManager.getAPI('browserAction',extension,'addon_parent').action;
                action.setActiveTabForPreload(window.gBrowser.selectedTab);
                return extension.floorpTabCaptureGrant || null;
            """, addon_id)
            check(preload_grant is None, "Popup preloading does not grant permission to capture the tab")
            click()
            receive("panel", path="/tab.html")
            client.script("""
                const browser=window.SidebarController.browser.contentDocument.getElementById('webext-panels-browser');
                browser.messageManager.loadFrameScript('data:application/javascript,'+encodeURIComponent(`
                    const input=content.windowUtils.setHandlingUserInput(true);
                    try {content.document.getElementById('capture').click();}
                    finally {input.destruct();}
                `),false);
            """)
            action_capture = receive("capture-token", path="/tab.html")
            check(str(action_capture.get("token", "")).startswith("floorp-tab-capture-"),
                  f"A real Action click opening the side panel authorizes its capture button: {action_capture}")
            check(client.script("return window.SidebarController.isOpen;"), "Action behavior opens the tab panel")

            def api_call(method, options):
                client.script("""
                    window.__panelCall = {done:false};
                    const {ExtensionParent} = ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                    const extension = ExtensionParent.GlobalManager.getExtension(arguments[0]);
                    const api = extension.apiManager.getAPI('sidePanel',extension,'addon_parent').getAPI(extension.backgroundContext).sidePanel;
                    Promise.resolve(api[arguments[1]](arguments[2])).then(
                        value=>window.__panelCall={done:true,value},
                        error=>window.__panelCall={done:true,error:String(error)});
                """, addon_id, method, options)
                result = runtime.wait_for(method, lambda: client.script("return window.__panelCall.done ? window.__panelCall : null;"), 10)
                if result.get("error"):
                    raise AssertionError(result)
                return result.get("value")

            def select(tab_id):
                client.script("""
                    const {ExtensionParent} = ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                    const extension = ExtensionParent.GlobalManager.getExtension(arguments[0]);
                    window.gBrowser.selectedTab = extension.apiManager.global.tabTracker.getTab(arguments[1]);
                """, addon_id, tab_id)

            first = ready["tab"]["id"]
            second = ready["inactive"]["id"]
            api_call("setOptions", {"tabId": second, "path": "global.html", "enabled": True})
            select(second)
            receive("panel", path="/global.html")
            select(first)
            restored_first = receive("panel", path="/tab.html")
            check(restored_first["instance"] == action_capture["instance"], "Enabled tabs retain independent documents and restore the first tab state")
            api_call("setOptions", {"tabId": second, "enabled": False})
            select(second)
            runtime.wait_for("disabled tab hidden", lambda: not client.script("return window.SidebarController.isOpen;"), 10)
            select(first)
            receive("panel", path="/tab.html")
            check(True, "Disabled tab hides the panel and returning restores open intent")

            api_call("setOptions", {"tabId": first, "path": "global.html"})
            tab_instance = receive("panel", path="/global.html")["instance"]
            api_call("open", {"windowId": ready["tab"]["windowId"]})
            still_tab = receive("panel",path="/global.html")["instance"]
            check(still_tab == tab_instance, "Opening a window-global panel preserves the currently open tab-specific panel")
            api_call("close", {"tabId": first})
            api_call("open", {"windowId": ready["tab"]["windowId"]})
            global_instance = receive("panel", path="/global.html")["instance"]
            api_call("open", {"tabId": first})
            next_tab_instance = receive("panel", path="/global.html")["instance"]
            check(tab_instance != global_instance and tab_instance == next_tab_instance,
                  "Global and tab scopes with the same URL retain distinct independent documents")
            api_call("close", {"tabId": first})
            api_call("close", {"tabId": first})
            check(not client.script("return window.SidebarController.isOpen;"), "close hides the sidebar and repeated close succeeds")
            api_call("open", {"tabId": first})
            receive("panel", path="/global.html")
            client.script("window.SidebarController.hide();")
            runtime.wait_for("user close", lambda: not client.script("return window.SidebarController.isOpen;"), 10)
            select(second)
            select(first)
            check(not client.script("return window.SidebarController.isOpen;"), "Explicit UI close clears automatic reopening intent")
            api_call("setOptions", {"tabId": second, "enabled": True})
            api_call("open", {"tabId": second})
            check(not client.script("return window.SidebarController.isOpen;"), "Opening an inactive tab does not reveal its panel in the selected closed tab")
            select(second)
            second_restored = receive("panel", path="/global.html")
            check(client.script("return window.SidebarController.isOpen;"), "An inactive-tab open request becomes visible when that tab is selected")
            api_call("close", {"tabId": first})
            check(client.script("return window.SidebarController.isOpen;"), "Closing an inactive tab leaves the selected tab panel open")
            api_call("close", {"windowId": ready["tab"]["windowId"]})
            check(client.script("return window.SidebarController.isOpen;"), "Window close targets the global panel and preserves an open tab-specific panel")
            select(first)
            runtime.wait_for("closed inactive tab", lambda: not client.script("return window.SidebarController.isOpen;"), 10)
            api_call("open", {"tabId": first})
            final_first = receive("panel", path="/global.html")
            check(final_first["instance"] == tab_instance, "Inactive-tab close/open retains the scope document without sharing another tab's state")
            layout_before = client.script("return Services.prefs.getBoolPref('sidebar.position_start',true);")
            for start, expected_side in [(True,"left"),(False,"right")]:
                client.script("Services.prefs.setBoolPref('sidebar.position_start',arguments[0]);", start)
                check(api_call("getLayout", {}) == {"side":expected_side}, "getLayout reports the actual "+expected_side+" sidebar position")
            client.script("Services.prefs.setBoolPref('sidebar.position_start',arguments[0]);", layout_before)
            client.script("""
                const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                const extension=ExtensionParent.GlobalManager.getExtension(arguments[0]);
                extension.backgroundContext.xulBrowser.messageManager.loadFrameScript('data:application/javascript,'+encodeURIComponent("content.postMessage('floorp-sidepanel-events','*');"),false);
            """, addon_id)
            opened_closed = receive("panel-events")["events"]
            check(any(item["event"]=="opened" and item["info"].get("tabId")==first and item["info"].get("path")=="/global.html" for item in opened_closed), "onOpened reaches the real extension with tab/window/path information")
            check(any(item["event"]=="closed" and item["info"].get("windowId")==ready["tab"]["windowId"] for item in opened_closed), "onClosed reaches the real extension for native visibility changes")
            client.script("""
                window.__ordinaryInstall = {done:false};
                const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');
                const file=Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);
                file.initWithPath(arguments[0]);
                AddonManager.installTemporaryAddon(file).then(
                    addon=>window.__ordinaryInstall={done:true,id:addon.id},
                    error=>window.__ordinaryInstall={done:true,error:String(error)});
            """, str(ordinary_fixture))
            ordinary_install = runtime.wait_for("ordinary fixture install", lambda: client.script("return window.__ordinaryInstall.done ? window.__ordinaryInstall : null;"), 10)
            check(ordinary_install.get("id") == ordinary_id, "Ordinary Firefox fixture installs")
            ordinary_title = runtime.wait_for("ordinary background", lambda: client.script("""
                const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                const extension=ExtensionParent.GlobalManager.getExtension(arguments[0]);
                return [...extension.views].find(view=>view.viewType==='background')?.xulBrowser?.contentTitle || null;
            """, ordinary_id), 10)
            check(ordinary_title == "ordinary:undefined", "Ordinary Firefox extensions do not receive the sidePanel namespace")
            client.script("""
                window.__firefoxSidebar={done:false};
                const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                const extension=ExtensionParent.GlobalManager.getExtension(arguments[0]);
                const action=extension.apiManager.getAPI('sidebarAction',extension);
                window.SidebarController.show(action.id).then(()=>window.__firefoxSidebar={done:true});
            """, ordinary_id)
            runtime.wait_for("Firefox sidebar shown",lambda:client.script("return window.__firefoxSidebar.done;"),10)
            ordinary_panel = runtime.wait_for("Firefox sidebar document",lambda:client.script("return window.SidebarController.browser.contentDocument?.getElementById('webext-panels-browser')?.contentTitle || null;"),10)
            check(ordinary_panel == "Firefox sidebar", "Ordinary Firefox sidebar_action still loads its native document")
            api_call("open", {"tabId": first})
            after_firefox = receive("panel", path="/global.html")
            check(after_firefox["instance"] == final_first["instance"], "Switching to a Firefox sidebar and back retains the Chrome scope document")
            client.script("""
                const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                const extension=ExtensionParent.GlobalManager.getExtension(arguments[0]);
                const tab=extension.apiManager.global.tabTracker.getTab(arguments[1]);
                window.gBrowser.removeTab(tab,{animate:false});
            """, addon_id, second)
            check(client.script("""
                const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                const extension=ExtensionParent.GlobalManager.getExtension(arguments[0]);
                const manager=extension.apiManager.getAPI('sidePanel',extension,'addon_parent');
                const host=window.SidebarController._floorpSidePanelBrowsers.get(manager.action.id);
                return ![...host.contentDocument.querySelectorAll('browser')].some(browser=>browser.contentTitle?.includes(arguments[1]));
            """, addon_id, second_restored["instance"]), "Closing a tab destroys only its retained side-panel scope")
            client.script("window.__sidePanelOtherWindow=window.OpenBrowserWindow();")
            other_window = runtime.wait_for("second browser window",lambda:client.script("""
                const win=window.__sidePanelOtherWindow;
                if(!win?.gBrowserInit?.delayedStartupFinished)return null;
                const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                const extension=ExtensionParent.GlobalManager.getExtension(arguments[0]);
                return extension.apiManager.global.windowTracker.getId(win);
            """,addon_id),30)
            api_call("open", {"windowId":other_window})
            other_instance=runtime.wait_for("second window panel",lambda:client.script("""
                const win=window.__sidePanelOtherWindow;
                const title=win.SidebarController.browser.contentDocument?.getElementById('webext-panels-browser')?.contentTitle;
                try{return JSON.parse(title);}catch{return null;}
            """),10)
            check(other_instance["instance"] != final_first["instance"], "Separate windows own independent global and tab panel documents")
            api_call("close", {"windowId":other_window})
            check(client.script("return !window.__sidePanelOtherWindow.SidebarController.isOpen && window.SidebarController.isOpen;"), "Closing one window's global panel leaves another window's tab panel open")
            client.script("window.__sidePanelOtherWindow.close();")
            runtime.wait_for("second window closed",lambda:client.script("return window.__sidePanelOtherWindow.closed;"),10)
            check(client.script("""
                const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                const extension=ExtensionParent.GlobalManager.getExtension(arguments[0]);
                return !extension.apiManager.getAPI('sidePanel',extension).states.has(window.__sidePanelOtherWindow);
            """,addon_id), "Closing a browser window releases its SidePanel state")
            if args.floorp_ui:
                client.script("""
                    const directory=Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);
                    directory.initWithPath(arguments[0]);
                    Services.io.getProtocolHandler('resource').QueryInterface(Ci.nsIResProtocolHandler)
                      .setSubstitution('floorp-sidepanel-ui',Services.io.newFileURI(directory));
                    window.__floorpSidePanelUI={done:false};
                    const script=document.createElementNS('http://www.w3.org/1999/xhtml','script');
                    script.type='module';
                    script.onerror=()=>window.__floorpSidePanelUI={done:true,error:'Floorp UI module failed to load; see browser console'};
                    script.src='resource://floorp-sidepanel-ui/side-panel-ui.js';
                    document.documentElement.appendChild(script);
                """, str(ROOT / "_dist/side-panel-ui"))
                ui=runtime.wait_for("Floorp SidePanel UI",lambda:client.script("return window.__floorpSidePanelUI.done ? window.__floorpSidePanelUI : null;"),90)
                if ui.get("error"): raise AssertionError(ui)
                report["checks"].extend(ui["checks"])
            api_call("close", {"tabId":first})
            client.script("""
                window.__sidePanelStopped=false;
                const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                const extension=ExtensionParent.GlobalManager.getExtension(arguments[0]);
                extension.terminateBackground().then(()=>window.__sidePanelStopped=true);
            """,addon_id)
            runtime.wait_for("side-panel background stopped",lambda:client.script("return window.__sidePanelStopped;"),10)
            client.script("""
                const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                const extension=ExtensionParent.GlobalManager.getExtension(arguments[0]);
                const manager=extension.apiManager.getAPI('sidePanel',extension,'addon_parent');
                const tab=manager.action.getTargetFromDetails({tabId:arguments[1]});
                manager.openInWindow(tab.documentGlobal,tab);
            """,addon_id,first)
            receive("restarted")
            client.script("""
                const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                const extension=ExtensionParent.GlobalManager.getExtension(arguments[0]);
                extension.backgroundContext.xulBrowser.messageManager.loadFrameScript('data:application/javascript,'+encodeURIComponent("content.postMessage('floorp-sidepanel-events','*');"),false);
            """,addon_id)
            woke_events=receive("panel-events")["events"]
            check(any(item["event"]=="opened" for item in woke_events), "Native onOpened wakes a stopped event-page background and delivers its persistent event")
            client.script("""
                window.__sidePanelReload={done:false};
                const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                const extension=ExtensionParent.GlobalManager.getExtension(arguments[0]);
                const manager=extension.apiManager.getAPI('sidePanel',extension,'addon_parent');
                window.__oldSidePanelManager=manager;
                window.__oldSidePanelHost=window.SidebarController._floorpSidePanelBrowsers.get(manager.action.id);
                const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');
                AddonManager.getAddonByID(arguments[0]).then(addon=>addon.reload()).then(()=>window.__sidePanelReload={done:true},error=>window.__sidePanelReload={done:true,error:String(error)});
            """,addon_id)
            reload_result=runtime.wait_for("side-panel extension reload",lambda:client.script("return window.__sidePanelReload.done ? window.__sidePanelReload : null;"),20)
            check(not reload_result.get("error") and client.script("return !window.__oldSidePanelHost.isConnected && window.__oldSidePanelManager.states.size===0;"), "Reloading an extension destroys old retained hosts and window state")
            runtime.wait_for("reloaded background",lambda:client.script("""
                const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                return !!ExtensionParent.GlobalManager.getExtension(arguments[0])?.backgroundContext;
            """,addon_id),15)
            check(api_call("getPanelBehavior",{}).get("openPanelOnActionClick") is True, "The configured action behavior survives an actual extension reload")
            client.script("""
                window.__sidePanelUninstall={done:false};
                const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');
                AddonManager.getAddonByID(arguments[0]).then(addon=>addon.uninstall()).then(()=>window.__sidePanelUninstall={done:true},error=>window.__sidePanelUninstall={done:true,error:String(error)});
            """,addon_id)
            uninstall_result=runtime.wait_for("side-panel extension uninstall",lambda:client.script("return window.__sidePanelUninstall.done ? window.__sidePanelUninstall : null;"),20)
            check(not uninstall_result.get("error") and client.script("return !Services.prefs.prefHasUserValue('extensions.floorp.sidePanel.openOnAction.'+arguments[0]);",addon_id), "Uninstall removes the persisted panel action preference")
            report["passed"] = True
        except Exception as error:
            report["error"] = str(error)
            if client:
                report["panelState"] = client.script("""
                    const {ExtensionParent} = ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
                    const extension=ExtensionParent.GlobalManager.getExtension(arguments[0]);
                    const action=extension.apiManager.getAPI('sidebarAction',extension);
                    const manager=extension.apiManager.getAPI('sidePanel',extension,'addon_parent');
                    return {backgroundState:extension.backgroundState,
                        persistent:[...extension.persistentListeners??[]].map(([module,events])=>[module,[...events.keys()]]),
                        visible:manager.states.get(window)?.visible,
                        url:window.gBrowser.selectedBrowser.currentURI.spec,
                        selected:extension.apiManager.global.tabTracker.getId(window.gBrowser.selectedTab),
                        data:action.getContextData(window.gBrowser.selectedTab),
                        panel:action.panels.get(window),
                        target:action.chromePanelTargets.has(window),
                        isOpen:action.isOpen(window)};
                """, addon_id)
                report["console"] = client.script("return Services.console.getMessageArray().map(x=>x.message).filter(x=>/sidePanel|sidebarAction|extension|Error/i.test(x)).slice(-30);")
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
    (output / "report.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))
    print(f"Report: {output / 'report.json'}")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
