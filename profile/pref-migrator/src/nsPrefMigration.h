

#ifndef nsPrefMigration_h___

#define nsPrefMigration_h___



#include "nsPrefMigrationIIDs.h"

#include "nsIPrefMigration.h"



#include "nscore.h"

#include "nsIFactory.h"

#include "nsISupports.h"



class nsPrefMigration: public nsIPrefMigration

{

    public:

      static const nsIID& IID() { static nsIID iid = NS_PrefMigration_CID; return iid; }

      nsPrefMigration();

      ~nsPrefMigration();

        

      NS_DECL_ISUPPORTS



      //NS_IMETHOD Startup();

      //NS_IMETHOD Shutdown();

      NS_IMETHOD ProcessPrefs(char* , char* );

    

    private:



      nsresult CreateNewUser5Tree(char* oldProfilePath, 

                                  char* newProfilePath);



      nsresult GetDirFromPref(char* newProfilePath, 

                              char* pref, 

                              char* newPath, 

                              char* oldPath);

      

      nsresult Read4xFiles(char* ProfilePath, 

                           char* fileArray[], 

                           PRUint32* sizeTotal);



      nsresult CheckForSpace(char* newProfilePath, 

                             PRFloat64 requiredSpace);



		  nsresult DoTheCopy(char* oldPath, 

                         char* newPath, 

                         char* fileArray[]);



};



#endif

