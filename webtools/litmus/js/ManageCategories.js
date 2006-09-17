var product;
var platform;
var opsys;
var branch;

function enableBranchModeButtons() {
  document.getElementById("edit_branch_button").disabled=false;
  document.getElementById("delete_branch_button").disabled=false;
}

function disableBranchModeButtons() {
  document.getElementById("edit_branch_button").disabled=true;
  document.getElementById("delete_branch_button").disabled=true;
}

function loadBranch() {
  var branch_select = document.getElementById("branch_id");

  if (! branch_select ||
      branch_select.options[branch_select.selectedIndex].value=="") {
    disableBranchModeButtons();
    document.getElementById('edit_branch_form_div').style.display = 'none';
    disableForm('edit_branch_form');
    blankBranchForm('edit_branch_form');
    return false;
  }

  var branch_id = branch_select.options[branch_select.selectedIndex].value;

  disableForm('edit_branch_form');
  toggleMessage('loading','Loading Branch ID# ' + branch_id + '...');
  var url = 'json.cgi?branch_id=' + branch_id;
  fetchJSON(url,populateBranch);
}

function populateBranch(data) {
  branch=data;
  document.getElementById('edit_branch_form_branch_id').value = branch.branch_id;
  document.getElementById('edit_branch_form_branch_id_display').innerHTML = branch.branch_id;
  document.getElementById('edit_branch_form_name').value = branch.name;
  document.getElementById('edit_branch_form_detect_regexp').value = branch.detect_regexp;
  var product_box = document.getElementById('edit_branch_form_product_id');
  var options = product_box.getElementsByTagName('option');
  var found_product = 0;
  for (var i=0; i<options.length; i++) {
    if (options[i].value == branch.product_id.product_id) {
      options[i].selected = true;
      found_product=1;
    } else {
      options[i].selected = false;
    }
  }
  if (found_product == 0) {
    options[0].selected = true;
  }

  var enabled_em = document.getElementById('edit_branch_form_enabled')
  if (branch.enabled == 1) {
    enabled_em.checked = true;
  } else {
    enabled_em.checked = false;
  }

  document.getElementById('edit_branch_form_div').style.display = 'block';
  enableBranchModeButtons();
}

function blankBranchForm(formid) {
  blankForm(formid);
  document.getElementById('edit_branch_form_branch_id_display').innerHTML = '';
}

function switchBranchFormToAdd() {
  disableBranchModeButtons();
  blankBranchForm('edit_branch_form');
  document.getElementById('edit_branch_form_submit').value = 'Add Branch';
  document.getElementById('edit_branch_form_mode').value = 'add';
  enableForm('edit_branch_form');
  document.getElementById('edit_branch_form_div').style.display = 'block';
}

function switchBranchFormToEdit() {
  document.getElementById('edit_branch_form_submit').value = 'Submit Edits';
  document.getElementById('edit_branch_form_mode').value = 'edit';
  enableForm('edit_branch_form');
  document.getElementById('edit_branch_form_div').style.display = 'block';
}


function checkBranchForm(f) {
  return (
          checkString(f.edit_branch_form_name,"Branch Name",false) &&
          checkString(f.edit_branch_form_detect_regexp,"Branch Detect Regexp",true) &&
          verifySelected(f.edit_branch_form_product_id, "Product")
         );
}

function resetBranch() {
  if (document.getElementById('edit_branch_form_branch_id').value != '') {
    populateBranch(branch);
    switchBranchFormToEdit();   
  } else {
    switchBranchFormToAdd();   
  }
}

function checkAddLogTypeForm(f) {
   return (
           checkString(f.new_log_type_name,"log type name",false)
          );
}

function getLogTypeByName(logTypeName) {
    http.open('get', 'rpc.cgi?action=getLogTypeByName&logTypeName='+logTypeName);
    http.onreadystatechange = updateLogTypeForm;
    http.send(null);
}

function updateLogTypeForm() {
    if(http.readyState == 4){
        var response = http.responseText;
        var update = new Array();

        var em = document.getElementById("notification-content")
        if(response.indexOf('|' != -1)) {
            update = response.split('|');
            document.getElementById("modify_log_type_id").value = update[0];
            document.getElementById("modify_log_type_name").disabled = false;
            document.getElementById("modify_log_type_name").value = update[1];
            document.getElementById("delete_log_type").disabled = false;
            document.getElementById("update_log_type").disabled = false;
        } else {
            document.getElementById("notification").style.display = 'block';
            em.innerHTML = response;
        }
    }
}

function enableOpsysModeButtons() {
  document.getElementById("edit_opsys_button").disabled=false;
  document.getElementById("delete_opsys_button").disabled=false;
}

