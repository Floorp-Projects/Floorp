/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pete Zha <pete.zha@sun.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include "nsJVMConfigManagerUnix.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsReadableUtils.h"
#include "prprf.h"
#include "nsNetCID.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIVariant.h"
#include "nsISimpleEnumerator.h"
#include <stdio.h>  /* OSF/1 requires this before grp.h, so put it first */
#include <unistd.h>

#define NS_COMPILER_GNUC3 defined(__GXX_ABI_VERSION) && \
                          (__GXX_ABI_VERSION >= 102) /* G++ V3 ABI */

static NS_DEFINE_CID(kHttpHandlerCID, NS_HTTPPROTOCOLHANDLER_CID);

// Implementation of nsJVMConfigManagerUnix
NS_IMPL_ISUPPORTS1(nsJVMConfigManagerUnix, nsIJVMConfigManager)

nsJVMConfigManagerUnix::nsJVMConfigManagerUnix()
{
    InitJVMConfigList();
}

PR_STATIC_CALLBACK(PRBool)
FreeJVMConfig(nsHashKey *aKey, void *aData, void* aClosure)
{
    nsJVMConfig* config = NS_STATIC_CAST(nsJVMConfig *, aData);

    NS_IF_RELEASE(config);

    return PR_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
AppendJVMConfig(nsHashKey *aKey, void *aData, void* aClosure)
{
    nsJVMConfig* config = NS_STATIC_CAST(nsJVMConfig *, aData);
    nsIMutableArray *array = NS_STATIC_CAST(nsIMutableArray *, aClosure);
    NS_ENSURE_TRUE(config && array, PR_FALSE);

    array->AppendElement(config, PR_FALSE);
    return PR_TRUE;
}

nsJVMConfigManagerUnix::~nsJVMConfigManagerUnix()
{
    ClearJVMConfigList();
}

void
nsJVMConfigManagerUnix::ClearJVMConfigList()
{
    if (mJVMConfigList.Count() > 0) {
        mJVMConfigList.Reset(FreeJVMConfig);
    }
}

NS_IMETHODIMP
nsJVMConfigManagerUnix::GetJVMConfigList(nsIArray **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    ClearJVMConfigList();
    InitJVMConfigList();

    nsCOMPtr<nsIMutableArray> array;
    nsresult rv = NS_NewArray(getter_AddRefs(array));
    NS_ENSURE_SUCCESS(rv, rv);

    if (mJVMConfigList.Count() > 0) {
        mJVMConfigList.Enumerate(AppendJVMConfig,
                                 NS_STATIC_CAST(void *, array));
        *_retval = NS_STATIC_CAST(nsIArray *, array);
        NS_IF_ADDREF(*_retval);
    } else
        *_retval = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsJVMConfigManagerUnix::InitJVMConfigList()
{
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
    NS_ENSURE_TRUE(prefs, NS_ERROR_FAILURE);

    nsCOMPtr<nsILocalFile> globalFile;
    prefs->GetComplexValue("java.global_java_version_file",
                           NS_GET_IID(nsILocalFile),
                           getter_AddRefs(globalFile));

    nsCOMPtr<nsILocalFile> privateFile;
    prefs->GetComplexValue("java.private_java_version_file",
                           NS_GET_IID(nsILocalFile),
                           getter_AddRefs(privateFile));

    nsCOMPtr<nsILineInputStream> globalStream;
    nsresult rv = GetLineInputStream(globalFile, getter_AddRefs(globalStream));
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) || rv == NS_ERROR_FILE_NOT_FOUND, rv);

    nsCOMPtr<nsILineInputStream> privateStream;
    rv = GetLineInputStream(privateFile, getter_AddRefs(privateStream));
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) || rv == NS_ERROR_FILE_NOT_FOUND, rv);

    rv = InitJVMConfigList(globalStream, privateStream);
    NS_ENSURE_SUCCESS(rv, rv);

    // Search for a Java installation in the default install location.
    return SearchDefault();
}

