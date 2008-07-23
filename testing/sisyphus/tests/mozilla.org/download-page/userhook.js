/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

function userOnStart()
{
  dlog('userOnStart()');

  debugger;

  loadBundle('chrome://browser-region/locale/region.properties');
  loadBundle('chrome://branding/content/searchconfig.properties');
  loadBundle('resource:/browserconfig.properties');

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


  cdump('win.location.href =' + win.location.href);
  cdump('win.appInfo.toSource() = ' + win.appInfo.toSource());

  var xmlhttp = new XMLHttpRequest();
  var date = new Date();

  if (!('toLocalFormat' in Date.prototype))
  {
    date.toLocaleFormat = function(fmt) {
      var year = this.getFullYear().toString();
      var mon  = (this.getMonth() + 1).toString()
      var day  = (this.getDate()).toString();

      if (mon.length < 2)
      {
        mon = '0' + mon;
      }
      if (day.length < 2)
      {
        day = '0' + day;
      }
      return year + mon + day;
    };
  }

  xmlhttp.open('get', 'http://test.mozilla.com/credentials/builds.xml', false);
  xmlhttp.send(null);

  if (!xmlhttp.responseXML)
  {
    cdump('Unable to retrieve credentials');
    gPageCompleted = true;
    return;
  }

  var password = xmlhttp.responseXML.documentElement.getAttribute('password');

  xmlhttp.open('post', 
               'http://bclary.com/log/2006/02/24/update/index.pl', 
               false,
               'buildid',
               password);

  xmlhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');


  var preferencesList = [
    'app.distributor',
    'app.distributor.channel',
    'app.releaseNotesURL',
    'app.update.channel',
    'app.update.url',
    'app.update.url.details',
    'app.update.url.manual',
    'browser.contentHandlers.types.0.title',
    'browser.contentHandlers.types.0.uri',
    'browser.contentHandlers.types.1.title',
    'browser.contentHandlers.types.1.uri',
    'browser.contentHandlers.types.2.title',
    'browser.contentHandlers.types.2.uri',
    'browser.search.defaultenginename',
    'browser.search.order.1',
    'browser.search.order.2',
    'browser.search.order.3',
    'browser.search.order.4',
    'browser.search.order.5',
    'browser.search.order.6',
    'browser.search.order.7',
    'browser.search.order.8',
    'browser.search.order.Yahoo',
    'browser.search.order.Yahoo.1',
    'browser.search.order.Yahoo.2',
    'browser.search.param.Google.1.custom',
    'browser.search.param.Google.1.default',
    'browser.search.param.Google.release',
    'browser.search.param.Yahoo.release',
    'browser.search.param.yahoo-f-CN',
    'browser.search.param.yahoo-fr',
    'browser.search.param.yahoo-fr-cjkt',
    'browser.search.searchEnginesURL',
    'browser.search.selectedEngine',
    'browser.search.suggest.enabled',
    'browser.search.useDBForOrder',
    'browser.startup.homepage',
    'keyword.URL',
    'mozilla.partner.id',
    'startup.homepage_override_url',
    'startup.homepage_welcome_url'
    ];

  var preferences = new Preferences('');
  var data = {};

  for (var i = 0; i < preferencesList.length; i++)
  {
    var prefName   = preferencesList[i];
    var prefValue  = preferences.getPref(prefName);
    if (/^(chrome:\/\/|resource:\/)/.test(prefValue))
    {
debugger;
      var override = getOverridePref(prefName, prefValue);
      prefValue += '->' + override;
    }
    
    cdump('preference ' + prefName + '=' + prefValue);
    data[prefName] = prefValue;
  }

  data['000 runDate']                 = date.toLocaleFormat('%Y%c%d');

  data['001 appInfo.vendor']          = win.appInfo.vendor;
  data['002 appInfo.name']            = win.appInfo.name;
  data['003 appInfo.version']         = win.appInfo.version;
  data['004 appInfo.platformVersion'] = win.appInfo.platformVersion;
  data['005 navigator.language']      = navigator.language;
  data['006 navigator.platform']      = navigator.platform;
  data['007 appInfo.platformBuildID'] = win.appInfo.platformBuildID;
  data['008 appInfo.buildID']         = win.appInfo.appBuildID;
  data['009 navigator.buildID']       = navigator.buildID;
  data['010 appInfo.ID']              = win.appInfo.ID;

  var qs = '';

  for (var n in data)
  {
    qs += n + '=' + encodeURIComponent(data[n]) + '&';
  }
  qs = qs.substring(0, qs.length - 1);

  cdump(qs);
  xmlhttp.send(qs);

  gPageCompleted = true;
}


function userOnStop()
{
  dlog('userOnStop()');
}

function Preferences(aPrefRoot)
{
  this.privs = 'UniversalXPConnect UniversalPreferencesRead UniversalPreferencesWrite';

  if (typeof netscape != 'undefined' &&
      'security' in netscape &&
      'PrivilegeManager' in netscape.security &&
      'enablePrivilege' in netscape.security.PrivilegeManager)
  {
    netscape.security.PrivilegeManager.enablePrivilege(this.privs);
  }

  const nsIPrefService = Components.interfaces.nsIPrefService;
  const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
  const nsPrefService_CONTRACTID = "@mozilla.org/preferences-service;1";

  this.prefRoot    = aPrefRoot;
  this.prefService = Components.classes[nsPrefService_CONTRACTID].getService(nsIPrefService);
  this.prefBranch = this.prefService.getBranch(aPrefRoot).QueryInterface(Components.interfaces.nsIPrefBranch2);

}

