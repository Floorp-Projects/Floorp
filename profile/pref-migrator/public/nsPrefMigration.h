

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



        NS_IMETHOD Startup();

        NS_IMETHOD Shutdown();

        NS_IMETHOD ProcessPrefs(char* , char* );

    

    private:



        nsresult CreateNewUser5Tree(char*, char *);

        nsresult Copy4xFiles(char*, char*);

        nsresult CheckForSpace(char*, PRUint32);



};



#endif

