import sys,os,json
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[2]);m=Marionette('localhost',port=port,socket_timeout=20,startup_timeout=15);m.start_session();m.set_context('chrome')
variant='baseline' if os.environ.get('QA_COMBO_BASELINE') else 'patched'
code=(R/f'KAN-98/combination-source/{variant}.js').read_text()
r=m.execute_script(code+';try {qaComboTest.runCombinationRegression();return {passed:true};}catch(e){return {passed:false,error:String(e),stack:e.stack};}')
(R/f'KAN-98/combination-source-{variant}-{port}.json').write_text(json.dumps(r,indent=2));print(json.dumps(r));m.delete_session()
