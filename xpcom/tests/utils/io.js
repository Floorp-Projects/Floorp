/*************

The contents of this file are subject to the Mozilla Public
License Version 1.1 (the "License"); you may not use this file
except in compliance with the License. You may obtain a copy of
the License at http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS
IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
implied. See the License for the specific language governing
rights and limitations under the License.

The Original Code is Alphanumerica code.
The Initial Developer of the Original Code is Alphanumerica.

Portions created by Alphanumerica are
Copyright (C) 2000 Alphanumerica.  All
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
*       1. open(path, mode);
*       2. exists(path);
*       3. read(path);
*       4. write(contents, permissions);
*       5. append(dirPath, fileName);
*       6. mkdir(path, permissions); //permissions optional
*       7. rmdir(path);
*       8. rm(path);
*       9. copy(source, dest);
*       10.leaf(path);
*       11.readDir(dirPath);
*
*       Instructions:
*
*       First include this js file 
*
*       file = new File();
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

////////////////// Globals //////////////////////

var FilePath    = new Components.Constructor( "component://mozilla/file/local", "nsILocalFile", "initWithPath");
var FileChannel = new Components.Constructor( "component://netscape/network/local-file-channel", "nsIFileChannel" );
var InputStream = new Components.Constructor( "component://netscape/scriptableinputstream", "nsIScriptableInputStream" );

////////////////// Globals //////////////////////


////////////////// File Object Class /////////////////////

function File(path, mode){
    
    if(path)
    this.open(path,mode);

} // constructor

File.prototype  = {

mode            : null,

fileInst        : null,

fileChannel     : null,

inputStream     : null,

/********************* EXISTS ***************************/

exists : function (path) {

    if(!path){
        dump("Missing path argument . . . \n\n");
        return;
    }

    //dump("\n\n**** Checking to see if the file exists ****\n\n");

    try{ 

        var file                = new FilePath(path);
        var fileExists          = file.exists();

    }

    catch(error) { dump("**** ERROR:"+error+"\n\n"); }

    return fileExists;
},

/********************* EXISTS ***************************/


/********************* OPEN *****************************/


open : function(path, setMode){

    if(!setMode)
        setMode="r";

switch(setMode){


case "w":

    //dump("open file for writing\n\n");

    if(!path || !setMode){
        dump("Missing path or mode in arguments . . .\n\n");
        return;
    }

    try{

        //dump("Closing any existing handles . . . \n\n");
        this.close();

        this.fileInst           = new FilePath( path );

        var fileExists          = this.exists( path );

    if(fileExists){

        //dump("deleting old file and creating a new one . . . \n\n"); 
        this.fileInst["delete"](false);
        fileExists=false;

    }

    if (!fileExists){
        //dump("\n\nCreating new file "+path+"\n\n");
        this.fileInst.create(0, 0644);
        fileExists=true;
    }

    this.mode=8;
    return;

    }

    catch (error){ dump("**** ERROR:"+error+"\n\n"); }
    break;



case "a":

    //dump("open file for appending\n\n");

    if(!path || !setMode){
        dump("Missing path or mode in arguments . . .\n\n");
        return;
    }

    try{

        //dump("Closing any existing handles . . . \n\n");
        this.close();

        this.fileInst           = new FilePath(path);

        var fileExists          = this.exists(path);

    if ( !fileExists ){
        //dump("\n\nCreating new file "+path+"\n\n");
        this.fileInst.create(0, 0644);
        fileExists=true;
    }

   this.mode=16;
   return;

    }


    catch (error){ dump("**** ERROR:"+error+"\n\n"); }
    break;



case "r":

    //dump("open file for reading\n\n");

    if(!path){
        dump("Missing path argument . . . \n\n");
        return;
    }

    //dump("Closing any existing handles . . . \n\n");
    this.close();

    try {

        this.fileInst                = new FilePath(path);
        this.fileChannel             = new FileChannel();
        this.inputStream             = new InputStream();    

    }
        
    catch (error){ dump("**** ERROR:"+error+"\n\n"); }
    break;


    default:
    dump("\n\n**** WARNING: \""+setMode+"\" is an Invalid file mode\n\n");

    }

},

/********************* OPEN *****************************/


/********************* WRITE ****************************/

