//
// The contents of this file are subject to the Mozilla Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/MPL/
// 
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
// implied. See the License for the specific language governing
// rights and limitations under the License.
// 
// The Original Code is the Netscape security libraries.
// 
// The Initial Developer of the Original Code is Netscape
// Communications Corporation.  Portions created by Netscape are 
// Copyright (C) 1994-2000 Netscape Communications Corporation.  All
// Rights Reserved.
// 
// Contributor(s):
// 
// Alternatively, the contents of this file may be used under the
// terms of the GNU General Public License Version 2 or later (the
// "GPL"), in which case the provisions of the GPL are applicable 
// instead of those above.  If you wish to allow use of your 
// version of this file only under the terms of the GPL and not to
// allow others to use your version of this file under the MPL,
// indicate your decision by deleting the provisions above and
// replace them with the notice and other provisions required by
// the GPL.  If you do not delete the provisions above, a recipient
// may use your version of this file under either the MPL or the
// GPL.
//
////////////////////////////////////////////////////////////////////////////////////////
// Crypto Mechanism Flags
PKCS11_MECH_RSA_FLAG           =  0x1<<0; 
PKCS11_MECH_DSA_FLAG           =  0x1<<1; 
PKCS11_MECH_RC2_FLAG           =  0x1<<2; 
PKCS11_MECH_RC4_FLAG           =  0x1<<3; 
PKCS11_MECH_DES_FLAG           =  0x1<<4; 
PKCS11_MECH_DH_FLAG            =  0x1<<5; //Diffie-Hellman 
PKCS11_MECH_SKIPJACK_FLAG      =  0x1<<6; //SKIPJACK algorithm as in Fortezza cards 
PKCS11_MECH_RC5_FLAG           =  0x1<<7; 
PKCS11_MECH_SHA1_FLAG          =  0x1<<8; 
PKCS11_MECH_MD5_FLAG           =  0x1<<9; 
PKCS11_MECH_MD2_FLAG           =  0x1<<10; 
PKCS11_MECH_RANDOM_FLAG        =  0x1<<27; //Random number generator 
PKCS11_PUB_READABLE_CERT_FLAG  =  0x1<<28; //Stored certs can be read off the token w/o logging in 
PKCS11_DISABLE_FLAG            =  0x1<<30; //tell Navigator to disable this slot by default 

// Important: 
// 0x1<<11, 0x1<<12, ... , 0x1<<26, 0x1<<29, and 0x1<<31 are reserved 
// for internal use in Navigator. 
// Therefore, these bits should always be set to 0; otherwise,
// Navigator might exhibit unpredictable behavior. 

// These flags indicate which mechanisms should be turned on by 
var pkcs11MechanismFlags = PKCS11_MECH_RANDOM_FLAG; 
  
////////////////////////////////////////////////////////////////////////////////////////
// Ciphers that support SSL or S/MIME 
PKCS11_CIPHER_FORTEZZA_FLAG    = 0x1<<0; 

// Important: 
// 0x1<<1, 0x1<<2, ... , 0x1<<31 are reserved 
// for internal use in Navigator. 
// Therefore, these bits should ALWAYS be set to 0; otherwise, 
// Navigator might exhibit unpredictable behavior. 

// These flags indicate which SSL ciphers are supported 
var pkcs11CipherFlags = PKCS11_CIPHER_FORTEZZA_FLAG; 
  
////////////////////////////////////////////////////////////////////////////////////////
// Return values of pkcs11.addmodule() & pkcs11.delmodule() 
// success codes 
JS_OK_ADD_MODULE                 = 3; // Successfully added a module 
JS_OK_DEL_EXTERNAL_MODULE        = 2; // Successfully deleted ext. module 
JS_OK_DEL_INTERNAL_MODULE        = 1; // Successfully deleted int. module 

