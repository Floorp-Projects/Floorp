var testcase;
var filter_req;

var showAllTests = function(err) {
	// if they cancelled, then just don't change anything:
	if (err instanceof CancelledError) { return }
	toggleMessage('none');
	
	var testbox = document.getElementById("testcase_id");
	for (var i=0; i<testbox.options.length; i++) {
		var option = testbox.options[i];
		option.style.display = '';
	}
};

function splitIt(it) {
	return it.split("\n");
}

var doFilterList = function(req) {
	var tests = splitIt(req.responseText);
	var testbox = document.getElementById("testcase_id");
	var l = testbox.options.length;
	var hideTest;
	for (var i=0; i<l; i++) {
		var test = testbox.options[i];
		if (tests.indexOf(test.value) == -1) { hideTest = 1; }
		else { hideTest=0 } 
		hideTest == 1 ? test.style.display = 'none' : test.style.display = '';
	}
	toggleMessage('none');
};

// filter the list by various criteria:
function filterList() {
	// they just changed the selection, so cancel any pending filter actions:
	if (filter_req instanceof Deferred && filter_req.fired == -1)
	filter_req.cancel();

	var productfilter = document.getElementById('product_filter');
	var testgroupfilter = document.getElementById('testgroup_filter');
	var subgroupfilter = document.getElementById('subgroup_filter');
	
	if (productfilter.options[productfilter.selectedIndex].value == '' &&
	    testgroupfilter.options[testgroupfilter.selectedIndex].value == '' &&
	    subgroupfilter.options[subgroupfilter.selectedIndex].value == '') {
		// nothing to do here
		showAllTests();
		return;
	}
	
	toggleMessage('loading','Filtering testcase list...');
	filter_req = doSimpleXMLHttpRequest('manage_testcases.cgi', {
	  searchTestcaseList: 1,
	  product: (productfilter.options[productfilter.selectedIndex].value == '---' ?
	  		'' : productfilter.options[productfilter.selectedIndex].value),
	  testgroup: (testgroupfilter.options[testgroupfilter.selectedIndex].value == '---' ? 
	  		'' : testgroupfilter.options[testgroupfilter.selectedIndex].value),
	  subgroup: (subgroupfilter.options[subgroupfilter.selectedIndex].value == '---' ? 
	  		'' : subgroupfilter.options[subgroupfilter.selectedIndex].value),
	  });
	// if something went wrong, just show all the tests:
	filter_req.addErrback(showAllTests);
	filter_req.addCallback(doFilterList);
}

function setAuthor(user_id) {
  changeSelectedValue('editform_author_id',user_id);;
}

function enableModeButtons() {
  document.getElementById("edit_testcase_button").disabled=false;
  document.getElementById("clone_testcase_button").disabled=false;
  document.getElementById("delete_testcase_button").disabled=false;
}

function disableModeButtons() {
  document.getElementById("edit_testcase_button").disabled=true;
  document.getElementById("clone_testcase_button").disabled=true;
  document.getElementById("delete_testcase_button").disabled=true;
}

function loadTestcase(silent) {
  var testcase_select = document.getElementById("testcase_id");

  if (! testcase_select ||
      testcase_select.options[testcase_select.selectedIndex].value=="") {
    disableModeButtons();
    document.getElementById('testcase_display_div').style.display = 'none';
    document.getElementById('editform_div').style.display = 'none';
    disableForm('edit_testcase_form');
    blankTestcaseForm('edit_testcase_form');
    return false;
  } 

  var testcase_id = testcase_select.options[testcase_select.selectedIndex].value;

  disableForm('edit_testcase_form');
  if (!silent) {
    toggleMessage('loading','Loading Testcase ID# ' + testcase_id + '...');
  }
  var url = 'json.cgi?testcase_id=' + testcase_id;
  fetchJSON(url,populateTestcase,silent);
}

