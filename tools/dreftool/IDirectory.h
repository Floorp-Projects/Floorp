#ifndef _IDIRECTORY
#define _IDIRECTORY

#include "sharedtypes.h"
#include <typeinfo.h>

class nsCString;

class  IDirEntry {
public:
  virtual nsCString&   getName(void)=0;
  virtual IDirEntry*  getParent(void)=0;
  virtual int      getAttributes(void)=0;
  virtual int      getSize(void)=0;
  virtual void        getPath(nsCString& aString)=0;
};

class  IDirectory : public IDirEntry {
public:
  virtual IDirEntry*  getEntryAt(int anIndex)=0;
  virtual void        rescan(void)=0;

  static  IDirectory* openDirectory(const nsCString& aPath,const char* aFileSpec);
  static  void        closeDirectory(IDirectory* aDirectory);

};


#endif 


