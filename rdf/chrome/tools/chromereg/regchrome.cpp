/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 * 
 */

#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIChromeRegistry.h"

#include "rdf.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsIRDFContainer.h"

#include "nsIFile.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsNetUtil.h"

#include "nsString.h"

#include "plgetopt.h"

#include <stdlib.h>             // for exit()

static const char uri_prefix[] = "http://www.mozilla.org/rdf/chrome#";
static const char urn_prefix[] = "urn:mozilla:";

// this is nasty - this is in nsChromeRegistry.h, but to include that,
// we double our REQUIRES dependencies. We need the CID rather than
// the ContractID because we need to guarantee that we're getting the
// RDF-based chrome registry in order to build up chrome.rdf

// {D8C7D8A2-E84C-11d2-BF87-00105A1B0627}
#define NS_CHROMEREGISTRY_CID \
{ 0xd8c7d8a2, 0xe84c, 0x11d2, { 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } }

static NS_DEFINE_CID(kChromeRegistryCID, NS_CHROMEREGISTRY_CID);

nsresult
WritePropertiesTo(nsIRDFDataSource*, const char* aProviderType, FILE* out);

nsresult
WriteAttributes(nsIRDFDataSource*, const char* aProviderType, const char* aProvider,
                nsIRDFResource* aResource,
                FILE* out);

void
TranslateResourceValue(const char* aProviderType,
                       const char* aProvider,
                       const char* aArc,
                       const char* aResourceValue,
                       nsACString& aResult);

nsresult WriteProperties(const char* properties_file);

void print_help();

int main(int argc, char **argv)
{
    const char* properties_filename = nsnull;

    PLOptState* optstate = PL_CreateOptState(argc, argv, "p:");
    
    while (PL_GetNextOpt(optstate) == PL_OPT_OK) {
        switch (optstate->option) {
        case 'p':               // output properties file
            properties_filename = optstate->value;
            break;
        case '?':
            print_help();
            exit(1);
            break;
        }
    }
    PL_DestroyOptState(optstate);

    NS_InitXPCOM2(nsnull, nsnull, nsnull);

    nsCOMPtr <nsIChromeRegistry> chromeReg = 
        do_GetService(kChromeRegistryCID);
    if (!chromeReg) {
        NS_WARNING("chrome check couldn't get the chrome registry");
        return NS_ERROR_FAILURE;
    }
    chromeReg->CheckForNewChrome();

    // ok, now load the rdf that just got written
    if (properties_filename)
        WriteProperties(properties_filename);

    // release the chrome registry before we shutdown XPCOM
    chromeReg = 0;
    NS_ShutdownXPCOM(nsnull);
    return 0;
}


nsresult
WritePropertiesTo(nsIRDFDataSource* aDataSource,
                  const char* aProviderType, FILE* out)
{

    // attempt to mimic nsXMLRDFDataSource::Flush?
    nsCOMPtr<nsIRDFService> rdfService =
        do_GetService("@mozilla.org/rdf/rdf-service;1");
    
    nsCOMPtr<nsIRDFResource> providerRoot;
    nsCAutoString urn(NS_LITERAL_CSTRING(urn_prefix) +
                      nsDependentCString(aProviderType) + NS_LITERAL_CSTRING(":root"));

    rdfService->GetResource(urn,
                            getter_AddRefs(providerRoot));

    nsCOMPtr<nsIRDFContainer> providerContainer =
        do_CreateInstance("@mozilla.org/rdf/container;1");

    providerContainer->Init(aDataSource, providerRoot);

    nsCOMPtr<nsISimpleEnumerator> providers;
    providerContainer->GetElements(getter_AddRefs(providers));

    PRBool hasMore;
    providers->HasMoreElements(&hasMore);
    for (; hasMore; providers->HasMoreElements(&hasMore)) {
        nsCOMPtr<nsISupports> supports;
        providers->GetNext(getter_AddRefs(supports));

        nsCOMPtr<nsIRDFResource> kid = do_QueryInterface(supports);

        const char* providerUrn;
        kid->GetValueConst(&providerUrn);
        
        // remove the prefix, the provider type, and the trailing ':'
        // (the compiler will optimize out the -1 + 1
        providerUrn += (sizeof(urn_prefix)-1) + strlen(aProviderType) + 1;

        WriteAttributes(aDataSource, aProviderType, providerUrn, kid, out);
    }

    return NS_OK;
}


