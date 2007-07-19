 const FIREFOX_ID = "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}";
 
 var litmus = {
	baseURL : qaPref.getPref(qaPref.prefBase+".litmus.url", "char"),
	
	getTestcase : function(testcase_id, callback) {
		var url = litmus.baseURL+'json.cgi?testcase_id='+testcase_id;
		var d = loadJSONDoc(url);
		d.addBoth(function (res) { 
		d.deferred = null;  
		return res; 
		});
		d.addCallback(callback);
		d.addErrback(function (err) { 
		if (err instanceof CancelledError) { 
			return; 
		}
			alert(err);
		});
	},
	
	getSubgroup : function(subgroupID, callback) {
        var url = litmus.baseURL+'json.cgi?subgroup_id='+subgroupID;
        var d = loadJSONDoc(url);
		d.addBoth(function (res) { 
			d.deferred = null;  
			return res; 
		});
		d.addCallback(callback);
		d.addErrback(function (err) { 
            if (err instanceof CancelledError) { 
                return; 
            }
            	alert(err);
            });
        },
	
	validateLogin : function(uname, passwd, callback) {
	    var req = doSimpleXMLHttpRequest(litmus.baseURL+'json.cgi', {
	       validate_login: 1,
	       username: uname,
	       password: passwd,
	    });
	    req.addErrback(callback);
	    req.addCallback(callback);
	},
	createAccount : function() {
	    alert("XXX: not implemented");
	},
	postResultXML : function(xml, callback, errback) {
		var req = doSimpleXMLHttpRequest(litmus.baseURL+'process_test.cgi', 
										    { data: xml });
		req.addErrback(errback);
		req.addCallback(function(resp) {
			// only call callback() if we really had a sucuess. XML 
			// processing errors should result in a call to errback()
			if ((/ok/i).exec(resp.responseText)) {
				callback(resp);
			} else {
				errback(resp);
			}
		});
	},
    
    
    currentTestCaseIndex: 0, // position in array
	
   currentSubgroupID: 21,	
	cachedTests: null,
    
    checkRadioButtons : function() {
        var menu = document.getElementById('testlist');
        var disable = menu.selectedItem.firstChild.getAttribute("checked");
        document.getElementById("qa-testcase-result").disabled = disable;
    },
    
    prevButton : function() {
        litmus.currentTestCaseIndex--;
        litmus.selectCurrentTestCase();
    },
	nextButton: function() {
		// if they selected a result, then submit the result
		if ($('qa-testcase-result').selectedItem) {
			litmus.submitResult();
		}
        litmus.currentTestCaseIndex++;
		litmus.selectCurrentTestCase();
	},
    handleSelect : function() {
        litmus.currentTestCaseIndex = document.getElementById('testlist').selectedIndex;
        litmus.selectCurrentTestCase();
    },
    selectCurrentTestCase : function() {

        var menu = document.getElementById('testlist');
        menu.selectedIndex = litmus.currentTestCaseIndex;
        litmus.populateTestcase(litmus.cachedTests[litmus.currentTestCaseIndex]);
        litmus.checkRadioButtons();
    },
	populatePreviewBox : function() {
        //document.getElementById("prev1").innerHTML = "hi!"; This doesn't even work, I guess I'll rewrite table in XUL
        
        var menu = document.getElementById('testlist');
        if (!menu) return;
        
        while (menu.firstChild) {  // clear menu
            menu.removeChild(menu.firstChild);
        };
    
        for (var i = 0; i < litmus.cachedTests.length; i++) {
            var row = menu.appendItem("");
            var checkbox = document.createElement("listcell");
            checkbox.setAttribute("label", "");
            checkbox.setAttribute("type", "checkbox");
            checkbox.setAttribute("disabled", "true");
            //checkbox.setAttribute("checked", "true");
            var name = document.createElement("listcell");
            name.setAttribute("label", "#" + litmus.cachedTests[i].testcase_id + " -- " + litmus.cachedTests[i].summary);
            name.setAttribute("crop", "end");
            
            name.setAttribute("maxwidth", "175");
            row.appendChild(checkbox);
            row.appendChild(name);
        }
	},
    populateTestcase : function(testcase) {
        document.getElementById('qa-testcase-id').value = 
            qaMain.bundle.getString("qa.extension.testcase.head")+testcase.testcase_id;
		document.getElementById('qa-testcase-summary').value = testcase.summary;
	
		document.getElementById('qa-testcase-steps').innerHTML = testcase.steps;
		document.getElementById('qa-testcase-expected').innerHTML = testcase.expected_results;
	},
	populateFields : function(subgroup) {
		litmus.cachedTests = subgroup.testcases;
		//litmus.currentTestCaseIndex = 0;
		litmus.populatePreviewBox();
        litmus.currentTestCaseIndex = 0;
        litmus.selectCurrentTestCase();
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
			testid: litmus.cachedTests[litmus.currentTestCaseIndex].testcase_id,
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
        
        var menu = document.getElementById('testlist');
        var item = menu.selectedItem;
        item.firstChild.setAttribute("checked", "true");
        return false;   // ?? Got rid of strict warning...
    },
 };

