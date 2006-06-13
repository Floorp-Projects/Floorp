var MSG_TEST_CASE_STATUS_UPDATED = 'Status updated';
var MSG_TEST_CASE_ORDER_UPDATED = 'Order updated';

var count;

var list_index = null;
var list_id = null;

var httpReq = null;
var _timeOutID = null;

var _newStatus = null;
var _order = null;

function _disableSelect(id,nv) {
  var os;
  os = document.getElementById(id).options;
  os[0].selected=true;
  for (var i=1;i<os.length;i++) {
    os[i].disabled=nv;
  }
  //IE doesn't disable individual options:
  document.getElementById(id).disabled = nv;
}

function disableAllSelects(nv) {
  _disableSelect('form_action',nv);
  _disableSelect('status',nv);
}

function disableAllButtons(nv) {
  disableAllSelects(nv);
  var o = document.getElementById('addtc');
  if (o) {
    o.disabled=nv;
  }
}

/*
function ol() {
  count = 0;
  for(var i=0; ; i++) {
    var c = document.getElementById('c_'+i);
    if (c) {
      if (c.checked) {
        count++;
      }
    } else {
      break;
    }    
  }
  st();
}
*/

function ol() {
  snone();
}

function ck(c) {
  _disable_sr=true;

  if(c.checked) {
    count++;
  } else {
    count--;
  }
  st();
}


//change status
function sx(sel) {
  var index = sel.selectedIndex;
  if (index > 0) {
    list_index = new Array();
    list_id = new Array();
    sp(list_index, list_id);

    sel.options[0].selected=true;
    _newStatus = sel.options[index].value;
    
    makeCall('1', 's=' + _newStatus, sx_done);
  }
}

function sx_done () {

  if (httpReq.readyState == 4) {
    if(_timeOutID) {
      clearTimeout(_timeOutID);
      _timeOutID = null;
    }

    var s = httpReq.responseText;
    if (s.substring(0,2)=='OK') {
        s = s.substring(3);

		updateStatus(list_id, _newStatus);
		_newStatus = null;

        displayMsg('floatMsg', 1, MSG_TEST_CASE_STATUS_UPDATED);
        setTimeout("clearMsg('floatMsg')",OK_TIMEOUT);        
    } else {
        if (s.substring(0,5)=='Error') {
          //controlled error
        } else {
          s = MSG_REQUEST_FAILED;
        }
        displayMsg('floatMsg', 2, s);
    }
    httpReq = null;
    disableAllButtons(false);
    
    list_index = null;
    list_id = null;
  }
}

function updateStatus(list, status) {

  for(var i=list.length-1; i>-1; i--) {
    var s = document.getElementById('sta'+list[i]);
    s.innerHTML = status;
  }

}

//actions
function ss(sel) {
  var index = sel.selectedIndex;
  if (index > 0) {  
    list_index = new Array();
    list_id = new Array();
    sp(list_index, list_id);
          
    switch (index) {
      case 1:
        //move up
        _order = 'up';
        makeCall('2', '', ss_done);
        break;
      case 2:
        //move down
        _order = 'down';
        makeCall('3', '', ss_done);
        break;
    }
    
    //
    sel.options[0].selected=true;
  }
}

function ss_done () {

  if (httpReq.readyState == 4) {
    if(_timeOutID) {
      clearTimeout(_timeOutID);
      _timeOutID = null;
    }

    var s = httpReq.responseText;
    if (s.substring(0,2)=='OK') {

if(_inOrder){
		if (_order == 'up') {
	        for(var i=0; i<list_index.length; i++) {
	          moveup(list_index[i]);
	        }
		} else if (_order == 'down') {
            //reversed order:
            for(var i=list_index.length-1; i>-1; i--) {
              movedown(list_index[i]);
            }
		}

		_order = null;
				
		displayMsg('floatMsg', 1, MSG_TEST_CASE_ORDER_UPDATED);
        setTimeout("clearMsg('floatMsg')",OK_TIMEOUT);
} else {
  //I give up refreshing the list dinamically, let the server do it:
  document.location.reload();
}      
    } else {
        if (s.substring(0,5)=='Error') {
          //controlled error
        } else {
          s = MSG_REQUEST_FAILED;
        }
        displayMsg('floatMsg', 2, s);
    }
    httpReq = null;
    disableAllButtons(false);
    
    list_index = null;
    list_id = null;
  }

}

function _swap(r1, r2) {
  
  var nextSibling = r1.nextSibling;
  var parentNode = r1.parentNode;
  r2.parentNode.replaceChild(r1, r2);
  parentNode.insertBefore(r2, nextSibling);
  
  c = r1.className;
  r1.className = r2.className;
  r2.className = c;
}

function swapRows(i1, i2) {

  var c2;
  c2 = document.getElementById('c_' + i2);
  if (!c2) {
    //stop wasting time
    return;
  }

  var t=document.getElementById('ttt');  
  
  var x;
  
  //checkBoxes:  
  var c1;
  c1 = document.getElementById('c_' + i1);
  x = c1.id;
  c1.id = c2.id;
  c2.id = x;
  
  var c1_checked = c1.checked;
  var c2_checked = c2.checked;
  
  
  //array with ids:
  x = ids[i1];
  ids[i1] = ids[i2];
  ids[i2] = x;
  
  //order:
  var s1, s2;
  s1 = document.getElementById('sq'+ids[i1]);
  s2 = document.getElementById('sq'+ids[i2]);
  x = s1.innerHTML;
  s1.innerHTML = s2.innerHTML;
  s2.innerHTML = x;

  
  var row1, row2;
  
  row1 = t.rows[i1*2+1];
  row2 = t.rows[i2*2+1];
  _swap(row1, row2);
  
  row1 = t.rows[i1*2+2];
  row2 = t.rows[i2*2+2];
  _swap(row1, row2);

  //IE resets the checked status:
  if (c1_checked) c1.checked = true;
  if (c2_checked) c2.checked = true;
}

function moveup(index) {

  swapRows(index, index-1);
}

function movedown(index) {

  swapRows(index, index+1);
}

//

function makeCall(action, query, callback) {

  var q = 'a=' + action + '&ids=' + list_id.join(',');
  if (query != '') {
    q += '&' + query;
  }
  
  if (httpReq) {
    httpReq.abort();
  }
  httpReq = createHttpReq();
  if (httpReq) {
    disableAllButtons(true);

    displayMsg('floatMsg', 3, MSG_WAIT.blink());

    _timeOutID = window.setTimeout("timeout()",HTTPREQUEST_TIMEOUT);
    httpReq.open('POST', 'tr_service_testcases.cgi', true);
    httpReq.setRequestHeader('Content-Type', CONTENT_TYPE);
    httpReq.onreadystatechange = callback;
    httpReq.send(q);
  }
}

function timeout() {
  if (httpReq) {
    httpReq.abort();
    httpReq = null;
    displayMsg('floatMsg', 2, MSG_TIMEOUT);
    disableAllButtons(false);
  }
}
