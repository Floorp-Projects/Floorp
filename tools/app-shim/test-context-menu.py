#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Check trusted PWA context-menu delivery to a remote content process."""

import argparse
import http.server
import importlib.util
import json
from pathlib import Path
import plistlib
import tempfile
import threading
import uuid

PROJECT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('helpers', PROJECT / 'tools/app-shim/test-runtime.py')
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
STATE = 'Services.appShell.hiddenDOMWindow.__pwaContextMenuProbe'


class Page(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = b'''<!doctype html><meta charset=utf-8><title>PWA context menu probe</title>
<style>body{margin:0;font:20px system-ui}aside{padding:60px;width:300px}
a{display:block;padding:30px;background:#cde}</style>
<aside><a id=chat href=#chat>Test conversation</a></aside><main id=menu hidden>Custom menu</main>
<script>window.probe={events:[]};
for(const type of ['mousedown','mouseup','contextmenu'])document.querySelector('#chat').addEventListener(type,e=>{
probe.events.push({type:e.type,constructor:e.constructor.name,button:e.button,buttons:e.buttons,
pointerType:e.pointerType,pointerId:e.pointerId,isTrusted:e.isTrusted});
if(e.type==='contextmenu'){e.preventDefault();document.querySelector('#menu').hidden=false;}});</script>'''
        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--browser', type=Path, required=True, help='Source-built, signed test Runtime .app')
    parser.add_argument('--output', type=Path, default=Path('_dist/app-shim-context-menu'))
    parser.add_argument('--iterations', type=int, default=20)
    args = parser.parse_args()
    if args.iterations < 1:
        parser.error('--iterations must be positive')
    browser = args.browser.resolve(strict=True)
    h.run('/usr/bin/codesign', '--verify', '--strict', str(browser))
    args.output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='run-', dir=args.output.resolve()))
    profile = output / 'profile'
    profile.mkdir(mode=0o700)
    exe = browser / 'Contents/MacOS' / plistlib.loads((browser / 'Contents/Info.plist').read_bytes())['CFBundleExecutable']
    port = h.unused_port()
    prefs = {'marionette.port':port,'browser.shell.checkDefaultBrowser':False,'browser.aboutwelcome.enabled':False,
             'browser.startup.homepage_override.mstone':'ignore','browser.startup.page':0,'browser.startup.homepage':'about:blank',
             'floorp.browser.nativeApp.appShim.enabled':True,'browser.sessionstore.resume_from_crash':False,
             'app.update.enabled':False,'datareporting.policy.dataSubmissionEnabled':False}
    (profile / 'user.js').write_text('\n'.join(f'user_pref({json.dumps(k)}, {json.dumps(v)});' for k,v in prefs.items())+'\n')
    report = {'status':'failed','browser':str(browser),'output':str(output),'requestedIterations':args.iterations}
    app_id = '{'+str(uuid.uuid4())+'}'
    host = client = server = None
    log = (output / 'browser.log').open('w')
    code = 1
    try:
        host = h.HostProcess(browser, exe, profile, output, log, launch_services=True)
        def connect():
            if host.poll() is not None:raise RuntimeError('Test host exited')
            try:return h.Marionette(port)
            except OSError:return None
        client = h.wait_for('Marionette', connect, 35)
        host.verify_session(client.session)
        client.context('chrome')
        identity = json.loads(client.script(f'return {h.SERVICE}.hostIdentityJSON;'))
        profile_id = 'context-menu-'+uuid.uuid4().hex
        bundle = h.seal_bundle(output,browser/'Contents/MacOS/floorp-app-shim',identity,profile,app_id,profile_id)
        client.script(f'''{STATE}={{events:[],failures:[],crashes:[]}};
{STATE}.observer={{observe(s,t,d){{if(t==='floorp-web-app-shim-event'){{const e=JSON.parse(d);
if(e.type!==111){STATE}.events.push(e);}}else if(t==='oop-frameloader-crashed'){{
{STATE}.crashes.push({{time:Date.now(),data:d}});}}else {STATE}.failures.push(d);}}}};
for(const t of ['floorp-web-app-shim-event','floorp-web-app-presentation-failed','oop-frameloader-crashed'])Services.obs.addObserver({STATE}.observer,t);
{h.SERVICE}.configure(arguments[1]);{h.SERVICE}.registerApp(arguments[0],arguments[2]);{h.SERVICE}.launchApp(arguments[0]);''',app_id,profile_id,str(bundle))
        connected = h.wait_for('Shim authentication',lambda:next((e for e in client.script(f'return {STATE}.events;') if e['type']=='connected'),None),20)
        server = http.server.ThreadingHTTPServer(('127.0.0.1',0),Page)
        threading.Thread(target=server.serve_forever,daemon=True).start()
        client.script(f'''const {{BrowserWindowTracker}}=ChromeUtils.importESModule('resource:///modules/BrowserWindowTracker.sys.mjs');
const args=Cc['@mozilla.org/supports-string;1'].createInstance(Ci.nsISupportsString);args.data=arguments[1];
{h.SERVICE}.withWindowContext(arguments[0],{{createWindow(){{{STATE}.window=BrowserWindowTracker.openWindow(
{{args,features:'width=900,height=700',remote:true,fission:true}});return {STATE}.window;}}}});''',app_id,f'http://127.0.0.1:{server.server_port}/')
        created = h.wait_for('Native PWA window',lambda:next((e for e in client.script(f'return {STATE}.events;') if e['type']==101 and e['payload'].get('kind')=='created'),None),20)
        root_id = created['payload']['windowId']
        handle = h.wait_for('PWA browsing context',lambda:client.script(f'''const w={STATE}.window;
if(!w?.gBrowserInit?.delayedStartupFinished)return null;
const {{NavigableManager}}=ChromeUtils.importESModule('chrome://remote/content/shared/NavigableManager.sys.mjs');
return NavigableManager.getIdForBrowser(w.gBrowser.selectedBrowser);'''),20)
        client.switch(handle)
        client.context('content')
        h.wait_for('Fixture',lambda:client.script('return !!window.probe;'),20)
        rect = client.script('const r=document.querySelector("#chat").getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};')
        client.context('chrome')
        geometry = client.script(f'const b={STATE}.window.gBrowser.selectedBrowser;const r=b.getBoundingClientRect();return {{x:r.x,y:r.y,remote:b.isRemoteBrowser,remoteType:b.remoteType,pid:b.frameLoader.remoteTab.osPid}};')
        if not geometry['remote']:raise RuntimeError('Fixture must use a remote content process')
        report.update({'hostPid':host.pid,'shimPid':connected['payload']['pid'],'contentProcess':geometry,'results':[]})
        for iteration in range(args.iterations):
            client.context('chrome')
            for kind in ('mouseDown','mouseUp'):
                packet = {'windowId':root_id,'kind':kind,'x':geometry['x']+rect['x'],'y':geometry['y']+rect['y'],'modifiers':0,'button':1,'clickCount':1}
                client.script("Services.obs.notifyObservers(null,'floorp-web-app-shim-event',JSON.stringify({appId:arguments[0],type:100,payload:arguments[1]}));",app_id,packet)
            def delivered():
                client.context('chrome')
                state = client.script(f'return {{crashes:{STATE}.crashes,failures:{STATE}.failures}};')
                report['nativeState'] = state
                if state['crashes']:raise RuntimeError('Remote content process crashed after right click')
                client.context('content')
                events = client.script('return window.probe?.events;')
                if events and sum(e['type']=='contextmenu' for e in events)==iteration+1:return events
                return None
            report['results'] = h.wait_for('Trusted contextmenu delivery',delivered,8)
        client.context('content')
        report['customMenuVisible'] = client.script('return !document.querySelector("#menu").hidden;')
        if not report['customMenuVisible']:
            raise RuntimeError('Page context-menu handler did not open its custom menu')
        client.context('chrome')
        final = client.script(f'const b={STATE}.window.gBrowser.selectedBrowser;return {{pid:b.frameLoader.remoteTab.osPid,crashes:{STATE}.crashes,failures:{STATE}.failures,closed:{STATE}.window.closed}};')
        report['finalState'] = final
        contexts = [e for e in report['results'] if e['type']=='contextmenu']
        if len(contexts)!=args.iterations or any(e['constructor']!='PointerEvent' or e.get('pointerType')!='mouse' or e['button']!=2 or not e['isTrusted'] for e in contexts):
            raise RuntimeError('Contextmenu must be a trusted mouse PointerEvent')
        if final['crashes'] or final['failures'] or final['closed'] or final['pid']!=geometry['pid']:
            raise RuntimeError('PWA or its content process did not survive')
        report['status']='passed'
        code=0
    except Exception as error:
        report['error']=str(error)
        if client:
            try:
                client.context('chrome')
                report['nativeState']=client.script(f'return {{crashes:{STATE}?.crashes,failures:{STATE}?.failures}};')
            except Exception as inner:report['stateError']=str(inner)
    finally:
        if client:
            try:
                client.context('chrome')
                client.script(f'''for(const t of ['floorp-web-app-shim-event','floorp-web-app-presentation-failed','oop-frameloader-crashed'])Services.obs.removeObserver({STATE}.observer,t);
if({STATE}?.window&&!{STATE}.window.closed){STATE}.window.close();{h.SERVICE}.sendControl(arguments[0],40,'{{}}');''',app_id)
            except Exception as error:
                report['cleanupError']=str(error)
                report['status']='failed'
                code=1
            client.close()
        if server:server.shutdown();server.server_close()
        if host:host.close()
        log.close()
        (output/'result.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))
    return code


if __name__=='__main__':
    raise SystemExit(main())
