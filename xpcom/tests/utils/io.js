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
*       14.size(path);                    // returns the file size
*       15.extension(path);               // returns a file extension if there is one
*       16.help();                        // currently dumps a list of available functions 
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
*       file.open("/path/i/want/to/open", "w");
*
*       where in this case you will be creating a new file called:
*       '/path/i/want/to/open'
*       with a mode of "w" which means you want to write a new file.
*
*       If you want to read from a file, just call:
*
*       file.open("/path/i/want/to/open", "r");
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

const FilePath    = new Components.Constructor( "@mozilla.org/file/local;1", "nsILocalFile", "initWithPath");
const FileChannel = new Components.Constructor( "@mozilla.org/network/local-file-channel;1", "nsIFileChannel" );
const InputStream = new Components.Constructor( "@mozilla.org/scriptableinputstream;1", "nsIScriptableInputStream" );

const READ        = 0x01;     // 1
const WRITE       = 0x08;     // 8
const APPEND      = 0x10;     // 16

const FILE        = 0x00;     // 0
const DIRECTORY   = 0x01;     // 1

const OK          = true;

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
      return null;
    }

    var retval;
    try
    { 
      var file                = new FilePath(path);
      retval=file.exists();
    }

    catch(e) { 
      dump("io.js:exists:ERROR: "+e+"\n\n"); 
      retval=null;
    }

  return retval;
},

/********************* EXISTS ***************************/


/********************* OPEN *****************************/


open : function(path, setMode) {

    if(!setMode)
      setMode="r";

    if(!path)
    {
      dump("io.js:open:ERROR: Missing path argument\n\n");
      return null;
    }

  var retval;

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
      this.fileInst.create(FILE, 0644);
      fileExists=true;
    }

    this.mode=WRITE;
    retval=OK;

    }

    catch (e)
    { 
      dump("io.js:open:ERROR::"+e+"\n\n"); 
      return null;
    }
    break;

  case "a":

    try
    {
      this.close();
      this.fileInst           = new FilePath(path);
      fileExists              = this.exists(path);

    if ( !fileExists )
    {
      this.fileInst.create(FILE, 0644);
      fileExists=true;
    }

    this.mode=APPEND;
    retval=OK;

    }

    catch (e)
    { 
      dump("io.js:open:ERROR::"+e+"\n\n"); 
      return null;
    }
    break;


  case "r":

    this.close();

    try 
    {
      this.fileInst                = new FilePath(path);
      this.fileChannel             = new FileChannel();
      this.inputStream             = new InputStream();    
      retval=OK;
    }
        
    catch (e)
    { 
      dump("io.js:open:ERROR::"+e+"\n\n"); 
      retval=null;
    }
    break;

    default:
    dump("io.js:open:WARNING: \""+setMode+"\" is an Invalid file mode\n\n");
    return null;

    }

  return retval;

},

/********************* OPEN *****************************/


/********************* WRITE ****************************/

write : function(buffer, perms) {

    if(!this.fileInst)
    {
      dump("io.js:write:ERROR: Please open a file handle first\n");
      return null;
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
      return null;
    }               

    }

    if(!perms)
      perms=0644;

    this.fileChannel.init(this.fileInst, this.mode, perms)
    this.fileInst.permissions=perms;

    var fileSize            = parseInt( this.fileInst.fileSize );

    var outStream           = this.fileChannel.open();

    if( outStream.write(buffer, buffSize) )
      dump("io.js:write: Write to file successful . . . \n\n");
      outStream.flush();
    }

    catch (e)
    { 
      dump("io.js:write:ERROR: "+e+"\n\n"); 
      return null;
    }

  return OK;

},

/********************* WRITE ****************************/


/********************* READ *****************************/

read : function() {

    var retval;

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
        return null;
    }

    var offset              = this.fileInst.fileSize;
    var perm                = this.fileInst.permissions;     

    this.fileChannel.init(this.fileInst, READ, perm);

    var inStream = this.fileChannel.open();

    this.inputStream.init(inStream);

    var fileContents        = this.inputStream.read(this.fileInst.fileSize);

    inStream.close();
        
    retval=fileContents;

    }

    catch (e)
    { 
      dump("io.js:read:ERROR: "+e+"\n\n"); 
      retval=null;
    }

    return retval;

},

/********************* READ *****************************/


/********************* MKDIR ****************************/

mkdir : function(path, permissions) {

  this.close();

  if(!path)
  {
    dump("io.js:rm:ERROR: Missing path argument\n\n");
    return null;
  }

  var fileExists    = this.exists(path);

  if(permissions){
    var checkPerms  = this.validatePermissions(permissions);

  if(!checkPerms){
    dump("io.js:mkdir:ERROR: Sorry invalid permissions set\n\n");
    return null;
  }               

  var baseTen       = permissions.toString(10);

  if(baseTen.substring(0,1) != 0)
    permissions = 0+baseTen;
  }

  var retval;
  try
  { 
    this.fileInst  = new FilePath(path);

    if (!fileExists)
    {
      if(!permissions)
        permissions     = 0755;

    this.fileInst.create( DIRECTORY, parseInt(permissions) ); 
    retval=OK;

    }

    else 
      retval=false; 
  }

  catch (e)
  { 
    dump("io.js:mkdir:ERROR: "+e+"\n\n"); 
    return null;
  }

  this.close();
  return retval;

},

