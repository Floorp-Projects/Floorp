/*** -*- Mode: Javascript; tab-width: 2;

The contents of this file are subject to the Mozilla Public
License Version 1.1 (the "License"); you may not use this file
except in compliance with the License. You may obtain a copy of
the License at http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS
IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
implied. See the License for the specific language governing
rights and limitations under the License.

The Original Code is Collabnet code.
The Initial Developer of the Original Code is Collabnet.

Portions created by Collabnet are
Copyright (C) 2000 Collabnet.  All
Rights Reserved.

Contributor(s): Pete Collins, Doug Turner, Brendan Eich, Warren Harris


*******
*
* JS File IO API (The purpose of this file is to make it a little easier to do file IO from js) 
*
*       io.js
*
* Function List
*
*       1. open(path, mode);              // open a file handle for reading, writing or appending
*       2. exists(path);                  // check to see if a file exists
*       3. read();                        // returns the contents of a file
*       4. write(contents, permissions);  // permissions are optional 
*       5. append(dirPath, fileName);     // append is for abstracting platform specific file paths
*       6. mkdir(path, permissions);      // permissions are optional
*       7. rmdir(path);                   // remove a directory
*       8. rm(path);                      // remove a file
*       9. copy(source, dest);            // copy a file from source to destination
*       10.leaf(path);                    // leaf is the endmost file string ex: foo.html in /myDir/foo.html
*       11.readDir(dirPath);              // returns an array listing of a dirs contents 
*       12.permissions(path);             // returns the files permissions
*       13.dateModified(path);            // returns the last modified date in locale string
*
*       Instructions:
*
*       First include this js file 
*
*       var file = new File();
*
*       to open a file for reading<"r">, writing<"w"> or appending<"a">, 
*       just call: 
*
*       file.open('/path/i/want/to/open', "w");
*
*       where in this case you will be creating a new file called:
*       '/path/i/want/to/open'
*       with a mode of "w" which means you want to write a new file.
*
*       If you want to read from a file, just call:
*
*       file.open('/path/i/want/to/open', "r");
*       
*       var theFilesContents    = file.read();
*
*       the file contents will be returned to the caller
*       so you can do something usefull with it
*
*       file.close(); 
*
*       destroys any created objects
*
*       if you forget to use file.close() no probs all objects are discarded anyway
*
*       Warning: these API's are not for religious types
*
*************/

/****************** Globals **********************/

const FilePath    = new Components.Constructor( "component://mozilla/file/local", "nsILocalFile", "initWithPath");
const FileChannel = new Components.Constructor( "component://netscape/network/local-file-channel", "nsIFileChannel" );
const InputStream = new Components.Constructor( "component://netscape/scriptableinputstream", "nsIScriptableInputStream" );

/****************** Globals **********************/


/****************** File Object Class *********************/

function File(path, mode) {
    
    if(path)
      this.open(path, mode);

} // constructor

