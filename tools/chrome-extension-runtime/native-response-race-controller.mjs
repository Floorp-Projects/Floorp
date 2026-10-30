// SPDX-License-Identifier: MPL-2.0
export const messageControllerSource = `
const ownedControlName='floorp-owned-message-json-control';
async function ownedMessageCommand(request){
 const a=request.args||[];
 if(request.operation==='race-run')return runResponseRace(...a);
 if(request.operation==='race-read')return getResponseRace(...a);
 throw Error('Unknown owned response race command '+request.operation);
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
