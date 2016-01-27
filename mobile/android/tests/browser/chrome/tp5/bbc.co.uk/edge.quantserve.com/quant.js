if(!__qc){var __qc={qcdst:function(){if(__qc.qctzoff(0)!=__qc.qctzoff(6))return 1;return 0;},qctzoff:function(m){var d1=new Date(2000,m,1,0,0,0,0);var t=d1.toGMTString();var d3=new Date(t.substring(0,t.lastIndexOf(" ")-1));return d1-d3;},qceuc:function(s){if(typeof(encodeURIComponent)=='function'){return encodeURIComponent(s);}
else{return escape(s);}},qcrnd:function(){return Math.round(Math.random()*2147483647);},qcgc:function(n){var v='';var c=document.cookie;if(!c)return v;var i=c.indexOf(n+"=");var len=i+n.length+1;if(i>-1){var end=c.indexOf(";",len);if(end<0)end=c.length;v=c.substring(len,end);}
return v;},qcdomain:function(){var d=document.domain;if(d.substring(0,4)=="www.")d=d.substring(4,d.length);var a=d.split(".");var len=a.length;if(len<3)return d;var e=a[len-1];if(e.length<3)return d;d=a[len-2]+"."+a[len-1];return d;},qhash2:function(h,s){for(var i=0;i<s.length;i++){h^=s.charCodeAt(i);h+=(h<<1)+(h<<4)+(h<<7)+(h<<8)+(h<<24);}
return h;},qhash:function(s){var h1=0x811c9dc5,h2=0xc9dc5118;var hash1=__qc.qhash2(h1,s);var hash2=__qc.qhash2(h2,s);return(Math.round(Math.abs(hash1*hash2)/65536)).toString(16);},sd:["4dcfa7079941","127fdf7967f31","588ab9292a3f","32f92b0727e5","22f9aa38dfd3","a4abfe8f3e04","18b66bc1325c","958e70ea2f28","bdbf0cb4bbb","65118a0d557","40a1d9db1864","18ae3d985046","3b26460f55d"],qcsc:function(){var s="";var d=__qc.qcdomain();if(__qc.qad==1)return";fpan=u;fpa=";var qh=__qc.qhash(d);for(var i=0;i<__qc.sd.length;i++){if(__qc.sd[i]==qh)return";fpan=u;fpa=";}
var u=document;var a=__qc.qcgc("__qca");if(a.length>0){s+=";fpan=0;fpa="+a;}
else{var da=new Date();a='P0-'+__qc.qcrnd()+'-'+da.getTime();u.cookie="__qca="+a+"; expires=Sun, 18 Jan 2038 00:00:00 GMT; path=/; domain="+d;a=__qc.qcgc("__qca");if(a.length>0){s+=";fpan=1;fpa="+a;}
else{s+=";fpan=u;fpa=";}}
return s;},qcdc:function(n){document.cookie=n+"=; expires=Thu, 01 Jan 1970 00:00:01 GMT; path=/; domain="+__qc.qcdomain();},qpxloaddisabled:function(img){if(img&&typeof(img.width)=="number"&&img.width==3){__qc.qcdc("__qca");}},qcp:function(p,myqo){var s='',a=null;var media='webpage',event='loaddisabled';if(myqo!=null){for(var k in myqo){if(typeof(k)!='string'){continue;}
if(typeof(myqo[k])!='string'){continue;}
if(k=='qacct'){a=myqo[k];continue;}
s+=';'+k+p+'='+__qc.qceuc(myqo[k]);if(k=='media'){media=myqo[k];}
if(k=='event'){event=myqo[k];}}}
if(typeof a!="string"){if((typeof _qacct=="undefined")||(_qacct.length==0))return'';a=_qacct;}
if(media=='webpage'&&event=='loaddisabled'){for(var i=0;i<__qc.qpixelsent.length;i++){if(__qc.qpixelsent[i]==a)return'';}
__qc.qpixelsent.push(a);}
if(media=='ad'){__qc.qad=1;}
s=';a'+p+'='+a+s;return s;},qcesc:function(s){return s.replace(/\./g,'%2E').replace(/,/g,'%2C');},qcd:function(o){return(typeof(o)!="undefined"&&o!=null);},qcogl:function(){var m=document.getElementsByTagName('meta');var o='';for(var i=0;i<m.length;i++){if(o.length>=1000)return o;if(__qc.qcd(m[i])&&__qc.qcd(m[i].attributes)&&__qc.qcd(m[i].attributes.property)&&__qc.qcd(m[i].attributes.property.value)&&__qc.qcd(m[i].content)){var p=m[i].attributes.property.value;var c=m[i].content;if(p.length>3&&p.substring(0,3)=='og:'){if(o.length>0)o+=',';var l=(c.length>80)?80:c.length;o+=__qc.qcesc(p.substring(3,p.length))+'.'+__qc.qcesc(c.substring(0,l));}}}
return __qc.qceuc(o);},firepixel:function(qoptions){var e=(typeof(encodeURIComponent)=='function')?"n":"s";var r=__qc.qcrnd();var sr='',qo='',qm='',url='',ref='',je='u',ns='1';var qocount=0;__qc.qad=0;if(typeof __qc.qpixelsent=="undefined"){__qc.qpixelsent=new Array();}
if(typeof qoptions!="undefined"&&qoptions!=null){__qc.qopts=qoptions;for(var k in __qc.qopts){if(typeof(__qc.qopts[k])=='string'){qo=__qc.qcp("",__qc.qopts);break;}else if(typeof(__qc.qopts[k])=='object'&&__qc.qopts[k]!=null){++qocount;qo+=__qc.qcp("."+qocount,__qc.qopts[k]);}}}else if(typeof _qacct=="string"){qo=__qc.qcp("",null);}
if(qo.length==0)return;var ce=(navigator.cookieEnabled)?"1":"0";if(typeof navigator.javaEnabled!='undefined')je=(navigator.javaEnabled())?"1":"0";if(typeof _qmeta!="undefined"&&_qmeta!=null){qm=';m='+__qc.qceuc(_qmeta);_qmeta=null;}
if(self.screen){sr=screen.width+"x"+screen.height+"x"+screen.colorDepth;}
var d=new Date();var dst=__qc.qcdst();var qs='http';if(window.location.protocol=='https:'){qs+='s';}
qs="../../pixel.quantserve.com";var fp=__qc.qcsc();if(window.location&&window.location.href)url=__qc.qceuc(window.location.href);if(window.document&&window.document.referrer)ref=__qc.qceuc(window.document.referrer);if(self==top)ns='0';var ogl=__qc.qcogl();var img=new Image();img.alt="";img.src=qs+'/pixel/r.html';img.onloaddisabled=function(){__qc.qpxloaddisabled(img);}},quantserve:function(){if(typeof _qevents=='undefined'){_qevents=[];}
if(typeof _qoptions!="undefined"&&_qoptions!=null){__qc.firepixel(_qoptions);_qoptions=null;}else if(!_qevents.length&&typeof _qacct!="undefined"){__qc.firepixel(null);}
if(!__qc.evts){for(var k in _qevents){__qc.firepixel(_qevents[k]);}
_qevents={push:function(){var a=arguments;for(var i=0;i<a.length;i++){__qc.firepixel(a[i]);}}};__qc.evts=1;}}};}
function quantserve(){__qc.quantserve();}
quantserve();