write : function(buffer, perms){

    if(!this.fileInst){
        dump("Please open a file handle first . . .\n");
        return;
    }
    
    if(!buffer)
        buffer                  = " ";
        var buffSize            = buffer.length;

    try{

    if(!this.fileChannel)
        this.fileChannel        = new FileChannel();

    if(perms){

        var checkPerms  = this.validatePermissions( perms );

    if(!checkPerms){
        dump("**** Sorry invalid permissions set\n\n");
        return;
    }               

     }

    if(!perms)
        perms=0644;

    //dump("trying to initialize filechannel\n\n");

    this.fileChannel.init(this.fileInst, this.mode, perms)
    this.fileInst.permissions=perms;

    //dump("initialized filechannel and set desired permissions\n\n");
    

    var fileSize            = parseInt( this.fileInst.fileSize );
    //dump("got initial file size = "+fileSize+"\n\n");

    var outStream           = this.fileChannel.openOutputStream();
    //dump("got outStream\n\n");

        
    if( outStream.Write(buffer, buffSize) )
        //dump("Write to file successful . . . \n\n");

    outStream.Flush();

    }

    catch (error){ dump("**** ERROR:"+error+"\n\n"); }

},

/********************* WRITE ****************************/


/********************* READ *****************************/

read : function() {

    try {

    if(!this.fileInst || !this.fileChannel || !this.inputStream){
        dump("Please open a valid file handle for reading . . .\n");
        return null;
    }
        

    var fileExists          = this.exists(this.fileInst.path);

    if(!fileExists) {
        dump("WARNING: \""+this.fileInst.path+"\" does not exist...\n");
        return;
    }

    var offset      = this.fileInst.fileSize;
    var perm        = this.fileInst.permissions;     

    //dump("PATH i am trying to read = " + this.fileInst.path + "\n");

    this.fileChannel.init(this.fileInst, 1, perm);

    var inStream = this.fileChannel.openInputStream();

    this.inputStream.init(inStream);

    var fileContents        = this.inputStream.read(this.fileInst.fileSize);

    //dump(fileContents);

    inStream.Close();
        
    return fileContents;

    }

    catch (error){ dump("**** ERROR:"+error+"\n\n"); }

},

/********************* READ *****************************/



/********************* MKDIR ****************************/

mkdir : function(path, permissions){

    //dump("Closing any existing handles . . . \n\n");
    this.close();

    if(!path){
        dump("Missing path in argument . . .\n\n");
        return;
    }

    var fileExists  = this.exists(path);

    if(permissions){
        var checkPerms  = this.validatePermissions(permissions);

    if(!checkPerms){
        dump("**** Sorry invalid permissions set\n\n");
        return;
    }               

    var baseTen     = permissions.toString(10);

    if(baseTen.substring(0,1) != 0)
        permissions = 0+baseTen;

    }


    try{ 

    this.fileInst  = new FilePath(path);

    if (!fileExists){

    if(!permissions)
        permissions     = 0755;
        //dump("\n\nCreating new Directory \""+path+"\"\n\n");
        this.fileInst.create( 1, parseInt(permissions) );  //*NOTE* permission 0777 doesn't seem to work here -pete

    }

    else{ return; /**dump("Sorry Directory "+path+" already exists\n\n");***/ }
    
    }

    catch (error){ dump("**** ERROR:"+error+"\n\n"); }

    this.close();

},

/********************* MKDIR ****************************/

/********************* RM *******************************/

rm : function (path) {

    this.close();

    if(!path){
        dump("Missing path in argument . . .\n\n");
        return;
    }

    if(!this.exists(path))
    return;

    try{ 
    this.fileInst            = new FilePath(path);
    if(this.fileInst.isDirectory()){
    dump("Sorry file is a directory. Try rmdir() instead . . .\n");
    return;
    }

    this.fileInst['delete'](false);

    }

    catch (error){ dump("**** ERROR:"+error+"\n\n"); }
    this.close();

},

/********************* RM *******************************/

/********************* RMDIR ****************************/

rmdir : function (path) {

    this.close();

    if(!path){
        dump("Missing path in argument . . .\n\n");
        return;
    }

    if(!this.exists(path))
    return;

    try{ 
    this.fileInst            = new FilePath(path);
    this.fileInst['delete'](true);
    }

    catch (error){ dump("**** ERROR:"+error+"\n\n"); }
    this.close();

},

