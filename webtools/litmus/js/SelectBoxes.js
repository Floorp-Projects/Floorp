function selects_onload() {
    load_products(getElementByClass("select_product"));
//    load_testgroups(getElementByClass("select_testgroup"));
//    load_subgroups(getElementByClass("select_subgroup"));
    
    load_platforms(getElementByClass("select_platform"));
    load_opsyses(getElementByClass("select_opsys"));
    load_branches(getElementByClass("select_branch"));
}

function load_products(selects) {
    if (!selects) { return; }
    // for each select box in selects, load in the list of products
    for (var select=0; select<selects.length; select++) {
        var productbox = selects[select];
        clearSelect(productbox);
        addNullEntry(productbox);
        for (var i=0; i<litmusconfig.length; i++) {
            var option = makeOption(litmusconfig[i]);
            productbox.add(option, null);
            // handle the default selection
            if (isDefault(document.getElementById(productbox.name+"_default"), litmusconfig[i]['id'])) {
                 productbox.selectedIndex = i+1;
            }
        }
    }
}

function load_testgroups(selects) {
    if (!selects[0]) { return; }
    // load the proper list of testgroups for the 
    // currently selected product for each testgroup
    // select:
    for (var select=0; select<selects.length; select++) {
        var groupbox = selects[select];
        clearSelect(groupbox);
        addNullEntry(groupbox);
        // find the currently selected product that goes with this select
        var productbox = document.getElementById("product"+groupbox.name.substr(9));
        var productid = productbox.options[productbox.selectedIndex].value;
        var product = getProductById(productid);
        if (!product) {
            return;
        }
        // now get the list of testgroups that goes with that product:
        var testgroups = product['testgroups'];
        for (var group=0; group<testgroups.length; group++) {
            var option = makeOption(testgroups[group])
            groupbox.add(option, null);
            // handle the default selection
            if (isDefault(document.getElementById(groupbox.name+"_default"), testgroups[group]['id'])) {
                    groupbox.selectedIndex = group+1;
            }     
        }
    }
}

function load_subgroups(selects) {
    if (!selects[0]) { return; }
    for (var select=0; select<selects.length; select++) {
        var subgroupbox = selects[select];
        clearSelect(subgroupbox);
        addNullEntry(subgroupbox);
        // find the currently selected testgroup that goes with this select
        var testgroupbox = document.getElementById("testgroup"+subgroupbox.name.substr(8));
        var testgroupid = testgroupbox.options[testgroupbox.selectedIndex].value;
        var testgroup = getTestgroupById(testgroupid);
        if (!testgroup) {
            // no testgroup set
            return;
        }
        // now get the list of subgroups that goes with that testgroup
        var subgroups = testgroup['subgroups'];
        for (var i=0; i<subgroups.length; i++) {
            var option = makeOption(subgroups[i]);
            subgroupbox.add(option, null);
            if (isDefault(document.getElementById(subgroupbox.name+"_default"), subgroups[i]['id'])) {
                    subgroupbox.selectedIndex = i+1;
            }
        }
    }
} // wow, that was fun


function load_platforms(selects) {
    if (!selects[0]) { return; }
    for (var select=0; select<selects.length; select++) {
        var platformbox = selects[select];
        clearSelect(platformbox);
        addNullEntry(platformbox);
        // find the currently selected product that goes with this select
        var productbox = document.getElementById("product"+platformbox.name.substr(8));
        var productid = productbox.options[productbox.selectedIndex].value;
        var product = getProductById(productid);
        if (!product) {
            // no product set
            return;
        }
        var platforms = product['platforms'];
        for (var i=0; i<platforms.length; i++) {
            var option = makeOption(platforms[i]);
            platformbox.add(option, null);
            if (isDefault(document.getElementById(platformbox.name+"_default"), platforms[i]['id'])) {
                    platformbox.selectedIndex = i+1;
            }
        }
    }
}

