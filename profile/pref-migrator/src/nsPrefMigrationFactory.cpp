/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsPrefMigration.h"
#include "nsPrefMigrationFactory.h"
                          
static NS_IMETHODIMP  
CreateNewPrefMigration(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{                                                  
    if (!aResult) {                                                  
        return NS_ERROR_NULL_POINTER;                             
    }                                                                
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }                                                                
    nsPrefMigration* inst = nsPrefMigration::GetInstance();   
    if (inst == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
                               
    nsresult rv = inst->QueryInterface(aIID, aResult);                        
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    return rv;                                                       
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrefConverter)

static nsModuleComponentInfo components[] = 
{
    { "Profile Migration", 
      NS_PREFMIGRATION_CID,
      NS_PROFILEMIGRATION_CONTRACTID, 
      CreateNewPrefMigration },
    { "Pref Conversion",
      NS_PREFCONVERTER_CID,
      NS_PREFCONVERTER_CONTRACTID, nsPrefConverterConstructor}
};

NS_IMPL_NSGETMODULE("nsPrefMigrationModule", components);
