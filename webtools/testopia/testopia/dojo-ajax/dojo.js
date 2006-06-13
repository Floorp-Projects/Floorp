/*
	Copyright (c) 2004-2006, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

/*
	This is a compiled version of Dojo, built for deployment and not for
	development. To get an editable version, please visit:

		http://dojotoolkit.org

	for documentation and information on getting the source.
*/

var dj_global=this;
function dj_undef(_1,_2){
if(!_2){
_2=dj_global;
}
return (typeof _2[_1]=="undefined");
}
if(dj_undef("djConfig")){
var djConfig={};
}
if(dj_undef("dojo")){
var dojo={};
}
dojo.version={major:0,minor:2,patch:2,flag:"+",revision:Number("$Rev: 3802 $".match(/[0-9]+/)[0]),toString:function(){
with(dojo.version){
return major+"."+minor+"."+patch+flag+" ("+revision+")";
}
}};
dojo.evalProp=function(_3,_4,_5){
return (_4&&!dj_undef(_3,_4)?_4[_3]:(_5?(_4[_3]={}):undefined));
};
dojo.parseObjPath=function(_6,_7,_8){
var _9=(_7?_7:dj_global);
var _a=_6.split(".");
var _b=_a.pop();
for(var i=0,l=_a.length;i<l&&_9;i++){
_9=dojo.evalProp(_a[i],_9,_8);
}
return {obj:_9,prop:_b};
};
dojo.evalObjPath=function(_d,_e){
if(typeof _d!="string"){
return dj_global;
}
if(_d.indexOf(".")==-1){
return dojo.evalProp(_d,dj_global,_e);
}
with(dojo.parseObjPath(_d,dj_global,_e)){
return dojo.evalProp(prop,obj,_e);
}
};
dojo.errorToString=function(_f){
return ((!dj_undef("message",_f))?_f.message:(dj_undef("description",_f)?_f:_f.description));
};
dojo.raise=function(_10,_11){
if(_11){
_10=_10+": "+dojo.errorToString(_11);
}
var he=dojo.hostenv;
if((!dj_undef("hostenv",dojo))&&(!dj_undef("println",dojo.hostenv))){
dojo.hostenv.println("FATAL: "+_10);
}
throw Error(_10);
};
dojo.debug=function(){
};
dojo.debugShallow=function(obj){
};
dojo.profile={start:function(){
},end:function(){
},stop:function(){
},dump:function(){
}};
function dj_eval(s){
return dj_global.eval?dj_global.eval(s):eval(s);
}
dojo.unimplemented=function(_15,_16){
var _17="'"+_15+"' not implemented";
if((!dj_undef(_16))&&(_16)){
_17+=" "+_16;
}
dojo.raise(_17);
};
dojo.deprecated=function(_18,_19,_1a){
var _1b="DEPRECATED: "+_18;
if(_19){
_1b+=" "+_19;
}
if(_1a){
_1b+=" -- will be removed in version: "+_1a;
}
dojo.debug(_1b);
};
dojo.inherits=function(_1c,_1d){
if(typeof _1d!="function"){
dojo.raise("dojo.inherits: superclass argument ["+_1d+"] must be a function (subclass: ["+_1c+"']");
}
_1c.prototype=new _1d();
_1c.prototype.constructor=_1c;
_1c.superclass=_1d.prototype;
_1c["super"]=_1d.prototype;
};
dojo.render=(function(){
function vscaffold(_1e,_1f){
var tmp={capable:false,support:{builtin:false,plugin:false},prefixes:_1e};
for(var x in _1f){
tmp[x]=false;
}
return tmp;
}
return {name:"",ver:dojo.version,os:{win:false,linux:false,osx:false},html:vscaffold(["html"],["ie","opera","khtml","safari","moz"]),svg:vscaffold(["svg"],["corel","adobe","batik"]),vml:vscaffold(["vml"],["ie"]),swf:vscaffold(["Swf","Flash","Mm"],["mm"]),swt:vscaffold(["Swt"],["ibm"])};
})();
dojo.hostenv=(function(){
var _22={isDebug:false,allowQueryConfig:false,baseScriptUri:"",baseRelativePath:"",libraryScriptUri:"",iePreventClobber:false,ieClobberMinimal:true,preventBackButtonFix:true,searchIds:[],parseWidgets:true};
if(typeof djConfig=="undefined"){
djConfig=_22;
}else{
for(var _23 in _22){
if(typeof djConfig[_23]=="undefined"){
djConfig[_23]=_22[_23];
}
}
}
return {name_:"(unset)",version_:"(unset)",getName:function(){
return this.name_;
},getVersion:function(){
return this.version_;
},getText:function(uri){
dojo.unimplemented("getText","uri="+uri);
}};
})();
dojo.hostenv.getBaseScriptUri=function(){
if(djConfig.baseScriptUri.length){
return djConfig.baseScriptUri;
}
var uri=new String(djConfig.libraryScriptUri||djConfig.baseRelativePath);
if(!uri){
dojo.raise("Nothing returned by getLibraryScriptUri(): "+uri);
}
var _26=uri.lastIndexOf("/");
djConfig.baseScriptUri=djConfig.baseRelativePath;
return djConfig.baseScriptUri;
};
(function(){
var _27={pkgFileName:"__package__",loading_modules_:{},loaded_modules_:{},addedToLoadingCount:[],removedFromLoadingCount:[],inFlightCount:0,modulePrefixes_:{dojo:{name:"dojo",value:"src"}},setModulePrefix:function(_28,_29){
this.modulePrefixes_[_28]={name:_28,value:_29};
},getModulePrefix:function(_2a){
var mp=this.modulePrefixes_;
if((mp[_2a])&&(mp[_2a]["name"])){
return mp[_2a].value;
}
return _2a;
},getTextStack:[],loadUriStack:[],loadedUris:[],post_load_:false,modulesLoadedListeners:[]};
for(var _2c in _27){
dojo.hostenv[_2c]=_27[_2c];
}
})();
dojo.hostenv.loadPath=function(_2d,_2e,cb){
if((_2d.charAt(0)=="/")||(_2d.match(/^\w+:/))){
dojo.raise("relpath '"+_2d+"'; must be relative");
}
var uri=this.getBaseScriptUri()+_2d;
if(djConfig.cacheBust&&dojo.render.html.capable){
uri+="?"+String(djConfig.cacheBust).replace(/\W+/g,"");
}
try{
return ((!_2e)?this.loadUri(uri,cb):this.loadUriAndCheck(uri,_2e,cb));
}
catch(e){
dojo.debug(e);
return false;
}
};
dojo.hostenv.loadUri=function(uri,cb){
if(this.loadedUris[uri]){
return;
}
var _33=this.getText(uri,null,true);
if(_33==null){
return 0;
}
this.loadedUris[uri]=true;
var _34=dj_eval(_33);
return 1;
};
dojo.hostenv.loadUriAndCheck=function(uri,_36,cb){
var ok=true;
try{
ok=this.loadUri(uri,cb);
}
catch(e){
dojo.debug("failed loading ",uri," with error: ",e);
}
return ((ok)&&(this.findModule(_36,false)))?true:false;
};
dojo.loaded=function(){
};
dojo.hostenv.loaded=function(){
this.post_load_=true;
var mll=this.modulesLoadedListeners;
this.modulesLoadedListeners=[];
for(var x=0;x<mll.length;x++){
mll[x]();
}
dojo.loaded();
};
dojo.addOnLoad=function(obj,_3c){
var dh=dojo.hostenv;
if(arguments.length==1){
dh.modulesLoadedListeners.push(obj);
}else{
if(arguments.length>1){
dh.modulesLoadedListeners.push(function(){
obj[_3c]();
});
}
}
if(dh.post_load_&&dh.inFlightCount==0){
dh.callLoaded();
}
};
dojo.hostenv.modulesLoaded=function(){
if(this.post_load_){
return;
}
if((this.loadUriStack.length==0)&&(this.getTextStack.length==0)){
if(this.inFlightCount>0){
dojo.debug("files still in flight!");
return;
}
dojo.hostenv.callLoaded();
}
};
dojo.hostenv.callLoaded=function(){
if(typeof setTimeout=="object"){
setTimeout("dojo.hostenv.loaded();",0);
}else{
dojo.hostenv.loaded();
}
};
dojo.hostenv._global_omit_module_check=false;
dojo.hostenv.loadModule=function(_3e,_3f,_40){
if(!_3e){
return;
}
_40=this._global_omit_module_check||_40;
var _41=this.findModule(_3e,false);
if(_41){
return _41;
}
if(dj_undef(_3e,this.loading_modules_)){
this.addedToLoadingCount.push(_3e);
}
this.loading_modules_[_3e]=1;
var _42=_3e.replace(/\./g,"/")+".js";
var _43=_3e.split(".");
var _44=_3e.split(".");
for(var i=_43.length-1;i>0;i--){
var _46=_43.slice(0,i).join(".");
var _47=this.getModulePrefix(_46);
if(_47!=_46){
_43.splice(0,i,_47);
break;
}
}
var _48=_43[_43.length-1];
if(_48=="*"){
_3e=(_44.slice(0,-1)).join(".");
while(_43.length){
_43.pop();
_43.push(this.pkgFileName);
_42=_43.join("/")+".js";
if(_42.charAt(0)=="/"){
_42=_42.slice(1);
}
ok=this.loadPath(_42,((!_40)?_3e:null));
if(ok){
break;
}
_43.pop();
}
}else{
_42=_43.join("/")+".js";
_3e=_44.join(".");
var ok=this.loadPath(_42,((!_40)?_3e:null));
if((!ok)&&(!_3f)){
_43.pop();
while(_43.length){
_42=_43.join("/")+".js";
ok=this.loadPath(_42,((!_40)?_3e:null));
if(ok){
break;
}
_43.pop();
_42=_43.join("/")+"/"+this.pkgFileName+".js";
if(_42.charAt(0)=="/"){
_42=_42.slice(1);
}
ok=this.loadPath(_42,((!_40)?_3e:null));
if(ok){
break;
}
}
}
if((!ok)&&(!_40)){
dojo.raise("Could not load '"+_3e+"'; last tried '"+_42+"'");
}
}
if(!_40&&!this["isXDomain"]){
_41=this.findModule(_3e,false);
if(!_41){
dojo.raise("symbol '"+_3e+"' is not defined after loading '"+_42+"'");
}
}
return _41;
};
dojo.hostenv.startPackage=function(_4a){
var _4b=dojo.evalObjPath((_4a.split(".").slice(0,-1)).join("."));
this.loaded_modules_[(new String(_4a)).toLowerCase()]=_4b;
var _4c=_4a.split(/\./);
if(_4c[_4c.length-1]=="*"){
_4c.pop();
}
return dojo.evalObjPath(_4c.join("."),true);
};
dojo.hostenv.findModule=function(_4d,_4e){
var lmn=(new String(_4d)).toLowerCase();
if(this.loaded_modules_[lmn]){
return this.loaded_modules_[lmn];
}
var _50=dojo.evalObjPath(_4d);
if((_4d)&&(typeof _50!="undefined")&&(_50)){
this.loaded_modules_[lmn]=_50;
return _50;
}
if(_4e){
dojo.raise("no loaded module named '"+_4d+"'");
}
return null;
};
dojo.kwCompoundRequire=function(_51){
var _52=_51["common"]||[];
var _53=(_51[dojo.hostenv.name_])?_52.concat(_51[dojo.hostenv.name_]||[]):_52.concat(_51["default"]||[]);
for(var x=0;x<_53.length;x++){
var _55=_53[x];
if(_55.constructor==Array){
dojo.hostenv.loadModule.apply(dojo.hostenv,_55);
}else{
dojo.hostenv.loadModule(_55);
}
}
};
dojo.require=function(){
dojo.hostenv.loadModule.apply(dojo.hostenv,arguments);
};
dojo.requireIf=function(){
if((arguments[0]===true)||(arguments[0]=="common")||(arguments[0]&&dojo.render[arguments[0]].capable)){
var _56=[];
for(var i=1;i<arguments.length;i++){
_56.push(arguments[i]);
}
dojo.require.apply(dojo,_56);
}
};
dojo.requireAfterIf=dojo.requireIf;
dojo.provide=function(){
return dojo.hostenv.startPackage.apply(dojo.hostenv,arguments);
};
dojo.setModulePrefix=function(_58,_59){
return dojo.hostenv.setModulePrefix(_58,_59);
};
dojo.exists=function(obj,_5b){
var p=_5b.split(".");
for(var i=0;i<p.length;i++){
if(!(obj[p[i]])){
return false;
}
obj=obj[p[i]];
}
return true;
};
if(typeof window=="undefined"){
dojo.raise("no window object");
}
(function(){
if(djConfig.allowQueryConfig){
var _5e=document.location.toString();
var _5f=_5e.split("?",2);
if(_5f.length>1){
var _60=_5f[1];
var _61=_60.split("&");
for(var x in _61){
var sp=_61[x].split("=");
if((sp[0].length>9)&&(sp[0].substr(0,9)=="djConfig.")){
var opt=sp[0].substr(9);
try{
djConfig[opt]=eval(sp[1]);
}
catch(e){
djConfig[opt]=sp[1];
}
}
}
}
}
if(((djConfig["baseScriptUri"]=="")||(djConfig["baseRelativePath"]==""))&&(document&&document.getElementsByTagName)){
var _65=document.getElementsByTagName("script");
var _66=/(__package__|dojo|bootstrap1)\.js([\?\.]|$)/i;
for(var i=0;i<_65.length;i++){
var src=_65[i].getAttribute("src");
if(!src){
continue;
}
var m=src.match(_66);
if(m){
root=src.substring(0,m.index);
if(src.indexOf("bootstrap1")>-1){
root+="../";
}
if(!this["djConfig"]){
djConfig={};
}
if(djConfig["baseScriptUri"]==""){
djConfig["baseScriptUri"]=root;
}
if(djConfig["baseRelativePath"]==""){
djConfig["baseRelativePath"]=root;
}
break;
}
}
}
var dr=dojo.render;
var drh=dojo.render.html;
var drs=dojo.render.svg;
var dua=drh.UA=navigator.userAgent;
var dav=drh.AV=navigator.appVersion;
var t=true;
var f=false;
drh.capable=t;
drh.support.builtin=t;
dr.ver=parseFloat(drh.AV);
dr.os.mac=dav.indexOf("Macintosh")>=0;
dr.os.win=dav.indexOf("Windows")>=0;
dr.os.linux=dav.indexOf("X11")>=0;
drh.opera=dua.indexOf("Opera")>=0;
drh.khtml=(dav.indexOf("Konqueror")>=0)||(dav.indexOf("Safari")>=0);
drh.safari=dav.indexOf("Safari")>=0;
var _71=dua.indexOf("Gecko");
drh.mozilla=drh.moz=(_71>=0)&&(!drh.khtml);
if(drh.mozilla){
drh.geckoVersion=dua.substring(_71+6,_71+14);
}
drh.ie=(document.all)&&(!drh.opera);
drh.ie50=drh.ie&&dav.indexOf("MSIE 5.0")>=0;
drh.ie55=drh.ie&&dav.indexOf("MSIE 5.5")>=0;
drh.ie60=drh.ie&&dav.indexOf("MSIE 6.0")>=0;
dr.vml.capable=drh.ie;
drs.capable=f;
drs.support.plugin=f;
drs.support.builtin=f;
if(document.implementation&&document.implementation.hasFeature&&document.implementation.hasFeature("org.w3c.dom.svg","1.0")){
drs.capable=t;
drs.support.builtin=t;
drs.support.plugin=f;
}
})();
dojo.hostenv.startPackage("dojo.hostenv");
dojo.render.name=dojo.hostenv.name_="browser";
dojo.hostenv.searchIds=[];
var DJ_XMLHTTP_PROGIDS=["Msxml2.XMLHTTP","Microsoft.XMLHTTP","Msxml2.XMLHTTP.4.0"];
dojo.hostenv.getXmlhttpObject=function(){
var _72=null;
var _73=null;
try{
_72=new XMLHttpRequest();
}
catch(e){
}
if(!_72){
for(var i=0;i<3;++i){
var _75=DJ_XMLHTTP_PROGIDS[i];
try{
_72=new ActiveXObject(_75);
}
catch(e){
_73=e;
}
if(_72){
DJ_XMLHTTP_PROGIDS=[_75];
break;
}
}
}
if(!_72){
return dojo.raise("XMLHTTP not available",_73);
}
return _72;
};
dojo.hostenv.getText=function(uri,_77,_78){
var _79=this.getXmlhttpObject();
if(_77){
_79.onreadystatechange=function(){
if((4==_79.readyState)&&(_79["status"])){
if(_79.status==200){
_77(_79.responseText);
}
}
};
}
_79.open("GET",uri,_77?true:false);
try{
_79.send(null);
}
catch(e){
if(_78&&!_77){
return null;
}else{
throw e;
}
}
if(_77){
return null;
}
return _79.responseText;
};
dojo.hostenv.defaultDebugContainerId="dojoDebug";
dojo.hostenv._println_buffer=[];
dojo.hostenv._println_safe=false;
dojo.hostenv.println=function(_7a){
if(!dojo.hostenv._println_safe){
dojo.hostenv._println_buffer.push(_7a);
}else{
try{
var _7b=document.getElementById(djConfig.debugContainerId?djConfig.debugContainerId:dojo.hostenv.defaultDebugContainerId);
if(!_7b){
_7b=document.getElementsByTagName("body")[0]||document.body;
}
var div=document.createElement("div");
div.appendChild(document.createTextNode(_7a));
_7b.appendChild(div);
}
catch(e){
try{
document.write("<div>"+_7a+"</div>");
}
catch(e2){
window.status=_7a;
}
}
}
};
dojo.addOnLoad(function(){
dojo.hostenv._println_safe=true;
while(dojo.hostenv._println_buffer.length>0){
dojo.hostenv.println(dojo.hostenv._println_buffer.shift());
}
});
function dj_addNodeEvtHdlr(_7d,_7e,fp,_80){
var _81=_7d["on"+_7e]||function(){
};
_7d["on"+_7e]=function(){
fp.apply(_7d,arguments);
_81.apply(_7d,arguments);
};
return true;
}
dj_addNodeEvtHdlr(window,"load",function(){
if(arguments.callee.initialized){
return;
}
arguments.callee.initialized=true;
var _82=function(){
if(dojo.render.html.ie){
dojo.hostenv.makeWidgets();
}
};
if(dojo.hostenv.inFlightCount==0){
_82();
dojo.hostenv.modulesLoaded();
}else{
dojo.addOnLoad(_82);
}
});
dojo.hostenv.makeWidgets=function(){
var _83=[];
if(djConfig.searchIds&&djConfig.searchIds.length>0){
_83=_83.concat(djConfig.searchIds);
}
if(dojo.hostenv.searchIds&&dojo.hostenv.searchIds.length>0){
_83=_83.concat(dojo.hostenv.searchIds);
}
if((djConfig.parseWidgets)||(_83.length>0)){
if(dojo.evalObjPath("dojo.widget.Parse")){
try{
var _84=new dojo.xml.Parse();
if(_83.length>0){
for(var x=0;x<_83.length;x++){
var _86=document.getElementById(_83[x]);
if(!_86){
continue;
}
var _87=_84.parseElement(_86,null,true);
dojo.widget.getParser().createComponents(_87);
}
}else{
if(djConfig.parseWidgets){
var _87=_84.parseElement(document.getElementsByTagName("body")[0]||document.body,null,true);
dojo.widget.getParser().createComponents(_87);
}
}
}
catch(e){
dojo.debug("auto-build-widgets error:",e);
}
}
}
};
dojo.addOnLoad(function(){
if(!dojo.render.html.ie){
dojo.hostenv.makeWidgets();
}
});
try{
if(dojo.render.html.ie){
document.namespaces.add("v","urn:schemas-microsoft-com:vml");
document.createStyleSheet().addRule("v\\:*","behavior:url(#default#VML)");
}
}
catch(e){
}
dojo.hostenv.writeIncludes=function(){
};
dojo.byId=function(id,doc){
if(id&&(typeof id=="string"||id instanceof String)){
if(!doc){
doc=document;
}
return doc.getElementById(id);
}
return id;
};
(function(){
if(typeof dj_usingBootstrap!="undefined"){
return;
}
var _8a=false;
var _8b=false;
var _8c=false;
if((typeof this["load"]=="function")&&(typeof this["Packages"]=="function")){
_8a=true;
}else{
if(typeof this["load"]=="function"){
_8b=true;
}else{
if(window.widget){
_8c=true;
}
}
}
var _8d=[];
if((this["djConfig"])&&((djConfig["isDebug"])||(djConfig["debugAtAllCosts"]))){
_8d.push("debug.js");
}
if((this["djConfig"])&&(djConfig["debugAtAllCosts"])&&(!_8a)&&(!_8c)){
_8d.push("browser_debug.js");
}
if((this["djConfig"])&&(djConfig["compat"])){
_8d.push("compat/"+djConfig["compat"]+".js");
}
var _8e=djConfig["baseScriptUri"];
if((this["djConfig"])&&(djConfig["baseLoaderUri"])){
_8e=djConfig["baseLoaderUri"];
}
for(var x=0;x<_8d.length;x++){
var _90=_8e+"src/"+_8d[x];
if(_8a||_8b){
load(_90);
}else{
try{
document.write("<scr"+"ipt type='text/javascript' src='"+_90+"'></scr"+"ipt>");
}
catch(e){
var _91=document.createElement("script");
_91.src=_90;
document.getElementsByTagName("head")[0].appendChild(_91);
}
}
}
})();
dojo.provide("dojo.lang.common");
dojo.require("dojo.lang");
dojo.lang.mixin=function(obj,_93){
var _94={};
for(var x in _93){
if(typeof _94[x]=="undefined"||_94[x]!=_93[x]){
obj[x]=_93[x];
}
}
if(dojo.render.html.ie&&dojo.lang.isFunction(_93["toString"])&&_93["toString"]!=obj["toString"]){
obj.toString=_93.toString;
}
return obj;
};
dojo.lang.extend=function(_96,_97){
this.mixin(_96.prototype,_97);
};
dojo.lang.find=function(arr,val,_9a,_9b){
if(!dojo.lang.isArrayLike(arr)&&dojo.lang.isArrayLike(val)){
var a=arr;
arr=val;
val=a;
}
var _9d=dojo.lang.isString(arr);
if(_9d){
arr=arr.split("");
}
if(_9b){
var _9e=-1;
var i=arr.length-1;
var end=-1;
}else{
var _9e=1;
var i=0;
var end=arr.length;
}
if(_9a){
while(i!=end){
if(arr[i]===val){
return i;
}
i+=_9e;
}
}else{
while(i!=end){
if(arr[i]==val){
return i;
}
i+=_9e;
}
}
return -1;
};
dojo.lang.indexOf=dojo.lang.find;
dojo.lang.findLast=function(arr,val,_a3){
return dojo.lang.find(arr,val,_a3,true);
};
dojo.lang.lastIndexOf=dojo.lang.findLast;
dojo.lang.inArray=function(arr,val){
return dojo.lang.find(arr,val)>-1;
};
dojo.lang.isObject=function(wh){
return typeof wh=="object"||dojo.lang.isArray(wh)||dojo.lang.isFunction(wh);
};
dojo.lang.isArray=function(wh){
return (wh instanceof Array||typeof wh=="array");
};
dojo.lang.isArrayLike=function(wh){
if(dojo.lang.isString(wh)){
return false;
}
if(dojo.lang.isFunction(wh)){
return false;
}
if(dojo.lang.isArray(wh)){
return true;
}
if(typeof wh!="undefined"&&wh&&dojo.lang.isNumber(wh.length)&&isFinite(wh.length)){
return true;
}
return false;
};
dojo.lang.isFunction=function(wh){
return (wh instanceof Function||typeof wh=="function");
};
dojo.lang.isString=function(wh){
return (wh instanceof String||typeof wh=="string");
};
dojo.lang.isAlien=function(wh){
return !dojo.lang.isFunction()&&/\{\s*\[native code\]\s*\}/.test(String(wh));
};
dojo.lang.isBoolean=function(wh){
return (wh instanceof Boolean||typeof wh=="boolean");
};
dojo.lang.isNumber=function(wh){
return (wh instanceof Number||typeof wh=="number");
};
dojo.lang.isUndefined=function(wh){
return ((wh==undefined)&&(typeof wh=="undefined"));
};
dojo.provide("dojo.lang");
dojo.provide("dojo.lang.Lang");
dojo.require("dojo.lang.common");
dojo.provide("dojo.lang.func");
dojo.require("dojo.lang.common");
dojo.lang.hitch=function(_af,_b0){
if(dojo.lang.isString(_b0)){
var fcn=_af[_b0];
}else{
var fcn=_b0;
}
return function(){
return fcn.apply(_af,arguments);
};
};
dojo.lang.anonCtr=0;
dojo.lang.anon={};
dojo.lang.nameAnonFunc=function(_b2,_b3){
var nso=(_b3||dojo.lang.anon);
if((dj_global["djConfig"])&&(djConfig["slowAnonFuncLookups"]==true)){
for(var x in nso){
if(nso[x]===_b2){
return x;
}
}
}
var ret="__"+dojo.lang.anonCtr++;
while(typeof nso[ret]!="undefined"){
ret="__"+dojo.lang.anonCtr++;
}
nso[ret]=_b2;
return ret;
};
dojo.lang.forward=function(_b7){
return function(){
return this[_b7].apply(this,arguments);
};
};
dojo.lang.curry=function(ns,_b9){
var _ba=[];
ns=ns||dj_global;
if(dojo.lang.isString(_b9)){
_b9=ns[_b9];
}
for(var x=2;x<arguments.length;x++){
_ba.push(arguments[x]);
}
var _bc=(_b9["__preJoinArity"]||_b9.length)-_ba.length;
function gather(_bd,_be,_bf){
var _c0=_bf;
var _c1=_be.slice(0);
for(var x=0;x<_bd.length;x++){
_c1.push(_bd[x]);
}
_bf=_bf-_bd.length;
if(_bf<=0){
var res=_b9.apply(ns,_c1);
_bf=_c0;
return res;
}else{
return function(){
return gather(arguments,_c1,_bf);
};
}
}
return gather([],_ba,_bc);
};
dojo.lang.curryArguments=function(ns,_c5,_c6,_c7){
var _c8=[];
var x=_c7||0;
for(x=_c7;x<_c6.length;x++){
_c8.push(_c6[x]);
}
return dojo.lang.curry.apply(dojo.lang,[ns,_c5].concat(_c8));
};
dojo.lang.tryThese=function(){
for(var x=0;x<arguments.length;x++){
try{
if(typeof arguments[x]=="function"){
var ret=(arguments[x]());
if(ret){
return ret;
}
}
}
catch(e){
dojo.debug(e);
}
}
};
dojo.lang.delayThese=function(_cc,cb,_ce,_cf){
if(!_cc.length){
if(typeof _cf=="function"){
_cf();
}
return;
}
if((typeof _ce=="undefined")&&(typeof cb=="number")){
_ce=cb;
cb=function(){
};
}else{
if(!cb){
cb=function(){
};
if(!_ce){
_ce=0;
}
}
}
setTimeout(function(){
(_cc.shift())();
cb();
dojo.lang.delayThese(_cc,cb,_ce,_cf);
},_ce);
};
dojo.provide("dojo.lang.array");
dojo.require("dojo.lang.common");
dojo.lang.has=function(obj,_d1){
try{
return (typeof obj[_d1]!="undefined");
}
catch(e){
return false;
}
};
dojo.lang.isEmpty=function(obj){
if(dojo.lang.isObject(obj)){
var tmp={};
var _d4=0;
for(var x in obj){
if(obj[x]&&(!tmp[x])){
_d4++;
break;
}
}
return (_d4==0);
}else{
if(dojo.lang.isArrayLike(obj)||dojo.lang.isString(obj)){
return obj.length==0;
}
}
};
dojo.lang.map=function(arr,obj,_d8){
var _d9=dojo.lang.isString(arr);
if(_d9){
arr=arr.split("");
}
if(dojo.lang.isFunction(obj)&&(!_d8)){
_d8=obj;
obj=dj_global;
}else{
if(dojo.lang.isFunction(obj)&&_d8){
var _da=obj;
obj=_d8;
_d8=_da;
}
}
if(Array.map){
var _db=Array.map(arr,_d8,obj);
}else{
var _db=[];
for(var i=0;i<arr.length;++i){
_db.push(_d8.call(obj,arr[i]));
}
}
if(_d9){
return _db.join("");
}else{
return _db;
}
};
dojo.lang.forEach=function(_dd,_de,_df){
if(dojo.lang.isString(_dd)){
_dd=_dd.split("");
}
if(Array.forEach){
Array.forEach(_dd,_de,_df);
}else{
if(!_df){
_df=dj_global;
}
for(var i=0,l=_dd.length;i<l;i++){
_de.call(_df,_dd[i],i,_dd);
}
}
};
dojo.lang._everyOrSome=function(_e1,arr,_e3,_e4){
if(dojo.lang.isString(arr)){
arr=arr.split("");
}
if(Array.every){
return Array[(_e1)?"every":"some"](arr,_e3,_e4);
}else{
if(!_e4){
_e4=dj_global;
}
for(var i=0,l=arr.length;i<l;i++){
var _e6=_e3.call(_e4,arr[i],i,arr);
if((_e1)&&(!_e6)){
return false;
}else{
if((!_e1)&&(_e6)){
return true;
}
}
}
return (_e1)?true:false;
}
};
dojo.lang.every=function(arr,_e8,_e9){
return this._everyOrSome(true,arr,_e8,_e9);
};
dojo.lang.some=function(arr,_eb,_ec){
return this._everyOrSome(false,arr,_eb,_ec);
};
dojo.lang.filter=function(arr,_ee,_ef){
var _f0=dojo.lang.isString(arr);
if(_f0){
arr=arr.split("");
}
if(Array.filter){
var _f1=Array.filter(arr,_ee,_ef);
}else{
if(!_ef){
if(arguments.length>=3){
dojo.raise("thisObject doesn't exist!");
}
_ef=dj_global;
}
var _f1=[];
for(var i=0;i<arr.length;i++){
if(_ee.call(_ef,arr[i],i,arr)){
_f1.push(arr[i]);
}
}
}
if(_f0){
return _f1.join("");
}else{
return _f1;
}
};
dojo.lang.unnest=function(){
var out=[];
for(var i=0;i<arguments.length;i++){
if(dojo.lang.isArrayLike(arguments[i])){
var add=dojo.lang.unnest.apply(this,arguments[i]);
out=out.concat(add);
}else{
out.push(arguments[i]);
}
}
return out;
};
dojo.lang.toArray=function(_f6,_f7){
var _f8=[];
for(var i=_f7||0;i<_f6.length;i++){
_f8.push(_f6[i]);
}
return _f8;
};
dojo.provide("dojo.dom");
dojo.require("dojo.lang.array");
dojo.dom.ELEMENT_NODE=1;
dojo.dom.ATTRIBUTE_NODE=2;
dojo.dom.TEXT_NODE=3;
dojo.dom.CDATA_SECTION_NODE=4;
dojo.dom.ENTITY_REFERENCE_NODE=5;
dojo.dom.ENTITY_NODE=6;
dojo.dom.PROCESSING_INSTRUCTION_NODE=7;
dojo.dom.COMMENT_NODE=8;
dojo.dom.DOCUMENT_NODE=9;
dojo.dom.DOCUMENT_TYPE_NODE=10;
dojo.dom.DOCUMENT_FRAGMENT_NODE=11;
dojo.dom.NOTATION_NODE=12;
dojo.dom.dojoml="http://www.dojotoolkit.org/2004/dojoml";
dojo.dom.xmlns={svg:"http://www.w3.org/2000/svg",smil:"http://www.w3.org/2001/SMIL20/",mml:"http://www.w3.org/1998/Math/MathML",cml:"http://www.xml-cml.org",xlink:"http://www.w3.org/1999/xlink",xhtml:"http://www.w3.org/1999/xhtml",xul:"http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",xbl:"http://www.mozilla.org/xbl",fo:"http://www.w3.org/1999/XSL/Format",xsl:"http://www.w3.org/1999/XSL/Transform",xslt:"http://www.w3.org/1999/XSL/Transform",xi:"http://www.w3.org/2001/XInclude",xforms:"http://www.w3.org/2002/01/xforms",saxon:"http://icl.com/saxon",xalan:"http://xml.apache.org/xslt",xsd:"http://www.w3.org/2001/XMLSchema",dt:"http://www.w3.org/2001/XMLSchema-datatypes",xsi:"http://www.w3.org/2001/XMLSchema-instance",rdf:"http://www.w3.org/1999/02/22-rdf-syntax-ns#",rdfs:"http://www.w3.org/2000/01/rdf-schema#",dc:"http://purl.org/dc/elements/1.1/",dcq:"http://purl.org/dc/qualifiers/1.0","soap-env":"http://schemas.xmlsoap.org/soap/envelope/",wsdl:"http://schemas.xmlsoap.org/wsdl/",AdobeExtensions:"http://ns.adobe.com/AdobeSVGViewerExtensions/3.0/"};
dojo.dom.isNode=function(wh){
if(typeof Element=="object"){
try{
return wh instanceof Element;
}
catch(E){
}
}else{
return wh&&!isNaN(wh.nodeType);
}
};
dojo.dom.getTagName=function(_fb){
dojo.deprecated("dojo.dom.getTagName","use node.tagName instead","0.4");
var _fc=_fb.tagName;
if(_fc.substr(0,5).toLowerCase()!="dojo:"){
if(_fc.substr(0,4).toLowerCase()=="dojo"){
return "dojo:"+_fc.substring(4).toLowerCase();
}
var djt=_fb.getAttribute("dojoType")||_fb.getAttribute("dojotype");
if(djt){
return "dojo:"+djt.toLowerCase();
}
if((_fb.getAttributeNS)&&(_fb.getAttributeNS(this.dojoml,"type"))){
return "dojo:"+_fb.getAttributeNS(this.dojoml,"type").toLowerCase();
}
try{
djt=_fb.getAttribute("dojo:type");
}
catch(e){
}
if(djt){
return "dojo:"+djt.toLowerCase();
}
if((!dj_global["djConfig"])||(!djConfig["ignoreClassNames"])){
var _fe=_fb.className||_fb.getAttribute("class");
if((_fe)&&(_fe.indexOf)&&(_fe.indexOf("dojo-")!=-1)){
var _ff=_fe.split(" ");
for(var x=0;x<_ff.length;x++){
if((_ff[x].length>5)&&(_ff[x].indexOf("dojo-")>=0)){
return "dojo:"+_ff[x].substr(5).toLowerCase();
}
}
}
}
}
return _fc.toLowerCase();
};
dojo.dom.getUniqueId=function(){
do{
var id="dj_unique_"+(++arguments.callee._idIncrement);
}while(document.getElementById(id));
return id;
};
dojo.dom.getUniqueId._idIncrement=0;
dojo.dom.firstElement=dojo.dom.getFirstChildElement=function(_102,_103){
var node=_102.firstChild;
while(node&&node.nodeType!=dojo.dom.ELEMENT_NODE){
node=node.nextSibling;
}
if(_103&&node&&node.tagName&&node.tagName.toLowerCase()!=_103.toLowerCase()){
node=dojo.dom.nextElement(node,_103);
}
return node;
};
dojo.dom.lastElement=dojo.dom.getLastChildElement=function(_105,_106){
var node=_105.lastChild;
while(node&&node.nodeType!=dojo.dom.ELEMENT_NODE){
node=node.previousSibling;
}
if(_106&&node&&node.tagName&&node.tagName.toLowerCase()!=_106.toLowerCase()){
node=dojo.dom.prevElement(node,_106);
}
return node;
};
dojo.dom.nextElement=dojo.dom.getNextSiblingElement=function(node,_109){
if(!node){
return null;
}
do{
node=node.nextSibling;
}while(node&&node.nodeType!=dojo.dom.ELEMENT_NODE);
if(node&&_109&&_109.toLowerCase()!=node.tagName.toLowerCase()){
return dojo.dom.nextElement(node,_109);
}
return node;
};
dojo.dom.prevElement=dojo.dom.getPreviousSiblingElement=function(node,_10b){
if(!node){
return null;
}
if(_10b){
_10b=_10b.toLowerCase();
}
do{
node=node.previousSibling;
}while(node&&node.nodeType!=dojo.dom.ELEMENT_NODE);
if(node&&_10b&&_10b.toLowerCase()!=node.tagName.toLowerCase()){
return dojo.dom.prevElement(node,_10b);
}
return node;
};
dojo.dom.moveChildren=function(_10c,_10d,trim){
var _10f=0;
if(trim){
while(_10c.hasChildNodes()&&_10c.firstChild.nodeType==dojo.dom.TEXT_NODE){
_10c.removeChild(_10c.firstChild);
}
while(_10c.hasChildNodes()&&_10c.lastChild.nodeType==dojo.dom.TEXT_NODE){
_10c.removeChild(_10c.lastChild);
}
}
while(_10c.hasChildNodes()){
_10d.appendChild(_10c.firstChild);
_10f++;
}
return _10f;
};
dojo.dom.copyChildren=function(_110,_111,trim){
var _113=_110.cloneNode(true);
return this.moveChildren(_113,_111,trim);
};
dojo.dom.removeChildren=function(node){
var _115=node.childNodes.length;
while(node.hasChildNodes()){
node.removeChild(node.firstChild);
}
return _115;
};
dojo.dom.replaceChildren=function(node,_117){
dojo.dom.removeChildren(node);
node.appendChild(_117);
};
dojo.dom.removeNode=function(node){
if(node&&node.parentNode){
return node.parentNode.removeChild(node);
}
};
dojo.dom.getAncestors=function(node,_11a,_11b){
var _11c=[];
var _11d=dojo.lang.isFunction(_11a);
while(node){
if(!_11d||_11a(node)){
_11c.push(node);
}
if(_11b&&_11c.length>0){
return _11c[0];
}
node=node.parentNode;
}
if(_11b){
return null;
}
return _11c;
};
dojo.dom.getAncestorsByTag=function(node,tag,_120){
tag=tag.toLowerCase();
return dojo.dom.getAncestors(node,function(el){
return ((el.tagName)&&(el.tagName.toLowerCase()==tag));
},_120);
};
dojo.dom.getFirstAncestorByTag=function(node,tag){
return dojo.dom.getAncestorsByTag(node,tag,true);
};
dojo.dom.isDescendantOf=function(node,_125,_126){
if(_126&&node){
node=node.parentNode;
}
while(node){
if(node==_125){
return true;
}
node=node.parentNode;
}
return false;
};
dojo.dom.innerXML=function(node){
if(node.innerXML){
return node.innerXML;
}else{
if(typeof XMLSerializer!="undefined"){
return (new XMLSerializer()).serializeToString(node);
}
}
};
dojo.dom.createDocumentFromText=function(str,_129){
if(!_129){
_129="text/xml";
}
if(typeof DOMParser!="undefined"){
var _12a=new DOMParser();
return _12a.parseFromString(str,_129);
}else{
if(typeof ActiveXObject!="undefined"){
var _12b=new ActiveXObject("Microsoft.XMLDOM");
if(_12b){
_12b.async=false;
_12b.loadXML(str);
return _12b;
}else{
dojo.debug("toXml didn't work?");
}
}else{
if(document.createElement){
var tmp=document.createElement("xml");
tmp.innerHTML=str;
if(document.implementation&&document.implementation.createDocument){
var _12d=document.implementation.createDocument("foo","",null);
for(var i=0;i<tmp.childNodes.length;i++){
_12d.importNode(tmp.childNodes.item(i),true);
}
return _12d;
}
return tmp.document&&tmp.document.firstChild?tmp.document.firstChild:tmp;
}
}
}
return null;
};
dojo.dom.prependChild=function(node,_130){
if(_130.firstChild){
_130.insertBefore(node,_130.firstChild);
}else{
_130.appendChild(node);
}
return true;
};
dojo.dom.insertBefore=function(node,ref,_133){
if(_133!=true&&(node===ref||node.nextSibling===ref)){
return false;
}
var _134=ref.parentNode;
_134.insertBefore(node,ref);
return true;
};
dojo.dom.insertAfter=function(node,ref,_137){
var pn=ref.parentNode;
if(ref==pn.lastChild){
if((_137!=true)&&(node===ref)){
return false;
}
pn.appendChild(node);
}else{
return this.insertBefore(node,ref.nextSibling,_137);
}
return true;
};
dojo.dom.insertAtPosition=function(node,ref,_13b){
if((!node)||(!ref)||(!_13b)){
return false;
}
switch(_13b.toLowerCase()){
case "before":
return dojo.dom.insertBefore(node,ref);
case "after":
return dojo.dom.insertAfter(node,ref);
case "first":
if(ref.firstChild){
return dojo.dom.insertBefore(node,ref.firstChild);
}else{
ref.appendChild(node);
return true;
}
break;
default:
ref.appendChild(node);
return true;
}
};
dojo.dom.insertAtIndex=function(node,_13d,_13e){
var _13f=_13d.childNodes;
if(!_13f.length){
_13d.appendChild(node);
return true;
}
var _140=null;
for(var i=0;i<_13f.length;i++){
var _142=_13f.item(i)["getAttribute"]?parseInt(_13f.item(i).getAttribute("dojoinsertionindex")):-1;
if(_142<_13e){
_140=_13f.item(i);
}
}
if(_140){
return dojo.dom.insertAfter(node,_140);
}else{
return dojo.dom.insertBefore(node,_13f.item(0));
}
};
dojo.dom.textContent=function(node,text){
if(text){
dojo.dom.replaceChildren(node,document.createTextNode(text));
return text;
}else{
var _145="";
if(node==null){
return _145;
}
for(var i=0;i<node.childNodes.length;i++){
switch(node.childNodes[i].nodeType){
case 1:
case 5:
_145+=dojo.dom.textContent(node.childNodes[i]);
break;
case 3:
case 2:
case 4:
_145+=node.childNodes[i].nodeValue;
break;
default:
break;
}
}
return _145;
}
};
dojo.dom.collectionToArray=function(_147){
dojo.deprecated("dojo.dom.collectionToArray","use dojo.lang.toArray instead","0.4");
return dojo.lang.toArray(_147);
};
dojo.dom.hasParent=function(node){
return node&&node.parentNode&&dojo.dom.isNode(node.parentNode);
};
dojo.dom.isTag=function(node){
if(node&&node.tagName){
var arr=dojo.lang.toArray(arguments,1);
return arr[dojo.lang.find(node.tagName,arr)]||"";
}
return "";
};
dojo.provide("dojo.graphics.color");
dojo.require("dojo.lang.array");
dojo.graphics.color.Color=function(r,g,b,a){
if(dojo.lang.isArray(r)){
this.r=r[0];
this.g=r[1];
this.b=r[2];
this.a=r[3]||1;
}else{
if(dojo.lang.isString(r)){
var rgb=dojo.graphics.color.extractRGB(r);
this.r=rgb[0];
this.g=rgb[1];
this.b=rgb[2];
this.a=g||1;
}else{
if(r instanceof dojo.graphics.color.Color){
this.r=r.r;
this.b=r.b;
this.g=r.g;
this.a=r.a;
}else{
this.r=r;
this.g=g;
this.b=b;
this.a=a;
}
}
}
};
dojo.graphics.color.Color.fromArray=function(arr){
return new dojo.graphics.color.Color(arr[0],arr[1],arr[2],arr[3]);
};
dojo.lang.extend(dojo.graphics.color.Color,{toRgb:function(_151){
if(_151){
return this.toRgba();
}else{
return [this.r,this.g,this.b];
}
},toRgba:function(){
return [this.r,this.g,this.b,this.a];
},toHex:function(){
return dojo.graphics.color.rgb2hex(this.toRgb());
},toCss:function(){
return "rgb("+this.toRgb().join()+")";
},toString:function(){
return this.toHex();
},blend:function(_152,_153){
return dojo.graphics.color.blend(this.toRgb(),new Color(_152).toRgb(),_153);
}});
dojo.graphics.color.named={white:[255,255,255],black:[0,0,0],red:[255,0,0],green:[0,255,0],blue:[0,0,255],navy:[0,0,128],gray:[128,128,128],silver:[192,192,192]};
dojo.graphics.color.blend=function(a,b,_156){
if(typeof a=="string"){
return dojo.graphics.color.blendHex(a,b,_156);
}
if(!_156){
_156=0;
}else{
if(_156>1){
_156=1;
}else{
if(_156<-1){
_156=-1;
}
}
}
var c=new Array(3);
for(var i=0;i<3;i++){
var half=Math.abs(a[i]-b[i])/2;
c[i]=Math.floor(Math.min(a[i],b[i])+half+(half*_156));
}
return c;
};
dojo.graphics.color.blendHex=function(a,b,_15c){
return dojo.graphics.color.rgb2hex(dojo.graphics.color.blend(dojo.graphics.color.hex2rgb(a),dojo.graphics.color.hex2rgb(b),_15c));
};
dojo.graphics.color.extractRGB=function(_15d){
var hex="0123456789abcdef";
_15d=_15d.toLowerCase();
if(_15d.indexOf("rgb")==0){
var _15f=_15d.match(/rgba*\((\d+), *(\d+), *(\d+)/i);
var ret=_15f.splice(1,3);
return ret;
}else{
var _161=dojo.graphics.color.hex2rgb(_15d);
if(_161){
return _161;
}else{
return dojo.graphics.color.named[_15d]||[255,255,255];
}
}
};
dojo.graphics.color.hex2rgb=function(hex){
var _163="0123456789ABCDEF";
var rgb=new Array(3);
if(hex.indexOf("#")==0){
hex=hex.substring(1);
}
hex=hex.toUpperCase();
if(hex.replace(new RegExp("["+_163+"]","g"),"")!=""){
return null;
}
if(hex.length==3){
rgb[0]=hex.charAt(0)+hex.charAt(0);
rgb[1]=hex.charAt(1)+hex.charAt(1);
rgb[2]=hex.charAt(2)+hex.charAt(2);
}else{
rgb[0]=hex.substring(0,2);
rgb[1]=hex.substring(2,4);
rgb[2]=hex.substring(4);
}
for(var i=0;i<rgb.length;i++){
rgb[i]=_163.indexOf(rgb[i].charAt(0))*16+_163.indexOf(rgb[i].charAt(1));
}
return rgb;
};
dojo.graphics.color.rgb2hex=function(r,g,b){
if(dojo.lang.isArray(r)){
g=r[1]||0;
b=r[2]||0;
r=r[0]||0;
}
var ret=dojo.lang.map([r,g,b],function(x){
x=new Number(x);
var s=x.toString(16);
while(s.length<2){
s="0"+s;
}
return s;
});
ret.unshift("#");
return ret.join("");
};
dojo.provide("dojo.uri.Uri");
dojo.uri=new function(){
this.joinPath=function(){
var arr=[];
for(var i=0;i<arguments.length;i++){
arr.push(arguments[i]);
}
return arr.join("/").replace(/\/{2,}/g,"/").replace(/((https*|ftps*):)/i,"$1/");
};
this.dojoUri=function(uri){
return new dojo.uri.Uri(dojo.hostenv.getBaseScriptUri(),uri);
};
this.Uri=function(){
var uri=arguments[0];
for(var i=1;i<arguments.length;i++){
if(!arguments[i]){
continue;
}
var _171=new dojo.uri.Uri(arguments[i].toString());
var _172=new dojo.uri.Uri(uri.toString());
if(_171.path==""&&_171.scheme==null&&_171.authority==null&&_171.query==null){
if(_171.fragment!=null){
_172.fragment=_171.fragment;
}
_171=_172;
}else{
if(_171.scheme==null){
_171.scheme=_172.scheme;
if(_171.authority==null){
_171.authority=_172.authority;
if(_171.path.charAt(0)!="/"){
var path=_172.path.substring(0,_172.path.lastIndexOf("/")+1)+_171.path;
var segs=path.split("/");
for(var j=0;j<segs.length;j++){
if(segs[j]=="."){
if(j==segs.length-1){
segs[j]="";
}else{
segs.splice(j,1);
j--;
}
}else{
if(j>0&&!(j==1&&segs[0]=="")&&segs[j]==".."&&segs[j-1]!=".."){
if(j==segs.length-1){
segs.splice(j,1);
segs[j-1]="";
}else{
segs.splice(j-1,2);
j-=2;
}
}
}
}
_171.path=segs.join("/");
}
}
}
}
uri="";
if(_171.scheme!=null){
uri+=_171.scheme+":";
}
if(_171.authority!=null){
uri+="//"+_171.authority;
}
uri+=_171.path;
if(_171.query!=null){
uri+="?"+_171.query;
}
if(_171.fragment!=null){
uri+="#"+_171.fragment;
}
}
this.uri=uri.toString();
var _176="^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\\?([^#]*))?(#(.*))?$";
var r=this.uri.match(new RegExp(_176));
this.scheme=r[2]||(r[1]?"":null);
this.authority=r[4]||(r[3]?"":null);
this.path=r[5];
this.query=r[7]||(r[6]?"":null);
this.fragment=r[9]||(r[8]?"":null);
if(this.authority!=null){
_176="^((([^:]+:)?([^@]+))@)?([^:]*)(:([0-9]+))?$";
r=this.authority.match(new RegExp(_176));
this.user=r[3]||null;
this.password=r[4]||null;
this.host=r[5];
this.port=r[7]||null;
}
this.toString=function(){
return this.uri;
};
};
};
dojo.provide("dojo.style");
dojo.require("dojo.graphics.color");
dojo.require("dojo.uri.Uri");
dojo.require("dojo.lang.common");
(function(){
var h=dojo.render.html;
var ds=dojo.style;
var db=document["body"]||document["documentElement"];
ds.boxSizing={MARGIN_BOX:"margin-box",BORDER_BOX:"border-box",PADDING_BOX:"padding-box",CONTENT_BOX:"content-box"};
var bs=ds.boxSizing;
ds.getBoxSizing=function(node){
if((h.ie)||(h.opera)){
var cm=document["compatMode"];
if((cm=="BackCompat")||(cm=="QuirksMode")){
return bs.BORDER_BOX;
}else{
return bs.CONTENT_BOX;
}
}else{
if(arguments.length==0){
node=document.documentElement;
}
var _17e=ds.getStyle(node,"-moz-box-sizing");
if(!_17e){
_17e=ds.getStyle(node,"box-sizing");
}
return (_17e?_17e:bs.CONTENT_BOX);
}
};
ds.isBorderBox=function(node){
return (ds.getBoxSizing(node)==bs.BORDER_BOX);
};
ds.getUnitValue=function(node,_181,_182){
var s=ds.getComputedStyle(node,_181);
if((!s)||((s=="auto")&&(_182))){
return {value:0,units:"px"};
}
if(dojo.lang.isUndefined(s)){
return ds.getUnitValue.bad;
}
var _184=s.match(/(\-?[\d.]+)([a-z%]*)/i);
if(!_184){
return ds.getUnitValue.bad;
}
return {value:Number(_184[1]),units:_184[2].toLowerCase()};
};
ds.getUnitValue.bad={value:NaN,units:""};
ds.getPixelValue=function(node,_186,_187){
var _188=ds.getUnitValue(node,_186,_187);
if(isNaN(_188.value)){
return 0;
}
if((_188.value)&&(_188.units!="px")){
return NaN;
}
return _188.value;
};
ds.getNumericStyle=function(){
dojo.deprecated("dojo.(style|html).getNumericStyle","in favor of dojo.(style|html).getPixelValue","0.4");
return ds.getPixelValue.apply(this,arguments);
};
ds.setPositivePixelValue=function(node,_18a,_18b){
if(isNaN(_18b)){
return false;
}
node.style[_18a]=Math.max(0,_18b)+"px";
return true;
};
ds._sumPixelValues=function(node,_18d,_18e){
var _18f=0;
for(x=0;x<_18d.length;x++){
_18f+=ds.getPixelValue(node,_18d[x],_18e);
}
return _18f;
};
ds.isPositionAbsolute=function(node){
return (ds.getComputedStyle(node,"position")=="absolute");
};
ds.getBorderExtent=function(node,side){
return (ds.getStyle(node,"border-"+side+"-style")=="none"?0:ds.getPixelValue(node,"border-"+side+"-width"));
};
ds.getMarginWidth=function(node){
return ds._sumPixelValues(node,["margin-left","margin-right"],ds.isPositionAbsolute(node));
};
ds.getBorderWidth=function(node){
return ds.getBorderExtent(node,"left")+ds.getBorderExtent(node,"right");
};
ds.getPaddingWidth=function(node){
return ds._sumPixelValues(node,["padding-left","padding-right"],true);
};
ds.getPadBorderWidth=function(node){
return ds.getPaddingWidth(node)+ds.getBorderWidth(node);
};
ds.getContentBoxWidth=function(node){
node=dojo.byId(node);
return node.offsetWidth-ds.getPadBorderWidth(node);
};
ds.getBorderBoxWidth=function(node){
node=dojo.byId(node);
return node.offsetWidth;
};
ds.getMarginBoxWidth=function(node){
return ds.getInnerWidth(node)+ds.getMarginWidth(node);
};
ds.setContentBoxWidth=function(node,_19b){
node=dojo.byId(node);
if(ds.isBorderBox(node)){
_19b+=ds.getPadBorderWidth(node);
}
return ds.setPositivePixelValue(node,"width",_19b);
};
ds.setMarginBoxWidth=function(node,_19d){
node=dojo.byId(node);
if(!ds.isBorderBox(node)){
_19d-=ds.getPadBorderWidth(node);
}
_19d-=ds.getMarginWidth(node);
return ds.setPositivePixelValue(node,"width",_19d);
};
ds.getContentWidth=ds.getContentBoxWidth;
ds.getInnerWidth=ds.getBorderBoxWidth;
ds.getOuterWidth=ds.getMarginBoxWidth;
ds.setContentWidth=ds.setContentBoxWidth;
ds.setOuterWidth=ds.setMarginBoxWidth;
ds.getMarginHeight=function(node){
return ds._sumPixelValues(node,["margin-top","margin-bottom"],ds.isPositionAbsolute(node));
};
ds.getBorderHeight=function(node){
return ds.getBorderExtent(node,"top")+ds.getBorderExtent(node,"bottom");
};
ds.getPaddingHeight=function(node){
return ds._sumPixelValues(node,["padding-top","padding-bottom"],true);
};
ds.getPadBorderHeight=function(node){
return ds.getPaddingHeight(node)+ds.getBorderHeight(node);
};
ds.getContentBoxHeight=function(node){
node=dojo.byId(node);
return node.offsetHeight-ds.getPadBorderHeight(node);
};
ds.getBorderBoxHeight=function(node){
node=dojo.byId(node);
return node.offsetHeight;
};
ds.getMarginBoxHeight=function(node){
return ds.getInnerHeight(node)+ds.getMarginHeight(node);
};
ds.setContentBoxHeight=function(node,_1a6){
node=dojo.byId(node);
if(ds.isBorderBox(node)){
_1a6+=ds.getPadBorderHeight(node);
}
return ds.setPositivePixelValue(node,"height",_1a6);
};
ds.setMarginBoxHeight=function(node,_1a8){
node=dojo.byId(node);
if(!ds.isBorderBox(node)){
_1a8-=ds.getPadBorderHeight(node);
}
_1a8-=ds.getMarginHeight(node);
return ds.setPositivePixelValue(node,"height",_1a8);
};
ds.getContentHeight=ds.getContentBoxHeight;
ds.getInnerHeight=ds.getBorderBoxHeight;
ds.getOuterHeight=ds.getMarginBoxHeight;
ds.setContentHeight=ds.setContentBoxHeight;
ds.setOuterHeight=ds.setMarginBoxHeight;
ds.getAbsolutePosition=ds.abs=function(node,_1aa){
var ret=[];
ret.x=ret.y=0;
var st=dojo.html.getScrollTop();
var sl=dojo.html.getScrollLeft();
if(h.ie){
with(node.getBoundingClientRect()){
ret.x=left-2;
ret.y=top-2;
}
}else{
if(node["offsetParent"]){
var _1ae;
if((h.safari)&&(node.style.getPropertyValue("position")=="absolute")&&(node.parentNode==db)){
_1ae=db;
}else{
_1ae=db.parentNode;
}
if(node.parentNode!=db){
ret.x-=ds.sumAncestorProperties(node,"scrollLeft");
ret.y-=ds.sumAncestorProperties(node,"scrollTop");
}
do{
var n=node["offsetLeft"];
ret.x+=isNaN(n)?0:n;
var m=node["offsetTop"];
ret.y+=isNaN(m)?0:m;
node=node.offsetParent;
}while((node!=_1ae)&&(node!=null));
}else{
if(node["x"]&&node["y"]){
ret.x+=isNaN(node.x)?0:node.x;
ret.y+=isNaN(node.y)?0:node.y;
}
}
}
if(_1aa){
ret.y+=st;
ret.x+=sl;
}
ret[0]=ret.x;
ret[1]=ret.y;
return ret;
};
ds.sumAncestorProperties=function(node,prop){
node=dojo.byId(node);
if(!node){
return 0;
}
var _1b3=0;
while(node){
var val=node[prop];
if(val){
_1b3+=val-0;
}
node=node.parentNode;
}
return _1b3;
};
ds.getTotalOffset=function(node,type,_1b7){
node=dojo.byId(node);
return ds.abs(node,_1b7)[(type=="top")?"y":"x"];
};
ds.getAbsoluteX=ds.totalOffsetLeft=function(node,_1b9){
return ds.getTotalOffset(node,"left",_1b9);
};
ds.getAbsoluteY=ds.totalOffsetTop=function(node,_1bb){
return ds.getTotalOffset(node,"top",_1bb);
};
ds.styleSheet=null;
ds.insertCssRule=function(_1bc,_1bd,_1be){
if(!ds.styleSheet){
if(document.createStyleSheet){
ds.styleSheet=document.createStyleSheet();
}else{
if(document.styleSheets[0]){
ds.styleSheet=document.styleSheets[0];
}else{
return null;
}
}
}
if(arguments.length<3){
if(ds.styleSheet.cssRules){
_1be=ds.styleSheet.cssRules.length;
}else{
if(ds.styleSheet.rules){
_1be=ds.styleSheet.rules.length;
}else{
return null;
}
}
}
if(ds.styleSheet.insertRule){
var rule=_1bc+" { "+_1bd+" }";
return ds.styleSheet.insertRule(rule,_1be);
}else{
if(ds.styleSheet.addRule){
return ds.styleSheet.addRule(_1bc,_1bd,_1be);
}else{
return null;
}
}
};
ds.removeCssRule=function(_1c0){
if(!ds.styleSheet){
dojo.debug("no stylesheet defined for removing rules");
return false;
}
if(h.ie){
if(!_1c0){
_1c0=ds.styleSheet.rules.length;
ds.styleSheet.removeRule(_1c0);
}
}else{
if(document.styleSheets[0]){
if(!_1c0){
_1c0=ds.styleSheet.cssRules.length;
}
ds.styleSheet.deleteRule(_1c0);
}
}
return true;
};
ds.insertCssFile=function(URI,doc,_1c3){
if(!URI){
return;
}
if(!doc){
doc=document;
}
var _1c4=dojo.hostenv.getText(URI);
_1c4=ds.fixPathsInCssText(_1c4,URI);
if(_1c3){
var _1c5=doc.getElementsByTagName("style");
var _1c6="";
for(var i=0;i<_1c5.length;i++){
_1c6=(_1c5[i].styleSheet&&_1c5[i].styleSheet.cssText)?_1c5[i].styleSheet.cssText:_1c5[i].innerHTML;
if(_1c4==_1c6){
return;
}
}
}
var _1c8=ds.insertCssText(_1c4);
if(_1c8&&djConfig.isDebug){
_1c8.setAttribute("dbgHref",URI);
}
return _1c8;
};
ds.insertCssText=function(_1c9,doc,URI){
if(!_1c9){
return;
}
if(!doc){
doc=document;
}
if(URI){
_1c9=ds.fixPathsInCssText(_1c9,URI);
}
var _1cc=doc.createElement("style");
_1cc.setAttribute("type","text/css");
if(_1cc.styleSheet){
_1cc.styleSheet.cssText=_1c9;
}else{
var _1cd=doc.createTextNode(_1c9);
_1cc.appendChild(_1cd);
}
var head=doc.getElementsByTagName("head")[0];
if(!head){
dojo.debug("No head tag in document, aborting styles");
}else{
head.appendChild(_1cc);
}
return _1cc;
};
ds.fixPathsInCssText=function(_1cf,URI){
if(!_1cf||!URI){
return;
}
var pos=0;
var str="";
var url="";
while(pos!=-1){
pos=0;
url="";
pos=_1cf.indexOf("url(",pos);
if(pos<0){
break;
}
str+=_1cf.slice(0,pos+4);
_1cf=_1cf.substring(pos+4,_1cf.length);
url+=_1cf.match(/^[\t\s\w()\/.\\'"-:#=&?]*\)/)[0];
_1cf=_1cf.substring(url.length-1,_1cf.length);
url=url.replace(/^[\s\t]*(['"]?)([\w()\/.\\'"-:#=&?]*)\1[\s\t]*?\)/,"$2");
if(url.search(/(file|https?|ftps?):\/\//)==-1){
url=(new dojo.uri.Uri(URI,url).toString());
}
str+=url;
}
return str+_1cf;
};
ds.getBackgroundColor=function(node){
node=dojo.byId(node);
var _1d5;
do{
_1d5=ds.getStyle(node,"background-color");
if(_1d5.toLowerCase()=="rgba(0, 0, 0, 0)"){
_1d5="transparent";
}
if(node==document.getElementsByTagName("body")[0]){
node=null;
break;
}
node=node.parentNode;
}while(node&&dojo.lang.inArray(_1d5,["transparent",""]));
if(_1d5=="transparent"){
_1d5=[255,255,255,0];
}else{
_1d5=dojo.graphics.color.extractRGB(_1d5);
}
return _1d5;
};
ds.getComputedStyle=function(node,_1d7,_1d8){
node=dojo.byId(node);
var _1d7=ds.toSelectorCase(_1d7);
var _1d9=ds.toCamelCase(_1d7);
if(!node||!node.style){
return _1d8;
}else{
if(document.defaultView){
try{
var cs=document.defaultView.getComputedStyle(node,"");
if(cs){
return cs.getPropertyValue(_1d7);
}
}
catch(e){
if(node.style.getPropertyValue){
return node.style.getPropertyValue(_1d7);
}else{
return _1d8;
}
}
}else{
if(node.currentStyle){
return node.currentStyle[_1d9];
}
}
}
if(node.style.getPropertyValue){
return node.style.getPropertyValue(_1d7);
}else{
return _1d8;
}
};
ds.getStyleProperty=function(node,_1dc){
node=dojo.byId(node);
return (node&&node.style?node.style[ds.toCamelCase(_1dc)]:undefined);
};
ds.getStyle=function(node,_1de){
var _1df=ds.getStyleProperty(node,_1de);
return (_1df?_1df:ds.getComputedStyle(node,_1de));
};
ds.setStyle=function(node,_1e1,_1e2){
node=dojo.byId(node);
if(node&&node.style){
var _1e3=ds.toCamelCase(_1e1);
node.style[_1e3]=_1e2;
}
};
ds.toCamelCase=function(_1e4){
var arr=_1e4.split("-"),cc=arr[0];
for(var i=1;i<arr.length;i++){
cc+=arr[i].charAt(0).toUpperCase()+arr[i].substring(1);
}
return cc;
};
ds.toSelectorCase=function(_1e7){
return _1e7.replace(/([A-Z])/g,"-$1").toLowerCase();
};
ds.setOpacity=function setOpacity(node,_1e9,_1ea){
node=dojo.byId(node);
if(!_1ea){
if(_1e9>=1){
if(h.ie){
ds.clearOpacity(node);
return;
}else{
_1e9=0.999999;
}
}else{
if(_1e9<0){
_1e9=0;
}
}
}
if(h.ie){
if(node.nodeName.toLowerCase()=="tr"){
var tds=node.getElementsByTagName("td");
for(var x=0;x<tds.length;x++){
tds[x].style.filter="Alpha(Opacity="+_1e9*100+")";
}
}
node.style.filter="Alpha(Opacity="+_1e9*100+")";
}else{
if(h.moz){
node.style.opacity=_1e9;
node.style.MozOpacity=_1e9;
}else{
if(h.safari){
node.style.opacity=_1e9;
node.style.KhtmlOpacity=_1e9;
}else{
node.style.opacity=_1e9;
}
}
}
};
ds.getOpacity=function getOpacity(node){
node=dojo.byId(node);
if(h.ie){
var opac=(node.filters&&node.filters.alpha&&typeof node.filters.alpha.opacity=="number"?node.filters.alpha.opacity:100)/100;
}else{
var opac=node.style.opacity||node.style.MozOpacity||node.style.KhtmlOpacity||1;
}
return opac>=0.999999?1:Number(opac);
};
ds.clearOpacity=function clearOpacity(node){
node=dojo.byId(node);
var ns=node.style;
if(h.ie){
try{
if(node.filters&&node.filters.alpha){
ns.filter="";
}
}
catch(e){
}
}else{
if(h.moz){
ns.opacity=1;
ns.MozOpacity=1;
}else{
if(h.safari){
ns.opacity=1;
ns.KhtmlOpacity=1;
}else{
ns.opacity=1;
}
}
}
};
ds._toggle=function(node,_1f2,_1f3){
node=dojo.byId(node);
_1f3(node,!_1f2(node));
return _1f2(node);
};
ds.show=function(node){
node=dojo.byId(node);
if(ds.getStyleProperty(node,"display")=="none"){
ds.setStyle(node,"display",(node.dojoDisplayCache||""));
node.dojoDisplayCache=undefined;
}
};
ds.hide=function(node){
node=dojo.byId(node);
if(typeof node["dojoDisplayCache"]=="undefined"){
var d=ds.getStyleProperty(node,"display");
if(d!="none"){
node.dojoDisplayCache=d;
}
}
ds.setStyle(node,"display","none");
};
ds.setShowing=function(node,_1f8){
ds[(_1f8?"show":"hide")](node);
};
ds.isShowing=function(node){
return (ds.getStyleProperty(node,"display")!="none");
};
ds.toggleShowing=function(node){
return ds._toggle(node,ds.isShowing,ds.setShowing);
};
ds.displayMap={tr:"",td:"",th:"",img:"inline",span:"inline",input:"inline",button:"inline"};
ds.suggestDisplayByTagName=function(node){
node=dojo.byId(node);
if(node&&node.tagName){
var tag=node.tagName.toLowerCase();
return (tag in ds.displayMap?ds.displayMap[tag]:"block");
}
};
ds.setDisplay=function(node,_1fe){
ds.setStyle(node,"display",(dojo.lang.isString(_1fe)?_1fe:(_1fe?ds.suggestDisplayByTagName(node):"none")));
};
ds.isDisplayed=function(node){
return (ds.getComputedStyle(node,"display")!="none");
};
ds.toggleDisplay=function(node){
return ds._toggle(node,ds.isDisplayed,ds.setDisplay);
};
ds.setVisibility=function(node,_202){
ds.setStyle(node,"visibility",(dojo.lang.isString(_202)?_202:(_202?"visible":"hidden")));
};
ds.isVisible=function(node){
return (ds.getComputedStyle(node,"visibility")!="hidden");
};
ds.toggleVisibility=function(node){
return ds._toggle(node,ds.isVisible,ds.setVisibility);
};
ds.toCoordinateArray=function(_205,_206){
if(dojo.lang.isArray(_205)){
while(_205.length<4){
_205.push(0);
}
while(_205.length>4){
_205.pop();
}
var ret=_205;
}else{
var node=dojo.byId(_205);
var pos=ds.getAbsolutePosition(node,_206);
var ret=[pos.x,pos.y,ds.getBorderBoxWidth(node),ds.getBorderBoxHeight(node)];
}
ret.x=ret[0];
ret.y=ret[1];
ret.w=ret[2];
ret.h=ret[3];
return ret;
};
})();
dojo.provide("dojo.string.common");
dojo.require("dojo.string");
dojo.string.trim=function(str,wh){
if(!str.replace){
return str;
}
if(!str.length){
return str;
}
var re=(wh>0)?(/^\s+/):(wh<0)?(/\s+$/):(/^\s+|\s+$/g);
return str.replace(re,"");
};
dojo.string.trimStart=function(str){
return dojo.string.trim(str,1);
};
dojo.string.trimEnd=function(str){
return dojo.string.trim(str,-1);
};
dojo.string.repeat=function(str,_210,_211){
var out="";
for(var i=0;i<_210;i++){
out+=str;
if(_211&&i<_210-1){
out+=_211;
}
}
return out;
};
dojo.string.pad=function(str,len,c,dir){
var out=String(str);
if(!c){
c="0";
}
if(!dir){
dir=1;
}
while(out.length<len){
if(dir>0){
out=c+out;
}else{
out+=c;
}
}
return out;
};
dojo.string.padLeft=function(str,len,c){
return dojo.string.pad(str,len,c,1);
};
dojo.string.padRight=function(str,len,c){
return dojo.string.pad(str,len,c,-1);
};
dojo.provide("dojo.string");
dojo.require("dojo.string.common");
dojo.provide("dojo.html");
dojo.require("dojo.lang.func");
dojo.require("dojo.dom");
dojo.require("dojo.style");
dojo.require("dojo.string");
dojo.lang.mixin(dojo.html,dojo.dom);
dojo.lang.mixin(dojo.html,dojo.style);
dojo.html.clearSelection=function(){
try{
if(window["getSelection"]){
if(dojo.render.html.safari){
window.getSelection().collapse();
}else{
window.getSelection().removeAllRanges();
}
}else{
if(document.selection){
if(document.selection.empty){
document.selection.empty();
}else{
if(document.selection.clear){
document.selection.clear();
}
}
}
}
return true;
}
catch(e){
dojo.debug(e);
return false;
}
};
dojo.html.disableSelection=function(_21f){
_21f=dojo.byId(_21f)||document.body;
var h=dojo.render.html;
if(h.mozilla){
_21f.style.MozUserSelect="none";
}else{
if(h.safari){
_21f.style.KhtmlUserSelect="none";
}else{
if(h.ie){
_21f.unselectable="on";
}else{
return false;
}
}
}
return true;
};
dojo.html.enableSelection=function(_221){
_221=dojo.byId(_221)||document.body;
var h=dojo.render.html;
if(h.mozilla){
_221.style.MozUserSelect="";
}else{
if(h.safari){
_221.style.KhtmlUserSelect="";
}else{
if(h.ie){
_221.unselectable="off";
}else{
return false;
}
}
}
return true;
};
dojo.html.selectElement=function(_223){
_223=dojo.byId(_223);
if(document.selection&&document.body.createTextRange){
var _224=document.body.createTextRange();
_224.moveToElementText(_223);
_224.select();
}else{
if(window["getSelection"]){
var _225=window.getSelection();
if(_225["selectAllChildren"]){
_225.selectAllChildren(_223);
}
}
}
};
dojo.html.selectInputText=function(_226){
_226=dojo.byId(_226);
if(document.selection&&document.body.createTextRange){
var _227=_226.createTextRange();
_227.moveStart("character",0);
_227.moveEnd("character",_226.value.length);
_227.select();
}else{
if(window["getSelection"]){
var _228=window.getSelection();
_226.setSelectionRange(0,_226.value.length);
}
}
_226.focus();
};
dojo.html.isSelectionCollapsed=function(){
if(document["selection"]){
return document.selection.createRange().text=="";
}else{
if(window["getSelection"]){
var _229=window.getSelection();
if(dojo.lang.isString(_229)){
return _229=="";
}else{
return _229.isCollapsed;
}
}
}
};
dojo.html.getEventTarget=function(evt){
if(!evt){
evt=window.event||{};
}
var t=(evt.srcElement?evt.srcElement:(evt.target?evt.target:null));
while((t)&&(t.nodeType!=1)){
t=t.parentNode;
}
return t;
};
dojo.html.getDocumentWidth=function(){
dojo.deprecated("dojo.html.getDocument* has been deprecated in favor of dojo.html.getViewport*");
return dojo.html.getViewportWidth();
};
dojo.html.getDocumentHeight=function(){
dojo.deprecated("dojo.html.getDocument* has been deprecated in favor of dojo.html.getViewport*");
return dojo.html.getViewportHeight();
};
dojo.html.getDocumentSize=function(){
dojo.deprecated("dojo.html.getDocument* has been deprecated in favor of dojo.html.getViewport*");
return dojo.html.getViewportSize();
};
dojo.html.getViewportWidth=function(){
var w=0;
if(window.innerWidth){
w=window.innerWidth;
}
if(dojo.exists(document,"documentElement.clientWidth")){
var w2=document.documentElement.clientWidth;
if(!w||w2&&w2<w){
w=w2;
}
return w;
}
if(document.body){
return document.body.clientWidth;
}
return 0;
};
dojo.html.getViewportHeight=function(){
if(window.innerHeight){
return window.innerHeight;
}
if(dojo.exists(document,"documentElement.clientHeight")){
return document.documentElement.clientHeight;
}
if(document.body){
return document.body.clientHeight;
}
return 0;
};
dojo.html.getViewportSize=function(){
var ret=[dojo.html.getViewportWidth(),dojo.html.getViewportHeight()];
ret.w=ret[0];
ret.h=ret[1];
return ret;
};
dojo.html.getScrollTop=function(){
return window.pageYOffset||document.documentElement.scrollTop||document.body.scrollTop||0;
};
dojo.html.getScrollLeft=function(){
return window.pageXOffset||document.documentElement.scrollLeft||document.body.scrollLeft||0;
};
dojo.html.getScrollOffset=function(){
var off=[dojo.html.getScrollLeft(),dojo.html.getScrollTop()];
off.x=off[0];
off.y=off[1];
return off;
};
dojo.html.getParentOfType=function(node,type){
dojo.deprecated("dojo.html.getParentOfType has been deprecated in favor of dojo.html.getParentByType*");
return dojo.html.getParentByType(node,type);
};
dojo.html.getParentByType=function(node,type){
var _234=dojo.byId(node);
type=type.toLowerCase();
while((_234)&&(_234.nodeName.toLowerCase()!=type)){
if(_234==(document["body"]||document["documentElement"])){
return null;
}
_234=_234.parentNode;
}
return _234;
};
dojo.html.getAttribute=function(node,attr){
node=dojo.byId(node);
if((!node)||(!node.getAttribute)){
return null;
}
var ta=typeof attr=="string"?attr:new String(attr);
var v=node.getAttribute(ta.toUpperCase());
if((v)&&(typeof v=="string")&&(v!="")){
return v;
}
if(v&&v.value){
return v.value;
}
if((node.getAttributeNode)&&(node.getAttributeNode(ta))){
return (node.getAttributeNode(ta)).value;
}else{
if(node.getAttribute(ta)){
return node.getAttribute(ta);
}else{
if(node.getAttribute(ta.toLowerCase())){
return node.getAttribute(ta.toLowerCase());
}
}
}
return null;
};
dojo.html.hasAttribute=function(node,attr){
node=dojo.byId(node);
return dojo.html.getAttribute(node,attr)?true:false;
};
dojo.html.getClass=function(node){
node=dojo.byId(node);
if(!node){
return "";
}
var cs="";
if(node.className){
cs=node.className;
}else{
if(dojo.html.hasAttribute(node,"class")){
cs=dojo.html.getAttribute(node,"class");
}
}
return dojo.string.trim(cs);
};
dojo.html.getClasses=function(node){
var c=dojo.html.getClass(node);
return (c=="")?[]:c.split(/\s+/g);
};
dojo.html.hasClass=function(node,_240){
return dojo.lang.inArray(dojo.html.getClasses(node),_240);
};
dojo.html.prependClass=function(node,_242){
_242+=" "+dojo.html.getClass(node);
return dojo.html.setClass(node,_242);
};
dojo.html.addClass=function(node,_244){
if(dojo.html.hasClass(node,_244)){
return false;
}
_244=dojo.string.trim(dojo.html.getClass(node)+" "+_244);
return dojo.html.setClass(node,_244);
};
dojo.html.setClass=function(node,_246){
node=dojo.byId(node);
var cs=new String(_246);
try{
if(typeof node.className=="string"){
node.className=cs;
}else{
if(node.setAttribute){
node.setAttribute("class",_246);
node.className=cs;
}else{
return false;
}
}
}
catch(e){
dojo.debug("dojo.html.setClass() failed",e);
}
return true;
};
dojo.html.removeClass=function(node,_249,_24a){
var _249=dojo.string.trim(new String(_249));
try{
var cs=dojo.html.getClasses(node);
var nca=[];
if(_24a){
for(var i=0;i<cs.length;i++){
if(cs[i].indexOf(_249)==-1){
nca.push(cs[i]);
}
}
}else{
for(var i=0;i<cs.length;i++){
if(cs[i]!=_249){
nca.push(cs[i]);
}
}
}
dojo.html.setClass(node,nca.join(" "));
}
catch(e){
dojo.debug("dojo.html.removeClass() failed",e);
}
return true;
};
dojo.html.replaceClass=function(node,_24f,_250){
dojo.html.removeClass(node,_250);
dojo.html.addClass(node,_24f);
};
dojo.html.classMatchType={ContainsAll:0,ContainsAny:1,IsOnly:2};
dojo.html.getElementsByClass=function(_251,_252,_253,_254){
_252=dojo.byId(_252)||document;
var _255=_251.split(/\s+/g);
var _256=[];
if(_254!=1&&_254!=2){
_254=0;
}
var _257=new RegExp("(\\s|^)(("+_255.join(")|(")+"))(\\s|$)");
if(!_253){
_253="*";
}
var _258=_252.getElementsByTagName(_253);
var node,i=0;
outer:
while(node=_258[i++]){
var _25a=dojo.html.getClasses(node);
if(_25a.length==0){
continue outer;
}
var _25b=0;
for(var j=0;j<_25a.length;j++){
if(_257.test(_25a[j])){
if(_254==dojo.html.classMatchType.ContainsAny){
_256.push(node);
continue outer;
}else{
_25b++;
}
}else{
if(_254==dojo.html.classMatchType.IsOnly){
continue outer;
}
}
}
if(_25b==_255.length){
if(_254==dojo.html.classMatchType.IsOnly&&_25b==_25a.length){
_256.push(node);
}else{
if(_254==dojo.html.classMatchType.ContainsAll){
_256.push(node);
}
}
}
}
return _256;
};
dojo.html.getElementsByClassName=dojo.html.getElementsByClass;
dojo.html.getCursorPosition=function(e){
e=e||window.event;
var _25e={x:0,y:0};
if(e.pageX||e.pageY){
_25e.x=e.pageX;
_25e.y=e.pageY;
}else{
var de=document.documentElement;
var db=document.body;
_25e.x=e.clientX+((de||db)["scrollLeft"])-((de||db)["clientLeft"]);
_25e.y=e.clientY+((de||db)["scrollTop"])-((de||db)["clientTop"]);
}
return _25e;
};
dojo.html.overElement=function(_261,e){
_261=dojo.byId(_261);
var _263=dojo.html.getCursorPosition(e);
with(dojo.html){
var top=getAbsoluteY(_261,true);
var _265=top+getInnerHeight(_261);
var left=getAbsoluteX(_261,true);
var _267=left+getInnerWidth(_261);
}
return (_263.x>=left&&_263.x<=_267&&_263.y>=top&&_263.y<=_265);
};
dojo.html.setActiveStyleSheet=function(_268){
var i=0,a,els=document.getElementsByTagName("link");
while(a=els[i++]){
if(a.getAttribute("rel").indexOf("style")!=-1&&a.getAttribute("title")){
a.disabled=true;
if(a.getAttribute("title")==_268){
a.disabled=false;
}
}
}
};
dojo.html.getActiveStyleSheet=function(){
var i=0,a,els=document.getElementsByTagName("link");
while(a=els[i++]){
if(a.getAttribute("rel").indexOf("style")!=-1&&a.getAttribute("title")&&!a.disabled){
return a.getAttribute("title");
}
}
return null;
};
dojo.html.getPreferredStyleSheet=function(){
var i=0,a,els=document.getElementsByTagName("link");
while(a=els[i++]){
if(a.getAttribute("rel").indexOf("style")!=-1&&a.getAttribute("rel").indexOf("alt")==-1&&a.getAttribute("title")){
return a.getAttribute("title");
}
}
return null;
};
dojo.html.body=function(){
dojo.deprecated("dojo.html.body","use document.body instead");
return document.body||document.getElementsByTagName("body")[0];
};
dojo.html.isTag=function(node){
node=dojo.byId(node);
if(node&&node.tagName){
var arr=dojo.lang.map(dojo.lang.toArray(arguments,1),function(a){
return String(a).toLowerCase();
});
return arr[dojo.lang.find(node.tagName.toLowerCase(),arr)]||"";
}
return "";
};
dojo.html._callExtrasDeprecated=function(_26f,args){
var _271="dojo.html.extras";
dojo.deprecated("dojo.html."+_26f+" has been moved to "+_271);
dojo["require"](_271);
return dojo.html[_26f].apply(dojo.html,args);
};
dojo.html.createNodesFromText=function(){
return dojo.html._callExtrasDeprecated("createNodesFromText",arguments);
};
dojo.html.gravity=function(){
return dojo.html._callExtrasDeprecated("gravity",arguments);
};
dojo.html.placeOnScreen=function(){
return dojo.html._callExtrasDeprecated("placeOnScreen",arguments);
};
dojo.html.placeOnScreenPoint=function(){
return dojo.html._callExtrasDeprecated("placeOnScreenPoint",arguments);
};
dojo.html.renderedTextContent=function(){
return dojo.html._callExtrasDeprecated("renderedTextContent",arguments);
};
dojo.html.BackgroundIframe=function(){
return dojo.html._callExtrasDeprecated("BackgroundIframe",arguments);
};
dojo.provide("dojo.lfx.Animation");
dojo.provide("dojo.lfx.Line");
dojo.require("dojo.lang.func");
dojo.lfx.Line=function(_272,end){
this.start=_272;
this.end=end;
if(dojo.lang.isArray(_272)){
var diff=[];
dojo.lang.forEach(this.start,function(s,i){
diff[i]=this.end[i]-s;
},this);
this.getValue=function(n){
var res=[];
dojo.lang.forEach(this.start,function(s,i){
res[i]=(diff[i]*n)+s;
},this);
return res;
};
}else{
var diff=end-_272;
this.getValue=function(n){
return (diff*n)+this.start;
};
}
};
dojo.lfx.easeIn=function(n){
return Math.pow(n,3);
};
dojo.lfx.easeOut=function(n){
return (1-Math.pow(1-n,3));
};
dojo.lfx.easeInOut=function(n){
return ((3*Math.pow(n,2))-(2*Math.pow(n,3)));
};
dojo.lfx.IAnimation=function(){
};
dojo.lang.extend(dojo.lfx.IAnimation,{curve:null,duration:1000,easing:null,repeatCount:0,rate:25,handler:null,beforeBegin:null,onBegin:null,onAnimate:null,onEnd:null,onPlay:null,onPause:null,onStop:null,play:null,pause:null,stop:null,fire:function(evt,args){
if(this[evt]){
if(args){
this[evt].apply(this,args);
}else{
this[evt].apply(this);
}
}
},_active:false,_paused:false});
dojo.lfx.Animation=function(_281,_282,_283,_284,_285,rate){
dojo.lfx.IAnimation.call(this);
if(dojo.lang.isNumber(_281)||(!_281&&_282.getValue)){
rate=_285;
_285=_284;
_284=_283;
_283=_282;
_282=_281;
_281=null;
}else{
if(_281.getValue||dojo.lang.isArray(_281)){
rate=_284;
_285=_283;
_284=_282;
_283=_281;
_282=null;
_281=null;
}
}
if(dojo.lang.isArray(_283)){
this.curve=new dojo.lfx.Line(_283[0],_283[1]);
}else{
this.curve=_283;
}
if(_282!=null&&_282>0){
this.duration=_282;
}
if(_285){
this.repeatCount=_285;
}
if(rate){
this.rate=rate;
}
if(_281){
this.handler=_281.handler;
this.beforeBegin=_281.beforeBegin;
this.onBegin=_281.onBegin;
this.onEnd=_281.onEnd;
this.onPlay=_281.onPlay;
this.onPause=_281.onPause;
this.onStop=_281.onStop;
this.onAnimate=_281.onAnimate;
}
if(_284&&dojo.lang.isFunction(_284)){
this.easing=_284;
}
};
dojo.inherits(dojo.lfx.Animation,dojo.lfx.IAnimation);
dojo.lang.extend(dojo.lfx.Animation,{_startTime:null,_endTime:null,_timer:null,_percent:0,_startRepeatCount:0,play:function(_287,_288){
if(_288){
clearTimeout(this._timer);
this._active=false;
this._paused=false;
this._percent=0;
}else{
if(this._active&&!this._paused){
return;
}
}
this.fire("beforeBegin");
if(_287>0){
setTimeout(dojo.lang.hitch(this,function(){
this.play(null,_288);
}),_287);
return;
}
this._startTime=new Date().valueOf();
if(this._paused){
this._startTime-=(this.duration*this._percent/100);
}
this._endTime=this._startTime+this.duration;
this._active=true;
this._paused=false;
var step=this._percent/100;
var _28a=this.curve.getValue(step);
if(this._percent==0){
if(!this._startRepeatCount){
this._startRepeatCount=this.repeatCount;
}
this.fire("handler",["begin",_28a]);
this.fire("onBegin",[_28a]);
}
this.fire("handler",["play",_28a]);
this.fire("onPlay",[_28a]);
this._cycle();
},pause:function(){
clearTimeout(this._timer);
if(!this._active){
return;
}
this._paused=true;
var _28b=this.curve.getValue(this._percent/100);
this.fire("handler",["pause",_28b]);
this.fire("onPause",[_28b]);
},gotoPercent:function(pct,_28d){
clearTimeout(this._timer);
this._active=true;
this._paused=true;
this._percent=pct;
if(_28d){
this.play();
}
},stop:function(_28e){
clearTimeout(this._timer);
var step=this._percent/100;
if(_28e){
step=1;
}
var _290=this.curve.getValue(step);
this.fire("handler",["stop",_290]);
this.fire("onStop",[_290]);
this._active=false;
this._paused=false;
},status:function(){
if(this._active){
return this._paused?"paused":"playing";
}else{
return "stopped";
}
},_cycle:function(){
clearTimeout(this._timer);
if(this._active){
var curr=new Date().valueOf();
var step=(curr-this._startTime)/(this._endTime-this._startTime);
if(step>=1){
step=1;
this._percent=100;
}else{
this._percent=step*100;
}
if(this.easing&&dojo.lang.isFunction(this.easing)){
step=this.easing(step);
}
var _293=this.curve.getValue(step);
this.fire("handler",["animate",_293]);
this.fire("onAnimate",[_293]);
if(step<1){
this._timer=setTimeout(dojo.lang.hitch(this,"_cycle"),this.rate);
}else{
this._active=false;
this.fire("handler",["end"]);
this.fire("onEnd");
if(this.repeatCount>0){
this.repeatCount--;
this.play(null,true);
}else{
if(this.repeatCount==-1){
this.play(null,true);
}else{
if(this._startRepeatCount){
this.repeatCount=this._startRepeatCount;
this._startRepeatCount=0;
}
}
}
}
}
}});
dojo.lfx.Combine=function(){
dojo.lfx.IAnimation.call(this);
this._anims=[];
this._animsEnded=0;
var _294=arguments;
if(_294.length==1&&(dojo.lang.isArray(_294[0])||dojo.lang.isArrayLike(_294[0]))){
_294=_294[0];
}
var _295=this;
dojo.lang.forEach(_294,function(anim){
_295._anims.push(anim);
dojo.event.connect(anim,"onEnd",function(){
_295._onAnimsEnded();
});
});
};
dojo.inherits(dojo.lfx.Combine,dojo.lfx.IAnimation);
dojo.lang.extend(dojo.lfx.Combine,{_animsEnded:0,play:function(_297,_298){
if(!this._anims.length){
return;
}
this.fire("beforeBegin");
if(_297>0){
setTimeout(dojo.lang.hitch(this,function(){
this.play(null,_298);
}),_297);
return;
}
if(_298||this._anims[0].percent==0){
this.fire("onBegin");
}
this.fire("onPlay");
this._animsCall("play",null,_298);
},pause:function(){
this.fire("onPause");
this._animsCall("pause");
},stop:function(_299){
this.fire("onStop");
this._animsCall("stop",_299);
},_onAnimsEnded:function(){
this._animsEnded++;
if(this._animsEnded>=this._anims.length){
this.fire("onEnd");
}
},_animsCall:function(_29a){
var args=[];
if(arguments.length>1){
for(var i=1;i<arguments.length;i++){
args.push(arguments[i]);
}
}
var _29d=this;
dojo.lang.forEach(this._anims,function(anim){
anim[_29a](args);
},_29d);
}});
dojo.lfx.Chain=function(){
dojo.lfx.IAnimation.call(this);
this._anims=[];
this._currAnim=-1;
var _29f=arguments;
if(_29f.length==1&&(dojo.lang.isArray(_29f[0])||dojo.lang.isArrayLike(_29f[0]))){
_29f=_29f[0];
}
var _2a0=this;
dojo.lang.forEach(_29f,function(anim,i,_2a3){
_2a0._anims.push(anim);
if(i<_2a3.length-1){
dojo.event.connect(anim,"onEnd",function(){
_2a0._playNext();
});
}else{
dojo.event.connect(anim,"onEnd",function(){
_2a0.fire("onEnd");
});
}
},_2a0);
};
dojo.inherits(dojo.lfx.Chain,dojo.lfx.IAnimation);
dojo.lang.extend(dojo.lfx.Chain,{_currAnim:-1,play:function(_2a4,_2a5){
if(!this._anims.length){
return;
}
if(_2a5||!this._anims[this._currAnim]){
this._currAnim=0;
}
this.fire("beforeBegin");
if(_2a4>0){
setTimeout(dojo.lang.hitch(this,function(){
this.play(null,_2a5);
}),_2a4);
return;
}
if(this._anims[this._currAnim]){
if(this._currAnim==0){
this.fire("handler",["begin",this._currAnim]);
this.fire("onBegin",[this._currAnim]);
}
this.fire("onPlay",[this._currAnim]);
this._anims[this._currAnim].play(null,_2a5);
}
},pause:function(){
if(this._anims[this._currAnim]){
this._anims[this._currAnim].pause();
this.fire("onPause",[this._currAnim]);
}
},playPause:function(){
if(this._anims.length==0){
return;
}
if(this._currAnim==-1){
this._currAnim=0;
}
var _2a6=this._anims[this._currAnim];
if(_2a6){
if(!_2a6._active||_2a6._paused){
this.play();
}else{
this.pause();
}
}
},stop:function(){
if(this._anims[this._currAnim]){
this._anims[this._currAnim].stop();
this.fire("onStop",[this._currAnim]);
}
},_playNext:function(){
if(this._currAnim==-1||this._anims.length==0){
return;
}
this._currAnim++;
if(this._anims[this._currAnim]){
this._anims[this._currAnim].play(null,true);
}
}});
dojo.lfx.combine=function(){
var _2a7=arguments;
if(dojo.lang.isArray(arguments[0])){
_2a7=arguments[0];
}
return new dojo.lfx.Combine(_2a7);
};
dojo.lfx.chain=function(){
var _2a8=arguments;
if(dojo.lang.isArray(arguments[0])){
_2a8=arguments[0];
}
return new dojo.lfx.Chain(_2a8);
};
dojo.provide("dojo.lang.extras");
dojo.require("dojo.lang.common");
dojo.lang.setTimeout=function(func,_2aa){
var _2ab=window,argsStart=2;
if(!dojo.lang.isFunction(func)){
_2ab=func;
func=_2aa;
_2aa=arguments[2];
argsStart++;
}
if(dojo.lang.isString(func)){
func=_2ab[func];
}
var args=[];
for(var i=argsStart;i<arguments.length;i++){
args.push(arguments[i]);
}
return setTimeout(function(){
func.apply(_2ab,args);
},_2aa);
};
dojo.lang.getNameInObj=function(ns,item){
if(!ns){
ns=dj_global;
}
for(var x in ns){
if(ns[x]===item){
return new String(x);
}
}
return null;
};
dojo.lang.shallowCopy=function(obj){
var ret={},key;
for(key in obj){
if(dojo.lang.isUndefined(ret[key])){
ret[key]=obj[key];
}
}
return ret;
};
dojo.lang.firstValued=function(){
for(var i=0;i<arguments.length;i++){
if(typeof arguments[i]!="undefined"){
return arguments[i];
}
}
return undefined;
};
dojo.lang.getObjPathValue=function(_2b4,_2b5,_2b6){
with(dojo.parseObjPath(_2b4,_2b5,_2b6)){
return dojo.evalProp(prop,obj,_2b6);
}
};
dojo.lang.setObjPathValue=function(_2b7,_2b8,_2b9,_2ba){
if(arguments.length<4){
_2ba=true;
}
with(dojo.parseObjPath(_2b7,_2b9,_2ba)){
if(obj&&(_2ba||(prop in obj))){
obj[prop]=_2b8;
}
}
};
dojo.provide("dojo.event");
dojo.require("dojo.lang.array");
dojo.require("dojo.lang.extras");
dojo.require("dojo.lang.func");
dojo.event=new function(){
this.canTimeout=dojo.lang.isFunction(dj_global["setTimeout"])||dojo.lang.isAlien(dj_global["setTimeout"]);
function interpolateArgs(args){
var dl=dojo.lang;
var ao={srcObj:dj_global,srcFunc:null,adviceObj:dj_global,adviceFunc:null,aroundObj:null,aroundFunc:null,adviceType:(args.length>2)?args[0]:"after",precedence:"last",once:false,delay:null,rate:0,adviceMsg:false};
switch(args.length){
case 0:
return;
case 1:
return;
case 2:
ao.srcFunc=args[0];
ao.adviceFunc=args[1];
break;
case 3:
if((dl.isObject(args[0]))&&(dl.isString(args[1]))&&(dl.isString(args[2]))){
ao.adviceType="after";
ao.srcObj=args[0];
ao.srcFunc=args[1];
ao.adviceFunc=args[2];
}else{
if((dl.isString(args[1]))&&(dl.isString(args[2]))){
ao.srcFunc=args[1];
ao.adviceFunc=args[2];
}else{
if((dl.isObject(args[0]))&&(dl.isString(args[1]))&&(dl.isFunction(args[2]))){
ao.adviceType="after";
ao.srcObj=args[0];
ao.srcFunc=args[1];
var _2be=dojo.lang.nameAnonFunc(args[2],ao.adviceObj);
ao.adviceFunc=_2be;
}else{
if((dl.isFunction(args[0]))&&(dl.isObject(args[1]))&&(dl.isString(args[2]))){
ao.adviceType="after";
ao.srcObj=dj_global;
var _2be=dojo.lang.nameAnonFunc(args[0],ao.srcObj);
ao.srcFunc=_2be;
ao.adviceObj=args[1];
ao.adviceFunc=args[2];
}
}
}
}
break;
case 4:
if((dl.isObject(args[0]))&&(dl.isObject(args[2]))){
ao.adviceType="after";
ao.srcObj=args[0];
ao.srcFunc=args[1];
ao.adviceObj=args[2];
ao.adviceFunc=args[3];
}else{
if((dl.isString(args[0]))&&(dl.isString(args[1]))&&(dl.isObject(args[2]))){
ao.adviceType=args[0];
ao.srcObj=dj_global;
ao.srcFunc=args[1];
ao.adviceObj=args[2];
ao.adviceFunc=args[3];
}else{
if((dl.isString(args[0]))&&(dl.isFunction(args[1]))&&(dl.isObject(args[2]))){
ao.adviceType=args[0];
ao.srcObj=dj_global;
var _2be=dojo.lang.nameAnonFunc(args[1],dj_global);
ao.srcFunc=_2be;
ao.adviceObj=args[2];
ao.adviceFunc=args[3];
}else{
if((dl.isString(args[0]))&&(dl.isObject(args[1]))&&(dl.isString(args[2]))&&(dl.isFunction(args[3]))){
ao.srcObj=args[1];
ao.srcFunc=args[2];
var _2be=dojo.lang.nameAnonFunc(args[3],dj_global);
ao.adviceObj=dj_global;
ao.adviceFunc=_2be;
}else{
if(dl.isObject(args[1])){
ao.srcObj=args[1];
ao.srcFunc=args[2];
ao.adviceObj=dj_global;
ao.adviceFunc=args[3];
}else{
if(dl.isObject(args[2])){
ao.srcObj=dj_global;
ao.srcFunc=args[1];
ao.adviceObj=args[2];
ao.adviceFunc=args[3];
}else{
ao.srcObj=ao.adviceObj=ao.aroundObj=dj_global;
ao.srcFunc=args[1];
ao.adviceFunc=args[2];
ao.aroundFunc=args[3];
}
}
}
}
}
}
break;
case 6:
ao.srcObj=args[1];
ao.srcFunc=args[2];
ao.adviceObj=args[3];
ao.adviceFunc=args[4];
ao.aroundFunc=args[5];
ao.aroundObj=dj_global;
break;
default:
ao.srcObj=args[1];
ao.srcFunc=args[2];
ao.adviceObj=args[3];
ao.adviceFunc=args[4];
ao.aroundObj=args[5];
ao.aroundFunc=args[6];
ao.once=args[7];
ao.delay=args[8];
ao.rate=args[9];
ao.adviceMsg=args[10];
break;
}
if(dl.isFunction(ao.aroundFunc)){
var _2be=dojo.lang.nameAnonFunc(ao.aroundFunc,ao.aroundObj);
ao.aroundFunc=_2be;
}
if(!dl.isString(ao.srcFunc)){
ao.srcFunc=dojo.lang.getNameInObj(ao.srcObj,ao.srcFunc);
}
if(!dl.isString(ao.adviceFunc)){
ao.adviceFunc=dojo.lang.getNameInObj(ao.adviceObj,ao.adviceFunc);
}
if((ao.aroundObj)&&(!dl.isString(ao.aroundFunc))){
ao.aroundFunc=dojo.lang.getNameInObj(ao.aroundObj,ao.aroundFunc);
}
if(!ao.srcObj){
dojo.raise("bad srcObj for srcFunc: "+ao.srcFunc);
}
if(!ao.adviceObj){
dojo.raise("bad adviceObj for adviceFunc: "+ao.adviceFunc);
}
return ao;
}
this.connect=function(){
if(arguments.length==1){
var ao=arguments[0];
}else{
var ao=interpolateArgs(arguments);
}
if(dojo.lang.isArray(ao.srcObj)&&ao.srcObj!=""){
var _2c0={};
for(var x in ao){
_2c0[x]=ao[x];
}
var mjps=[];
dojo.lang.forEach(ao.srcObj,function(src){
if((dojo.render.html.capable)&&(dojo.lang.isString(src))){
src=dojo.byId(src);
}
_2c0.srcObj=src;
mjps.push(dojo.event.connect.call(dojo.event,_2c0));
});
return mjps;
}
var mjp=dojo.event.MethodJoinPoint.getForMethod(ao.srcObj,ao.srcFunc);
if(ao.adviceFunc){
var mjp2=dojo.event.MethodJoinPoint.getForMethod(ao.adviceObj,ao.adviceFunc);
}
mjp.kwAddAdvice(ao);
return mjp;
};
this.log=function(a1,a2){
var _2c8;
if((arguments.length==1)&&(typeof a1=="object")){
_2c8=a1;
}else{
_2c8={srcObj:a1,srcFunc:a2};
}
_2c8.adviceFunc=function(){
var _2c9=[];
for(var x=0;x<arguments.length;x++){
_2c9.push(arguments[x]);
}
dojo.debug("("+_2c8.srcObj+")."+_2c8.srcFunc,":",_2c9.join(", "));
};
this.kwConnect(_2c8);
};
this.connectBefore=function(){
var args=["before"];
for(var i=0;i<arguments.length;i++){
args.push(arguments[i]);
}
return this.connect.apply(this,args);
};
this.connectAround=function(){
var args=["around"];
for(var i=0;i<arguments.length;i++){
args.push(arguments[i]);
}
return this.connect.apply(this,args);
};
this.connectOnce=function(){
var ao=interpolateArgs(arguments);
ao.once=true;
return this.connect(ao);
};
this._kwConnectImpl=function(_2d0,_2d1){
var fn=(_2d1)?"disconnect":"connect";
if(typeof _2d0["srcFunc"]=="function"){
_2d0.srcObj=_2d0["srcObj"]||dj_global;
var _2d3=dojo.lang.nameAnonFunc(_2d0.srcFunc,_2d0.srcObj);
_2d0.srcFunc=_2d3;
}
if(typeof _2d0["adviceFunc"]=="function"){
_2d0.adviceObj=_2d0["adviceObj"]||dj_global;
var _2d3=dojo.lang.nameAnonFunc(_2d0.adviceFunc,_2d0.adviceObj);
_2d0.adviceFunc=_2d3;
}
return dojo.event[fn]((_2d0["type"]||_2d0["adviceType"]||"after"),_2d0["srcObj"]||dj_global,_2d0["srcFunc"],_2d0["adviceObj"]||_2d0["targetObj"]||dj_global,_2d0["adviceFunc"]||_2d0["targetFunc"],_2d0["aroundObj"],_2d0["aroundFunc"],_2d0["once"],_2d0["delay"],_2d0["rate"],_2d0["adviceMsg"]||false);
};
this.kwConnect=function(_2d4){
return this._kwConnectImpl(_2d4,false);
};
this.disconnect=function(){
var ao=interpolateArgs(arguments);
if(!ao.adviceFunc){
return;
}
var mjp=dojo.event.MethodJoinPoint.getForMethod(ao.srcObj,ao.srcFunc);
return mjp.removeAdvice(ao.adviceObj,ao.adviceFunc,ao.adviceType,ao.once);
};
this.kwDisconnect=function(_2d7){
return this._kwConnectImpl(_2d7,true);
};
};
dojo.event.MethodInvocation=function(_2d8,obj,args){
this.jp_=_2d8;
this.object=obj;
this.args=[];
for(var x=0;x<args.length;x++){
this.args[x]=args[x];
}
this.around_index=-1;
};
dojo.event.MethodInvocation.prototype.proceed=function(){
this.around_index++;
if(this.around_index>=this.jp_.around.length){
return this.jp_.object[this.jp_.methodname].apply(this.jp_.object,this.args);
}else{
var ti=this.jp_.around[this.around_index];
var mobj=ti[0]||dj_global;
var meth=ti[1];
return mobj[meth].call(mobj,this);
}
};
dojo.event.MethodJoinPoint=function(obj,_2e0){
this.object=obj||dj_global;
this.methodname=_2e0;
this.methodfunc=this.object[_2e0];
this.before=[];
this.after=[];
this.around=[];
};
dojo.event.MethodJoinPoint.getForMethod=function(obj,_2e2){
if(!obj){
obj=dj_global;
}
if(!obj[_2e2]){
obj[_2e2]=function(){
};
}else{
if((!dojo.lang.isFunction(obj[_2e2]))&&(!dojo.lang.isAlien(obj[_2e2]))){
return null;
}
}
var _2e3=_2e2+"$joinpoint";
var _2e4=_2e2+"$joinpoint$method";
var _2e5=obj[_2e3];
if(!_2e5){
var _2e6=false;
if(dojo.event["browser"]){
if((obj["attachEvent"])||(obj["nodeType"])||(obj["addEventListener"])){
_2e6=true;
dojo.event.browser.addClobberNodeAttrs(obj,[_2e3,_2e4,_2e2]);
}
}
var _2e7=obj[_2e2].length;
obj[_2e4]=obj[_2e2];
_2e5=obj[_2e3]=new dojo.event.MethodJoinPoint(obj,_2e4);
obj[_2e2]=function(){
var args=[];
if((_2e6)&&(!arguments.length)){
var evt=null;
try{
if(obj.ownerDocument){
evt=obj.ownerDocument.parentWindow.event;
}else{
if(obj.documentElement){
evt=obj.documentElement.ownerDocument.parentWindow.event;
}else{
evt=window.event;
}
}
}
catch(e){
evt=window.event;
}
if(evt){
args.push(dojo.event.browser.fixEvent(evt,this));
}
}else{
for(var x=0;x<arguments.length;x++){
if((x==0)&&(_2e6)&&(dojo.event.browser.isEvent(arguments[x]))){
args.push(dojo.event.browser.fixEvent(arguments[x],this));
}else{
args.push(arguments[x]);
}
}
}
return _2e5.run.apply(_2e5,args);
};
obj[_2e2].__preJoinArity=_2e7;
}
return _2e5;
};
dojo.lang.extend(dojo.event.MethodJoinPoint,{unintercept:function(){
this.object[this.methodname]=this.methodfunc;
this.before=[];
this.after=[];
this.around=[];
},disconnect:dojo.lang.forward("unintercept"),run:function(){
var obj=this.object||dj_global;
var args=arguments;
var _2ed=[];
for(var x=0;x<args.length;x++){
_2ed[x]=args[x];
}
var _2ef=function(marr){
if(!marr){
dojo.debug("Null argument to unrollAdvice()");
return;
}
var _2f1=marr[0]||dj_global;
var _2f2=marr[1];
if(!_2f1[_2f2]){
dojo.raise("function \""+_2f2+"\" does not exist on \""+_2f1+"\"");
}
var _2f3=marr[2]||dj_global;
var _2f4=marr[3];
var msg=marr[6];
var _2f6;
var to={args:[],jp_:this,object:obj,proceed:function(){
return _2f1[_2f2].apply(_2f1,to.args);
}};
to.args=_2ed;
var _2f8=parseInt(marr[4]);
var _2f9=((!isNaN(_2f8))&&(marr[4]!==null)&&(typeof marr[4]!="undefined"));
if(marr[5]){
var rate=parseInt(marr[5]);
var cur=new Date();
var _2fc=false;
if((marr["last"])&&((cur-marr.last)<=rate)){
if(dojo.event.canTimeout){
if(marr["delayTimer"]){
clearTimeout(marr.delayTimer);
}
var tod=parseInt(rate*2);
var mcpy=dojo.lang.shallowCopy(marr);
marr.delayTimer=setTimeout(function(){
mcpy[5]=0;
_2ef(mcpy);
},tod);
}
return;
}else{
marr.last=cur;
}
}
if(_2f4){
_2f3[_2f4].call(_2f3,to);
}else{
if((_2f9)&&((dojo.render.html)||(dojo.render.svg))){
dj_global["setTimeout"](function(){
if(msg){
_2f1[_2f2].call(_2f1,to);
}else{
_2f1[_2f2].apply(_2f1,args);
}
},_2f8);
}else{
if(msg){
_2f1[_2f2].call(_2f1,to);
}else{
_2f1[_2f2].apply(_2f1,args);
}
}
}
};
if(this.before.length>0){
dojo.lang.forEach(this.before,_2ef);
}
var _2ff;
if(this.around.length>0){
var mi=new dojo.event.MethodInvocation(this,obj,args);
_2ff=mi.proceed();
}else{
if(this.methodfunc){
_2ff=this.object[this.methodname].apply(this.object,args);
}
}
if(this.after.length>0){
dojo.lang.forEach(this.after,_2ef);
}
return (this.methodfunc)?_2ff:null;
},getArr:function(kind){
var arr=this.after;
if((typeof kind=="string")&&(kind.indexOf("before")!=-1)){
arr=this.before;
}else{
if(kind=="around"){
arr=this.around;
}
}
return arr;
},kwAddAdvice:function(args){
this.addAdvice(args["adviceObj"],args["adviceFunc"],args["aroundObj"],args["aroundFunc"],args["adviceType"],args["precedence"],args["once"],args["delay"],args["rate"],args["adviceMsg"]);
},addAdvice:function(_304,_305,_306,_307,_308,_309,once,_30b,rate,_30d){
var arr=this.getArr(_308);
if(!arr){
dojo.raise("bad this: "+this);
}
var ao=[_304,_305,_306,_307,_30b,rate,_30d];
if(once){
if(this.hasAdvice(_304,_305,_308,arr)>=0){
return;
}
}
if(_309=="first"){
arr.unshift(ao);
}else{
arr.push(ao);
}
},hasAdvice:function(_310,_311,_312,arr){
if(!arr){
arr=this.getArr(_312);
}
var ind=-1;
for(var x=0;x<arr.length;x++){
if((arr[x][0]==_310)&&(arr[x][1]==_311)){
ind=x;
}
}
return ind;
},removeAdvice:function(_316,_317,_318,once){
var arr=this.getArr(_318);
var ind=this.hasAdvice(_316,_317,_318,arr);
if(ind==-1){
return false;
}
while(ind!=-1){
arr.splice(ind,1);
if(once){
break;
}
ind=this.hasAdvice(_316,_317,_318,arr);
}
return true;
}});
dojo.provide("dojo.lfx.html");
dojo.require("dojo.lfx.Animation");
dojo.require("dojo.html");
dojo.require("dojo.event");
dojo.require("dojo.lang.func");
dojo.lfx.html._byId=function(_31c){
if(dojo.lang.isArrayLike(_31c)){
if(!_31c.alreadyChecked){
var n=[];
dojo.lang.forEach(_31c,function(node){
n.push(dojo.byId(node));
});
n.alreadyChecked=true;
return n;
}else{
return _31c;
}
}else{
return [dojo.byId(_31c)];
}
};
dojo.lfx.html.propertyAnimation=function(_31f,_320,_321,_322){
_31f=dojo.lfx.html._byId(_31f);
if(_31f.length==1){
dojo.lang.forEach(_320,function(prop){
if(typeof prop["start"]=="undefined"){
prop.start=parseInt(dojo.style.getComputedStyle(_31f[0],prop.property));
if(isNaN(prop.start)&&(prop.property=="opacity")){
prop.start=1;
}
}
});
}
var _324=function(_325){
var _326=new Array(_325.length);
for(var i=0;i<_325.length;i++){
_326[i]=Math.round(_325[i]);
}
return _326;
};
var _328=function(n,_32a){
n=dojo.byId(n);
if(!n||!n.style){
return;
}
for(s in _32a){
if(s=="opacity"){
dojo.style.setOpacity(n,_32a[s]);
}else{
n.style[dojo.style.toCamelCase(s)]=_32a[s];
}
}
};
var _32b=function(_32c){
this._properties=_32c;
this.diffs=new Array(_32c.length);
dojo.lang.forEach(_32c,function(prop,i){
if(dojo.lang.isArray(prop.start)){
this.diffs[i]=null;
}else{
this.diffs[i]=prop.end-prop.start;
}
},this);
this.getValue=function(n){
var ret={};
dojo.lang.forEach(this._properties,function(prop,i){
var _333=null;
if(dojo.lang.isArray(prop.start)){
_333=(prop.units||"rgb")+"(";
for(var j=0;j<prop.start.length;j++){
_333+=Math.round(((prop.end[j]-prop.start[j])*n)+prop.start[j])+(j<prop.start.length-1?",":"");
}
_333+=")";
}else{
_333=((this.diffs[i])*n)+prop.start+(prop.property!="opacity"?prop.units||"px":"");
}
ret[prop.property]=_333;
},this);
return ret;
};
};
var anim=new dojo.lfx.Animation(_321,new _32b(_320),_322);
dojo.event.connect(anim,"onAnimate",function(_336){
dojo.lang.forEach(_31f,function(node){
_328(node,_336);
});
});
return anim;
};
dojo.lfx.html._makeFadeable=function(_338){
var _339=function(node){
if(dojo.render.html.ie){
if((node.style.zoom.length==0)&&(dojo.style.getStyle(node,"zoom")=="normal")){
node.style.zoom="1";
}
if((node.style.width.length==0)&&(dojo.style.getStyle(node,"width")=="auto")){
node.style.width="auto";
}
}
};
if(dojo.lang.isArrayLike(_338)){
dojo.lang.forEach(_338,_339);
}else{
_339(_338);
}
};
dojo.lfx.html.fadeIn=function(_33b,_33c,_33d,_33e){
_33b=dojo.lfx.html._byId(_33b);
dojo.lfx.html._makeFadeable(_33b);
var anim=dojo.lfx.propertyAnimation(_33b,[{property:"opacity",start:dojo.style.getOpacity(_33b[0]),end:1}],_33c,_33d);
if(_33e){
dojo.event.connect(anim,"onEnd",function(){
_33e(_33b,anim);
});
}
return anim;
};
dojo.lfx.html.fadeOut=function(_340,_341,_342,_343){
_340=dojo.lfx.html._byId(_340);
dojo.lfx.html._makeFadeable(_340);
var anim=dojo.lfx.propertyAnimation(_340,[{property:"opacity",start:dojo.style.getOpacity(_340[0]),end:0}],_341,_342);
if(_343){
dojo.event.connect(anim,"onEnd",function(){
_343(_340,anim);
});
}
return anim;
};
dojo.lfx.html.fadeShow=function(_345,_346,_347,_348){
var anim=dojo.lfx.html.fadeIn(_345,_346,_347,_348);
dojo.event.connect(anim,"beforeBegin",function(){
if(dojo.lang.isArrayLike(_345)){
dojo.lang.forEach(_345,dojo.style.show);
}else{
dojo.style.show(_345);
}
});
return anim;
};
dojo.lfx.html.fadeHide=function(_34a,_34b,_34c,_34d){
var anim=dojo.lfx.html.fadeOut(_34a,_34b,_34c,function(){
if(dojo.lang.isArrayLike(_34a)){
dojo.lang.forEach(_34a,dojo.style.hide);
}else{
dojo.style.hide(_34a);
}
if(_34d){
_34d(_34a,anim);
}
});
return anim;
};
dojo.lfx.html.wipeIn=function(_34f,_350,_351,_352){
_34f=dojo.lfx.html._byId(_34f);
var _353=[];
var init=function(node,_356){
if(_356=="visible"){
node.style.overflow="hidden";
}
dojo.style.show(node);
node.style.height=0;
};
dojo.lang.forEach(_34f,function(node){
var _358=dojo.style.getStyle(node,"overflow");
var _359=function(){
init(node,_358);
};
_359();
var anim=dojo.lfx.propertyAnimation(node,[{property:"height",start:0,end:node.scrollHeight}],_350,_351);
dojo.event.connect(anim,"beforeBegin",_359);
dojo.event.connect(anim,"onEnd",function(){
node.style.overflow=_358;
node.style.height="auto";
if(_352){
_352(node,anim);
}
});
_353.push(anim);
});
if(_34f.length>1){
return dojo.lfx.combine(_353);
}else{
return _353[0];
}
};
dojo.lfx.html.wipeOut=function(_35b,_35c,_35d,_35e){
_35b=dojo.lfx.html._byId(_35b);
var _35f=[];
var init=function(node,_362){
dojo.style.show(node);
if(_362=="visible"){
node.style.overflow="hidden";
}
};
dojo.lang.forEach(_35b,function(node){
var _364=dojo.style.getStyle(node,"overflow");
var _365=function(){
init(node,_364);
};
_365();
var anim=dojo.lfx.propertyAnimation(node,[{property:"height",start:node.offsetHeight,end:0}],_35c,_35d);
dojo.event.connect(anim,"beforeBegin",_365);
dojo.event.connect(anim,"onEnd",function(){
dojo.style.hide(node);
node.style.overflow=_364;
if(_35e){
_35e(node,anim);
}
});
_35f.push(anim);
});
if(_35b.length>1){
return dojo.lfx.combine(_35f);
}else{
return _35f[0];
}
};
dojo.lfx.html.slideTo=function(_367,_368,_369,_36a,_36b){
_367=dojo.lfx.html._byId(_367);
var _36c=[];
dojo.lang.forEach(_367,function(node){
var top=null;
var left=null;
var pos=null;
var init=(function(){
var _372=node;
return function(){
top=node.offsetTop;
left=node.offsetLeft;
pos=dojo.style.getComputedStyle(node,"position");
if(pos=="relative"||pos=="static"){
top=parseInt(dojo.style.getComputedStyle(node,"top"))||0;
left=parseInt(dojo.style.getComputedStyle(node,"left"))||0;
}
};
})();
init();
var anim=dojo.lfx.propertyAnimation(node,[{property:"top",start:top,end:_368[0]},{property:"left",start:left,end:_368[1]}],_369,_36a);
dojo.event.connect(anim,"beforeBegin",init);
if(_36b){
dojo.event.connect(anim,"onEnd",function(){
_36b(node,anim);
});
}
_36c.push(anim);
});
if(_367.length>1){
return dojo.lfx.combine(_36c);
}else{
return _36c[0];
}
};
dojo.lfx.html.explode=function(_374,_375,_376,_377,_378){
var _379=dojo.style.toCoordinateArray(_374);
var _37a=document.createElement("div");
with(_37a.style){
position="absolute";
border="1px solid black";
display="none";
}
document.body.appendChild(_37a);
_375=dojo.byId(_375);
with(_375.style){
visibility="hidden";
display="block";
}
var _37b=dojo.style.toCoordinateArray(_375);
with(_375.style){
display="none";
visibility="visible";
}
var anim=new dojo.lfx.Animation({beforeBegin:function(){
dojo.style.show(_37a);
},onAnimate:function(_37d){
with(_37a.style){
left=_37d[0]+"px";
top=_37d[1]+"px";
width=_37d[2]+"px";
height=_37d[3]+"px";
}
},onEnd:function(){
dojo.style.show(_375);
_37a.parentNode.removeChild(_37a);
}},_376,new dojo.lfx.Line(_379,_37b),_377);
if(_378){
dojo.event.connect(anim,"onEnd",function(){
_378(_375,anim);
});
}
return anim;
};
dojo.lfx.html.implode=function(_37e,end,_380,_381,_382){
var _383=dojo.style.toCoordinateArray(_37e);
var _384=dojo.style.toCoordinateArray(end);
_37e=dojo.byId(_37e);
var _385=document.createElement("div");
with(_385.style){
position="absolute";
border="1px solid black";
display="none";
}
document.body.appendChild(_385);
var anim=new dojo.lfx.Animation({beforeBegin:function(){
dojo.style.hide(_37e);
dojo.style.show(_385);
},onAnimate:function(_387){
with(_385.style){
left=_387[0]+"px";
top=_387[1]+"px";
width=_387[2]+"px";
height=_387[3]+"px";
}
},onEnd:function(){
_385.parentNode.removeChild(_385);
}},_380,new dojo.lfx.Line(_383,_384),_381);
if(_382){
dojo.event.connect(anim,"onEnd",function(){
_382(_37e,anim);
});
}
return anim;
};
dojo.lfx.html.highlight=function(_388,_389,_38a,_38b,_38c){
_388=dojo.lfx.html._byId(_388);
var _38d=[];
dojo.lang.forEach(_388,function(node){
var _38f=dojo.style.getBackgroundColor(node);
var bg=dojo.style.getStyle(node,"background-color").toLowerCase();
var _391=(bg=="transparent"||bg=="rgba(0, 0, 0, 0)");
while(_38f.length>3){
_38f.pop();
}
var rgb=new dojo.graphics.color.Color(_389).toRgb();
var _393=new dojo.graphics.color.Color(_38f).toRgb();
var anim=dojo.lfx.propertyAnimation(node,[{property:"background-color",start:rgb,end:_393}],_38a,_38b);
dojo.event.connect(anim,"beforeBegin",function(){
node.style.backgroundColor="rgb("+rgb.join(",")+")";
});
dojo.event.connect(anim,"onEnd",function(){
if(_391){
node.style.backgroundColor="transparent";
}
if(_38c){
_38c(node,anim);
}
});
_38d.push(anim);
});
if(_388.length>1){
return dojo.lfx.combine(_38d);
}else{
return _38d[0];
}
};
dojo.lfx.html.unhighlight=function(_395,_396,_397,_398,_399){
_395=dojo.lfx.html._byId(_395);
var _39a=[];
dojo.lang.forEach(_395,function(node){
var _39c=new dojo.graphics.color.Color(dojo.style.getBackgroundColor(node)).toRgb();
var rgb=new dojo.graphics.color.Color(_396).toRgb();
var anim=dojo.lfx.propertyAnimation(node,[{property:"background-color",start:_39c,end:rgb}],_397,_398);
dojo.event.connect(anim,"beforeBegin",function(){
node.style.backgroundColor="rgb("+_39c.join(",")+")";
});
if(_399){
dojo.event.connect(anim,"onEnd",function(){
_399(node,anim);
});
}
_39a.push(anim);
});
if(_395.length>1){
return dojo.lfx.combine(_39a);
}else{
return _39a[0];
}
};
dojo.lang.mixin(dojo.lfx,dojo.lfx.html);
dojo.kwCompoundRequire({browser:["dojo.lfx.html"],dashboard:["dojo.lfx.html"]});
dojo.provide("dojo.lfx.*");
dojo.require("dojo.event");
dojo.provide("dojo.event.topic");
dojo.event.topic=new function(){
this.topics={};
this.getTopic=function(_39f){
if(!this.topics[_39f]){
this.topics[_39f]=new this.TopicImpl(_39f);
}
return this.topics[_39f];
};
this.registerPublisher=function(_3a0,obj,_3a2){
var _3a0=this.getTopic(_3a0);
_3a0.registerPublisher(obj,_3a2);
};
this.subscribe=function(_3a3,obj,_3a5){
var _3a3=this.getTopic(_3a3);
_3a3.subscribe(obj,_3a5);
};
this.unsubscribe=function(_3a6,obj,_3a8){
var _3a6=this.getTopic(_3a6);
_3a6.unsubscribe(obj,_3a8);
};
this.destroy=function(_3a9){
this.getTopic(_3a9).destroy();
delete this.topics[_3a9];
};
this.publish=function(_3aa,_3ab){
var _3aa=this.getTopic(_3aa);
var args=[];
if(arguments.length==2&&(dojo.lang.isArray(_3ab)||_3ab.callee)){
args=_3ab;
}else{
var args=[];
for(var x=1;x<arguments.length;x++){
args.push(arguments[x]);
}
}
_3aa.sendMessage.apply(_3aa,args);
};
};
dojo.event.topic.TopicImpl=function(_3ae){
this.topicName=_3ae;
this.subscribe=function(_3af,_3b0){
var tf=_3b0||_3af;
var to=(!_3b0)?dj_global:_3af;
dojo.event.kwConnect({srcObj:this,srcFunc:"sendMessage",adviceObj:to,adviceFunc:tf});
};
this.unsubscribe=function(_3b3,_3b4){
var tf=(!_3b4)?_3b3:_3b4;
var to=(!_3b4)?null:_3b3;
dojo.event.kwDisconnect({srcObj:this,srcFunc:"sendMessage",adviceObj:to,adviceFunc:tf});
};
this.destroy=function(){
dojo.event.MethodJoinPoint.getForMethod(this,"sendMessage").disconnect();
};
this.registerPublisher=function(_3b7,_3b8){
dojo.event.connect(_3b7,_3b8,this,"sendMessage");
};
this.sendMessage=function(_3b9){
};
};
dojo.provide("dojo.event.browser");
dojo.require("dojo.event");
dojo_ie_clobber=new function(){
this.clobberNodes=[];
function nukeProp(node,prop){
try{
node[prop]=null;
}
catch(e){
}
try{
delete node[prop];
}
catch(e){
}
try{
node.removeAttribute(prop);
}
catch(e){
}
}
this.clobber=function(_3bc){
var na;
var tna;
if(_3bc){
tna=_3bc.all||_3bc.getElementsByTagName("*");
na=[_3bc];
for(var x=0;x<tna.length;x++){
if(tna[x]["__doClobber__"]){
na.push(tna[x]);
}
}
}else{
try{
window.onload=null;
}
catch(e){
}
na=(this.clobberNodes.length)?this.clobberNodes:document.all;
}
tna=null;
var _3c0={};
for(var i=na.length-1;i>=0;i=i-1){
var el=na[i];
if(el["__clobberAttrs__"]){
for(var j=0;j<el.__clobberAttrs__.length;j++){
nukeProp(el,el.__clobberAttrs__[j]);
}
nukeProp(el,"__clobberAttrs__");
nukeProp(el,"__doClobber__");
}
}
na=null;
};
};
if(dojo.render.html.ie){
window.onunload=function(){
dojo_ie_clobber.clobber();
try{
if((dojo["widget"])&&(dojo.widget["manager"])){
dojo.widget.manager.destroyAll();
}
}
catch(e){
}
try{
window.onload=null;
}
catch(e){
}
try{
window.onunload=null;
}
catch(e){
}
dojo_ie_clobber.clobberNodes=[];
};
}
dojo.event.browser=new function(){
var _3c4=0;
this.clean=function(node){
if(dojo.render.html.ie){
dojo_ie_clobber.clobber(node);
}
};
this.addClobberNode=function(node){
if(!node["__doClobber__"]){
node.__doClobber__=true;
dojo_ie_clobber.clobberNodes.push(node);
node.__clobberAttrs__=[];
}
};
this.addClobberNodeAttrs=function(node,_3c8){
this.addClobberNode(node);
for(var x=0;x<_3c8.length;x++){
node.__clobberAttrs__.push(_3c8[x]);
}
};
this.removeListener=function(node,_3cb,fp,_3cd){
if(!_3cd){
var _3cd=false;
}
_3cb=_3cb.toLowerCase();
if(_3cb.substr(0,2)=="on"){
_3cb=_3cb.substr(2);
}
if(node.removeEventListener){
node.removeEventListener(_3cb,fp,_3cd);
}
};
this.addListener=function(node,_3cf,fp,_3d1,_3d2){
if(!node){
return;
}
if(!_3d1){
var _3d1=false;
}
_3cf=_3cf.toLowerCase();
if(_3cf.substr(0,2)!="on"){
_3cf="on"+_3cf;
}
if(!_3d2){
var _3d3=function(evt){
if(!evt){
evt=window.event;
}
var ret=fp(dojo.event.browser.fixEvent(evt,this));
if(_3d1){
dojo.event.browser.stopEvent(evt);
}
return ret;
};
}else{
_3d3=fp;
}
if(node.addEventListener){
node.addEventListener(_3cf.substr(2),_3d3,_3d1);
return _3d3;
}else{
if(typeof node[_3cf]=="function"){
var _3d6=node[_3cf];
node[_3cf]=function(e){
_3d6(e);
return _3d3(e);
};
}else{
node[_3cf]=_3d3;
}
if(dojo.render.html.ie){
this.addClobberNodeAttrs(node,[_3cf]);
}
return _3d3;
}
};
this.isEvent=function(obj){
return (typeof obj!="undefined")&&(typeof Event!="undefined")&&(obj.eventPhase);
};
this.currentEvent=null;
this.callListener=function(_3d9,_3da){
if(typeof _3d9!="function"){
dojo.raise("listener not a function: "+_3d9);
}
dojo.event.browser.currentEvent.currentTarget=_3da;
return _3d9.call(_3da,dojo.event.browser.currentEvent);
};
this.stopPropagation=function(){
dojo.event.browser.currentEvent.cancelBubble=true;
};
this.preventDefault=function(){
dojo.event.browser.currentEvent.returnValue=false;
};
this.keys={KEY_BACKSPACE:8,KEY_TAB:9,KEY_ENTER:13,KEY_SHIFT:16,KEY_CTRL:17,KEY_ALT:18,KEY_PAUSE:19,KEY_CAPS_LOCK:20,KEY_ESCAPE:27,KEY_SPACE:32,KEY_PAGE_UP:33,KEY_PAGE_DOWN:34,KEY_END:35,KEY_HOME:36,KEY_LEFT_ARROW:37,KEY_UP_ARROW:38,KEY_RIGHT_ARROW:39,KEY_DOWN_ARROW:40,KEY_INSERT:45,KEY_DELETE:46,KEY_LEFT_WINDOW:91,KEY_RIGHT_WINDOW:92,KEY_SELECT:93,KEY_F1:112,KEY_F2:113,KEY_F3:114,KEY_F4:115,KEY_F5:116,KEY_F6:117,KEY_F7:118,KEY_F8:119,KEY_F9:120,KEY_F10:121,KEY_F11:122,KEY_F12:123,KEY_NUM_LOCK:144,KEY_SCROLL_LOCK:145};
this.revKeys=[];
for(var key in this.keys){
this.revKeys[this.keys[key]]=key;
}
this.fixEvent=function(evt,_3dd){
if((!evt)&&(window["event"])){
var evt=window.event;
}
if((evt["type"])&&(evt["type"].indexOf("key")==0)){
evt.keys=this.revKeys;
for(var key in this.keys){
evt[key]=this.keys[key];
}
if((dojo.render.html.ie)&&(evt["type"]=="keypress")){
evt.charCode=evt.keyCode;
}
}
if(dojo.render.html.ie){
if(!evt.target){
evt.target=evt.srcElement;
}
if(!evt.currentTarget){
evt.currentTarget=(_3dd?_3dd:evt.srcElement);
}
if(!evt.layerX){
evt.layerX=evt.offsetX;
}
if(!evt.layerY){
evt.layerY=evt.offsetY;
}
if(!evt.pageX){
evt.pageX=evt.clientX+(window.pageXOffset||document.documentElement.scrollLeft||document.body.scrollLeft||0);
}
if(!evt.pageY){
evt.pageY=evt.clientY+(window.pageYOffset||document.documentElement.scrollTop||document.body.scrollTop||0);
}
if(evt.type=="mouseover"){
evt.relatedTarget=evt.fromElement;
}
if(evt.type=="mouseout"){
evt.relatedTarget=evt.toElement;
}
this.currentEvent=evt;
evt.callListener=this.callListener;
evt.stopPropagation=this.stopPropagation;
evt.preventDefault=this.preventDefault;
}
return evt;
};
this.stopEvent=function(ev){
if(window.event){
ev.returnValue=false;
ev.cancelBubble=true;
}else{
ev.preventDefault();
ev.stopPropagation();
}
};
};
dojo.kwCompoundRequire({common:["dojo.event","dojo.event.topic"],browser:["dojo.event.browser"],dashboard:["dojo.event.browser"]});
dojo.provide("dojo.event.*");
dojo.provide("dojo.logging.Logger");
dojo.provide("dojo.log");
dojo.require("dojo.lang");
dojo.logging.Record=function(lvl,msg){
this.level=lvl;
this.message=msg;
this.time=new Date();
};
dojo.logging.LogFilter=function(_3e2){
this.passChain=_3e2||"";
this.filter=function(_3e3){
return true;
};
};
dojo.logging.Logger=function(){
this.cutOffLevel=0;
this.propagate=true;
this.parent=null;
this.data=[];
this.filters=[];
this.handlers=[];
};
dojo.lang.extend(dojo.logging.Logger,{argsToArr:function(args){
var ret=[];
for(var x=0;x<args.length;x++){
ret.push(args[x]);
}
return ret;
},setLevel:function(lvl){
this.cutOffLevel=parseInt(lvl);
},isEnabledFor:function(lvl){
return parseInt(lvl)>=this.cutOffLevel;
},getEffectiveLevel:function(){
if((this.cutOffLevel==0)&&(this.parent)){
return this.parent.getEffectiveLevel();
}
return this.cutOffLevel;
},addFilter:function(flt){
this.filters.push(flt);
return this.filters.length-1;
},removeFilterByIndex:function(_3ea){
if(this.filters[_3ea]){
delete this.filters[_3ea];
return true;
}
return false;
},removeFilter:function(_3eb){
for(var x=0;x<this.filters.length;x++){
if(this.filters[x]===_3eb){
delete this.filters[x];
return true;
}
}
return false;
},removeAllFilters:function(){
this.filters=[];
},filter:function(rec){
for(var x=0;x<this.filters.length;x++){
if((this.filters[x]["filter"])&&(!this.filters[x].filter(rec))||(rec.level<this.cutOffLevel)){
return false;
}
}
return true;
},addHandler:function(hdlr){
this.handlers.push(hdlr);
return this.handlers.length-1;
},handle:function(rec){
if((!this.filter(rec))||(rec.level<this.cutOffLevel)){
return false;
}
for(var x=0;x<this.handlers.length;x++){
if(this.handlers[x]["handle"]){
this.handlers[x].handle(rec);
}
}
return true;
},log:function(lvl,msg){
if((this.propagate)&&(this.parent)&&(this.parent.rec.level>=this.cutOffLevel)){
this.parent.log(lvl,msg);
return false;
}
this.handle(new dojo.logging.Record(lvl,msg));
return true;
},debug:function(msg){
return this.logType("DEBUG",this.argsToArr(arguments));
},info:function(msg){
return this.logType("INFO",this.argsToArr(arguments));
},warning:function(msg){
return this.logType("WARNING",this.argsToArr(arguments));
},error:function(msg){
return this.logType("ERROR",this.argsToArr(arguments));
},critical:function(msg){
return this.logType("CRITICAL",this.argsToArr(arguments));
},exception:function(msg,e,_3fb){
if(e){
var _3fc=[e.name,(e.description||e.message)];
if(e.fileName){
_3fc.push(e.fileName);
_3fc.push("line "+e.lineNumber);
}
msg+=" "+_3fc.join(" : ");
}
this.logType("ERROR",msg);
if(!_3fb){
throw e;
}
},logType:function(type,args){
var na=[dojo.logging.log.getLevel(type)];
if(typeof args=="array"){
na=na.concat(args);
}else{
if((typeof args=="object")&&(args["length"])){
na=na.concat(this.argsToArr(args));
}else{
na=na.concat(this.argsToArr(arguments).slice(1));
}
}
return this.log.apply(this,na);
}});
void (function(){
var _400=dojo.logging.Logger.prototype;
_400.warn=_400.warning;
_400.err=_400.error;
_400.crit=_400.critical;
})();
dojo.logging.LogHandler=function(_401){
this.cutOffLevel=(_401)?_401:0;
this.formatter=null;
this.data=[];
this.filters=[];
};
dojo.logging.LogHandler.prototype.setFormatter=function(fmtr){
dojo.unimplemented("setFormatter");
};
dojo.logging.LogHandler.prototype.flush=function(){
dojo.unimplemented("flush");
};
dojo.logging.LogHandler.prototype.close=function(){
dojo.unimplemented("close");
};
dojo.logging.LogHandler.prototype.handleError=function(){
dojo.unimplemented("handleError");
};
dojo.logging.LogHandler.prototype.handle=function(_403){
if((this.filter(_403))&&(_403.level>=this.cutOffLevel)){
this.emit(_403);
}
};
dojo.logging.LogHandler.prototype.emit=function(_404){
dojo.unimplemented("emit");
};
void (function(){
var _405=["setLevel","addFilter","removeFilterByIndex","removeFilter","removeAllFilters","filter"];
var tgt=dojo.logging.LogHandler.prototype;
var src=dojo.logging.Logger.prototype;
for(var x=0;x<_405.length;x++){
tgt[_405[x]]=src[_405[x]];
}
})();
dojo.logging.log=new dojo.logging.Logger();
dojo.logging.log.levels=[{"name":"DEBUG","level":1},{"name":"INFO","level":2},{"name":"WARNING","level":3},{"name":"ERROR","level":4},{"name":"CRITICAL","level":5}];
dojo.logging.log.loggers={};
dojo.logging.log.getLogger=function(name){
if(!this.loggers[name]){
this.loggers[name]=new dojo.logging.Logger();
this.loggers[name].parent=this;
}
return this.loggers[name];
};
dojo.logging.log.getLevelName=function(lvl){
for(var x=0;x<this.levels.length;x++){
if(this.levels[x].level==lvl){
return this.levels[x].name;
}
}
return null;
};
dojo.logging.log.addLevelName=function(name,lvl){
if(this.getLevelName(name)){
this.err("could not add log level "+name+" because a level with that name already exists");
return false;
}
this.levels.append({"name":name,"level":parseInt(lvl)});
return true;
};
dojo.logging.log.getLevel=function(name){
for(var x=0;x<this.levels.length;x++){
if(this.levels[x].name.toUpperCase()==name.toUpperCase()){
return this.levels[x].level;
}
}
return null;
};
dojo.logging.MemoryLogHandler=function(_410,_411,_412,_413){
dojo.logging.LogHandler.call(this,_410);
this.numRecords=(typeof djConfig["loggingNumRecords"]!="undefined")?djConfig["loggingNumRecords"]:((_411)?_411:-1);
this.postType=(typeof djConfig["loggingPostType"]!="undefined")?djConfig["loggingPostType"]:(_412||-1);
this.postInterval=(typeof djConfig["loggingPostInterval"]!="undefined")?djConfig["loggingPostInterval"]:(_412||-1);
};
dojo.logging.MemoryLogHandler.prototype=new dojo.logging.LogHandler();
dojo.logging.MemoryLogHandler.prototype.emit=function(_414){
this.data.push(_414);
if(this.numRecords!=-1){
while(this.data.length>this.numRecords){
this.data.shift();
}
}
};
dojo.logging.logQueueHandler=new dojo.logging.MemoryLogHandler(0,50,0,10000);
dojo.logging.logQueueHandler.emit=function(_415){
var _416=String(dojo.log.getLevelName(_415.level)+": "+_415.time.toLocaleTimeString())+": "+_415.message;
if(!dj_undef("debug",dj_global)){
dojo.debug(_416);
}else{
if((typeof dj_global["print"]=="function")&&(!dojo.render.html.capable)){
print(_416);
}
}
this.data.push(_415);
if(this.numRecords!=-1){
while(this.data.length>this.numRecords){
this.data.shift();
}
}
};
dojo.logging.log.addHandler(dojo.logging.logQueueHandler);
dojo.log=dojo.logging.log;
dojo.kwCompoundRequire({common:["dojo.logging.Logger",false,false],rhino:["dojo.logging.RhinoLogger"]});
dojo.provide("dojo.logging.*");
dojo.provide("dojo.io.IO");
dojo.require("dojo.string");
dojo.require("dojo.lang.extras");
dojo.io.transports=[];
dojo.io.hdlrFuncNames=["load","error","timeout"];
dojo.io.Request=function(url,_418,_419,_41a){
if((arguments.length==1)&&(arguments[0].constructor==Object)){
this.fromKwArgs(arguments[0]);
}else{
this.url=url;
if(_418){
this.mimetype=_418;
}
if(_419){
this.transport=_419;
}
if(arguments.length>=4){
this.changeUrl=_41a;
}
}
};
dojo.lang.extend(dojo.io.Request,{url:"",mimetype:"text/plain",method:"GET",content:undefined,transport:undefined,changeUrl:undefined,formNode:undefined,sync:false,bindSuccess:false,useCache:false,preventCache:false,load:function(type,data,evt){
},error:function(type,_41f){
},timeout:function(type){
},handle:function(){
},timeoutSeconds:0,abort:function(){
},fromKwArgs:function(_421){
if(_421["url"]){
_421.url=_421.url.toString();
}
if(_421["formNode"]){
_421.formNode=dojo.byId(_421.formNode);
}
if(!_421["method"]&&_421["formNode"]&&_421["formNode"].method){
_421.method=_421["formNode"].method;
}
if(!_421["handle"]&&_421["handler"]){
_421.handle=_421.handler;
}
if(!_421["load"]&&_421["loaded"]){
_421.load=_421.loaded;
}
if(!_421["changeUrl"]&&_421["changeURL"]){
_421.changeUrl=_421.changeURL;
}
_421.encoding=dojo.lang.firstValued(_421["encoding"],djConfig["bindEncoding"],"");
_421.sendTransport=dojo.lang.firstValued(_421["sendTransport"],djConfig["ioSendTransport"],false);
var _422=dojo.lang.isFunction;
for(var x=0;x<dojo.io.hdlrFuncNames.length;x++){
var fn=dojo.io.hdlrFuncNames[x];
if(_422(_421[fn])){
continue;
}
if(_422(_421["handle"])){
_421[fn]=_421.handle;
}
}
dojo.lang.mixin(this,_421);
}});
dojo.io.Error=function(msg,type,num){
this.message=msg;
this.type=type||"unknown";
this.number=num||0;
};
dojo.io.transports.addTransport=function(name){
this.push(name);
this[name]=dojo.io[name];
};
dojo.io.bind=function(_429){
if(!(_429 instanceof dojo.io.Request)){
try{
_429=new dojo.io.Request(_429);
}
catch(e){
dojo.debug(e);
}
}
var _42a="";
if(_429["transport"]){
_42a=_429["transport"];
if(!this[_42a]){
return _429;
}
}else{
for(var x=0;x<dojo.io.transports.length;x++){
var tmp=dojo.io.transports[x];
if((this[tmp])&&(this[tmp].canHandle(_429))){
_42a=tmp;
}
}
if(_42a==""){
return _429;
}
}
this[_42a].bind(_429);
_429.bindSuccess=true;
return _429;
};
dojo.io.queueBind=function(_42d){
if(!(_42d instanceof dojo.io.Request)){
try{
_42d=new dojo.io.Request(_42d);
}
catch(e){
dojo.debug(e);
}
}
var _42e=_42d.load;
_42d.load=function(){
dojo.io._queueBindInFlight=false;
var ret=_42e.apply(this,arguments);
dojo.io._dispatchNextQueueBind();
return ret;
};
var _430=_42d.error;
_42d.error=function(){
dojo.io._queueBindInFlight=false;
var ret=_430.apply(this,arguments);
dojo.io._dispatchNextQueueBind();
return ret;
};
dojo.io._bindQueue.push(_42d);
dojo.io._dispatchNextQueueBind();
return _42d;
};
dojo.io._dispatchNextQueueBind=function(){
if(!dojo.io._queueBindInFlight){
dojo.io._queueBindInFlight=true;
if(dojo.io._bindQueue.length>0){
dojo.io.bind(dojo.io._bindQueue.shift());
}else{
dojo.io._queueBindInFlight=false;
}
}
};
dojo.io._bindQueue=[];
dojo.io._queueBindInFlight=false;
dojo.io.argsFromMap=function(map,_433,last){
var enc=/utf/i.test(_433||"")?encodeURIComponent:dojo.string.encodeAscii;
var _436=[];
var _437=new Object();
for(var name in map){
var _439=function(elt){
var val=enc(name)+"="+enc(elt);
_436[(last==name)?"push":"unshift"](val);
};
if(!_437[name]){
var _43c=map[name];
if(dojo.lang.isArray(_43c)){
dojo.lang.forEach(_43c,_439);
}else{
_439(_43c);
}
}
}
return _436.join("&");
};
dojo.io.setIFrameSrc=function(_43d,src,_43f){
try{
var r=dojo.render.html;
if(!_43f){
if(r.safari){
_43d.location=src;
}else{
frames[_43d.name].location=src;
}
}else{
var idoc;
if(r.ie){
idoc=_43d.contentWindow.document;
}else{
if(r.safari){
idoc=_43d.document;
}else{
idoc=_43d.contentWindow;
}
}
idoc.location.replace(src);
}
}
catch(e){
dojo.debug(e);
dojo.debug("setIFrameSrc: "+e);
}
};
dojo.provide("dojo.string.extras");
dojo.require("dojo.string.common");
dojo.require("dojo.lang");
dojo.string.paramString=function(str,_443,_444){
for(var name in _443){
var re=new RegExp("\\%\\{"+name+"\\}","g");
str=str.replace(re,_443[name]);
}
if(_444){
str=str.replace(/%\{([^\}\s]+)\}/g,"");
}
return str;
};
dojo.string.capitalize=function(str){
if(!dojo.lang.isString(str)){
return "";
}
if(arguments.length==0){
str=this;
}
var _448=str.split(" ");
var _449="";
var len=_448.length;
for(var i=0;i<len;i++){
var word=_448[i];
word=word.charAt(0).toUpperCase()+word.substring(1,word.length);
_449+=word;
if(i<len-1){
_449+=" ";
}
}
return new String(_449);
};
dojo.string.isBlank=function(str){
if(!dojo.lang.isString(str)){
return true;
}
return (dojo.string.trim(str).length==0);
};
dojo.string.encodeAscii=function(str){
if(!dojo.lang.isString(str)){
return str;
}
var ret="";
var _450=escape(str);
var _451,re=/%u([0-9A-F]{4})/i;
while((_451=_450.match(re))){
var num=Number("0x"+_451[1]);
var _453=escape("&#"+num+";");
ret+=_450.substring(0,_451.index)+_453;
_450=_450.substring(_451.index+_451[0].length);
}
ret+=_450.replace(/\+/g,"%2B");
return ret;
};
dojo.string.escape=function(type,str){
var args=[];
for(var i=1;i<arguments.length;i++){
args.push(arguments[i]);
}
switch(type.toLowerCase()){
case "xml":
case "html":
case "xhtml":
return dojo.string.escapeXml.apply(this,args);
case "sql":
return dojo.string.escapeSql.apply(this,args);
case "regexp":
case "regex":
return dojo.string.escapeRegExp.apply(this,args);
case "javascript":
case "jscript":
case "js":
return dojo.string.escapeJavaScript.apply(this,args);
case "ascii":
return dojo.string.encodeAscii.apply(this,args);
default:
return str;
}
};
dojo.string.escapeXml=function(str,_459){
str=str.replace(/&/gm,"&amp;").replace(/</gm,"&lt;").replace(/>/gm,"&gt;").replace(/"/gm,"&quot;");
if(!_459){
str=str.replace(/'/gm,"&#39;");
}
return str;
};
dojo.string.escapeSql=function(str){
return str.replace(/'/gm,"''");
};
dojo.string.escapeRegExp=function(str){
return str.replace(/\\/gm,"\\\\").replace(/([\f\b\n\t\r[\^$|?*+(){}])/gm,"\\$1");
};
dojo.string.escapeJavaScript=function(str){
return str.replace(/(["'\f\b\n\t\r])/gm,"\\$1");
};
dojo.string.escapeString=function(str){
return ("\""+str.replace(/(["\\])/g,"\\$1")+"\"").replace(/[\f]/g,"\\f").replace(/[\b]/g,"\\b").replace(/[\n]/g,"\\n").replace(/[\t]/g,"\\t").replace(/[\r]/g,"\\r");
};
dojo.string.summary=function(str,len){
if(!len||str.length<=len){
return str;
}else{
return str.substring(0,len).replace(/\.+$/,"")+"...";
}
};
dojo.string.endsWith=function(str,end,_462){
if(_462){
str=str.toLowerCase();
end=end.toLowerCase();
}
if((str.length-end.length)<0){
return false;
}
return str.lastIndexOf(end)==str.length-end.length;
};
dojo.string.endsWithAny=function(str){
for(var i=1;i<arguments.length;i++){
if(dojo.string.endsWith(str,arguments[i])){
return true;
}
}
return false;
};
dojo.string.startsWith=function(str,_466,_467){
if(_467){
str=str.toLowerCase();
_466=_466.toLowerCase();
}
return str.indexOf(_466)==0;
};
dojo.string.startsWithAny=function(str){
for(var i=1;i<arguments.length;i++){
if(dojo.string.startsWith(str,arguments[i])){
return true;
}
}
return false;
};
dojo.string.has=function(str){
for(var i=1;i<arguments.length;i++){
if(str.indexOf(arguments[i])>-1){
return true;
}
}
return false;
};
dojo.string.normalizeNewlines=function(text,_46d){
if(_46d=="\n"){
text=text.replace(/\r\n/g,"\n");
text=text.replace(/\r/g,"\n");
}else{
if(_46d=="\r"){
text=text.replace(/\r\n/g,"\r");
text=text.replace(/\n/g,"\r");
}else{
text=text.replace(/([^\r])\n/g,"$1\r\n");
text=text.replace(/\r([^\n])/g,"\r\n$1");
}
}
return text;
};
dojo.string.splitEscaped=function(str,_46f){
var _470=[];
for(var i=0,prevcomma=0;i<str.length;i++){
if(str.charAt(i)=="\\"){
i++;
continue;
}
if(str.charAt(i)==_46f){
_470.push(str.substring(prevcomma,i));
prevcomma=i+1;
}
}
_470.push(str.substr(prevcomma));
return _470;
};
dojo.provide("dojo.undo.browser");
dojo.require("dojo.io");
try{
if((!djConfig["preventBackButtonFix"])&&(!dojo.hostenv.post_load_)){
document.write("<iframe style='border: 0px; width: 1px; height: 1px; position: absolute; bottom: 0px; right: 0px; visibility: visible;' name='djhistory' id='djhistory' src='"+(dojo.hostenv.getBaseScriptUri()+"iframe_history.html")+"'></iframe>");
}
}
catch(e){
}
dojo.undo.browser={initialHref:window.location.href,initialHash:window.location.hash,moveForward:false,historyStack:[],forwardStack:[],historyIframe:null,bookmarkAnchor:null,locationTimer:null,setInitialState:function(args){
this.initialState={"url":this.initialHref,"kwArgs":args,"urlHash":this.initialHash};
},addToHistory:function(args){
var hash=null;
if(!this.historyIframe){
this.historyIframe=window.frames["djhistory"];
}
if(!this.bookmarkAnchor){
this.bookmarkAnchor=document.createElement("a");
(document.body||document.getElementsByTagName("body")[0]).appendChild(this.bookmarkAnchor);
this.bookmarkAnchor.style.display="none";
}
if((!args["changeUrl"])||(dojo.render.html.ie)){
var url=dojo.hostenv.getBaseScriptUri()+"iframe_history.html?"+(new Date()).getTime();
this.moveForward=true;
dojo.io.setIFrameSrc(this.historyIframe,url,false);
}
if(args["changeUrl"]){
this.changingUrl=true;
hash="#"+((args["changeUrl"]!==true)?args["changeUrl"]:(new Date()).getTime());
setTimeout("window.location.href = '"+hash+"'; dojo.undo.browser.changingUrl = false;",1);
this.bookmarkAnchor.href=hash;
if(dojo.render.html.ie){
var _476=args["back"]||args["backButton"]||args["handle"];
var tcb=function(_478){
if(window.location.hash!=""){
setTimeout("window.location.href = '"+hash+"';",1);
}
_476.apply(this,[_478]);
};
if(args["back"]){
args.back=tcb;
}else{
if(args["backButton"]){
args.backButton=tcb;
}else{
if(args["handle"]){
args.handle=tcb;
}
}
}
this.forwardStack=[];
var _479=args["forward"]||args["forwardButton"]||args["handle"];
var tfw=function(_47b){
if(window.location.hash!=""){
window.location.href=hash;
}
if(_479){
_479.apply(this,[_47b]);
}
};
if(args["forward"]){
args.forward=tfw;
}else{
if(args["forwardButton"]){
args.forwardButton=tfw;
}else{
if(args["handle"]){
args.handle=tfw;
}
}
}
}else{
if(dojo.render.html.moz){
if(!this.locationTimer){
this.locationTimer=setInterval("dojo.undo.browser.checkLocation();",200);
}
}
}
}
this.historyStack.push({"url":url,"kwArgs":args,"urlHash":hash});
},checkLocation:function(){
if(!this.changingUrl){
var hsl=this.historyStack.length;
if((window.location.hash==this.initialHash)||(window.location.href==this.initialHref)&&(hsl==1)){
this.handleBackButton();
return;
}
if(this.forwardStack.length>0){
if(this.forwardStack[this.forwardStack.length-1].urlHash==window.location.hash){
this.handleForwardButton();
return;
}
}
if((hsl>=2)&&(this.historyStack[hsl-2])){
if(this.historyStack[hsl-2].urlHash==window.location.hash){
this.handleBackButton();
return;
}
}
}
},iframeLoaded:function(evt,_47e){
var _47f=this._getUrlQuery(_47e.href);
if(_47f==null){
if(this.historyStack.length==1){
this.handleBackButton();
}
return;
}
if(this.moveForward){
this.moveForward=false;
return;
}
if(this.historyStack.length>=2&&_47f==this._getUrlQuery(this.historyStack[this.historyStack.length-2].url)){
this.handleBackButton();
}else{
if(this.forwardStack.length>0&&_47f==this._getUrlQuery(this.forwardStack[this.forwardStack.length-1].url)){
this.handleForwardButton();
}
}
},handleBackButton:function(){
var _480=this.historyStack.pop();
if(!_480){
return;
}
var last=this.historyStack[this.historyStack.length-1];
if(!last&&this.historyStack.length==0){
last=this.initialState;
}
if(last){
if(last.kwArgs["back"]){
last.kwArgs["back"]();
}else{
if(last.kwArgs["backButton"]){
last.kwArgs["backButton"]();
}else{
if(last.kwArgs["handle"]){
last.kwArgs.handle("back");
}
}
}
}
this.forwardStack.push(_480);
},handleForwardButton:function(){
var last=this.forwardStack.pop();
if(!last){
return;
}
if(last.kwArgs["forward"]){
last.kwArgs.forward();
}else{
if(last.kwArgs["forwardButton"]){
last.kwArgs.forwardButton();
}else{
if(last.kwArgs["handle"]){
last.kwArgs.handle("forward");
}
}
}
this.historyStack.push(last);
},_getUrlQuery:function(url){
var _484=url.split("?");
if(_484.length<2){
return null;
}else{
return _484[1];
}
}};
dojo.provide("dojo.io.BrowserIO");
dojo.require("dojo.io");
dojo.require("dojo.lang.array");
dojo.require("dojo.lang.func");
dojo.require("dojo.string.extras");
dojo.require("dojo.dom");
dojo.require("dojo.undo.browser");
dojo.io.checkChildrenForFile=function(node){
var _486=false;
var _487=node.getElementsByTagName("input");
dojo.lang.forEach(_487,function(_488){
if(_486){
return;
}
if(_488.getAttribute("type")=="file"){
_486=true;
}
});
return _486;
};
dojo.io.formHasFile=function(_489){
return dojo.io.checkChildrenForFile(_489);
};
dojo.io.updateNode=function(node,_48b){
node=dojo.byId(node);
var args=_48b;
if(dojo.lang.isString(_48b)){
args={url:_48b};
}
args.mimetype="text/html";
args.load=function(t,d,e){
while(node.firstChild){
if(dojo["event"]){
try{
dojo.event.browser.clean(node.firstChild);
}
catch(e){
}
}
node.removeChild(node.firstChild);
}
node.innerHTML=d;
};
dojo.io.bind(args);
};
dojo.io.formFilter=function(node){
var type=(node.type||"").toLowerCase();
return !node.disabled&&node.name&&!dojo.lang.inArray(type,["file","submit","image","reset","button"]);
};
dojo.io.encodeForm=function(_492,_493,_494){
if((!_492)||(!_492.tagName)||(!_492.tagName.toLowerCase()=="form")){
dojo.raise("Attempted to encode a non-form element.");
}
if(!_494){
_494=dojo.io.formFilter;
}
var enc=/utf/i.test(_493||"")?encodeURIComponent:dojo.string.encodeAscii;
var _496=[];
for(var i=0;i<_492.elements.length;i++){
var elm=_492.elements[i];
if(!elm||elm.tagName.toLowerCase()=="fieldset"||!_494(elm)){
continue;
}
var name=enc(elm.name);
var type=elm.type.toLowerCase();
if(type=="select-multiple"){
for(var j=0;j<elm.options.length;j++){
if(elm.options[j].selected){
_496.push(name+"="+enc(elm.options[j].value));
}
}
}else{
if(dojo.lang.inArray(type,["radio","checkbox"])){
if(elm.checked){
_496.push(name+"="+enc(elm.value));
}
}else{
_496.push(name+"="+enc(elm.value));
}
}
}
var _49c=_492.getElementsByTagName("input");
for(var i=0;i<_49c.length;i++){
var _49d=_49c[i];
if(_49d.type.toLowerCase()=="image"&&_49d.form==_492&&_494(_49d)){
var name=enc(_49d.name);
_496.push(name+"="+enc(_49d.value));
_496.push(name+".x=0");
_496.push(name+".y=0");
}
}
return _496.join("&")+"&";
};
dojo.io.FormBind=function(args){
this.bindArgs={};
if(args&&args.formNode){
this.init(args);
}else{
if(args){
this.init({formNode:args});
}
}
};
dojo.lang.extend(dojo.io.FormBind,{form:null,bindArgs:null,clickedButton:null,init:function(args){
var form=dojo.byId(args.formNode);
if(!form||!form.tagName||form.tagName.toLowerCase()!="form"){
throw new Error("FormBind: Couldn't apply, invalid form");
}else{
if(this.form==form){
return;
}else{
if(this.form){
throw new Error("FormBind: Already applied to a form");
}
}
}
dojo.lang.mixin(this.bindArgs,args);
this.form=form;
this.connect(form,"onsubmit","submit");
for(var i=0;i<form.elements.length;i++){
var node=form.elements[i];
if(node&&node.type&&dojo.lang.inArray(node.type.toLowerCase(),["submit","button"])){
this.connect(node,"onclick","click");
}
}
var _4a3=form.getElementsByTagName("input");
for(var i=0;i<_4a3.length;i++){
var _4a4=_4a3[i];
if(_4a4.type.toLowerCase()=="image"&&_4a4.form==form){
this.connect(_4a4,"onclick","click");
}
}
},onSubmit:function(form){
return true;
},submit:function(e){
e.preventDefault();
if(this.onSubmit(this.form)){
dojo.io.bind(dojo.lang.mixin(this.bindArgs,{formFilter:dojo.lang.hitch(this,"formFilter")}));
}
},click:function(e){
var node=e.currentTarget;
if(node.disabled){
return;
}
this.clickedButton=node;
},formFilter:function(node){
var type=(node.type||"").toLowerCase();
var _4ab=false;
if(node.disabled||!node.name){
_4ab=false;
}else{
if(dojo.lang.inArray(type,["submit","button","image"])){
if(!this.clickedButton){
this.clickedButton=node;
}
_4ab=node==this.clickedButton;
}else{
_4ab=!dojo.lang.inArray(type,["file","submit","reset","button"]);
}
}
return _4ab;
},connect:function(_4ac,_4ad,_4ae){
if(dojo.evalObjPath("dojo.event.connect")){
dojo.event.connect(_4ac,_4ad,this,_4ae);
}else{
var fcn=dojo.lang.hitch(this,_4ae);
_4ac[_4ad]=function(e){
if(!e){
e=window.event;
}
if(!e.currentTarget){
e.currentTarget=e.srcElement;
}
if(!e.preventDefault){
e.preventDefault=function(){
window.event.returnValue=false;
};
}
fcn(e);
};
}
}});
dojo.io.XMLHTTPTransport=new function(){
var _4b1=this;
var _4b2={};
this.useCache=false;
this.preventCache=false;
function getCacheKey(url,_4b4,_4b5){
return url+"|"+_4b4+"|"+_4b5.toLowerCase();
}
function addToCache(url,_4b7,_4b8,http){
_4b2[getCacheKey(url,_4b7,_4b8)]=http;
}
function getFromCache(url,_4bb,_4bc){
return _4b2[getCacheKey(url,_4bb,_4bc)];
}
this.clearCache=function(){
_4b2={};
};
function doLoad(_4bd,http,url,_4c0,_4c1){
if((http.status==200)||(http.status==304)||(http.status==204)||(location.protocol=="file:"&&(http.status==0||http.status==undefined))||(location.protocol=="chrome:"&&(http.status==0||http.status==undefined))){
var ret;
if(_4bd.method.toLowerCase()=="head"){
var _4c3=http.getAllResponseHeaders();
ret={};
ret.toString=function(){
return _4c3;
};
var _4c4=_4c3.split(/[\r\n]+/g);
for(var i=0;i<_4c4.length;i++){
var pair=_4c4[i].match(/^([^:]+)\s*:\s*(.+)$/i);
if(pair){
ret[pair[1]]=pair[2];
}
}
}else{
if(_4bd.mimetype=="text/javascript"){
try{
ret=dj_eval(http.responseText);
}
catch(e){
dojo.debug(e);
dojo.debug(http.responseText);
ret=null;
}
}else{
if(_4bd.mimetype=="text/json"){
try{
ret=dj_eval("("+http.responseText+")");
}
catch(e){
dojo.debug(e);
dojo.debug(http.responseText);
ret=false;
}
}else{
if((_4bd.mimetype=="application/xml")||(_4bd.mimetype=="text/xml")){
ret=http.responseXML;
if(!ret||typeof ret=="string"){
ret=dojo.dom.createDocumentFromText(http.responseText);
}
}else{
ret=http.responseText;
}
}
}
}
if(_4c1){
addToCache(url,_4c0,_4bd.method,http);
}
_4bd[(typeof _4bd.load=="function")?"load":"handle"]("load",ret,http,_4bd);
}else{
var _4c7=new dojo.io.Error("XMLHttpTransport Error: "+http.status+" "+http.statusText);
_4bd[(typeof _4bd.error=="function")?"error":"handle"]("error",_4c7,http,_4bd);
}
}
function setHeaders(http,_4c9){
if(_4c9["headers"]){
for(var _4ca in _4c9["headers"]){
if(_4ca.toLowerCase()=="content-type"&&!_4c9["contentType"]){
_4c9["contentType"]=_4c9["headers"][_4ca];
}else{
http.setRequestHeader(_4ca,_4c9["headers"][_4ca]);
}
}
}
}
this.inFlight=[];
this.inFlightTimer=null;
this.startWatchingInFlight=function(){
if(!this.inFlightTimer){
this.inFlightTimer=setInterval("dojo.io.XMLHTTPTransport.watchInFlight();",10);
}
};
this.watchInFlight=function(){
var now=null;
for(var x=this.inFlight.length-1;x>=0;x--){
var tif=this.inFlight[x];
if(!tif){
this.inFlight.splice(x,1);
continue;
}
if(4==tif.http.readyState){
this.inFlight.splice(x,1);
doLoad(tif.req,tif.http,tif.url,tif.query,tif.useCache);
}else{
if(tif.startTime){
if(!now){
now=(new Date()).getTime();
}
if(tif.startTime+(tif.req.timeoutSeconds*1000)<now){
if(typeof tif.http.abort=="function"){
tif.http.abort();
}
this.inFlight.splice(x,1);
tif.req[(typeof tif.req.timeout=="function")?"timeout":"handle"]("timeout",null,tif.http,tif.req);
}
}
}
}
if(this.inFlight.length==0){
clearInterval(this.inFlightTimer);
this.inFlightTimer=null;
}
};
var _4ce=dojo.hostenv.getXmlhttpObject()?true:false;
this.canHandle=function(_4cf){
return _4ce&&dojo.lang.inArray((_4cf["mimetype"].toLowerCase()||""),["text/plain","text/html","application/xml","text/xml","text/javascript","text/json"])&&!(_4cf["formNode"]&&dojo.io.formHasFile(_4cf["formNode"]));
};
this.multipartBoundary="45309FFF-BD65-4d50-99C9-36986896A96F";
this.bind=function(_4d0){
if(!_4d0["url"]){
if(!_4d0["formNode"]&&(_4d0["backButton"]||_4d0["back"]||_4d0["changeUrl"]||_4d0["watchForURL"])&&(!djConfig.preventBackButtonFix)){
dojo.deprecated("Using dojo.io.XMLHTTPTransport.bind() to add to browser history without doing an IO request is deprecated. Use dojo.undo.browser.addToHistory() instead.");
dojo.undo.browser.addToHistory(_4d0);
return true;
}
}
var url=_4d0.url;
var _4d2="";
if(_4d0["formNode"]){
var ta=_4d0.formNode.getAttribute("action");
if((ta)&&(!_4d0["url"])){
url=ta;
}
var tp=_4d0.formNode.getAttribute("method");
if((tp)&&(!_4d0["method"])){
_4d0.method=tp;
}
_4d2+=dojo.io.encodeForm(_4d0.formNode,_4d0.encoding,_4d0["formFilter"]);
}
if(url.indexOf("#")>-1){
dojo.debug("Warning: dojo.io.bind: stripping hash values from url:",url);
url=url.split("#")[0];
}
if(_4d0["file"]){
_4d0.method="post";
}
if(!_4d0["method"]){
_4d0.method="get";
}
if(_4d0.method.toLowerCase()=="get"){
_4d0.multipart=false;
}else{
if(_4d0["file"]){
_4d0.multipart=true;
}else{
if(!_4d0["multipart"]){
_4d0.multipart=false;
}
}
}
if(_4d0["backButton"]||_4d0["back"]||_4d0["changeUrl"]){
dojo.undo.browser.addToHistory(_4d0);
}
var _4d5=_4d0["content"]||{};
if(_4d0.sendTransport){
_4d5["dojo.transport"]="xmlhttp";
}
do{
if(_4d0.postContent){
_4d2=_4d0.postContent;
break;
}
if(_4d5){
_4d2+=dojo.io.argsFromMap(_4d5,_4d0.encoding);
}
if(_4d0.method.toLowerCase()=="get"||!_4d0.multipart){
break;
}
var t=[];
if(_4d2.length){
var q=_4d2.split("&");
for(var i=0;i<q.length;++i){
if(q[i].length){
var p=q[i].split("=");
t.push("--"+this.multipartBoundary,"Content-Disposition: form-data; name=\""+p[0]+"\"","",p[1]);
}
}
}
if(_4d0.file){
if(dojo.lang.isArray(_4d0.file)){
for(var i=0;i<_4d0.file.length;++i){
var o=_4d0.file[i];
t.push("--"+this.multipartBoundary,"Content-Disposition: form-data; name=\""+o.name+"\"; filename=\""+("fileName" in o?o.fileName:o.name)+"\"","Content-Type: "+("contentType" in o?o.contentType:"application/octet-stream"),"",o.content);
}
}else{
var o=_4d0.file;
t.push("--"+this.multipartBoundary,"Content-Disposition: form-data; name=\""+o.name+"\"; filename=\""+("fileName" in o?o.fileName:o.name)+"\"","Content-Type: "+("contentType" in o?o.contentType:"application/octet-stream"),"",o.content);
}
}
if(t.length){
t.push("--"+this.multipartBoundary+"--","");
_4d2=t.join("\r\n");
}
}while(false);
var _4db=_4d0["sync"]?false:true;
var _4dc=_4d0["preventCache"]||(this.preventCache==true&&_4d0["preventCache"]!=false);
var _4dd=_4d0["useCache"]==true||(this.useCache==true&&_4d0["useCache"]!=false);
if(!_4dc&&_4dd){
var _4de=getFromCache(url,_4d2,_4d0.method);
if(_4de){
doLoad(_4d0,_4de,url,_4d2,false);
return;
}
}
var http=dojo.hostenv.getXmlhttpObject(_4d0);
var _4e0=false;
if(_4db){
var _4e1=this.inFlight.push({"req":_4d0,"http":http,"url":url,"query":_4d2,"useCache":_4dd,"startTime":_4d0.timeoutSeconds?(new Date()).getTime():0});
this.startWatchingInFlight();
}
if(_4d0.method.toLowerCase()=="post"){
http.open("POST",url,_4db);
setHeaders(http,_4d0);
http.setRequestHeader("Content-Type",_4d0.multipart?("multipart/form-data; boundary="+this.multipartBoundary):(_4d0.contentType||"application/x-www-form-urlencoded"));
try{
http.send(_4d2);
}
catch(e){
if(typeof http.abort=="function"){
http.abort();
}
doLoad(_4d0,{status:404},url,_4d2,_4dd);
}
}else{
var _4e2=url;
if(_4d2!=""){
_4e2+=(_4e2.indexOf("?")>-1?"&":"?")+_4d2;
}
if(_4dc){
_4e2+=(dojo.string.endsWithAny(_4e2,"?","&")?"":(_4e2.indexOf("?")>-1?"&":"?"))+"dojo.preventCache="+new Date().valueOf();
}
http.open(_4d0.method.toUpperCase(),_4e2,_4db);
setHeaders(http,_4d0);
try{
http.send(null);
}
catch(e){
if(typeof http.abort=="function"){
http.abort();
}
doLoad(_4d0,{status:404},url,_4d2,_4dd);
}
}
if(!_4db){
doLoad(_4d0,http,url,_4d2,_4dd);
}
_4d0.abort=function(){
return http.abort();
};
return;
};
dojo.io.transports.addTransport("XMLHTTPTransport");
};
dojo.provide("dojo.io.cookie");
dojo.io.cookie.setCookie=function(name,_4e4,days,path,_4e7,_4e8){
var _4e9=-1;
if(typeof days=="number"&&days>=0){
var d=new Date();
d.setTime(d.getTime()+(days*24*60*60*1000));
_4e9=d.toGMTString();
}
_4e4=escape(_4e4);
document.cookie=name+"="+_4e4+";"+(_4e9!=-1?" expires="+_4e9+";":"")+(path?"path="+path:"")+(_4e7?"; domain="+_4e7:"")+(_4e8?"; secure":"");
};
dojo.io.cookie.set=dojo.io.cookie.setCookie;
dojo.io.cookie.getCookie=function(name){
var idx=document.cookie.lastIndexOf(name+"=");
if(idx==-1){
return null;
}
value=document.cookie.substring(idx+name.length+1);
var end=value.indexOf(";");
if(end==-1){
end=value.length;
}
value=value.substring(0,end);
value=unescape(value);
return value;
};
dojo.io.cookie.get=dojo.io.cookie.getCookie;
dojo.io.cookie.deleteCookie=function(name){
dojo.io.cookie.setCookie(name,"-",0);
};
dojo.io.cookie.setObjectCookie=function(name,obj,days,path,_4f3,_4f4,_4f5){
if(arguments.length==5){
_4f5=_4f3;
_4f3=null;
_4f4=null;
}
var _4f6=[],cookie,value="";
if(!_4f5){
cookie=dojo.io.cookie.getObjectCookie(name);
}
if(days>=0){
if(!cookie){
cookie={};
}
for(var prop in obj){
if(prop==null){
delete cookie[prop];
}else{
if(typeof obj[prop]=="string"||typeof obj[prop]=="number"){
cookie[prop]=obj[prop];
}
}
}
prop=null;
for(var prop in cookie){
_4f6.push(escape(prop)+"="+escape(cookie[prop]));
}
value=_4f6.join("&");
}
dojo.io.cookie.setCookie(name,value,days,path,_4f3,_4f4);
};
dojo.io.cookie.getObjectCookie=function(name){
var _4f9=null,cookie=dojo.io.cookie.getCookie(name);
if(cookie){
_4f9={};
var _4fa=cookie.split("&");
for(var i=0;i<_4fa.length;i++){
var pair=_4fa[i].split("=");
var _4fd=pair[1];
if(isNaN(_4fd)){
_4fd=unescape(pair[1]);
}
_4f9[unescape(pair[0])]=_4fd;
}
}
return _4f9;
};
dojo.io.cookie.isSupported=function(){
if(typeof navigator.cookieEnabled!="boolean"){
dojo.io.cookie.setCookie("__TestingYourBrowserForCookieSupport__","CookiesAllowed",90,null);
var _4fe=dojo.io.cookie.getCookie("__TestingYourBrowserForCookieSupport__");
navigator.cookieEnabled=(_4fe=="CookiesAllowed");
if(navigator.cookieEnabled){
this.deleteCookie("__TestingYourBrowserForCookieSupport__");
}
}
return navigator.cookieEnabled;
};
if(!dojo.io.cookies){
dojo.io.cookies=dojo.io.cookie;
}
dojo.kwCompoundRequire({common:["dojo.io"],rhino:["dojo.io.RhinoIO"],browser:["dojo.io.BrowserIO","dojo.io.cookie"],dashboard:["dojo.io.BrowserIO","dojo.io.cookie"]});
dojo.provide("dojo.io.*");
dojo.kwCompoundRequire({common:["dojo.uri.Uri",false,false]});
dojo.provide("dojo.uri.*");
dojo.provide("dojo.io.IframeIO");
dojo.require("dojo.io.BrowserIO");
dojo.require("dojo.uri.*");
dojo.io.createIFrame=function(_4ff,_500){
if(window[_4ff]){
return window[_4ff];
}
if(window.frames[_4ff]){
return window.frames[_4ff];
}
var r=dojo.render.html;
var _502=null;
var turi=dojo.uri.dojoUri("iframe_history.html?noInit=true");
var _504=((r.ie)&&(dojo.render.os.win))?"<iframe name='"+_4ff+"' src='"+turi+"' onload='"+_500+"'>":"iframe";
_502=document.createElement(_504);
with(_502){
name=_4ff;
setAttribute("name",_4ff);
id=_4ff;
}
(document.body||document.getElementsByTagName("body")[0]).appendChild(_502);
window[_4ff]=_502;
with(_502.style){
position="absolute";
left=top="0px";
height=width="1px";
visibility="hidden";
}
if(!r.ie){
dojo.io.setIFrameSrc(_502,turi,true);
_502.onload=new Function(_500);
}
return _502;
};
dojo.io.iframeContentWindow=function(_505){
var win=_505.contentWindow||dojo.io.iframeContentDocument(_505).defaultView||dojo.io.iframeContentDocument(_505).__parent__||(_505.name&&document.frames[_505.name])||null;
return win;
};
dojo.io.iframeContentDocument=function(_507){
var doc=_507.contentDocument||((_507.contentWindow)&&(_507.contentWindow.document))||((_507.name)&&(document.frames[_507.name])&&(document.frames[_507.name].document))||null;
return doc;
};
dojo.io.IframeTransport=new function(){
var _509=this;
this.currentRequest=null;
this.requestQueue=[];
this.iframeName="dojoIoIframe";
this.fireNextRequest=function(){
if((this.currentRequest)||(this.requestQueue.length==0)){
return;
}
var cr=this.currentRequest=this.requestQueue.shift();
cr._contentToClean=[];
var fn=cr["formNode"];
var _50c=cr["content"]||{};
if(cr.sendTransport){
_50c["dojo.transport"]="iframe";
}
if(fn){
if(_50c){
for(var x in _50c){
if(!fn[x]){
var tn;
if(dojo.render.html.ie){
tn=document.createElement("<input type='hidden' name='"+x+"' value='"+_50c[x]+"'>");
fn.appendChild(tn);
}else{
tn=document.createElement("input");
fn.appendChild(tn);
tn.type="hidden";
tn.name=x;
tn.value=_50c[x];
}
cr._contentToClean.push(x);
}else{
fn[x].value=_50c[x];
}
}
}
if(cr["url"]){
cr._originalAction=fn.getAttribute("action");
fn.setAttribute("action",cr.url);
}
if(!fn.getAttribute("method")){
fn.setAttribute("method",(cr["method"])?cr["method"]:"post");
}
cr._originalTarget=fn.getAttribute("target");
fn.setAttribute("target",this.iframeName);
fn.target=this.iframeName;
fn.submit();
}else{
var _50f=dojo.io.argsFromMap(this.currentRequest.content);
var _510=(cr.url.indexOf("?")>-1?"&":"?")+_50f;
dojo.io.setIFrameSrc(this.iframe,_510,true);
}
};
this.canHandle=function(_511){
return ((dojo.lang.inArray(_511["mimetype"],["text/plain","text/html","application/xml","text/xml","text/javascript","text/json"]))&&((_511["formNode"])&&(dojo.io.checkChildrenForFile(_511["formNode"])))&&(dojo.lang.inArray(_511["method"].toLowerCase(),["post","get"]))&&(!((_511["sync"])&&(_511["sync"]==true))));
};
this.bind=function(_512){
if(!this["iframe"]){
this.setUpIframe();
}
this.requestQueue.push(_512);
this.fireNextRequest();
return;
};
this.setUpIframe=function(){
this.iframe=dojo.io.createIFrame(this.iframeName,"dojo.io.IframeTransport.iframeOnload();");
};
this.iframeOnload=function(){
if(!_509.currentRequest){
_509.fireNextRequest();
return;
}
var req=_509.currentRequest;
var _514=req._contentToClean;
for(var i=0;i<_514.length;i++){
var key=_514[i];
var _517=req.formNode[key];
req.formNode.removeChild(_517);
req.formNode[key]=null;
}
if(req["_originalAction"]){
req.formNode.setAttribute("action",req._originalAction);
}
req.formNode.setAttribute("target",req._originalTarget);
req.formNode.target=req._originalTarget;
var ifr=_509.iframe;
var ifw=dojo.io.iframeContentWindow(ifr);
var _51a;
var _51b=false;
try{
var cmt=req.mimetype;
if((cmt=="text/javascript")||(cmt=="text/json")){
var cd=dojo.io.iframeContentDocument(_509.iframe);
var js=cd.getElementsByTagName("textarea")[0].value;
if(cmt=="text/json"){
js="("+js+")";
}
_51a=dj_eval(js);
}else{
if((cmt=="application/xml")||(cmt=="text/xml")){
_51a=dojo.io.iframeContentDocument(_509.iframe);
}else{
_51a=ifw.innerHTML;
}
}
_51b=true;
}
catch(e){
var _51f=new dojo.io.Error("IframeTransport Error");
if(dojo.lang.isFunction(req["error"])){
req.error("error",_51f,req);
}
}
try{
if(_51b&&dojo.lang.isFunction(req["load"])){
req.load("load",_51a,req);
}
}
catch(e){
throw e;
}
finally{
_509.currentRequest=null;
_509.fireNextRequest();
}
};
dojo.io.transports.addTransport("IframeTransport");
};
dojo.provide("dojo.date");
dojo.date.setDayOfYear=function(_520,_521){
_520.setMonth(0);
_520.setDate(_521);
return _520;
};
dojo.date.getDayOfYear=function(_522){
var _523=new Date(_522.getFullYear(),0,1);
return Math.floor((_522.getTime()-_523.getTime())/86400000);
};
dojo.date.setWeekOfYear=function(_524,week,_526){
if(arguments.length==1){
_526=0;
}
dojo.unimplemented("dojo.date.setWeekOfYear");
};
dojo.date.getWeekOfYear=function(_527,_528){
if(arguments.length==1){
_528=0;
}
var _529=new Date(_527.getFullYear(),0,1);
var day=_529.getDay();
_529.setDate(_529.getDate()-day+_528-(day>_528?7:0));
return Math.floor((_527.getTime()-_529.getTime())/604800000);
};
dojo.date.setIsoWeekOfYear=function(_52b,week,_52d){
if(arguments.length==1){
_52d=1;
}
dojo.unimplemented("dojo.date.setIsoWeekOfYear");
};
dojo.date.getIsoWeekOfYear=function(_52e,_52f){
if(arguments.length==1){
_52f=1;
}
dojo.unimplemented("dojo.date.getIsoWeekOfYear");
};
dojo.date.setIso8601=function(_530,_531){
var _532=(_531.indexOf("T")==-1)?_531.split(" "):_531.split("T");
dojo.date.setIso8601Date(_530,_532[0]);
if(_532.length==2){
dojo.date.setIso8601Time(_530,_532[1]);
}
return _530;
};
dojo.date.fromIso8601=function(_533){
return dojo.date.setIso8601(new Date(0,0),_533);
};
dojo.date.setIso8601Date=function(_534,_535){
var _536="^([0-9]{4})((-?([0-9]{2})(-?([0-9]{2}))?)|"+"(-?([0-9]{3}))|(-?W([0-9]{2})(-?([1-7]))?))?$";
var d=_535.match(new RegExp(_536));
if(!d){
dojo.debug("invalid date string: "+_535);
return false;
}
var year=d[1];
var _539=d[4];
var date=d[6];
var _53b=d[8];
var week=d[10];
var _53d=(d[12])?d[12]:1;
_534.setYear(year);
if(_53b){
dojo.date.setDayOfYear(_534,Number(_53b));
}else{
if(week){
_534.setMonth(0);
_534.setDate(1);
var gd=_534.getDay();
var day=(gd)?gd:7;
var _540=Number(_53d)+(7*Number(week));
if(day<=4){
_534.setDate(_540+1-day);
}else{
_534.setDate(_540+8-day);
}
}else{
if(_539){
_534.setDate(1);
_534.setMonth(_539-1);
}
if(date){
_534.setDate(date);
}
}
}
return _534;
};
dojo.date.fromIso8601Date=function(_541){
return dojo.date.setIso8601Date(new Date(0,0),_541);
};
dojo.date.setIso8601Time=function(_542,_543){
var _544="Z|(([-+])([0-9]{2})(:?([0-9]{2}))?)$";
var d=_543.match(new RegExp(_544));
var _546=0;
if(d){
if(d[0]!="Z"){
_546=(Number(d[3])*60)+Number(d[5]);
_546*=((d[2]=="-")?1:-1);
}
_546-=_542.getTimezoneOffset();
_543=_543.substr(0,_543.length-d[0].length);
}
var _547="^([0-9]{2})(:?([0-9]{2})(:?([0-9]{2})(.([0-9]+))?)?)?$";
var d=_543.match(new RegExp(_547));
if(!d){
dojo.debug("invalid time string: "+_543);
return false;
}
var _548=d[1];
var mins=Number((d[3])?d[3]:0);
var secs=(d[5])?d[5]:0;
var ms=d[7]?(Number("0."+d[7])*1000):0;
_542.setHours(_548);
_542.setMinutes(mins);
_542.setSeconds(secs);
_542.setMilliseconds(ms);
return _542;
};
dojo.date.fromIso8601Time=function(_54c){
return dojo.date.setIso8601Time(new Date(0,0),_54c);
};
dojo.date.shortTimezones=["IDLW","BET","HST","MART","AKST","PST","MST","CST","EST","AST","NFT","BST","FST","AT","GMT","CET","EET","MSK","IRT","GST","AFT","AGTT","IST","NPT","ALMT","MMT","JT","AWST","JST","ACST","AEST","LHST","VUT","NFT","NZT","CHAST","PHOT","LINT"];
dojo.date.timezoneOffsets=[-720,-660,-600,-570,-540,-480,-420,-360,-300,-240,-210,-180,-120,-60,0,60,120,180,210,240,270,300,330,345,360,390,420,480,540,570,600,630,660,690,720,765,780,840];
dojo.date.months=["January","February","March","April","May","June","July","August","September","October","November","December"];
dojo.date.shortMonths=["Jan","Feb","Mar","Apr","May","June","July","Aug","Sep","Oct","Nov","Dec"];
dojo.date.days=["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"];
dojo.date.shortDays=["Sun","Mon","Tues","Wed","Thur","Fri","Sat"];
dojo.date.getDaysInMonth=function(_54d){
var _54e=_54d.getMonth();
var days=[31,28,31,30,31,30,31,31,30,31,30,31];
if(_54e==1&&dojo.date.isLeapYear(_54d)){
return 29;
}else{
return days[_54e];
}
};
dojo.date.isLeapYear=function(_550){
var year=_550.getFullYear();
return (year%400==0)?true:(year%100==0)?false:(year%4==0)?true:false;
};
dojo.date.getDayName=function(_552){
return dojo.date.days[_552.getDay()];
};
dojo.date.getDayShortName=function(_553){
return dojo.date.shortDays[_553.getDay()];
};
dojo.date.getMonthName=function(_554){
return dojo.date.months[_554.getMonth()];
};
dojo.date.getMonthShortName=function(_555){
return dojo.date.shortMonths[_555.getMonth()];
};
dojo.date.getTimezoneName=function(_556){
var _557=-(_556.getTimezoneOffset());
for(var i=0;i<dojo.date.timezoneOffsets.length;i++){
if(dojo.date.timezoneOffsets[i]==_557){
return dojo.date.shortTimezones[i];
}
}
function $(s){
s=String(s);
while(s.length<2){
s="0"+s;
}
return s;
}
return (_557<0?"-":"+")+$(Math.floor(Math.abs(_557)/60))+":"+$(Math.abs(_557)%60);
};
dojo.date.getOrdinal=function(_55a){
var date=_55a.getDate();
if(date%100!=11&&date%10==1){
return "st";
}else{
if(date%100!=12&&date%10==2){
return "nd";
}else{
if(date%100!=13&&date%10==3){
return "rd";
}else{
return "th";
}
}
}
};
dojo.date.format=dojo.date.strftime=function(_55c,_55d){
var _55e=null;
function _(s,n){
s=String(s);
n=(n||2)-s.length;
while(n-->0){
s=(_55e==null?"0":_55e)+s;
}
return s;
}
function $(_561){
switch(_561){
case "a":
return dojo.date.getDayShortName(_55c);
break;
case "A":
return dojo.date.getDayName(_55c);
break;
case "b":
case "h":
return dojo.date.getMonthShortName(_55c);
break;
case "B":
return dojo.date.getMonthName(_55c);
break;
case "c":
return _55c.toLocaleString();
break;
case "C":
return _(Math.floor(_55c.getFullYear()/100));
break;
case "d":
return _(_55c.getDate());
break;
case "D":
return $("m")+"/"+$("d")+"/"+$("y");
break;
case "e":
if(_55e==null){
_55e=" ";
}
return _(_55c.getDate(),2);
break;
case "g":
break;
case "G":
break;
case "F":
return $("Y")+"-"+$("m")+"-"+$("d");
break;
case "H":
return _(_55c.getHours());
break;
case "I":
return _(_55c.getHours()%12||12);
break;
case "j":
return _(dojo.date.getDayOfYear(_55c),3);
break;
case "m":
return _(_55c.getMonth()+1);
break;
case "M":
return _(_55c.getMinutes());
break;
case "n":
return "\n";
break;
case "p":
return _55c.getHours()<12?"am":"pm";
break;
case "r":
return $("I")+":"+$("M")+":"+$("S")+" "+$("p");
break;
case "R":
return $("H")+":"+$("M");
break;
case "S":
return _(_55c.getSeconds());
break;
case "t":
return "\t";
break;
case "T":
return $("H")+":"+$("M")+":"+$("S");
break;
case "u":
return String(_55c.getDay()||7);
break;
case "U":
return _(dojo.date.getWeekOfYear(_55c));
break;
case "V":
return _(dojo.date.getIsoWeekOfYear(_55c));
break;
case "W":
return _(dojo.date.getWeekOfYear(_55c,1));
break;
case "w":
return String(_55c.getDay());
break;
case "x":
break;
case "X":
break;
case "y":
return _(_55c.getFullYear()%100);
break;
case "Y":
return String(_55c.getFullYear());
break;
case "z":
var _562=_55c.getTimezoneOffset();
return (_562<0?"-":"+")+_(Math.floor(Math.abs(_562)/60))+":"+_(Math.abs(_562)%60);
break;
case "Z":
return dojo.date.getTimezoneName(_55c);
break;
case "%":
return "%";
break;
}
}
var _563="";
var i=0,index=0,switchCase;
while((index=_55d.indexOf("%",i))!=-1){
_563+=_55d.substring(i,index++);
switch(_55d.charAt(index++)){
case "_":
_55e=" ";
break;
case "-":
_55e="";
break;
case "0":
_55e="0";
break;
case "^":
switchCase="upper";
break;
case "#":
switchCase="swap";
break;
default:
_55e=null;
index--;
break;
}
property=$(_55d.charAt(index++));
if(switchCase=="upper"||(switchCase=="swap"&&/[a-z]/.test(property))){
property=property.toUpperCase();
}else{
if(switchCase=="swap"&&!/[a-z]/.test(property)){
property=property.toLowerCase();
}
}
swicthCase=null;
_563+=property;
i=index;
}
_563+=_55d.substring(i);
return _563;
};
dojo.date.compareTypes={DATE:1,TIME:2};
dojo.date.compare=function(_565,_566,_567){
var dA=_565;
var dB=_566||new Date();
var now=new Date();
var opt=_567||(dojo.date.compareTypes.DATE|dojo.date.compareTypes.TIME);
var d1=new Date(((opt&dojo.date.compareTypes.DATE)?(dA.getFullYear()):now.getFullYear()),((opt&dojo.date.compareTypes.DATE)?(dA.getMonth()):now.getMonth()),((opt&dojo.date.compareTypes.DATE)?(dA.getDate()):now.getDate()),((opt&dojo.date.compareTypes.TIME)?(dA.getHours()):0),((opt&dojo.date.compareTypes.TIME)?(dA.getMinutes()):0),((opt&dojo.date.compareTypes.TIME)?(dA.getSeconds()):0));
var d2=new Date(((opt&dojo.date.compareTypes.DATE)?(dB.getFullYear()):now.getFullYear()),((opt&dojo.date.compareTypes.DATE)?(dB.getMonth()):now.getMonth()),((opt&dojo.date.compareTypes.DATE)?(dB.getDate()):now.getDate()),((opt&dojo.date.compareTypes.TIME)?(dB.getHours()):0),((opt&dojo.date.compareTypes.TIME)?(dB.getMinutes()):0),((opt&dojo.date.compareTypes.TIME)?(dB.getSeconds()):0));
if(d1.valueOf()>d2.valueOf()){
return 1;
}
if(d1.valueOf()<d2.valueOf()){
return -1;
}
return 0;
};
dojo.date.dateParts={YEAR:0,MONTH:1,DAY:2,HOUR:3,MINUTE:4,SECOND:5,MILLISECOND:6};
dojo.date.add=function(d,unit,_570){
var n=(_570)?_570:1;
var v;
switch(unit){
case dojo.date.dateParts.YEAR:
v=new Date(d.getFullYear()+n,d.getMonth(),d.getDate(),d.getHours(),d.getMinutes(),d.getSeconds(),d.getMilliseconds());
break;
case dojo.date.dateParts.MONTH:
v=new Date(d.getFullYear(),d.getMonth()+n,d.getDate(),d.getHours(),d.getMinutes(),d.getSeconds(),d.getMilliseconds());
break;
case dojo.date.dateParts.HOUR:
v=new Date(d.getFullYear(),d.getMonth(),d.getDate(),d.getHours()+n,d.getMinutes(),d.getSeconds(),d.getMilliseconds());
break;
case dojo.date.dateParts.MINUTE:
v=new Date(d.getFullYear(),d.getMonth(),d.getDate(),d.getHours(),d.getMinutes()+n,d.getSeconds(),d.getMilliseconds());
break;
case dojo.date.dateParts.SECOND:
v=new Date(d.getFullYear(),d.getMonth(),d.getDate(),d.getHours(),d.getMinutes(),d.getSeconds()+n,d.getMilliseconds());
break;
case dojo.date.dateParts.MILLISECOND:
v=new Date(d.getFullYear(),d.getMonth(),d.getDate(),d.getHours(),d.getMinutes(),d.getSeconds(),d.getMilliseconds()+n);
break;
default:
v=new Date(d.getFullYear(),d.getMonth(),d.getDate()+n,d.getHours(),d.getMinutes(),d.getSeconds(),d.getMilliseconds());
}
return v;
};
dojo.date.toString=function(date,_574){
dojo.deprecated("dojo.date.toString","use dojo.date.format instead","0.4");
if(_574.indexOf("#d")>-1){
_574=_574.replace(/#dddd/g,dojo.date.getDayOfWeekName(date));
_574=_574.replace(/#ddd/g,dojo.date.getShortDayOfWeekName(date));
_574=_574.replace(/#dd/g,(date.getDate().toString().length==1?"0":"")+date.getDate());
_574=_574.replace(/#d/g,date.getDate());
}
if(_574.indexOf("#M")>-1){
_574=_574.replace(/#MMMM/g,dojo.date.getMonthName(date));
_574=_574.replace(/#MMM/g,dojo.date.getShortMonthName(date));
_574=_574.replace(/#MM/g,((date.getMonth()+1).toString().length==1?"0":"")+(date.getMonth()+1));
_574=_574.replace(/#M/g,date.getMonth()+1);
}
if(_574.indexOf("#y")>-1){
var _575=date.getFullYear().toString();
_574=_574.replace(/#yyyy/g,_575);
_574=_574.replace(/#yy/g,_575.substring(2));
_574=_574.replace(/#y/g,_575.substring(3));
}
if(_574.indexOf("#")==-1){
return _574;
}
if(_574.indexOf("#h")>-1){
var _576=date.getHours();
_576=(_576>12?_576-12:(_576==0)?12:_576);
_574=_574.replace(/#hh/g,(_576.toString().length==1?"0":"")+_576);
_574=_574.replace(/#h/g,_576);
}
if(_574.indexOf("#H")>-1){
_574=_574.replace(/#HH/g,(date.getHours().toString().length==1?"0":"")+date.getHours());
_574=_574.replace(/#H/g,date.getHours());
}
if(_574.indexOf("#m")>-1){
_574=_574.replace(/#mm/g,(date.getMinutes().toString().length==1?"0":"")+date.getMinutes());
_574=_574.replace(/#m/g,date.getMinutes());
}
if(_574.indexOf("#s")>-1){
_574=_574.replace(/#ss/g,(date.getSeconds().toString().length==1?"0":"")+date.getSeconds());
_574=_574.replace(/#s/g,date.getSeconds());
}
if(_574.indexOf("#T")>-1){
_574=_574.replace(/#TT/g,date.getHours()>=12?"PM":"AM");
_574=_574.replace(/#T/g,date.getHours()>=12?"P":"A");
}
if(_574.indexOf("#t")>-1){
_574=_574.replace(/#tt/g,date.getHours()>=12?"pm":"am");
_574=_574.replace(/#t/g,date.getHours()>=12?"p":"a");
}
return _574;
};
dojo.date.daysInMonth=function(_577,year){
dojo.deprecated("daysInMonth(month, year)","replaced by getDaysInMonth(dateObject)","0.4");
return dojo.date.getDaysInMonth(new Date(year,_577,1));
};
dojo.date.toLongDateString=function(date){
dojo.deprecated("dojo.date.toLongDateString","use dojo.date.format(date, \"%B %e, %Y\") instead","0.4");
return dojo.date.format(date,"%B %e, %Y");
};
dojo.date.toShortDateString=function(date){
dojo.deprecated("dojo.date.toShortDateString","use dojo.date.format(date, \"%b %e, %Y\") instead","0.4");
return dojo.date.format(date,"%b %e, %Y");
};
dojo.date.toMilitaryTimeString=function(date){
dojo.deprecated("dojo.date.toMilitaryTimeString","use dojo.date.format(date, \"%T\")","0.4");
return dojo.date.format(date,"%T");
};
dojo.date.toRelativeString=function(date){
var now=new Date();
var diff=(now-date)/1000;
var end=" ago";
var _580=false;
if(diff<0){
_580=true;
end=" from now";
diff=-diff;
}
if(diff<60){
diff=Math.round(diff);
return diff+" second"+(diff==1?"":"s")+end;
}else{
if(diff<3600){
diff=Math.round(diff/60);
return diff+" minute"+(diff==1?"":"s")+end;
}else{
if(diff<3600*24&&date.getDay()==now.getDay()){
diff=Math.round(diff/3600);
return diff+" hour"+(diff==1?"":"s")+end;
}else{
if(diff<3600*24*7){
diff=Math.round(diff/(3600*24));
if(diff==1){
return _580?"Tomorrow":"Yesterday";
}else{
return diff+" days"+end;
}
}else{
return dojo.date.toShortDateString(date);
}
}
}
}
};
dojo.date.getDayOfWeekName=function(date){
dojo.deprecated("dojo.date.getDayOfWeekName","use dojo.date.getDayName instead","0.4");
return dojo.date.days[date.getDay()];
};
dojo.date.getShortDayOfWeekName=function(date){
dojo.deprecated("dojo.date.getShortDayOfWeekName","use dojo.date.getDayShortName instead","0.4");
return dojo.date.shortDays[date.getDay()];
};
dojo.date.getShortMonthName=function(date){
dojo.deprecated("dojo.date.getShortMonthName","use dojo.date.getMonthShortName instead","0.4");
return dojo.date.shortMonths[date.getMonth()];
};
dojo.date.toSql=function(date,_585){
return dojo.date.format(date,"%F"+!_585?" %T":"");
};
dojo.date.fromSql=function(_586){
var _587=_586.split(/[\- :]/g);
while(_587.length<6){
_587.push(0);
}
return new Date(_587[0],(parseInt(_587[1],10)-1),_587[2],_587[3],_587[4],_587[5]);
};
dojo.provide("dojo.string.Builder");
dojo.require("dojo.string");
dojo.string.Builder=function(str){
this.arrConcat=(dojo.render.html.capable&&dojo.render.html["ie"]);
var a=[];
var b=str||"";
var _58b=this.length=b.length;
if(this.arrConcat){
if(b.length>0){
a.push(b);
}
b="";
}
this.toString=this.valueOf=function(){
return (this.arrConcat)?a.join(""):b;
};
this.append=function(s){
if(this.arrConcat){
a.push(s);
}else{
b+=s;
}
_58b+=s.length;
this.length=_58b;
return this;
};
this.clear=function(){
a=[];
b="";
_58b=this.length=0;
return this;
};
this.remove=function(f,l){
var s="";
if(this.arrConcat){
b=a.join("");
}
a=[];
if(f>0){
s=b.substring(0,(f-1));
}
b=s+b.substring(f+l);
_58b=this.length=b.length;
if(this.arrConcat){
a.push(b);
b="";
}
return this;
};
this.replace=function(o,n){
if(this.arrConcat){
b=a.join("");
}
a=[];
b=b.replace(o,n);
_58b=this.length=b.length;
if(this.arrConcat){
a.push(b);
b="";
}
return this;
};
this.insert=function(idx,s){
if(this.arrConcat){
b=a.join("");
}
a=[];
if(idx==0){
b=s+b;
}else{
var t=b.split("");
t.splice(idx,0,s);
b=t.join("");
}
_58b=this.length=b.length;
if(this.arrConcat){
a.push(b);
b="";
}
return this;
};
};
dojo.kwCompoundRequire({common:["dojo.string","dojo.string.common","dojo.string.extras","dojo.string.Builder"]});
dojo.provide("dojo.string.*");
if(!this["dojo"]){
alert("\"dojo/__package__.js\" is now located at \"dojo/dojo.js\". Please update your includes accordingly");
}
dojo.provide("dojo.AdapterRegistry");
dojo.require("dojo.lang.func");
dojo.AdapterRegistry=function(){
this.pairs=[];
};
dojo.lang.extend(dojo.AdapterRegistry,{register:function(name,_596,wrap,_598){
if(_598){
this.pairs.unshift([name,_596,wrap]);
}else{
this.pairs.push([name,_596,wrap]);
}
},match:function(){
for(var i=0;i<this.pairs.length;i++){
var pair=this.pairs[i];
if(pair[1].apply(this,arguments)){
return pair[2].apply(this,arguments);
}
}
throw new Error("No match found");
},unregister:function(name){
for(var i=0;i<this.pairs.length;i++){
var pair=this.pairs[i];
if(pair[0]==name){
this.pairs.splice(i,1);
return true;
}
}
return false;
}});
dojo.provide("dojo.json");
dojo.require("dojo.lang.func");
dojo.require("dojo.string.extras");
dojo.require("dojo.AdapterRegistry");
dojo.json={jsonRegistry:new dojo.AdapterRegistry(),register:function(name,_59f,wrap,_5a1){
dojo.json.jsonRegistry.register(name,_59f,wrap,_5a1);
},evalJson:function(json){
try{
return eval("("+json+")");
}
catch(e){
dojo.debug(e);
return json;
}
},evalJSON:function(json){
dojo.deprecated("dojo.json.evalJSON","use dojo.json.evalJson","0.4");
return this.evalJson(json);
},serialize:function(o){
var _5a5=typeof (o);
if(_5a5=="undefined"){
return "undefined";
}else{
if((_5a5=="number")||(_5a5=="boolean")){
return o+"";
}else{
if(o===null){
return "null";
}
}
}
if(_5a5=="string"){
return dojo.string.escapeString(o);
}
var me=arguments.callee;
var _5a7;
if(typeof (o.__json__)=="function"){
_5a7=o.__json__();
if(o!==_5a7){
return me(_5a7);
}
}
if(typeof (o.json)=="function"){
_5a7=o.json();
if(o!==_5a7){
return me(_5a7);
}
}
if(_5a5!="function"&&typeof (o.length)=="number"){
var res=[];
for(var i=0;i<o.length;i++){
var val=me(o[i]);
if(typeof (val)!="string"){
val="undefined";
}
res.push(val);
}
return "["+res.join(",")+"]";
}
try{
window.o=o;
_5a7=dojo.json.jsonRegistry.match(o);
return me(_5a7);
}
catch(e){
}
if(_5a5=="function"){
return null;
}
res=[];
for(var k in o){
var _5ac;
if(typeof (k)=="number"){
_5ac="\""+k+"\"";
}else{
if(typeof (k)=="string"){
_5ac=dojo.string.escapeString(k);
}else{
continue;
}
}
val=me(o[k]);
if(typeof (val)!="string"){
continue;
}
res.push(_5ac+":"+val);
}
return "{"+res.join(",")+"}";
}};
dojo.provide("dojo.Deferred");
dojo.require("dojo.lang.func");
dojo.Deferred=function(_5ad){
this.chain=[];
this.id=this._nextId();
this.fired=-1;
this.paused=0;
this.results=[null,null];
this.canceller=_5ad;
this.silentlyCancelled=false;
};
dojo.lang.extend(dojo.Deferred,{getFunctionFromArgs:function(){
var a=arguments;
if((a[0])&&(!a[1])){
if(dojo.lang.isFunction(a[0])){
return a[0];
}else{
if(dojo.lang.isString(a[0])){
return dj_global[a[0]];
}
}
}else{
if((a[0])&&(a[1])){
return dojo.lang.hitch(a[0],a[1]);
}
}
return null;
},repr:function(){
var _5af;
if(this.fired==-1){
_5af="unfired";
}else{
if(this.fired==0){
_5af="success";
}else{
_5af="error";
}
}
return "Deferred("+this.id+", "+_5af+")";
},toString:dojo.lang.forward("repr"),_nextId:(function(){
var n=1;
return function(){
return n++;
};
})(),cancel:function(){
if(this.fired==-1){
if(this.canceller){
this.canceller(this);
}else{
this.silentlyCancelled=true;
}
if(this.fired==-1){
this.errback(new Error(this.repr()));
}
}else{
if((this.fired==0)&&(this.results[0] instanceof dojo.Deferred)){
this.results[0].cancel();
}
}
},_pause:function(){
this.paused++;
},_unpause:function(){
this.paused--;
if((this.paused==0)&&(this.fired>=0)){
this._fire();
}
},_continue:function(res){
this._resback(res);
this._unpause();
},_resback:function(res){
this.fired=((res instanceof Error)?1:0);
this.results[this.fired]=res;
this._fire();
},_check:function(){
if(this.fired!=-1){
if(!this.silentlyCancelled){
dojo.raise("already called!");
}
this.silentlyCancelled=false;
return;
}
},callback:function(res){
this._check();
this._resback(res);
},errback:function(res){
this._check();
if(!(res instanceof Error)){
res=new Error(res);
}
this._resback(res);
},addBoth:function(cb,cbfn){
var _5b7=this.getFunctionFromArgs(cb,cbfn);
if(arguments.length>2){
_5b7=dojo.lang.curryArguments(null,_5b7,arguments,2);
}
return this.addCallbacks(_5b7,_5b7);
},addCallback:function(cb,cbfn){
var _5ba=this.getFunctionFromArgs(cb,cbfn);
if(arguments.length>2){
_5ba=dojo.lang.curryArguments(null,_5ba,arguments,2);
}
return this.addCallbacks(_5ba,null);
},addErrback:function(cb,cbfn){
var _5bd=this.getFunctionFromArgs(cb,cbfn);
if(arguments.length>2){
_5bd=dojo.lang.curryArguments(null,_5bd,arguments,2);
}
return this.addCallbacks(null,_5bd);
return this.addCallbacks(null,fn);
},addCallbacks:function(cb,eb){
this.chain.push([cb,eb]);
if(this.fired>=0){
this._fire();
}
return this;
},_fire:function(){
var _5c0=this.chain;
var _5c1=this.fired;
var res=this.results[_5c1];
var self=this;
var cb=null;
while(_5c0.length>0&&this.paused==0){
var pair=_5c0.shift();
var f=pair[_5c1];
if(f==null){
continue;
}
try{
res=f(res);
_5c1=((res instanceof Error)?1:0);
if(res instanceof dojo.Deferred){
cb=function(res){
self._continue(res);
};
this._pause();
}
}
catch(err){
_5c1=1;
res=err;
}
}
this.fired=_5c1;
this.results[_5c1]=res;
if((cb)&&(this.paused)){
res.addBoth(cb);
}
}});
dojo.provide("dojo.rpc.Deferred");
dojo.require("dojo.Deferred");
dojo.rpc.Deferred=dojo.Deferred;
dojo.rpc.Deferred.prototype=dojo.Deferred.prototype;
dojo.provide("dojo.rpc.RpcService");
dojo.require("dojo.io.*");
dojo.require("dojo.json");
dojo.require("dojo.lang.func");
dojo.require("dojo.rpc.Deferred");
dojo.rpc.RpcService=function(url){
if(url){
this.connect(url);
}
};
dojo.lang.extend(dojo.rpc.RpcService,{strictArgChecks:true,serviceUrl:"",parseResults:function(obj){
return obj;
},errorCallback:function(_5ca){
return function(type,obj,e){
_5ca.errback(e);
};
},resultCallback:function(_5ce){
var tf=dojo.lang.hitch(this,function(type,obj,e){
var _5d3=this.parseResults(obj||e);
_5ce.callback(_5d3);
});
return tf;
},generateMethod:function(_5d4,_5d5,url){
return dojo.lang.hitch(this,function(){
var _5d7=new dojo.rpc.Deferred();
if((this.strictArgChecks)&&(_5d5!=null)&&(arguments.length!=_5d5.length)){
dojo.raise("Invalid number of parameters for remote method.");
}else{
this.bind(_5d4,arguments,_5d7,url);
}
return _5d7;
});
},processSmd:function(_5d8){
dojo.debug("RpcService: Processing returned SMD.");
if(_5d8.methods){
dojo.lang.forEach(_5d8.methods,function(m){
if(m&&m["name"]){
dojo.debug("RpcService: Creating Method: this.",m.name,"()");
this[m.name]=this.generateMethod(m.name,m.parameters,m["url"]||m["serviceUrl"]||m["serviceURL"]);
if(dojo.lang.isFunction(this[m.name])){
dojo.debug("RpcService: Successfully created",m.name,"()");
}else{
dojo.debug("RpcService: Failed to create",m.name,"()");
}
}
},this);
}
this.serviceUrl=_5d8.serviceUrl||_5d8.serviceURL;
dojo.debug("RpcService: Dojo RpcService is ready for use.");
},connect:function(_5da){
dojo.debug("RpcService: Attempting to load SMD document from:",_5da);
dojo.io.bind({url:_5da,mimetype:"text/json",load:dojo.lang.hitch(this,function(type,_5dc,e){
return this.processSmd(_5dc);
}),sync:true});
}});
dojo.provide("dojo.rpc.JsonService");
dojo.require("dojo.rpc.RpcService");
dojo.require("dojo.io.*");
dojo.require("dojo.json");
dojo.require("dojo.lang");
dojo.rpc.JsonService=function(args){
if(args){
if(dojo.lang.isString(args)){
this.connect(args);
}else{
if(args["smdUrl"]){
this.connect(args.smdUrl);
}
if(args["smdStr"]){
this.processSmd(dj_eval("("+args.smdStr+")"));
}
if(args["smdObj"]){
this.processSmd(args.smdObj);
}
if(args["serviceUrl"]){
this.serviceUrl=args.serviceUrl;
}
if(typeof args["strictArgChecks"]!="undefined"){
this.strictArgChecks=args.strictArgChecks;
}
}
}
};
dojo.inherits(dojo.rpc.JsonService,dojo.rpc.RpcService);
dojo.lang.extend(dojo.rpc.JsonService,{bustCache:false,contentType:"application/json-rpc",lastSubmissionId:0,callRemote:function(_5df,_5e0){
var _5e1=new dojo.rpc.Deferred();
this.bind(_5df,_5e0,_5e1);
return _5e1;
},bind:function(_5e2,_5e3,_5e4,url){
dojo.io.bind({url:url||this.serviceUrl,postContent:this.createRequest(_5e2,_5e3),method:"POST",contentType:this.contentType,mimetype:"text/json",load:this.resultCallback(_5e4),preventCache:this.bustCache});
},createRequest:function(_5e6,_5e7){
var req={"params":_5e7,"method":_5e6,"id":++this.lastSubmissionId};
var data=dojo.json.serialize(req);
dojo.debug("JsonService: JSON-RPC Request: "+data);
return data;
},parseResults:function(obj){
if(!obj){
return;
}
if(obj["Result"]||obj["result"]){
return obj["result"]||obj["Result"];
}else{
if(obj["ResultSet"]){
return obj["ResultSet"];
}else{
return obj;
}
}
}});
dojo.kwCompoundRequire({common:["dojo.rpc.JsonService",false,false]});
dojo.provide("dojo.rpc.*");
dojo.provide("dojo.xml.Parse");
dojo.require("dojo.dom");
dojo.xml.Parse=function(){
function getDojoTagName(node){
var _5ec=node.tagName;
if(_5ec.substr(0,5).toLowerCase()!="dojo:"){
if(_5ec.substr(0,4).toLowerCase()=="dojo"){
return "dojo:"+_5ec.substring(4).toLowerCase();
}
var djt=node.getAttribute("dojoType")||node.getAttribute("dojotype");
if(djt){
return "dojo:"+djt.toLowerCase();
}
if(node.getAttributeNS&&node.getAttributeNS(dojo.dom.dojoml,"type")){
return "dojo:"+node.getAttributeNS(dojo.dom.dojoml,"type").toLowerCase();
}
try{
djt=node.getAttribute("dojo:type");
}
catch(e){
}
if(djt){
return "dojo:"+djt.toLowerCase();
}
if(!dj_global["djConfig"]||!djConfig["ignoreClassNames"]){
var _5ee=node.className||node.getAttribute("class");
if(_5ee&&_5ee.indexOf&&_5ee.indexOf("dojo-")!=-1){
var _5ef=_5ee.split(" ");
for(var x=0;x<_5ef.length;x++){
if(_5ef[x].length>5&&_5ef[x].indexOf("dojo-")>=0){
return "dojo:"+_5ef[x].substr(5).toLowerCase();
}
}
}
}
}
return _5ec.toLowerCase();
}
this.parseElement=function(node,_5f2,_5f3,_5f4){
if(node.getAttribute("parseWidgets")=="false"){
return {};
}
var _5f5={};
var _5f6=getDojoTagName(node);
_5f5[_5f6]=[];
if((!_5f3)||(_5f6.substr(0,4).toLowerCase()=="dojo")){
var _5f7=parseAttributes(node);
for(var attr in _5f7){
if((!_5f5[_5f6][attr])||(typeof _5f5[_5f6][attr]!="array")){
_5f5[_5f6][attr]=[];
}
_5f5[_5f6][attr].push(_5f7[attr]);
}
_5f5[_5f6].nodeRef=node;
_5f5.tagName=_5f6;
_5f5.index=_5f4||0;
}
var _5f9=0;
var tcn,i=0,nodes=node.childNodes;
while(tcn=nodes[i++]){
switch(tcn.nodeType){
case dojo.dom.ELEMENT_NODE:
_5f9++;
var ctn=getDojoTagName(tcn);
if(!_5f5[ctn]){
_5f5[ctn]=[];
}
_5f5[ctn].push(this.parseElement(tcn,true,_5f3,_5f9));
if((tcn.childNodes.length==1)&&(tcn.childNodes.item(0).nodeType==dojo.dom.TEXT_NODE)){
_5f5[ctn][_5f5[ctn].length-1].value=tcn.childNodes.item(0).nodeValue;
}
break;
case dojo.dom.TEXT_NODE:
if(node.childNodes.length==1){
_5f5[_5f6].push({value:node.childNodes.item(0).nodeValue});
}
break;
default:
break;
}
}
return _5f5;
};
function parseAttributes(node){
var _5fd={};
var atts=node.attributes;
var _5ff,i=0;
while(_5ff=atts[i++]){
if((dojo.render.html.capable)&&(dojo.render.html.ie)){
if(!_5ff){
continue;
}
if((typeof _5ff=="object")&&(typeof _5ff.nodeValue=="undefined")||(_5ff.nodeValue==null)||(_5ff.nodeValue=="")){
continue;
}
}
var nn=(_5ff.nodeName.indexOf("dojo:")==-1)?_5ff.nodeName:_5ff.nodeName.split("dojo:")[1];
_5fd[nn]={value:_5ff.nodeValue};
}
return _5fd;
}
};
dojo.provide("dojo.xml.domUtil");
dojo.require("dojo.graphics.color");
dojo.require("dojo.dom");
dojo.require("dojo.style");
dojo.deprecated("dojo.xml.domUtil is deprecated, use dojo.dom instead");
dojo.xml.domUtil=new function(){
this.nodeTypes={ELEMENT_NODE:1,ATTRIBUTE_NODE:2,TEXT_NODE:3,CDATA_SECTION_NODE:4,ENTITY_REFERENCE_NODE:5,ENTITY_NODE:6,PROCESSING_INSTRUCTION_NODE:7,COMMENT_NODE:8,DOCUMENT_NODE:9,DOCUMENT_TYPE_NODE:10,DOCUMENT_FRAGMENT_NODE:11,NOTATION_NODE:12};
this.dojoml="http://www.dojotoolkit.org/2004/dojoml";
this.idIncrement=0;
this.getTagName=function(){
return dojo.dom.getTagName.apply(dojo.dom,arguments);
};
this.getUniqueId=function(){
return dojo.dom.getUniqueId.apply(dojo.dom,arguments);
};
this.getFirstChildTag=function(){
return dojo.dom.getFirstChildElement.apply(dojo.dom,arguments);
};
this.getLastChildTag=function(){
return dojo.dom.getLastChildElement.apply(dojo.dom,arguments);
};
this.getNextSiblingTag=function(){
return dojo.dom.getNextSiblingElement.apply(dojo.dom,arguments);
};
this.getPreviousSiblingTag=function(){
return dojo.dom.getPreviousSiblingElement.apply(dojo.dom,arguments);
};
this.forEachChildTag=function(node,_602){
var _603=this.getFirstChildTag(node);
while(_603){
if(_602(_603)=="break"){
break;
}
_603=this.getNextSiblingTag(_603);
}
};
this.moveChildren=function(){
return dojo.dom.moveChildren.apply(dojo.dom,arguments);
};
this.copyChildren=function(){
return dojo.dom.copyChildren.apply(dojo.dom,arguments);
};
this.clearChildren=function(){
return dojo.dom.removeChildren.apply(dojo.dom,arguments);
};
this.replaceChildren=function(){
return dojo.dom.replaceChildren.apply(dojo.dom,arguments);
};
this.getStyle=function(){
return dojo.style.getStyle.apply(dojo.style,arguments);
};
this.toCamelCase=function(){
return dojo.style.toCamelCase.apply(dojo.style,arguments);
};
this.toSelectorCase=function(){
return dojo.style.toSelectorCase.apply(dojo.style,arguments);
};
this.getAncestors=function(){
return dojo.dom.getAncestors.apply(dojo.dom,arguments);
};
this.isChildOf=function(){
return dojo.dom.isDescendantOf.apply(dojo.dom,arguments);
};
this.createDocumentFromText=function(){
return dojo.dom.createDocumentFromText.apply(dojo.dom,arguments);
};
if(dojo.render.html.capable||dojo.render.svg.capable){
this.createNodesFromText=function(txt,wrap){
return dojo.dom.createNodesFromText.apply(dojo.dom,arguments);
};
}
this.extractRGB=function(_606){
return dojo.graphics.color.extractRGB(_606);
};
this.hex2rgb=function(hex){
return dojo.graphics.color.hex2rgb(hex);
};
this.rgb2hex=function(r,g,b){
return dojo.graphics.color.rgb2hex(r,g,b);
};
this.insertBefore=function(){
return dojo.dom.insertBefore.apply(dojo.dom,arguments);
};
this.before=this.insertBefore;
this.insertAfter=function(){
return dojo.dom.insertAfter.apply(dojo.dom,arguments);
};
this.after=this.insertAfter;
this.insert=function(){
return dojo.dom.insertAtPosition.apply(dojo.dom,arguments);
};
this.insertAtIndex=function(){
return dojo.dom.insertAtIndex.apply(dojo.dom,arguments);
};
this.textContent=function(){
return dojo.dom.textContent.apply(dojo.dom,arguments);
};
this.renderedTextContent=function(){
return dojo.dom.renderedTextContent.apply(dojo.dom,arguments);
};
this.remove=function(node){
return dojo.dom.removeNode.apply(dojo.dom,arguments);
};
};
dojo.provide("dojo.xml.htmlUtil");
dojo.require("dojo.html");
dojo.require("dojo.style");
dojo.require("dojo.dom");
dojo.deprecated("dojo.xml.htmlUtil is deprecated, use dojo.html instead");
dojo.xml.htmlUtil=new function(){
this.styleSheet=dojo.style.styleSheet;
this._clobberSelection=function(){
return dojo.html.clearSelection.apply(dojo.html,arguments);
};
this.disableSelect=function(){
return dojo.html.disableSelection.apply(dojo.html,arguments);
};
this.enableSelect=function(){
return dojo.html.enableSelection.apply(dojo.html,arguments);
};
this.getInnerWidth=function(){
return dojo.style.getInnerWidth.apply(dojo.style,arguments);
};
this.getOuterWidth=function(node){
dojo.unimplemented("dojo.xml.htmlUtil.getOuterWidth");
};
this.getInnerHeight=function(){
return dojo.style.getInnerHeight.apply(dojo.style,arguments);
};
this.getOuterHeight=function(node){
dojo.unimplemented("dojo.xml.htmlUtil.getOuterHeight");
};
this.getTotalOffset=function(){
return dojo.style.getTotalOffset.apply(dojo.style,arguments);
};
this.totalOffsetLeft=function(){
return dojo.style.totalOffsetLeft.apply(dojo.style,arguments);
};
this.getAbsoluteX=this.totalOffsetLeft;
this.totalOffsetTop=function(){
return dojo.style.totalOffsetTop.apply(dojo.style,arguments);
};
this.getAbsoluteY=this.totalOffsetTop;
this.getEventTarget=function(){
return dojo.html.getEventTarget.apply(dojo.html,arguments);
};
this.getScrollTop=function(){
return dojo.html.getScrollTop.apply(dojo.html,arguments);
};
this.getScrollLeft=function(){
return dojo.html.getScrollLeft.apply(dojo.html,arguments);
};
this.evtTgt=this.getEventTarget;
this.getParentOfType=function(){
return dojo.html.getParentOfType.apply(dojo.html,arguments);
};
this.getAttribute=function(){
return dojo.html.getAttribute.apply(dojo.html,arguments);
};
this.getAttr=function(node,attr){
dojo.deprecated("dojo.xml.htmlUtil.getAttr is deprecated, use dojo.xml.htmlUtil.getAttribute instead");
return dojo.xml.htmlUtil.getAttribute(node,attr);
};
this.hasAttribute=function(){
return dojo.html.hasAttribute.apply(dojo.html,arguments);
};
this.hasAttr=function(node,attr){
dojo.deprecated("dojo.xml.htmlUtil.hasAttr is deprecated, use dojo.xml.htmlUtil.hasAttribute instead");
return dojo.xml.htmlUtil.hasAttribute(node,attr);
};
this.getClass=function(){
return dojo.html.getClass.apply(dojo.html,arguments);
};
this.hasClass=function(){
return dojo.html.hasClass.apply(dojo.html,arguments);
};
this.prependClass=function(){
return dojo.html.prependClass.apply(dojo.html,arguments);
};
this.addClass=function(){
return dojo.html.addClass.apply(dojo.html,arguments);
};
this.setClass=function(){
return dojo.html.setClass.apply(dojo.html,arguments);
};
this.removeClass=function(){
return dojo.html.removeClass.apply(dojo.html,arguments);
};
this.classMatchType={ContainsAll:0,ContainsAny:1,IsOnly:2};
this.getElementsByClass=function(){
return dojo.html.getElementsByClass.apply(dojo.html,arguments);
};
this.getElementsByClassName=this.getElementsByClass;
this.setOpacity=function(){
return dojo.style.setOpacity.apply(dojo.style,arguments);
};
this.getOpacity=function(){
return dojo.style.getOpacity.apply(dojo.style,arguments);
};
this.clearOpacity=function(){
return dojo.style.clearOpacity.apply(dojo.style,arguments);
};
this.gravity=function(){
return dojo.html.gravity.apply(dojo.html,arguments);
};
this.gravity.NORTH=1;
this.gravity.SOUTH=1<<1;
this.gravity.EAST=1<<2;
this.gravity.WEST=1<<3;
this.overElement=function(){
return dojo.html.overElement.apply(dojo.html,arguments);
};
this.insertCssRule=function(){
return dojo.style.insertCssRule.apply(dojo.style,arguments);
};
this.insertCSSRule=function(_612,_613,_614){
dojo.deprecated("dojo.xml.htmlUtil.insertCSSRule is deprecated, use dojo.xml.htmlUtil.insertCssRule instead");
return dojo.xml.htmlUtil.insertCssRule(_612,_613,_614);
};
this.removeCssRule=function(){
return dojo.style.removeCssRule.apply(dojo.style,arguments);
};
this.removeCSSRule=function(_615){
dojo.deprecated("dojo.xml.htmlUtil.removeCSSRule is deprecated, use dojo.xml.htmlUtil.removeCssRule instead");
return dojo.xml.htmlUtil.removeCssRule(_615);
};
this.insertCssFile=function(){
return dojo.style.insertCssFile.apply(dojo.style,arguments);
};
this.insertCSSFile=function(URI,doc,_618){
dojo.deprecated("dojo.xml.htmlUtil.insertCSSFile is deprecated, use dojo.xml.htmlUtil.insertCssFile instead");
return dojo.xml.htmlUtil.insertCssFile(URI,doc,_618);
};
this.getBackgroundColor=function(){
return dojo.style.getBackgroundColor.apply(dojo.style,arguments);
};
this.getUniqueId=function(){
return dojo.dom.getUniqueId();
};
this.getStyle=function(){
return dojo.style.getStyle.apply(dojo.style,arguments);
};
};
dojo.require("dojo.xml.Parse");
dojo.kwCompoundRequire({common:["dojo.xml.domUtil"],browser:["dojo.xml.htmlUtil"],dashboard:["dojo.xml.htmlUtil"],svg:["dojo.xml.svgUtil"]});
dojo.provide("dojo.xml.*");
dojo.provide("dojo.lang.type");
dojo.require("dojo.lang.common");
dojo.lang.whatAmI=function(wh){
try{
if(dojo.lang.isArray(wh)){
return "array";
}
if(dojo.lang.isFunction(wh)){
return "function";
}
if(dojo.lang.isString(wh)){
return "string";
}
if(dojo.lang.isNumber(wh)){
return "number";
}
if(dojo.lang.isBoolean(wh)){
return "boolean";
}
if(dojo.lang.isAlien(wh)){
return "alien";
}
if(dojo.lang.isUndefined(wh)){
return "undefined";
}
for(var name in dojo.lang.whatAmI.custom){
if(dojo.lang.whatAmI.custom[name](wh)){
return name;
}
}
if(dojo.lang.isObject(wh)){
return "object";
}
}
catch(E){
}
return "unknown";
};
dojo.lang.whatAmI.custom={};
dojo.lang.isNumeric=function(wh){
return (!isNaN(wh)&&isFinite(wh)&&(wh!=null)&&!dojo.lang.isBoolean(wh)&&!dojo.lang.isArray(wh));
};
dojo.lang.isBuiltIn=function(wh){
return (dojo.lang.isArray(wh)||dojo.lang.isFunction(wh)||dojo.lang.isString(wh)||dojo.lang.isNumber(wh)||dojo.lang.isBoolean(wh)||(wh==null)||(wh instanceof Error)||(typeof wh=="error"));
};
dojo.lang.isPureObject=function(wh){
return ((wh!=null)&&dojo.lang.isObject(wh)&&wh.constructor==Object);
};
dojo.lang.isOfType=function(_61e,type){
if(dojo.lang.isArray(type)){
var _620=type;
for(var i in _620){
var _622=_620[i];
if(dojo.lang.isOfType(_61e,_622)){
return true;
}
}
return false;
}else{
if(dojo.lang.isString(type)){
type=type.toLowerCase();
}
switch(type){
case Array:
case "array":
return dojo.lang.isArray(_61e);
break;
case Function:
case "function":
return dojo.lang.isFunction(_61e);
break;
case String:
case "string":
return dojo.lang.isString(_61e);
break;
case Number:
case "number":
return dojo.lang.isNumber(_61e);
break;
case "numeric":
return dojo.lang.isNumeric(_61e);
break;
case Boolean:
case "boolean":
return dojo.lang.isBoolean(_61e);
break;
case Object:
case "object":
return dojo.lang.isObject(_61e);
break;
case "pureobject":
return dojo.lang.isPureObject(_61e);
break;
case "builtin":
return dojo.lang.isBuiltIn(_61e);
break;
case "alien":
return dojo.lang.isAlien(_61e);
break;
case "undefined":
return dojo.lang.isUndefined(_61e);
break;
case null:
case "null":
return (_61e===null);
break;
case "optional":
return ((_61e===null)||dojo.lang.isUndefined(_61e));
break;
default:
if(dojo.lang.isFunction(type)){
return (_61e instanceof type);
}else{
dojo.raise("dojo.lang.isOfType() was passed an invalid type");
}
break;
}
}
dojo.raise("If we get here, it means a bug was introduced above.");
};
dojo.lang.getObject=function(str){
var _624=str.split("."),i=0,obj=dj_global;
do{
obj=obj[_624[i++]];
}while(i<_624.length&&obj);
return (obj!=dj_global)?obj:null;
};
dojo.lang.doesObjectExist=function(str){
var _626=str.split("."),i=0,obj=dj_global;
do{
obj=obj[_626[i++]];
}while(i<_626.length&&obj);
return (obj&&obj!=dj_global);
};
dojo.provide("dojo.lang.assert");
dojo.require("dojo.lang.common");
dojo.require("dojo.lang.array");
dojo.require("dojo.lang.type");
dojo.lang.assert=function(_627,_628){
if(!_627){
var _629="An assert statement failed.\n"+"The method dojo.lang.assert() was called with a 'false' value.\n";
if(_628){
_629+="Here's the assert message:\n"+_628+"\n";
}
throw new Error(_629);
}
};
dojo.lang.assertType=function(_62a,type,_62c){
if(!dojo.lang.isOfType(_62a,type)){
if(!_62c){
if(!dojo.lang.assertType._errorMessage){
dojo.lang.assertType._errorMessage="Type mismatch: dojo.lang.assertType() failed.";
}
_62c=dojo.lang.assertType._errorMessage;
}
dojo.lang.assert(false,_62c);
}
};
dojo.lang.assertValidKeywords=function(_62d,_62e,_62f){
var key;
if(!_62f){
if(!dojo.lang.assertValidKeywords._errorMessage){
dojo.lang.assertValidKeywords._errorMessage="In dojo.lang.assertValidKeywords(), found invalid keyword:";
}
_62f=dojo.lang.assertValidKeywords._errorMessage;
}
if(dojo.lang.isArray(_62e)){
for(key in _62d){
if(!dojo.lang.inArray(_62e,key)){
dojo.lang.assert(false,_62f+" "+key);
}
}
}else{
for(key in _62d){
if(!(key in _62e)){
dojo.lang.assert(false,_62f+" "+key);
}
}
}
};
dojo.provide("dojo.lang.repr");
dojo.require("dojo.lang.common");
dojo.require("dojo.AdapterRegistry");
dojo.require("dojo.string.extras");
dojo.lang.reprRegistry=new dojo.AdapterRegistry();
dojo.lang.registerRepr=function(name,_632,wrap,_634){
dojo.lang.reprRegistry.register(name,_632,wrap,_634);
};
dojo.lang.repr=function(obj){
if(typeof (obj)=="undefined"){
return "undefined";
}else{
if(obj===null){
return "null";
}
}
try{
if(typeof (obj["__repr__"])=="function"){
return obj["__repr__"]();
}else{
if((typeof (obj["repr"])=="function")&&(obj.repr!=arguments.callee)){
return obj["repr"]();
}
}
return dojo.lang.reprRegistry.match(obj);
}
catch(e){
if(typeof (obj.NAME)=="string"&&(obj.toString==Function.prototype.toString||obj.toString==Object.prototype.toString)){
return o.NAME;
}
}
if(typeof (obj)=="function"){
obj=(obj+"").replace(/^\s+/,"");
var idx=obj.indexOf("{");
if(idx!=-1){
obj=obj.substr(0,idx)+"{...}";
}
}
return obj+"";
};
dojo.lang.reprArrayLike=function(arr){
try{
var na=dojo.lang.map(arr,dojo.lang.repr);
return "["+na.join(", ")+"]";
}
catch(e){
}
};
dojo.lang.reprString=function(str){
dojo.deprecated("dojo.lang.reprNumber","use `String(num)` instead","0.4");
return dojo.string.escapeString(str);
};
dojo.lang.reprNumber=function(num){
dojo.deprecated("dojo.lang.reprNumber","use `String(num)` instead","0.4");
return num+"";
};
(function(){
var m=dojo.lang;
m.registerRepr("arrayLike",m.isArrayLike,m.reprArrayLike);
m.registerRepr("string",m.isString,m.reprString);
m.registerRepr("numbers",m.isNumber,m.reprNumber);
m.registerRepr("boolean",m.isBoolean,m.reprNumber);
})();
dojo.provide("dojo.lang.declare");
dojo.require("dojo.lang.common");
dojo.require("dojo.lang.extras");
dojo.lang.declare=function(_63c,_63d,_63e,init){
var ctor=function(){
var self=this._getPropContext();
var s=self.constructor.superclass;
if((s)&&(s.constructor)){
if(s.constructor==arguments.callee){
this.inherited("constructor",arguments);
}else{
this._inherited(s,"constructor",arguments);
}
}
if((!this.prototyping)&&(self.initializer)){
self.initializer.apply(this,arguments);
}
};
var scp=(_63d?_63d.prototype:null);
if(scp){
scp.prototyping=true;
ctor.prototype=new _63d();
scp.prototyping=false;
}
ctor.prototype.constructor=ctor;
ctor.superclass=scp;
dojo.lang.extend(ctor,dojo.lang.declare.base);
_63e=(_63e||{});
_63e.initializer=(_63e.initializer)||(init)||(function(){
});
_63e.className=_63c;
dojo.lang.extend(ctor,_63e);
dojo.lang.setObjPathValue(_63c,ctor,null,true);
};
dojo.lang.declare.base={_getPropContext:function(){
return (this.___proto||this);
},_inherited:function(_644,_645,args){
var _647=this.___proto;
this.___proto=_644;
var _648=_644[_645].apply(this,(args||[]));
this.___proto=_647;
return _648;
},inherited:function(prop,args){
var p=this._getPropContext();
do{
if((!p.constructor)||(!p.constructor.superclass)){
return;
}
p=p.constructor.superclass;
}while(!(prop in p));
return (typeof p[prop]=="function"?this._inherited(p,prop,args):p[prop]);
}};
dojo.declare=dojo.lang.declare;
dojo.kwCompoundRequire({common:["dojo.lang","dojo.lang.common","dojo.lang.assert","dojo.lang.array","dojo.lang.type","dojo.lang.func","dojo.lang.extras","dojo.lang.repr","dojo.lang.declare"]});
dojo.provide("dojo.lang.*");
dojo.provide("dojo.storage");
dojo.provide("dojo.storage.StorageProvider");
dojo.require("dojo.lang.*");
dojo.require("dojo.event.*");
dojo.storage=function(){
};
dojo.lang.extend(dojo.storage,{SUCCESS:"success",FAILED:"failed",PENDING:"pending",SIZE_NOT_AVAILABLE:"Size not available",SIZE_NO_LIMIT:"No size limit",namespace:"dojoStorage",onHideSettingsUI:null,initialize:function(){
dojo.unimplemented("dojo.storage.initialize");
},isAvailable:function(){
dojo.unimplemented("dojo.storage.isAvailable");
},put:function(key,_64d,_64e){
dojo.unimplemented("dojo.storage.put");
},get:function(key){
dojo.unimplemented("dojo.storage.get");
},hasKey:function(key){
if(this.get(key)!=null){
return true;
}else{
return false;
}
},getKeys:function(){
dojo.unimplemented("dojo.storage.getKeys");
},clear:function(){
dojo.unimplemented("dojo.storage.clear");
},remove:function(key){
dojo.unimplemented("dojo.storage.remove");
},isPermanent:function(){
dojo.unimplemented("dojo.storage.isPermanent");
},getMaximumSize:function(){
dojo.unimplemented("dojo.storage.getMaximumSize");
},hasSettingsUI:function(){
return false;
},showSettingsUI:function(){
dojo.unimplemented("dojo.storage.showSettingsUI");
},hideSettingsUI:function(){
dojo.unimplemented("dojo.storage.hideSettingsUI");
},getType:function(){
dojo.unimplemented("dojo.storage.getType");
},isValidKey:function(_652){
if(_652==null||typeof _652=="undefined"){
return false;
}
return /^[0-9A-Za-z_]*$/.test(_652);
}});
dojo.storage.manager=new function(){
this.currentProvider=null;
this.available=false;
this.initialized=false;
this.providers=new Array();
this.namespace="dojo.storage";
this.initialize=function(){
this.autodetect();
};
this.register=function(name,_654){
this.providers[this.providers.length]=_654;
this.providers[name]=_654;
};
this.setProvider=function(_655){
};
this.autodetect=function(){
if(this.initialized==true){
return;
}
var _656=null;
for(var i=0;i<this.providers.length;i++){
_656=this.providers[i];
if(_656.isAvailable()){
break;
}
}
if(_656==null){
this.initialized=true;
this.available=false;
this.currentProvider=null;
dojo.raise("No storage provider found for this platform");
}
this.currentProvider=_656;
for(var i in _656){
dojo.storage[i]=_656[i];
}
dojo.storage.manager=this;
dojo.storage.initialize();
this.initialized=true;
this.available=true;
};
this.isAvailable=function(){
return this.available;
};
this.isInitialized=function(){
if(dojo.flash.ready==false){
return false;
}else{
return this.initialized;
}
};
this.supportsProvider=function(_658){
try{
var _659=eval("new "+_658+"()");
var _65a=_659.isAvailable();
if(_65a==null||typeof _65a=="undefined"){
return false;
}
return _65a;
}
catch(exception){
dojo.debug("exception="+exception);
return false;
}
};
this.getProvider=function(){
return this.currentProvider;
};
this.loaded=function(){
};
};
dojo.provide("dojo.flash");
dojo.require("dojo.string.*");
dojo.require("dojo.uri.*");
dojo.flash={flash6_version:null,flash8_version:null,ready:false,_visible:true,_loadedListeners:new Array(),_installingListeners:new Array(),setSwf:function(_65b){
if(_65b==null||dojo.lang.isUndefined(_65b)){
return;
}
if(_65b.flash6!=null&&!dojo.lang.isUndefined(_65b.flash6)){
this.flash6_version=_65b.flash6;
}
if(_65b.flash8!=null&&!dojo.lang.isUndefined(_65b.flash8)){
this.flash8_version=_65b.flash8;
}
if(!dojo.lang.isUndefined(_65b.visible)){
this._visible=_65b.visible;
}
this._initialize();
},useFlash6:function(){
if(this.flash6_version==null){
return false;
}else{
if(this.flash6_version!=null&&dojo.flash.info.commVersion==6){
return true;
}else{
return false;
}
}
},useFlash8:function(){
if(this.flash8_version==null){
return false;
}else{
if(this.flash8_version!=null&&dojo.flash.info.commVersion==8){
return true;
}else{
return false;
}
}
},addLoadedListener:function(_65c){
this._loadedListeners.push(_65c);
},addInstallingListener:function(_65d){
this._installingListeners.push(_65d);
},loaded:function(){
dojo.flash.ready=true;
if(dojo.flash._loadedListeners.length>0){
for(var i=0;i<dojo.flash._loadedListeners.length;i++){
dojo.flash._loadedListeners[i].call(null);
}
}
},installing:function(){
if(dojo.flash._installingListeners.length>0){
for(var i=0;i<dojo.flash._installingListeners.length;i++){
dojo.flash._installingListeners[i].call(null);
}
}
},_initialize:function(){
var _660=new dojo.flash.Install();
dojo.flash.installer=_660;
if(_660.needed()==true){
_660.install();
}else{
dojo.flash.obj=new dojo.flash.Embed(this._visible);
dojo.flash.obj.write(dojo.flash.info.commVersion);
dojo.flash.comm=new dojo.flash.Communicator();
}
}};
dojo.flash.Info=function(){
if(dojo.render.html.ie){
document.writeln("<script language=\"VBScript\" type=\"text/vbscript\">");
document.writeln("Function VBGetSwfVer(i)");
document.writeln("  on error resume next");
document.writeln("  Dim swControl, swVersion");
document.writeln("  swVersion = 0");
document.writeln("  set swControl = CreateObject(\"ShockwaveFlash.ShockwaveFlash.\" + CStr(i))");
document.writeln("  if (IsObject(swControl)) then");
document.writeln("    swVersion = swControl.GetVariable(\"$version\")");
document.writeln("  end if");
document.writeln("  VBGetSwfVer = swVersion");
document.writeln("End Function");
document.writeln("</script>");
}
this._detectVersion();
this._detectCommunicationVersion();
};
dojo.flash.Info.prototype={version:-1,versionMajor:-1,versionMinor:-1,versionRevision:-1,capable:false,commVersion:6,installing:false,isVersionOrAbove:function(_661,_662,_663){
_663=parseFloat("."+_663);
if(this.versionMajor>=_661&&this.versionMinor>=_662&&this.versionRevision>=_663){
return true;
}else{
return false;
}
},_detectVersion:function(){
var _664;
for(var _665=25;_665>0;_665--){
if(dojo.render.html.ie){
_664=VBGetSwfVer(_665);
}else{
_664=this._JSFlashInfo(_665);
}
if(_664==-1){
this.capable=false;
return;
}else{
if(_664!=0){
var _666;
if(dojo.render.html.ie){
var _667=_664.split(" ");
var _668=_667[1];
_666=_668.split(",");
}else{
_666=_664.split(".");
}
this.versionMajor=_666[0];
this.versionMinor=_666[1];
this.versionRevision=_666[2];
versionString=this.versionMajor+"."+this.versionRevision;
this.version=parseFloat(versionString);
this.capable=true;
break;
}
}
}
},_JSFlashInfo:function(_669){
if(navigator.plugins!=null&&navigator.plugins.length>0){
if(navigator.plugins["Shockwave Flash 2.0"]||navigator.plugins["Shockwave Flash"]){
var _66a=navigator.plugins["Shockwave Flash 2.0"]?" 2.0":"";
var _66b=navigator.plugins["Shockwave Flash"+_66a].description;
var _66c=_66b.split(" ");
var _66d=_66c[2].split(".");
var _66e=_66d[0];
var _66f=_66d[1];
if(_66c[3]!=""){
tempArrayMinor=_66c[3].split("r");
}else{
tempArrayMinor=_66c[4].split("r");
}
var _670=tempArrayMinor[1]>0?tempArrayMinor[1]:0;
var _671=_66e+"."+_66f+"."+_670;
return _671;
}
}
return -1;
},_detectCommunicationVersion:function(){
if(this.capable==false){
this.commVersion=null;
return;
}
if(typeof djConfig["forceFlashComm"]!="undefined"&&typeof djConfig["forceFlashComm"]!=null){
this.commVersion=djConfig["forceFlashComm"];
return;
}
if(dojo.render.html.safari==true||dojo.render.html.opera==true){
this.commVersion=8;
}else{
this.commVersion=6;
}
}};
dojo.flash.Embed=function(_672){
this._visible=_672;
};
dojo.flash.Embed.prototype={width:215,height:138,id:"flashObject",_visible:true,write:function(_673,_674){
if(dojo.lang.isUndefined(_674)){
_674=false;
}
var _675=new dojo.string.Builder();
_675.append("width: "+this.width+"px; ");
_675.append("height: "+this.height+"px; ");
if(this._visible==false){
_675.append("position: absolute; ");
_675.append("z-index: 10000; ");
_675.append("top: -1000px; ");
_675.append("left: -1000px; ");
}
_675=_675.toString();
var _676;
var _677;
if(_673==6){
_677=dojo.flash.flash6_version;
var _678=djConfig.baseRelativePath;
_677=_677+"?baseRelativePath="+escape(_678);
_676="<embed id=\""+this.id+"\" src=\""+_677+"\" "+"    quality=\"high\" bgcolor=\"#ffffff\" "+"    width=\""+this.width+"\" height=\""+this.height+"\" "+"    name=\""+this.id+"\" "+"    align=\"middle\" allowScriptAccess=\"sameDomain\" "+"    type=\"application/x-shockwave-flash\" swLiveConnect=\"true\" "+"    pluginspage=\"http://www.macromedia.com/go/getflashplayer\">";
}else{
_677=dojo.flash.flash8_version;
var _679=_677,swflocEmbed=_677;
if(_674){
var _67a=escape(window.location);
document.title=document.title.slice(0,47)+" - Flash Player Installation";
var _67b=escape(document.title);
_679+="?MMredirectURL="+_67a+"&MMplayerType=ActiveX"+"&MMdoctitle="+_67b;
swflocEmbed+="?MMredirectURL="+_67a+"&MMplayerType=PlugIn";
}
_676="<object classid=\"clsid:d27cdb6e-ae6d-11cf-96b8-444553540000\" "+"codebase=\"http://fpdownload.macromedia.com/pub/shockwave/cabs/flash/swflash.cab#version=8,0,0,0\" "+"width=\""+this.width+"\" "+"height=\""+this.height+"\" "+"id=\""+this.id+"\" "+"align=\"middle\"> "+"<param name=\"allowScriptAccess\" value=\"sameDomain\" /> "+"<param name=\"movie\" value=\""+_679+"\" /> "+"<param name=\"quality\" value=\"high\" /> "+"<param name=\"bgcolor\" value=\"#ffffff\" /> "+"<embed src=\""+swflocEmbed+"\" "+"quality=\"high\" "+"bgcolor=\"#ffffff\" "+"width=\""+this.width+"\" "+"height=\""+this.height+"\" "+"id=\""+this.id+"\" "+"name=\""+this.id+"\" "+"swLiveConnect=\"true\" "+"align=\"middle\" "+"allowScriptAccess=\"sameDomain\" "+"type=\"application/x-shockwave-flash\" "+"pluginspage=\"http://www.macromedia.com/go/getflashplayer\" />"+"</object>";
}
_676="<div id=\""+this.id+"Container\" style=\""+_675+"\"> "+_676+"</div>";
document.writeln(_676);
},get:function(){
return document.getElementById(this.id);
},setVisible:function(_67c){
var _67d=dojo.byId(this.id+"Container");
if(_67c==true){
_67d.style.visibility="visible";
}else{
_67d.style.position="absolute";
_67d.style.x="-1000px";
_67d.style.y="-1000px";
_67d.style.visibility="hidden";
}
},center:function(){
var _67e=this.width;
var _67f=this.height;
var _680=document.body.clientWidth;
var _681=document.body.clientHeight;
if(!dojo.render.html.ie&&document.compatMode=="CSS1Compat"){
_680=document.body.parentNode.clientWidth;
_681=document.body.parentNode.clientHeight;
}else{
if(dojo.render.html.ie&&document.compatMode=="CSS1Compat"){
_680=document.documentElement.clientWidth;
_681=document.documentElement.clientHeight;
}else{
if(dojo.render.html.safari){
_681=self.innerHeight;
}
}
}
var _682=window.scrollX;
var _683=window.scrollY;
if(typeof _682=="undefined"){
if(document.compatMode=="CSS1Compat"){
_682=document.documentElement.scrollLeft;
_683=document.documentElement.scrollTop;
}else{
_682=document.body.scrollLeft;
_683=document.body.scrollTop;
}
}
var x=_682+(_680-_67e)/2;
var y=_683+(_681-_67f)/2;
var _686=dojo.byId(this.id+"Container");
_686.style.top=y+"px";
_686.style.left=x+"px";
}};
dojo.flash.Communicator=function(){
if(dojo.flash.useFlash6()){
this._writeFlash6();
}else{
if(dojo.flash.useFlash8()){
this._writeFlash8();
}
}
};
dojo.flash.Communicator.prototype={_writeFlash6:function(){
var id=dojo.flash.obj.id;
document.writeln("<script language=\"JavaScript\">");
document.writeln("  function "+id+"_DoFSCommand(command, args){ ");
document.writeln("    dojo.flash.comm._handleFSCommand(command, args); ");
document.writeln("}");
document.writeln("</script>");
if(dojo.render.html.ie){
document.writeln("<SCRIPT LANGUAGE=VBScript> ");
document.writeln("on error resume next ");
document.writeln("Sub "+id+"_FSCommand(ByVal command, ByVal args)");
document.writeln(" call "+id+"_DoFSCommand(command, args)");
document.writeln("end sub");
document.writeln("</SCRIPT> ");
}
},_writeFlash8:function(){
},_handleFSCommand:function(_688,args){
if(_688!=null&&!dojo.lang.isUndefined(_688)&&/^FSCommand:(.*)/.test(_688)==true){
_688=_688.match(/^FSCommand:(.*)/)[1];
}
if(_688=="addCallback"){
this._fscommandAddCallback(_688,args);
}else{
if(_688=="call"){
this._fscommandCall(_688,args);
}else{
if(_688=="fscommandReady"){
this._fscommandReady();
}
}
}
},_fscommandAddCallback:function(_68a,args){
var _68c=args;
var _68d=function(){
return dojo.flash.comm._call(_68c,arguments);
};
dojo.flash.comm[_68c]=_68d;
dojo.flash.obj.get().SetVariable("_succeeded",true);
},_fscommandCall:function(_68e,args){
var _690=dojo.flash.obj.get();
var _691=args;
var _692=parseInt(_690.GetVariable("_numArgs"));
var _693=new Array();
for(var i=0;i<_692;i++){
var _695=_690.GetVariable("_"+i);
_693.push(_695);
}
var _696;
if(_691.indexOf(".")==-1){
_696=window[_691];
}else{
_696=eval(_691);
}
var _697=null;
if(!dojo.lang.isUndefined(_696)&&_696!=null){
_697=_696.apply(null,_693);
}
_690.SetVariable("_returnResult",_697);
},_fscommandReady:function(){
var _698=dojo.flash.obj.get();
_698.SetVariable("fscommandReady","true");
},_call:function(_699,args){
var _69b=dojo.flash.obj.get();
_69b.SetVariable("_functionName",_699);
_69b.SetVariable("_numArgs",args.length);
for(var i=0;i<args.length;i++){
var _69d=args[i];
_69d=_69d.replace(/\0/g,"\\0");
_69b.SetVariable("_"+i,_69d);
}
_69b.TCallLabel("/_flashRunner","execute");
var _69e=_69b.GetVariable("_returnResult");
_69e=_69e.replace(/\\0/g,"\x00");
return _69e;
},_addExternalInterfaceCallback:function(_69f){
var _6a0=function(){
var _6a1=new Array(arguments.length);
for(var i=0;i<arguments.length;i++){
_6a1[i]=arguments[i];
}
return dojo.flash.comm._execFlash(_69f,_6a1);
};
dojo.flash.comm[_69f]=_6a0;
},_encodeData:function(data){
var _6a4=/\&([^;]*)\;/g;
data=data.replace(_6a4,"&amp;$1;");
data=data.replace(/</g,"&lt;");
data=data.replace(/>/g,"&gt;");
data=data.replace("\\","&custom_backslash;&custom_backslash;");
data=data.replace(/\n/g,"\\n");
data=data.replace(/\r/g,"\\r");
data=data.replace(/\f/g,"\\f");
data=data.replace(/\0/g,"\\0");
data=data.replace(/\'/g,"\\'");
data=data.replace(/\"/g,"\\\"");
return data;
},_decodeData:function(data){
if(data==null||typeof data=="undefined"){
return data;
}
data=data.replace(/\&custom_lt\;/g,"<");
data=data.replace(/\&custom_gt\;/g,">");
data=eval("\""+data+"\"");
return data;
},_chunkArgumentData:function(_6a6,_6a7){
var _6a8=dojo.flash.obj.get();
var _6a9=Math.ceil(_6a6.length/1024);
for(var i=0;i<_6a9;i++){
var _6ab=i*1024;
var _6ac=i*1024+1024;
if(i==(_6a9-1)){
_6ac=i*1024+_6a6.length;
}
var _6ad=_6a6.substring(_6ab,_6ac);
_6ad=this._encodeData(_6ad);
_6a8.CallFunction("<invoke name=\"chunkArgumentData\" "+"returntype=\"javascript\">"+"<arguments>"+"<string>"+_6ad+"</string>"+"<number>"+_6a7+"</number>"+"</arguments>"+"</invoke>");
}
},_chunkReturnData:function(){
var _6ae=dojo.flash.obj.get();
var _6af=_6ae.getReturnLength();
var _6b0=new Array();
for(var i=0;i<_6af;i++){
var _6b2=_6ae.CallFunction("<invoke name=\"chunkReturnData\" "+"returntype=\"javascript\">"+"<arguments>"+"<number>"+i+"</number>"+"</arguments>"+"</invoke>");
if(_6b2=="\"\""||_6b2=="''"){
_6b2="";
}else{
_6b2=_6b2.substring(1,_6b2.length-1);
}
_6b0.push(_6b2);
}
var _6b3=_6b0.join("");
return _6b3;
},_execFlash:function(_6b4,_6b5){
var _6b6=dojo.flash.obj.get();
_6b6.startExec();
_6b6.setNumberArguments(_6b5.length);
for(var i=0;i<_6b5.length;i++){
this._chunkArgumentData(_6b5[i],i);
}
_6b6.exec(_6b4);
var _6b8=this._chunkReturnData();
_6b8=this._decodeData(_6b8);
_6b6.endExec();
return _6b8;
}};
dojo.flash.Install=function(){
};
dojo.flash.Install.prototype={needed:function(){
if(dojo.flash.info.capable==false){
return true;
}
if(dojo.render.os.mac==true&&!dojo.flash.info.isVersionOrAbove(8,0,0)){
return true;
}
if(!dojo.flash.info.isVersionOrAbove(6,0,0)){
return true;
}
return false;
},install:function(){
dojo.flash.info.installing=true;
dojo.flash.installing();
if(dojo.flash.info.capable==false){
var _6b9=new dojo.flash.Embed(false);
_6b9.write(8);
}else{
if(dojo.flash.info.isVersionOrAbove(6,0,65)){
var _6b9=new dojo.flash.Embed(false);
_6b9.write(8,true);
_6b9.setVisible(true);
_6b9.center();
}else{
alert("This content requires a more recent version of the Macromedia "+" Flash Player.");
window.location.href="http://www.macromedia.com/go/getflashplayer";
}
}
},_onInstallStatus:function(msg){
if(msg=="Download.Complete"){
dojo.flash._initialize();
}else{
if(msg=="Download.Cancelled"){
alert("This content requires a more recent version of the Macromedia "+" Flash Player.");
window.location.href="http://www.macromedia.com/go/getflashplayer";
}else{
if(msg=="Download.Failed"){
alert("There was an error downloading the Flash Player update. "+"Please try again later, or visit macromedia.com to download "+"the latest version of the Flash plugin.");
}
}
}
}};
dojo.flash.info=new dojo.flash.Info();
dojo.provide("dojo.storage.browser");
dojo.provide("dojo.storage.browser.FlashStorageProvider");
dojo.require("dojo.storage");
dojo.require("dojo.flash");
dojo.require("dojo.json");
dojo.require("dojo.uri.*");
dojo.storage.browser.FlashStorageProvider=function(){
};
dojo.inherits(dojo.storage.browser.FlashStorageProvider,dojo.storage);
dojo.lang.extend(dojo.storage.browser.FlashStorageProvider,{namespace:"default",initialized:false,_available:null,_statusHandler:null,initialize:function(){
var _6bb=function(){
dojo.storage._flashLoaded();
};
dojo.flash.addLoadedListener(_6bb);
var _6bc=dojo.uri.dojoUri("Storage_version6.swf").toString();
var _6bd=dojo.uri.dojoUri("Storage_version8.swf").toString();
dojo.flash.setSwf({flash6:_6bc,flash8:_6bd,visible:false});
},isAvailable:function(){
if(djConfig["disableFlashStorage"]==true){
this._available=false;
}
return this._available;
},setNamespace:function(_6be){
this.namespace=_6be;
},put:function(key,_6c0,_6c1){
if(this.isValidKey(key)==false){
dojo.raise("Invalid key given: "+key);
}
this._statusHandler=_6c1;
if(dojo.lang.isString(_6c0)){
_6c0="string:"+_6c0;
}else{
_6c0=dojo.json.serialize(_6c0);
}
dojo.flash.comm.put(key,_6c0,this.namespace);
},get:function(key){
if(this.isValidKey(key)==false){
dojo.raise("Invalid key given: "+key);
}
var _6c3=dojo.flash.comm.get(key,this.namespace);
if(_6c3==""){
return null;
}
if(!dojo.lang.isUndefined(_6c3)&&_6c3!=null&&/^string:/.test(_6c3)){
_6c3=_6c3.substring("string:".length);
}else{
_6c3=dojo.json.evalJson(_6c3);
}
return _6c3;
},getKeys:function(){
var _6c4=dojo.flash.comm.getKeys(this.namespace);
if(_6c4==""){
return new Array();
}
_6c4=_6c4.split(",");
return _6c4;
},clear:function(){
dojo.flash.comm.clear(this.namespace);
},remove:function(key){
},isPermanent:function(){
return true;
},getMaximumSize:function(){
return dojo.storage.SIZE_NO_LIMIT;
},hasSettingsUI:function(){
return true;
},showSettingsUI:function(){
dojo.flash.comm.showSettings();
dojo.flash.obj.setVisible(true);
dojo.flash.obj.center();
},hideSettingsUI:function(){
dojo.flash.obj.setVisible(false);
if(dojo.storage.onHideSettingsUI!=null&&!dojo.lang.isUndefined(dojo.storage.onHideSettingsUI)){
dojo.storage.onHideSettingsUI.call(null);
}
},getType:function(){
return "dojo.storage.FlashStorageProvider";
},_flashLoaded:function(){
this.initialized=true;
dojo.storage.manager.loaded();
},_onStatus:function(_6c6,key){
if(_6c6==dojo.storage.PENDING){
dojo.flash.obj.center();
dojo.flash.obj.setVisible(true);
}else{
dojo.flash.obj.setVisible(false);
}
if(!dojo.lang.isUndefined(dojo.storage._statusHandler)&&dojo.storage._statusHandler!=null){
dojo.storage._statusHandler.call(null,_6c6,key);
}
}});
dojo.storage.manager.register("dojo.storage.browser.FlashStorageProvider",new dojo.storage.browser.FlashStorageProvider());
dojo.storage.manager.initialize();
dojo.kwCompoundRequire({common:["dojo.storage"],browser:["dojo.storage.browser"],dashboard:["dojo.storage.dashboard"]});
dojo.provide("dojo.storage.*");
dojo.provide("dojo.undo.Manager");
dojo.require("dojo.lang");
dojo.undo.Manager=function(_6c8){
this.clear();
this._parent=_6c8;
};
dojo.lang.extend(dojo.undo.Manager,{_parent:null,_undoStack:null,_redoStack:null,_currentManager:null,canUndo:false,canRedo:false,isUndoing:false,isRedoing:false,onUndo:function(_6c9,item){
},onRedo:function(_6cb,item){
},onUndoAny:function(_6cd,item){
},onRedoAny:function(_6cf,item){
},_updateStatus:function(){
this.canUndo=this._undoStack.length>0;
this.canRedo=this._redoStack.length>0;
},clear:function(){
this._undoStack=[];
this._redoStack=[];
this._currentManager=this;
this.isUndoing=false;
this.isRedoing=false;
this._updateStatus();
},undo:function(){
if(!this.canUndo){
return false;
}
this.endAllTransactions();
this.isUndoing=true;
var top=this._undoStack.pop();
if(top instanceof this.constructor){
top.undoAll();
}else{
top.undo();
}
if(top.redo){
this._redoStack.push(top);
}
this.isUndoing=false;
this._updateStatus();
this.onUndo(this,top);
if(!(top instanceof this.constructor)){
this.getTop().onUndoAny(this,top);
}
return true;
},redo:function(){
if(!this.canRedo){
return false;
}
this.isRedoing=true;
var top=this._redoStack.pop();
if(top instanceof this.constructor){
top.redoAll();
}else{
top.redo();
}
this._undoStack.push(top);
this.isRedoing=false;
this._updateStatus();
this.onRedo(this,top);
if(!(top instanceof this.constructor)){
this.getTop().onRedoAny(this,top);
}
return true;
},undoAll:function(){
while(this._undoStack.length>0){
this.undo();
}
},redoAll:function(){
while(this._redoStack.length>0){
this.redo();
}
},push:function(undo,redo,_6d5){
if(!undo){
return;
}
if(this._currentManager==this){
this._undoStack.push({undo:undo,redo:redo,description:_6d5});
}else{
this._currentManager.push.apply(this._currentManager,arguments);
}
this._redoStack=[];
this._updateStatus();
},concat:function(_6d6){
if(!_6d6){
return;
}
if(this._currentManager==this){
for(var x=0;x<_6d6._undoStack.length;x++){
this._undoStack.push(_6d6._undoStack[x]);
}
this._updateStatus();
}else{
this._currentManager.concat.apply(this._currentManager,arguments);
}
},beginTransaction:function(_6d8){
if(this._currentManager==this){
var mgr=new dojo.undo.Manager(this);
mgr.description=_6d8?_6d8:"";
this._undoStack.push(mgr);
this._currentManager=mgr;
return mgr;
}else{
this._currentManager=this._currentManager.beginTransaction.apply(this._currentManager,arguments);
}
},endTransaction:function(_6da){
if(this._currentManager==this){
if(this._parent){
this._parent._currentManager=this._parent;
if(this._undoStack.length==0||_6da){
var idx=dojo.lang.find(this._parent._undoStack,this);
if(idx>=0){
this._parent._undoStack.splice(idx,1);
if(_6da){
for(var x=0;x<this._undoStack.length;x++){
this._parent._undoStack.splice(idx++,0,this._undoStack[x]);
}
this._updateStatus();
}
}
}
return this._parent;
}
}else{
this._currentManager=this._currentManager.endTransaction.apply(this._currentManager,arguments);
}
},endAllTransactions:function(){
while(this._currentManager!=this){
this.endTransaction();
}
},getTop:function(){
if(this._parent){
return this._parent.getTop();
}else{
return this;
}
}});
dojo.require("dojo.undo.Manager");
dojo.provide("dojo.undo.*");
dojo.provide("dojo.crypto");
dojo.crypto.cipherModes={ECB:0,CBC:1,PCBC:2,CFB:3,OFB:4,CTR:5};
dojo.crypto.outputTypes={Base64:0,Hex:1,String:2,Raw:3};
dojo.require("dojo.crypto");
dojo.provide("dojo.crypto.MD5");
dojo.crypto.MD5=new function(){
var _6dd=8;
var mask=(1<<_6dd)-1;
function toWord(s){
var wa=[];
for(var i=0;i<s.length*_6dd;i+=_6dd){
wa[i>>5]|=(s.charCodeAt(i/_6dd)&mask)<<(i%32);
}
return wa;
}
function toString(wa){
var s=[];
for(var i=0;i<wa.length*32;i+=_6dd){
s.push(String.fromCharCode((wa[i>>5]>>>(i%32))&mask));
}
return s.join("");
}
function toHex(wa){
var h="0123456789abcdef";
var s=[];
for(var i=0;i<wa.length*4;i++){
s.push(h.charAt((wa[i>>2]>>((i%4)*8+4))&15)+h.charAt((wa[i>>2]>>((i%4)*8))&15));
}
return s.join("");
}
function toBase64(wa){
var p="=";
var tab="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
var s=[];
for(var i=0;i<wa.length*4;i+=3){
var t=(((wa[i>>2]>>8*(i%4))&255)<<16)|(((wa[i+1>>2]>>8*((i+1)%4))&255)<<8)|((wa[i+2>>2]>>8*((i+2)%4))&255);
for(var j=0;j<4;j++){
if(i*8+j*6>wa.length*32){
s.push(p);
}else{
s.push(tab.charAt((t>>6*(3-j))&63));
}
}
}
return s.join("");
}
function add(x,y){
var l=(x&65535)+(y&65535);
var m=(x>>16)+(y>>16)+(l>>16);
return (m<<16)|(l&65535);
}
function R(n,c){
return (n<<c)|(n>>>(32-c));
}
function C(q,a,b,x,s,t){
return add(R(add(add(a,q),add(x,t)),s),b);
}
function FF(a,b,c,d,x,s,t){
return C((b&c)|((~b)&d),a,b,x,s,t);
}
function GG(a,b,c,d,x,s,t){
return C((b&d)|(c&(~d)),a,b,x,s,t);
}
function HH(a,b,c,d,x,s,t){
return C(b^c^d,a,b,x,s,t);
}
function II(a,b,c,d,x,s,t){
return C(c^(b|(~d)),a,b,x,s,t);
}
function core(x,len){
x[len>>5]|=128<<((len)%32);
x[(((len+64)>>>9)<<4)+14]=len;
var a=1732584193;
var b=-271733879;
var c=-1732584194;
var d=271733878;
for(var i=0;i<x.length;i+=16){
var olda=a;
var oldb=b;
var oldc=c;
var oldd=d;
a=FF(a,b,c,d,x[i+0],7,-680876936);
d=FF(d,a,b,c,x[i+1],12,-389564586);
c=FF(c,d,a,b,x[i+2],17,606105819);
b=FF(b,c,d,a,x[i+3],22,-1044525330);
a=FF(a,b,c,d,x[i+4],7,-176418897);
d=FF(d,a,b,c,x[i+5],12,1200080426);
c=FF(c,d,a,b,x[i+6],17,-1473231341);
b=FF(b,c,d,a,x[i+7],22,-45705983);
a=FF(a,b,c,d,x[i+8],7,1770035416);
d=FF(d,a,b,c,x[i+9],12,-1958414417);
c=FF(c,d,a,b,x[i+10],17,-42063);
b=FF(b,c,d,a,x[i+11],22,-1990404162);
a=FF(a,b,c,d,x[i+12],7,1804603682);
d=FF(d,a,b,c,x[i+13],12,-40341101);
c=FF(c,d,a,b,x[i+14],17,-1502002290);
b=FF(b,c,d,a,x[i+15],22,1236535329);
a=GG(a,b,c,d,x[i+1],5,-165796510);
d=GG(d,a,b,c,x[i+6],9,-1069501632);
c=GG(c,d,a,b,x[i+11],14,643717713);
b=GG(b,c,d,a,x[i+0],20,-373897302);
a=GG(a,b,c,d,x[i+5],5,-701558691);
d=GG(d,a,b,c,x[i+10],9,38016083);
c=GG(c,d,a,b,x[i+15],14,-660478335);
b=GG(b,c,d,a,x[i+4],20,-405537848);
a=GG(a,b,c,d,x[i+9],5,568446438);
d=GG(d,a,b,c,x[i+14],9,-1019803690);
c=GG(c,d,a,b,x[i+3],14,-187363961);
b=GG(b,c,d,a,x[i+8],20,1163531501);
a=GG(a,b,c,d,x[i+13],5,-1444681467);
d=GG(d,a,b,c,x[i+2],9,-51403784);
c=GG(c,d,a,b,x[i+7],14,1735328473);
b=GG(b,c,d,a,x[i+12],20,-1926607734);
a=HH(a,b,c,d,x[i+5],4,-378558);
d=HH(d,a,b,c,x[i+8],11,-2022574463);
c=HH(c,d,a,b,x[i+11],16,1839030562);
b=HH(b,c,d,a,x[i+14],23,-35309556);
a=HH(a,b,c,d,x[i+1],4,-1530992060);
d=HH(d,a,b,c,x[i+4],11,1272893353);
c=HH(c,d,a,b,x[i+7],16,-155497632);
b=HH(b,c,d,a,x[i+10],23,-1094730640);
a=HH(a,b,c,d,x[i+13],4,681279174);
d=HH(d,a,b,c,x[i+0],11,-358537222);
c=HH(c,d,a,b,x[i+3],16,-722521979);
b=HH(b,c,d,a,x[i+6],23,76029189);
a=HH(a,b,c,d,x[i+9],4,-640364487);
d=HH(d,a,b,c,x[i+12],11,-421815835);
c=HH(c,d,a,b,x[i+15],16,530742520);
b=HH(b,c,d,a,x[i+2],23,-995338651);
a=II(a,b,c,d,x[i+0],6,-198630844);
d=II(d,a,b,c,x[i+7],10,1126891415);
c=II(c,d,a,b,x[i+14],15,-1416354905);
b=II(b,c,d,a,x[i+5],21,-57434055);
a=II(a,b,c,d,x[i+12],6,1700485571);
d=II(d,a,b,c,x[i+3],10,-1894986606);
c=II(c,d,a,b,x[i+10],15,-1051523);
b=II(b,c,d,a,x[i+1],21,-2054922799);
a=II(a,b,c,d,x[i+8],6,1873313359);
d=II(d,a,b,c,x[i+15],10,-30611744);
c=II(c,d,a,b,x[i+6],15,-1560198380);
b=II(b,c,d,a,x[i+13],21,1309151649);
a=II(a,b,c,d,x[i+4],6,-145523070);
d=II(d,a,b,c,x[i+11],10,-1120210379);
c=II(c,d,a,b,x[i+2],15,718787259);
b=II(b,c,d,a,x[i+9],21,-343485551);
a=add(a,olda);
b=add(b,oldb);
c=add(c,oldc);
d=add(d,oldd);
}
return [a,b,c,d];
}
function hmac(data,key){
var wa=toWord(key);
if(wa.length>16){
wa=core(wa,key.length*_6dd);
}
var l=[],r=[];
for(var i=0;i<16;i++){
l[i]=wa[i]^909522486;
r[i]=wa[i]^1549556828;
}
var h=core(l.concat(toWord(data)),512+data.length*_6dd);
return core(r.concat(h),640);
}
this.compute=function(data,_72a){
var out=_72a||dojo.crypto.outputTypes.Base64;
switch(out){
case dojo.crypto.outputTypes.Hex:
return toHex(core(toWord(data),data.length*_6dd));
case dojo.crypto.outputTypes.String:
return toString(core(toWord(data),data.length*_6dd));
default:
return toBase64(core(toWord(data),data.length*_6dd));
}
};
this.getHMAC=function(data,key,_72e){
var out=_72e||dojo.crypto.outputTypes.Base64;
switch(out){
case dojo.crypto.outputTypes.Hex:
return toHex(hmac(data,key));
case dojo.crypto.outputTypes.String:
return toString(hmac(data,key));
default:
return toBase64(hmac(data,key));
}
};
}();
dojo.kwCompoundRequire({common:["dojo.crypto","dojo.crypto.MD5"]});
dojo.provide("dojo.crypto.*");
dojo.provide("dojo.collections.Collections");
dojo.collections={Collections:true};
dojo.collections.DictionaryEntry=function(k,v){
this.key=k;
this.value=v;
this.valueOf=function(){
return this.value;
};
this.toString=function(){
return String(this.value);
};
};
dojo.collections.Iterator=function(arr){
var a=arr;
var _734=0;
this.element=a[_734]||null;
this.atEnd=function(){
return (_734>=a.length);
};
this.get=function(){
if(this.atEnd()){
return null;
}
this.element=a[_734++];
return this.element;
};
this.map=function(fn,_736){
var s=_736||dj_global;
if(Array.map){
return Array.map(a,fn,s);
}else{
var arr=[];
for(var i=0;i<a.length;i++){
arr.push(fn.call(s,a[i]));
}
return arr;
}
};
this.reset=function(){
_734=0;
this.element=a[_734];
};
};
dojo.collections.DictionaryIterator=function(obj){
var a=[];
for(var p in obj){
a.push(obj[p]);
}
var _73d=0;
this.element=a[_73d]||null;
this.atEnd=function(){
return (_73d>=a.length);
};
this.get=function(){
if(this.atEnd()){
return null;
}
this.element=a[_73d++];
return this.element;
};
this.map=function(fn,_73f){
var s=_73f||dj_global;
if(Array.map){
return Array.map(a,fn,s);
}else{
var arr=[];
for(var i=0;i<a.length;i++){
arr.push(fn.call(s,a[i]));
}
return arr;
}
};
this.reset=function(){
_73d=0;
this.element=a[_73d];
};
};
dojo.provide("dojo.collections.ArrayList");
dojo.require("dojo.collections.Collections");
dojo.collections.ArrayList=function(arr){
var _744=[];
if(arr){
_744=_744.concat(arr);
}
this.count=_744.length;
this.add=function(obj){
_744.push(obj);
this.count=_744.length;
};
this.addRange=function(a){
if(a.getIterator){
var e=a.getIterator();
while(!e.atEnd()){
this.add(e.get());
}
this.count=_744.length;
}else{
for(var i=0;i<a.length;i++){
_744.push(a[i]);
}
this.count=_744.length;
}
};
this.clear=function(){
_744.splice(0,_744.length);
this.count=0;
};
this.clone=function(){
return new dojo.collections.ArrayList(_744);
};
this.contains=function(obj){
for(var i=0;i<_744.length;i++){
if(_744[i]==obj){
return true;
}
}
return false;
};
this.forEach=function(fn,_74c){
var s=_74c||dj_global;
if(Array.forEach){
Array.forEach(_744,fn,s);
}else{
for(var i=0;i<_744.length;i++){
fn.call(s,_744[i],i,_744);
}
}
};
this.getIterator=function(){
return new dojo.collections.Iterator(_744);
};
this.indexOf=function(obj){
for(var i=0;i<_744.length;i++){
if(_744[i]==obj){
return i;
}
}
return -1;
};
this.insert=function(i,obj){
_744.splice(i,0,obj);
this.count=_744.length;
};
this.item=function(i){
return _744[i];
};
this.remove=function(obj){
var i=this.indexOf(obj);
if(i>=0){
_744.splice(i,1);
}
this.count=_744.length;
};
this.removeAt=function(i){
_744.splice(i,1);
this.count=_744.length;
};
this.reverse=function(){
_744.reverse();
};
this.sort=function(fn){
if(fn){
_744.sort(fn);
}else{
_744.sort();
}
};
this.setByIndex=function(i,obj){
_744[i]=obj;
this.count=_744.length;
};
this.toArray=function(){
return [].concat(_744);
};
this.toString=function(_75a){
return _744.join((_75a||","));
};
};
dojo.provide("dojo.collections.Queue");
dojo.require("dojo.collections.Collections");
dojo.collections.Queue=function(arr){
var q=[];
if(arr){
q=q.concat(arr);
}
this.count=q.length;
this.clear=function(){
q=[];
this.count=q.length;
};
this.clone=function(){
return new dojo.collections.Queue(q);
};
this.contains=function(o){
for(var i=0;i<q.length;i++){
if(q[i]==o){
return true;
}
}
return false;
};
this.copyTo=function(arr,i){
arr.splice(i,0,q);
};
this.dequeue=function(){
var r=q.shift();
this.count=q.length;
return r;
};
this.enqueue=function(o){
this.count=q.push(o);
};
this.forEach=function(fn,_764){
var s=_764||dj_global;
if(Array.forEach){
Array.forEach(q,fn,s);
}else{
for(var i=0;i<items.length;i++){
fn.call(s,q[i],i,q);
}
}
};
this.getIterator=function(){
return new dojo.collections.Iterator(q);
};
this.peek=function(){
return q[0];
};
this.toArray=function(){
return [].concat(q);
};
};
dojo.provide("dojo.collections.Stack");
dojo.require("dojo.collections.Collections");
dojo.collections.Stack=function(arr){
var q=[];
if(arr){
q=q.concat(arr);
}
this.count=q.length;
this.clear=function(){
q=[];
this.count=q.length;
};
this.clone=function(){
return new dojo.collections.Stack(q);
};
this.contains=function(o){
for(var i=0;i<q.length;i++){
if(q[i]==o){
return true;
}
}
return false;
};
this.copyTo=function(arr,i){
arr.splice(i,0,q);
};
this.forEach=function(fn,_76e){
var s=_76e||dj_global;
if(Array.forEach){
Array.forEach(q,fn,s);
}else{
for(var i=0;i<items.length;i++){
fn.call(s,q[i],i,q);
}
}
};
this.getIterator=function(){
return new dojo.collections.Iterator(q);
};
this.peek=function(){
return q[(q.length-1)];
};
this.pop=function(){
var r=q.pop();
this.count=q.length;
return r;
};
this.push=function(o){
this.count=q.push(o);
};
this.toArray=function(){
return [].concat(q);
};
};
dojo.require("dojo.lang");
dojo.provide("dojo.dnd.DragSource");
dojo.provide("dojo.dnd.DropTarget");
dojo.provide("dojo.dnd.DragObject");
dojo.provide("dojo.dnd.DragAndDrop");
dojo.dnd.DragSource=function(){
var dm=dojo.dnd.dragManager;
if(dm["registerDragSource"]){
dm.registerDragSource(this);
}
};
dojo.lang.extend(dojo.dnd.DragSource,{type:"",onDragEnd:function(){
},onDragStart:function(){
},unregister:function(){
dojo.dnd.dragManager.unregisterDragSource(this);
},reregister:function(){
dojo.dnd.dragManager.registerDragSource(this);
}});
dojo.dnd.DragObject=function(){
var dm=dojo.dnd.dragManager;
if(dm["registerDragObject"]){
dm.registerDragObject(this);
}
};
dojo.lang.extend(dojo.dnd.DragObject,{type:"",onDragStart:function(){
},onDragMove:function(){
},onDragOver:function(){
},onDragOut:function(){
},onDragEnd:function(){
},onDragLeave:this.onDragOut,onDragEnter:this.onDragOver,ondragout:this.onDragOut,ondragover:this.onDragOver});
dojo.dnd.DropTarget=function(){
if(this.constructor==dojo.dnd.DropTarget){
return;
}
this.acceptedTypes=[];
dojo.dnd.dragManager.registerDropTarget(this);
};
dojo.lang.extend(dojo.dnd.DropTarget,{acceptsType:function(type){
if(!dojo.lang.inArray(this.acceptedTypes,"*")){
if(!dojo.lang.inArray(this.acceptedTypes,type)){
return false;
}
}
return true;
},accepts:function(_776){
if(!dojo.lang.inArray(this.acceptedTypes,"*")){
for(var i=0;i<_776.length;i++){
if(!dojo.lang.inArray(this.acceptedTypes,_776[i].type)){
return false;
}
}
}
return true;
},onDragOver:function(){
},onDragOut:function(){
},onDragMove:function(){
},onDropStart:function(){
},onDrop:function(){
},onDropEnd:function(){
}});
dojo.dnd.DragEvent=function(){
this.dragSource=null;
this.dragObject=null;
this.target=null;
this.eventStatus="success";
};
dojo.dnd.DragManager=function(){
};
dojo.lang.extend(dojo.dnd.DragManager,{selectedSources:[],dragObjects:[],dragSources:[],registerDragSource:function(){
},dropTargets:[],registerDropTarget:function(){
},lastDragTarget:null,currentDragTarget:null,onKeyDown:function(){
},onMouseOut:function(){
},onMouseMove:function(){
},onMouseUp:function(){
}});
dojo.provide("dojo.dnd.HtmlDragManager");
dojo.require("dojo.dnd.DragAndDrop");
dojo.require("dojo.event.*");
dojo.require("dojo.lang.array");
dojo.require("dojo.html");
dojo.require("dojo.style");
dojo.dnd.HtmlDragManager=function(){
};
dojo.inherits(dojo.dnd.HtmlDragManager,dojo.dnd.DragManager);
dojo.lang.extend(dojo.dnd.HtmlDragManager,{disabled:false,nestedTargets:false,mouseDownTimer:null,dsCounter:0,dsPrefix:"dojoDragSource",dropTargetDimensions:[],currentDropTarget:null,previousDropTarget:null,_dragTriggered:false,selectedSources:[],dragObjects:[],currentX:null,currentY:null,lastX:null,lastY:null,mouseDownX:null,mouseDownY:null,threshold:7,dropAcceptable:false,cancelEvent:function(e){
e.stopPropagation();
e.preventDefault();
},registerDragSource:function(ds){
if(ds["domNode"]){
var dp=this.dsPrefix;
var _77b=dp+"Idx_"+(this.dsCounter++);
ds.dragSourceId=_77b;
this.dragSources[_77b]=ds;
ds.domNode.setAttribute(dp,_77b);
if(dojo.render.html.ie){
dojo.event.connect(ds.domNode,"ondragstart",this.cancelEvent);
}
}
},unregisterDragSource:function(ds){
if(ds["domNode"]){
var dp=this.dsPrefix;
var _77e=ds.dragSourceId;
delete ds.dragSourceId;
delete this.dragSources[_77e];
ds.domNode.setAttribute(dp,null);
}
if(dojo.render.html.ie){
dojo.event.disconnect(ds.domNode,"ondragstart",this.cancelEvent);
}
},registerDropTarget:function(dt){
this.dropTargets.push(dt);
},unregisterDropTarget:function(dt){
var _781=dojo.lang.find(this.dropTargets,dt,true);
if(_781>=0){
this.dropTargets.splice(_781,1);
}
},getDragSource:function(e){
var tn=e.target;
if(tn===document.body){
return;
}
var ta=dojo.html.getAttribute(tn,this.dsPrefix);
while((!ta)&&(tn)){
tn=tn.parentNode;
if((!tn)||(tn===document.body)){
return;
}
ta=dojo.html.getAttribute(tn,this.dsPrefix);
}
return this.dragSources[ta];
},onKeyDown:function(e){
},onMouseDown:function(e){
if(this.disabled){
return;
}
if(dojo.render.html.ie){
if(e.button!=1){
return;
}
}else{
if(e.which!=1){
return;
}
}
var _787=e.target.nodeType==dojo.dom.TEXT_NODE?e.target.parentNode:e.target;
if(dojo.html.isTag(_787,"button","textarea","input","select","option")){
return;
}
var ds=this.getDragSource(e);
if(!ds){
return;
}
if(!dojo.lang.inArray(this.selectedSources,ds)){
this.selectedSources.push(ds);
}
this.mouseDownX=e.pageX;
this.mouseDownY=e.pageY;
e.preventDefault();
dojo.event.connect(document,"onmousemove",this,"onMouseMove");
},onMouseUp:function(e,_78a){
if(this.selectedSources.length==0){
return;
}
this.mouseDownX=null;
this.mouseDownY=null;
this._dragTriggered=false;
e.dragSource=this.dragSource;
if((!e.shiftKey)&&(!e.ctrlKey)){
if(this.currentDropTarget){
this.currentDropTarget.onDropStart();
}
dojo.lang.forEach(this.dragObjects,function(_78b){
var ret=null;
if(!_78b){
return;
}
if(this.currentDropTarget){
e.dragObject=_78b;
var ce=this.currentDropTarget.domNode.childNodes;
if(ce.length>0){
e.dropTarget=ce[0];
while(e.dropTarget==_78b.domNode){
e.dropTarget=e.dropTarget.nextSibling;
}
}else{
e.dropTarget=this.currentDropTarget.domNode;
}
if(this.dropAcceptable){
ret=this.currentDropTarget.onDrop(e);
}else{
this.currentDropTarget.onDragOut(e);
}
}
e.dragStatus=this.dropAcceptable&&ret?"dropSuccess":"dropFailure";
_78b.dragSource.onDragEnd(e);
_78b.onDragEnd(e);
},this);
this.selectedSources=[];
this.dragObjects=[];
this.dragSource=null;
if(this.currentDropTarget){
this.currentDropTarget.onDropEnd();
}
}
dojo.event.disconnect(document,"onmousemove",this,"onMouseMove");
this.currentDropTarget=null;
},onScroll:function(){
for(var i=0;i<this.dragObjects.length;i++){
if(this.dragObjects[i].updateDragOffset){
this.dragObjects[i].updateDragOffset();
}
}
this.cacheTargetLocations();
},_dragStartDistance:function(x,y){
if((!this.mouseDownX)||(!this.mouseDownX)){
return;
}
var dx=Math.abs(x-this.mouseDownX);
var dx2=dx*dx;
var dy=Math.abs(y-this.mouseDownY);
var dy2=dy*dy;
return parseInt(Math.sqrt(dx2+dy2),10);
},cacheTargetLocations:function(){
this.dropTargetDimensions=[];
dojo.lang.forEach(this.dropTargets,function(_795){
var tn=_795.domNode;
if(!tn){
return;
}
var ttx=dojo.style.getAbsoluteX(tn,true);
var tty=dojo.style.getAbsoluteY(tn,true);
this.dropTargetDimensions.push([[ttx,tty],[ttx+dojo.style.getInnerWidth(tn),tty+dojo.style.getInnerHeight(tn)],_795]);
},this);
},onMouseMove:function(e){
if((dojo.render.html.ie)&&(e.button!=1)){
this.currentDropTarget=null;
this.onMouseUp(e,true);
return;
}
if((this.selectedSources.length)&&(!this.dragObjects.length)){
var dx;
var dy;
if(!this._dragTriggered){
this._dragTriggered=(this._dragStartDistance(e.pageX,e.pageY)>this.threshold);
if(!this._dragTriggered){
return;
}
dx=e.pageX-this.mouseDownX;
dy=e.pageY-this.mouseDownY;
}
if(this.selectedSources.length==1){
this.dragSource=this.selectedSources[0];
}
dojo.lang.forEach(this.selectedSources,function(_79c){
if(!_79c){
return;
}
var tdo=_79c.onDragStart(e);
if(tdo){
tdo.onDragStart(e);
tdo.dragOffset.top+=dy;
tdo.dragOffset.left+=dx;
tdo.dragSource=_79c;
this.dragObjects.push(tdo);
}
},this);
this.previousDropTarget=null;
this.cacheTargetLocations();
}
dojo.lang.forEach(this.dragObjects,function(_79e){
if(_79e){
_79e.onDragMove(e);
}
});
if(this.currentDropTarget){
var c=dojo.style.toCoordinateArray(this.currentDropTarget.domNode,true);
var dtp=[[c[0],c[1]],[c[0]+c[2],c[1]+c[3]]];
}
if((!this.nestedTargets)&&(dtp)&&(this.isInsideBox(e,dtp))){
if(this.dropAcceptable){
this.currentDropTarget.onDragMove(e,this.dragObjects);
}
}else{
var _7a1=this.findBestTarget(e);
if(_7a1.target===null){
if(this.currentDropTarget){
this.currentDropTarget.onDragOut(e);
this.previousDropTarget=this.currentDropTarget;
this.currentDropTarget=null;
}
this.dropAcceptable=false;
return;
}
if(this.currentDropTarget!==_7a1.target){
if(this.currentDropTarget){
this.previousDropTarget=this.currentDropTarget;
this.currentDropTarget.onDragOut(e);
}
this.currentDropTarget=_7a1.target;
e.dragObjects=this.dragObjects;
this.dropAcceptable=this.currentDropTarget.onDragOver(e);
}else{
if(this.dropAcceptable){
this.currentDropTarget.onDragMove(e,this.dragObjects);
}
}
}
},findBestTarget:function(e){
var _7a3=this;
var _7a4=new Object();
_7a4.target=null;
_7a4.points=null;
dojo.lang.every(this.dropTargetDimensions,function(_7a5){
if(!_7a3.isInsideBox(e,_7a5)){
return true;
}
_7a4.target=_7a5[2];
_7a4.points=_7a5;
return Boolean(_7a3.nestedTargets);
});
return _7a4;
},isInsideBox:function(e,_7a7){
if((e.pageX>_7a7[0][0])&&(e.pageX<_7a7[1][0])&&(e.pageY>_7a7[0][1])&&(e.pageY<_7a7[1][1])){
return true;
}
return false;
},onMouseOver:function(e){
},onMouseOut:function(e){
}});
dojo.dnd.dragManager=new dojo.dnd.HtmlDragManager();
(function(){
var d=document;
var dm=dojo.dnd.dragManager;
dojo.event.connect(d,"onkeydown",dm,"onKeyDown");
dojo.event.connect(d,"onmouseover",dm,"onMouseOver");
dojo.event.connect(d,"onmouseout",dm,"onMouseOut");
dojo.event.connect(d,"onmousedown",dm,"onMouseDown");
dojo.event.connect(d,"onmouseup",dm,"onMouseUp");
dojo.event.connect(window,"onscroll",dm,"onScroll");
})();
dojo.require("dojo.html");
dojo.provide("dojo.html.extras");
dojo.require("dojo.string.extras");
dojo.html.gravity=function(node,e){
node=dojo.byId(node);
var _7ae=dojo.html.getCursorPosition(e);
with(dojo.html){
var _7af=getAbsoluteX(node,true)+(getInnerWidth(node)/2);
var _7b0=getAbsoluteY(node,true)+(getInnerHeight(node)/2);
}
with(dojo.html.gravity){
return ((_7ae.x<_7af?WEST:EAST)|(_7ae.y<_7b0?NORTH:SOUTH));
}
};
dojo.html.gravity.NORTH=1;
dojo.html.gravity.SOUTH=1<<1;
dojo.html.gravity.EAST=1<<2;
dojo.html.gravity.WEST=1<<3;
dojo.html.renderedTextContent=function(node){
node=dojo.byId(node);
var _7b2="";
if(node==null){
return _7b2;
}
for(var i=0;i<node.childNodes.length;i++){
switch(node.childNodes[i].nodeType){
case 1:
case 5:
var _7b4="unknown";
try{
_7b4=dojo.style.getStyle(node.childNodes[i],"display");
}
catch(E){
}
switch(_7b4){
case "block":
case "list-item":
case "run-in":
case "table":
case "table-row-group":
case "table-header-group":
case "table-footer-group":
case "table-row":
case "table-column-group":
case "table-column":
case "table-cell":
case "table-caption":
_7b2+="\n";
_7b2+=dojo.html.renderedTextContent(node.childNodes[i]);
_7b2+="\n";
break;
case "none":
break;
default:
if(node.childNodes[i].tagName&&node.childNodes[i].tagName.toLowerCase()=="br"){
_7b2+="\n";
}else{
_7b2+=dojo.html.renderedTextContent(node.childNodes[i]);
}
break;
}
break;
case 3:
case 2:
case 4:
var text=node.childNodes[i].nodeValue;
var _7b6="unknown";
try{
_7b6=dojo.style.getStyle(node,"text-transform");
}
catch(E){
}
switch(_7b6){
case "capitalize":
text=dojo.string.capitalize(text);
break;
case "uppercase":
text=text.toUpperCase();
break;
case "lowercase":
text=text.toLowerCase();
break;
default:
break;
}
switch(_7b6){
case "nowrap":
break;
case "pre-wrap":
break;
case "pre-line":
break;
case "pre":
break;
default:
text=text.replace(/\s+/," ");
if(/\s$/.test(_7b2)){
text.replace(/^\s/,"");
}
break;
}
_7b2+=text;
break;
default:
break;
}
}
return _7b2;
};
dojo.html.createNodesFromText=function(txt,trim){
if(trim){
txt=dojo.string.trim(txt);
}
var tn=document.createElement("div");
tn.style.visibility="hidden";
document.body.appendChild(tn);
var _7ba="none";
if((/^<t[dh][\s\r\n>]/i).test(dojo.string.trimStart(txt))){
txt="<table><tbody><tr>"+txt+"</tr></tbody></table>";
_7ba="cell";
}else{
if((/^<tr[\s\r\n>]/i).test(dojo.string.trimStart(txt))){
txt="<table><tbody>"+txt+"</tbody></table>";
_7ba="row";
}else{
if((/^<(thead|tbody|tfoot)[\s\r\n>]/i).test(dojo.string.trimStart(txt))){
txt="<table>"+txt+"</table>";
_7ba="section";
}
}
}
tn.innerHTML=txt;
if(tn["normalize"]){
tn.normalize();
}
var _7bb=null;
switch(_7ba){
case "cell":
_7bb=tn.getElementsByTagName("tr")[0];
break;
case "row":
_7bb=tn.getElementsByTagName("tbody")[0];
break;
case "section":
_7bb=tn.getElementsByTagName("table")[0];
break;
default:
_7bb=tn;
break;
}
var _7bc=[];
for(var x=0;x<_7bb.childNodes.length;x++){
_7bc.push(_7bb.childNodes[x].cloneNode(true));
}
tn.style.display="none";
document.body.removeChild(tn);
return _7bc;
};
dojo.html.placeOnScreen=function(node,_7bf,_7c0,_7c1,_7c2){
if(dojo.lang.isArray(_7bf)){
_7c2=_7c1;
_7c1=_7c0;
_7c0=_7bf[1];
_7bf=_7bf[0];
}
if(!isNaN(_7c1)){
_7c1=[Number(_7c1),Number(_7c1)];
}else{
if(!dojo.lang.isArray(_7c1)){
_7c1=[0,0];
}
}
var _7c3=dojo.html.getScrollOffset();
var view=dojo.html.getViewportSize();
node=dojo.byId(node);
var w=node.offsetWidth+_7c1[0];
var h=node.offsetHeight+_7c1[1];
if(_7c2){
_7bf-=_7c3.x;
_7c0-=_7c3.y;
}
var x=_7bf+w;
if(x>view.w){
x=view.w-w;
}else{
x=_7bf;
}
x=Math.max(_7c1[0],x)+_7c3.x;
var y=_7c0+h;
if(y>view.h){
y=view.h-h;
}else{
y=_7c0;
}
y=Math.max(_7c1[1],y)+_7c3.y;
node.style.left=x+"px";
node.style.top=y+"px";
var ret=[x,y];
ret.x=x;
ret.y=y;
return ret;
};
dojo.html.placeOnScreenPoint=function(node,_7cb,_7cc,_7cd,_7ce){
if(dojo.lang.isArray(_7cb)){
_7ce=_7cd;
_7cd=_7cc;
_7cc=_7cb[1];
_7cb=_7cb[0];
}
if(!isNaN(_7cd)){
_7cd=[Number(_7cd),Number(_7cd)];
}else{
if(!dojo.lang.isArray(_7cd)){
_7cd=[0,0];
}
}
var _7cf=dojo.html.getScrollOffset();
var view=dojo.html.getViewportSize();
node=dojo.byId(node);
var _7d1=node.style.display;
node.style.display="";
var w=dojo.style.getInnerWidth(node);
var h=dojo.style.getInnerHeight(node);
node.style.display=_7d1;
if(_7ce){
_7cb-=_7cf.x;
_7cc-=_7cf.y;
}
var x=-1,y=-1;
if((_7cb+_7cd[0])+w<=view.w&&(_7cc+_7cd[1])+h<=view.h){
x=(_7cb+_7cd[0]);
y=(_7cc+_7cd[1]);
}
if((x<0||y<0)&&(_7cb-_7cd[0])<=view.w&&(_7cc+_7cd[1])+h<=view.h){
x=(_7cb-_7cd[0])-w;
y=(_7cc+_7cd[1]);
}
if((x<0||y<0)&&(_7cb+_7cd[0])+w<=view.w&&(_7cc-_7cd[1])<=view.h){
x=(_7cb+_7cd[0]);
y=(_7cc-_7cd[1])-h;
}
if((x<0||y<0)&&(_7cb-_7cd[0])<=view.w&&(_7cc-_7cd[1])<=view.h){
x=(_7cb-_7cd[0])-w;
y=(_7cc-_7cd[1])-h;
}
if(x<0||y<0||(x+w>view.w)||(y+h>view.h)){
return dojo.html.placeOnScreen(node,_7cb,_7cc,_7cd,_7ce);
}
x+=_7cf.x;
y+=_7cf.y;
node.style.left=x+"px";
node.style.top=y+"px";
var ret=[x,y];
ret.x=x;
ret.y=y;
return ret;
};
dojo.html.BackgroundIframe=function(node){
if(dojo.render.html.ie){
var html="<iframe "+"style='position: absolute; left: 0px; top: 0px; width: 100%; height: 100%;"+"z-index: -1; filter:Alpha(Opacity=\"0\");' "+">";
this.iframe=document.createElement(html);
if(node){
node.appendChild(this.iframe);
this.domNode=node;
}else{
document.body.appendChild(this.iframe);
this.iframe.style.display="none";
}
}
};
dojo.lang.extend(dojo.html.BackgroundIframe,{iframe:null,onResized:function(){
if(this.iframe&&this.domNode){
var w=dojo.style.getOuterWidth(this.domNode);
var h=dojo.style.getOuterHeight(this.domNode);
if(w==0||h==0){
dojo.lang.setTimeout(this,this.onResized,50);
return;
}
var s=this.iframe.style;
s.width=w+"px";
s.height=h+"px";
}
},size:function(node){
if(!this.iframe){
return;
}
coords=dojo.style.toCoordinateArray(node,true);
var s=this.iframe.style;
s.width=coords.w+"px";
s.height=coords.h+"px";
s.left=coords.x+"px";
s.top=coords.y+"px";
},setZIndex:function(node){
if(!this.iframe){
return;
}
if(dojo.dom.isNode(node)){
this.iframe.style.zIndex=dojo.html.getStyle(node,"z-index")-1;
}else{
if(!isNaN(node)){
this.iframe.style.zIndex=node;
}
}
},show:function(){
if(!this.iframe){
return;
}
this.iframe.style.display="block";
},hide:function(){
if(!this.ie){
return;
}
var s=this.iframe.style;
s.display="none";
},remove:function(){
dojo.dom.removeNode(this.iframe);
}});
dojo.provide("dojo.dnd.HtmlDragAndDrop");
dojo.provide("dojo.dnd.HtmlDragSource");
dojo.provide("dojo.dnd.HtmlDropTarget");
dojo.provide("dojo.dnd.HtmlDragObject");
dojo.require("dojo.dnd.HtmlDragManager");
dojo.require("dojo.dnd.DragAndDrop");
dojo.require("dojo.dom");
dojo.require("dojo.style");
dojo.require("dojo.html");
dojo.require("dojo.html.extras");
dojo.require("dojo.lang.extras");
dojo.require("dojo.lfx.*");
dojo.dnd.HtmlDragSource=function(node,type){
node=dojo.byId(node);
this.constrainToContainer=false;
if(node){
this.domNode=node;
this.dragObject=node;
dojo.dnd.DragSource.call(this);
this.type=type||this.domNode.nodeName.toLowerCase();
}
};
dojo.inherits(dojo.dnd.HtmlDragSource,dojo.dnd.DragSource);
dojo.lang.extend(dojo.dnd.HtmlDragSource,{dragClass:"",onDragStart:function(){
var _7e1=new dojo.dnd.HtmlDragObject(this.dragObject,this.type);
if(this.dragClass){
_7e1.dragClass=this.dragClass;
}
if(this.constrainToContainer){
_7e1.constrainTo(this.constrainingContainer||this.domNode.parentNode);
}
return _7e1;
},setDragHandle:function(node){
node=dojo.byId(node);
dojo.dnd.dragManager.unregisterDragSource(this);
this.domNode=node;
dojo.dnd.dragManager.registerDragSource(this);
},setDragTarget:function(node){
this.dragObject=node;
},constrainTo:function(_7e4){
this.constrainToContainer=true;
if(_7e4){
this.constrainingContainer=_7e4;
}
}});
dojo.dnd.HtmlDragObject=function(node,type){
this.domNode=dojo.byId(node);
this.type=type;
this.constrainToContainer=false;
this.dragSource=null;
};
dojo.inherits(dojo.dnd.HtmlDragObject,dojo.dnd.DragObject);
dojo.lang.extend(dojo.dnd.HtmlDragObject,{dragClass:"",opacity:0.5,createIframe:true,disableX:false,disableY:false,createDragNode:function(){
var node=this.domNode.cloneNode(true);
if(this.dragClass){
dojo.html.addClass(node,this.dragClass);
}
if(this.opacity<1){
dojo.style.setOpacity(node,this.opacity);
}
if(dojo.render.html.ie&&this.createIframe){
with(node.style){
top="0px";
left="0px";
}
var _7e8=document.createElement("div");
_7e8.appendChild(node);
this.bgIframe=new dojo.html.BackgroundIframe(_7e8);
_7e8.appendChild(this.bgIframe.iframe);
node=_7e8;
}
node.style.zIndex=999;
return node;
},onDragStart:function(e){
dojo.html.clearSelection();
this.scrollOffset=dojo.html.getScrollOffset();
this.dragStartPosition=dojo.style.getAbsolutePosition(this.domNode,true);
this.dragOffset={y:this.dragStartPosition.y-e.pageY,x:this.dragStartPosition.x-e.pageX};
this.dragClone=this.createDragNode();
if((this.domNode.parentNode.nodeName.toLowerCase()=="body")||(dojo.style.getComputedStyle(this.domNode.parentNode,"position")=="static")){
this.parentPosition={y:0,x:0};
}else{
this.parentPosition=dojo.style.getAbsolutePosition(this.domNode.parentNode,true);
}
if(this.constrainToContainer){
this.constraints=this.getConstraints();
}
with(this.dragClone.style){
position="absolute";
top=this.dragOffset.y+e.pageY+"px";
left=this.dragOffset.x+e.pageX+"px";
}
document.body.appendChild(this.dragClone);
},getConstraints:function(){
if(this.constrainingContainer.nodeName.toLowerCase()=="body"){
width=dojo.html.getViewportWidth();
height=dojo.html.getViewportHeight();
padLeft=0;
padTop=0;
}else{
width=dojo.style.getContentWidth(this.constrainingContainer);
height=dojo.style.getContentHeight(this.constrainingContainer);
padLeft=dojo.style.getPixelValue(this.constrainingContainer,"padding-left",true);
padTop=dojo.style.getPixelValue(this.constrainingContainer,"padding-top",true);
}
return {minX:padLeft,minY:padTop,maxX:padLeft+width-dojo.style.getOuterWidth(this.domNode),maxY:padTop+height-dojo.style.getOuterHeight(this.domNode)};
},updateDragOffset:function(){
var _7ea=dojo.html.getScrollOffset();
if(_7ea.y!=this.scrollOffset.y){
var diff=_7ea.y-this.scrollOffset.y;
this.dragOffset.y+=diff;
this.scrollOffset.y=_7ea.y;
}
if(_7ea.x!=this.scrollOffset.x){
var diff=_7ea.x-this.scrollOffset.x;
this.dragOffset.x+=diff;
this.scrollOffset.x=_7ea.x;
}
},onDragMove:function(e){
this.updateDragOffset();
var x=this.dragOffset.x+e.pageX;
var y=this.dragOffset.y+e.pageY;
if(this.constrainToContainer){
if(x<this.constraints.minX){
x=this.constraints.minX;
}
if(y<this.constraints.minY){
y=this.constraints.minY;
}
if(x>this.constraints.maxX){
x=this.constraints.maxX;
}
if(y>this.constraints.maxY){
y=this.constraints.maxY;
}
}
if(!this.disableY){
this.dragClone.style.top=y+"px";
}
if(!this.disableX){
this.dragClone.style.left=x+"px";
}
},onDragEnd:function(e){
switch(e.dragStatus){
case "dropSuccess":
dojo.dom.removeNode(this.dragClone);
this.dragClone=null;
break;
case "dropFailure":
var _7f0=dojo.style.getAbsolutePosition(this.dragClone,true);
var _7f1=[this.dragStartPosition.x+1,this.dragStartPosition.y+1];
var line=new dojo.lfx.Line(_7f0,_7f1);
var anim=new dojo.lfx.Animation(500,line,dojo.lfx.easeOut);
var _7f4=this;
dojo.event.connect(anim,"onAnimate",function(e){
_7f4.dragClone.style.left=e[0]+"px";
_7f4.dragClone.style.top=e[1]+"px";
});
dojo.event.connect(anim,"onEnd",function(e){
dojo.lang.setTimeout(dojo.dom.removeNode,200,_7f4.dragClone);
});
anim.play();
break;
}
dojo.event.connect(this.domNode,"onclick",this,"squelchOnClick");
},squelchOnClick:function(e){
e.preventDefault();
dojo.event.disconnect(this.domNode,"onclick",this,"squelchOnClick");
},constrainTo:function(_7f8){
this.constrainToContainer=true;
if(_7f8){
this.constrainingContainer=_7f8;
}else{
this.constrainingContainer=this.domNode.parentNode;
}
}});
dojo.dnd.HtmlDropTarget=function(node,_7fa){
if(arguments.length==0){
return;
}
this.domNode=dojo.byId(node);
dojo.dnd.DropTarget.call(this);
if(_7fa&&dojo.lang.isString(_7fa)){
_7fa=[_7fa];
}
this.acceptedTypes=_7fa||[];
};
dojo.inherits(dojo.dnd.HtmlDropTarget,dojo.dnd.DropTarget);
dojo.lang.extend(dojo.dnd.HtmlDropTarget,{onDragOver:function(e){
if(!this.accepts(e.dragObjects)){
return false;
}
this.childBoxes=[];
for(var i=0,child;i<this.domNode.childNodes.length;i++){
child=this.domNode.childNodes[i];
if(child.nodeType!=dojo.dom.ELEMENT_NODE){
continue;
}
var pos=dojo.style.getAbsolutePosition(child,true);
var _7fe=dojo.style.getInnerHeight(child);
var _7ff=dojo.style.getInnerWidth(child);
this.childBoxes.push({top:pos.y,bottom:pos.y+_7fe,left:pos.x,right:pos.x+_7ff,node:child});
}
return true;
},_getNodeUnderMouse:function(e){
for(var i=0,child;i<this.childBoxes.length;i++){
with(this.childBoxes[i]){
if(e.pageX>=left&&e.pageX<=right&&e.pageY>=top&&e.pageY<=bottom){
return i;
}
}
}
return -1;
},createDropIndicator:function(){
this.dropIndicator=document.createElement("div");
with(this.dropIndicator.style){
position="absolute";
zIndex=999;
borderTopWidth="1px";
borderTopColor="black";
borderTopStyle="solid";
width=dojo.style.getInnerWidth(this.domNode)+"px";
left=dojo.style.getAbsoluteX(this.domNode,true)+"px";
}
},onDragMove:function(e,_803){
var i=this._getNodeUnderMouse(e);
if(!this.dropIndicator){
this.createDropIndicator();
}
if(i<0){
if(this.childBoxes.length){
var _805=(dojo.html.gravity(this.childBoxes[0].node,e)&dojo.html.gravity.NORTH);
}else{
var _805=true;
}
}else{
var _806=this.childBoxes[i];
var _805=(dojo.html.gravity(_806.node,e)&dojo.html.gravity.NORTH);
}
this.placeIndicator(e,_803,i,_805);
if(!dojo.html.hasParent(this.dropIndicator)){
document.body.appendChild(this.dropIndicator);
}
},placeIndicator:function(e,_808,_809,_80a){
with(this.dropIndicator.style){
if(_809<0){
if(this.childBoxes.length){
top=(_80a?this.childBoxes[0].top:this.childBoxes[this.childBoxes.length-1].bottom)+"px";
}else{
top=dojo.style.getAbsoluteY(this.domNode,true)+"px";
}
}else{
var _80b=this.childBoxes[_809];
top=(_80a?_80b.top:_80b.bottom)+"px";
}
}
},onDragOut:function(e){
if(this.dropIndicator){
dojo.dom.removeNode(this.dropIndicator);
delete this.dropIndicator;
}
},onDrop:function(e){
this.onDragOut(e);
var i=this._getNodeUnderMouse(e);
if(i<0){
if(this.childBoxes.length){
if(dojo.html.gravity(this.childBoxes[0].node,e)&dojo.html.gravity.NORTH){
return this.insert(e,this.childBoxes[0].node,"before");
}else{
return this.insert(e,this.childBoxes[this.childBoxes.length-1].node,"after");
}
}
return this.insert(e,this.domNode,"append");
}
var _80f=this.childBoxes[i];
if(dojo.html.gravity(_80f.node,e)&dojo.html.gravity.NORTH){
return this.insert(e,_80f.node,"before");
}else{
return this.insert(e,_80f.node,"after");
}
},insert:function(e,_811,_812){
var node=e.dragObject.domNode;
if(_812=="before"){
return dojo.html.insertBefore(node,_811);
}else{
if(_812=="after"){
return dojo.html.insertAfter(node,_811);
}else{
if(_812=="append"){
_811.appendChild(node);
return true;
}
}
}
return false;
}});
dojo.kwCompoundRequire({common:["dojo.dnd.DragAndDrop"],browser:["dojo.dnd.HtmlDragAndDrop"],dashboard:["dojo.dnd.HtmlDragAndDrop"]});
dojo.provide("dojo.dnd.*");
dojo.provide("dojo.widget.Manager");
dojo.require("dojo.lang.array");
dojo.require("dojo.lang.func");
dojo.require("dojo.event.*");
dojo.widget.manager=new function(){
this.widgets=[];
this.widgetIds=[];
this.topWidgets={};
var _814={};
var _815=[];
this.getUniqueId=function(_816){
return _816+"_"+(_814[_816]!=undefined?++_814[_816]:_814[_816]=0);
};
this.add=function(_817){
dojo.profile.start("dojo.widget.manager.add");
this.widgets.push(_817);
if(!_817.extraArgs["id"]){
_817.extraArgs["id"]=_817.extraArgs["ID"];
}
if(_817.widgetId==""){
if(_817["id"]){
_817.widgetId=_817["id"];
}else{
if(_817.extraArgs["id"]){
_817.widgetId=_817.extraArgs["id"];
}else{
_817.widgetId=this.getUniqueId(_817.widgetType);
}
}
}
if(this.widgetIds[_817.widgetId]){
dojo.debug("widget ID collision on ID: "+_817.widgetId);
}
this.widgetIds[_817.widgetId]=_817;
dojo.profile.end("dojo.widget.manager.add");
};
this.destroyAll=function(){
for(var x=this.widgets.length-1;x>=0;x--){
try{
this.widgets[x].destroy(true);
delete this.widgets[x];
}
catch(e){
}
}
};
this.remove=function(_819){
var tw=this.widgets[_819].widgetId;
delete this.widgetIds[tw];
this.widgets.splice(_819,1);
};
this.removeById=function(id){
for(var i=0;i<this.widgets.length;i++){
if(this.widgets[i].widgetId==id){
this.remove(i);
break;
}
}
};
this.getWidgetById=function(id){
return this.widgetIds[id];
};
this.getWidgetsByType=function(type){
var lt=type.toLowerCase();
var ret=[];
dojo.lang.forEach(this.widgets,function(x){
if(x.widgetType.toLowerCase()==lt){
ret.push(x);
}
});
return ret;
};
this.getWidgetsOfType=function(id){
dojo.deprecated("getWidgetsOfType is depecrecated, use getWidgetsByType");
return dojo.widget.manager.getWidgetsByType(id);
};
this.getWidgetsByFilter=function(_823,_824){
var ret=[];
dojo.lang.every(this.widgets,function(x){
if(_823(x)){
ret.push(x);
if(_824){
return false;
}
}
return true;
});
return (_824?ret[0]:ret);
};
this.getAllWidgets=function(){
return this.widgets.concat();
};
this.getWidgetByNode=function(node){
var w=this.getAllWidgets();
for(var i=0;i<w.length;i++){
if(w[i].domNode==node){
return w[i];
}
}
return null;
};
this.byId=this.getWidgetById;
this.byType=this.getWidgetsByType;
this.byFilter=this.getWidgetsByFilter;
this.byNode=this.getWidgetByNode;
var _82a={};
var _82b=["dojo.widget"];
for(var i=0;i<_82b.length;i++){
_82b[_82b[i]]=true;
}
this.registerWidgetPackage=function(_82d){
if(!_82b[_82d]){
_82b[_82d]=true;
_82b.push(_82d);
}
};
this.getWidgetPackageList=function(){
return dojo.lang.map(_82b,function(elt){
return (elt!==true?elt:undefined);
});
};
this.getImplementation=function(_82f,_830,_831){
var impl=this.getImplementationName(_82f);
if(impl){
var ret=new impl(_830);
return ret;
}
};
this.getImplementationName=function(_834){
var _835=_834.toLowerCase();
var impl=_82a[_835];
if(impl){
return impl;
}
if(!_815.length){
for(var _837 in dojo.render){
if(dojo.render[_837]["capable"]===true){
var _838=dojo.render[_837].prefixes;
for(var i=0;i<_838.length;i++){
_815.push(_838[i].toLowerCase());
}
}
}
_815.push("");
}
for(var i=0;i<_82b.length;i++){
var _83a=dojo.evalObjPath(_82b[i]);
if(!_83a){
continue;
}
for(var j=0;j<_815.length;j++){
if(!_83a[_815[j]]){
continue;
}
for(var _83c in _83a[_815[j]]){
if(_83c.toLowerCase()!=_835){
continue;
}
_82a[_835]=_83a[_815[j]][_83c];
return _82a[_835];
}
}
for(var j=0;j<_815.length;j++){
for(var _83c in _83a){
if(_83c.toLowerCase()!=(_815[j]+_835)){
continue;
}
_82a[_835]=_83a[_83c];
return _82a[_835];
}
}
}
throw new Error("Could not locate \""+_834+"\" class");
};
this.resizing=false;
this.onWindowResized=function(){
if(this.resizing){
return;
}
try{
this.resizing=true;
for(var id in this.topWidgets){
var _83e=this.topWidgets[id];
if(_83e.onParentResized){
_83e.onParentResized();
}
}
}
catch(e){
}
finally{
this.resizing=false;
}
};
if(typeof window!="undefined"){
dojo.addOnLoad(this,"onWindowResized");
dojo.event.connect(window,"onresize",this,"onWindowResized");
}
};
(function(){
var dw=dojo.widget;
var dwm=dw.manager;
var h=dojo.lang.curry(dojo.lang,"hitch",dwm);
var g=function(_843,_844){
dw[(_844||_843)]=h(_843);
};
g("add","addWidget");
g("destroyAll","destroyAllWidgets");
g("remove","removeWidget");
g("removeById","removeWidgetById");
g("getWidgetById");
g("getWidgetById","byId");
g("getWidgetsByType");
g("getWidgetsByFilter");
g("getWidgetsByType","byType");
g("getWidgetsByFilter","byFilter");
g("getWidgetByNode","byNode");
dw.all=function(n){
var _846=dwm.getAllWidgets.apply(dwm,arguments);
if(arguments.length>0){
return _846[n];
}
return _846;
};
g("registerWidgetPackage");
g("getImplementation","getWidgetImplementation");
g("getImplementationName","getWidgetImplementationName");
dw.widgets=dwm.widgets;
dw.widgetIds=dwm.widgetIds;
dw.root=dwm.root;
})();
dojo.provide("dojo.widget.Widget");
dojo.provide("dojo.widget.tags");
dojo.require("dojo.lang.func");
dojo.require("dojo.lang.array");
dojo.require("dojo.lang.extras");
dojo.require("dojo.lang.declare");
dojo.require("dojo.widget.Manager");
dojo.require("dojo.event.*");
dojo.declare("dojo.widget.Widget",null,{initializer:function(){
this.children=[];
this.extraArgs={};
},parent:null,isTopLevel:false,isModal:false,isEnabled:true,isHidden:false,isContainer:false,widgetId:"",widgetType:"Widget",toString:function(){
return "[Widget "+this.widgetType+", "+(this.widgetId||"NO ID")+"]";
},repr:function(){
return this.toString();
},enable:function(){
this.isEnabled=true;
},disable:function(){
this.isEnabled=false;
},hide:function(){
this.isHidden=true;
},show:function(){
this.isHidden=false;
},onResized:function(){
this.notifyChildrenOfResize();
},notifyChildrenOfResize:function(){
for(var i=0;i<this.children.length;i++){
var _848=this.children[i];
if(_848.onResized){
_848.onResized();
}
}
},create:function(args,_84a,_84b){
this.satisfyPropertySets(args,_84a,_84b);
this.mixInProperties(args,_84a,_84b);
this.postMixInProperties(args,_84a,_84b);
dojo.widget.manager.add(this);
this.buildRendering(args,_84a,_84b);
this.initialize(args,_84a,_84b);
this.postInitialize(args,_84a,_84b);
this.postCreate(args,_84a,_84b);
return this;
},destroy:function(_84c){
this.destroyChildren();
this.uninitialize();
this.destroyRendering(_84c);
dojo.widget.manager.removeById(this.widgetId);
},destroyChildren:function(){
while(this.children.length>0){
var tc=this.children[0];
this.removeChild(tc);
tc.destroy();
}
},getChildrenOfType:function(type,_84f){
var ret=[];
var _851=dojo.lang.isFunction(type);
if(!_851){
type=type.toLowerCase();
}
for(var x=0;x<this.children.length;x++){
if(_851){
if(this.children[x] instanceof type){
ret.push(this.children[x]);
}
}else{
if(this.children[x].widgetType.toLowerCase()==type){
ret.push(this.children[x]);
}
}
if(_84f){
ret=ret.concat(this.children[x].getChildrenOfType(type,_84f));
}
}
return ret;
},getDescendants:function(){
var _853=[];
var _854=[this];
var elem;
while(elem=_854.pop()){
_853.push(elem);
dojo.lang.forEach(elem.children,function(elem){
_854.push(elem);
});
}
return _853;
},satisfyPropertySets:function(args){
return args;
},mixInProperties:function(args,frag){
if((args["fastMixIn"])||(frag["fastMixIn"])){
for(var x in args){
this[x]=args[x];
}
return;
}
var _85b;
var _85c=dojo.widget.lcArgsCache[this.widgetType];
if(_85c==null){
_85c={};
for(var y in this){
_85c[((new String(y)).toLowerCase())]=y;
}
dojo.widget.lcArgsCache[this.widgetType]=_85c;
}
var _85e={};
for(var x in args){
if(!this[x]){
var y=_85c[(new String(x)).toLowerCase()];
if(y){
args[y]=args[x];
x=y;
}
}
if(_85e[x]){
continue;
}
_85e[x]=true;
if((typeof this[x])!=(typeof _85b)){
if(typeof args[x]!="string"){
this[x]=args[x];
}else{
if(dojo.lang.isString(this[x])){
this[x]=args[x];
}else{
if(dojo.lang.isNumber(this[x])){
this[x]=new Number(args[x]);
}else{
if(dojo.lang.isBoolean(this[x])){
this[x]=(args[x].toLowerCase()=="false")?false:true;
}else{
if(dojo.lang.isFunction(this[x])){
if(args[x].search(/[^\w\.]+/i)==-1){
this[x]=dojo.evalObjPath(args[x],false);
}else{
var tn=dojo.lang.nameAnonFunc(new Function(args[x]),this);
dojo.event.connect(this,x,this,tn);
}
}else{
if(dojo.lang.isArray(this[x])){
this[x]=args[x].split(";");
}else{
if(this[x] instanceof Date){
this[x]=new Date(Number(args[x]));
}else{
if(typeof this[x]=="object"){
if(this[x] instanceof dojo.uri.Uri){
this[x]=args[x];
}else{
var _860=args[x].split(";");
for(var y=0;y<_860.length;y++){
var si=_860[y].indexOf(":");
if((si!=-1)&&(_860[y].length>si)){
this[x][_860[y].substr(0,si).replace(/^\s+|\s+$/g,"")]=_860[y].substr(si+1);
}
}
}
}else{
this[x]=args[x];
}
}
}
}
}
}
}
}
}else{
this.extraArgs[x.toLowerCase()]=args[x];
}
}
},postMixInProperties:function(){
},initialize:function(args,frag){
return false;
},postInitialize:function(args,frag){
return false;
},postCreate:function(args,frag){
return false;
},uninitialize:function(){
return false;
},buildRendering:function(){
dojo.unimplemented("dojo.widget.Widget.buildRendering, on "+this.toString()+", ");
return false;
},destroyRendering:function(){
dojo.unimplemented("dojo.widget.Widget.destroyRendering");
return false;
},cleanUp:function(){
dojo.unimplemented("dojo.widget.Widget.cleanUp");
return false;
},addedTo:function(_868){
},addChild:function(_869){
dojo.unimplemented("dojo.widget.Widget.addChild");
return false;
},removeChild:function(_86a){
for(var x=0;x<this.children.length;x++){
if(this.children[x]===_86a){
this.children.splice(x,1);
break;
}
}
return _86a;
},resize:function(_86c,_86d){
this.setWidth(_86c);
this.setHeight(_86d);
},setWidth:function(_86e){
if((typeof _86e=="string")&&(_86e.substr(-1)=="%")){
this.setPercentageWidth(_86e);
}else{
this.setNativeWidth(_86e);
}
},setHeight:function(_86f){
if((typeof _86f=="string")&&(_86f.substr(-1)=="%")){
this.setPercentageHeight(_86f);
}else{
this.setNativeHeight(_86f);
}
},setPercentageHeight:function(_870){
return false;
},setNativeHeight:function(_871){
return false;
},setPercentageWidth:function(_872){
return false;
},setNativeWidth:function(_873){
return false;
},getPreviousSibling:function(){
var idx=this.getParentIndex();
if(idx<=0){
return null;
}
return this.getSiblings()[idx-1];
},getSiblings:function(){
return this.parent.children;
},getParentIndex:function(){
return dojo.lang.indexOf(this.getSiblings(),this,true);
},getNextSibling:function(){
var idx=this.getParentIndex();
if(idx==this.getSiblings().length-1){
return null;
}
if(idx<0){
return null;
}
return this.getSiblings()[idx+1];
}});
dojo.widget.lcArgsCache={};
dojo.widget.tags={};
dojo.widget.tags.addParseTreeHandler=function(type){
var _877=type.toLowerCase();
this[_877]=function(_878,_879,_87a,_87b,_87c){
return dojo.widget.buildWidgetFromParseTree(_877,_878,_879,_87a,_87b,_87c);
};
};
dojo.widget.tags.addParseTreeHandler("dojo:widget");
dojo.widget.tags["dojo:propertyset"]=function(_87d,_87e,_87f){
var _880=_87e.parseProperties(_87d["dojo:propertyset"]);
};
dojo.widget.tags["dojo:connect"]=function(_881,_882,_883){
var _884=_882.parseProperties(_881["dojo:connect"]);
};
dojo.widget.buildWidgetFromParseTree=function(type,frag,_887,_888,_889,_88a){
var _88b=type.split(":");
_88b=(_88b.length==2)?_88b[1]:type;
var _88c=_88a||_887.parseProperties(frag["dojo:"+_88b]);
var _88d=dojo.widget.manager.getImplementation(_88b);
if(!_88d){
throw new Error("cannot find \""+_88b+"\" widget");
}else{
if(!_88d.create){
throw new Error("\""+_88b+"\" widget object does not appear to implement *Widget");
}
}
_88c["dojoinsertionindex"]=_889;
var ret=_88d.create(_88c,frag,_888);
return ret;
};
dojo.widget.defineWidget=function(_88f,_890,_891,_892,ctor){
var _894=_88f.split(".");
var type=_894.pop();
if(_892){
while((_894.length)&&(_894.pop()!=_892)){
}
}
_894=_894.join(".");
dojo.widget.manager.registerWidgetPackage(_894);
dojo.widget.tags.addParseTreeHandler("dojo:"+type.toLowerCase());
if(!_891){
_891={};
}
_891.widgetType=type;
if((!ctor)&&(_891["classConstructor"])){
ctor=_891.classConstructor;
delete _891.classConstructor;
}
dojo.declare(_88f,_890,_891,ctor);
};
dojo.provide("dojo.widget.Parse");
dojo.require("dojo.widget.Manager");
dojo.require("dojo.dom");
dojo.widget.Parse=function(_896){
this.propertySetsList=[];
this.fragment=_896;
this.createComponents=function(frag,_898){
var _899=[];
var _89a=false;
try{
if((frag)&&(frag["tagName"])&&(frag!=frag["nodeRef"])){
var _89b=dojo.widget.tags;
var tna=String(frag["tagName"]).split(";");
for(var x=0;x<tna.length;x++){
var ltn=(tna[x].replace(/^\s+|\s+$/g,"")).toLowerCase();
if(_89b[ltn]){
_89a=true;
frag.tagName=ltn;
var ret=_89b[ltn](frag,this,_898,frag["index"]);
_899.push(ret);
}else{
if((dojo.lang.isString(ltn))&&(ltn.substr(0,5)=="dojo:")){
dojo.debug("no tag handler registed for type: ",ltn);
}
}
}
}
}
catch(e){
dojo.debug("dojo.widget.Parse: error:",e);
}
if(!_89a){
_899=_899.concat(this.createSubComponents(frag,_898));
}
return _899;
};
this.createSubComponents=function(_8a0,_8a1){
var frag,comps=[];
for(var item in _8a0){
frag=_8a0[item];
if((frag)&&(typeof frag=="object")&&(frag!=_8a0.nodeRef)&&(frag!=_8a0["tagName"])){
comps=comps.concat(this.createComponents(frag,_8a1));
}
}
return comps;
};
this.parsePropertySets=function(_8a4){
return [];
var _8a5=[];
for(var item in _8a4){
if((_8a4[item]["tagName"]=="dojo:propertyset")){
_8a5.push(_8a4[item]);
}
}
this.propertySetsList.push(_8a5);
return _8a5;
};
this.parseProperties=function(_8a7){
var _8a8={};
for(var item in _8a7){
if((_8a7[item]==_8a7["tagName"])||(_8a7[item]==_8a7.nodeRef)){
}else{
if((_8a7[item]["tagName"])&&(dojo.widget.tags[_8a7[item].tagName.toLowerCase()])){
}else{
if((_8a7[item][0])&&(_8a7[item][0].value!="")&&(_8a7[item][0].value!=null)){
try{
if(item.toLowerCase()=="dataprovider"){
var _8aa=this;
this.getDataProvider(_8aa,_8a7[item][0].value);
_8a8.dataProvider=this.dataProvider;
}
_8a8[item]=_8a7[item][0].value;
var _8ab=this.parseProperties(_8a7[item]);
for(var _8ac in _8ab){
_8a8[_8ac]=_8ab[_8ac];
}
}
catch(e){
dojo.debug(e);
}
}
}
}
}
return _8a8;
};
this.getDataProvider=function(_8ad,_8ae){
dojo.io.bind({url:_8ae,load:function(type,_8b0){
if(type=="load"){
_8ad.dataProvider=_8b0;
}
},mimetype:"text/javascript",sync:true});
};
this.getPropertySetById=function(_8b1){
for(var x=0;x<this.propertySetsList.length;x++){
if(_8b1==this.propertySetsList[x]["id"][0].value){
return this.propertySetsList[x];
}
}
return "";
};
this.getPropertySetsByType=function(_8b3){
var _8b4=[];
for(var x=0;x<this.propertySetsList.length;x++){
var cpl=this.propertySetsList[x];
var cpcc=cpl["componentClass"]||cpl["componentType"]||null;
if((cpcc)&&(propertySetId==cpcc[0].value)){
_8b4.push(cpl);
}
}
return _8b4;
};
this.getPropertySets=function(_8b8){
var ppl="dojo:propertyproviderlist";
var _8ba=[];
var _8bb=_8b8["tagName"];
if(_8b8[ppl]){
var _8bc=_8b8[ppl].value.split(" ");
for(propertySetId in _8bc){
if((propertySetId.indexOf("..")==-1)&&(propertySetId.indexOf("://")==-1)){
var _8bd=this.getPropertySetById(propertySetId);
if(_8bd!=""){
_8ba.push(_8bd);
}
}else{
}
}
}
return (this.getPropertySetsByType(_8bb)).concat(_8ba);
};
this.createComponentFromScript=function(_8be,_8bf,_8c0){
var ltn="dojo:"+_8bf.toLowerCase();
if(dojo.widget.tags[ltn]){
_8c0.fastMixIn=true;
return [dojo.widget.tags[ltn](_8c0,this,null,null,_8c0)];
}else{
if(ltn.substr(0,5)=="dojo:"){
dojo.debug("no tag handler registed for type: ",ltn);
}
}
};
};
dojo.widget._parser_collection={"dojo":new dojo.widget.Parse()};
dojo.widget.getParser=function(name){
if(!name){
name="dojo";
}
if(!this._parser_collection[name]){
this._parser_collection[name]=new dojo.widget.Parse();
}
return this._parser_collection[name];
};
dojo.widget.createWidget=function(name,_8c4,_8c5,_8c6){
var _8c7=name.toLowerCase();
var _8c8="dojo:"+_8c7;
var _8c9=(dojo.byId(name)&&(!dojo.widget.tags[_8c8]));
if((arguments.length==1)&&((typeof name!="string")||(_8c9))){
var xp=new dojo.xml.Parse();
var tn=(_8c9)?dojo.byId(name):name;
return dojo.widget.getParser().createComponents(xp.parseElement(tn,null,true))[0];
}
function fromScript(_8cc,name,_8ce){
_8ce[_8c8]={dojotype:[{value:_8c7}],nodeRef:_8cc,fastMixIn:true};
return dojo.widget.getParser().createComponentFromScript(_8cc,name,_8ce,true);
}
if(typeof name!="string"&&typeof _8c4=="string"){
dojo.deprecated("dojo.widget.createWidget","argument order is now of the form "+"dojo.widget.createWidget(NAME, [PROPERTIES, [REFERENCENODE, [POSITION]]])");
return fromScript(name,_8c4,_8c5);
}
_8c4=_8c4||{};
var _8cf=false;
var tn=null;
var h=dojo.render.html.capable;
if(h){
tn=document.createElement("span");
}
if(!_8c5){
_8cf=true;
_8c5=tn;
if(h){
document.body.appendChild(_8c5);
}
}else{
if(_8c6){
dojo.dom.insertAtPosition(tn,_8c5,_8c6);
}else{
tn=_8c5;
}
}
var _8d1=fromScript(tn,name,_8c4);
if(!_8d1[0]||typeof _8d1[0].widgetType=="undefined"){
throw new Error("createWidget: Creation of \""+name+"\" widget failed.");
}
if(_8cf){
if(_8d1[0].domNode.parentNode){
_8d1[0].domNode.parentNode.removeChild(_8d1[0].domNode);
}
}
return _8d1[0];
};
dojo.widget.fromScript=function(name,_8d3,_8d4,_8d5){
dojo.deprecated("dojo.widget.fromScript"," use "+"dojo.widget.createWidget instead");
return dojo.widget.createWidget(name,_8d3,_8d4,_8d5);
};
dojo.provide("dojo.widget.DomWidget");
dojo.require("dojo.event.*");
dojo.require("dojo.widget.Widget");
dojo.require("dojo.dom");
dojo.require("dojo.xml.Parse");
dojo.require("dojo.uri.*");
dojo.require("dojo.lang.func");
dojo.widget._cssFiles={};
dojo.widget._cssStrings={};
dojo.widget._templateCache={};
dojo.widget.defaultStrings={dojoRoot:dojo.hostenv.getBaseScriptUri(),baseScriptUri:dojo.hostenv.getBaseScriptUri()};
dojo.widget.buildFromTemplate=function(){
dojo.lang.forward("fillFromTemplateCache");
};
dojo.widget.fillFromTemplateCache=function(obj,_8d7,_8d8,_8d9,_8da){
var _8db=_8d7||obj.templatePath;
var _8dc=_8d8||obj.templateCssPath;
if(_8db&&!(_8db instanceof dojo.uri.Uri)){
_8db=dojo.uri.dojoUri(_8db);
dojo.deprecated("templatePath should be of type dojo.uri.Uri");
}
if(_8dc&&!(_8dc instanceof dojo.uri.Uri)){
_8dc=dojo.uri.dojoUri(_8dc);
dojo.deprecated("templateCssPath should be of type dojo.uri.Uri");
}
var _8dd=dojo.widget._templateCache;
if(!obj["widgetType"]){
do{
var _8de="__dummyTemplate__"+dojo.widget._templateCache.dummyCount++;
}while(_8dd[_8de]);
obj.widgetType=_8de;
}
var wt=obj.widgetType;
if((!obj.templateCssString)&&(_8dc)&&(!dojo.widget._cssFiles[_8dc])){
obj.templateCssString=dojo.hostenv.getText(_8dc);
obj.templateCssPath=null;
dojo.widget._cssFiles[_8dc]=true;
}
if((obj["templateCssString"])&&(!obj.templateCssString["loaded"])){
dojo.style.insertCssText(obj.templateCssString,null,_8dc);
if(!obj.templateCssString){
obj.templateCssString="";
}
obj.templateCssString.loaded=true;
}
var ts=_8dd[wt];
if(!ts){
_8dd[wt]={"string":null,"node":null};
if(_8da){
ts={};
}else{
ts=_8dd[wt];
}
}
if(!obj.templateString){
obj.templateString=_8d9||ts["string"];
}
if(!obj.templateNode){
obj.templateNode=ts["node"];
}
if((!obj.templateNode)&&(!obj.templateString)&&(_8db)){
var _8e1=dojo.hostenv.getText(_8db);
if(_8e1){
var _8e2=_8e1.match(/<body[^>]*>\s*([\s\S]+)\s*<\/body>/im);
if(_8e2){
_8e1=_8e2[1];
}
}else{
_8e1="";
}
obj.templateString=_8e1;
if(!_8da){
_8dd[wt]["string"]=_8e1;
}
}
if((!ts["string"])&&(!_8da)){
ts.string=obj.templateString;
}
};
dojo.widget._templateCache.dummyCount=0;
dojo.widget.attachProperties=["dojoAttachPoint","id"];
dojo.widget.eventAttachProperty="dojoAttachEvent";
dojo.widget.onBuildProperty="dojoOnBuild";
dojo.widget.attachTemplateNodes=function(_8e3,_8e4,_8e5){
var _8e6=dojo.dom.ELEMENT_NODE;
function trim(str){
return str.replace(/^\s+|\s+$/g,"");
}
if(!_8e3){
_8e3=_8e4.domNode;
}
if(_8e3.nodeType!=_8e6){
return;
}
var _8e8=_8e3.all||_8e3.getElementsByTagName("*");
var _8e9=_8e4;
for(var x=-1;x<_8e8.length;x++){
var _8eb=(x==-1)?_8e3:_8e8[x];
var _8ec=[];
for(var y=0;y<this.attachProperties.length;y++){
var _8ee=_8eb.getAttribute(this.attachProperties[y]);
if(_8ee){
_8ec=_8ee.split(";");
for(var z=0;z<_8ec.length;z++){
if(dojo.lang.isArray(_8e4[_8ec[z]])){
_8e4[_8ec[z]].push(_8eb);
}else{
_8e4[_8ec[z]]=_8eb;
}
}
break;
}
}
var _8f0=_8eb.getAttribute(this.templateProperty);
if(_8f0){
_8e4[_8f0]=_8eb;
}
var _8f1=_8eb.getAttribute(this.eventAttachProperty);
if(_8f1){
var evts=_8f1.split(";");
for(var y=0;y<evts.length;y++){
if((!evts[y])||(!evts[y].length)){
continue;
}
var _8f3=null;
var tevt=trim(evts[y]);
if(evts[y].indexOf(":")>=0){
var _8f5=tevt.split(":");
tevt=trim(_8f5[0]);
_8f3=trim(_8f5[1]);
}
if(!_8f3){
_8f3=tevt;
}
var tf=function(){
var ntf=new String(_8f3);
return function(evt){
if(_8e9[ntf]){
_8e9[ntf](dojo.event.browser.fixEvent(evt,this));
}
};
}();
dojo.event.browser.addListener(_8eb,tevt,tf,false,true);
}
}
for(var y=0;y<_8e5.length;y++){
var _8f9=_8eb.getAttribute(_8e5[y]);
if((_8f9)&&(_8f9.length)){
var _8f3=null;
var _8fa=_8e5[y].substr(4);
_8f3=trim(_8f9);
var _8fb=[_8f3];
if(_8f3.indexOf(";")>=0){
_8fb=dojo.lang.map(_8f3.split(";"),trim);
}
for(var z=0;z<_8fb.length;z++){
if(!_8fb[z].length){
continue;
}
var tf=function(){
var ntf=new String(_8fb[z]);
return function(evt){
if(_8e9[ntf]){
_8e9[ntf](dojo.event.browser.fixEvent(evt,this));
}
};
}();
dojo.event.browser.addListener(_8eb,_8fa,tf,false,true);
}
}
}
var _8fe=_8eb.getAttribute(this.onBuildProperty);
if(_8fe){
eval("var node = baseNode; var widget = targetObj; "+_8fe);
}
}
};
dojo.widget.getDojoEventsFromStr=function(str){
var re=/(dojoOn([a-z]+)(\s?))=/gi;
var evts=str?str.match(re)||[]:[];
var ret=[];
var lem={};
for(var x=0;x<evts.length;x++){
if(evts[x].legth<1){
continue;
}
var cm=evts[x].replace(/\s/,"");
cm=(cm.slice(0,cm.length-1));
if(!lem[cm]){
lem[cm]=true;
ret.push(cm);
}
}
return ret;
};
dojo.declare("dojo.widget.DomWidget",dojo.widget.Widget,{initializer:function(){
if((arguments.length>0)&&(typeof arguments[0]=="object")){
this.create(arguments[0]);
}
},templateNode:null,templateString:null,templateCssString:null,preventClobber:false,domNode:null,containerNode:null,addChild:function(_906,_907,pos,ref,_90a){
if(!this.isContainer){
dojo.debug("dojo.widget.DomWidget.addChild() attempted on non-container widget");
return null;
}else{
this.addWidgetAsDirectChild(_906,_907,pos,ref,_90a);
this.registerChild(_906,_90a);
}
return _906;
},addWidgetAsDirectChild:function(_90b,_90c,pos,ref,_90f){
if((!this.containerNode)&&(!_90c)){
this.containerNode=this.domNode;
}
var cn=(_90c)?_90c:this.containerNode;
if(!pos){
pos="after";
}
if(!ref){
if(!cn){
cn=document.body;
}
ref=cn.lastChild;
}
if(!_90f){
_90f=0;
}
_90b.domNode.setAttribute("dojoinsertionindex",_90f);
if(!ref){
cn.appendChild(_90b.domNode);
}else{
if(pos=="insertAtIndex"){
dojo.dom.insertAtIndex(_90b.domNode,ref.parentNode,_90f);
}else{
if((pos=="after")&&(ref===cn.lastChild)){
cn.appendChild(_90b.domNode);
}else{
dojo.dom.insertAtPosition(_90b.domNode,cn,pos);
}
}
}
},registerChild:function(_911,_912){
_911.dojoInsertionIndex=_912;
var idx=-1;
for(var i=0;i<this.children.length;i++){
if(this.children[i].dojoInsertionIndex<_912){
idx=i;
}
}
this.children.splice(idx+1,0,_911);
_911.parent=this;
_911.addedTo(this);
delete dojo.widget.manager.topWidgets[_911.widgetId];
},removeChild:function(_915){
dojo.dom.removeNode(_915.domNode);
return dojo.widget.DomWidget.superclass.removeChild.call(this,_915);
},getFragNodeRef:function(frag){
if(!frag||!frag["dojo:"+this.widgetType.toLowerCase()]){
dojo.raise("Error: no frag for widget type "+this.widgetType+", id "+this.widgetId+" (maybe a widget has set it's type incorrectly)");
}
return (frag?frag["dojo:"+this.widgetType.toLowerCase()]["nodeRef"]:null);
},postInitialize:function(args,frag,_919){
var _91a=this.getFragNodeRef(frag);
if(_919&&(_919.snarfChildDomOutput||!_91a)){
_919.addWidgetAsDirectChild(this,"","insertAtIndex","",args["dojoinsertionindex"],_91a);
}else{
if(_91a){
if(this.domNode&&(this.domNode!==_91a)){
var _91b=_91a.parentNode.replaceChild(this.domNode,_91a);
}
}
}
if(_919){
_919.registerChild(this,args.dojoinsertionindex);
}else{
dojo.widget.manager.topWidgets[this.widgetId]=this;
}
if(this.isContainer){
var _91c=dojo.widget.getParser();
_91c.createSubComponents(frag,this);
}
},buildRendering:function(args,frag){
var ts=dojo.widget._templateCache[this.widgetType];
if((!this.preventClobber)&&((this.templatePath)||(this.templateNode)||((this["templateString"])&&(this.templateString.length))||((typeof ts!="undefined")&&((ts["string"])||(ts["node"]))))){
this.buildFromTemplate(args,frag);
}else{
this.domNode=this.getFragNodeRef(frag);
}
this.fillInTemplate(args,frag);
},buildFromTemplate:function(args,frag){
var _922=false;
if(args["templatecsspath"]){
args["templateCssPath"]=args["templatecsspath"];
}
if(args["templatepath"]){
_922=true;
args["templatePath"]=args["templatepath"];
}
dojo.widget.fillFromTemplateCache(this,args["templatePath"],args["templateCssPath"],null,_922);
var ts=dojo.widget._templateCache[this.widgetType];
if((ts)&&(!_922)){
if(!this.templateString.length){
this.templateString=ts["string"];
}
if(!this.templateNode){
this.templateNode=ts["node"];
}
}
var _924=false;
var node=null;
var tstr=this.templateString;
if((!this.templateNode)&&(this.templateString)){
_924=this.templateString.match(/\$\{([^\}]+)\}/g);
if(_924){
var hash=this.strings||{};
for(var key in dojo.widget.defaultStrings){
if(dojo.lang.isUndefined(hash[key])){
hash[key]=dojo.widget.defaultStrings[key];
}
}
for(var i=0;i<_924.length;i++){
var key=_924[i];
key=key.substring(2,key.length-1);
var kval=(key.substring(0,5)=="this.")?this[key.substring(5)]:hash[key];
var _92b;
if((kval)||(dojo.lang.isString(kval))){
_92b=(dojo.lang.isFunction(kval))?kval.call(this,key,this.templateString):kval;
tstr=tstr.replace(_924[i],_92b);
}
}
}else{
this.templateNode=this.createNodesFromText(this.templateString,true)[0];
ts.node=this.templateNode;
}
}
if((!this.templateNode)&&(!_924)){
dojo.debug("weren't able to create template!");
return false;
}else{
if(!_924){
node=this.templateNode.cloneNode(true);
if(!node){
return false;
}
}else{
node=this.createNodesFromText(tstr,true)[0];
}
}
this.domNode=node;
this.attachTemplateNodes(this.domNode,this);
if(this.isContainer&&this.containerNode){
var src=this.getFragNodeRef(frag);
if(src){
dojo.dom.moveChildren(src,this.containerNode);
}
}
},attachTemplateNodes:function(_92d,_92e){
if(!_92e){
_92e=this;
}
return dojo.widget.attachTemplateNodes(_92d,_92e,dojo.widget.getDojoEventsFromStr(this.templateString));
},fillInTemplate:function(){
},destroyRendering:function(){
try{
delete this.domNode;
}
catch(e){
}
},cleanUp:function(){
},getContainerHeight:function(){
dojo.unimplemented("dojo.widget.DomWidget.getContainerHeight");
},getContainerWidth:function(){
dojo.unimplemented("dojo.widget.DomWidget.getContainerWidth");
},createNodesFromText:function(){
dojo.unimplemented("dojo.widget.DomWidget.createNodesFromText");
}});
dojo.provide("dojo.lfx.toggle");
dojo.require("dojo.lfx.*");
dojo.lfx.toggle.plain={show:function(node,_930,_931,_932){
dojo.style.show(node);
if(dojo.lang.isFunction(_932)){
_932();
}
},hide:function(node,_934,_935,_936){
dojo.style.hide(node);
if(dojo.lang.isFunction(_936)){
_936();
}
}};
dojo.lfx.toggle.fade={show:function(node,_938,_939,_93a){
dojo.lfx.fadeShow(node,_938,_939,_93a).play();
},hide:function(node,_93c,_93d,_93e){
dojo.lfx.fadeHide(node,_93c,_93d,_93e).play();
}};
dojo.lfx.toggle.wipe={show:function(node,_940,_941,_942){
dojo.lfx.wipeIn(node,_940,_941,_942).play();
},hide:function(node,_944,_945,_946){
dojo.lfx.wipeOut(node,_944,_945,_946).play();
}};
dojo.lfx.toggle.explode={show:function(node,_948,_949,_94a,_94b){
dojo.lfx.explode(_94b||[0,0,0,0],node,_948,_949,_94a).play();
},hide:function(node,_94d,_94e,_94f,_950){
dojo.lfx.implode(node,_950||[0,0,0,0],_94d,_94e,_94f).play();
}};
dojo.provide("dojo.widget.HtmlWidget");
dojo.require("dojo.widget.DomWidget");
dojo.require("dojo.html");
dojo.require("dojo.html.extras");
dojo.require("dojo.lang.extras");
dojo.require("dojo.lang.func");
dojo.require("dojo.lfx.toggle");
dojo.declare("dojo.widget.HtmlWidget",dojo.widget.DomWidget,{widgetType:"HtmlWidget",templateCssPath:null,templatePath:null,toggle:"plain",toggleDuration:150,animationInProgress:false,initialize:function(args,frag){
},postMixInProperties:function(args,frag){
this.toggleObj=dojo.lfx.toggle[this.toggle.toLowerCase()]||dojo.lfx.toggle.plain;
},getContainerHeight:function(){
dojo.unimplemented("dojo.widget.HtmlWidget.getContainerHeight");
},getContainerWidth:function(){
return this.parent.domNode.offsetWidth;
},setNativeHeight:function(_955){
var ch=this.getContainerHeight();
},createNodesFromText:function(txt,wrap){
return dojo.html.createNodesFromText(txt,wrap);
},destroyRendering:function(_959){
try{
if(!_959){
dojo.event.browser.clean(this.domNode);
}
this.domNode.parentNode.removeChild(this.domNode);
delete this.domNode;
}
catch(e){
}
},isShowing:function(){
return dojo.style.isShowing(this.domNode);
},toggleShowing:function(){
if(this.isHidden){
this.show();
}else{
this.hide();
}
},show:function(){
this.animationInProgress=true;
this.isHidden=false;
this.toggleObj.show(this.domNode,this.toggleDuration,null,dojo.lang.hitch(this,this.onShow),this.explodeSrc);
},onShow:function(){
this.animationInProgress=false;
},hide:function(){
this.animationInProgress=true;
this.isHidden=true;
this.toggleObj.hide(this.domNode,this.toggleDuration,null,dojo.lang.hitch(this,this.onHide),this.explodeSrc);
},onHide:function(){
this.animationInProgress=false;
},_isResized:function(w,h){
if(!this.isShowing()){
return false;
}
w=w||dojo.style.getOuterWidth(this.domNode);
h=h||dojo.style.getOuterHeight(this.domNode);
if(this.width==w&&this.height==h){
return false;
}
this.width=w;
this.height=h;
return true;
},onParentResized:function(){
if(!this._isResized()){
return;
}
this.onResized();
},resizeTo:function(w,h){
if(!this._isResized(w,h)){
return;
}
dojo.style.setOuterWidth(this.domNode,w);
dojo.style.setOuterHeight(this.domNode,h);
this.onResized();
},resizeSoon:function(){
if(this.isShowing()){
dojo.lang.setTimeout(this,this.onResized,0);
}
},onResized:function(){
dojo.lang.forEach(this.children,function(_95e){
_95e.onParentResized();
});
}});
dojo.kwCompoundRequire({common:["dojo.xml.Parse","dojo.widget.Widget","dojo.widget.Parse","dojo.widget.Manager"],browser:["dojo.widget.DomWidget","dojo.widget.HtmlWidget"],dashboard:["dojo.widget.DomWidget","dojo.widget.HtmlWidget"],svg:["dojo.widget.SvgWidget"]});
dojo.provide("dojo.widget.*");
dojo.provide("dojo.math");
dojo.math.degToRad=function(x){
return (x*Math.PI)/180;
};
dojo.math.radToDeg=function(x){
return (x*180)/Math.PI;
};
dojo.math.factorial=function(n){
if(n<1){
return 0;
}
var _962=1;
for(var i=1;i<=n;i++){
_962*=i;
}
return _962;
};
dojo.math.permutations=function(n,k){
if(n==0||k==0){
return 1;
}
return (dojo.math.factorial(n)/dojo.math.factorial(n-k));
};
dojo.math.combinations=function(n,r){
if(n==0||r==0){
return 1;
}
return (dojo.math.factorial(n)/(dojo.math.factorial(n-r)*dojo.math.factorial(r)));
};
dojo.math.bernstein=function(t,n,i){
return (dojo.math.combinations(n,i)*Math.pow(t,i)*Math.pow(1-t,n-i));
};
dojo.math.gaussianRandom=function(){
var k=2;
do{
var i=2*Math.random()-1;
var j=2*Math.random()-1;
k=i*i+j*j;
}while(k>=1);
k=Math.sqrt((-2*Math.log(k))/k);
return i*k;
};
dojo.math.mean=function(){
var _96e=dojo.lang.isArray(arguments[0])?arguments[0]:arguments;
var mean=0;
for(var i=0;i<_96e.length;i++){
mean+=_96e[i];
}
return mean/_96e.length;
};
dojo.math.round=function(_971,_972){
if(!_972){
var _973=1;
}else{
var _973=Math.pow(10,_972);
}
return Math.round(_971*_973)/_973;
};
dojo.math.sd=function(){
var _974=dojo.lang.isArray(arguments[0])?arguments[0]:arguments;
return Math.sqrt(dojo.math.variance(_974));
};
dojo.math.variance=function(){
var _975=dojo.lang.isArray(arguments[0])?arguments[0]:arguments;
var mean=0,squares=0;
for(var i=0;i<_975.length;i++){
mean+=_975[i];
squares+=Math.pow(_975[i],2);
}
return (squares/_975.length)-Math.pow(mean/_975.length,2);
};
dojo.math.range=function(a,b,step){
if(arguments.length<2){
b=a;
a=0;
}
if(arguments.length<3){
step=1;
}
var _97b=[];
if(step>0){
for(var i=a;i<b;i+=step){
_97b.push(i);
}
}else{
if(step<0){
for(var i=a;i>b;i+=step){
_97b.push(i);
}
}else{
throw new Error("dojo.math.range: step must be non-zero");
}
}
return _97b;
};
dojo.provide("dojo.math.curves");
dojo.require("dojo.math");
dojo.math.curves={Line:function(_97d,end){
this.start=_97d;
this.end=end;
this.dimensions=_97d.length;
for(var i=0;i<_97d.length;i++){
_97d[i]=Number(_97d[i]);
}
for(var i=0;i<end.length;i++){
end[i]=Number(end[i]);
}
this.getValue=function(n){
var _981=new Array(this.dimensions);
for(var i=0;i<this.dimensions;i++){
_981[i]=((this.end[i]-this.start[i])*n)+this.start[i];
}
return _981;
};
return this;
},Bezier:function(pnts){
this.getValue=function(step){
if(step>=1){
return this.p[this.p.length-1];
}
if(step<=0){
return this.p[0];
}
var _985=new Array(this.p[0].length);
for(var k=0;j<this.p[0].length;k++){
_985[k]=0;
}
for(var j=0;j<this.p[0].length;j++){
var C=0;
var D=0;
for(var i=0;i<this.p.length;i++){
C+=this.p[i][j]*this.p[this.p.length-1][0]*dojo.math.bernstein(step,this.p.length,i);
}
for(var l=0;l<this.p.length;l++){
D+=this.p[this.p.length-1][0]*dojo.math.bernstein(step,this.p.length,l);
}
_985[j]=C/D;
}
return _985;
};
this.p=pnts;
return this;
},CatmullRom:function(pnts,c){
this.getValue=function(step){
var _98f=step*(this.p.length-1);
var node=Math.floor(_98f);
var _991=_98f-node;
var i0=node-1;
if(i0<0){
i0=0;
}
var i=node;
var i1=node+1;
if(i1>=this.p.length){
i1=this.p.length-1;
}
var i2=node+2;
if(i2>=this.p.length){
i2=this.p.length-1;
}
var u=_991;
var u2=_991*_991;
var u3=_991*_991*_991;
var _999=new Array(this.p[0].length);
for(var k=0;k<this.p[0].length;k++){
var x1=(-this.c*this.p[i0][k])+((2-this.c)*this.p[i][k])+((this.c-2)*this.p[i1][k])+(this.c*this.p[i2][k]);
var x2=(2*this.c*this.p[i0][k])+((this.c-3)*this.p[i][k])+((3-2*this.c)*this.p[i1][k])+(-this.c*this.p[i2][k]);
var x3=(-this.c*this.p[i0][k])+(this.c*this.p[i1][k]);
var x4=this.p[i][k];
_999[k]=x1*u3+x2*u2+x3*u+x4;
}
return _999;
};
if(!c){
this.c=0.7;
}else{
this.c=c;
}
this.p=pnts;
return this;
},Arc:function(_99f,end,ccw){
var _9a2=dojo.math.points.midpoint(_99f,end);
var _9a3=dojo.math.points.translate(dojo.math.points.invert(_9a2),_99f);
var rad=Math.sqrt(Math.pow(_9a3[0],2)+Math.pow(_9a3[1],2));
var _9a5=dojo.math.radToDeg(Math.atan(_9a3[1]/_9a3[0]));
if(_9a3[0]<0){
_9a5-=90;
}else{
_9a5+=90;
}
dojo.math.curves.CenteredArc.call(this,_9a2,rad,_9a5,_9a5+(ccw?-180:180));
},CenteredArc:function(_9a6,_9a7,_9a8,end){
this.center=_9a6;
this.radius=_9a7;
this.start=_9a8||0;
this.end=end;
this.getValue=function(n){
var _9ab=new Array(2);
var _9ac=dojo.math.degToRad(this.start+((this.end-this.start)*n));
_9ab[0]=this.center[0]+this.radius*Math.sin(_9ac);
_9ab[1]=this.center[1]-this.radius*Math.cos(_9ac);
return _9ab;
};
return this;
},Circle:function(_9ad,_9ae){
dojo.math.curves.CenteredArc.call(this,_9ad,_9ae,0,360);
return this;
},Path:function(){
var _9af=[];
var _9b0=[];
var _9b1=[];
var _9b2=0;
this.add=function(_9b3,_9b4){
if(_9b4<0){
dojo.raise("dojo.math.curves.Path.add: weight cannot be less than 0");
}
_9af.push(_9b3);
_9b0.push(_9b4);
_9b2+=_9b4;
computeRanges();
};
this.remove=function(_9b5){
for(var i=0;i<_9af.length;i++){
if(_9af[i]==_9b5){
_9af.splice(i,1);
_9b2-=_9b0.splice(i,1)[0];
break;
}
}
computeRanges();
};
this.removeAll=function(){
_9af=[];
_9b0=[];
_9b2=0;
};
this.getValue=function(n){
var _9b8=false,value=0;
for(var i=0;i<_9b1.length;i++){
var r=_9b1[i];
if(n>=r[0]&&n<r[1]){
var subN=(n-r[0])/r[2];
value=_9af[i].getValue(subN);
_9b8=true;
break;
}
}
if(!_9b8){
value=_9af[_9af.length-1].getValue(1);
}
for(j=0;j<i;j++){
value=dojo.math.points.translate(value,_9af[j].getValue(1));
}
return value;
};
function computeRanges(){
var _9bc=0;
for(var i=0;i<_9b0.length;i++){
var end=_9bc+_9b0[i]/_9b2;
var len=end-_9bc;
_9b1[i]=[_9bc,end,len];
_9bc=end;
}
}
return this;
}};
dojo.provide("dojo.math.points");
dojo.require("dojo.math");
dojo.math.points={translate:function(a,b){
if(a.length!=b.length){
dojo.raise("dojo.math.translate: points not same size (a:["+a+"], b:["+b+"])");
}
var c=new Array(a.length);
for(var i=0;i<a.length;i++){
c[i]=a[i]+b[i];
}
return c;
},midpoint:function(a,b){
if(a.length!=b.length){
dojo.raise("dojo.math.midpoint: points not same size (a:["+a+"], b:["+b+"])");
}
var c=new Array(a.length);
for(var i=0;i<a.length;i++){
c[i]=(a[i]+b[i])/2;
}
return c;
},invert:function(a){
var b=new Array(a.length);
for(var i=0;i<a.length;i++){
b[i]=-a[i];
}
return b;
},distance:function(a,b){
return Math.sqrt(Math.pow(b[0]-a[0],2)+Math.pow(b[1]-a[1],2));
}};
dojo.kwCompoundRequire({common:[["dojo.math",false,false],["dojo.math.curves",false,false],["dojo.math.points",false,false]]});
dojo.provide("dojo.math.*");