/********************* RMDIR ****************************/

/********************* COPY *****************************/

copy  : function (source, dest) {

  if(!source || !dest){
  dump('not enough args . . . \n\n');
  return;
  }

  if(!this.exists(source)){
  dump("Sorry, source file "+source+" doesn't exist\n\n");
  return;
  }

  var fileInst      = new FilePath(source);
  var dir           = new FilePath(dest);

  var copyName      = fileInst.leafName;

  if(fileInst.isDirectory()){
  dump("Sorry, you can't copy a directory yet\n\n");
  return
  }

  if(!this.exists(dest) || !dir.isDirectory()){
  copyName          = dir.leafName;
  dump(dir.path.replace(copyName,'')+'\n\n');
  var dir           = new FilePath(dir.path.replace(copyName,''));
    if(!this.exists(dir.path)){
    dump("Sorry, dest directory "+dir.path+" doesn't exist\n\n");
    return;
    }
      if(!dir.isDirectory()){
      dump("Sorry, destination dir "+dir.path+" is not a valid dir path\n\n");
      return;
      }
  }

  if(this.exists(this.append(dir.path, copyName))){
  dump('Sorry destination file '+this.append(dir.path, copyName)+' already exists . . .\n\n');
  return;
  }

  dump("copyName = "+copyName+"\n\n");
  dump("dir is directory = "+dir.isDirectory()+"\n\n");

  try{ 
  fileInst.copyTo(dir, copyName);
  dump('copy successful!\n\n');
  }

  catch (error){ dump("**** ERROR:"+error+"\n\n"); }
  this.close();

},

/********************* COPY *****************************/

/********************* LEAF *****************************/

leaf  : function (path) {

  if(!path){
  dump('Please enter a file path as arg\n');
  return null;
  }

  fileInst = new FilePath(path);

  return fileInst.leafName;

},

/********************* LEAF *****************************/

/********************* READDIR **************************/

readDir : function (dirPath) {

  if(!dirPath){
  dump('Please enter a dir path as arg\n');
  return null;
  }

  if(!this.exists(dirPath)){
  dump("Sorry, directory "+dirPath+" doesn't exist\n\n");
  return;
  }

  var file      = new FilePath(dirPath);

  if(!file.isDirectory()){
  dump("Sorry, "+dirPath+" is not a directory\n\n");
  return;
  }

  var files     = file.directoryEntries;
  var listings  = new Array();

  i=0;
  while(files.hasMoreElements()){
  listings[i]   = files.getNext().QueryInterface(Components.interfaces.nsILocalFile).path;
  i++;
  }

  return eval(listings.toSource());

},

/********************* READDIR **************************/

/********************* APPEND ***************************/

append : function (dirPath, fileName) {

    if(!dirPath || !fileName){
        dump("Missing path argument . . . \n\n");
        return;
    }

    //dump("\n\n**** Checking to see if the directory exists "+fileName+"****\n\n");
    this.exists(dirPath);

    try{ 

    //if(!this.fileInst)
        this.fileInst           = new FilePath(dirPath);

        fileAppended            = this.fileInst.append(fileName);
        fileAppended            = this.fileInst.path;

    }

    catch(error) { dump("**** ERROR:"+error+"\n\n"); }

    //dump("File \""+dirPath+"\" appended = "+fileAppended+"\n\n");

    return fileAppended;
},

/********************* APPEND ***************************/

/********************* VALIDATE PERMISSIONS *************/

validatePermissions : function(num){

    //dump("Checking for valid permission\n\n");

    if ( parseInt(num.toString(10).length) < 3 ) 
        return false;

    return true;

},

/********************* VALIDATE PERMISSIONS *************/

/********************* CLOSE ****************************/

close : function(){

    /***************** Destroy Instances *********************/

    //dump("**** destroying any object instances ****\n\n");

    if(this.fileInst)       delete this.fileInst;
    if(this.fileChannel)    delete this.fileChannel;
    if(this.inputStream)    delete this.inputStream;
    if(this.mode)           this.mode=null;

    /***************** Destroy Instances *********************/


}

/********************* CLOSE ****************************/

};

////////////////// End File Object Class /////////////////////


/************** Create an instance of file object *************/

//file          = new File(); // commenting out for now, the user can initialize the object . . . 

/************** Create an instance of file object *************/



