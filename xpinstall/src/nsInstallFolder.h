#ifndef __NS_INSTALLFOLDER_H__
#define __NS_INSTALLFOLDER_H__

#include "nscore.h"
#include "prtypes.h"

#include "nsString.h"
#include "nsFileSpec.h"
#include "nsSpecialSystemDirectory.h"

class nsInstallFolder
{
    public:
        
       nsInstallFolder(const nsString& aFolderID);
       nsInstallFolder(const nsString& aFolderID, const nsString& aRelativePath);
       ~nsInstallFolder();

       void GetDirectoryPath(nsString& aDirectoryPath);
       
    private:
        
        nsFileSpec*  mUrlPath;

		void         SetDirectoryPath(const nsString& aFolderID, const nsString& aRelativePath);
        void         PickDefaultDirectory();
        PRInt32      MapNameToEnum(const nsString&  name);

        void         operator = (enum nsSpecialSystemDirectory::SystemDirectories aSystemSystemDirectory);
};


#endif