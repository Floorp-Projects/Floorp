// -*- Mode: Java -*-

var isSidebarOpen = true;

function resize() {
  dump("Resize()\n");
  if (isSidebarOpen)
  {
    var container = document.getElementById('container');
    var sidebar = container.firstChild;
    sidebar.setAttribute('style','width:0px; visibility:hidden');

    //container.removeChild(container.firstChild);

    var grippy = document.getElementById('grippy');
    grippy.setAttribute('open','');

    isSidebarOpen = false;
  }
  else
  {
    var container = document.getElementById('container');
    var sidebar = container.firstChild;
    sidebar.setAttribute('style','width:200px; visibility:visible');
    isSidebarOpen = false;

    //var sidebar = document.createElement('html:iframe');
    //sidebar.setAttribute('src','resource:/res/rdf/sidebar-browser.xul');
    //sidebar.setAttribute('class','sidebarframe');
    //container.insertBefore(sidebar,container.firstChild);
    //container.appendChild(sidebar);

    var grippy = document.getElementById('grippy');
    grippy.setAttribute('open','true');

    isSidebarOpen = true;
  }  
}
