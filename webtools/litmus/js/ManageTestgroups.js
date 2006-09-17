var testgroup;
var filter_req;

var showAll = function(err) {
  // if they cancelled, then just don't change anything:
  if (err instanceof CancelledError) { return }
  toggleMessage('none');
	
  var testgroupbox = document.getElementById("testgroup_id");
  for (var i=0; i<testgroupbox.options.length; i++) {
    var option = testgroupbox.options[i];
    option.style.display = '';
  }
};

var doFilterList = function(req) {
  var testgroups = req.responseText.split("\n");
  var testgroupbox = document.getElementById("testgroup_id");
  for (var i=0; i<testgroupbox.options.length; i++) {
    var testgroup = testgroupbox.options[i];
    var hide = 0;
    var id = testgroup.value;
    if (testgroups.indexOf(id) == -1) { hide = 1; }
    hide == 1 ? testgroup.style.display = 'none' : testgroup.style.display = '';
  }
  toggleMessage('none');
};

// filter the list by various criteria:
function filterList() {
  // they just changed the selection, so cancel any pending filter actions:
  if (filter_req instanceof Deferred && filter_req.fired == -1)
    filter_req.cancel();

  var productfilter = document.getElementById('product_filter');

  if (productfilter.options[productfilter.selectedIndex].value == '---') {
    // nothing to do here
    showAll();
    return;
  }

  toggleMessage('loading','Filtering testgroup list...');
  filter_req = doSimpleXMLHttpRequest('manage_testgroups.cgi', {
    searchTestgroupList: 1,
    product: (productfilter.options[productfilter.selectedIndex].value == '---' ? '' : productfilter.options[productfilter.selectedIndex].value)
  });
  // if something went wrong, just show all the tests:
  filter_req.addErrback(showAll);
  filter_req.addCallback(doFilterList);
}

function enableModeButtons() {
  document.getElementById("edit_testgroup_button").disabled=false;
  document.getElementById("clone_testgroup_button").disabled=false;
  document.getElementById("delete_testgroup_button").disabled=false;
}

function disableModeButtons() {
  document.getElementById("edit_testgroup_button").disabled=true;
  document.getElementById("clone_testgroup_button").disabled=true;
  document.getElementById("delete_testgroup_button").disabled=true;
}

function loadTestgroup(silent) {
  var testgroup_select = document.getElementById("testgroup_id");

  if (! testgroup_select ||
      testgroup_select.options[testgroup_select.selectedIndex].value=="") {
    disableModeButtons();
    document.getElementById('testgroup_display_div').style.display = 'none';
    document.getElementById('editform_div').style.display = 'none';
    disableForm('edit_testgroup_form');
    blankTestgroupForm('edit_testgroup_form');
    return false;
  } 

  var testgroup_id = testgroup_select.options[testgroup_select.selectedIndex].value;

  disableForm('edit_testgroup_form');
  if (!silent) {
    toggleMessage('loading','Loading Testgroup ID# ' + testgroup_id + '...');
  }
  var url = 'json.cgi?testgroup_id=' + testgroup_id;
  fetchJSON(url,populateTestgroup,silent);
}

function populateBranches(productBox) {
  var branchBox = document.getElementById('editform_branches'); 
  branchBox.options.length = 0;
  
  var productId = productBox.options[productBox.selectedIndex].value;
  var product = getProductById(productId);
  if (!product) {
    // no product set
    var option = new Option('-No product selected-','');
    branchBox.add(option, null);
    return;
  }
  var option = new Option('-Branch-','');
  branchBox.add(option, null);
  var branches = product['branches'];
  for (var i=0; i<branches.length; i++) {
    var option = new Option(branches[i].name,branches[i].id);
    option.selected = false;
    if (testgroup.product_id && productId == testgroup.product_id.product_id) {
      for (var j=0; j<testgroup.branches.length; j++) {
        if (option.value == testgroup.branches[j].branch_id) {
          option.selected = true;
        }
      }
    }
    branchBox.add(option, null);
  }
}

function populateTestgroup(data) {
  testgroup=data;
  document.getElementById('editform_testgroup_id').value = testgroup.testgroup_id;
  document.getElementById('editform_testgroup_id_display').innerHTML = testgroup.testgroup_id;
  document.getElementById('editform_name').value = testgroup.name;
  document.getElementById('name_text').innerHTML = testgroup.name;
  document.getElementById('testgroup_id_display').innerHTML = testgroup.testgroup_id;

  var productBox = document.getElementById('product');
  var options = productBox.getElementsByTagName('option');  
  var found_product = 0;
  for (var i=0; i<options.length; i++) {
    if (options[i].value == testgroup.product_id.product_id) {
      options[i].selected = true;
      document.getElementById('product_text').innerHTML = options[i].text;
      found_product=1;
    } else {
      options[i].selected = false;
    }      
  }
  if (found_product == 0) {
    options[0].selected = true;
  }
  changeProduct();
  populateBranches(productBox);
  var enabled_em = document.getElementById('editform_enabled')
  if (testgroup.enabled == 1) {
    enabled_em.checked = true;
  } else {
    enabled_em.checked = false;
 } 
  document.getElementById('editform_testrunner_plan_id').innerHTML = testgroup.testrunner_plan_id;

  populateAllSubgroups();

  var selectBoxTestgroup = document.getElementById('editform_testgroup_subgroups');
  selectBoxTestgroup.options.length = 0;
  for (var i=0; i<testgroup.subgroups.length; i++) {
    var optionText = testgroup.subgroups[i].name + ' (' + testgroup.subgroups[i].subgroup_id + ')'; 

    selectBoxTestgroup.options[selectBoxTestgroup.length] = new Option(optionText,
                                                     testgroup.subgroups[i].subgroup_id);
  }

  document.getElementById('editform_div').style.display = 'none';
  document.getElementById('testgroup_display_div').style.display = 'block';
  enableModeButtons();
}

