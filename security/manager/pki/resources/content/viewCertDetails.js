function setWindowName()
{
  myName = self.name;
//  alert(myName);
  var windowReference=document.getElementById('certDetails');
  windowReference.setAttribute("title","Certificate Detail: "+myName);

  certmgr = Components
            .classes["@mozilla.org/security/certmanager;1"]
            .createInstance();
  certmgr = certmgr.QueryInterface(Components
                                   .interfaces
                                   .nsICertificateManager);

  cnstr = certmgr.getCertCN(myName);
  var cn=document.getElementById('commonname');
  cn.setAttribute("value", cnstr);
  // for now
  orgstr = certmgr.getCertCN(myName);
  var org=document.getElementById('organization');
  org.setAttribute("value", orgstr);
  oustr = certmgr.getCertCN(myName);
  var ou=document.getElementById('orgunit');
  ou.setAttribute("value", oustr);
}