function disableOpsysModeButtons() {
  document.getElementById("edit_opsys_button").disabled=true;
  document.getElementById("delete_opsys_button").disabled=true;
}

function loadOpsys() {
  var opsys_select = document.getElementById("opsys_id");

  if (! opsys_select ||
      opsys_select.options[opsys_select.selectedIndex].value=="") {
    disableOpsysModeButtons();
    document.getElementById('edit_opsys_form_div').style.display = 'none';
    disableForm('edit_opsys_form');
    blankOpsysForm('edit_opsys_form');
    return false;
  }

  var opsys_id = opsys_select.options[opsys_select.selectedIndex].value;

  disableForm('edit_opsys_form');
  toggleMessage('loading','Loading Opsys ID# ' + opsys_id + '...');
  var url = 'json.cgi?opsys_id=' + opsys_id;
  fetchJSON(url,populateOpsys);
}

function populateOpsys(data) {
  opsys=data;
  document.getElementById('edit_opsys_form_opsys_id').value = opsys.opsys_id;
  document.getElementById('edit_opsys_form_opsys_id_display').innerHTML = opsys.opsys_id;
  document.getElementById('edit_opsys_form_name').value = opsys.name;
  document.getElementById('edit_opsys_form_detect_regexp').value = opsys.detect_regexp;
  var platform_box = document.getElementById('edit_opsys_form_platform_id');
  var options = platform_box.getElementsByTagName('option');
  var found_platform = 0;
  for (var i=0; i<options.length; i++) {
    if (options[i].value == opsys.platform_id.platform_id) {
      options[i].selected = true;
      found_platform=1;
    } else {
      options[i].selected = false;
    }
  }
  if (found_platform == 0) {
    options[0].selected = true;
  }

  document.getElementById('edit_opsys_form_div').style.display = 'block';
  enableOpsysModeButtons();
}

function blankOpsysForm(formid) {
  blankForm(formid);
  document.getElementById('edit_opsys_form_opsys_id_display').innerHTML = '';
}

function switchOpsysFormToAdd() {
  disableOpsysModeButtons();
  blankOpsysForm('edit_opsys_form');
  document.getElementById('edit_opsys_form_submit').value = 'Add Opsys';
  document.getElementById('edit_opsys_form_mode').value = 'add';
  enableForm('edit_opsys_form');
  document.getElementById('edit_opsys_form_div').style.display = 'block';
}

function switchOpsysFormToEdit() {
  document.getElementById('edit_opsys_form_submit').value = 'Submit Edits';
  document.getElementById('edit_opsys_form_mode').value = 'edit';
  enableForm('edit_opsys_form');
  document.getElementById('edit_opsys_form_div').style.display = 'block';
}


function checkOpsysForm(f) {
  return (
          checkString(f.edit_opsys_form_name,"Operating System Name",false) &&
          checkString(f.edit_opsys_form_detect_regexp,"Operating System Detect Regexp",true) &&
          verifySelected(f.edit_opsys_form_platform_id, "Platform")
         );
}

function resetOpsys() {
  if (document.getElementById('edit_opsys_form_opsys_id').value != '') {
    populateOpsys(opsys);
    switchOpsysFormToEdit();   
  } else {
    switchOpsysFormToAdd();   
  }
}

function enablePlatformModeButtons() {
  document.getElementById("edit_platform_button").disabled=false;
  document.getElementById("delete_platform_button").disabled=false;
}

function disablePlatformModeButtons() {
  document.getElementById("edit_platform_button").disabled=true;
  document.getElementById("delete_platform_button").disabled=true;
}

function loadPlatform() {
  var platform_select = document.getElementById("platform_id");

  if (! platform_select ||
      platform_select.options[platform_select.selectedIndex].value=="") {
    disablePlatformModeButtons();
    document.getElementById('edit_platform_form_div').style.display = 'none';
    disableForm('edit_platform_form');
    blankPlatformForm('edit_platform_form');
    return false;
  }

  var platform_id = platform_select.options[platform_select.selectedIndex].value;

  disableForm('edit_platform_form');
  toggleMessage('loading','Loading Platform ID# ' + platform_id + '...');
  var url = 'json.cgi?platform_id=' + platform_id;
  fetchJSON(url,populatePlatform);
}

function populatePlatform(data) {
  platform=data;
  document.getElementById('edit_platform_form_platform_id').value = platform.platform_id;
  document.getElementById('edit_platform_form_platform_id_display').innerHTML = platform.platform_id;
  document.getElementById('edit_platform_form_name').value = platform.name;
  document.getElementById('edit_platform_form_detect_regexp').value = platform.detect_regexp;
  document.getElementById('edit_platform_form_iconpath').value = platform.iconpath;

  var selectBoxProduct = document.getElementById('edit_platform_form_platform_products');
  selectBoxProduct.options.length = 0;
  for (var i=0; i<platform.products.length; i++) {
    var optionText = platform.products[i].name + ' (' + platform.products[i].product_id + ')'; 
    selectBoxProduct.options[selectBoxProduct.length] = 
	new Option(optionText,
		   platform.products[i].product_id);
  }

  document.getElementById('edit_platform_form_div').style.display = 'block';
  enablePlatformModeButtons();
}

