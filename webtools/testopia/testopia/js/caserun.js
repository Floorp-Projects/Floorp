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

function fillrow(data, idx){
	//document.write(data);
	if (data.substring(0,5) == 'Error'){
		displayMsg('pp'+idx, 2, data);
		return;
	}
	if (data.substring(0,9) == '<!DOCTYPE'){
	    var doc = document.open("text/html","replace"); 
	    doc.write(data);
	    doc.close();
	    return;
	}
}
function getNote(idx,cid){
	disableAllButtons(true);
	dojo.io.bind({
		url:     "tr_show_caserun.cgi",
		content: {  caserun_id: cid, index: idx, action: 'get_notes'},
		load:    function(type, data, evt){ 
                dojo.byId('old_notes' + idx).innerHTML = data;
		},
		error:   function(type, error){ alert(error.message);},
		mimetype: "text/plain"
	});
}
//chBld Updates the caserun build 
function chBld(idx, bid, sid, cid){
	disableAllButtons(true);
	dojo.io.bind({
		url:     "tr_show_caserun.cgi",
		content: {  caserun_id: cid, index: idx, build_id: bid, action: 'update_build'},
		load:    function(type, data, evt){ 
				fillrow(data, idx);
				var fields = data.split("|~+");
				dojo.widget.manager.getWidgetById('head_caserun_'+idx).setContent(fields[0]);
				document.getElementById('body_caserun_'+idx).innerHTML = fields[1];
				document.getElementById('ra'+idx).style.display='block'; 
				document.getElementById('id'+idx).src='testopia/img/td.gif';
				displayMsg('pp'+ idx, 1, MSG_TESTLOG_UPDATED);
	            setTimeout("clearMsg('pp"+ idx +"')",OK_TIMEOUT);
				disableAllButtons(false);
                getNote(idx,cid);
		},
		error:   function(type, error){ alert(error.message);},
		mimetype: "text/plain"
	});
}
//chEnv Updates the caserun environment 
function chEnv(idx, eid, sid, cid, oldid){
	if (oldid == eid || !eid)
		return;
	disableAllButtons(true);
	dojo.io.bind({
		url:     "tr_show_caserun.cgi",
		content: {  caserun_id: cid, index: idx, caserun_env: eid, action: 'update_environment'},
		load:    function(type, data, evt){ 
				fillrow(data, idx);
				var fields = data.split("|~+");
				dojo.widget.manager.getWidgetById('head_caserun_'+idx).setContent(fields[0]);
				document.getElementById('body_caserun_'+idx).innerHTML = fields[1];
				document.getElementById('ra'+idx).style.display='block'; 
				document.getElementById('id'+idx).src='testopia/img/td.gif';
				displayMsg('pp'+ idx, 1, MSG_TESTLOG_UPDATED);
	            setTimeout("clearMsg('pp"+ idx +"')",OK_TIMEOUT);
				disableAllButtons(false);
                getNote(idx,cid);
		},
		error:   function(type, error){ alert(error.message);},
		mimetype: "text/plain"
	});
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
					try{
						var deps = fields[3];
						ids = deps.split(",");
						for (var i=0; i<ids.length; i++){
							if (status == 'FAILED')
								document.getElementById('xs'+ids[i]).src="testopia/img/BLOCKED_small.gif";
							else
								document.getElementById('xs'+ids[i]).src="testopia/img/IDLE_small.gif";
						}
					}
					catch (e){}
					document.getElementById('xs'+cid).src="testopia/img/"+status+"_small.gif";
					document.getElementById('tdb'+idx).innerHTML = tested;
					document.getElementById('cld'+idx).innerHTML = closed;
		            displayMsg('pp'+ idx, 1, MSG_TESTLOG_UPDATED);
		            setTimeout("clearMsg('pp"+ idx +"')",OK_TIMEOUT);
		            disableAllButtons(false);
                    getNote(idx,cid);
		         },
		error:   function(type, error){ alert(error.message);},
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
					document.getElementById('notes'+idx).value = '';
					document.getElementById('old_notes'+idx).innerHTML = data;
		            displayMsg('pp'+ idx, 1, MSG_TESTLOG_UPDATED);
		            setTimeout("clearMsg('pp"+ idx +"')",OK_TIMEOUT);
		            disableAllButtons(false);
                    getNote(idx,cid);
		         },
		error:   function(type, error){ alert(error.message);},
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
					if (data.substring(0,5) == 'Error'){
						displayMsg('pp'+ idx, 2, data);
						return;
					}
					document.getElementById('own'+idx).innerHTML = owner;
		            displayMsg('pp'+ idx, 1, MSG_TESTLOG_UPDATED);
		            setTimeout("clearMsg('pp"+ idx +"')",OK_TIMEOUT);
		            disableAllButtons(false);
                    getNote(idx,cid);
		         },
		error:   function(type, error){ alert(error.message);},
		mimetype: "text/plain"
	});

}
//chSortKey
function chSortKey(idx, cid, svalue){
    displayMsg('pp'+idx, 3, MSG_WAIT.blink());
	disableAllButtons(true);
	dojo.io.bind({
		url:     "tr_show_caserun.cgi",
		content: {  caserun_id: cid, index: idx, sortkey: svalue, action: 'update_sortkey'},
		load:    function(type, data, evt){ fillrow(data, idx);
					if (data.substring(0,5) == 'Error'){
						displayMsg('pp'+ idx, 2, data);
						return;
					}
		            displayMsg('pp'+ idx, 1, MSG_TESTLOG_UPDATED);
		            setTimeout("clearMsg('pp"+ idx +"')",OK_TIMEOUT);
		            disableAllButtons(false);
		         },
		error:   function(type, error){ alert(error.message);},
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
		error:   function(type, error){ alert(error.message);},
		mimetype: "text/plain"
	});

}
