var _disable_sr=false;

function addOption(selectElement,newOption) {
  try {
    selectElement.add(newOption,null);
  }
  
  catch (e) {
    selectElement.add(newOption,selectElement.length);
  }
}

function coal() {
  for(var i=1;;i++) {
    var ra = document.getElementById('ra'+i);
    if (ra) {
      if (ra.style.display=='block') {
        ra.style.display='none';
        document.getElementById('id'+i).src='testopia/img/tr.gif';
      }
    } else {
      break;
    }
  }
}
function exal() {
  for(var i=1;;i++) {
    var ra = document.getElementById('ra'+i);
    if (ra) {
      if (ra.style && (ra.style.display=='none' || ra.style.display=='')) {
        ra.style.display='block';
        document.getElementById('id'+i).src='testopia/img/tr.gif';
      }
    } else {
      break;
    }
  }
}
function _cset(newstate) {
  var myform = document.getElementById('table');
  for(i=0;i<myform.length;i++) {
    if(myform[i].type == 'checkbox' && myform[i].name != 'togglearch'){
      myform[i].checked = newstate;
    }
  }
  //st();
}

function st() {
  var nv = ((_canedit) && (count>0))? false : true;
  disableAllSelects(nv);
}

function sp(list_index, list_id) {
  for(var i=0; true; i++) {
    var c = document.getElementById('c_'+i);
    if (c) {
      if (c.checked) {
        list_index.push(i);
        list_id.push(ids[i]);
      }
    } else {
      break;
    }    
  }
}

function sall() {
  _cset(true);
}

function snone() {
  _cset(false);
}
// Show/hide a row
function sr(i) {
  if (_disable_sr) {
    _disable_sr = false;
  } else {
    _sr(document.getElementById('ra'+i), document.getElementById('id'+i));
  }
}

function _sr(obj,im) {
  if (obj.style.display=='block') {obj.style.display='none'; im.src='testopia/img/tr.gif';}
  else {obj.style.display='block'; im.src='testopia/img/td.gif'}
}

function get_ie_ver () {
  var UAversion = parseFloat( navigator.Appversion );
  if (navigator.appVersion.indexOf("MSIE") != -1) 
  {
    IEmajorStart = navigator.appVersion.indexOf("MSIE") + 4;
    IEmajorEnd = (IEmajorStart + 5);
    theMajor = navigator.appVersion.substring(IEmajorStart, IEmajorEnd);
    (UAversion = theMajor);
  } else {
    if (isNaN(UAversion)) {
      (UAversion = (UAversion.substring(0, (UAversion.length - 1))));
    } 
  }
  return UAversion;
}

var ie45,ns6,ns4,dom;

if (navigator.appName=="Microsoft Internet Explorer") {
  ie45 = parseInt(get_ie_ver()) >=4;
} else if (navigator.appName=="Netscape") {
  ns6=parseInt(navigator.appVersion)>=5;
  ns4=parseInt(navigator.appVersion)<5;
}
dom=ie45 || ns6;

function showit (id) {
  el = document.all ? document.all[id] : 
    (dom ? document.getElementById(id) : document.layers[id]);
  els = dom ? el.style : el;
  if (dom){
    if (els.visibility == "hidden") els.visibility = "visible";
  } else if (ns4){    
    if (els.visibility == "hide") els.visibility = "show";
  }
}

// There is only AJAX

var HTTPREQUEST_TIMEOUT=30000;
var CONTENT_TYPE='application/x-www-form-urlencoded';
var OK_TIMEOUT=3000;

var MSG_WAIT='Please wait...';
var MSG_TIMEOUT='Request timed out. Please try again.';
var MSG_REQUEST_FAILED='Error, request failed. Please, try again or use the classic interface.';
var MSG_COMM_PROBLEM='Error, communication problem. Please try again.';
var MSG_BAD_BROWSER="Your browser doesn't support client HTTP requests (XMLHttp). Please, use the classic interface.";

function createHttpReq () {

  var httpReq;
  
  /*@cc_on @*/
  /*@if (@_jscript_version >= 5)
  try {
    httpReq = new ActiveXObject("Msxml2.XMLHTTP");
  } catch (e) {
    try {
      httpReq = new ActiveXObject("Microsoft.XMLHTTP");
    } catch (E) {
      httpReq = null;
    }
  }
  @end @*/
  if (!httpReq && typeof XMLHttpRequest!='undefined') {
    httpReq = new XMLHttpRequest();
    httpReq.multipart = false;
  }
  
  if (!httpReq) {
    alert(MSG_BAD_BROWSER);
  }
  
  return httpReq;
}


function clearMsg(id) {
    var o = document.getElementById(id);
    o.style.display="none";
}

function displayMsg(id, type, text) {

      var o = document.getElementById(id);
      switch(type) {
        case 1:
          o.style.background='green';
          o.style.color='white';
          break;
        case 2:
          o.style.background='red';
          o.style.color='white';
          break;
        case 3:
          o.style.background='#FFDD66';
          o.style.color='black';
          break;
    }
    o.innerHTML = text;
    o.style.display = 'inline';
}

function nc(obj) {
  obj.style.height='160px';
  //obj.rows=8;
}

function nb(obj) {
  //obj.rows=1;
  obj.style.height='20px';
}

function getProdVers(product, plan){
    document.getElementById("prod_version").disabled = true;
    dojo.io.bind({
        url:   "tr_new_plan.cgi",
        content: {  product_id: product, action: "getversions" },
        load:  function(type, data, evt){
                 if (data.error){
                   alert(data.error);
                   document.getElementById("prod_version").disabled = false;
                   return;
                 }
                 var prodvers = document.getElementById("prod_version");
                 prodvers.options.length = 0;
                 for (i in data){
                   var myOp = new Option(data[i].name, data[i].id);
                   addOption(prodvers, myOp);
                   if (data[i].name == '[% plan.product_version FILTER none %]'){
                      prodvers.options[i].selected = true;
                   }
                 }
                 document.getElementById("prod_version").disabled = false;
               },
        error: function(type, error){ alert("ERROR");},
        mimetype: "text/json"
    });
}
