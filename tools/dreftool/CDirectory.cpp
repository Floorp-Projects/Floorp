

#include "IDirectory.h"
#include "nsDeque.h"
#include "nsString.h"
#include "direct.h"
#include "io.h"

/***************************************************************************************

 ***************************************************************************************/
class  CDirEntry : public IDirEntry {
public:

  CDirEntry(const nsCString& aName,IDirectory* aParent,int aAttrs,int aSize) : IDirEntry(), mName(aName){
    mAttributes=aAttrs;
    mParent=aParent;
    mSize=aSize;
  }

  virtual CDirEntry::~CDirEntry() {
  }

  virtual nsCString&   getName(void){
    return mName;
  }

  virtual IDirEntry*  getParent(void){
    return mParent;
  }

  virtual int      getAttributes(void){
    return mAttributes;
  }

  virtual int      getSize(void){
    return mSize;
  }

  virtual void getPath(nsCString& aString){
    if(mParent){
      mParent->getPath(aString);
      aString+='/';
    }
    aString+=mName;
  }

protected:
    nsCString     mName;
    int           mAttributes;
    int           mSize;
    IDirectory*   mParent;
};

/***************************************************************************************

 ***************************************************************************************/
class  CDirectory : public IDirectory {
public:

  CDirectory(const nsCString& aName,const char* aFileSpec,IDirectory* aParent) : 
      IDirectory(),
      mChildren(0), 
      mName(aName),
      mFileSpec(aFileSpec)
  {
    mState=eUninitialized;
    mParent=aParent;
  }

  virtual CDirectory::~CDirectory() {
    IDirEntry* theEntry=0;
    while(theEntry=(IDirEntry*)mChildren.Pop()) {
      delete theEntry;
    }
  }

  /**
   *   
   *  @update  gess 6/1/99
   *  @param   
   *  @return  
   */
  virtual nsCString&   getName(void){
    return mName;
  }

  /**
   *   
   *  @update  gess 6/1/99
   *  @param   
   *  @return  
   */
  virtual IDirEntry*  getParent(void){
    return mParent;
  }

  /**
   *   
   *  @update  gess 6/1/99
   *  @param   
   *  @return  
   */
  virtual int      getAttributes(void){
    return mAttributes;
  }

  virtual int      getSize(void){
    if(eUninitialized==mState){
      rescan();
    }
    return mChildren.GetSize();
  }

  /**
   *   
   *  @update  gess 6/1/99
   *  @param   
   *  @return  
   */
  virtual void getPath(nsCString& aString){
    if(mParent){
      mParent->getPath(aString);
      aString+='/';
    }
    aString+=mName;
  }

  /**
   *   
   *  @update  gess 6/1/99
   *  @param   
   *  @return  
   */
  virtual IDirEntry*  getEntryAt(int anIndex){
    IDirEntry* result=0;

    if(eUninitialized==mState){
      rescan();
    }

    if(anIndex<mChildren.GetSize()){
      result=(IDirEntry*)mChildren.ObjectAt(anIndex);
    }
    return result;
  }

  /**
   *   
   *  @update  gess 6/1/99
   *  @param   
   *  @return  
   */
  virtual void rescan(void){
    mChildren.Erase();
    nsCAutoString thePath("");
    getPath(thePath);
    if(thePath.Length()){
      int result=_chdir(thePath.GetBuffer());
      if(!result){
        struct _finddata_t theFile;    
        long hFile;
        if(-1!=(hFile=_findfirst(mFileSpec.GetBuffer(),&theFile))) {
          do {
            if('.'!=theFile.name[0]){
              nsCAutoString theName(theFile.name);
              IDirEntry* theEntry=0;
              if(theFile.attrib & _A_SUBDIR){
                theEntry=new CDirectory(theName,mFileSpec.GetBuffer(),this);
              }
              else {
                theEntry=new CDirEntry(theName,this,0,theFile.size);
              }
              mChildren.Push(theEntry);
            }
          } while(0==_findnext(hFile,&theFile));
          _findclose(hFile);   
        }
        mState=eOk;
      }
    }
  }

protected:

  enum  eDirState {eUninitialized,eOk,eNotADir};
  nsDeque      mChildren;
  eDirState   mState;
  IDirectory* mParent;
  int      mSize;
  int      mAttributes;
  nsCAutoString mName;
  nsCAutoString mFileSpec;
};

 
/**
 *  Ask the system to open a given directory, with given path
 *  @update  gess 6/1/99
 *  @param   aPath contains the path to directory you want opened
 *  @return  ptr to directory  (if legal) or 0 if invalid path
 */
IDirectory* IDirectory::openDirectory(const nsCString& aPath,const char* aFileSpec){
  IDirectory* result=0;

  nsCAutoString theCopy(aPath);
  theCopy.ReplaceChar('\\','/'); //replace all forward slashes with backslashes 

  if(0<theCopy.Length()){
    //by now, you have nothing but the actual directory name...
    //so let's go ask the OS to open the directory...
    int rv=_chdir(theCopy);
    if(!rv) {
      //if you're here, we successfully opened the directory...
      result=new CDirectory(theCopy,aFileSpec,0);
    }
  }
  return result;
}

void IDirectory::closeDirectory(IDirectory* aDirectory){
  if(aDirectory) {
    CDirectory* theDir=(CDirectory*)aDirectory;
    delete theDir;
  }
}
