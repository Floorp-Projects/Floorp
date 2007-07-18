/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the DomainMapper code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu>        (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


//
// http://wp.netscape.com/eng/mozilla/2.0/relnotes/demo/proxy-live.html
// http://kb.mozillazine.org/Network.proxy.autoconfig_url
// http://kb.mozillazine.org/Network.proxy.type
//
  
try
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  if (!this.Cc)
    this.Cc = Components.classes;
  if (!this.Ci)
    this.Ci = Components.interfaces;
  if (!this.Cr)
    this.Cr = Components.results;
  
  const PROXY_AUTOCONFIG_PREF = "network.proxy.autoconfig_url";
  const PROXY_TYPE_PREF = "network.proxy.type";
  const PROXY_TYPE_USE_PAC = 2;
  
  
  /**
   * Maps requests in the browser to particular domains/ports through other
   * domains/ports in a customizable manner.
   */
  function DomainMapper()
  {
    /**
     * Hash of host:port for mapped host/ports to host:port for the host:port
     * which should be used instead.
     */
    this._mappings = {};
  
    /** Identifies the old proxy type, if any. */
    this._oldProxyType = undefined;
    
    /** Identifies the old PAC URL, if any. */
    this._oldPAC = undefined;
  
    /** True when mapping is enabled. */
    this._enabled = false;  
  }
  DomainMapper.prototype =
  {
    /**
     * Adds a mapping for requests to fromHost:fromPort so that they are
     * actually made to toHost:toPort.
     */
    addMapping: function(fromHost, fromPort, toHost, toPort)
    {
      this._mappings[fromHost + ":" + fromPort] = toHost + ":" + toPort;
    },
    
    /**
     * Removes a mapping for requests to fromHost:fromPort, if one exists; if
     * none exists, this is a no-op.
     */
    removeMapping: function(host, port)
    {
      delete this._mappings[host + ":" + port];
    },
    
    /** True if mapping functionality is enabled. */
    get isEnabled()
    {
      return this._enabled;
    },
  
    /**
     * Enables all registered mappings, or updates mappings if already enabled.
     */
    enable: function()
    {
      if (this._enabled)
      {
        this.syncMappings();
        return;
      }
      
      var prefs = Cc["@mozilla.org/preferences-service;1"]
                    .getService(Ci.nsIPrefBranch);
        
      // Change the proxy autoconfiguration URL first, if it's present
      try
      {
        this._oldPAC = prefs.getCharPref(PROXY_AUTOCONFIG_PREF);
      }
      catch (e)
      {
        this._oldPAC = undefined;
      }
    
      // Now change the proxy settings to use the PAC URL
      try
      {
        this._oldProxyType = prefs.getIntPref(PROXY_TYPE_PREF);
        prefs.setIntPref(PROXY_TYPE_PREF, PROXY_TYPE_USE_PAC);
      }
      catch (e)
      {
        this._oldProxyType = 0;
      }
      
      this._enabled = true;
      
      this.syncMappings();
    },
    
    /**
     * Updates URL mappings currently in use with those specified when this was
     * enabled.
     *
     * @throws NS_ERROR_UNEXPECTED
     *   if this method is called when mapping functionality is not enabled
     */
    syncMappings: function()
    {
      if (!this._enabled)
        throw Cr.NS_ERROR_UNEXPECTED;
  
      var url = "data:text/plain,";
      url += "function FindProxyForURL(url, host)                  ";
      url += "{                                                    ";
      url += "  var mappings = " + this._mappings.toSource() + ";  ";
      url += "  var regex = new RegExp('http://(.*?(:\\\\d+)?)/'); ";
      url += "  var matches = regex.exec(url);                     ";
      url += "  var hostport = matches[1], port = matches[2];      ";
      url += "  if (!port)                                         ";
      url += "    hostport += ':80';                               ";
      url += "  if (hostport in mappings)                          ";
      url += "    return 'PROXY ' + mappings[hostport];            ";
      url += "  return 'DIRECT';                                   ";
      url += "}";
      
      Cc["@mozilla.org/preferences-service;1"]
        .getService(Ci.nsIPrefBranch)
        .setCharPref(PROXY_AUTOCONFIG_PREF, url);
    },
  
    /** Disables all domain mapping functionality. */
    disable: function()
    {
      if (!this._enabled)
        return;
  
      var prefs = Cc["@mozilla.org/preferences-service;1"]
                    .getService(Ci.nsIPrefBranch);
        
      if (this._oldPAC !== undefined)
        prefs.setCharPref(PROXY_AUTOCONFIG_PREF, this._oldPAC);
      this._oldPAC = undefined;
  
      prefs.setIntPref(PROXY_TYPE_PREF, this._oldProxyType);
      this._oldProxyType = undefined;
  
      this._enabled = false;
    }
  };
  
  
  //
  // BEGIN CROSS-DOMAIN CODE
  //
  // Various pieces of code must be able to test functionality when run on
  // effectively arbitrary ports, hosts, etc. for cross-domain purposes,
  // usually for security.  We provide that support here, mapping the desired
  // additional host/port pairs onto the original server at localhost:8888.
  //
  // We have chosen to "take over" the example.org and example.com domains here,
  // because per RFC 2606, example.org is reserved for testing and other such
  // activities and can be safely "hijacked" for cross-domain testing.
  //
  // The set of domains chosen here is mostly taken from an IRC conversation
  // with bz and from <https://bugzilla.mozilla.org/show_bug.cgi?id=332179>;
  // the original set was .org+80, and bz wanted a parallel .com and .org+8000.
  //
  // This list must be duplicated in runtests.pl.in to set the appropriate
  // security preferences to not have to display privilege escalation prompts.
  // KEEP THIS LIST IN SYNC WITH THAT ONE!
  //
  var mappedHostPorts =
    [
     {host: "example.org", port: 80},
     {host: "test1.example.org", port: 80},
     {host: "test2.example.org", port: 80},
     {host: "sub1.test1.example.org", port: 80},
     {host: "sub1.test2.example.org", port: 80},
     {host: "sub2.test1.example.org", port: 80},
     {host: "sub2.test2.example.org", port: 80},
     {host: "example.org", port: 8000},
     {host: "test1.example.org", port: 8000},
     {host: "test2.example.org", port: 8000},
     {host: "sub1.test1.example.org", port: 8000},
     {host: "sub1.test2.example.org", port: 8000},
     {host: "sub2.test1.example.org", port: 8000},
     {host: "sub2.test2.example.org", port: 8000},
     {host: "example.com", port: 80},
     {host: "test1.example.com", port: 80},
     {host: "test2.example.com", port: 80},
     {host: "sub1.test1.example.com", port: 80},
     {host: "sub1.test2.example.com", port: 80},
     {host: "sub2.test1.example.com", port: 80},
     {host: "sub2.test2.example.com", port: 80},
    ];

  var crossDomain = new DomainMapper();
  for (var i = 0, sz = mappedHostPorts.length; i < sz; i++)
  {
    var hostPort = mappedHostPorts[i];
    crossDomain.addMapping(hostPort.host, hostPort.port, "localhost", 8888);
  }
  crossDomain.enable();
}
catch (e)
{
  throw "privilege failure enabling cross-domain: " + e;
}