// failure codes 
JS_ERR_OTHER                     = -1; // Other errors than the followings 
JS_ERR_USER_CANCEL_ACTION        = -2; // User abort an action 
JS_ERR_INCORRECT_NUM_OF_ARGUMENTS= -3; // Calling a method w/ incorrect # of arguments 
JS_ERR_DEL_MODULE                = -4; // Error deleting a module 
JS_ERR_ADD_MODULE                = -5; // Error adding a module 
JS_ERR_BAD_MODULE_NAME           = -6; // The module name is invalid 
JS_ERR_BAD_DLL_NAME              = -7; // The DLL name is bad 
JS_ERR_BAD_MECHANISM_FLAGS       = -8; // The mechanism flags are invalid 
JS_ERR_BAD_CIPHER_ENABLE_FLAGS   = -9; // The SSL, S/MIME cipher flags are invalid 
JS_ERR_ADD_MODULE_DULICATE       =-10; // Module with the same name already installed

////////////////////////////////////////////////////////////////////////////////////////
// Find out which library is to be installed depending on the platform

// pathname seperator is platform specific
var sep = "/";
var vendor = "netscape";
var moduleName = "not_supported";
var plat = navigator.platform;
 
var dir = "pkcs11/" + vendor + "/" + plat + "/";
if (plat == "Win16") {
    dir = "pkcs11/";
}

bAbort = false;
if (plat == "Win32") {
    moduleName = "fort32.dll";
    sep = "\\";
} else if (plat == "Win16") {
    moduleName = "FORT16.DLL";
    sep = "\\";
} else if (plat == "MacPPC") {
    moduleName = "FortPK11Lib";
    sep = ":";
} else if (plat == "AIX4.1") {
    moduleName = "libfort_shr.a";
} else if (plat == "SunOS4.1.3_U1") {
    moduleName = "libfort.so.1.0";        
} else if ((plat == "SunOS5.4") || (plat == "SunOS5.5.1")){
    moduleName = "libfort.so";
} else if ((plat == "HP-UXA.09") || (plat == "HP-UXB.10")){
    moduleName = "libfort.sl";
} else if (plat == "IRIX6.2"){
    // The module only works on 6.3, but Communicator returns 6.2 even when
    // running 6.3.  So in order to prevent the user from thinking
    // the module actually works on 6.2, we will force the name to 
    // say 6.3 instead of 6.2.  In the even the user tries to install
    // on 6.2, the user will see 6.3 instead.  If they don't get it that
    // it's not going to work at this point in time, then the entire install
    // process wil just fail miserably, and that is OK.
    plat = "IRIX6.3";
    moduleName = "libfort.so";
} else {
    window.alert("Sorry, platform "+plat+" is not supported.");
    bAbort = true;
}

