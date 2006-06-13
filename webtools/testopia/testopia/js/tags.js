// Requires testopia/js/util.js

function makeCall(query){
  
  if (typeof(httpReq) != 'undefined') {
    httpReq.abort();
  }

  httpReq = createHttpReq();
  if (httpReq) {
    document.getElementById('doAdd').disabled = true;
//alert("Got it " + plan_id);
    displayMsg('message', 3, MSG_WAIT.blink());

    _timeOutID = window.setTimeout("timeout('message')",HTTPREQUEST_TIMEOUT);
    httpReq.open('POST', 'tr_tags.cgi', true);
    httpReq.setRequestHeader('Content-Type', CONTENT_TYPE);
    httpReq.onreadystatechange = tagAttached;
    httpReq.send(query);
  }
}

function addTag(id, type){
  var tagValue = document.getElementById('newtag').value;
  if (tagValue == ''){
     alert("I can't work with a blank tag. Please enter something in the space provided");
     return;
  }
  var q = 'action=addtag&id=' + id + '&tag=' + tagValue + '&type=' + type;
  makeCall(q);
}

function removeTag(tagid, id, type){
  var q = 'action=removetag&id=' + id + '&tagid=' + tagid + '&type=' + type;
  makeCall(q);
	
}

function tagAttached() {
  if (httpReq.readyState == 4) {
    if(_timeOutID) {
      clearTimeout(_timeOutID);
      _timeOutID = null;
    }
  
    var s = httpReq.responseText;
//    alert(s);
    if (s.substring(0,2)=='OK') {
        s = s.substring(2);

        displayMsg('message', 1, "Updated");
        setTimeout("clearMsg('message')",OK_TIMEOUT);        
        document.getElementById('tagTable').innerHTML = s;
    } else {
        if (s.substring(0,5)=='Error') {
          //controlled error
        } else {
          s = MSG_REQUEST_FAILED;
        }
        displayMsg('message', 2, s);
    }
    httpReq = undefined;
    document.getElementById('doAdd').disabled = false;
    document.getElementById('newtag').value = '';
    
  }

}