function populateProductSelect(selectID)
{
    if (products) {
        var selectBox = document.getElementById(selectID)
        selectBox.options.length = 0;
        selectBox.options[0] = new Option('-Product-','');
        for (var i=0; i<products.length; i++) {
            selectBox.options[i+1] = new Option(products[i].name,products[i].product_id);
        }
    }
}

function changeProduct(productSelectBox,branchSelectBox,testgroupSelectBox)
{
    if (productSelectBox.selectedIndex &&
	productSelectBox.options[productSelectBox.selectedIndex].value != '') {

        if (branchSelectBox) {
            branchSelectBox.options.length = 0;
            branchSelectBox.options[0] = new Option('-Branch-','');
            var i = 1;
            for (var j=0; j<branches.length; j++) {
                if (branches[j].product_id == productSelectBox.options[productSelectBox.selectedIndex].value) {
                    branchSelectBox.options[i] = new Option(branches[j].name,branches[j].branch_id);
                    i++;
                }
            }
        }  

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

    } else {
        branchSelectBox.options.length = 0;
        branchSelectBox.options[0] = new Option('-Branch-','');
        testgroupSelectBox.options.length = 0;
        testgroupSelectBox.options[0] = new Option('-Testgroup-','');
    }
}

function changeBranch(branchSelectBox,testgroupSelectBox)
{
    if (branchSelectBox.selectedIndex &&
        branchSelectBox.options[branchSelectBox.selectedIndex].value != '') {
        if (testgroupSelectBox) {
            testgroupSelectBox.options.length = 0;
            testgroupSelectBox.options[0] = new Option('-Testgroup-','');
            var i = 1;
            for (var j=0; j<testgroups.length; j++) {
                if (testgroups[j].branch_id == branchSelectBox.options[branchSelectBox.selectedIndex].value) {
                    testgroupSelectBox.options[i] = new Option(testgroups[j].name,testgroups[j].testgroup_id);
                    i++;
                }
            }
        }  
        
    } else {
        testgroupSelectBox.options.length = 0;
        testgroupSelectBox.options[0] = new Option('-Testgroup-','');
    }
}