////////////////////////////////////////////////////////////////////////////////////////
// Installation Begins...
if (!bAbort) {
if (confirm("This script will install a security module. \nIt may over-write older files having the same name. \nDo you want to continue?")) { 
 // Step 1. Create a version object and a software update object 
 vi = new netscape.softupdate.VersionInfo(1, 6, 0, 0); 
 su = new netscape.softupdate.SoftwareUpdate(this, "Fortezza Card PKCS#11 Module"); 
                 // "Fortezza ... Module" is the logical name of the bundle 

 ////////////////////////////////////////
 // Step 2. Start the install process 
 bAbort = false; 
 err = su.StartInstall("NSfortezza", // NSfortezza is the component folder (logical) 
                       vi, 
                       netscape.softupdate.SoftwareUpdate.FULL_INSTALL); 
                            
 bAbort = (err!=0); 

 if (err == 0) { 
     ////////////////////////////////////////
    // Step 3. Find out the physical location of the Program dir 
    Folder = su.GetFolder("Program"); 

     ////////////////////////////////////////
    // Step 4. Install the files. Unpack them and list where they go 

    err = su.AddSubcomponent("FortezzaLibrary_"+plat, //component name (logical) 
                                    vi, // version info 
                                    moduleName, // source file in JAR (physical) 
                                    Folder, // target folder (physical) 
                                    dir + moduleName, // target path & filename (physical) 
                                    true); // forces update 
    if (err != 0) {
        if (err == -200) {
             errmsg = "Bad Package Name.";
        } else if (err == -201) {
             errmsg = "Unexpected error.";
        } else if (err == -203) {
             errmsg = "Installation script was signed by more than one certificate.";
        } else if (err == -204) {
             errmsg = "Installation script was not signed."
        } else if (err == -205) {
             errmsg = "The file to be installed is not signed."
        } else if (err == -206) {
             errmsg = "The file to be installed is not present, or it was signed with a different certificate than the one used to sign the install script.";
        } else if (err == -207) {
             errmsg = "JAR archive has not been opened."
        } else if (err == -208) {
             errmsg = "Bad arguments to AddSubcomponent( )."
        } else if (err == -209) {
             errmsg = "Illegal relative path( )."
        } else if (err == -210) {
             errmsg = "User cancelled installation."
        } else if (err == -211) {
             errmsg = "A problem occurred with the StartInstall( )."
        } else {
             errmsg = "Unknown error";
        }
        window.alert("Error adding sub-component: "+"("+err+")"+errmsg); 
        //window.alert("Aborting, Folder="+Folder+" module="+dir+moduleName);
        bAbort = true;
    }
 } 

 ////////////////////////////////////////
 // Step 5. Unless there was a problem, move files to final location 
 // and update the Client Version Registry 
 if (bAbort) { 
    su.AbortInstall(); 
 } else { 
    err = su.FinalizeInstall(); 

    if (err != 0) {

        if (err == -900) {
             errmsg = "Restart the computer, and install again.";
        } else if (err == -201) {
             errmsg = "Unexpected error.";
        } else if (err == -202) {
             errmsg = "Access denied. Make sure you have the permissions to write to the disk.";    
        } else if (err == -203) {
             errmsg = "Installation script was signed by more than one certificate.";
        } else if (err == -204) {
             errmsg = "Installation script was not signed."
        } else if (err == -205) {
             errmsg = "The file to be installed is not signed."
        } else if (err == -206) {
             errmsg = "The file to be installed is not present, or it was signed with a different certificate than the one used to sign the install script." 
        } else if (err == -207) {
             errmsg = "JAR archive has not been opened."
        } else if (err == -208) {
             errmsg = "Bad arguments to AddSubcomponent( )."
        } else if (err == -209) {
             errmsg = "Illegal relative path( )."
        } else if (err == -210) {
             errmsg = "User cancelled installation."
        } else if (err == -211) {
             errmsg = "A problem occurred with the StartInstall( )."
        } else {
             errmsg = "\nIf you have FORTEZZA module already installed, try deleting it first.";
        }
        window.alert("Error Finalizing Install: "+"("+err+")"+errmsg); 
        //window.alert("Aborting, Folder="+Folder+" module="+dir+moduleName);

    } else {

        // Platform specific full path 
        if (plat=="Win16") { 
            fullpath = Folder +  "pkcs11" + sep + moduleName; 
        } else { 
            fullpath = Folder +  "pkcs11" + sep + vendor + sep + plat + sep + moduleName; 
        } 
 
         ////////////////////////////////////////
        // Step 6: Call pkcs11.addmodule() to register the newly downloaded module
        moduleCommonName = "Netscape FORTEZZA Module " + plat;
        result = pkcs11.addmodule(moduleCommonName, 
                              fullpath, 
                              pkcs11MechanismFlags, 
                              pkcs11CipherFlags); 
       if (result == -10) {
           window.alert("New module was copied to destination, \nbut setup failed because a module "
                   +"with the same name has been installed. \nTry deleting the module "
                   + moduleCommonName +" first.")
       } else if (result < 0) {
           window.alert("New module was copied to destination, but setup failed.  Error code: " + result); 
       }           
    } 
 } 
}
}
