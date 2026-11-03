#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Verify compiled Chrome proxy scopes with real regular/private HTTP traffic."""
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
    parser.add_argument('--browser', type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('runtime', ROOT / 'tools/app-shim/test-runtime.py')
    runtime = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runtime)
    output_root = ROOT / '_dist/chrome-runtime-tests'
    output_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='proxy-scopes-', dir=output_root))
    records = {role: [] for role in ['regular', 'persistent', 'session', 'origin']}

    def handler(role):
        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                records[role].append(self.path)
                data = (f"function FindProxyForURL(url,host){{if(url.indexOf('/scope-')>=0)alert('SCOPE-private-url:'+url);return 'PROXY 127.0.0.1:{servers['persistent'].server_port}';}}" if self.path == '/private.pac' else role).encode()
                self.send_response(200)
                self.send_header('Content-Type', 'application/x-ns-proxy-autoconfig' if self.path == '/private.pac' else 'text/plain')
                self.send_header('Cache-Control', 'no-store')
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def log_message(self, *_args):
                pass
        return Handler

    servers = {role: ThreadingHTTPServer(('0.0.0.0' if role == 'origin' else '127.0.0.1', 0), handler(role)) for role in records}
    for server in servers.values():
        threading.Thread(target=server.serve_forever, daemon=True).start()
    addon_id = 'eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee@chromium-extension'
    background = r'''
const checks=[],events=[];
const settings=chrome.proxy.settings;
const report=value=>{document.title=JSON.stringify(value);};
const check=(value,message)=>{if(!value)throw Error(message);checks.push(message);report({type:'progress',checks,events});};
const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
settings.onChange.addListener(value=>events.push(value));
const pac=(role,port)=>({mode:'pac_script',pacScript:{data:
  `var count=0;function FindProxyForURL(url,host){if(url.indexOf('/scope-')>=0)alert('SCOPE-${role}:'+url+'#'+(++count));return 'PROXY 127.0.0.1:${port}';}`}});
const a=pac('regular',REGULAR_PORT),b=pac('persistent',PERSISTENT_PORT),c=pac('session',SESSION_PORT);
async function rejects(operation,pattern){try{await operation();}catch(error){check(pattern.test(String(error)),'Explicit scoped error: '+error);return;}throw Error('Expected scoped error '+pattern);}
async function body(windowId,path,expected,direct=false){
  const url=(direct?'http://127.0.0.1:ORIGIN_PORT':'http://floorp-scope.invalid')+path;
  const tab=await chrome.tabs.create({windowId,url,active:true});
  let value;
  for(let i=0;i<150;i++){
    await sleep(100);
    try{value=(await chrome.scripting.executeScript({target:{tabId:tab.id},func:()=>document.body?.textContent.trim()}))[0]?.result;}catch{}
    if(value===expected)break;
  }
  await chrome.tabs.remove(tab.id);
  check(value===expected,path+' reaches actual '+expected+' endpoint ('+value+')');
}
async function set(value,scope){
  check(await settings.set({value,scope})===undefined,'Set resolves void for '+scope);
}
(async()=>{try{
  const state=await chrome.storage.local.get('restart');
  const regular=(await chrome.windows.getAll({windowTypes:['normal']})).find(win=>!win.incognito);
  if(state.restart){
    const got=await settings.get({incognito:true});
    check(got.incognitoSpecific&&got.value.pacScript.data===b.pacScript.data,'Persistent incognito configuration survives add-on restart');
    const win=await chrome.windows.create({incognito:true,url:'about:blank'});
    await body(win.id,'/scope-private-restarted','persistent');
    await chrome.windows.remove(win.id);
    report({type:'restarted',checks,events});return;
  }
  await rejects(()=>settings.set({scope:'incognito_session_only',value:c}),/incognito window/i);
  await set(a,'regular_only');
  await body(regular.id,'/scope-regular-only','regular');
  let privateWindow=await chrome.windows.create({incognito:true,url:'about:blank'});
  await body(privateWindow.id,'/scope-private-regular-only','origin',true);
  check(!(await settings.get({incognito:true})).incognitoSpecific,'Regular-only configuration does not become an incognito setting');
  await set(a,'regular');
  await body(regular.id,'/scope-regular-inherited-first','regular');
  await body(privateWindow.id,'/scope-private-inherited','regular');
  await body(regular.id,'/scope-regular-inherited-second','regular');
  await set(b,'incognito_persistent');
  const privateConfig=await settings.get({incognito:true});
  check(privateConfig.incognitoSpecific&&privateConfig.value.pacScript.data===b.pacScript.data,'Private get exposes actual persistent scope');
  check((await settings.get({})).value.pacScript.data===a.pacScript.data,'Private set preserves regular configuration');
  await body(privateWindow.id,'/scope-private-persistent','persistent');
  await body(regular.id,'/scope-regular-during-private','regular');
  await set({mode:'pac_script',pacScript:{url:'http://127.0.0.1:ORIGIN_PORT/private.pac'}},'incognito_persistent');
  await body(privateWindow.id,'/scope-private-url-pac','persistent');
  await set(b,'incognito_persistent');
  await new Promise((resolve,reject)=>settings.set({value:c,scope:'incognito_session_only'},()=>chrome.runtime.lastError?reject(Error(chrome.runtime.lastError.message)):resolve()));
  await body(privateWindow.id,'/scope-private-session','session');
  await settings.clear({scope:'incognito_session_only'});
  await body(privateWindow.id,'/scope-private-session-cleared','persistent');
  await set(c,'incognito_session_only');
  await chrome.windows.remove(privateWindow.id);
  await sleep(500);
  privateWindow=await chrome.windows.create({incognito:true,url:'about:blank'});
  await body(privateWindow.id,'/scope-private-new-session','persistent');
  check((await settings.get({incognito:true})).value.pacScript.data===b.pacScript.data,'Closing final private window clears volatile session scope');
  await settings.clear({scope:'incognito_persistent'});
  await body(privateWindow.id,'/scope-private-clear-inherits','regular');
  check(!(await settings.get({incognito:true})).incognitoSpecific,'Cleared private setting reports regular inheritance');
  await set(b,'incognito_persistent');
  await chrome.windows.remove(privateWindow.id);
  await set({mode:'pac_script',pacScript:{data:'this is invalid PAC javascript',mandatory:true}},'regular');
  let failed=false;try{await fetch('http://ORIGIN_IP:ORIGIN_PORT/scope-mandatory-must-not-direct',{signal:AbortSignal.timeout(10000)});}catch{failed=true;}
  check(failed,'Mandatory invalid PAC rejects actual request instead of DIRECT fallback');
  await set(a,'regular');
  await sleep(250);
  check(events.some(event=>event.incognitoSpecific&&event.value.pacScript?.data===b.pacScript.data),'onChange reports actual private scope');
  check(events.some(event=>event.incognitoSpecific&&event.value.pacScript?.data===c.pacScript.data),'onChange reports actual private session scope');
  await chrome.storage.local.set({restart:true});
  report({type:'done',checks,events});
}catch(error){report({type:'error',error:String(error),stack:error.stack,checks,events});}})();
'''
    for role, server in servers.items():
        background = background.replace(role.upper() + '_PORT', str(server.server_port))
    background = background.replace('ORIGIN_IP', socket.gethostbyname(socket.gethostname()))
    fixture = output / 'proxy-scopes.xpi'
    with zipfile.ZipFile(fixture, 'w') as archive:
        archive.writestr('manifest.json', json.dumps({
            'manifest_version': 3, 'name': 'Native Chrome proxy scope fixture', 'version': '1',
            'permissions': ['proxy', 'storage', 'tabs', 'scripting'], 'host_permissions': ['<all_urls>'],
            'background': {'scripts': ['background.js']},
            'content_security_policy': {'extension_pages': "script-src 'self'; object-src 'self'; connect-src http: https:"},
            'browser_specific_settings': {'gecko': {'id': addon_id,
                'floorp_chrome_extension': {'schema_version': 1, 'original_id': 'e' * 32}}},
        }))
        archive.writestr('background.js', background)
    profile = output / 'profile'
    profile.mkdir()
    port = runtime.unused_port()
    prefs = {'marionette.port': port, 'network.proxy.type': 0,
        'browser.shell.checkDefaultBrowser': False, 'browser.startup.page': 0,
        'browser.startup.homepage_override.mstone': 'ignore', 'browser.aboutwelcome.enabled': False,
        'datareporting.policy.dataSubmissionEnabled': False, 'extensions.autoDisableScopes': 0,
        'floorp.releaseNotes.mode': 'disabled', 'floorp.releaseNotes.choiceConfirmed': True}
    (profile / 'user.js').write_text('\n'.join(f'user_pref({json.dumps(k)}, {json.dumps(v)});' for k,v in prefs.items())+'\n')
    client = host = None
    result = {'passed': False, 'checks': [], 'traffic': records}

    def check(value, message):
        if not value: raise AssertionError(message)
        result['checks'].append(message)

    def state():
        data = client.script("""
          const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
          const ext=ExtensionParent.GlobalManager.getExtension(arguments[0]);
          const title=[...(ext?.views||[])].find(v=>v.viewType==='background'&&!v.unloaded)?.xulBrowser?.contentTitle;
          try{return JSON.parse(title);}catch{return null;}
        """, addon_id)
        if data and data.get('type') == 'error': raise RuntimeError(data)
        return data

    with (output / 'browser.log').open('w') as log:
        try:
            binary = args.browser.resolve()
            if binary.is_dir(): binary /= 'floorp'
            host = subprocess.Popen([str(binary),'--headless','--no-remote','--profile',str(profile),
                '--marionette','--remote-allow-system-access','about:blank'], stdout=log,stderr=subprocess.STDOUT,
                env={**os.environ,'MOZ_CRASHREPORTER_DISABLE':'1'})
            def connect():
                if host.poll() is not None: raise RuntimeError(f'Browser exited {host.returncode}')
                try:return runtime.Marionette(port)
                except (ConnectionRefusedError,socket.timeout):return None
            client=runtime.wait_for('Marionette',connect,45)
            client.context('chrome')
            check(client.script("return typeof Cc['@mozilla.org/network/protocol-proxy-service;1'].getService(Ci.nsIProtocolProxyService2).registerChromeScopeProvider==='function';"),'Compiled pre-PAC scope provider exists')
            client.script("""
              window.__scopesInstalled=false;
              (async()=>{
                const {ExtensionPermissions}=ChromeUtils.importESModule('resource://gre/modules/ExtensionPermissions.sys.mjs');
                await ExtensionPermissions.add(arguments[1],{permissions:['internal:privateBrowsingAllowed'],origins:[]});
                const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');
                const file=Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);file.initWithPath(arguments[0]);
                await AddonManager.installTemporaryAddon(file);window.__scopesInstalled=true;
              })().catch(error=>{window.__scopesInstalled=String(error);});
            """,str(fixture),addon_id)
            check(runtime.wait_for('install',lambda:client.script('return window.__scopesInstalled;'),30) is True,'Installed scoped proxy fixture')
            result['extension']=runtime.wait_for('scope network effects',lambda:(data if data and data.get('type')=='done' else None) if (data:=state()) is not None else None,180)
            check('/scope-private-regular-only' in records['origin'],'Regular-only private request reaches origin')
            check(not any('scope-private-regular-only' in path for path in records['regular']),'Regular-only proxy never receives private request')
            check(not any('scope-private-persistent' in path or 'scope-private-session' in path or 'scope-private-url-pac' in path for path in records['regular']),'Private override destinations never reach regular proxy')
            check('/scope-mandatory-must-not-direct' not in records['origin'],'Mandatory PAC failure never reaches direct origin')
            console=client.script('return Services.console.getMessageArray().map(message=>message.message);')
            result['pacAudit']=[line for line in console if 'SCOPE-' in line]
            check(not any('SCOPE-regular:' in line and ('/scope-private-persistent' in line or '/scope-private-session' in line or '/scope-private-url-pac' in line or '/scope-private-regular-only' in line) for line in console),'Regular PAC never observes private override destinations')
            check(any('SCOPE-regular:' in line and '/scope-private-inherited#1' in line for line in console),'Inherited private PAC has a separate interpreter counter')
            client.script("""
              window.__scopeRestart=false;
              (async()=>{const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');
                const addon=await AddonManager.getAddonByID(arguments[0]);await addon.disable();await addon.enable();window.__scopeRestart=true;
              })().catch(error=>{window.__scopeRestart=String(error);});
            """,addon_id)
            check(runtime.wait_for('restart',lambda:client.script('return window.__scopeRestart;'),30) is True,'Disabled and re-enabled actual add-on')
            result['restart']=runtime.wait_for('persistent scope restart',lambda:(data if data and data.get('type')=='restarted' else None) if (data:=state()) is not None else None,45)
            result['passed']=True
        except Exception as error:
            result['error']=str(error)
            if client:
                try:result['diagnostics']=client.script('return Services.console.getMessageArray().slice(-50).map(message=>message.message);')
                except Exception:pass
        finally:
            if client:client.close()
            if host:
                host.terminate()
                try:host.wait(timeout=8)
                except subprocess.TimeoutExpired:host.kill();host.wait(timeout=5)
            for server in servers.values():server.shutdown();server.server_close()
    (output/'report.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2));print(output/'report.json')
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
