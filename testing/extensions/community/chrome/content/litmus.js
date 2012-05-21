/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


 const FIREFOX_ID = "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}";

 var litmus = {
  baseURL : qaPref.getPref(qaPref.prefBase+".litmus.url", "char"),
  userStats : '',

  getTestcase : function(testcase_id, callback) {
        litmus.getLitmusJson(testcase_id, callback, "testcase_id=");
  },
  getSubgroup : function(subgroupID, callback) {
        litmus.getLitmusJson(subgroupID, callback, "subgroup_id=");
    },
    getTestgroup : function(testgroupID, callback) {
        litmus.getLitmusJson(testgroupID, callback, "testgroup_id=");
    },
    getTestrun : function(testrunID, callback) {
        litmus.getLitmusJson(testrunID, callback, "test_run_id=");
    },
    getTestruns : function(callback) {
        var s = new Sysconfig();
        var branch = encodeURIComponent(s.branch);
        litmus.getLitmusJson("&product_name=Firefox&branch_name=" + branch,
                        callback, "test_runs_by_branch_product_name=1");
    },
    getLitmusJson : function(ID, callback, prefix) {
        var url = litmus.baseURL+'json.cgi?' + prefix + ID;
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
            dump(err);
        });
    },


    lastTestRunSummary : "",
    lastTestGroupSummary : "",
    lastSubgroupObject: null,    // these have to be objects to avoid the async call later.
    lastTestcaseObject: null,    // they are saved every time a subgroup or testcase is written to screen
    lastTestcaseIndex: null,

    preDialogSubgroupObject: null,  // saved to be restored in case of Cancel()
    preDialogTestcaseObject: null,

    dialogActive: false,            // we want to keep controls disabled, even if the selection changes.

    storeSubgroup : function(subgroup) {
        litmus.lastSubgroupObject = subgroup;
    },
    storeTestcase : function(testcase) {
        litmus.lastTestcaseObject = testcase;
    },

    handleDialog : function() {
        if ($("qa-testrun-label").label != "") {
            litmus.lastTestRunSummary = $("qa-testrun-label").label;
            litmus.lastTestGroupSummary = $("qa-testgroup-label").value;
            lastTestcaseIndex = $("testlist").selectedIndex;
        }
        litmus.preDialogueSubgroupObject = litmus.lastSubgroupObject;
        litmus.preDialogTestcaseObject = litmus.lastTestcaseObject;
        litmus.disableAll();
        litmus.dialogActive = true;
        var newWindow = window.openDialog('chrome://qa/content/tabs/selecttests.xul', '_blank', 'chrome,all,dialog=yes',
                                          litmus.readStateFromPref, litmus.handleDialogCancel, litmus.handleDialogOK);
    },

    handleDialogCancel : function() {
        if (litmus.lastTestRunSummary == "") return;

        // this code is v. similar to readStateFromPref, but without an async call.
        $("qa-testrun-label").label = litmus.lastTestRunSummary;
        $("qa-testgroup-label").value = litmus.lastTestGroupSummary;
        litmus.lastSubgroupObject = litmus.preDialogueSubgroupObject;
        litmus.lastTestcaseObject = litmus.preDialogTestcaseObject;

        if (litmus.lastSubgroupObject != null)
            litmus.populatePreviewBox(litmus.lastSubgroupObject.testcases);
        if (litmus.lastTestcaseObject != null) {
            litmus.populateTestcase(litmus.lastTestcaseObject);
        }
        // set the selection without firing an event, which would cause async call
        $("testlist").setAttribute("suppressonselect", "true");
        $("testlist").selectedIndex = lastTestcaseIndex;
        $("testlist").setAttribute("suppressonselect", "false");

        // rewrite the prefs
        litmus.writeStateToPref(litmus.lastTestRunSummary, litmus.lastTestGroupSummary,
                                litmus.lastSubgroupObject.subgroup_id, lastTestcaseIndex);

        litmus.dialogActive = false;
        litmus.undisableAll();
    },

    handleDialogOK : function() {
        litmus.dialogActive = false;
        litmus.undisableAll();
    },

  validateLogin : function(uname, passwd, callback) {
    var qs = queryString({
        validate_login: 1,
          username: uname,
          password: passwd
      });
      qaTools.httpPostRequest(litmus.baseURL+'json.cgi', qs, callback);
  },
  postResultXML : function(xml, callback, errback) {
    var qs = queryString({ data: xml});
    var fake_callback = function(resp) {
      // only call callback() if we really had a sucuess. XML
      // processing errors should result in a call to errback()
      if ((/^ok/i).exec(resp.responseText)) {
        callback(resp);
      } else {
        errback(resp);
      }
    };
    qaTools.httpPostRequest(litmus.baseURL+'process_test.cgi',
                        qs, fake_callback, errback);
  },

    currentSubgroupID: null,

    writeStateToPref : function(testrunSummary, testgroupSummary, subgroupID, index) {
        qaPref.setPref(qaPref.prefBase + ".currentTestcase.testrunSummary", testrunSummary, "char");
        qaPref.setPref(qaPref.prefBase + ".currentTestcase.testgroupSummary", testgroupSummary, "char");
        qaPref.setPref(qaPref.prefBase + ".currentTestcase.subgroupId", subgroupID, "int");
        qaPref.setPref(qaPref.prefBase + ".currentTestcase.testcaseIndex", index, "int");
    },
    readStateFromPref : function() {
        $("qa-testrun-label").label = qaPref.getPref(qaPref.prefBase + ".currentTestcase.testrunSummary", "char");
        $("qa-testgroup-label").value = qaPref.getPref(qaPref.prefBase + ".currentTestcase.testgroupSummary", "char");
        litmus.currentSubgroupID = qaPref.getPref(qaPref.prefBase + ".currentTestcase.subgroupId", "int");
        litmus.disableAll();

        if (litmus.currentSubgroupID != 0)
            litmus.getSubgroup(litmus.currentSubgroupID, function(subgroup) {litmus.statePopulateFields(subgroup); litmus.undisableAll();});
    },
    loadStats : function() {
      // pull new user statistics from litmus
      var url = litmus.baseURL+'json.cgi?' + 'user_stats=' + qaPref.litmus.getUsername();
      var req = loadJSONDoc(url);
      req.addCallback(function(data) {
        litmus.userStats = data;
        litmus.displayStats();
      });
    },
    displayStats : function() {
      var statbox = $('qa-litmus-stats');
      if (litmus.userStats != '') {
        var statline = qaMain.bundle.getFormattedString('qa.extension.litmus.stats',
          [litmus.userStats.week, litmus.userStats.month, litmus.userStats.alltime]);
        statbox.value = statline;
      }
    },
    checkRadioButtons : function() {
        var menu = document.getElementById('testlist');
        if (menu.selectedIndex == -1) return;
        var disable = menu.selectedItem.firstChild.getAttribute("checked");
        document.getElementById("qa-testcase-result").disabled = disable;
    },

    prevButton : function() {
        $("testlist").selectedIndex--;
    },
  nextButton: function() {
    // if they selected a result, then submit the result
    if ($('qa-testcase-result').selectedItem) {
      litmus.submitResult();
    }
        $("testlist").selectedIndex++;
  },
    handleSelect : function() {
        if ($("testlist").selectedIndex == -1)
            return;

        litmus.disableAll();
        litmus.getTestcase($("testlist").selectedItem.value, function(testcase) {
          litmus.populateTestcase(testcase);
          $('qa-testcase-progress').label =
            qaMain.bundle.getFormattedString('qa.extension.litmus.progress',
              [$("testlist").selectedIndex+1, $("testlist").getRowCount()]);
            litmus.undisableAll();
        });
    },
  populatePreviewBox : function(testcases) {

        var menu = document.getElementById('testlist');
        if (!menu) return;

        while (menu.firstChild) {  // clear menu
            menu.removeChild(menu.firstChild);
        };

        for (var i = 0; i < testcases.length; i++) {
            var row = document.createElement("richlistitem");
            row.value = testcases[i].testcase_id;
                      
            var checkbox = document.createElement("listcell");
            checkbox.setAttribute("label", "");
            checkbox.setAttribute("type", "checkbox");
            checkbox.setAttribute("disabled", "true");
            
            var name = document.createElement("listcell");
            name.setAttribute("label", (i+1) + " -- " + testcases[i].summary);
            name.setAttribute("crop", "end");
            name.setAttribute("flex", "1");
            
            row.appendChild(checkbox);
            row.appendChild(name);
            menu.appendChild(row);
        }        

    },
    populateTestcase : function(testcase) {
        litmus.lastTestcaseObject = testcase;
        if (testcase == undefined) {
                return;
            }
        document.getElementById('qa-testcase-id').value = "(" +
            qaMain.bundle.getString("qa.extension.testcase.head")+testcase.testcase_id + ")";
        document.getElementById('qa-testcase-summary').value = testcase.summary;

        qaTools.writeSafeHTML('qa-testcase-steps', testcase.steps_formatted);
        qaTools.writeSafeHTML('qa-testcase-expected', testcase.expected_results_formatted);

        qaTools.assignLinkHandlers($('qa-testcase-steps'));
        qaTools.assignLinkHandlers($('qa-testcase-expected'));

        if (testcase.regression_bug_id) {
            bugzilla.loadBug(testcase.regression_bug_id);
        } else {
            var bugarray = bugzilla.findBugzillaLinks($("qa-testcase-expected"));
            if (bugarray.length) {
               bugzilla.loadBug(bugarray[0]);
               for (var i = 1; i < bugarray.length; i++) {
                  bugzilla.loadBug(bugarray[i], true);
               }
            }
        }
        litmus.checkRadioButtons();
    },
  populateFields : function(subgroup) {
        litmus.lastSubgroupObject = subgroup;
    litmus.populatePreviewBox(subgroup.testcases);
    $('qa-subgroup-label').value = subgroup.name;
        $("testlist").selectedIndex = 0;
    },
    statePopulateFields : function(subgroup) {  //TODO: there's gotta be a better way to do this...
        litmus.lastSubgroupObject = subgroup;
        litmus.populatePreviewBox(subgroup.testcases);
        $('qa-subgroup-label').value = subgroup.name;

        $("testlist").selectedIndex = qaPref.getPref(qaPref.prefBase + ".currentTestcase.testcaseIndex", "int");
    },
    disableAll : function() {   //
        $("testlist").disabled = true;
        $("qa-testcase-result").disabled = true;
        $("qa-mainwindow-previousButton").disabled = true;
        $("qa-mainwindow-nextButton").disabled = true;

    },
    undisableAll : function() {
         if(litmus.dialogActive) return;   // ignore all requests while there is a dialog open

        $("testlist").disabled = false;
        $("qa-testcase-result").disabled = false;
        $("qa-mainwindow-previousButton").disabled = false;
        $("qa-mainwindow-nextButton").disabled = false;

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

        var menu = document.getElementById('testlist');

    var l = new LitmusResults({username: qaPref.litmus.getUsername(),
                  password: qaPref.litmus.getPassword(),
                  server: litmus.baseURL});
    l.sysconfig(new Sysconfig());

    l.addResult(new Result({
      testid: menu.selectedItem.value,
      resultstatus: rs,
      exitstatus: 'Exited Normally',
      duration: 0,
      comment: $('qa-testcase-comment').value,
      isAutomatedResult: 0
    }));

    var callback = function(resp) {
      // increment the stat counters:
      for (var i in litmus.userStats) {
        litmus.userStats[i]++;
      }
      litmus.displayStats();
    };

    var errback = function(resp) {
      dump(resp.responseText);
    };

    litmus.postResultXML(l.toXML(), callback, errback);
        var item = menu.selectedItem;
        item.firstChild.setAttribute("checked", "true");
        return false;   // ?? Got rid of strict warning...
    }
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
  platform: null,
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
      this.branch = '3.0 Branch';
    } else if ((/^2\.0/).exec(appinfo.version)) {
      this.branch = '2.0 Branch';
    } else if ((/^1\.5\./).exec(appinfo.version)) {
      this.branch = '1.5 Branch';
    }
    if (! this.branch) { throw "branch" }

        // platform:
        if (! this.platform) {
      if ((/^MacPPC/).exec(navigator.platform)) {
        this.platform = 'Mac (PPC)';
      } else if ((/^MacIntel/).exec(navigator.platform)) {
        this.platform = 'Mac (Intel)';
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
        } else if (this.platform == 'Mac (PPC)' || this.platform == 'Mac (Intel)') {
          // no way to detect the OS on mac, so just assume
          // it's 10.4. The user can provide the real OS in setup
          this.opsys = 'Mac OS 10.4';
        }
        if (! this.opsys) {throw "opsys" }

        // locale
    this.locale = navigator.language;
    if (!this.locale) { throw "locale" }
  }
};
