import pathlib,json,re
p=pathlib.Path(__file__).parent
read=lambda n:json.loads((p/n).read_text())
assert read('result-normal-candidate.json')['filter']=='###picker-normal'
assert '###picker-panel' in read('result-panel-candidate.json')['text']
assert not read('result-panel-save.json')['disabled']
for f in ['result-reload-result.json','result-restart-result.json']:
 d=read(f);assert len(d)==2
 for page in d:assert page['normal']=='none' and page['panel']=='none' and page['control']=='block'
s=read('result-saved-filter-list.json')['filterText'];assert '127.0.0.1###picker-normal' in s and '127.0.0.1###picker-panel' in s
normal=read('result-twp-normal-result.json')['content']['text'];panel=read('result-twp-panel-refreshed-result.json')['content']['text'];assert normal==panel and normal.startswith('El hermoso jardín')
meta=read('result-final-metadata.json');assert meta['protections']=={'contentSandbox':6,'signatureRequired':True}
assert all(a['signedState']==2 and a['active'] and not a['appDisabled'] for a in meta['extensions'])
assert any(x.get('status')==200 and x['origin']=='https://translate-pa.googleapis.com' for x in meta['network'])
assert read('cleanup.json')['remainingOwnedProcesses']==[]
(p/'verification.json').write_text(json.dumps({'passed':True,'uBO':'actual generated filters/save/reload/restart assertions pass for both','TWP':'actual Spanish text matches across tab/panel, service200','protections':'sandbox6/signaturesRequiredtrue/both signedState2','cleanup':'no owned processes'},indent=2));print('all evidence assertions pass')
