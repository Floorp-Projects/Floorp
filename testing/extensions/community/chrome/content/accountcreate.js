<!-- -*- Mode: HTML -*- 
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
# The Original Code is the Mozilla Community QA Extension
#
# The Initial Developer of the Original Code is Zach Lipton.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Hsieh <ben.hsieh@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the LGPL or the GPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** -->

var accountCreate = {
    oldPassword: null,    //TODO: is this secure?
    oldUsername: null,
    updateFunction : null,
	loadSetup : function() {
		document.getElementById('qa-setup-createaccount-iframe').src = 
		litmus.baseURL+'extension.cgi?createAccount=1';
        accountCreate.updateFunction = window.arguments[0];
        
        accountCreate.oldPassword = qaPref.litmus.getPassword();
        accountCreate.oldUsername = qaPref.litmus.getUsername();
	},
    validateAccount : function() {
			// we're creating a new account, so get the uname and passwd 
			// from the account created page:
			var page = document.getElementById('qa-setup-createaccount-iframe').
						contentDocument;
			if (!page) {
				alert("create account page is missing");
				return false;
			}
			if (page.wrappedJSObject == null)
				page.wrappedJSObject = page;
			if (page.forms[0] && page.forms[0].wrappedJSObject == null)
				page.forms[0].wrappedJSObject = page.forms[0];
			
			if (page.location == litmus.baseURL+'extension.cgi?createAccount=1'
			    && qaSetup.didSubmitForm==0) {
				page.forms[0].wrappedJSObject.submit();
				qaSetup.didSubmitForm = 1;
				setTimeout("qaSetup.validateAccount()", 5000);
				return false;
			}
			if (qaSetup.didSubmitForm == 1 && ! page.forms || 
			   ! page.forms[0].wrappedJSObject || 
			   ! page.forms[0].wrappedJSObject.email && 
			   ! page.forms[0].wrappedJSObject.email.value)  
			   		{qaSetup.didSubmitForm = 2;
			   		setTimeout("qaSetup.validateAccount()", 4000);
			   		return false;}
			var e = '';
			var p = '';
			if (page.forms && page.forms[0].wrappedJSObject && 
			    page.forms[0].wrappedJSObject.email && 
				page.forms[0].wrappedJSObject.email.value) 
				{ e=page.forms[0].wrappedJSObject.email.value }
			if (page.forms && page.forms[0].wrappedJSObject && 
			    page.forms[0].wrappedJSObject.password && 
				page.forms[0].wrappedJSObject.password.value) 
				{ p=page.forms[0].wrappedJSObject.password.value }
        // e is account, p is password
		
		var uname = e;
		var passwd = p;
		

        if (e == '' || p == '') {
            alert("No username or password has been registered.");
            return false;    //we need better error handling.
        }
        qaPref.litmus.setPassword(uname, passwd);
        accountCreate.updateFunction();
                
        // TODO: ideally we would validate against litmus, but...
		return true;
	},
    
    handleCancel : function() {
        qaPref.litmus.setPassword(oldUsername, oldPassword);
    }
}