nsresult
WriteAttributes(nsIRDFDataSource* aDataSource,
                const char* aProviderType,
                const char* aProvider,
                nsIRDFResource* aResource, FILE* out)
{
    nsresult rv;
    
    nsCOMPtr<nsISimpleEnumerator> arcs;
    rv = aDataSource->ArcLabelsOut(aResource, getter_AddRefs(arcs));
    if (NS_FAILED(rv))
        return rv;

    PRBool hasMore;
    rv = arcs->HasMoreElements(&hasMore);
    if (NS_FAILED(rv))
        return rv;


    for (; hasMore; arcs->HasMoreElements(&hasMore)) {
        nsCOMPtr<nsISupports> supports;
        arcs->GetNext(getter_AddRefs(supports));

        nsCOMPtr<nsIRDFResource> arc = do_QueryInterface(supports);

        const char* arcValue;
        arc->GetValueConst(&arcValue);
        arcValue += (sizeof(uri_prefix)-1); // skip past prefix
         
        // get the literal value on the other end
        nsCOMPtr<nsIRDFNode> valueNode;
        aDataSource->GetTarget(aResource, arc, PR_TRUE, 
                               getter_AddRefs(valueNode));
         
        nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(valueNode);
        if (literal) {
            const PRUnichar* literalValue;
            literal->GetValueConst(&literalValue);
            fprintf(out, "%s.%s.%s=%s\n", aProviderType, aProvider, arcValue, NS_ConvertUCS2toUTF8(literalValue).get());
        }

        nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(valueNode);
        if (resource) {
            const char* resourceValue;
            resource->GetValueConst(&resourceValue);
            
            nsCAutoString translatedValue;
            TranslateResourceValue(aProviderType, aProvider,
                                   arcValue, resourceValue, translatedValue);
            fprintf(out, "%s.%s.%s=%s\n", aProviderType, aProvider, arcValue, translatedValue.get());
        }
    }

    return NS_OK;
}
    
nsresult WriteProperties(const char* properties_file)
{
    FILE* props;
    printf("writing to %s\n", properties_file);
    if (!(props = fopen(properties_file, "w"))) {
        printf("Could not write to %s\n", properties_file);
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIFile> chromeFile;
    nsresult rv = NS_GetSpecialDirectory(NS_APP_CHROME_DIR, getter_AddRefs(chromeFile));
    if(NS_FAILED(rv)) {
        fclose(props);
        return rv;
    }

    chromeFile->AppendNative(NS_LITERAL_CSTRING("chrome.rdf"));
    
    nsCAutoString pathURL;
    NS_GetURLSpecFromFile(chromeFile, pathURL);

    nsCOMPtr<nsIRDFService> rdf = 
        do_GetService("@mozilla.org/rdf/rdf-service;1");

    nsCOMPtr<nsIRDFDataSource> chromeDS;
    rdf->GetDataSource(pathURL.get(), getter_AddRefs(chromeDS));

    WritePropertiesTo(chromeDS, "package", props);
    WritePropertiesTo(chromeDS, "skin", props);
    WritePropertiesTo(chromeDS, "locale", props);

    fclose(props);
    return NS_OK;
}

void print_help() {

    printf("Usage: regchrome [-p file.properties]\n"
           "Registers chrome by scanning installed-chrome.txt\n"
           "\n"
           "Options: \n"
           "  -p file.properties    Output a flat-file version of the registry to\n"
           "                        the specified file\n");

}

void
TranslateResourceValue(const char* aProviderType,
                       const char* aProvider,
                       const char* aArc,
                       const char* aResourceValue,
                       nsACString& aResult)
{
    PRUint32 chopStart=0, chopEnd=0; // bytes to chop off the front/back of aResourceValue

    static const char localeUrn[] = "urn:mozilla:locale:";
    if ((strcmp(aArc, "selectedLocale") == 0) &&
        (strncmp(aResourceValue, localeUrn, sizeof(localeUrn)-1) == 0)) {
        
        chopStart = sizeof(localeUrn) - 1;
        chopEnd = strlen(aProvider) + 1;
    }

    static const char skinUrn[] = "urn:mozilla:skin:";
    if ((strcmp(aArc, "selectedSkin") == 0) &&
        (strncmp(aResourceValue, skinUrn, sizeof(skinUrn)-1) == 0)) {

        chopStart = sizeof(skinUrn) - 1;
        chopEnd = strlen(aProvider) + 1;
    }

    // strip off 'urn:mozilla:<foo>:' and ':<provider>'
    aResult = (aResourceValue + chopStart);
    aResult.Truncate(aResult.Length() - chopEnd);
        
}