File.prototype  = {

mode            : null,

fileInst        : null,

fileChannel     : null,

inputStream     : null,

/********************* EXISTS ***************************/

exists : function (path) {

    if(!path)
    {
      dump("io.js:exists:ERROR: Missing path argument\n\n");
      return false;
    }

    try
    { 
      var file                = new FilePath(path);
      var fileExists          = file.exists();
      return fileExists;
    }

    catch(e) { dump("io.js:exists:ERROR: "+e+"\n\n"); }

  return null;
},

/********************* EXISTS ***************************/


/********************* OPEN *****************************/


open : function(path, setMode) {

    if(!setMode)
      setMode="r";

    if(!path)
    {
      dump("io.js:open:ERROR: Missing path argument\n\n");
      return false;
    }

  switch(setMode)
  {

  case "w":

    try
    {
      this.close();
      this.fileInst           = new FilePath( path );
      var fileExists          = this.exists( path );

    if(fileExists)
    {
      this.fileInst["delete"](false);
      fileExists=false;
    }

    if (!fileExists)
    {
      this.fileInst.create(0, 0644);
      fileExists=true;
    }

    this.mode=8;
    return true;

    }

    catch (e){ dump("io.js:open:ERROR::"+e+"\n\n"); }
    break;


  case "a":

    try
    {
      this.close();
      this.fileInst           = new FilePath(path);
      fileExists              = this.exists(path);

    if ( !fileExists )
    {
      this.fileInst.create(0, 0644);
      fileExists=true;
    }

    this.mode=16;
    return true;

    }

    catch (e){ dump("io.js:open:ERROR::"+e+"\n\n"); }
    break;


  case "r":

    this.close();

    try 
    {
      this.fileInst                = new FilePath(path);
      this.fileChannel             = new FileChannel();
      this.inputStream             = new InputStream();    
    }
        
    catch (e){ dump("io.js:open:ERROR::"+e+"\n\n"); }
    break;

    default:
    dump("io.js:open:WARNING: \""+setMode+"\" is an Invalid file mode\n\n");

    }

  return true;

},

/********************* OPEN *****************************/


/********************* WRITE ****************************/

write : function(buffer, perms) {

    if(!this.fileInst)
    {
      dump("io.js:write:ERROR: Please open a file handle first\n");
      return false;
    }
    
    if(!buffer)
      buffer                  = " ";

      var buffSize            = buffer.length;

    try
    {
      if(!this.fileChannel)
        this.fileChannel      = new FileChannel();

    if(perms)
    {
        var checkPerms        = this.validatePermissions(perms);

    if(!checkPerms){
      dump("io.js:write:ERROR: Sorry invalid permissions set\n\n");
      return false;
    }               

    }

    if(!perms)
      perms=0644;

    this.fileChannel.init(this.fileInst, this.mode, perms)
    this.fileInst.permissions=perms;

    var fileSize            = parseInt( this.fileInst.fileSize );

    var outStream           = this.fileChannel.openOutputStream();

    if( outStream.write(buffer, buffSize) )
      dump("io.js:write: Write to file successful . . . \n\n");
      outStream.flush();
    }

    catch (e){ dump("io.js:write:ERROR: "+e+"\n\n"); }

  return true;

},

/********************* WRITE ****************************/


/********************* READ *****************************/

read : function() {

    try 
    {

    if(!this.fileInst || !this.fileChannel || !this.inputStream)
    {
      dump("io.js:read:ERROR: Please open a valid file handle for reading\n");
      return null;
    }
        
    var fileExists          = this.exists(this.fileInst.path);

    if(!fileExists) 
    {
        dump("io.js:read:ERROR: \""+this.fileInst.path+"\" does not exist\n");
        return false;
    }

    var offset              = this.fileInst.fileSize;
    var perm                = this.fileInst.permissions;     

    this.fileChannel.init(this.fileInst, 1, perm);

    var inStream = this.fileChannel.openInputStream();

    this.inputStream.init(inStream);

    var fileContents        = this.inputStream.read(this.fileInst.fileSize);

    inStream.close();
        
    return fileContents;

    }

    catch (e){ dump("io.js:read:ERROR: "+e+"\n\n"); }

    return false;

},

/********************* READ *****************************/


/********************* MKDIR ****************************/

mkdir : function(path, permissions) {

  this.close();

  if(!path)
  {
    dump("io.js:rm:ERROR: Missing path argument\n\n");
    return false;
  }

  var fileExists  = this.exists(path);

  if(permissions){
    var checkPerms  = this.validatePermissions(permissions);

  if(!checkPerms){
    dump("io.js:mkdir:ERROR: Sorry invalid permissions set\n\n");
    return false;
  }               

  var baseTen       = permissions.toString(10);

  if(baseTen.substring(0,1) != 0)
    permissions = 0+baseTen;
  }

  try
  { 
    this.fileInst  = new FilePath(path);

    if (!fileExists)
    {
      if(!permissions)
        permissions     = 0755;

    this.fileInst.create( 1, parseInt(permissions) ); 

    }

    else 
      return false; 
  }

  catch (e){ dump("io.js:mkdir:ERROR: "+e+"\n\n"); }

  this.close();
  return true;

},

/********************* MKDIR ****************************/


/********************* RM *******************************/

rm : function (path) {

    this.close();

    if(!path)
    {
      dump("io.js:rm:ERROR: Missing path argument\n\n");
      return false;
    }

    if(!this.exists(path))
    {
      dump("io.js:rm:ERROR: Sorry, file "+path+" doesn't exist\n\n");
      return false;
    }

    try
    { 

    this.fileInst            = new FilePath(path);

      if(this.fileInst.isDirectory())
      {
        dump("io.js:rm:ERROR:Sorry file is a directory. Try rmdir() instead\n");
        return false;
      }

    this.fileInst['delete'](false);
    this.close();
    return true;

    }

    catch (e){ dump("io.js:rm:ERROR: ERROR:"+e+"\n\n"); }

  return null;

},

/********************* RM *******************************/


/********************* RMDIR ****************************/

rmdir : function (path) {

  this.close();

  if(!path)
  {
    dump("io.js:rmdir:ERROR: Missing path argument\n\n");
    return false;
  }

  if(!this.exists(path))
  {
    dump("io.js:rmdir:ERROR: Sorry, file "+path+" doesn't exist\n\n");
    return false;
  }

  try
  { 
    this.fileInst            = new FilePath(path);
    this.fileInst['delete'](true);
    this.close();
    return true;
  }

  catch (e){ dump("io.js:rmdir: ERROR: "+e+"\n\n"); }

  return null;

},

/********************* RMDIR ****************************/

/********************* COPY *****************************/

copy  : function (source, dest) {

  if(!source || !dest)
  {
    dump("io.js:copy:ERROR: Missing path argument\n\n");
    return false;
  }

  if(!this.exists(source))
  {
    dump("io.js:copy:ERROR: Sorry, source file "+source+" doesn't exist\n\n");
    return false;
  }

  try
  { 
    var fileInst      = new FilePath(source);
    var dir           = new FilePath(dest);

    var copyName      = fileInst.leafName;

    if(fileInst.isDirectory())
    {
      dump("io.js:copy:NOT IMPLEMENTED: Sorry, you can't copy a directory yet\n\n");
      return false;
    }

    if(!this.exists(dest) || !dir.isDirectory())
    {
      copyName          = dir.leafName;
      dump(dir.path.replace(copyName,'')+'\n\n');
      dir               = new FilePath(dir.path.replace(copyName,''));

      if(!this.exists(dir.path))
      {
        dump("io.js:copy:ERROR: Sorry, dest directory "+dir.path+" doesn't exist\n\n");
        return false;
      }

      if(!dir.isDirectory())
      {
        dump("io.js:copy:ERROR: Sorry, destination dir "+dir.path+" is not a valid dir path\n\n");
        return false;
      }

    }

    if(this.exists(this.append(dir.path, copyName)))
    {
      dump("io.js:copy:ERROR: Sorry destination file "+this.append(dir.path, copyName)+" already exists\n\n");
      return false;
    }

    fileInst.copyTo(dir, copyName);
    dump('io.js:copy successful!\n\n');
    this.close();
    return true;

  }

  catch (e){ dump("io.js:copy:ERROR: "+e+"\n\n"); }

  return null;  

},

/********************* COPY *****************************/


/********************* LEAF *****************************/

leaf  : function (path) {

  if(!path)
  {
    dump("io.js:leaf:ERROR: Missing path argument\n\n");
    return false;
  }

  if(!this.exists(path))
  {
    dump("io.js:leaf:ERROR: File doesn't exist\n\n");
    return false;
  }

  try
  {
    var fileInst = new FilePath(path);
    return fileInst.leafName;
  }

  catch(e) { dump("io.js:leaf:ERROR: "+e+"\n\n"); }
  
  return null;

},

/********************* LEAF *****************************/


/********************* READDIR **************************/

readDir : function (dirPath) {

  if(!dirPath)
  {
    dump("io.js:readDir:ERROR: Missing path argument\n\n");
    return false
  }

  if(!this.exists(dirPath))
  {
    dump("io.js:readDir:ERROR: Sorry, directory "+dirPath+" doesn't exist\n\n");
    return false;
  }

  try
  {

    var file      = new FilePath(dirPath);

    if(!file.isDirectory())
    {
      dump("io.js:readDir:ERROR: Sorry, "+dirPath+" is not a directory\n\n");
      return false;
    }

    var files     = file.directoryEntries;
    var listings  = new Array();

    while(files.hasMoreElements())
    listings.push(files.getNext().QueryInterface(Components.interfaces.nsILocalFile).path);

    return listings;

  }

  catch(e) { dump("io.js:readDir:ERROR: "+e+"\n\n"); }
  
  return null;

},

/********************* READDIR **************************/


/********************* APPEND ***************************/

append : function (dirPath, fileName) {

    if(!dirPath || !fileName)
    {
      dump("io.js:append:ERROR: Missing path argument\n\n");
      return false;
    }

    if(!this.exists(dirPath))
    {
      dump("io.js:append:ERROR: File doesn't exist\n\n");
      return false;
    }

    try
    { 
      this.fileInst           = new FilePath(dirPath);
      var fileAppended        = this.fileInst.append(fileName);
      fileAppended            = this.fileInst.path;
      return fileAppended;
    }

    catch(e) { dump("io.js:append:ERROR: "+e+"\n\n"); }

    return null;
},

/********************* APPEND ***************************/


/********************* VALIDATE PERMISSIONS *************/

validatePermissions : function(num) {

    if ( parseInt(num.toString(10).length) < 3 ) 
      return false;

    return true;

},

/********************* VALIDATE PERMISSIONS *************/


/********************* PERMISSIONS **********************/

permissions : function (path) {

    if(!path)
    {
      dump("io.js:permissions:ERROR: Missing path argument\n\n");
      return false;
    }
    
    if(!this.exists(path))
    {
      dump("io.js:permissions:ERROR: File doesn't exist\n\n");
      return false;
    }

    try
    { 
      var fileInst  = new FilePath(path); 
      return fileInst.permissions.toString(8);
    }

    catch(e)
      { dump("io.js:permissions:ERROR: "+e+"\n"); return false; }

    return null;

},

/********************* PERMISSIONS **********************/


/********************* MODIFIED *************************/

dateModified  : function (path) {

    if(!path)
    {
      dump("io.js:dateModified:ERROR: Missing path argument\n\n");
      return false;
    }
    
    if(!this.exists(path))
    {
      dump("io.js:dateModified:ERROR: File doesn't exist\n\n");
      return false;
    }

    try
    { 
      var fileInst  = new FilePath(path); 
      var date = new Date(fileInst.lastModificationDate).toLocaleString();
      return date;
    }

    catch(e)
      { dump("io.js:dateModified:ERROR: "+e+"\n"); return false; }

    return null;

},

/********************* MODIFIED *************************/


/********************* CLOSE ****************************/

close : function() {

    /***************** Destroy Instances *********************/
    if(this.fileInst)       delete this.fileInst;
    if(this.fileChannel)    delete this.fileChannel;
    if(this.inputStream)    delete this.inputStream;
    if(this.mode)           this.mode=null;
    /***************** Destroy Instances *********************/
  
  return true;

}

/********************* CLOSE ****************************/

};

/****************** End File Object Class *********************/


/************** Create an instance of file object *************/

//file          = new File(); // commenting out for now, the user can initialize the object . . . 

/************** Create an instance of file object *************/



