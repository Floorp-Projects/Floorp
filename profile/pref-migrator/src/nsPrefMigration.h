
#ifndef nsPrefMigration_h___
#define nsPrefMigration_h___

#include "nsPrefMigrationIIDs.h"
#include "nsIPrefMigration.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsFileSpec.h"

class nsPrefMigration: public nsIPrefMigration
{
    public:
      static const nsIID& IID() { static nsIID iid = NS_PrefMigration_CID; return iid; }
      nsPrefMigration();
      ~nsPrefMigration();

      NS_DECL_ISUPPORTS

      NS_IMETHOD ProcessPrefs(char* , char*, nsresult *aResult );
    
    private:

      nsresult CreateNewUser5Tree(char* oldProfilePath, 
                                  char* newProfilePath);

      nsresult GetDirFromPref(char* newProfilePath, 
                              char* pref, 
                              char* newPath, 
                              char* oldPath);

      nsresult GetSizes(nsFileSpec inputPath,
                        PRBool readSubdirs,
                        PRUint32* sizeTotal);

      nsresult GetDriveName(nsFileSpec inputPath,
                            char* driveName);

      nsresult CheckForSpace(nsFileSpec newProfilePath, 
                             PRFloat64 requiredSpace);

		  nsresult DoTheCopy(nsFileSpec oldPath, 
                         nsFileSpec newPath,
                         PRBool readSubdirs); 

      nsresult DoSpecialUpdates(nsFileSpec profilePath);


};

#endif

