"""Execute the colocated browser regression bundle on an isolated product."""
import json
import os
import sys
import traceback
from pathlib import Path
from marionette_driver.marionette import Marionette

ROOT = Path(__file__).resolve().parent.parent
port = int(sys.argv[2])
m = Marionette('localhost', port=port, socket_timeout=20)
m.start_session()
m.set_context('chrome')
result = {'scenario': 'regression', 'observations': []}
try:
    result['product'] = m.execute_script('return {name:Services.appinfo.name,version:Services.appinfo.version,build:Services.appinfo.appBuildID,os:Services.appinfo.OS}')
    result['graphics'] = m.execute_async_script("const done=arguments[arguments.length-1];const {Troubleshoot}=ChromeUtils.importESModule('resource://gre/modules/Troubleshoot.sys.mjs');Troubleshoot.snapshot().then(s=>done({windowProtocol:s.graphics.windowProtocol,compositing:s.graphics.windowLayerManagerType}));")
    result['fixture'] = {'bundle':os.environ.get('QA_REGRESSION_JS','regression-new.js'),'cssOverride':True,'adoption':'gBrowser API, not menu or physical drag'}
    bundle = (ROOT / 'KAN-3' / os.environ.get('QA_REGRESSION_JS', 'regression-new.js')).read_text()
    m.execute_script(bundle + ';window.qaKan3Test=qaKan3Test;')
    result['observations'].append(m.execute_async_script('''
      const done=arguments[arguments.length-1];
      window.qaKan3Test.runAllTests().then(
        ()=>done({passed:true}),
        error=>done({passed:false,error:String(error),stack:error.stack})
      );
    ''', script_timeout=40000))
except Exception as error:
    result.update(error=str(error), traceback=traceback.format_exc())
finally:
    m.delete_session()
    (ROOT / ('regression-' + str(port) + '.json')).write_text(json.dumps(result, indent=2))
    print(json.dumps(result))
