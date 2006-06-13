var MSG_ERROR_REFRESHING='An error occurred refreshing the list';
var MSG_TESTLOG_UPDATED='Test log updated';
var MSG_BUG_ATTACHED='Bug attached';
var MSG_TESTLOG_ALREADYRUNNING=' is already running this test log';

buttonsDisabled = false;

function disableAllButtons(newstate) {

  buttonsDisabled = newstate;
  
  for(var i=0;1;i++) {
    var o = document.getElementById('en'+i);
    if (o != null) {
      if (o.value == '1') {

        var sk = document.getElementById('sk'+i);
        if (!newstate) { // only on enable
          var bugid=document.getElementById('mbg'+i).value;
          if (validBugid(bugid)) {
            sk.disabled = false;
          }
        } else {
          sk.disabled = true;
        }

        if (newstate) { //only on disable
          document.getElementById('bs'+i).disabled = newstate;
        }
      }
      document.getElementById('sh'+i).disabled = newstate;
    } else {
      break;
    }
  }
}

var _index;
var _id;

function validBugid(bugid) {

  var v = parseInt(bugid);
  return (v != NaN) && (v > 0);
}

function edt_attch(index) {
  
  var bugid=document.getElementById('mbg'+index).value;
  document.getElementById('sk'+index).disabled = !validBugid(bugid);
}

function fillrow(data, idx){
	//document.write(data);
	if (data.substring(0,5) == 'Error'){
		displayMsg('pp'+_index, 2, data);
		return;
	}
	if (data.substring(0,9) == '<!DOCTYPE'){
	    var doc = document.open("text/html","replace"); 
	    doc.write(data);
	    doc.close();
	    return;
	}
}
//chBld Updates the caserun build 
function chBld(idx, bid, sid, cid){
	if (sid != 'IDLE') 
		var ret = confirm("You are changing the build of a statused test case. This will reset the testcase. Are you sure?");
	if (ret == true || sid == 'IDLE'){
		disableAllButtons(true);
		dojo.io.bind({
			url:     "tr_show_caserun.cgi",
			content: {  caserun_id: cid, index: idx, build_id: bid, action: 'update_build'},
			load:    function(type, data, evt){ fillrow(data, idx); sr(idx);
					var fields = data.split("|");
					var status = fields[0];
					var closed = fields[1];
					var tested = fields[2];
					document.getElementById('xs'+idx).src="testopia/img/"+status+"_small.gif";
					document.getElementById('tdb'+idx).innerHTML = tested;
					document.getElementById('cld'+idx).innerHTML = closed;
					displayMsg('pp'+ idx, 1, MSG_TESTLOG_UPDATED);
		            setTimeout("clearMsg('pp"+ idx +"')",OK_TIMEOUT);
					disableAllButtons(false);
			},
			error:   function(type, error){ alert("ERROR");},
			mimetype: "text/plain"
    	});
    }
}
//chStat updates the status
function chStat(idx, sid, cid){
    displayMsg('pp'+idx, 3, MSG_WAIT.blink());
	disableAllButtons(true);
	dojo.io.bind({
		url:     "tr_show_caserun.cgi",
		content: {  caserun_id: cid, index: idx, status_id: sid, action: 'update_status'},
		load:    function(type, data, evt){ fillrow(data, idx);
					var fields = data.split("|");
					var status = fields[0];
					var closed = fields[1];
					var tested = fields[2];
					document.getElementById('xs'+idx).src="testopia/img/"+status+"_small.gif";
					document.getElementById('tdb'+idx).innerHTML = tested;
					document.getElementById('cld'+idx).innerHTML = closed;
		            displayMsg('pp'+ idx, 1, MSG_TESTLOG_UPDATED);
		            setTimeout("clearMsg('pp"+ idx +"')",OK_TIMEOUT);
		            disableAllButtons(false);
		         },
		error:   function(type, error){ alert("ERROR");},
		mimetype: "text/plain"
	});

}
//chNote updates the notes
function chNote(idx, cid, note){
    displayMsg('pp'+idx, 3, MSG_WAIT.blink());
	disableAllButtons(true);
	dojo.io.bind({
		url:     "tr_show_caserun.cgi",
		content: {  caserun_id: cid, index: idx, note: note, action: 'update_note'},
		load:    function(type, data, evt){ fillrow(data, idx);
		            displayMsg('pp'+ idx, 1, MSG_TESTLOG_UPDATED);
		            setTimeout("clearMsg('pp"+ idx +"')",OK_TIMEOUT);
		            disableAllButtons(false);
		         },
		error:   function(type, error){ alert("ERROR");},
		mimetype: "text/plain"
	});

}
//chOwn updates the Assignee
function chOwn(idx, cid, owner){
    displayMsg('pp'+idx, 3, MSG_WAIT.blink());
	disableAllButtons(true);
	dojo.io.bind({
		url:     "tr_show_caserun.cgi",
		content: {  caserun_id: cid, index: idx, assignee: owner, action: 'update_assignee'},
		load:    function(type, data, evt){ fillrow(data, idx);
					document.getElementById('own'+idx).innerHTML = owner;
		            displayMsg('pp'+ idx, 1, MSG_TESTLOG_UPDATED);
		            setTimeout("clearMsg('pp"+ idx +"')",OK_TIMEOUT);
		            disableAllButtons(false);
		         },
		error:   function(type, error){ alert("ERROR");},
		mimetype: "text/plain"
	});

}
//Attach bugs
function attch(idx, cid, bugs){
    displayMsg('pp'+idx, 3, MSG_WAIT.blink());
	disableAllButtons(true);
	dojo.io.bind({
		url:     "tr_show_caserun.cgi",
		content: {  caserun_id: cid, index: idx, bugs: bugs, action: 'attach_bug'},
		load:    function(type, data, evt){
					document.getElementById('bgl'+idx).innerHTML = data;
		            displayMsg('pp'+ idx, 1, MSG_BUG_ATTACHED);
		            setTimeout("clearMsg('pp"+ idx +"')",OK_TIMEOUT);
		            disableAllButtons(false);
		         },
		error:   function(type, error){ alert(data);},
		mimetype: "text/plain"
	});

}