/********************* MKDIR ****************************/


/********************* RM *******************************/

rm : function (path) {

    this.close();

    if(!path)
    {
      dump("io.js:rm:ERROR: Missing path argument\n\n");
      return null;
    }

    if(!this.exists(path))
    {
      dump("io.js:rm:ERROR: Sorry, file "+path+" doesn't exist\n\n");
      return null;
    }

    var retval;
    try
    { 

    this.fileInst            = new FilePath(path);

      if(this.fileInst.isDirectory())
      {
        dump("io.js:rm:ERROR:Sorry file is a directory. Try rmdir() instead\n");
        return null;
      }

    this.fileInst['delete'](false);
    this.close();
    retval=OK;

    }

    catch (e)
    { 
      dump("io.js:rm:ERROR: ERROR:"+e+"\n\n"); 
      retval=null;
    }

  return retval;

},

/********************* RM *******************************/


/********************* RMDIR ****************************/

rmdir : function (path) {

  this.close();

  if(!path)
  {
    dump("io.js:rmdir:ERROR: Missing path argument\n\n");
    return null;
  }

  if(!this.exists(path))
  {
    dump("io.js:rmdir:ERROR: Sorry, file "+path+" doesn't exist\n\n");
    return null;
  }

  var retval;
  try
  { 
    this.fileInst            = new FilePath(path);
    this.fileInst['delete'](true);
    this.close();
    retval=OK;
  }

  catch (e)
  { 
    dump("io.js:rmdir: ERROR: "+e+"\n\n"); 
    retval=null;
  }

  return retval;

},

/********************* RMDIR ****************************/

/********************* COPY *****************************/

copy  : function (source, dest) {

  if(!source || !dest)
  {
    dump("io.js:copy:ERROR: Missing path argument\n\n");
    return null;
  }

  if(!this.exists(source))
  {
    dump("io.js:copy:ERROR: Sorry, source file "+source+" doesn't exist\n\n");
    return null;
  }

  var retval;
  try
  { 
    var fileInst      = new FilePath(source);
    var dir           = new FilePath(dest);

    var copyName      = fileInst.leafName;

    if(fileInst.isDirectory())
    {
      dump("io.js:copy:NOT IMPLEMENTED: Sorry, you can't copy a directory yet\n\n");
      return null;
    }

    if(!this.exists(dest) || !dir.isDirectory())
    {
      copyName          = dir.leafName;
      dir               = new FilePath(dir.path.replace(copyName,''));

      if(!this.exists(dir.path))
      {
        dump("io.js:copy:ERROR: Sorry, dest directory "+dir.path+" doesn't exist\n\n");
        return null;
      }

      if(!dir.isDirectory())
      {
        dump("io.js:copy:ERROR: Sorry, destination dir "+dir.path+" is not a valid dir path\n\n");
        return null;
      }

    }

    if(this.exists(this.append(dir.path, copyName)))
    {
      dump("io.js:copy:ERROR: Sorry destination file "+this.append(dir.path, copyName)+" already exists\n\n");
      return null;
    }

    fileInst.copyTo(dir, copyName);
    dump('io.js:copy successful!\n\n');
    this.close();
    retval=OK;

  }

  catch (e)
  { 
    dump("io.js:copy:ERROR: "+e+"\n\n"); 
    retval=null;
  }

  return retval;

},

/********************* COPY *****************************/


/********************* LEAF *****************************/

leaf  : function (path) {

  if(!path)
  {
    dump("io.js:leaf:ERROR: Missing path argument\n\n");
    return null;
  }

  if(!this.exists(path))
  {
    dump("io.js:leaf:ERROR: File doesn't exist\n\n");
    return null;
  }

  var retval;
  try
  {
    var fileInst = new FilePath(path);
    retval=fileInst.leafName;
  }

  catch(e)
  { 
    dump("io.js:leaf:ERROR: "+e+"\n\n"); 
  retval=null;
  }
  
  return retval;

},

/********************* LEAF *****************************/


/********************* READDIR **************************/

readDir : function (dirPath) {

  if(!dirPath)
  {
    dump("io.js:readDir:ERROR: Missing path argument\n\n");
    return null;
  }

  if(!this.exists(dirPath))
  {
    dump("io.js:readDir:ERROR: Sorry, directory "+dirPath+" doesn't exist\n\n");
    return null;
  }

  var retval;
  try
  {

    var file      = new FilePath(dirPath);

    if(!file.isDirectory())
    {
      dump("io.js:readDir:ERROR: Sorry, "+dirPath+" is not a directory\n\n");
      return null;
    }

    var files     = file.directoryEntries;
    var listings  = new Array();

    while(files.hasMoreElements())
    listings.push(files.getNext().QueryInterface(Components.interfaces.nsILocalFile).path);

    retval=listings;

  }

  catch(e)
  { 
    dump("io.js:readDir:ERROR: "+e+"\n\n"); 
    retval=null;
  }
  
  return retval;

},

