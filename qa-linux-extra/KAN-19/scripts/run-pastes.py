from pathlib import Path
import json,time,sys
from control import send
r=Path(__file__).parent;c=sys.argv[1];out=r/'trials'/c;url='https://example.org/kan19/status/123/video/1';rows=[]
def call(id,**kw):
 x=send(c,id,**kw);assert 'errorType' not in x,(id,x);return x['value']
def clip(id,plain,html=None):
 if html is None:script="Cc['@mozilla.org/widget/clipboardhelper;1'].getService(Ci.nsIClipboardHelper).copyString("+json.dumps(plain)+");return {writer:'nativeClipboardHelper',plainOnly:true};"
 else:script="const t=Cc['@mozilla.org/widget/transferable;1'].createInstance(Ci.nsITransferable);t.init(null);for(const [type,value] of "+json.dumps([['text/plain',plain],['text/html',html]])+"){t.addDataFlavor(type);let s=Cc['@mozilla.org/supports-string;1'].createInstance(Ci.nsISupportsString);s.data=value;t.setTransferData(type,s);}Services.clipboard.setData(t,null,Ci.nsIClipboard.kGlobalClipboard);return {writer:'nativeTransferable',flavors:['text/plain','text/html']};"
 return call(id,script=script)
call('dismiss-standard-prompt',nativeKeys=['Escape'])
# Select existing fixture tab, no reload or product handler replacement.
call('fixture-ready',target='index.html',context='content',script="return {ready:window.wrappedJSObject.qaReady,title:document.title};")
for target in ['plain','rooster']:
 for scenario in ['plain','plain-shift','rich-visible','rich-shift','textarea-recopy','subject-recopy','opacity-control']:
  id=target+'-'+scenario;call(id+'-reset',target='index.html',context='content',script="return window.wrappedJSObject.qaStart("+json.dumps(id)+','+json.dumps(target)+");")
  if scenario in ['textarea-recopy','subject-recopy']:
   source='notepad' if scenario=='textarea-recopy' else 'subject'
   call(id+'-source',target='index.html',context='content',click='#'+source,script='return true;');call(id+'-copy',nativeKeys=['ctrl+a','ctrl+c']);sourceData={'writer':'nativeCtrlC','sourceElement':source,'notWindowsNotepad':True}
  else:
   html=None
   if scenario.startswith('rich'):html='<a href="'+url+'">'+url+'</a>'
   if scenario=='opacity-control':html='<span style="opacity:0;white-space:pre-wrap;user-select:auto"><a href="'+url+'">'+url+'</a></span>'
   sourceData=clip(id+'-clipboard',url,html)
  call(id+'-focus',target='index.html',context='content',click='#'+target,script='return true;');key='ctrl+shift+v' if scenario in ['plain-shift','rich-shift'] else 'ctrl+v';call(id+'-paste',nativeKeys=[key]);time.sleep(.35)
  snapshots=[]
  for suffix,delay in [('settled',0),('later',.65)]:
   if delay:time.sleep(delay)
   value=call(id+'-'+suffix,target='index.html',context='content',script='return window.wrappedJSObject.qaSnapshot();');snapshots.append(value)
  row={'id':id,'source':sourceData,'nativeKey':key,'explicitOpacityControl':scenario=='opacity-control','snapshots':snapshots};rows.append(row);(out/'paste-results.json').write_text(json.dumps(rows,indent=2)+'\n');v=snapshots[-1];events=[x for x in v['log'] if x['type']=='paste'];print(id,'textlen',len(v['text'] or ''),'anchors',v['anchors'],'opacity0',v['opacityZero'],'types',[x['types'] for x in events],flush=True);assert events and all(x['trusted'] for x in events),row;assert v['text']==url,row
call('final-screenshot',target='index.html',context='content',script='return window.wrappedJSObject.qaSnapshot();',screenshot='fixture-final.png')
call('product-final');print(send(c,'quit',stop=True),flush=True)
