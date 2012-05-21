/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var qaPref = {
   litmus : null,
   prefBase : "qa.extension",

   setPref : function(aName, aValue, aType) {

     var pref = Components.classes["@mozilla.org/preferences-service;1"]
              .getService(Components.interfaces.nsIPrefBranch);
     try{
       if(aType == "bool")
         pref.setBoolPref(aName, aValue);
       else if(aType == "int")
         pref.setIntPref(aName, aValue);
       else if(aType == "char")
         pref.setCharPref(aName, aValue);
     }
     catch(e){ };
   },

   getPref : function(aName, aType) {

     var pref = Components.classes["@mozilla.org/preferences-service;1"]
              .getService(Components.interfaces.nsIPrefBranch);
     try{
       var result;
       if(aType == "bool")
         result = pref.getBoolPref(aName);
       else if(aType == "int")
         result = pref.getIntPref(aName);
       else if(aType == "char")
         result = pref.getCharPref(aName);

     return result;
     }
     catch(e){
       if(aType == "bool"){
         return false;
       }
       else if(aType == "int"){
         return 0;
       }
       else if(aType == "char"){
         return null;
       }
     }
   return null;
   }
};

// public API:
// qaPref.litmus.getUsername()
// qaPref.litmus.getPassword()
// qaPref.litmus.setPassword()
// everything else is abstracted away to deal with fx2 vs fx3 changes

var CC_passwordManager = Components.classes["@mozilla.org/passwordmanager;1"];
var CC_loginManager = Components.classes["@mozilla.org/login-manager;1"];

  if (CC_passwordManager != null) { // old-fashioned password manager
  qaPref.litmus = {
     passwordName : function() {return "mozqa-extension-litmus-"+litmus.baseURL},

     manager : function() {
       return Components.classes["@mozilla.org/passwordmanager;1"].
           getService(Components.interfaces.nsIPasswordManager);
     },
     getPasswordObj : function() {
       var m = this.manager();
       var e = m.enumerator;

       // iterate through the PasswordManager service to find
       // our password:
       var password;
       while(e.hasMoreElements()) {
         try {
           var pass = e.getNext().
             QueryInterface(Components.interfaces.nsIPassword);
           if (pass.host == this.passwordName()) {
             password=pass; // found it
             return password;
           }
         } catch (ex) { return false; }
       }
       if (!password) { return false; }
       return password;
     },
     getUsername : function() {
       var password = this.getPasswordObj();
       try {
         return password.user;
       } catch (ex) { return false; }
     },
     getPassword : function() {
       var password=this.getPasswordObj();
       try {
         return password.password;
       } catch (ex) { return false; }
     },
     setPassword : function(username, password) {
       var m = this.manager();
       // check to see whether we already have a password stored
       // for the current baseURL. If we do, remove it before adding
       // any new passwords:
       var p;
       while (p == this.getPasswordObj()) {
         m.removeUser(p.host, p.user);
       }
       m.addUser(this.passwordName(), username, password);
     }
  };
  } else if (CC_loginManager != null) { // use the new login manager interface
  qaPref.litmus = {
    manager : function() {
      return Components.classes["@mozilla.org/login-manager;1"]
               .getService(Components.interfaces.nsILoginManager);
    },
    getUsername : function() {
       var password = this.getPasswordObj();
       try {
         return password.username;
       } catch (ex) { return false; }
    },
    getPassword : function() {
       var password=this.getPasswordObj();
       try {
         return password.password;
       } catch (ex) { return false; }
    },
    setPassword : function(username, password) {
      // if we already have a password stored, remove it first:
      var login = this.getPasswordObj();
      if (login != false) {
        this.manager().removeLogin(login);
      }

      var nsLoginInfo = new Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                                        Components.interfaces.nsILoginInfo,
                                        "init");

      // The LoginManager does not like "null" for username/password fields,
      // So we send in the field names from the XUL.
      var newLogin = new nsLoginInfo('chrome://qa', null, litmus.baseURL,
                                     username, password, "username", "password");
      try {
        this.manager().addLogin(newLogin);
      } catch (err) {
        alert("ERROR: " + err);
      }
    },
    getPasswordObj: function() {
      try {
        var logins = this.manager().findLogins({}, 'chrome://qa',
          null, litmus.baseURL);
        if (logins.length > 0 && logins[0] != null)
          return logins[0];
        return false;
      } catch(ex) {
        return false;
      }
    }
  };
}