function populateTestcase(data) {
  testcase=data;
  document.getElementById('editform_testcase_id').value = testcase.testcase_id;
  document.getElementById('editform_testcase_id_display').innerHTML = testcase.testcase_id;
  document.getElementById('editform_summary').value = testcase.summary;
  document.getElementById('editform_steps').value = testcase.steps;
  document.getElementById('editform_results').value = testcase.expected_results;
  document.getElementById('testcase_id_display').innerHTML = testcase.testcase_id;
  document.getElementById('summary_text').innerHTML = testcase.summary;
  document.getElementById('steps_text').innerHTML = testcase.steps_formatted;
  document.getElementById('results_text').innerHTML = testcase.expected_results_formatted;

  var product_box = document.getElementById('product');
  var options = product_box.getElementsByTagName('option');  
  var found_product = 0;
  for (var i=0; i<options.length; i++) {
    if (options[i].value == testcase.product_id.product_id) {
      options[i].selected = true;
      document.getElementById('product_text').innerHTML = options[i].text;
      found_product=1;
    } else {
      options[i].selected = false;
    }
  }
  if (found_product == 0 && options[0]) {
    options[0].selected = true;
  }
  changeProduct();

  var testgroups_text = "";
  var testgroups_link_text = "";
  for (var i in testcase.testgroups) {
    if (testcase.testgroups[i].name != '') {
      testgroups_text = testgroups_text + testcase.testgroups[i].name + ', ';
      testgroups_link_text = testgroups_link_text + '<a target="manage_testgroups" href="manage_testgroups.cgi?testgroup_id=' + testcase.testgroups[i].testgroup_id + '">'+ testcase.testgroups[i].name + '</a>, ';

    }
  }
  if (testgroups_text != '') {
    testgroups_text = testgroups_text.replace(/, $/g,'');
    testgroups_link_text = testgroups_link_text.replace(/, $/g,'');
    document.getElementById('testgroups_display').innerHTML = testgroups_text;
    document.getElementById('testgroups_link_display').innerHTML = testgroups_link_text;
  } else {
    document.getElementById('testgroups_display').innerHTML = '<span class="errorHeading">This testcase does not belong to any testgroups that are currently enabled.</span>';
    document.getElementById('testgroups_link_display').innerHTML = '<span class="errorHeading">This testcase does not belong to any testgroups that are currently enabled &rArr;&nbsp;<a target="manage_testgroups" href="manage_testgroups.cgi">Jump to Manage Testgroups</a>.</span>';
  }

  var subgroups_text = "";
  var subgroups_link_text = "";
  for (var i in testcase.subgroups) {
    if (testcase.subgroups[i].name != '') {
      subgroups_text = subgroups_text + testcase.subgroups[i].name + ', ';
      subgroups_link_text = subgroups_link_text + '<a target="manage_subgroups" href="manage_subgroups.cgi?subgroup_id=' + testcase.subgroups[i].subgroup_id + '">'+ testcase.subgroups[i].name + '</a>, ';
    }
  }
  if (subgroups_text != '') {
    subgroups_text = subgroups_text.replace(/, $/g,'');
    subgroups_link_text = subgroups_link_text.replace(/, $/g,'');
    document.getElementById('subgroups_display').innerHTML = subgroups_text;
    document.getElementById('subgroups_link_display').innerHTML = subgroups_link_text;
  } else {
    document.getElementById('subgroups_display').innerHTML = '<span class="errorHeading">This testcase does not belong to any subgroups that are currently enabled.</span>';
    document.getElementById('subgroups_link_display').innerHTML = '<span class="errorHeading">This testcase does not belong to any subgroups that are currently enabled &rArr;&nbsp;<a target="manage_subgroups" href="manage_subgroups.cgi">Jump to Manage Subgroups</a>.</span>';
  }

  var enabled_em = document.getElementById('editform_enabled')
  if (testcase.enabled == 1) {
    enabled_em.checked = true;
  } else {
    enabled_em.checked = false;
 } 
 var communityenabled_em = document.getElementById('editform_communityenabled')
 if (testcase.community_enabled == 1) {
    communityenabled_em.checked = true;
  } else {
    communityenabled_em.checked = false;
  }
  if (testcase.regression_bug_id) {
    document.getElementById('editform_regression_bug_id').value = testcase.regression_bug_id;
  }
  setAuthor(testcase.author_id.user_id);
  document.getElementById('editform_creation_date').innerHTML = testcase.creation_date;
  document.getElementById('editform_last_updated').innerHTML = testcase.last_updated;
  document.getElementById('editform_litmus_version').innerHTML = testcase.version;
  document.getElementById('editform_testrunner_case_id').innerHTML = testcase.testrunner_case_id;
  document.getElementById('editform_testrunner_case_version').innerHTML = testcase.testrunner_case_version;

  document.getElementById('editform_div').style.display = 'none';
  document.getElementById('testcase_display_div').style.display = 'block';
  enableModeButtons();
}

