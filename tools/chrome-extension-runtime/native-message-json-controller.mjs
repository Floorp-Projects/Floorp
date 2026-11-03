// SPDX-License-Identifier: MPL-2.0
export const messageControllerSource = `
const ownedControlName='floorp-owned-message-json-control';
const ownedPause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function ownedMessageCommand(request){
 const a=request.args||[],operation=request.operation;
 if(operation==='json-arm')return(messageJSONOracle.armMessage(...a),true);
 if(operation==='json-arm-response')return(messageJSONOracle.armResponse(...a),true);
 if(operation==='json-arm-port-response')return(messageJSONOracle.armPortResponse(...a),true);
 if(operation==='json-send')return messageJSONOracle.send(...a);
 if(operation==='json-port')return messageJSONOracle.port(...a);
 if(operation==='json-response-request')return messageJSONOracle.sendResponseRequest(...a);
 if(operation==='json-messages')return messageJSONOracle.getMessages();
 if(operation==='json-ports')return messageJSONOracle.getPorts();
 if(operation==='overload')return runOverloadCase(...a);
 if(operation==='overload-receiver')return(setOverloadReceiverEnabled(...a),true);
 if(operation==='edge-run')return runResponseHook(...a);
 if(operation==='edge-options')return runOptionHook(...a);
 if(operation==='edge-port')return runPortHook(...a);
 if(operation==='edge-read'){
  const end=Date.now()+3000;
  while(Date.now()<end){const result=getResponseHook(...a);if(result)return result;await ownedPause(10);}
  throw Error('Edge receiver did not restore its prototype');
 }
 if(operation==='edge-read-port')return getPortHook(...a);
 if(operation==='ordinary')return ordinaryStructuredClone();
 throw Error('Unknown owned command '+operation);
}
`;
export const messageWorkerControllerSource = `
chrome.runtime.onConnect.addListener(port=>{
 if(port.name!==ownedControlName)return;
 port.onMessage.addListener(async request=>{
  try{const value=await ownedMessageCommand(request);port.postMessage({requestId:request.requestId,value});}
  catch(error){port.postMessage({requestId:request.requestId,error:String(error)});}
 });
 port.postMessage({ready:true,runtimeId:chrome.runtime.id,realm:typeof document==='undefined'?'worker':'document',moduleProof:typeof ownedModuleProof==='string'});
});
`;
export const messageDocumentControllerSource = `
const ownedControlPort=chrome.runtime.connect({name:ownedControlName}),ownedPending=new Map();
let ownedControlError='';
ownedControlPort.onMessage.addListener(reply=>{
 if(reply.ready){document.title=JSON.stringify({ready:true,worker:reply});return;}
 const pending=ownedPending.get(reply.requestId);if(!pending)return;ownedPending.delete(reply.requestId);
 if(reply.error)pending.reject(Error(reply.error));else pending.resolve(reply.value);
});
ownedControlPort.onDisconnect.addListener(()=>{
 ownedControlError=chrome.runtime.lastError?.message||'Owned Worker control disconnected';
 for(const pending of ownedPending.values())pending.reject(Error(ownedControlError));ownedPending.clear();
});
window.addEventListener('message',async event=>{
 const request=event.data;if(request?.target!=='floorp-owned-message-json')return;
 try{
  let value;
  if(request.mode==='worker'){
   if(ownedControlError)throw Error(ownedControlError);
   value=await new Promise((resolve,reject)=>{ownedPending.set(request.requestId,{resolve,reject});ownedControlPort.postMessage(request);});
  }else value=await ownedMessageCommand(request);
  document.title=JSON.stringify({requestId:request.requestId,value});
 }catch(error){document.title=JSON.stringify({requestId:request.requestId,error:String(error)});}
});
`;
export const ordinaryMessageSource = `
let ordinaryHooks=0;
const ordinarySnapshot=value=>({date:Object.prototype.toString.call(value.date),iso:value.date.toISOString(),map:Object.prototype.toString.call(value.map),entries:[...value.map],set:Object.prototype.toString.call(value.set),setValues:[...value.set],typed:Object.prototype.toString.call(value.typed),typedValues:[...value.typed],bigint:typeof value.bigint,bigintText:String(value.bigint),undefinedPresent:Object.hasOwn(value,'missing')&&value.missing===undefined,negativeZero:Object.is(value.negativeZero,-0),cycle:value.cycle.self===value.cycle,receiverRealm:value.receiverRealm});
browser.runtime.onMessage.addListener((value,sender,respond)=>{if(value?.ordinaryClone){value.receiverRealm=typeof document==='object'?'document':'worker';respond(value);return undefined;}});
browser.runtime.onConnect.addListener(port=>{if(port.name==='floorp-ordinary-structured-clone')port.onMessage.addListener(value=>{value.receiverRealm=typeof document==='object'?'document':'worker';port.postMessage(value);});});
async function ordinaryStructuredClone(){
 const cycle={};cycle.self=cycle;
 const value={ordinaryClone:true,date:new Date('2026-10-19T12:34:56.000Z'),map:new Map([['key','value']]),set:new Set(['owned']),typed:new Uint8Array([3,7,255]),bigint:12n,missing:undefined,negativeZero:-0,cycle};
 Object.defineProperty(value,'toJSON',{value(){ordinaryHooks++;return'wrong-json-path';}});
 const response=await browser.runtime.sendMessage(value);
 const portResponse=await new Promise((resolve,reject)=>{const port=browser.runtime.connect({name:'floorp-ordinary-structured-clone'}),timer=setTimeout(()=>{port.disconnect();reject(Error('Ordinary structured clone Port timed out'));},3000);port.onMessage.addListener(reply=>{clearTimeout(timer);port.disconnect();resolve(reply);});port.onDisconnect.addListener(()=>{const error=browser.runtime.lastError;if(error){clearTimeout(timer);reject(Error(error.message));}});port.postMessage(value);});
 return{realm:'document',runtimeId:browser.runtime.id,hooks:ordinaryHooks,message:ordinarySnapshot(response),port:ordinarySnapshot(portResponse)};
}
window.addEventListener('message',async event=>{const request=event.data;if(request?.target!=='floorp-owned-message-json')return;try{document.title=JSON.stringify({requestId:request.requestId,value:await ordinaryStructuredClone()});}catch(error){document.title=JSON.stringify({requestId:request.requestId,error:String(error)});}});
document.title='owned-ordinary-json-control-ready';
`;
