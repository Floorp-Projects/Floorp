/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

function userOnStart()
{
  dlog('userOnStart()');
}

function userOnBeforePage()
{
  dlog('userOnBeforePage()');
}

function userOnPause()
{
  dlog('userOnPause()');
}

function userOnAfterPage()
{
  dlog('userOnAfterPage()');

  setTimeout(collectLinks, 5000);
}

function collectLinks()
{
  dlog('collectLinks()');

  var win = gSpider.mDocument.defaultView;
  if (win.wrappedJSObject)
  {
    win = win.wrappedJSObject;
  }

  var os;

  switch(navigator.platform)
  {
  case 'Win32':
    os = 'win';
    break;
  case 'MacPPC':
  case 'MacIntel':
    os = 'osx';
    break;
  case 'Linux i686':
  case 'Linux i686 (x86_64)':
    os = 'linux';
    break;
  default:
    cdump('Error: Unknown OS ' + navigator.platform);
    return;
  }
    
  cdump('document.body: ' + win.document.body.innerHTML);
  var links = win.document.links;

  for (var ilink = 0; ilink < links.length; ilink++)
  {
    var link = links[ilink];
    if (link.href.indexOf('http://download.mozilla.org') != -1 && 
        link.href.indexOf('os=' + os) != -1)
    {
      var href = link.href;
      cdump('href: ' + href);
    }
  }

  gPageCompleted = true;
}


function userOnStop()
{
  dlog('userOnStop()');
}

