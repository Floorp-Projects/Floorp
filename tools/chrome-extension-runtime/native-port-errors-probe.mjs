// SPDX-License-Identifier: MPL-2.0
// Shared public extension code, executed unchanged in document and Worker realms.
export const nativePortErrorsProbeSource = `
chrome.runtime.onConnect.addListener(port=>{
 if(port.name==='floorp-owned-normal-port-error-scope')port.disconnect();
});
function nativePortErrorSnapshot(port,actual,listener,phase){
 const error=chrome.runtime.lastError;
 const portError=port.error;
 return {listener,phase,samePort:actual===port,lastError:error?{message:error.message,keys:Object.keys(error).sort(),isError:error instanceof Error,nameType:typeof error.name}:null,portError:portError?{message:portError.message,name:portError.name}:null};
}
async function probeNativePortErrors(request){
 const before=chrome.runtime.lastError?.message??null,records=[],microtasks=[],turns=[];
 let throwReached=false;
 await new Promise((resolve,reject)=>{
  const port=request.normal?chrome.runtime.connect({name:'floorp-owned-normal-port-error-scope'}):chrome.runtime.connectNative(request.hostName);
  const timer=setTimeout(()=>{port.disconnect();reject(Error('Owned native port error dispatch timed out'));},10000);
  port.onMessage.addListener(()=>{clearTimeout(timer);port.disconnect();reject(Error('Denied native port unexpectedly replied'));});
  port.onDisconnect.addListener(actual=>{
   records.push(nativePortErrorSnapshot(port,actual,1,'callback'));
   queueMicrotask(()=>microtasks.push(nativePortErrorSnapshot(port,actual,1,'microtask')));
   if(request.throwFirst){throwReached=true;throw Error('Owned first disconnect listener throws');}
  });
  port.onDisconnect.addListener(actual=>{
   records.push(nativePortErrorSnapshot(port,actual,2,'callback'));
   queueMicrotask(()=>microtasks.push(nativePortErrorSnapshot(port,actual,2,'microtask')));
   setTimeout(()=>{turns.push(nativePortErrorSnapshot(port,actual,2,'next-turn'));clearTimeout(timer);resolve();},0);
  });
  if(!request.normal)port.postMessage({token:request.token,step:'forbidden-native-port'});
 });
 return {isWorker:typeof document==='undefined',runtimeId:chrome.runtime.id,before,normal:request.normal===true,throwFirst:request.throwFirst===true,throwReached,records,microtasks,turns,after:chrome.runtime.lastError?.message??null};
}
`;
