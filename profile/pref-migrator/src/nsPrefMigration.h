
#ifndef nsPrefMigration_h___
#define nsPrefMigration_h___

#include "nsPrefMigrationIIDs.h"
#include "nsIPrefMigration.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsFileSpec.h"
#include "nsIPref.h"
#include "nsIServiceManager.h" 

class nsPrefMigration: public nsIPrefMigration, public nsIShutdownListener
{
    public:
      NS_DEFINE_STATIC_CID_ACCESSOR(NS_PrefMigration_CID) 

	  nsPrefMigration();
      virtual ~nsPrefMigration();

      NS_DECL_ISUPPORTS

      NS_IMETHOD ProcessPrefs(char* , char*, nsresult *aResult );

	  /* nsIShutdownListener methods */
	  NS_IMETHOD OnShutdown(const nsCID& aClass, nsISupports *service);
	  
    private:

      nsresult CreateNewUser5Tree(char* oldProfilePath, 
                                  char* newProfilePath);

      nsresult ComputeMailPath(nsFileSpec oldPath, 
                               nsFileSpec *newPath);

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

private:
      nsIPref* m_prefs;
      nsresult getPrefService();
};

#endif

