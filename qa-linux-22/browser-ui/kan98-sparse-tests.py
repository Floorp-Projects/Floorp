import sys,os,json
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[2]);m=Marionette('localhost',port=port,socket_timeout=20,startup_timeout=15);m.start_session();m.set_context('chrome')
variant='original' if os.environ.get('QA_SPARSE_ORIGINAL') else 'patched'
code=(R/f'KAN-98/sparse-source-test/{variant}.js').read_text()
r=m.execute_async_script(code+';qaSparseTests.runAllTests().then(()=>arguments[arguments.length-1]({passed:true,progress:globalThis.__NORA_TEST_PROGRESS__})).catch(e=>arguments[arguments.length-1]({passed:false,error:String(e),stack:e.stack,progress:globalThis.__NORA_TEST_PROGRESS__}));',script_timeout=90000)
(R/f'KAN-98/sparse-tests-{variant}-{port}.json').write_text(json.dumps(r,indent=2));print(json.dumps(r));m.delete_session()
