/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsJVMPluginTagInfo.h"
#include "nsIPluginTagInfo2.h"
#include "plstr.h"
#include "nsCRT.h"      // mixing metaphors with plstr.h!
#ifdef XP_UNIX
#undef Bool
#endif
#include "xp_mem.h"

static NS_DEFINE_IID(kIJVMPluginTagInfoIID, NS_IJVMPLUGINTAGINFO_IID);
static NS_DEFINE_IID(kIPluginTagInfo2IID, NS_IPLUGINTAGINFO2_IID);

////////////////////////////////////////////////////////////////////////////////
// nsJVMPluginTagInfo
////////////////////////////////////////////////////////////////////////////////

nsJVMPluginTagInfo::nsJVMPluginTagInfo(nsISupports* outer, nsIPluginTagInfo2* info)
    : fPluginTagInfo(info), fSimulatedCodebase(NULL), fSimulatedCode(NULL)
{
    NS_INIT_AGGREGATED(outer);
}

nsJVMPluginTagInfo::~nsJVMPluginTagInfo(void)
{
    if (fSimulatedCodebase)
        PL_strfree(fSimulatedCodebase);

    if (fSimulatedCode)
        PL_strfree(fSimulatedCode);
}

NS_IMPL_AGGREGATED(nsJVMPluginTagInfo);

NS_METHOD
nsJVMPluginTagInfo::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
	 if(!aInstancePtr)
	     return NS_ERROR_INVALID_POINTER;

    if (aIID.Equals(kIJVMPluginTagInfoIID))
	     *aInstancePtr = NS_STATIC_CAST(nsIJVMPluginTagInfo*, this);
	 else if (aIID.Equals(NS_GET_IID(nsISupports)))
	     *aInstancePtr = GetInner();
	 else	{
	     *aInstancePtr = nsnull;
		  return NS_NOINTERFACE;
	 }
	 
	 NS_ADDREF((nsISupports*)aInstancePtr);
	 return NS_OK;
}


static void
oji_StandardizeCodeAttribute(char* buf)
{
    // strip off the ".class" suffix
    char* cp;

    if ((cp = PL_strrstr(buf, ".class")) != NULL)
        *cp = '\0';

    // Convert '/' to '.'
    cp = buf;
    while ((*cp) != '\0') {
        if ((*cp) == '/')
            (*cp) = '.';

        ++cp;
    }
}

NS_METHOD
nsJVMPluginTagInfo::GetCode(const char* *result)
{
    if (fSimulatedCode) {
        *result = fSimulatedCode;
        return NS_OK;
    }

    const char* code;
    nsresult err = fPluginTagInfo->GetAttribute("code", &code);
    if (err == NS_OK && code) {
        fSimulatedCode = PL_strdup(code);
        oji_StandardizeCodeAttribute(fSimulatedCode);
        *result = fSimulatedCode;
        return NS_OK;
    }

    const char* classid;
    err = fPluginTagInfo->GetAttribute("classid", &classid);
    if (err == NS_OK && classid && PL_strncasecmp(classid, "java:", 5) == 0) {
        fSimulatedCode = PL_strdup(classid + 5); // skip "java:"
        oji_StandardizeCodeAttribute(fSimulatedCode);
        *result = fSimulatedCode;
        return NS_OK;
    }

    // XXX what about "javaprogram:" and "javabean:"?
    return NS_ERROR_FAILURE;
}

NS_METHOD
nsJVMPluginTagInfo::GetCodeBase(const char* *result)
{
    // If we've already cached and computed the value, use it...
    if (fSimulatedCodebase) {
        *result = fSimulatedCodebase;
        return NS_OK;
    }

    // See if it's supplied as an attribute...
    const char* codebase;
    nsresult err = fPluginTagInfo->GetAttribute("codebase", &codebase);
    if (err == NS_OK && codebase != NULL) {
        *result = codebase;
        return NS_OK;
    }

    // Okay, we'll need to simulate it from the layout tag's base URL.
    const char* docBase;
    err = fPluginTagInfo->GetDocumentBase(&docBase);
    if (err != NS_OK) return err;
    PA_LOCK(codebase, const char*, docBase);

    if ((fSimulatedCodebase = PL_strdup(codebase)) != NULL) {
        char* lastSlash = PL_strrchr(fSimulatedCodebase, '/');

        // chop of the filename from the original document base URL to
        // generate the codebase.
        if (lastSlash != NULL)
            *(lastSlash + 1) = '\0';
    }
    
    PA_UNLOCK(docBase);
    *result = fSimulatedCodebase;
    return NS_OK;
}

NS_METHOD
nsJVMPluginTagInfo::GetArchive(const char* *result)
{
    return fPluginTagInfo->GetAttribute("archive", result);
}

NS_METHOD
nsJVMPluginTagInfo::GetName(const char* *result)
{
    const char* attrName;
    nsPluginTagType type;
    nsresult err = fPluginTagInfo->GetTagType(&type);
    if (err != NS_OK) return err;
    switch (type) {
      case nsPluginTagType_Applet:
        attrName = "name";
        break;
      default:
        attrName = "id";
        break;
    }
    return fPluginTagInfo->GetAttribute(attrName, result);
}

NS_METHOD
nsJVMPluginTagInfo::GetMayScript(PRBool *result)
{
    const char* attr;
    *result = PR_FALSE;

    nsresult err = fPluginTagInfo->GetAttribute("mayscript", &attr);
    if (err) return err;

    if (PL_strcasecmp(attr, "true") == 0)
    {
       *result = PR_TRUE;
    }
    return NS_OK;
}

NS_METHOD
nsJVMPluginTagInfo::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr,
                           nsIPluginTagInfo2* info)
{
	 if(!aInstancePtr)
	     return NS_ERROR_INVALID_POINTER;

    if (outer && !aIID.Equals(NS_GET_IID(nsISupports)))
        return NS_ERROR_INVALID_ARG;

    nsJVMPluginTagInfo* jvmTagInfo = new nsJVMPluginTagInfo(outer, info);
    if (jvmTagInfo == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

	 nsresult result = jvmTagInfo->AggregatedQueryInterface(aIID, aInstancePtr);
	 if (NS_FAILED(result)) goto error;

    result = jvmTagInfo->QueryInterface(kIPluginTagInfo2IID,
                                            (void**)&jvmTagInfo->fPluginTagInfo);
    if (NS_FAILED(result)) goto error;
    return result;

  error:
    delete jvmTagInfo;
    return result;
}

////////////////////////////////////////////////////////////////////////////////

