#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Exercise Chrome proxy.settings and real proxy traffic in a private profile."""

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import importlib.util
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import zipfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", type=Path, default=ROOT / "_dist/bin/floorp/floorp")
    parser.add_argument("--native-bypass", action="store_true", help="Require compiled loopback-subtraction support")
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("runtime", ROOT / "tools/app-shim/test-runtime.py")
    runtime = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runtime)
    output = Path(tempfile.mkdtemp(prefix="proxy-", dir=ROOT / "_dist/chrome-runtime-tests"))
    records = {"proxy": [], "origin": []}
    proxy_port = 0

    def handler(role):
        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                records[role].append(self.path)
                data = (f"function FindProxyForURL(url,host){{return 'PROXY 127.0.0.1:{proxy_port}';}}"
                        if self.path == "/proxy.pac" else role).encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/x-ns-proxy-autoconfig" if self.path == "/proxy.pac" else "text/plain")
                self.send_header("Cache-Control", "no-store")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def log_message(self, *_args):
                pass
        return Handler

    proxy = ThreadingHTTPServer(("127.0.0.1", 0), handler("proxy"))
    origin = ThreadingHTTPServer(("0.0.0.0", 0), handler("origin"))
    origin_ip = socket.gethostbyname(socket.gethostname())
    proxy_port = proxy.server_port
    for server in [proxy, origin]:
        threading.Thread(target=server.serve_forever, daemon=True).start()
    original_id = "c" * 32
    addon_id = original_id + "@chromium-extension"
    profile = output / "profile"
    profile.mkdir()
    port = runtime.unused_port()
    prefs = {
        "marionette.port": port, "browser.shell.checkDefaultBrowser": False,
        "browser.startup.homepage_override.mstone": "ignore", "browser.startup.page": 0,
        "browser.aboutwelcome.enabled": False, "datareporting.policy.dataSubmissionEnabled": False,
        "extensions.autoDisableScopes": 0, "network.proxy.type": 5,
        "network.dns.localDomains": "bypass.floorp-proxy.test",
        "floorp.releaseNotes.mode": "disabled", "floorp.releaseNotes.choiceConfirmed": True,
    }
    (profile / "user.js").write_text("\n".join(
        f"user_pref({json.dumps(key)}, {json.dumps(value)});" for key, value in prefs.items()) + "\n")
    background = r"""
const settings=chrome.proxy.settings;
const checks=[];
const check=(condition,message)=>{if(!condition)throw Error(message);checks.push(message);};
const report=value=>{document.title=JSON.stringify(value);};
const changes=[];
let receiveChange;
settings.onChange.addListener(async change=>{
  changes.push(change);receiveChange?.(change);
  if(change.value.mode==='fixed_servers'&&!Object.keys(change.value.rules||{}).length){
    try{
      const value=await settings.get({});
      const body=await fetch('http://bypass.floorp-proxy.test:ORIGIN_PORT/manual-disabled').then(r=>r.text());
      report({type:'manual-disabled',value,body});
    }catch(error){report({type:'error',error:String(error)});}
  }
});
const nextChange=()=>new Promise((resolve,reject)=>{
  const timer=setTimeout(()=>reject(Error('onChange timed out')),7000);
  receiveChange=change=>{clearTimeout(timer);receiveChange=null;resolve(change);};
});
async function set(value,callback=false){
  const changed=nextChange();
  if(callback) await new Promise((resolve,reject)=>settings.set({value},()=>{
    if(chrome.runtime.lastError)reject(Error(chrome.runtime.lastError.message));else resolve();
  }));
  else check(await settings.set({value})===undefined,'Chrome set resolves void');
  const event=await changed;
  check(event.value.mode===value.mode,'onChange contains Chrome config '+value.mode);
  check(event.levelOfControl==='controlled_by_this_extension','native control ownership');
  const actual=await settings.get({});
  check(actual.value.mode===value.mode,'get reflects '+value.mode);
}
async function reject(fn,pattern){
  try{await fn();}catch(error){check(pattern.test(String(error)),'explicit rejection: '+error);return;}
  throw Error('Expected rejection '+pattern);
}
(async()=>{
try{
  if(await browser.storage.local.get('finished').then(data=>data.finished)){
    report({type:'restarted'});
    receiveChange=change=>report({type:'woken',change});
    if(changes.length)receiveChange(changes[changes.length-1]);
    return;
  }
  const initial=await settings.get({});
  let nativeScopes=false;
  try{await settings.get({incognito:true});nativeScopes=true;}catch{}
  for(const mode of ['direct','auto_detect','system'])await set({mode});
  const fixed={mode:'fixed_servers',rules:{singleProxy:{scheme:'http',host:'127.0.0.1',port:PROXY_PORT}}};
  await set(fixed,true);
  check(JSON.stringify((await settings.get({})).value)===JSON.stringify(fixed),'fixed config round trip');
  await new Promise((resolve,reject)=>settings.get({},details=>{
    try{check(details.value.mode==='fixed_servers'&&!chrome.runtime.lastError,'Chrome get callback');resolve();}catch(error){reject(error);}
  }));
  check(await fetch('http://floorp-proxy.invalid/fixed').then(r=>r.text())==='proxy','fixed mode sends actual traffic through proxy');
  check(await fetch('http://127.example.test/numeric-prefix').then(r=>r.text())==='proxy','numeric-prefix DNS names remain proxied');
  check(await fetch('http://169.254.example.test/link-prefix').then(r=>r.text())==='proxy','link-local-prefix DNS names remain proxied');
  await set({mode:'fixed_servers',rules:{...fixed.rules,bypassList:['bypass.floorp-proxy.test']}});
  check(await fetch('http://bypass.floorp-proxy.test:ORIGIN_PORT/bypassed').then(r=>r.text())==='origin','bypass reaches origin directly');
  await set({mode:'fixed_servers',rules:{...fixed.rules,bypassList:['ORIGIN_IP/32']}});
  check(await fetch('http://ORIGIN_IP:ORIGIN_PORT/cidr-bypassed').then(r=>r.text())==='origin','CIDR bypass reaches actual local-network origin');
  await set({mode:'fixed_servers',rules:{...fixed.rules,bypassList:['http://ORIGIN_IP:ORIGIN_PORT']}});
  check(await fetch('http://ORIGIN_IP:ORIGIN_PORT/port-bypassed').then(r=>r.text())==='origin','Scheme and port bypass reaches actual origin');
  check(await fetch('http://ORIGIN_IP:OTHER_PORT/port-mismatch').then(r=>r.text())==='proxy','Port mismatch still reaches actual proxy');
  if(NATIVE_BYPASS){
    await set({mode:'fixed_servers',rules:{...fixed.rules,bypassList:['<-loopback>']}});
    check(await fetch('http://127.0.0.1:ORIGIN_PORT/subtracted-loopback').then(r=>r.text())==='proxy','Loopback subtraction sends actual loopback request through proxy');
  }
  const data="function FindProxyForURL(url,host){return 'PROXY 127.0.0.1:PROXY_PORT';}";
  await set({mode:'pac_script',pacScript:{data}});
  check((await settings.get({})).value.pacScript.data===data,'inline PAC round trip');
  check(await fetch('http://floorp-proxy.invalid/inline-pac').then(r=>r.text())==='proxy','inline PAC proxy traffic');
  await set({mode:'pac_script',pacScript:{url:'http://localhost:ORIGIN_PORT/proxy.pac'}});
  check(await fetch('http://floorp-proxy.invalid/url-pac').then(r=>r.text())==='proxy','URL PAC proxy traffic');
  const before=JSON.stringify((await settings.get({})).value);
  for(const value of [
    {mode:'fixed_servers',rules:{singleProxy:{host:'localhost',scheme:'quic'}}},
    ...(!nativeScopes?[{mode:'pac_script',pacScript:{data,mandatory:true}}]:[]),
    {mode:'fixed_servers',rules:{singleProxy:{host:'localhost'},bypassList:['10.0.0.0/33']}},
    {mode:'fixed_servers',rules:{singleProxy:{host:'localhost'},proxyForHttp:{host:'localhost'}}},
  ])await reject(()=>settings.set({value}),/Unsupported|not supported|cannot be combined|Invalid/);
  if(!nativeScopes){
    await reject(()=>settings.set({value:{mode:'direct'},scope:'regular_only'}),/scope/);
    await reject(()=>settings.get({incognito:true}),/Incognito/);
  }
  check(JSON.stringify((await settings.get({})).value)===before,'invalid settings do not mutate active proxy');
  await new Promise((resolve,reject)=>settings.set({value:{mode:'bad-mode'}},()=>{
    try{check(!!chrome.runtime.lastError,'callback errors use runtime.lastError');resolve();}catch(error){reject(error);}
  }));
  const cleared=nextChange();
  check(await settings.clear({})===undefined,'Chrome clear resolves void');
  await cleared;
  check(JSON.stringify((await settings.get({})).value)===JSON.stringify(initial.value),'clear restores original config');
  await set(fixed);
  await browser.storage.local.set({finished:true});
  report({type:'done',checks,changes});
}catch(error){report({type:'error',error:String(error),stack:error.stack,checks,changes});}
})();
""".replace("PROXY_PORT", str(proxy_port)).replace("ORIGIN_PORT", str(origin.server_port)).replace("OTHER_PORT", str(origin.server_port + 1)).replace("ORIGIN_IP", origin_ip).replace("NATIVE_BYPASS", "true" if args.native_bypass else "false")
    fixture = output / "proxy.xpi"
    with zipfile.ZipFile(fixture, "w") as archive:
        archive.writestr("manifest.json", json.dumps({
            "manifest_version": 3, "name": "Chrome proxy compatibility fixture", "version": "1.0",
            "permissions": ["proxy", "storage"], "host_permissions": ["<all_urls>"],
            "content_security_policy": {"extension_pages": "script-src 'self'; object-src 'self'; connect-src http: https:"},
            "background": {"scripts": ["background.js"]},
            "browser_specific_settings": {"gecko": {"id": addon_id,
                "floorp_chrome_extension": {"schema_version": 1, "original_id": original_id}}},
        }))
        archive.writestr("background.js", background)
    ordinary_id = "ordinary-proxy@example.com"
    ordinary_fixture = output / "ordinary.xpi"
    with zipfile.ZipFile(ordinary_fixture, "w") as archive:
        archive.writestr("manifest.json", json.dumps({
            "manifest_version": 3, "name": "Firefox proxy compatibility fixture", "version": "1.0",
            "permissions": ["proxy"], "background": {"scripts": ["background.js"]},
            "browser_specific_settings": {"gecko": {"id": ordinary_id}},
        }))
        archive.writestr("background.js", """
          (async()=>{try{
            const result=await browser.proxy.settings.set({value:{proxyType:'none'}});
            const value=await browser.proxy.settings.get({});
            document.title=JSON.stringify({type:'ordinary',result,value});
          }catch(error){document.title=JSON.stringify({type:'error',error:String(error)});}})();
        """)
    client = host = None
    result = {"passed": False, "checks": [], "traffic": records}

    def check(condition, message):
        if not condition:
            raise AssertionError(message)
        result["checks"].append(message)

    def state(extension_id):
        data = client.script("""
          const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
          const ext=ExtensionParent.GlobalManager.getExtension(arguments[0]);
          const title=[...(ext?.views||[])].find(v=>v.viewType==='background'&&!v.unloaded)?.xulBrowser?.contentTitle;
          try{return JSON.parse(title);}catch{return null;}
        """, extension_id)
        if data and data.get("type") == "error":
            raise RuntimeError(data)
        return data

    def install(path, extension_id):
        client.script("""
          window.__proxyInstall={done:false};
          (async()=>{
            const {ExtensionPermissions}=ChromeUtils.importESModule('resource://gre/modules/ExtensionPermissions.sys.mjs');
            await ExtensionPermissions.add(arguments[1],{permissions:['internal:privateBrowsingAllowed'],origins:[]});
            const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');
            const file=Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);file.initWithPath(arguments[0]);
            const addon=await AddonManager.installTemporaryAddon(file);
            window.__proxyInstall={done:true,id:addon.id};
          })().catch(error=>{window.__proxyInstall={done:true,error:String(error)};});
        """, str(path), extension_id)
        data = runtime.wait_for("extension install", lambda: client.script(
            "return window.__proxyInstall.done?window.__proxyInstall:null;"), 30)
        check(data.get("id") == extension_id, f"Installed {extension_id}: {data}")

    with (output / "browser.log").open("w") as log:
        try:
            binary = args.browser.resolve()
            if binary.is_dir(): binary = binary / "floorp"
            host = subprocess.Popen([str(binary), "--headless", "--no-remote", "--profile", str(profile),
                "--marionette", "--remote-allow-system-access", "about:blank"],
                stdout=log, stderr=subprocess.STDOUT, env={**os.environ, "MOZ_CRASHREPORTER_DISABLE": "1"})

            def connect():
                if host.poll() is not None: raise RuntimeError(f"Browser exited {host.returncode}")
                try: return runtime.Marionette(port)
                except (ConnectionRefusedError, socket.timeout): return None

            client = runtime.wait_for("Marionette", connect, 45)
            client.context("chrome")
            install(fixture, addon_id)
            done = runtime.wait_for("proxy effects", lambda: (value if value and value.get("type") == "done" else None)
                if (value := state(addon_id)) is not None else None, 60)
            result["extension"] = done
            check(any("/fixed" in path for path in records["proxy"]), "Real fixed-server request reached proxy")
            check(any("/inline-pac" in path for path in records["proxy"]), "Real inline PAC request reached proxy")
            check(any("/url-pac" in path for path in records["proxy"]), "Real URL PAC request reached proxy")
            check(any("127.example.test/numeric-prefix" in path for path in records["proxy"]), "Numeric-prefix DNS request reached proxy")
            check(any("169.254.example.test/link-prefix" in path for path in records["proxy"]), "Link-local-prefix DNS request reached proxy")
            check("/bypassed" in records["origin"], "Real bypass request reached origin")
            check("/cidr-bypassed" in records["origin"], "Real CIDR bypass reached origin")
            check("/port-bypassed" in records["origin"], "Real scheme/port bypass reached origin")
            check(any("/port-mismatch" in path for path in records["proxy"]), "Mismatching bypass port request reached proxy")
            if args.native_bypass:
                check(any("/subtracted-loopback" in path for path in records["proxy"]), "Compiled loopback subtraction reached proxy")
            check(not any("/bypassed" in path for path in records["proxy"]), "Bypass request did not use proxy")
            client.script("""
              for(const [key,value] of Object.entries({
                'network.proxy.http':'proxy.example', 'network.proxy.http_port':0,
                'network.proxy.ssl':'secure.example', 'network.proxy.ssl_port':0,
                'network.proxy.socks':'', 'network.proxy.socks_port':0,
              })){
                if(typeof value==='string')Services.prefs.setStringPref(key,value);
                else Services.prefs.setIntPref(key,value);
              }
              Services.prefs.setIntPref('network.proxy.type',1);
            """)
            disabled = runtime.wait_for("manual port zero readback", lambda: (value if value and value.get("type") == "manual-disabled" else None)
                if (value := state(addon_id)) is not None else None, 20)
            check(disabled["value"]["value"] == {"mode": "fixed_servers", "rules": {}}, "Native manual endpoints with zero ports are absent from Chrome get")
            check(disabled["body"] == "origin" and "/manual-disabled" in records["origin"], "Native disabled endpoints route directly to origin")
            client.script("""
              const {ExtensionPreferencesManager}=ChromeUtils.importESModule('resource://gre/modules/ExtensionPreferencesManager.sys.mjs');
              window.__proxyRestore=false;
              ExtensionPreferencesManager.getSetting('proxy.settings').then(item=>
                ExtensionPreferencesManager.setSetting(arguments[0],'proxy.settings',item.value)
              ).then(()=>{window.__proxyRestore=true;});
            """, addon_id)
            runtime.wait_for("proxy restore before stop", lambda: client.script("return window.__proxyRestore;"), 10)
            client.script("""
              const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
              const ext=ExtensionParent.GlobalManager.getExtension(arguments[0]);
              window.__proxyStopped=false;
              ext.terminateBackground({disableResetIdleForTest:true}).then(()=>{window.__proxyStopped=true;});
            """, addon_id)
            runtime.wait_for("background stopped", lambda: client.script("return window.__proxyStopped;"), 15)
            client.script("Services.prefs.setIntPref('network.proxy.type',0);")
            woken = runtime.wait_for("proxy event background wakeup", lambda: (value if value and value.get("type") == "woken" else None)
                if (value := state(addon_id)) is not None else None, 30)
            check(woken["change"]["value"]["mode"] == "direct", "Native preference event wakes stopped background")
            # Return to the extension-controlled setting before verifying native rollback.
            client.script("""
              const {ExtensionPreferencesManager}=ChromeUtils.importESModule('resource://gre/modules/ExtensionPreferencesManager.sys.mjs');
              window.__proxyRestore=false;
              ExtensionPreferencesManager.getSetting('proxy.settings').then(item=>
                ExtensionPreferencesManager.setSetting(arguments[0],'proxy.settings',item.value)
              ).then(()=>{window.__proxyRestore=true;});
            """, addon_id)
            runtime.wait_for("proxy restore", lambda: client.script("return window.__proxyRestore;"), 10)
            client.script("""
              const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');
              window.__proxyUnloaded=false;
              AddonManager.getAddonByID(arguments[0]).then(addon=>addon.uninstall()).then(()=>{window.__proxyUnloaded=true;});
            """, addon_id)
            runtime.wait_for("proxy unload", lambda: client.script("return window.__proxyUnloaded;"), 15)
            check(client.script("return Services.prefs.getIntPref('network.proxy.type');") == 5, "Uninstall restores initial native proxy configuration")
            install(ordinary_fixture, ordinary_id)
            ordinary = runtime.wait_for("Firefox proxy interface", lambda: state(ordinary_id), 20)
            check(ordinary["result"] is True and ordinary["value"]["value"]["proxyType"] == "none", "Ordinary Firefox proxy.settings shape and boolean return are preserved")
            result["passed"] = True
        except Exception as error:
            result["error"] = str(error)
            if client:
                try:
                    result["diagnostics"] = client.script("""
                      return {config:Services.prefs.getStringPref('network.proxy.autoconfig_url'),
                        type:Services.prefs.getIntPref('network.proxy.type'),
                        console:Services.console.getMessageArray().slice(-40).map(m=>m.message)};
                    """)
                except Exception:
                    pass
        finally:
            if client: client.close()
            if host:
                host.terminate()
                try: host.wait(timeout=8)
                except subprocess.TimeoutExpired: host.kill(); host.wait(timeout=5)
            for server in [proxy, origin]: server.shutdown(); server.server_close()
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    print(output / "report.json")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
