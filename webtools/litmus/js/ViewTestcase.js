function changeProduct(productSelectBox) {
  if (productSelectBox.selectedIndex &&
      productSelectBox.options[productSelectBox.selectedIndex].value != '') {

    var testgroupSelectBox = document.getElementById('testgroup_id');
    if (testgroupSelectBox) {
      testgroupSelectBox.options.length = 0;
      testgroupSelectBox.options[0] = new Option('-Testgroup-','');
      var i = 1;
      for (var j=0; j<testgroups.length; j++) {
        if (testgroups[j].product_id == productSelectBox.options[productSelectBox.selectedIndex].value) {
          testgroupSelectBox.options[i] = new Option(testgroups[j].name,testgroups[j].testgroup_id);
          i++;
        }
      }
    }

    var subgroupSelectBox = document.getElementById('subgroup_id');
    if (subgroupSelectBox) {
      subgroupSelectBox.options.length = 0;
      subgroupSelectBox.options[0] = new Option('-Subgroup-','');
      var i = 1;
      for (var j=0; j<subgroups.length; j++) {
        if (subgroups[j].product_id == productSelectBox.options[productSelectBox.selectedIndex].value) {
          subgroupSelectBox.options[i] = new Option(subgroups[j].name,subgroups[j].subgroup_id);
          i++;
        }
      }
    }

  }

}

function changeTestgroup(testgroupSelectBox) {
  if (testgroupSelectBox.selectedIndex &&
      testgroupSelectBox.options[testgroupSelectBox.selectedIndex].value != '') {
    var subgroupSelectBox = document.getElementById('subgroup_id');
    if (subgroupSelectBox) {
      subgroupSelectBox.options.length = 0;
      subgroupSelectBox.options[0] = new Option('-Subgroup-','');
      var i = 1;
      for (var j=0; j<subgroups.length; j++) {
        if (subgroups[j].testgroup_id == testgroupSelectBox.options[testgroupSelectBox.selectedIndex].value) {
          subgroupSelectBox.options[i] = new Option(subgroups[j].name,subgroups[j].subgroup_id);
          i++;
        }
      }
    }

  }

}

function checkCategoryForm(f) {
  return verifySelected(f.product, 'Product');
}

function checkIDForm(f) {
  return checkString(f.id, 'Testcase ID #');
}

function checkFulltextForm(f) {
  return checkString(f.text_snippet, 'String to match');
}

function checkRecentForm(f) {
  return checkRadio(f.recently, 'Added or Changed') &&
         checkString(f.num_days, '# of days');
}
