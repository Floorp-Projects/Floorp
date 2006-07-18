# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
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
# The Original Code is The Extension Manager.
# 
# The Initial Developer of the Original Code is Ben Goodger. 
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@bengoodger.com>
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

function EM_NS(aProperty)
{
  return "http://www.mozilla.org/2004/em-rdf#" + aProperty;
}

var gExtensionID = "";
var gExtensionDB = null;

function init()
{
  gExtensionID = window.arguments[0];
  gExtensionDB = window.arguments[1];

  var extensionsStrings = document.getElementById("extensionsStrings");
  
  var rdfs = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                       .getService(Components.interfaces.nsIRDFService);

  var extension = rdfs.GetResource(gExtensionID);                       
                
  // Name       
  var nameArc = rdfs.GetResource(EM_NS("name"));
  var name = gExtensionDB.GetTarget(extension, nameArc, true);
  name = name.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  // Version
  var versionArc = rdfs.GetResource(EM_NS("version"));
  var version = gExtensionDB.GetTarget(extension, versionArc, true);
  version = version.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  // Description
  var descriptionArc = rdfs.GetResource(EM_NS("description"));
  var description = gExtensionDB.GetTarget(extension, descriptionArc, true);
  if (description)
    description = description.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  // Home Page URL
  var homepageArc = rdfs.GetResource(EM_NS("homepageURL"));
  var homepage = gExtensionDB.GetTarget(extension, homepageArc, true);
  if (homepage) {
    homepage = homepage.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    // only allow http(s) homepages
    var scheme = "";
    var uri = null;
    try {
      uri = makeURI(homepage);
      scheme = uri.scheme;
    } catch (ex) {}
    if (uri && (scheme == "http" || scheme == "https"))
      homepage = uri.spec;
    else
      homepage = null;
  }
    
  // Creator
  var creatorArc = rdfs.GetResource(EM_NS("creator"));
  var creator = gExtensionDB.GetTarget(extension, creatorArc, true);
  if (creator)
    creator = creator.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  
  document.title = extensionsStrings.getFormattedString("aboutWindowTitle", [name]);
  var extensionName = document.getElementById("extensionName");
  extensionName.setAttribute("value", name);
  var extensionVersion = document.getElementById("extensionVersion");
  extensionVersion.setAttribute("value", extensionsStrings.getFormattedString("aboutWindowVersionString", [version]));
  
  var extensionDescription = document.getElementById("extensionDescription");
  extensionDescription.appendChild(document.createTextNode(description));
  
  var extensionCreator = document.getElementById("extensionCreator");
  extensionCreator.setAttribute("value", creator);
  
  var extensionHomepage = document.getElementById("extensionHomepage");
  if (homepage) {
    extensionHomepage.setAttribute("onclick", "loadHomepage(\"" + homepage + "\");");
    extensionHomepage.setAttribute("tooltiptext", homepage);
  }
  else
    extensionHomepage.hidden = true;
    
  var sectionProps = [["developersBox", "developer", "extensionDevelopers"],
                      ["translatorsBox", "translator", "extensionTranslators"],
                      ["contributorsBox", "contributor", "extensionContributors"]];

  for (var i = 0; i < sectionProps.length; ++i) {
    var node = document.getElementById(sectionProps[i][0]);
    var arc = rdfs.GetResource(EM_NS(sectionProps[i][1]));
    var targets = gExtensionDB.GetTargets(extension, arc, true);
    if (!(targets.hasMoreElements()))
      document.getElementById(sectionProps[i][2]).hidden = true;
    else {
      while (targets.hasMoreElements()) {
        var literalValue = targets.getNext().QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
        var label = document.createElement("label");
        label.setAttribute("value", literalValue);
        label.setAttribute("class", "contributor");
        node.appendChild(label);
      }
    }
  }
    
  var acceptButton = document.documentElement.getButton("accept");
  acceptButton.label = extensionsStrings.getString("aboutWindowCloseButton");
}

function loadHomepage(aURI) {
  window.close();
  window.opener.openURL(aURI);
}

