var subgroup;
var filter_req;

var showAll = function(err) {
	// if they cancelled, then just don't change anything:
	if (err instanceof CancelledError) { return }
	toggleMessage('none');
	
	var testbox = document.getElementById("subgroup_id");
	for (var i=0; i<testbox.options.length; i++) {
		var option = testbox.options[i];
		option.style.display = '';
	}
};

var doFilterList = function(req) {
	var subgroups = req.responseText.split("\n");
	var subgroupbox = document.getElementById("subgroup_id");
	for (var i=0; i<subgroupbox.options.length; i++) {
		var subgroup = subgroupbox.options[i];
		var hide = 0;
		var id = subgroup.value;
		if (subgroups.indexOf(id) == -1) { hide = 1; }
		hide == 1 ? subgroup.style.display = 'none' : subgroup.style.display = '';
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
	
	if (productfilter.options[productfilter.selectedIndex].value == '---' &&
	    testgroupfilter.options[testgroupfilter.selectedIndex].value == '---') {
		// nothing to do here
		showAll();
		return;
	}
	
	toggleMessage('loading','Filtering subgroup list...');
	filter_req = doSimpleXMLHttpRequest('manage_subgroups.cgi', {
	  searchSubgroupList: 1,
	  product: (productfilter.options[productfilter.selectedIndex].value == '---' ?
	  		'' : productfilter.options[productfilter.selectedIndex].value),
	  testgroup: (testgroupfilter.options[testgroupfilter.selectedIndex].value == '---' ? 
	  		'' : testgroupfilter.options[testgroupfilter.selectedIndex].value)
	  });
	// if something went wrong, just show all the tests:
	filter_req.addErrback(showAll);
	filter_req.addCallback(doFilterList);
}


function enableModeButtons() {
  document.getElementById("edit_subgroup_button").disabled=false;
  document.getElementById("clone_subgroup_button").disabled=false;
  document.getElementById("delete_subgroup_button").disabled=false;
}

function disableModeButtons() {
  document.getElementById("edit_subgroup_button").disabled=true;
  document.getElementById("clone_subgroup_button").disabled=true;
  document.getElementById("delete_subgroup_button").disabled=true;
}

function loadSubgroup(silent) {
  var subgroup_select = document.getElementById("subgroup_id");

  if (! subgroup_select ||
      subgroup_select.options[subgroup_select.selectedIndex].value=="") {
    disableModeButtons();
    document.getElementById('subgroup_display_div').style.display = 'none';
    document.getElementById('editform_div').style.display = 'none';
    disableForm('edit_subgroup_form');
    blankSubgroupForm('edit_subgroup_form');
    return false;
  } 

  var subgroup_id = subgroup_select.options[subgroup_select.selectedIndex].value;

  disableForm('edit_subgroup_form');
  var url = 'json.cgi?subgroup_id=' + subgroup_id;
  if (!silent) {
    toggleMessage('loading','Loading Subgroup ID# ' + subgroup_id + '...');
  }
  fetchJSON(url,populateSubgroup,silent);
}

function populateSubgroup(data) {
  subgroup=data;
  document.getElementById('editform_subgroup_id').value = subgroup.subgroup_id;
  document.getElementById('editform_subgroup_id_display').innerHTML = subgroup.subgroup_id;
  document.getElementById('editform_name').value = subgroup.name;
  document.getElementById('name_text').innerHTML = subgroup.name;
  document.getElementById('subgroup_id_display').innerHTML = subgroup.subgroup_id;

  var productBox = document.getElementById('product');
  var options = productBox.getElementsByTagName('option');  
  var found_product = 0;
  for (var i=0; i<options.length; i++) {
    if (options[i].value == subgroup.product_id.product_id) {
      options[i].selected = true;
      document.getElementById('product_text').innerHTML = options[i].text;
      found_product=1;
    } else {
      options[i].selected = false;
    }      
  }
  if (found_product == 0 && options) {
    options[0].selected = true;
  }
  changeProduct();

  var testgroups_text = "";
  var testgroups_link_text = "";
  for (var i in subgroup.testgroups) {
    if (subgroup.testgroups[i].name != '') {
      testgroups_text = testgroups_text + subgroup.testgroups[i].name + ', ';
      testgroups_link_text = testgroups_link_text + '<a target="manage_testgroups" href="manage_testgroups.cgi?testgroup_id=' + subgroup.testgroups[i].testgroup_id + '">'+ subgroup.testgroups[i].name + '</a>, ';
    }
  }
  if (testgroups_text != '') {
    testgroups_text = testgroups_text.replace(/, $/g,'');
    testgroups_link_text = testgroups_link_text.replace(/, $/g,'');
    document.getElementById('testgroups_display').innerHTML = testgroups_text;
    document.getElementById('testgroups_link_display').innerHTML = testgroups_link_text;
  } else {
    document.getElementById('testgroups_display').innerHTML = '<span class="errorHeading">This subgroup does not belong to any testgroups that are currently enabled.</span>';
    document.getElementById('testgroups_link_display').innerHTML = '<span class="errorHeading">This subgroup does not belong to any testgroups that are currently enabled &rArr;&nbsp;<a target="manage_testgroups" href="manage_testgroups.cgi">Jump to Manage Testgroups</a>.</span>';
  }

  var enabled_em = document.getElementById('editform_enabled')
  if (subgroup.enabled == 1) {
    enabled_em.checked = true;
  } else {
    enabled_em.checked = false;
  } 
  document.getElementById('editform_testrunner_group_id').innerHTML = subgroup.testrunner_group_id;

  populateAllTestcases();

  var selectBoxSubgroup = document.getElementById('editform_subgroup_testcases');
  selectBoxSubgroup.options.length = 0;
  for (var i=0; i<subgroup.testcases.length; i++) {
    var optionText = subgroup.testcases[i].testcase_id + ': ' + subgroup.testcases[i].summary;
    selectBoxSubgroup.options[selectBoxSubgroup.length] = new Option(optionText,
                                                     subgroup.testcases[i].testcase_id);
  }

  document.getElementById('editform_div').style.display = 'none';
  document.getElementById('subgroup_display_div').style.display = 'block';
  enableModeButtons();
}

function blankSubgroupForm(formid) {
  blankForm(formid);
  document.getElementById('editform_subgroup_id_display').innerHTML = '';
  var selectBoxAll = document.getElementById('editform_testcases_for_product');
  selectBoxAll.options.length = 0;
  selectBoxAll.options[selectBoxAll.length] = new Option("--No product selected--",
                                                             "");
  selectBoxAll.selectedIndex=-1;
  var selectBoxSubgroup = document.getElementById('editform_subgroup_testcases');
  selectBoxSubgroup.options.length = 0;
  selectBoxSubgroup.options[selectBoxSubgroup.length] = new Option("--No subgroup selected--",
                                                             "");
  selectBoxSubgroup.selectedIndex=-1;
 
  document.getElementById('editform_testrunner_group_id').innerHTML = '';

  changeProduct();
  changeTestgroup();
}

function switchToAdd() {
  disableModeButtons();
  blankSubgroupForm('edit_subgroup_form');
  document.getElementById('editform_subgroup_id_display').innerHTML = '<em>Automatically generated for a new subgroup</em>';
  document.getElementById('testgroups_link_display').innerHTML = '<em>A new subgroup does not belong to any testgroups by default.<br/>Use the <a target="manage_testgroups" href="manage_testgroups.cgi">Manage Testgroups</a> interface to assign the subgroup to testgroups after the new subgroup is created.</em>';
 document.getElementById('editform_testrunner_group_id').innerHTML = '<em>Not Applicable</em>';
 document.getElementById('editform_submit').value = 'Add Subgroup';
  document.getElementById('editform_mode').value = 'add';
  enableForm('edit_subgroup_form');
  document.getElementById('subgroup_display_div').style.display = 'none';
  document.getElementById('editform_div').style.display = 'block';
}

function switchToEdit() {
  document.getElementById('editform_submit').value = 'Submit Edits';
  document.getElementById('editform_mode').value = 'edit';
  enableForm('edit_subgroup_form');
  document.getElementById('subgroup_display_div').style.display = 'none';
  document.getElementById('editform_div').style.display = 'block';
}

function populateAllTestcases() {
  var productBox = document.getElementById('product');
  var selectBoxAll = document.getElementById('editform_testcases_for_product');
  selectBoxAll.options.length = 0;
  for (var i in testcases) {
    if (testcases[i].product_id == productBox.options[productBox.selectedIndex].value) {
      var optionText = testcases[i].testcase_id + ': ' + testcases[i].summary;
    selectBoxAll.options[selectBoxAll.length] = new Option(optionText,
                                                               testcases[i].testcase_id);
    }
  }
}  

function resetSubgroup() {
  if (document.getElementById('editform_subgroup_id').value != '') {
    populateSubgroup(subgroup);
    switchToEdit();   
  } else {
    switchToAdd();
  }
}

function checkFormContents(f) {
  return (
          checkString(f.editform_name, 'Name') &&
          verifySelected(f.product, 'Product')
         );
}

function previewTestcase(selectID) {
  var selectBox = document.getElementById(selectID);
  if (selectBox) {
    if (selectBox.selectedIndex >= 0) {
      if (selectBox.options[selectBox.selectedIndex].value != '') {
        window.open('show_test.cgi?id=' + selectBox.options[selectBox.selectedIndex].value,
                    'testcase_preview');
      }
    } else {
      toggleMessage('failure','No testcase selected!');
    }
  }
}

var manageTestcasesHelpTitle="Help with Managing Testcases";
var manageTestcasesHelpText="<p>The select box on the left contains all the testcases for the chosen product, <strong><em>INCLUDING</em></strong> any testcases already contained in the subgroup. You can use the <input type='button' value='&rArr;' disabled> button to add testcases to the subgroup, and the <input type='button' value='&lArr;' disabled> button to remove testcases from the subgroup. <strong>NOTE</strong>: neither of the actions will alter the list of testcases on the left.</p><p>You can preview any testcase from the left-hand select box by selecting the testcase, and then clicking on  the 'Preview Testcase' link below the select box. If more than one testcase is selected, only the first testcase will be previewed.</p><p>You can change the display order of testcases within the subgroup using the <input type='button' value='&uArr;' disabled> and <input type='button' value='&dArr;' disabled> buttons to the right of the right-hand select box. Testcases can be re-ordered singly or in groups by selecting multiple testcases in the right-hand select box.</p>";
