function MM_findObj(n, d) { //v4.01
  var p,i,x;  if(!d) d=document; if((p=n.indexOf("?"))>0&&parent.frames.length) {
    d=parent.frames[n.substring(p+1)].document; n=n.substring(0,p);}
  if(!(x=d[n])&&d.all) x=d.all[n]; for (i=0;!x&&i<d.forms.length;i++) x=d.forms[i][n];
  for(i=0;!x&&d.layers&&i<d.layers.length;i++) x=MM_findObj(n,d.layers[i].document);
  if(!x && d.getElementById) x=d.getElementById(n); return x;
}

function showsubgroup() {
  var groupselect = MM_findObj("group");

  var selnum;

  for (var i=0; i<groupselect.length; i++) {
    if (groupselect[i].checked) {
      selnum = groupselect[i].value;
    }
  }

  // no selection yet so just keep everything as-is:
  if (! selnum) {
      return;
  }

  // object to show
  var obj = MM_findObj("divsubgroup_"+selnum);

  // disable all of them
  for (var i=0; i<groupselect.length; i++) {
    var gnum = groupselect[i].value;
    var disableobj = MM_findObj("divsubgroup_"+gnum);
    disableobj.style.display = "none";
  }
  MM_findObj("divsubgroup_null").style.display = "none";

  var num_subgroups_enabled = 0;
  var subgroupselect = MM_findObj("subgroup_"+selnum);
  for (var i=0; i<subgroupselect.length; i++) {
    if (!subgroupselect[i].disabled) {
      num_subgroups_enabled++;
    }
  }

  obj.style.display = "";

  if (num_subgroups_enabled == 0) {
    MM_findObj("Submit").disabled = true;
  } else {
    MM_findObj("Submit").disabled = false;
  }

}


