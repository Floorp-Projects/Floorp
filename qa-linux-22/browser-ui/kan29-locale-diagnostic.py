import os,sys,json,time,socket,subprocess,signal,traceback
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[1]);profile=R/'runtime'/f'profile-kan29-ar-{port}';profile.mkdir(exist_ok=False)
design=json.loads(json.loads((R/'inspect-29284.json').read_text())['observations'][0]['design']);design['tabbar']['tabbarStyle']='vertical'
prefs={'marionette.port':port,'browser.shell.checkDefaultBrowser':False,'floorp.tabstacks.enabled':True,'browser.aboutwelcome.enabled':False,'browser.tabs.splitView.enabled':True,'sidebar.revamp':True,'sidebar.verticalTabs':True,'sidebar.visibility':'expand-on-hover','floorp.design.configs':json.dumps(design),'floorp.panelSidebar.config':json.dumps({'globalWidth':400,'autoUnload':False,'position_start':True,'displayed':True,'webExtensionRunningEnabled':False,'overlay':False,'openOnHover':False})}
(profile/'user.js').write_text(''.join('user_pref('+json.dumps(k)+','+json.dumps(v)+');\n'for k,v in prefs.items()))
env=os.environ.copy();env.update(MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',PYTHONPATH=str(R/'python'),XDG_CACHE_HOME=str(R/'runtime/cache'),XDG_CONFIG_HOME=str(R/'runtime/config'));assert 'MOZ_DISABLE_CONTENT_SANDBOX' not in env
result={'languagePackUrl':'https://archive.mozilla.org/pub/firefox/releases/159.0.1/linux-x86_64/xpi/ar.xpi','fixture':'Normal signed-language-pack install via AddonManager API and restart; no signature/security prefs changed.'}
def start(stage):
 log=open(R/f'KAN-29/locale/{stage}-{port}.log','w')
 p=subprocess.Popen(['dbus-run-session','--',str(R/'products/floorp/floorp'),'--no-remote','--profile',str(profile),'--marionette','about:blank'],env=env,stdout=log,stderr=log,start_new_session=True)
 for i in range(120):
  if p.poll() is not None:raise RuntimeError('browser exited '+str(p.returncode))
  try:s=socket.create_connection(('127.0.0.1',port),.3);s.close();break
  except OSError:time.sleep(.2)
 else:raise RuntimeError('no Marionette')
 time.sleep(3);return p

def stop(p):
 try:os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=10)
 except ProcessLookupError:pass
 except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL)
p=None
try:
 p=start('install');m=Marionette('localhost',port=port,socket_timeout=20);m.start_session();m.set_context('chrome')
 result['install']=m.execute_async_script('''const done=arguments[arguments.length-1];const {AddonManager}=ChromeUtils.importESModule("resource://gre/modules/AddonManager.sys.mjs");const {FileUtils}=ChromeUtils.importESModule("resource://gre/modules/FileUtils.sys.mjs");(async()=>{const i=await AddonManager.getInstallForFile(new FileUtils.File(arguments[0]));await i.install();const a=i.addon;Services.locale.requestedLocales=['ar'];Services.prefs.savePrefFile(null);done({id:a.id,type:a.type,isActive:a.isActive,signedState:a.signedState,requested:Services.locale.requestedLocales,signaturePrefUser:Services.prefs.prefHasUserValue('extensions.langpacks.signatures.required')});})().catch(e=>done({error:String(e)}));''',script_args=[str(R/'KAN-29/locale/ar.xpi')],script_timeout=60000)
 m.delete_session();time.sleep(1);stop(p);p=None
 if result['install'].get('error'):raise RuntimeError(result['install']['error'])
 p=start('arabic');env['QA_DIR']='rtl'
 m=Marionette('localhost',port=port,socket_timeout=20);m.start_session();m.set_context('chrome')
 result['postRestart']=m.execute_async_script('''const done=arguments[arguments.length-1];const {AddonManager}=ChromeUtils.importESModule("resource://gre/modules/AddonManager.sys.mjs");(async()=>{let packs=await AddonManager.getAddonsByTypes(['locale']);done({requested:Services.locale.requestedLocales,available:Services.locale.availableLocales,app:Services.locale.appLocalesAsBCP47,rtl:Services.locale.isAppLocaleRTL,RTL_UI:window.RTL_UI,dir:getComputedStyle(document.documentElement).direction,packs:packs.map(a=>({id:a.id,active:a.isActive,disabled:a.appDisabled,userDisabled:a.userDisabled,signedState:a.signedState})),errors:Services.console.getMessageArray().map(m=>m.message).filter(s=>/langpack|locale|l10n|languagepack/i.test(s)).slice(-25)});})().catch(e=>done({error:String(e)}));''')
 m.delete_session()
except Exception as e:result['error']=repr(e);result['traceback']=traceback.format_exc()
finally:
 if p is not None:stop(p)
 (R/f'KAN-29/locale/setup-{port}.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
