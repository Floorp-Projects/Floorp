#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Exercise Chrome DNR quotas, atomic updates and actual native network effects."""
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
    parser.add_argument('--browser', type=Path, default=ROOT / '_dist/bin/floorp/floorp')
    parser.add_argument('--skip-native-regex', action='store_true', help='Explicitly run only JS/quota/network coverage on precompiled artifacts')
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('runtime', ROOT / 'tools/app-shim/test-runtime.py')
    runtime = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runtime)
    output = Path(tempfile.mkdtemp(prefix='network-', dir=ROOT / '_dist/chrome-runtime-tests'))
    requests = []

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            record = {'path': self.path, 'headers': dict(self.headers)}
            requests.append(record)
            data = json.dumps(record).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Origin', 'original')
            self.send_header('Set-Cookie', 'floorp-network-test=1; SameSite=Lax')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def log_message(self, *_args):
            pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    addon_id = 'dddddddddddddddddddddddddddddddd@chromium-extension'
    background = r'''
const d=chrome.declarativeNetRequest;
const checks=[];const debug=[];const rawHeaders=[];
chrome.webRequest.onHeadersReceived.addListener(info=>rawHeaders.push(info),{urls:['http://127.0.0.1/*']},['responseHeaders','extraHeaders']);
chrome.webRequest.onErrorOccurred.addListener(()=>{},{urls:['http://127.0.0.1/*']},['extraHeaders']);
const check=(value,message)=>{if(!value)throw Error(message);checks.push(message);document.title=JSON.stringify({type:'progress',checks});};
const report=value=>{document.title=JSON.stringify(value);};
d.onRuleMatchedDebug.addListener(info=>debug.push(info));
async function rejects(fn,pattern){try{await fn();}catch(error){check(pattern.test(String(error)),String(error));return;}throw Error('Expected rejection '+pattern);}
(async()=>{try{
 const state=await chrome.storage.local.get('restart');
 if(state.restart){
  check((await d.getDynamicRules()).length===6000,'Dynamic rules survive native add-on restart');
  check((await d.getSessionRules()).length===0,'Session rules clear on add-on restart');
  report({type:'restarted',checks});return;
 }
 check(d.MAX_NUMBER_OF_DYNAMIC_RULES===30000,'Chrome safe dynamic quota is 30000');
 check(d.MAX_NUMBER_OF_UNSAFE_DYNAMIC_RULES===5000,'Chrome unsafe dynamic quota is 5000');
 check(d.MAX_NUMBER_OF_ENABLED_STATIC_RULESETS===50,'Chrome enabled static rulesets quota is 50');
 const safe=Array.from({length:6000},(_,i)=>({id:i+1,action:{type:'block'},condition:{urlFilter:'||never'+i+'.invalid^'}}));
 await d.updateDynamicRules({addRules:safe});
 check((await d.getDynamicRules()).length===6000,'More than Firefox 5000 safe dynamic rules are accepted');
 const unsafe=Array.from({length:5001},(_,i)=>({id:i+10000,action:{type:'redirect',redirect:{url:'http://127.0.0.1:ORIGIN/redirected'}},condition:{urlFilter:'||unsafe'+i+'.invalid^'}}));
 await rejects(()=>d.updateDynamicRules({removeRuleIds:safe.map(r=>r.id),addRules:unsafe}),/UNSAFE_DYNAMIC_RULES/);
 check((await d.getDynamicRules()).length===6000,'Rejected unsafe quota update preserves original dynamic rules atomically');
 const excess=Array.from({length:30001},(_,i)=>({id:i+1,action:{type:'allow'},condition:{urlFilter:'||never'+i+'.invalid^'}}));
 await rejects(()=>d.updateDynamicRules({removeRuleIds:safe.map(r=>r.id),addRules:excess}),/MAX_NUMBER_OF_DYNAMIC_RULES/);
 check((await d.getDynamicRules({ruleIds:[1,6000,999999]})).length===2,'Dynamic query ruleIds filter');
 await d.updateSessionRules({addRules:[
  {id:1,action:{type:'block'},condition:{urlFilter:'/blocked',resourceTypes:['xmlhttprequest']}},
  {id:2,action:{type:'redirect',redirect:{url:'http://127.0.0.1:ORIGIN/redirected'}},condition:{urlFilter:'/redirect-me',resourceTypes:['xmlhttprequest']}},
  {id:3,action:{type:'modifyHeaders',requestHeaders:[{header:'x-floorp-dnr',operation:'set',value:'native'}],responseHeaders:[{header:'x-origin',operation:'set',value:'rewritten'}]},condition:{urlFilter:'/headers',resourceTypes:['xmlhttprequest']}}
 ]});
 await rejects(()=>d.updateSessionRules({addRules:[{id:1,action:{type:'allow'},condition:{urlFilter:'x'}}]}),/duplicate|unique|already/i);
 check((await d.getSessionRules()).length===3,'Invalid session update preserves original rules');
 await new Promise((resolve,reject)=>d.updateSessionRules({addRules:[{id:1,action:{type:'allow'},condition:{urlFilter:'x'}}]},()=>{try{check(!!chrome.runtime.lastError,'Callback errors use runtime.lastError');resolve();}catch(error){reject(error);}}));
 let blocked=false;try{await fetch('http://127.0.0.1:ORIGIN/blocked');}catch{blocked=true;}
 check(blocked,'Native DNR block rejects real network request');
 const redirect=await fetch('http://127.0.0.1:ORIGIN/redirect-me').then(r=>r.json());
 check(redirect.path==='/redirected','Native DNR redirect reaches destination');
 const response=await fetch('http://127.0.0.1:ORIGIN/headers');const headers=await response.json();
 check(headers.headers['x-floorp-dnr']==='native','Native request header modification reaches server');
 check(response.headers.get('x-origin')==='rewritten','Native response header modification reaches extension');
 await new Promise(resolve=>setTimeout(resolve,100));
 check(rawHeaders.some(info=>info.responseHeaders.some(header=>header.name.toLowerCase()==='set-cookie'&&header.value.includes('floorp-network-test'))),'Chrome extraHeaders listener receives actual protected response header');
 check(debug.some(info=>info.rule.ruleId===1&&info.request.url.endsWith('/blocked')),'Debug event reports actual blocked request');
 const history=await d.getMatchedRules({minTimeStamp:Date.now()-10000});
 check(history.rulesMatchedInfo.some(info=>info.rule.ruleId===2&&typeof info.tabId==='number'&&typeof info.timeStamp==='number'),'getMatchedRules returns actual native match history: '+JSON.stringify(history));
 check((await d.getMatchedRules({minTimeStamp:Date.now()+10000})).rulesMatchedInfo.length===0,'Match history honors minTimeStamp');
 for(let i=2;i<20;i++)await d.getMatchedRules();
 await rejects(()=>d.getMatchedRules(),/quota/);
 const match=await d.testMatchOutcome({url:'http://127.0.0.1:ORIGIN/blocked',type:'xmlhttprequest'});
 check(match.matchedRules.some(rule=>rule.ruleId===1),'Unpacked Chrome testMatchOutcome works without Firefox feedback pref');
 if(NATIVE_REGEX){
  const good=['^https?://([^/]+)/','(?i)HELLO','\\Qliteral.*\\E','\\p{Greek}','[a&&b]','(a+)+$'];
  for(const regex of good)check((await d.isRegexSupported({regex,requireCapturing:true})).isSupported,'Native linear regex supports '+regex);
  for(const regex of ['a(?=b)','(a)\\1','a{1001}','\\u0061'])check(!(await d.isRegexSupported({regex})).isSupported,'RE2-invalid regex is rejected '+regex);
  await d.updateSessionRules({addRules:[{id:4,action:{type:'redirect',redirect:{regexSubstitution:'http://127.0.0.1:ORIGIN/captured-\\1'}},condition:{regexFilter:'/capture-([0-9]+)$',resourceTypes:['xmlhttprequest']}}]});
  const captured=await fetch('http://127.0.0.1:ORIGIN/capture-42').then(r=>r.json());
  check(captured.path==='/captured-42','Native linear regex captures drive actual network redirect');
  await rejects(()=>d.updateDynamicRules({removeRuleIds:[1],addRules:[{id:1,action:{type:'block'},condition:{regexFilter:'a(?=b)'}}]}),/regex|regular expression/i);
  check((await d.getDynamicRules({ruleIds:[1]}))[0].condition.urlFilter==='||never0.invalid^','Invalid regex does not partially remove existing dynamic rule');
 }
 const groupedTabs=await Promise.all([chrome.tabs.create({url:'about:blank'}),chrome.tabs.create({url:'about:blank'})]);
 const groupId=await chrome.tabs.group({tabIds:groupedTabs.map(tab=>tab.id)});
 const group=await chrome.tabGroups.get(groupId);
 check(group.shared===false,'Native local tabGroups expose Chrome shared:false');
 check((await chrome.tabGroups.query({shared:true})).length===0,'Shared query returns no local groups');
 check((await chrome.tabGroups.query({shared:false,windowId:-2})).some(value=>value.id===groupId),'Current-window unshared query returns native group');
 const updated=await chrome.tabGroups.update(groupId,{title:'Chrome native group',color:'red',collapsed:true});
 check(updated.title==='Chrome native group'&&updated.color==='red'&&updated.collapsed,'Native group update affects real group');
 let removed;
 const removal=new Promise(resolve=>{removed=(...args)=>{if(args[0].id===groupId)resolve(args);};chrome.tabGroups.onRemoved.addListener(removed);});
 await chrome.tabs.ungroup(groupedTabs.map(tab=>tab.id));
 const removalArgs=await removal;
 check(removalArgs.length===1&&removalArgs[0].shared===false,'Chrome tabGroups.onRemoved delivers exactly one real group');
 chrome.tabGroups.onRemoved.removeListener(removed);
 await chrome.tabs.remove(groupedTabs.map(tab=>tab.id));
 await chrome.storage.local.set({restart:true});
 report({type:'done',checks,debug,nativeRegex:NATIVE_REGEX});
}catch(error){report({type:'error',error:String(error),stack:error.stack,checks,debug});}})();
'''.replace('ORIGIN', str(server.server_port)).replace('NATIVE_REGEX', 'false' if args.skip_native_regex else 'true')
    fixture = output / 'network.xpi'
    with zipfile.ZipFile(fixture, 'w') as archive:
        archive.writestr('manifest.json', json.dumps({
            'manifest_version': 3, 'name': 'Chrome native network compatibility fixture', 'version': '1',
            'permissions': ['declarativeNetRequest', 'declarativeNetRequestFeedback', 'storage', 'webRequest', 'tabs', 'tabGroups'],
            'host_permissions': ['<all_urls>'], 'background': {'scripts': ['background.js']},
            'content_security_policy': {'extension_pages': "script-src 'self'; object-src 'self'; connect-src http: https:"},
            'browser_specific_settings': {'gecko': {'id': addon_id, 'floorp_chrome_extension': {'schema_version': 1, 'original_id': 'd' * 32}}},
        }))
        archive.writestr('background.js', background)
    ordinary_id = 'firefox-network@example.test'
    ordinary_fixture = output / 'ordinary-network.xpi'
    with zipfile.ZipFile(ordinary_fixture, 'w') as archive:
        archive.writestr('manifest.json', json.dumps({'manifest_version':3, 'name':'Firefox native network interface', 'version':'1',
            'permissions':['declarativeNetRequest', 'tabs', 'tabGroups'], 'background':{'scripts':['background.js']},
            'browser_specific_settings':{'gecko':{'id':ordinary_id}}}))
        archive.writestr('background.js', r"""
          (async()=>{try{
            const d=browser.declarativeNetRequest;
            if(d.MAX_NUMBER_OF_DYNAMIC_RULES!==5000||d.MAX_NUMBER_OF_ENABLED_STATIC_RULESETS!==20)throw Error('Firefox native quota changed');
            if(!(await d.isRegexSupported({regex:'a(?=b)'})).isSupported)throw Error('Firefox RegExp engine changed');
            let rejected=false;
            try{await d.updateDynamicRules({addRules:Array.from({length:5001},(_,i)=>({id:i+1,action:{type:'block'},condition:{urlFilter:'||legacy'+i+'.invalid^'}}))});}catch(error){rejected=/MAX_NUMBER_OF_DYNAMIC_RULES/.test(String(error));}
            if(!rejected)throw Error('Firefox quota enforcement changed');
            const tab=await browser.tabs.create({url:'about:blank'});const id=await browser.tabs.group({tabIds:[tab.id]});
            const group=await browser.tabGroups.get(id);if(Object.hasOwn(group,'shared'))throw Error('Firefox group shape changed');
            await browser.tabs.remove(tab.id);
            document.title=JSON.stringify({type:'ordinary',lookahead:true,quota:5000,sharedAbsent:true});
          }catch(error){document.title=JSON.stringify({type:'error',error:String(error)});}})();
        """)
    profile = output / 'profile'
    profile.mkdir()
    port = runtime.unused_port()
    prefs = {'marionette.port': port, 'browser.shell.checkDefaultBrowser': False,
             'browser.startup.homepage_override.mstone': 'ignore', 'browser.startup.page': 0,
             'browser.aboutwelcome.enabled': False, 'datareporting.policy.dataSubmissionEnabled': False,
             'extensions.autoDisableScopes': 0, 'floorp.releaseNotes.mode': 'disabled',
             'floorp.releaseNotes.choiceConfirmed': True}
    (profile / 'user.js').write_text('\n'.join(f'user_pref({json.dumps(k)}, {json.dumps(v)});' for k, v in prefs.items()) + '\n')
    result = {'passed': False, 'nativeRegexRequired': not args.skip_native_regex, 'checks': [], 'requests': requests}
    client = host = None

    def check(condition, message):
        if not condition:
            raise AssertionError(message)
        result['checks'].append(message)

    def state(extension_id=addon_id):
        value = client.script('''
          const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
          const extension=ExtensionParent.GlobalManager.getExtension(arguments[0]);
          const title=[...(extension?.views||[])].find(view=>view.viewType==='background'&&!view.unloaded)?.xulBrowser?.contentTitle;
          try{return JSON.parse(title);}catch{return null;}
        ''', extension_id)
        result['lastState'] = value
        if value and value.get('type') == 'error':
            raise RuntimeError(value)
        return value

    with (output / 'browser.log').open('w') as log:
        try:
            binary = args.browser.resolve()
            host = subprocess.Popen([str(binary), '--headless', '--no-remote', '--profile', str(profile), '--marionette', '--remote-allow-system-access', 'about:blank'], stdout=log, stderr=subprocess.STDOUT, env={**os.environ, 'MOZ_CRASHREPORTER_DISABLE': '1'})

            def connect():
                if host.poll() is not None:
                    raise RuntimeError(f'Browser exited {host.returncode}')
                try:
                    return runtime.Marionette(port)
                except (ConnectionRefusedError, socket.timeout):
                    return None

            client = runtime.wait_for('Marionette', connect, 45)
            client.context('chrome')
            client.script('''
              window.__networkInstalled={done:false};
              const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');
              const file=Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);file.initWithPath(arguments[0]);
              AddonManager.installTemporaryAddon(file).then(addon=>{window.__networkInstalled={done:true,id:addon.id};},error=>{window.__networkInstalled={done:true,error:String(error)};});
            ''', str(fixture))
            installed = runtime.wait_for('network install', lambda: client.script('return window.__networkInstalled.done?window.__networkInstalled:null;'), 30)
            check(installed.get('id') == addon_id, f'Installed native network fixture: {installed}')
            done = runtime.wait_for('network API and traffic', lambda: value if (value := state()) and value.get('type') == 'done' else None, 90)
            result['extension'] = done
            check(not any(request['path'] == '/blocked' for request in requests), 'Blocked request never reached HTTP server')
            check(any(request['path'] == '/redirected' for request in requests), 'Redirect destination reached HTTP server')
            check(not any(request['path'] == '/redirect-me' for request in requests), 'Redirect source never reached HTTP server')
            check(any(request['headers'].get('x-floorp-dnr') == 'native' for request in requests), 'Modified request header recorded by actual HTTP server')
            client.script('''
              const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');
              window.__networkRestart=false;
              (async()=>{const addon=await AddonManager.getAddonByID(arguments[0]);await addon.disable();await addon.enable();window.__networkRestart=true;})().catch(error=>{window.__networkRestart=String(error);});
            ''', addon_id)
            runtime.wait_for('native add-on restart', lambda: client.script('return window.__networkRestart;'), 30)
            restarted = runtime.wait_for('persistent dynamic rules', lambda: value if (value := state()) and value.get('type') == 'restarted' else None, 45)
            result['restart'] = restarted
            check(len(restarted['checks']) == 2, 'Dynamic persistence and session reset verified in restarted extension')
            client.script("""
              window.__ordinaryNetwork={done:false};
              const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');
              const file=Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);file.initWithPath(arguments[0]);
              AddonManager.installTemporaryAddon(file).then(addon=>{window.__ordinaryNetwork={done:true,id:addon.id};},error=>{window.__ordinaryNetwork={done:true,error:String(error)};});
            """, str(ordinary_fixture))
            ordinary_install = runtime.wait_for('Firefox network fixture install', lambda: client.script('return window.__ordinaryNetwork.done?window.__ordinaryNetwork:null;'), 30)
            check(ordinary_install.get('id') == ordinary_id, 'Installed ordinary Firefox network fixture')
            ordinary = runtime.wait_for('Firefox native interfaces', lambda: value if (value := state(ordinary_id)) and value.get('type') == 'ordinary' else None, 30)
            result['ordinary'] = ordinary
            check(ordinary['lookahead'] and ordinary['quota'] == 5000 and ordinary['sharedAbsent'], 'Ordinary Firefox regex, quota and tab-group shape preserved')
            result['passed'] = True
        except Exception as error:
            result['error'] = str(error)
            if client:
                try:
                    result['console'] = client.script('return Services.console.getMessageArray().slice(-40).map(message=>message.message);')
                except Exception:
                    pass
        finally:
            if client:
                client.close()
            if host:
                host.terminate()
                try:
                    host.wait(timeout=8)
                except subprocess.TimeoutExpired:
                    host.kill()
                    host.wait(timeout=5)
            server.shutdown()
            server.server_close()
    (output / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    print(output / 'report.json')
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
