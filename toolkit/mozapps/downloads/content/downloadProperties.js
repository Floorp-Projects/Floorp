# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is Mozilla.org Code.
# 
# The Initial Developer of the Original Code is.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Blake Ross <blakeross@telocity.com>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

function Startup()
{
  const NC_NS = "http://home.netscape.com/NC-rdf#";
  const rdfSvcContractID = "@mozilla.org/rdf/rdf-service;1";
  const rdfSvcIID = Components.interfaces.nsIRDFService;
  var rdfService = Components.classes[rdfSvcContractID].getService(rdfSvcIID);

  const dlmgrContractID = "@mozilla.org/download-manager;1";
  const dlmgrIID = Components.interfaces.nsIDownloadManager;
  var downloadMgr = Components.classes[dlmgrContractID].getService(dlmgrIID);
  var ds = downloadMgr.datasource;
  
  const dateTimeContractID = "@mozilla.org/intl/scriptabledateformat;1";
  const dateTimeIID = Components.interfaces.nsIScriptableDateFormat;
  var dateTimeService = Components.classes[dateTimeContractID].getService(dateTimeIID);  

  var resource = rdfService.GetUnicodeResource(window.arguments[0]);
  var dateStartedRes = rdfService.GetResource(NC_NS + "DateStarted");
  var dateEndedRes = rdfService.GetResource(NC_NS + "DateEnded");
  var sourceRes = rdfService.GetResource(NC_NS + "URL");

  var dateStartedField = document.getElementById("dateStarted");
  var dateEndedField = document.getElementById("dateEnded");
  var pathField = document.getElementById("path");
  var sourceField = document.getElementById("source");

  try {
    var dateStarted = ds.GetTarget(resource, dateStartedRes, true).QueryInterface(Components.interfaces.nsIRDFDate).Value;
    dateStarted = new Date(dateStarted/1000);
    dateStarted = dateTimeService.FormatDateTime("", dateTimeService.dateFormatShort, dateTimeService.timeFormatSeconds, dateStarted.getFullYear(), dateStarted.getMonth()+1, dateStarted.getDate(), dateStarted.getHours(), dateStarted.getMinutes(), dateStarted.getSeconds());
    dateStartedField.setAttribute("value", dateStarted);
  }
  catch (e) {
  }
  
  try {
    var dateEnded = ds.GetTarget(resource, dateEndedRes, true).QueryInterface(Components.interfaces.nsIRDFDate).Value;
    dateEnded = new Date(dateEnded/1000);
    dateEnded = dateTimeService.FormatDateTime("", dateTimeService.dateFormatShort, dateTimeService.timeFormatSeconds, dateEnded.getFullYear(), dateEnded.getMonth()+1, dateEnded.getDate(), dateEnded.getHours(), dateEnded.getMinutes(), dateEnded.getSeconds());
    dateEndedField.setAttribute("value", dateEnded);
  }
  catch (e) {
  }
  
  var source = ds.GetTarget(resource, sourceRes, true).QueryInterface(Components.interfaces.nsIRDFResource).Value;

  pathField.value = window.arguments[0];
  sourceField.value = source;
  
  var dl = window.opener.gDownloadManager.getDownload(window.arguments[0]);
  if (dl) {
    document.getElementById("dateEndedRow").hidden = true;
    document.getElementById("dateEndedSeparator").hidden = true;
  }
  
  document.documentElement.getButton("accept").label = document.documentElement.getAttribute("acceptbuttontext");
  
  document.documentElement.getButton("accept").focus();
}
  