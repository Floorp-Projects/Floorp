function toggle_inactive() {
  var inac = document.getElementById('inactive');
  var img = document.getElementById('toggle');
  if (inac.style.display == 'none') { 
    inac.style.display='';
    img.src='testopia/img/td.gif';
  } else { 
    inac.style.display='none';
    img.src='testopia/img/tr.gif';
  }
}