function blankTestgroupForm(formid) {
  blankForm(formid);
  document.getElementById('editform_testgroup_id_display').innerHTML = '';
  var selectBoxAll = document.getElementById('editform_subgroups_for_product');
  selectBoxAll.options.length = 0;
  selectBoxAll.options[selectBoxAll.length] = new Option("--No product selected--",
                                                             "");
  selectBoxAll.selectedIndex=-1;
  var selectBoxTestgroup = document.getElementById('editform_testgroup_subgroups');
  selectBoxTestgroup.options.length = 0;
  selectBoxTestgroup.options[selectBoxTestgroup.length] = new Option("--No testgroup selected--","");
  selectBoxTestgroup.selectedIndex=-1;

  document.getElementById('editform_testrunner_plan_id').innerHTML = '';

  testgroup = new Object();

  changeProduct();
  populateBranches(document.getElementById('product'));
}

function switchToAdd() {
  disableModeButtons();
  blankTestgroupForm('edit_testgroup_form');
  document.getElementById('editform_testgroup_id_display').innerHTML = '<em>Automatically generated for a new testgroup</em>';
 document.getElementById('editform_testrunner_plan_id').innerHTML = '<em>Not Applicable</em>';
  document.getElementById('editform_submit').value = 'Add Testgroup';
  document.getElementById('editform_mode').value = 'add';
  enableForm('edit_testgroup_form');
  document.getElementById('testgroup_display_div').style.display = 'none';
  document.getElementById('editform_div').style.display = 'block';
}

function switchToEdit() {
  document.getElementById('editform_submit').value = 'Submit Edits';
  document.getElementById('editform_mode').value = 'edit';
  enableForm('edit_testgroup_form');
  document.getElementById('testgroup_display_div').style.display = 'none';
  document.getElementById('editform_div').style.display = 'block';
}

function populateAllSubgroups() {
  toggleMessage('loading','Narrowing Subgroup List...');
  try {
    var productBox = document.getElementById('product');
    var branchBox = document.getElementById('editform_branches');
    var selectBoxAll = document.getElementById('editform_subgroups_for_product');
    selectBoxAll.options.length = 0; 
    for (var i in subgroups) {
      if (subgroups[i].product_id != productBox.options[productBox.selectedIndex].value) {
        continue;
      }
      
      if (branchBox.selectedIndex >= 0) {
        var found_branch = 0;
        for (var j=0; j<branchBox.options.length; j++) {
          if (branchBox.options[j].value == '' ||
              branchBox.options[j].selected == false) {
            continue;
          }
          if (subgroups[i].branch_id == branchBox.options[j].value) {
            found_branch = 1;
          } 
        }
        if (branchBox.options[branchBox.selectedIndex].value != '' &&
            found_branch == 0) {
          continue;
        }
      } 
     
      var optionText = subgroups[i].name + ' (' + subgroups[i].subgroup_id + ')'; 
      selectBoxAll.options[selectBoxAll.length] = new Option(optionText,
                                                             subgroups[i].subgroup_id);
    }
    if (selectBoxAll.options.length == 0) {
      selectBoxAll.options[selectBoxAll.length] = new Option('-No Subgroups for this Product/Branch-','');
    }
  } catch (e) {
    // And do what exactly?
  }
  toggleMessage('none');
}  

function resetTestgroup() {
  if (document.getElementById('editform_testgroup_id').value != '') {
    populateTestgroup(testgroup);
    switchToEdit();   
  } else {
    switchToAdd();
  }
}

function checkFormContents(f) {
  return (
          checkString(f.editform_name, 'Name') &&
          verifySelected(f.product, 'Product') &&
          verifySelected(f.editform_branches, 'Branch')
         );
}

function previewSubgroup(selectID) {
  var selectBox = document.getElementById(selectID);
  if (selectBox) {
    if (selectBox.selectedIndex >= 0) {
      if (selectBox.options[selectBox.selectedIndex].value != '') {
        window.open('manage_subgroups.cgi?subgroup_id=' + selectBox.options[selectBox.selectedIndex].value,
                    'subgroup_preview');
      }
    } else {
      toggleMessage('failure','No testgroup selected!');
    }
  }
}

var manageTestgroupsHelpTitle="Help with Managing Testgroups";
var manageTestgroupsHelpText="<p>The select box on the left contains all the subgroups for the chosen product, <strong><em>INCLUDING</em></strong> any subgroups already contained in the testgroup. You can use the <input type='button' value='&rArr;' disabled> button to add subgroups to the testgroup, and the <input type='button' value='&lArr;' disabled> button to remove subgroups from the testgroup. <strong>NOTE</strong>: neither of the actions will alter the list of subgroups on the left.</p><p>You can preview any subgroup from the left-hand select box by selecting the subgroup, and then clicking on  the 'Preview Subgroup' link below the select box. If more than one subgroup is selected, only the first subgroup will be previewed.</p><p>You can change the display order of subgroups within the testgroup using the <input type='button' value='&uArr;' disabled> and <input type='button' value='&dArr;' disabled> buttons to the right of the right-hand select box. Subgroups can be re-ordered singly or in groups by selecting multiple subgroups in the right-hand select box.</p>";
