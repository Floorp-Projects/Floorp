// Read-only observers for real product and signed extension; no event fabrication/filtering.
window.qaState = (w = window) => {
 const {ExtensionParent}=ChromeUtils.importESModule('resource://gre/modules/ExtensionParent.sys.mjs');
 const tracker=ExtensionParent.apiManager.global.tabTracker;
 const id=t=>tracker.getId(t);
 const gb=w.gBrowser;
 return {at:Date.now(),workspace:w.workspacesFuncs?.getSelectedWorkspaceID(),selected:id(gb.selectedTab),nativeVisible:Array.from(gb.visibleTabs).map(id),nativeNonHidden:Array.from(gb.tabs).filter(t=>!t.hidden).map(id),tabs:Array.from(gb.tabs).map(t=>{const r=t.getBoundingClientRect(),s=w.getComputedStyle(t);return {id:id(t),url:t.linkedBrowser.currentURI.spec,workspace:t.getAttribute('floorpWorkspaceId'),hidden:t.hidden,visible:t.visible,selected:t.selected,pinned:t.pinned,display:s.display,visibility:s.visibility,width:r.width,height:r.height,cssWorkspaceHidden:t.hasAttribute('data-qa-workspace-hidden'),hiddenBy:SessionStore.getCustomTabValue(t,'hiddenBy'),workspaceOwned:SessionStore.getCustomTabValue(t,'floorpQaWorkspaceHidden')};})};
};
window.qaFrameRead=(type,body)=>new Promise(resolve=>{
 const policy=WebExtensionPolicy.getByID('simple-tab-groups@drive4ik');
 if(!policy?.extension){resolve({extensionInactive:true});return;}
 const view=Array.from(policy.extension.views).find(v=>v.viewType===type&&(type!=='tab'||v.uri.spec.includes('/manage/manage.html')));
 if(!view){resolve({missingView:type});return;}
 const mm=view.xulBrowser.messageManager;
 const name='QAKAN102:read:'+Math.random();
 mm.addMessageListener(name,function f(m){mm.removeMessageListener(name,f);resolve(m.data)});
 const script='(async()=>{try{const value=await (async()=>{'+body+'})();sendAsyncMessage('+JSON.stringify(name)+',JSON.parse(JSON.stringify(value)));}catch(e){sendAsyncMessage('+JSON.stringify(name)+',{error:String(e)});}})();';
 mm.loadFrameScript('data:application/javascript,'+encodeURIComponent(script),false);
});
window.qaExt=()=>qaFrameRead('background',`const b=content.wrappedJSObject.browser;const windows=await b.windows.getAll();const tabs=await b.tabs.query({});return {windows:await Promise.all(windows.map(async w=>({id:w.id,focused:w.focused,group:await b.sessions.getWindowValue(w.id,'groupId')}))),tabs:await Promise.all(tabs.map(async t=>({id:t.id,windowId:t.windowId,url:t.url,hidden:t.hidden,active:t.active,pinned:t.pinned,group:await b.sessions.getTabValue(t.id,'groupId')})))};`);
window.qaManage=()=>qaFrameRead('tab',`return {groups:Array.from(content.document.querySelectorAll('.group')).slice(0,2).map(e=>({title:e.querySelector('input')?.value,count:e.querySelector('.tabs-count')?.textContent})),fixtureRows:Array.from(content.document.querySelectorAll('.tab[title*="http://127.0.0.1"]')).map(e=>({title:e.title,text:e.textContent}))};`);
window.qaEvents=[];
for(const type of ['TabHide','TabShow','TabSelect'])gBrowser.tabContainer.addEventListener(type,e=>qaEvents.push({type,at:Date.now(),state:qaState()}));
return {ready:true,state:qaState()};