/********************* READDIR **************************/


/********************* APPEND ***************************/

append : function (dirPath, fileName) {

    if(!dirPath || !fileName)
    {
      dump("io.js:append:ERROR: Missing path argument\n\n");
      return null;
    }

    if(!this.exists(dirPath))
    {
      dump("io.js:append:ERROR: File doesn't exist\n\n");
      return null;
    }

    var retval;
    try
    { 
      this.fileInst           = new FilePath(dirPath);
      var fileAppended        = this.fileInst.append(fileName);
      fileAppended            = this.fileInst.path;
      retval=fileAppended;
    }

    catch(e)
    { 
      dump("io.js:append:ERROR: "+e+"\n\n"); 
      retval=null;
    }

    return retval;
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
      return null;
    }
    
    if(!this.exists(path))
    {
      dump("io.js:permissions:ERROR: File doesn't exist\n\n");
      return null;
    }

    var retval;
    try
    { 
      var fileInst  = new FilePath(path); 
      retval=fileInst.permissions.toString(8);
    }

    catch(e)
    { 
      dump("io.js:permissions:ERROR: "+e+"\n"); 
    retval=null;
    }

    return retval;

},

/********************* PERMISSIONS **********************/


/********************* MODIFIED *************************/

dateModified  : function (path) {

    if(!path)
    {
      dump("io.js:dateModified:ERROR: Missing path argument\n\n");
      return null;
    }
    
    if(!this.exists(path))
    {
      dump("io.js:dateModified:ERROR: File doesn't exist\n\n");
      return null;
    }

    var retval;
    try
    { 
      var fileInst  = new FilePath(path); 
      var date = new Date(fileInst.lastModificationTime).toLocaleString();
      retval=date;
    }

    catch(e)
    { 
      dump("io.js:dateModified:ERROR: "+e+"\n"); 
      retval=null;
    }

    return retval;

},

/********************* MODIFIED *************************/


/********************* SIZE *****************************/

size  : function (path) {

    if(!path)
    {
      dump("io.js:size:ERROR: Missing path argument\n\n");
      return null;
    }
    
    if(!this.exists(path))
    {
      dump("io.js:size:ERROR: File doesn't exist\n\n");
      return null;
    }

    var retval;
    try
    { 
      var fileInst    = new FilePath(path); 
      retval          = fileInst.fileSize;
    }

    catch(e)
    { 
      dump("io.js:size:ERROR: "+e+"\n"); 
      retval=0;
    }

    return retval;

},

/********************* SIZE *****************************/


/********************* EXTENSION ************************/

extension  : function (path) {

    if(!path)
    {
      dump("io.js:extension:ERROR: Missing path argument\n\n");
      return null;
    }
    
    if(!this.exists(path))
    {
      dump("io.js:extension:ERROR: File doesn't exist\n\n");
      return null;
    }

    var retval;
    try
    { 
      var fileInst  = new FilePath(path); 
      var leafName  = fileInst.leafName; 
      var dotIndex  = leafName.lastIndexOf('.'); 
      retval=(dotIndex >= 0) ? leafName.substring(dotIndex+1) : ""; 
    }

    catch(e)
    { 
      dump("io.js:extension:ERROR: "+e+"\n");
      retval=null;
    }

    return retval;

},

/********************* EXTENSION ************************/


/********************* HELP *****************************/

help  : function() {

var help =

"\n\nFunction List:\n"                        +
"\n"                                          +
"*       1. open(path, mode);\n"              +
"*       2. exists(path);\n"                  +
"*       3. read();\n"                        +
"*       4. write(contents, permissions);\n"  +
"*       5. append(dirPath, fileName);\n"     +
"*       6. mkdir(path, permissions);\n"      +
"*       7. rmdir(path);\n"                   +
"*       8. rm(path);\n"                      +
"*       9. copy(source, dest);\n"            +
"*       10.leaf(path);\n"                    +
"*       11.readDir(dirPath);\n"              +
"*       12.permissions(path);\n"             +
"*       13.dateModified(path);\n"            +
"*       14.size(path);\n"                    +
"*       15.extension(path);\n"               +
"*       16.help();\n";

dump(help+'\n');

return "";

},

/********************* HELP *****************************/


/********************* CLOSE ****************************/

close : function() {

    /***************** Destroy Instances *********************/
    if(this.fileInst)       delete this.fileInst;
    if(this.fileChannel)    delete this.fileChannel;
    if(this.inputStream)    delete this.inputStream;
    if(this.mode)           this.mode=null;
    /***************** Destroy Instances *********************/
  
  return OK;

}

/********************* CLOSE ****************************/

};

/****************** End File Object Class *********************/


/************** Create an instance of file object *************/

//file          = new File(); // commenting out for now, the user can initialize the object . . . 

/************** Create an instance of file object *************/

