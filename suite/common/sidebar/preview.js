// -*- Mode: Java -*-

function Init()
{
  dump("init preview\n");
  var panel_title = document.getElementById('paneltitle');
  var preview_frame = document.getElementById('previewframe');
  panel_title.setAttribute('value', panel_name);
  preview_frame.setAttribute('src', panel_URL);
}
