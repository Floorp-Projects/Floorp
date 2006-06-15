var editedtests = new Array();
var fields = ["product", "summary", "testgroup", "subgroup", 
              "steps", "results", "admin", "formatting"];

function MM_findObj(n) {
  var x = document.getElementById(n);
  return x;
}

function showEdit(testid) { 
    for (var i=0; i<fields.length; i++) {
        show(getField(fields[i]+"_edit",testid));
        hide(getField(fields[i]+"_text",testid));
    }
    
    hide(getField("editlink", testid));
    show(getField("canceleditlink", testid));

    document.getElementById("show_test_form").action = "show_test.cgi";
     
    editedtests.push(testid);
}

function findEdited() {
    MM_findObj("editingTestcases").value = editedtests.toString();
}

function cancelEdit(testid) {
    for (var i=0; i<fields.length; i++) {
        hide(getField(fields[i]+"_edit",testid));
        show(getField(fields[i]+"_text",testid));
    }
    
    show(getField("editlink", testid));
    hide(getField("canceleditlink", testid));
    
    // remove testid from the editedtests array:
    var newarray = new Array();
    for (var i=0; i<editedtests.length; i++) {
        if (editedtests[i] != testid) {
            newarray.push(testid);    
        }
    }
    editedtests=newarray;

    document.getElementById("show_test_form").action = "process_test.cgi";
}

// fields are in the format fieldname_testid
function getField(fieldname, testid) {
    return MM_findObj(fieldname+"_"+testid);    
}

function show(obj) {
    if (obj) {
        obj.style.display = "";
    }
}

function hide(obj) {
    if (obj) {
        obj.style.display = "none";
    }
}

function tc_init() {
    if (document.getElementById("testconfig")) {	
        testConfigHeight = new fx.Height('testconfig', {duration: 400});
        testConfigHeight.toggle();
    }
}