function blankPlatformForm(formid) {
  blankForm(formid);
  document.getElementById('edit_platform_form_platform_id_display').innerHTML = '';

  var selectBoxProduct = document.getElementById('edit_platform_form_platform_products');
  selectBoxProduct.options.length = 0;
  selectBoxProduct.options[selectBoxProduct.length] = new Option("--No platform selected--","");
  selectBoxProduct.selectedIndex=-1;
}

function switchPlatformFormToAdd() {
  disablePlatformModeButtons();
  blankPlatformForm('edit_platform_form');
  document.getElementById('edit_platform_form_submit').value = 'Add Platform';
  document.getElementById('edit_platform_form_mode').value = 'add';
  enableForm('edit_platform_form');
  document.getElementById('edit_platform_form_div').style.display = 'block';
}

function switchPlatformFormToEdit() {
  document.getElementById('edit_platform_form_submit').value = 'Submit Edits';
  document.getElementById('edit_platform_form_mode').value = 'edit';
  enableForm('edit_platform_form');
  document.getElementById('edit_platform_form_div').style.display = 'block';
}


function checkPlatformForm(f) {
  return (
          checkString(f.edit_platform_form_name,"Platform name",false) &&
          checkString(f.edit_platform_form_iconpath,"Platform icon path",true &&
	  checkString(f.edit_platform_form_detect_regexp,"Platform detect regexp",true))
         );
}

function resetPlatform() {
  if (document.getElementById('edit_platform_form_platform_id').value != '') {
    populatePlatform(platform);
    switchPlatformFormToEdit();   
  } else {
    switchPlatformFormToAdd();   
  }
}

function enableProductModeButtons() {
  document.getElementById("edit_product_button").disabled=false;
  document.getElementById("delete_product_button").disabled=false;
}

function disableProductModeButtons() {
  document.getElementById("edit_product_button").disabled=true;
  document.getElementById("delete_product_button").disabled=true;
}

function loadProduct() {
  var product_select = document.getElementById("product_id");

  if (! product_select ||
      product_select.options[product_select.selectedIndex].value=="") {
    disableProductModeButtons();
    document.getElementById('edit_product_form_div').style.display = 'none';
    disableForm('edit_product_form');
    blankProductForm('edit_product_form');
    return false;
  }

  var product_id = product_select.options[product_select.selectedIndex].value;

  disableForm('edit_product_form');
  toggleMessage('loading','Loading Product ID# ' + product_id + '...');
  var url = 'json.cgi?product_id=' + product_id;
  fetchJSON(url,populateProduct);
}

function populateProduct(data) {
  product=data;
  document.getElementById('edit_product_form_product_id').value = product.product_id;
  document.getElementById('edit_product_form_product_id_display').innerHTML = product.product_id;
  document.getElementById('edit_product_form_name').value = product.name;
  document.getElementById('edit_product_form_iconpath').value = product.iconpath;
  var enabled_em = document.getElementById('edit_product_form_enabled')
  if (product.enabled == 1) {
    enabled_em.checked = true;
  } else {
    enabled_em.checked = false;
  }

  document.getElementById('edit_product_form_div').style.display = 'block';
  enableProductModeButtons();
}

function blankProductForm(formid) {
  blankForm(formid);
  document.getElementById('edit_product_form_product_id_display').innerHTML = '';
}

function switchProductFormToAdd() {
  disableProductModeButtons();
  blankProductForm('edit_product_form');
  document.getElementById('edit_product_form_submit').value = 'Add Product';
  document.getElementById('edit_product_form_mode').value = 'add';
  enableForm('edit_product_form');
  document.getElementById('edit_product_form_div').style.display = 'block';
}

function switchProductFormToEdit() {
  document.getElementById('edit_product_form_submit').value = 'Submit Edits';
  document.getElementById('edit_product_form_mode').value = 'edit';
  enableForm('edit_product_form');
  document.getElementById('edit_product_form_div').style.display = 'block';
}


function checkProductForm(f) {
  return (
          checkString(f.edit_product_form_name,"product name",false) &&
          checkString(f.edit_product_form_iconpath,"product icon path",true)
         );
}

function resetProduct() {
  if (document.getElementById('edit_product_form_product_id').value != '') {
    populateProduct(product);
    switchProductFormToEdit();   
  } else {
    switchProductFormToAdd();   
  }
}