function blankTestcaseForm(formid) {
  blankForm(formid);
  document.getElementById('editform_testcase_id_display').innerHTML = '';
  document.getElementById('editform_creation_date').innerHTML = '';
  document.getElementById('editform_last_updated').innerHTML = '';
  document.getElementById('editform_litmus_version').innerHTML = '';
  document.getElementById('editform_testrunner_case_id').innerHTML = '';
  document.getElementById('editform_testrunner_case_version').innerHTML = '';
  changeProduct();
}

function switchToAdd() {
  disableModeButtons();
  blankTestcaseForm('edit_testcase_form');
  setAuthor(current_user_id);
  document.getElementById('editform_submit').value = 'Add Testcase';
  document.getElementById('editform_mode').value = 'add';
  enableForm('edit_testcase_form');
  document.getElementById('testcase_display_div').style.display = 'none';
  document.getElementById('editform_testcase_id_display').innerHTML = '<em>Automatically generated for a new testcase</em>';
  document.getElementById('testgroups_link_display').innerHTML = '<em>A new testcase does not belong to any testgroups by default.<br/>Use the <a target="manage_testgroups" href="manage_testgroups.cgi">Manage Testgroups</a> interface to assign the subgroups to testgroups after the new testcase is created.</em>';
  document.getElementById('subgroups_link_display').innerHTML = '<em>A new testcase does not belong to any subgroups by default.<br/>Use the <a target="manage_subgroups" href="manage_subgroups.cgi">Manage Subgroups</a> interface to assign the new testcase to subgroups once it is created.</em>';
  document.getElementById('editform_creation_date').innerHTML = '<em>Automatically generated for a new testcase</em>';
  document.getElementById('editform_last_updated').innerHTML = '<em>Automatically generated for a new testcase</em>';
  document.getElementById('editform_litmus_version').innerHTML = '<em>Automatically generated for a new testcase</em>';
  document.getElementById('editform_testrunner_case_id').innerHTML = '<em>Not Applicable</em>';
  document.getElementById('editform_testrunner_case_version').innerHTML = '<em>Not Applicable</em>';
  document.getElementById('editform_div').style.display = 'block';
}

function switchToEdit() {
  document.getElementById('editform_submit').value = 'Submit Edits';
  document.getElementById('editform_mode').value = 'edit';
  enableForm('edit_testcase_form');
  document.getElementById('testcase_display_div').style.display = 'none';
  document.getElementById('editform_div').style.display = 'block';
}

function resetTestcase() {
  if (document.getElementById('editform_testcase_id').value != '') {
    populateTestcase(testcase);
    switchToEdit();   
  } else {
    switchToAdd();
  }
}

function checkFormContents(f) {
  return (
          checkString(f.editform_summary, 'Summary') &&
          verifySelected(f.product, 'Product') &&
          verifySelected(f.editform_author_id, 'Author')
         );
}