function load_branches(selects) {
    if (!selects[0]) { return; }
    for (var select=0; select<selects.length; select++) {
        var branchbox = selects[select];
        clearSelect(branchbox);
        addNullEntry(branchbox);
        // find the currently selected product that goes with this select
        var productbox = document.getElementById("product"+branchbox.name.substr(6));
        var productid = productbox.options[productbox.selectedIndex].value;
        var product = getProductById(productid);
        if (!product) {
            // no product set
            return;
        }
        var branches = product['branches'];
        for (var i=0; i<branches.length; i++) {
            var option = makeOption(branches[i]);
            branchbox.add(option, null);
            if (isDefault(document.getElementById(branchbox.name+"_default"), branches[i]['id'])) {
                 branchbox.selectedIndex = i+1;
            }
        }
    }
}

function load_opsyses(selects) {
    if (!selects[0]) { return; }
    for (var select=0; select<selects.length; select++) {
        var opsysbox = selects[select];
        clearSelect(opsysbox);
        addNullEntry(opsysbox);
        // find the currently selected platform
        var platformbox = document.getElementById("platform"+opsysbox.name.substr(5));
        var platformid = platformbox.options[platformbox.selectedIndex].value;
        var platform = getPlatformById(platformid);
        if (!platform) {
            return;
        }
        var opsyses = platform['opsyses'];
        for (var i=0; i<opsyses.length; i++) {
            var option = makeOption(opsyses[i]);
            opsysbox.add(option, null);
           if (isDefault(document.getElementById(opsysbox.name+"_default"), opsyses[i]['id'])) {
                 opsysbox.selectedIndex = i+1;
            }
        }
    }
}

function changeProduct(testid) {
    var testidflag = "";
    if (testid) { testidflag = "_"+testid; }
    
    load_testgroups([document.getElementById("testgroup"+testidflag)]);    
    changeTestgroup(testid);
    
    load_platforms([document.getElementById("platform"+testidflag)]);
    changePlatform(testid);
    
    load_branches([document.getElementById("branch"+testidflag)]);
}

function changeTestgroup(testid) {
    var testidflag = "";
    if (testid) { testidflag = "_"+testid; }
    
    load_subgroups([document.getElementById("subgroup"+testidflag)]);
}

function changePlatform(testid) {
    var testidflag = "";
    if (testid) { testidflag = "_"+testid; }
    
    load_opsyses([document.getElementById("opsys"+testidflag)]);
}

function addNullEntry(select) {
    // add a blank entry to the current select:
    select.add(new Option("---", "---", false, false), null);
}

function clearSelect(select) {
    // remove all options from a select:
    while (select.options[0]) {
        select.remove(0);
    }
}

function getProductById(prodid) {
    for (var i=0; i<litmusconfig.length; i++) {
        if (litmusconfig[i]['id'] == prodid) {
                return(litmusconfig[i]);
        }
    }    
}

function getTestgroupById(testgroupid) {
    for (var i=0; i<litmusconfig.length; i++) {
        for (var j=0; j<litmusconfig[i]['testgroups'].length; j++) {
            if (litmusconfig[i]['testgroups'][j]['id'] == testgroupid) {
                return(litmusconfig[i]['testgroups'][j]);
            }
        }
    }    
}


function getPlatformById(platformid) {
    for (var i=0; i<litmusconfig.length; i++) {
        for (var j=0; j<litmusconfig[i]['platforms'].length; j++) {
            if (litmusconfig[i]['platforms'][j]['id'] == platformid) {
                return(litmusconfig[i]['platforms'][j]);
            }
        }
    }    
}


// pass this the <input> containing the list of possible default values
// and the current value, returns true if the current value appears in 
// defaultInput, otherwise returns false
function isDefault(defaultInput, curvalue) {
    if (! defaultInput) { return false; }
    var defaultarray = defaultInput.value.split(',');
    for (var i=0; i<defaultarray.length; i++) {
        if (defaultarray[i] == curvalue) { 
        return true; 
        }
    }
    return false;
}

function makeOption(obj) {
    return new Option(obj['name'], obj['id'], false, false)
}

function getElementByClass(theClass) {
    var elements = new Array();
    var all = document.getElementsByTagName("*"); 
    for (var i=0; i<all.length; i++) {
        if (all[i].className == theClass) {
            elements.push(all[i]);
        }
    }
    return elements;
}