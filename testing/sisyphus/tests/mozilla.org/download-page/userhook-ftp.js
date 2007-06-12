/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

var gOS;
var gURLS = {};

function userOnStart()
{
  dlog('userOnStart()');

  if (navigator.oscpu.search(/Linux/) != -1)
    gOS = 'linux';
  else if (navigator.oscpu.search(/Mac/) != -1)
    gOS = 'mac';
  else if (navigator.oscpu.search(/Windows/) != -1)
    gOS = 'win32';

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

  var win = gSpider.mDocument.defaultView;
  if (win.wrappedJSObject)
  {
    win = win.wrappedJSObject;
  }

  //cdump('processing = ' + win.location.href);

  var links  = win.document.links;
  var length = links.length;


  for (var ilink = 0; ilink < length; ilink++)
  {

    var href = links[ilink].href;

    if (typeof gURLS[href] != 'undefined')
      continue;

    gURLS[href] = 1;

    switch(gOS)
    {
    case 'linux':
      if (/\.linux-i686\.tar\.(gz|bz2)$/.test(href))
      {
        cdump('href: ' + href);
      }
      break;

    case 'mac':
      if (/\.mac\.dmg$/.test(href))
      {
        cdump('href: ' + href);
      }
      break;

    case 'win32':
      if (/\.win32\.(zip|installer\.exe)$/.test(href))
      {
        cdump('href: ' + href);
      }
      break;

    default:
      break;
    }
  }

  gPageCompleted = true;
}


function userOnStop()
{
  dlog('userOnStop()');
}

