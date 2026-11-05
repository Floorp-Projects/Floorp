import pathlib,sys,json,time
p=pathlib.Path(__file__).parent;d={'id':sys.argv[1],'script':sys.stdin.read() or 'return true;'}
for a in sys.argv[2:]:
 if a=='--async':d['async']=True
 else:
  k,v=a.lstrip('-').split('=',1);d[k]=int(v) if k=='select' else v
(p/('request-'+d['id']+'.json')).write_text(json.dumps(d,indent=2));(p/'control.json').write_text(json.dumps(d));r=p/('result-'+d['id']+'.json')
for i in range(200):
 if r.exists():print(r.read_text()[:3500]);break
 time.sleep(.1)
else:print('pending',d['id'])
