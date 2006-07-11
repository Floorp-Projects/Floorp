<?xml version="1.0"?>
<RDF:RDF xmlns:RDF="http://www.w3.org/1999/02/22-rdf-syntax-ns#" xmlns:pfs="http://www.mozilla.org/2004/pfs-rdf#">

<RDF:Description about="urn:mozilla:plugin-results:{$plugin.mimetype|escape:html:"UTF-8"}">
 <pfs:plugins><RDF:Seq>
  <RDF:li resource="urn:mozilla:plugin:{$plugin.guid|escape:html:"UTF-8"}"/>
 </RDF:Seq></pfs:plugins>
</RDF:Description>

<RDF:Description about="urn:mozilla:plugin:{$plugin.guid|escape:html:"UTF-8"}">
 <pfs:updates><RDF:Seq>
  <RDF:li resource="urn:mozilla:plugin:{$plugin.guid|escape:html:"UTF-8"}:{$plugin.version|escape:html:"UTF-8"}"/>
 </RDF:Seq></pfs:updates>
</RDF:Description>

<RDF:Description about="urn:mozilla:plugin:{$plugin.guid|escape:html:"UTF-8"}:{$plugin.version|escape:html:"UTF-8"}">
 <pfs:name>{$plugin.name|escape:html:"UTF-8"}</pfs:name>
 <pfs:requestedMimetype>{$plugin.mimetype|escape:html:"UTF-8"}</pfs:requestedMimetype>
 <pfs:guid>{$plugin.guid|escape:html:"UTF-8"}</pfs:guid>
 <pfs:version>{$plugin.version|escape:html:"UTF-8"}</pfs:version>
 <pfs:IconUrl>{$plugin.iconUrl|escape:html:"UTF-8"}</pfs:IconUrl>
 <pfs:XPILocation>{$plugin.XPILocation|escape:html:"UTF-8"}</pfs:XPILocation>
 <pfs:InstallerShowsUI>{$plugin.installerShowsUI|escape:html:"UTF-8"}</pfs:InstallerShowsUI>
 <pfs:manualInstallationURL>{$plugin.manualInstallationURL|escape:html:"UTF-8"}</pfs:manualInstallationURL>
 <pfs:licenseURL>{$plugin.licenseURL|escape:html:"UTF-8"}</pfs:licenseURL>
 <pfs:needsRestart>{$plugin.needsRestart|escape:html:"UTF-8"}</pfs:needsRestart>
</RDF:Description>

</RDF:RDF>
