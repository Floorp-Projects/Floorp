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
 * The Original Code is the Mozilla Community QA Extension
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Zach Lipton <zach@zachlipton.com>
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
       while (p = this.getPasswordObj()) {
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
            var newLogin = new nsLoginInfo('chrome://qa', 'Litmus Login', litmus.baseURL,
              username, password, null, null);
            this.manager().addLogin(newLogin);
    },
    getPasswordObj: function() {
      try {
        var logins = this.manager().findLogins({}, 'chrome://qa',
          'Litmus Login', litmus.baseURL);
        if (logins.length > 0 && logins[0] != null)
          return logins[0];
        return false;
      } catch(ex) {
        return false;
      }
    }
  };
}