NS_IMETHODIMP
nsJVMConfigManagerUnix::SetCurrentJVMConfig(nsIJVMConfig* aJVMConfig)
{
    NS_ENSURE_ARG_POINTER(aJVMConfig);

    nsCOMPtr<nsIFile> srcFile;
    nsresult rv = aJVMConfig->GetMozillaPluginPath(getter_AddRefs(srcFile));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> pluginDir;
    rv = NS_GetSpecialDirectory(NS_APP_PLUGINS_DIR, getter_AddRefs(pluginDir));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasPermission = PR_FALSE;
    pluginDir->IsWritable(&hasPermission);
    if (!hasPermission) {
        return NS_ERROR_FAILURE;
    }

    nsAutoString fileName;
    srcFile->GetLeafName(fileName);
    nsCOMPtr<nsILocalFile> destFile(do_QueryInterface(pluginDir));
    if (TestExists(destFile, fileName))
        destFile->Remove(PR_FALSE);

    nsCAutoString srcFileName;
    rv = srcFile->GetNativePath(srcFileName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString destFileName;
    destFile->GetNativePath(destFileName);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt16 result = 0;
    result = symlink(srcFileName.get(), destFileName.get());

    return result >= 0 ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsJVMConfigManagerUnix::InitJVMConfigList(nsILineInputStream* aGlobal,
                                          nsILineInputStream* aPrivate)
{
    nsresult rv = NS_OK;

    if (aGlobal) {
        rv = ParseStream(aGlobal);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (aPrivate) {
        rv = ParseStream(aPrivate);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

nsresult
nsJVMConfigManagerUnix::GetLineInputStream(nsIFile* aFile,
                                           nsILineInputStream** _retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    
    nsresult rv = NS_OK;
    
    nsCOMPtr<nsILocalFile> file(do_QueryInterface(aFile, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFileInputStream>
        fileStream(do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = fileStream->Init(file, -1, -1, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILineInputStream> lineStream(do_QueryInterface(fileStream, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    *_retval = lineStream;
    NS_IF_ADDREF(*_retval);

    return NS_OK;
}

nsresult
nsJVMConfigManagerUnix::ParseStream(nsILineInputStream* aStream)
{
    NS_ENSURE_ARG_POINTER(aStream);

    PRBool notEOF = PR_TRUE;

    nsAutoString lineBuffer;
    do {
        nsAutoString line;
        nsCAutoString cLine;
        nsresult rv = aStream->ReadLine(cLine, &notEOF);
        NS_ENSURE_SUCCESS(rv, rv);
        CopyASCIItoUTF16(cLine, line);

        PRInt32 slashOffset, equalsOffset;
        slashOffset = line.FindChar('\\');
        equalsOffset = line.FindChar('=');

        // Since one java installation contains several key/value pair
        // and they are separeted by '\'. We need to join them together
        // to a single line and then parse it.
        if (slashOffset != kNotFound && equalsOffset != kNotFound) {
            // This means the line not finished, we need to append it to buffer
            lineBuffer.Append(Substring(line, 0, slashOffset));
        } else if (slashOffset == kNotFound && equalsOffset != kNotFound) {
            // This should the last one of a line. Append it to line
            // and then we can Parse it and get necessary information
            lineBuffer.Append(line);
            ParseLine(lineBuffer);
        } else {
            // Start of a new line
            lineBuffer.Truncate();
        }
    } while (notEOF);

    return NS_OK;
}

nsresult
nsJVMConfigManagerUnix::ParseLine(nsAString& aLine)
{
#if (NS_COMPILER_GNUC3)
    nsAutoString compiler;
    GetValueFromLine(aLine, "compiler", compiler);

    NS_ENSURE_TRUE(compiler.Find("gcc32") != kNotFound, NS_OK);
#endif

    nsAutoString version;
    GetValueFromLine(aLine, "version", version);
    
    nsAutoString type;
    GetValueFromLine(aLine, "type", type);
    
    nsAutoString os;
    GetValueFromLine(aLine, "os", os);
    
    nsAutoString arch;
    GetValueFromLine(aLine, "arch", arch);
    
    nsAutoString pathStr;
    GetValueFromLine(aLine, "path", pathStr);

    nsAutoString mozillaPluginPath;
    GetMozillaPluginPath(aLine, mozillaPluginPath);

    NS_ENSURE_TRUE(!mozillaPluginPath.IsEmpty(), NS_OK);

    nsAutoString description;
    GetValueFromLine(aLine, "description", description);
    description.Trim("\"");

    // Test whether the plugin file is existing.
    nsresult rv = NS_OK;
    nsCOMPtr<nsILocalFile>
        testPath(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString testPathStr(pathStr);
    if (type.EqualsLiteral("jdk"))
        testPathStr.AppendLiteral("/jre");

    testPathStr.Append(mozillaPluginPath);
    testPath->InitWithPath(testPathStr);

    // If the file doesn't exists, we just return NS_OK
    PRBool exists;
    testPath->Exists(&exists);
    NS_ENSURE_TRUE(exists, NS_OK);

    nsCOMPtr<nsIFile> mozPluginPath(do_QueryInterface(testPath, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILocalFile>
        path(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
    path->InitWithPath(pathStr);

    // We use java home as the key because one home
    // will contain only one java installation.
    // This could make sure we don't duplicate the config info in the list
    nsStringKey key(pathStr);
    nsJVMConfig* config = NS_STATIC_CAST(nsJVMConfig *,
                                         mJVMConfigList.Get(&key));

    // Only create it and add the config to list if it doesn't exist.
    if (!config) {
        config = new nsJVMConfig(version, type, os, arch, path,
                                 mozPluginPath, description);
        NS_ENSURE_TRUE(config, NS_ERROR_OUT_OF_MEMORY);
        mJVMConfigList.Put(&key, NS_STATIC_CAST(void *, config));
        NS_ADDREF(config);
    }
    
    return NS_OK;
}

nsresult
nsJVMConfigManagerUnix::GetMozillaPluginPath(nsAString& aLine,
                                             nsAString& _retval)
{
    nsCAutoString agentVersion;
    nsresult rv = GetAgentVersion(agentVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get mozilla plugin path from key
    // mozilla<version>.plugin.path
    // <version> should like this: "1.2", "1.3"
    nsCAutoString key("mozilla");
    key.Append(agentVersion);
    key.Append(".plugin.path");

    GetValueFromLine(aLine, key.get(), _retval);

    if (!_retval.IsEmpty()) return NS_OK;

    nsAutoString versionStr;
    rv = GetNSVersion(versionStr);
    NS_ENSURE_SUCCESS(rv, rv);

    key.AssignWithConversion(versionStr);
    key.Append(".plugin.path");

    GetValueFromLine(aLine, key.get(), _retval);

    // Fall back to use ns610.plugin.path if _retval is still empty.
    if (_retval.IsEmpty())
        GetValueFromLine(aLine, "ns610.plugin.path", _retval);

    return NS_OK;
}

nsresult
nsJVMConfigManagerUnix::GetAgentVersion(nsCAutoString& _retval)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsIHttpProtocolHandler> http = do_GetService(kHttpHandlerCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString userAgent;
    rv = http->GetUserAgent(userAgent);
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt32 rvOffset = userAgent.Find("rv:");

    if (rvOffset != kNotFound)
        _retval.Assign(Substring(userAgent, rvOffset + 3, 3));

    return NS_OK;
}

nsresult
nsJVMConfigManagerUnix::GetAgentVersion(float* _retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    nsresult rv = NS_OK;

    nsCAutoString agentVersion;
    GetAgentVersion(agentVersion);
    nsCOMPtr <nsIWritableVariant> p =
        do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = p->SetAsACString(agentVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    return p->GetAsFloat(_retval);
}

PRBool
nsJVMConfigManagerUnix::GetValueFromLine(nsAString& aLine, const char* aKey,
                                         nsAString& _retval)
{
    _retval.Truncate();

    nsAutoString line(aLine);
    // Find the offset of the given key in the line
    PRInt32 keyOffset = line.Find(aKey);

    // make sure the key exists in the line.
    NS_ENSURE_TRUE(keyOffset != kNotFound, PR_FALSE);

    // Find '=' right after the key
    PRInt32 equalsOffset = aLine.FindChar('=', keyOffset);
    NS_ENSURE_TRUE(equalsOffset != kNotFound, PR_FALSE);

    // Find '|' which is the terminal of a pair of key/value
    PRInt32 lineOffset = aLine.FindChar('|', equalsOffset);
    lineOffset = lineOffset != kNotFound ? lineOffset : aLine.Length();

    // OK, we separate the value from the line between '=' and '|'
    nsAutoString value(Substring(aLine,
                                 equalsOffset + 1,
                                 lineOffset - equalsOffset -1));

    // Removing the leading/trailing spaces
    value.Trim(" ");
    _retval = value;
    return PR_TRUE;
}

nsresult
nsJVMConfigManagerUnix::GetNSVersion(nsAString& _retval)
{
    float version;
    nsresult rv = GetAgentVersion(&version);
    NS_ENSURE_SUCCESS(rv, rv);

    // Check mozilla's version
    // ns7 is for mozilla1.3 or later
    // ns610 is for earlier version of mozilla.
    if (version >= 1.3) {
        _retval.AssignLiteral("ns7");
    } else {
        _retval.AssignLiteral("ns610");
    }

    return NS_OK;
}

nsresult
nsJVMConfigManagerUnix::SearchDefault()
{
#ifdef SPARC
    const char* defaultLocationName = "java.default_java_location_solaris";
#else
    const char* defaultLocationName = "java.default_java_location_others";
#endif

    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
    NS_ENSURE_TRUE(prefs, NS_ERROR_FAILURE);

    nsXPIDLCString defaultLocationXPIDLValue;
    prefs->GetCharPref(defaultLocationName,
                       getter_Copies(defaultLocationXPIDLValue));

    NS_ConvertUTF8toUCS2 defaultLocation(defaultLocationXPIDLValue);

#ifdef SPARC
    // On Solaris, the default location is the java home
    return AddDirectory(defaultLocation);
#else
    // On Linux and other platform,
    // the default location can contain multiple java installations
    return SearchDirectory(defaultLocation);
#endif
}

nsresult
nsJVMConfigManagerUnix::SearchDirectory(nsAString& aDirName)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsILocalFile>
        localDir(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = localDir->InitWithPath(aDirName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> dir(do_QueryInterface(localDir, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = dir->GetDirectoryEntries(getter_AddRefs(entries));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMoreElements;
    entries->HasMoreElements(&hasMoreElements);
    while (hasMoreElements) {
        nsCOMPtr<nsISupports> next;
        rv = entries->GetNext(getter_AddRefs(next));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIFile> entry(do_QueryInterface(next, &rv));
        NS_ENSURE_SUCCESS(rv, rv);

        AddDirectory(entry);
        entries->HasMoreElements(&hasMoreElements);
    }
    
    return NS_OK;
}

nsresult
nsJVMConfigManagerUnix::AddDirectory(nsIFile* aHomeDir)
{
    NS_ENSURE_ARG_POINTER(aHomeDir);
    nsAutoString homeDirName;
    aHomeDir->GetPath(homeDirName);
    return AddDirectory(homeDirName);
}

nsresult
nsJVMConfigManagerUnix::AddDirectory(nsAString& aHomeDirName)
{
    nsresult rv = NS_OK;

    nsAutoString type;
    nsAutoString mozillaPluginPath;

    nsCOMPtr<nsILocalFile>
        testPath(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    testPath->InitWithPath(aHomeDirName);
    testPath->Append(NS_LITERAL_STRING("jre"));

    PRBool exists;
    testPath->Exists(&exists);
    if (exists) {
        type.AssignLiteral("jdk");
    } else {
        type.AssignLiteral("jre");
        testPath->InitWithPath(aHomeDirName);
    }

    testPath->Append(NS_LITERAL_STRING("plugin"));

    // Get Arch. "sparc" or "i386"
    nsAutoString arch;
    NS_ENSURE_TRUE(TestArch(testPath, arch), NS_OK);

    // Get NS Version. "ns610" or "ns7"
    nsAutoString nsVersion;
    NS_ENSURE_TRUE(TestNSVersion(testPath, nsVersion), NS_OK);

    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
    NS_ENSURE_TRUE(prefs, NS_ERROR_FAILURE);

    nsCAutoString javaLibName("java.java_plugin_library_name");
    nsXPIDLCString javaLibNameXPIDLValue;
    prefs->GetCharPref(javaLibName.get(),
                        getter_Copies(javaLibNameXPIDLValue));

    char* temp = PR_GetLibraryName(nsnull, javaLibNameXPIDLValue.get());
    nsCAutoString pluginFileName(temp);
    testPath->AppendNative(pluginFileName);
    PR_FreeLibraryName(temp);

    // If the plugin file doesn't exist, we just return NS_OK
    testPath->Exists(&exists);
    NS_ENSURE_TRUE(exists, NS_OK);

    nsCOMPtr<nsIFile> mozPluginPath(do_QueryInterface(testPath, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILocalFile>
        path(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    path->InitWithPath(aHomeDirName);

    nsAutoString version;
    path->GetLeafName(version);

    nsStringKey key(aHomeDirName);
    nsJVMConfig* config = NS_STATIC_CAST(nsJVMConfig *,
                                         mJVMConfigList.Get(&key));
    if (!config) {
        config = new nsJVMConfig(version, type, EmptyString(), arch, path,
                                 mozPluginPath, EmptyString());
        NS_ENSURE_TRUE(config, NS_ERROR_OUT_OF_MEMORY);
        mJVMConfigList.Put(&key, NS_STATIC_CAST(void *, config));
        NS_ADDREF(config);
    }

    return NS_OK;
}

PRBool
nsJVMConfigManagerUnix::TestArch(nsILocalFile* aPluginPath, nsAString& aArch)
{
#ifdef SPARC
    aArch.AssignLiteral("sparc");
#else
    aArch.AssignLiteral("i386");
#endif
    return TestExists(aPluginPath, aArch);
}

PRBool
nsJVMConfigManagerUnix::TestNSVersion(nsILocalFile* aArchPath,
                                      nsAString& aNSVersion)
{
    nsAutoString versionStr;
    nsresult rv = GetNSVersion(versionStr);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    aNSVersion.Assign(versionStr);
#if (NS_COMPILER_GNUC3)
    aNSVersion.AppendLiteral("-gcc32");
#endif
    return TestExists(aArchPath, aNSVersion);
}

PRBool
nsJVMConfigManagerUnix::TestExists(nsILocalFile* aBaseDir, nsAString& aSubName)
{
    NS_ENSURE_ARG_POINTER(aBaseDir);

    aBaseDir->Append(aSubName);
    PRBool exists;
    aBaseDir->Exists(&exists);

    return exists;
}