// any missing fields will be autodetected
function Sysconfig(aProduct, aPlatform, aOpsys, aBranch, aBuildid, aLocale) {
	this._load('product', aProduct);
	this._load('platform', aPlatform);
	this._load('opsys', aOpsys);
	this._load('branch', aBranch);
	this._load('buildid', aBuildid);
	this._load('locale', aLocale);
	this.populate();
}
 
Sysconfig.prototype = {
	product: null,
	platorm: null,
	opsys: null,
	branch: null,
	buildid: null,
	locale: null,
	
	// set a field according to the following priorities:
	// 1. 'setting'
	// 2. qa.extension.sysconfig.fieldname
	// 3. null
	_load: function(fieldname, setting) {
		if (this[fieldname]) { return }
		if (setting) {
			this[fieldname] = setting;
			return;
		} 
		var pref = qaPref.getPref(qaPref.prefBase+'.sysconfig.'+fieldname, 'char');
		if (pref) {
			this[fieldname] = pref;
			return;
		}
	},
	
	// if something cannot be autodetected, an exception is thrown 
	// with the name of the missing field
	populate: function() {
		var appinfo = Components.classes["@mozilla.org/xre/app-info;1"]
                        .getService(Components.interfaces.nsIXULAppInfo);
                        
        // build id:  
		this.buildid = appinfo.appBuildID;
		if (! this.buildid) { throw "buildid" }
        
        // product:
        if (! this.product) {
        	if (appinfo.ID == FIREFOX_ID) {
        		this.product = 'Firefox';
        	}
        	if (! this.product) { throw "product" }
        }
        
        // branch:
        // people switch branches, so we detect this even though it might 
        // already be set in a pref
		if ((/^3\.0/).exec(appinfo.version)) {
			this.branch = 'Trunk';
		} else if ((/^2\.0/).exec(appinfo.version)) {
			this.branch = '2.0 Branch';
		} else if ((/^1\.5\./).exec(appinfo.version)) {
			this.branch = '1.5 Branch';
		}
		if (! this.branch) { throw "branch" }
			
        // platform:
        if (! this.platform) {
			if ((/^Mac/).exec(navigator.platform)) {
				this.platform = 'Mac';
			} else if ((/^Win/).exec(navigator.platform)) {
				this.platform = 'Windows';
			} else if ((/^Linux/).exec(navigator.platform)) {
				this.platform = 'Linux';
			} else if ((/^Solaris/).exec(navigator.platform)) {
				this.platform = 'Solaris';
			}
			if (! this.platform) { throw "platform" }
        }
        // opsys
        if (this.platform == 'Windows') {
        	switch (navigator.oscpu) {
        		case 'Windows NT 5.1':
        			this.opsys = 'Windows XP';
        			break;
        		case 'Windows NT 5.2':
        			this.opsys = 'Windows XP';
        			break;
        		case 'Windows NT 6.0':
        			this.opsys = 'Windows Vista';
        			break;
        		case 'Windows NT 5.0':
        			this.opsys = 'Windows 2000';
        			break;
        		case 'Win 9x 4.90':
        			this.opsys = 'Windows ME';
        			break;
        		case 'Win98':
        			this.opsys = 'Windows 98';
        			break;
        	}
        } else if (this.platform == 'Linux') {
        	this.opsys = 'Linux';
        } else if (this.platform == 'Mac') {
        	// no way to detect the OS on mac, so just assume 
        	// it's 10.4. The user can provide the real OS in setup
        	this.opsys = 'Mac OS 10.4';
        }
        if (! this.opsys) {throw "opsys" }
        
        // locale
		this.locale = navigator.language;
		if (!this.locale) { throw "locale" }
	},
};