function Preferences_getPrefRoot() 
{
  if (typeof netscape != 'undefined' &&
      'security' in netscape &&
      'PrivilegeManager' in netscape.security &&
      'enablePrivilege' in netscape.security.PrivilegeManager)
  {
    netscape.security.PrivilegeManager.enablePrivilege(this.privs);
  }

  return this.prefBranch.root; 
}

function Preferences_getPrefType(aPrefName) 
{
  if (typeof netscape != 'undefined' &&
      'security' in netscape &&
      'PrivilegeManager' in netscape.security &&
      'enablePrivilege' in netscape.security.PrivilegeManager)
  {
    netscape.security.PrivilegeManager.enablePrivilege(this.privs);
  }

  return this.prefBranch.getPrefType(aPrefName); 
}

function Preferences_getPref(aPrefName) 
{
  if (typeof netscape != 'undefined' &&
      'security' in netscape &&
      'PrivilegeManager' in netscape.security &&
      'enablePrivilege' in netscape.security.PrivilegeManager)
  {
    netscape.security.PrivilegeManager.enablePrivilege(this.privs);
  }

  var prefType = this.prefBranch.getPrefType(aPrefName);
  var value;


  if (prefType == this.prefBranch.PREF_INVALID)
  {
    cdump('Preferences.prototype.getPref: ' + aPrefName + ' invalid type ' + prefType);
    cdump('Assuming string');
    prefType = this.prefBranch.PREF_STRING;
  }

  switch(prefType)
  {
  case this.prefBranch.PREF_STRING:
    try
    {
      value = this.prefBranch.getCharPref(aPrefName);
    }
    catch(ex)
    {
      cdump('Ignoring ' + ex);
    }
    break;
  case this.prefBranch.PREF_INT:
    try
    {
      value = this.prefBranch.getIntPref(aPrefName);
    }
    catch(ex)
    {
      cdump('Ignoring ' + ex);
    }
    break;
  case this.prefBranch.PREF_BOOL:
    try
    {    value = this.prefBranch.getBoolPref(aPrefName);
    }
    catch(ex)
    {
      cdump('Ignoring ' + ex);
    }
    break;
  default:
    cdump('Preferences.prototype.getPref: ' + aPrefName + ' unknown type ' + prefType);
    break;
  }
  return value;
}

function Preferences_setPref(aPrefName, aPrefValue) 
{
  if (typeof netscape != 'undefined' &&
      'security' in netscape &&
      'PrivilegeManager' in netscape.security &&
      'enablePrivilege' in netscape.security.PrivilegeManager)
  {
    netscape.security.PrivilegeManager.enablePrivilege(this.privs);
  }

  var prefType = this.getPrefType(aPrefName);

  if (prefType == this.prefBranch.PREF_INVALID)
  {
    cdump('Preferences.prototype.getPref: ' + aPrefName + ' invalid type ' + prefType);
    cdump('Assuming string');
    prefType = this.prefBranch.PREF_STRING;
  }

  switch(prefType)
  {
  case this.prefBranch.PREF_INVALID:
    cdump('Preferences.prototype.setPref: ' + aPrefName + ' invalid type ' + prefType);
    break;
  case this.prefBranch.PREF_STRING:
    if (typeof aPrefValue != 'string')
    {
      cdump('Preferences.prototype.setPref: ' + aPrefName + ' invalid value for type string ' + aPrefValue);
    }
    else
    {
      try
      {
        value = this.prefBranch.setCharPref(aPrefName, aPrefValue);
      }
      catch(ex)
      {
        cdump('Ignoring ' + ex);
      }
    }
    break;
  case this.prefBranch.PREF_INT:
    if (typeof aPrefValue != 'number')
    {
      cdump('Preferences.prototype.setPref: ' + aPrefName + ' invalid value for type number ' + aPrefValue);
    }
    else
    {
      try
      {
        value = this.prefBranch.setIntPref(aPrefName, aPrefValue);
      }
      catch(ex)
      {
        cdump('Ignoring ' + ex);
      }
    }
    break;
  case this.prefBranch.PREF_BOOL:
    if (typeof aPrefValue != 'boolean')
    {
      cdump('Preferences.prototype.setPref: ' + aPrefName + ' invalid value for type boolean ' + aPrefValue);
    }
    else
    {
      try
      {
        value = this.prefBranch.setBoolPref(aPrefName, aPrefValue);
      }
      catch(ex)
      {
        cdump('Ignoring ' + ex);
      }
    }
    break;
  default:
    cdump('Preferences.prototype.setPref: ' + aPrefName + ' unknown type ' + prefType);
    break;
  }
}

Preferences.prototype.getPref = Preferences_getPref;
Preferences.prototype.setPref = Preferences_setPref;

function loadBundle(uri)
{
  try
  {
    var bundleService = Components.classes["@mozilla.org/intl/stringbundle;1"].
      getService(Components.interfaces.nsIStringBundleService);

    var bundle = bundleService.createBundle(uri);
    var enumBundle = bundle.getSimpleEnumeration();

    while (enumBundle.hasMoreElements())
    {
      var item = enumBundle.getNext().QueryInterface(Components.interfaces.nsIPropertyElement);
      cdump('bundle ' + uri + ' key : ' + item.key + ', value : ' + item.value);
    }
  }
  catch(ex)
  {
    cdump('Error loading bundle ' + uri + ': ' + ex);
  }
}


function getOverridePref(prefName, uri)
{
  var bundleService = Components.classes["@mozilla.org/intl/stringbundle;1"].
    getService(Components.interfaces.nsIStringBundleService);

  var bundle = bundleService.createBundle(uri);
  var value;

  try
  {
    value =  bundle.GetStringFromName(prefName);
  }
  catch(ex)
  {
    cdump('Ignoring ' + ex);
  }
  return value;
}




