function displaySecurityAdvisor()
{
    var psm = Components.classes["component://netscape/psm"].getService();
    psm = psm.QueryInterface(Components.interfaces.nsIPSMComponent);
    psm.DisplaySecurityAdvisor( null );
}

