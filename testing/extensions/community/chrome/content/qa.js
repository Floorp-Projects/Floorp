var qaMain = {
	htmlNS: "http://www.w3.org/1999/xhtml",
	
	curtest: null,
  
	openQATool : function() {
		window.open("chrome://qa/content/qa.xul", "_blank", "chrome,all,dialog=no,resizable=no");
	},
	nextButton: function() {
		// if they selected a result, then submit the result
  		if ($('qa-testcase-result').selectedItem) {
  			qaMain.submitResult();
  		}
	},
	populateTestcase : function(testcase) {
		// stash the testcase object in curtest for future reference:
		curtest = testcase;
		
		document.getElementById('qa-testcase-id').value = 
  			qaMain.bundle.getString("qa.extension.testcase.head")+testcase.testcase_id;
		document.getElementById('qa-testcase-summary').value = testcase.summary;
    
		document.getElementById('qa-testcase-steps').innerHTML = testcase.steps_formatted;
		document.getElementById('qa-testcase-expected').innerHTML = testcase.expected_results_formatted;
	},
	onToolOpen : function() {
		if (qaPref.getPref(qaPref.prefBase+'.isFirstTime', 'bool') == true) {
			window.open("chrome://qa/content/setup.xul", "_blank", "chrome,all,dialog=no");
	}
		litmus.getTestcase('22', qaMain.populateTestcase);
	},
	submitResult : function() {
		var rs;
		var item = $('qa-testcase-result').selectedItem;
		if (item.id == "qa-testcase-pass") {
			rs = 'Pass';
		} else if (item.id == "qa-testcase-fail") {
			rs = 'Fail';
		} else if (item.id == "qa-testcase-unclearBroken") {
			rs = 'Test unclear/broken';
		} else {
			// no result selected, so don't submit anything for thes test:
			return false;
		}
	
		var l = new LitmusResults({username: qaPref.litmus.getUsername(), 
      							  password: qaPref.litmus.getPassword(),
      							  server: litmus.baseURL});
		l.sysconfig(new Sysconfig());
		
		l.addResult(new Result({
			testid: curtest.testcase_id,
			resultstatus: rs,
			exitstatus: 'Exited Normally',
			duration: 0,
			comment: $('qa-testcase-comment').value,
			isAutomatedResult: 0
		}));
		
		var callback = function(resp) {
			alert("yay");
		};
		
		var errback = function(resp) {
			alert(resp.responseText);
		};
		
		litmus.postResultXML(l.toXML(), callback, errback);
  },
};
qaMain.__defineGetter__("bundle", function(){return $("bundle_qa");});
qaMain.__defineGetter__("urlbundle", function(){return $("bundle_urls");});

function $() {
  var elements = new Array();

  for (var i = 0; i < arguments.length; i++) {
    var element = arguments[i];
    if (typeof element == 'string')
      element = document.getElementById(element);

    if (arguments.length == 1)
      return element;

    elements.push(element);
  }

  return elements;
}