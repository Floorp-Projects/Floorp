/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


#include "nsICmdLineService.h"
#include "nsVoidArray.h"
#include "nsRepository.h"
#include "nsString.h"
#include "plstr.h"


/* Define Class IDs */
static NS_DEFINE_IID(kCmdLineServiceCID,         NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_IID(kICommandLineIID,	      NS_ICOMMANDLINE_SERVICE_IID);

/* Define Interface IDs */

static NS_DEFINE_IID(kIFactoryIID,         NS_IFACTORY_IID);
static NS_DEFINE_IID(kICommandLineServiceIID, NS_ICOMMANDLINE_SERVICE_IID);


class nsCmdLineService : public nsICmdLineService
{
public:
  nsCmdLineService(void);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Initialize(int argc, char** argv);
  NS_IMETHOD GetCmdLineValue(char* arg, char **value);
  NS_IMETHOD GetURLToLoad(char ** aResult);
  NS_IMETHOD GetProgramName(char ** aResult);
//  nsresult PrintCmdArgs();

protected:
  virtual ~nsCmdLineService();

  nsVoidArray    mArgList;      // The arguments
  nsVoidArray    mArgValueList; // The argument values
  PRInt32       mArgCount; // This is not argc. This is # of argument pairs 
                            // in the arglist and argvaluelist arrays. This
                            // normally is argc/2.
};


nsCmdLineService::nsCmdLineService()
{
  mArgCount = 0;
}


NS_IMETHODIMP
nsCmdLineService::Initialize(int argc, char ** argv)
{
  NS_INIT_REFCNT();
  PRInt32   i=0;
  nsresult  rv = nsnull;

  //Insert the program name 
  mArgList.AppendElement((void *)PL_strdup("-progname"));
  mArgValueList.AppendElement((void *)PL_strdup(argv[0]));
  mArgCount++;
  i++;

  for(i=1; i<argc; i++) {

    if (argv[i][0] == '-') {
     /* An option that starts with -. May or many not
	    * have a value after it. 
	    */
	   mArgList.AppendElement((void *)PL_strdup(argv[i]));
	   //Increment the index to look ahead at the next option.
     i++;


     //Look ahead if this option has a value like -w 60

	   if (i == argc) {
	     /* All args have been parsed. Append a PR_TRUE for the
	      * previous option in the mArgValueList
	      */
	     mArgValueList.AppendElement((void *)PL_strdup("1"));
	     mArgCount++;
	     break;
	   }
     if (argv[i][0] == '-') {
        /* An other option. The previous one didn't have a value.
         * So, store the previous one's value as PR_TRUE in the
	       * mArgValue array and retract the index so that this option 
	       * will get stored in the next iteration
	       */
        mArgValueList.AppendElement((void *)PL_strdup("1"));
   	    mArgCount++;
        i--;
        continue;
	    }
      else {
        /* The next argument does not start with '-'. This 
	       * could be value to the previous option or a url to
         * load if this is the last argument and has a ':/' in it.
	       */
	      if ((i == (argc-1)) && (PL_strstr(argv[i], ":/"))) {
	         /* This is the last argument and a URL 
            * Append a PR_TRUE for the previous option in the value array
            */
           mArgValueList.AppendElement((void *)PL_strdup("1"));
	         mArgCount++;

 		       // Append the url to the arrays
           mArgList.AppendElement((void *)PL_strdup("-url"));
		       mArgValueList.AppendElement((void *) PL_strdup(argv[i]));
	 	       mArgCount++;
           continue;
        }
	      else {
	         /* This is a value to the previous option.
	          * Store it in the mArgValue array 
	          */
           mArgValueList.AppendElement((void *)PL_strdup(argv[i]));
	         mArgCount++;
	      }
	   }
  }
  else {
       if ((i == (argc-1)) && (PL_strstr(argv[i], ":/"))) {
	        /* This must be the  URL at the end 
	         * Append the url to the arrays
           */
           mArgList.AppendElement((void *)PL_strdup("-url"));
	         mArgValueList.AppendElement((void *) PL_strdup(argv[i]));
	         mArgCount++;
	     }
	     else {
	       /* A bunch of unrecognized arguments */
	       rv = NS_ERROR_INVALID_ARG;
	     }
  }
	
 }  // for

  for (i=0; i<mArgCount; i++)
  {
       printf("Argument: %s, ****** Value: %s\n", mArgList.ElementAt(i), mArgValueList.ElementAt(i));      
  }
     
   return rv;
	
}

NS_IMETHODIMP
nsCmdLineService::GetURLToLoad(char ** aResult)
{

   return GetCmdLineValue("-url", aResult);
}

NS_IMETHODIMP
nsCmdLineService::GetProgramName(char ** aResult)
{
  nsresult  rv=nsnull;

  *aResult = (char *)mArgValueList.ElementAt(0);

  return rv;

}


NS_IMETHODIMP
nsCmdLineService::GetCmdLineValue(char * arg, char ** aResult)
{
   nsresult  rv = NS_OK;
   PRInt32   index=0;
   
   if (nsnull == arg || nsnull == aResult ) {
	    return NS_ERROR_NULL_POINTER;
   }

   for (int i = 0; i<mArgCount; i++)
   {
     if (!PL_strcmp(arg, (char *) mArgList.ElementAt(i))) {
       *aResult = (char *)mArgValueList.ElementAt(i);
        return NS_OK;
     }
   }

   *aResult = nsnull;
   return rv;
	
}


nsCmdLineService::~nsCmdLineService()
{
}



#if 0
NS_IMETHODIMP
nsCmdLineService::PrintCmdArgs()
{

   if (mArgCount == 0) {
     printf("No command line options provided\n");
     return;
   }
   
   for (int i=0; i<mArgCount; i++)
   {
       printf("Argument: %s, ****** Value: %s\n", mArgList.ElementAt(i), mArgValueList.ElementAt(i));      

   }

  return NS_OK;

}
#endif

/*
 * Implement the nsISupports methods...
 */
NS_IMPL_ISUPPORTS(nsCmdLineService, kICommandLineServiceIID);




NS_EXPORT nsresult NS_NewCmdLineService(nsICmdLineService** aResult)
{
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = new nsCmdLineService();
  if (nsnull == *aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);
  return NS_OK;
}



//----------------------------------------------------------------------

// Factory code for creating nsAppShellService's

class nsCmdLineServiceFactory : public nsIFactory
{
public:
  nsCmdLineServiceFactory();
  NS_DECL_ISUPPORTS

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);
  
  NS_IMETHOD LockFactory(PRBool aLock);  
protected:
  virtual ~nsCmdLineServiceFactory();
};


nsCmdLineServiceFactory::nsCmdLineServiceFactory()
{
  NS_INIT_REFCNT();
}

nsresult
nsCmdLineServiceFactory::LockFactory(PRBool lock)
{

  return NS_OK;
}

nsCmdLineServiceFactory::~nsCmdLineServiceFactory()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMPL_ISUPPORTS(nsCmdLineServiceFactory, kIFactoryIID);


nsresult
nsCmdLineServiceFactory::CreateInstance(nsISupports *aOuter,
                                  const nsIID &aIID,
                                  void **aResult)
{
  nsresult rv;
  nsCmdLineService* inst;

  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;
  if (nsnull != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    goto done;
  }

  NS_NEWXPCOM(inst, nsCmdLineService);
  if (inst == NULL) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

done:
  return rv;
}


extern "C" NS_APPSHELL nsresult
NS_NewCmdLineServiceFactory(nsIFactory** aFactory)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsCmdLineServiceFactory();
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aFactory = inst;
  return rv;
}
