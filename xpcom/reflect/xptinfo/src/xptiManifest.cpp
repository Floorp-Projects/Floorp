/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Implementation of xptiManifest. */

#include "xptiprivate.h"


static const char g_MainManifestFilename[] = "xpti.dat";
static const char g_TempManifestFilename[] = "xptitemp.dat";

static const char g_Disclaimer[] = "# Generated file. ** DO NOT EDIT! **";

static const char g_TOKEN_Files[]          = "Files";
static const char g_TOKEN_ArchiveItems[]   = "ArchiveItems";
static const char g_TOKEN_Interfaces[]     = "Interfaces";
static const char g_TOKEN_Header[]         = "Header";
static const char g_TOKEN_Version[]        = "Version";

static const int  g_VERSION_MAJOR          = 1;
static const int  g_VERSION_MINOR          = 0;

// This is used to make PR_sscanf for strings safer. We refuse to even 
// attempt to do a PR_sscanf from a line longer than this.
static const PRUint32 g_MAX_LINE_LEN = 256;

/***************************************************************************/

PR_STATIC_CALLBACK(PRIntn)
xpti_InterfaceWriter(PLHashEntry *he, PRIntn i, void *arg)
{
    xptiInterfaceInfo* info = (xptiInterfaceInfo*) he->value;
    PRFileDesc* fd = (PRFileDesc*)  arg;

    if(!info->IsValid())
        return HT_ENUMERATE_STOP;

    char* iidStr = info->GetTheIID()->ToString();
    if(!iidStr)
        return HT_ENUMERATE_STOP;

    const xptiTypelib& typelib = info->GetTypelibRecord();

    PRBool success =  PR_fprintf(fd, "%d,%s,%s,%d,%d,%d\n",
                                 (int) i,
                                 info->GetTheName(),
                                 iidStr,
                                 (int) typelib.GetFileIndex(),
                                 (int) (typelib.IsZip() ? 
                                 typelib.GetZipItemIndex() : -1),
                                 (int) info->GetScriptableFlag());

    nsCRT::free(iidStr);

    return success ? HT_ENUMERATE_NEXT : HT_ENUMERATE_STOP;
}

// static
PRBool xptiManifest::Write(xptiInterfaceInfoManager* aMgr,
                           xptiWorkingSet*           aWorkingSet)
{

    PRBool succeeded = PR_FALSE;
    PRFileDesc* fd = nsnull;
    PRUint32 i;
    PRIntn interfaceCount = 0;

    nsCOMPtr<nsILocalFile> tempFile;
    if(!aMgr->GetManifestDir(getter_AddRefs(tempFile)) || !tempFile)
        return PR_FALSE;

    if(NS_FAILED(tempFile->Append(g_TempManifestFilename)))
        return PR_FALSE;

    // exist via "goto out;" from here on...


    if(NS_FAILED(tempFile->
                    OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                     0666, &fd)) || !fd)
    {
        goto out;
    }

    // write file header comments

    if(!PR_fprintf(fd, "%s\n", g_Disclaimer))
        goto out;

    // write the [Header] block and version number.

    if(!PR_fprintf(fd, "\n[%s,%d]\n", g_TOKEN_Header, 1))
        goto out;

    if(!PR_fprintf(fd, "%d,%s,%d,%d\n", 
                       0, g_TOKEN_Version, g_VERSION_MAJOR, g_VERSION_MINOR))
        goto out;

    // write Files list

    if(!PR_fprintf(fd, "\n[%s,%d]\n", 
                       g_TOKEN_Files, 
                       (int) aWorkingSet->GetFileCount()))
        goto out;

    for(i = 0; i < aWorkingSet->GetFileCount(); i++)
    {
        if(!PR_fprintf(fd, "%d,%s,%lld,%lld\n",
                           (int) i,
                           aWorkingSet->GetFileAt(i).GetName(),
                           PRInt64(aWorkingSet->GetFileAt(i).GetSize()),
                           PRInt64(aWorkingSet->GetFileAt(i).GetDate())))
        goto out;
    }

    // write ArchiveItems list

    if(!PR_fprintf(fd, "\n[%s,%d]\n", 
                       g_TOKEN_ArchiveItems, 
                       (int) aWorkingSet->GetZipItemCount()))
        goto out;

    for(i = 0; i < aWorkingSet->GetZipItemCount(); i++)
    {
        if(!PR_fprintf(fd, "%d,%s\n",
                           (int) i,
                           aWorkingSet->GetZipItemAt(i).GetName()))
        goto out;
    }

    // write the Interfaces list

    interfaceCount = aWorkingSet->mNameTable->nentries;

    if(!PR_fprintf(fd, "\n[%s,%d]\n", 
                       g_TOKEN_Interfaces, 
                       (int) interfaceCount))
        goto out;

    if(interfaceCount != 
        PL_HashTableEnumerateEntries(aWorkingSet->mNameTable, 
                                     xpti_InterfaceWriter, fd))
        goto out;


    if(PR_SUCCESS == PR_Close(fd))
    {
        succeeded = PR_TRUE;
    }
    fd = nsnull;

out:
    if(fd)
        PR_Close(fd);

    if(succeeded)
    {
        // delete the old file and rename this
        nsCOMPtr<nsILocalFile> mainFile;
        if(!aMgr->GetManifestDir(getter_AddRefs(mainFile)) || !mainFile)
            return PR_FALSE;
    
        if(NS_FAILED(mainFile->Append(g_MainManifestFilename)))
            return PR_FALSE;
    
        PRBool exists;
        if(NS_FAILED(mainFile->Exists(&exists)))
            return PR_FALSE;

        if(exists && NS_FAILED(mainFile->Remove(PR_FALSE)))
            return PR_FALSE;
    
        // XXX Would prefer MoveTo with a 'null' newdir, the but nsILocalFile
        // implementation are broken.
        // http://bugzilla.mozilla.org/show_bug.cgi?id=33098
#if 1        
        nsCOMPtr<nsILocalFile> dir;
        if(!aMgr->GetManifestDir(getter_AddRefs(dir)) || !dir)
            return PR_FALSE;

        if(NS_FAILED(tempFile->CopyTo(dir, g_MainManifestFilename)))
            return PR_FALSE;
#else
        // so let's try the MoveTo!
        if(NS_FAILED(tempFile->MoveTo(nsnull, g_MainManifestFilename)))
            return PR_FALSE;
#endif
    }

    return succeeded;
}        


static char* 
ReadManifestIntoMemory(xptiInterfaceInfoManager* aMgr,
                       PRUint32* pLength)
{
    PRFileDesc* fd = nsnull;
    PRInt32 flen;
    PRInt64 fileSize;
    char* whole = nsnull;

    nsCOMPtr<nsILocalFile> aFile;
    if(!aMgr->GetManifestDir(getter_AddRefs(aFile)) || !aFile)
        return nsnull;
    
    if(NS_FAILED(aFile->Append(g_MainManifestFilename)))
        return nsnull;

#ifdef DEBUG
    {
        static PRBool shown = PR_FALSE;
        char* path;
        if(!shown && NS_SUCCEEDED(aFile->GetPath(&path)) && path)
        {
            printf("Type Manifest File: %s\n", path);
            nsMemory::Free(path);
            shown = PR_TRUE;        
        } 
    }            
#endif

    if(NS_FAILED(aFile->GetFileSize(&fileSize)) || !(flen = nsInt64(fileSize)))
        return nsnull;

    whole = new char[flen];
    if (!whole)
        return nsnull;

    // all exits from on here should be via 'goto out' 

    if(NS_FAILED(aFile->OpenNSPRFileDesc(PR_RDONLY, 0444, &fd)) || !fd)
        goto out;

    if(flen > PR_Read(fd, whole, flen))
        goto out;

 out:
    if(fd)
        PR_Close(fd);
    *pLength = flen;
    return whole;
}

/***************************************************/
class ManifestLineReader
{
public:
    ManifestLineReader() : mBase(nsnull) {} 
    ~ManifestLineReader() {}

    void Init(char* base, PRUint32 flen) 
        {mBase = mCur = mNext = base; 
         mLength = 0;
         mLimit = base + flen;}

    PRBool      NextLine();
    char*       LinePtr() {return mCur;}    
    PRUint32    LineLength() {return mLength;}    

private:
    char*       mCur;
    PRUint32    mLength;
    char*       mNext;
    char*       mBase;
    char*       mLimit;
};

inline static PRBool is_eol(char c) {return c == '\n' || c == '\r';}

PRBool
ManifestLineReader::NextLine()
{
    if(mNext >= mLimit)
        return PR_FALSE;

    mCur = mNext;
    mLength = 0;

    while(mNext < mLimit)
    {
        if(is_eol(*mNext))
        {
            *mNext = '\0';
            for(++mNext; mNext < mLimit; ++mNext)
                if(!is_eol(*mNext))
                    break;
            return PR_TRUE;
        }
        ++mNext;
        ++mLength;
    }
    return PR_FALSE;        
}
/***************************************************/

static
PRBool ReadSectionHeader(ManifestLineReader& reader, 
                         const char *token, int minCount,
                         char* name, int* count)
{
    while(1)
    {
        if(!reader.NextLine())
            break;
        if(*reader.LinePtr() == '[')
        {
            if(reader.LineLength() > g_MAX_LINE_LEN)
                break;
            if(2 == PR_sscanf(reader.LinePtr(), "[%[^','],%d]", 
                              name, count) &&
               0 == PL_strcmp(name, token) &&
               *count >= minCount)
            {
                return PR_TRUE;
            }
            break;
        }
    }
    return PR_FALSE;
}


// static
PRBool xptiManifest::Read(xptiInterfaceInfoManager* aMgr,
                          xptiWorkingSet*           aWorkingSet)
{
    char* whole = nsnull;
    char* name = nsnull;
    char* iidStr = nsnull;
    PRBool succeeded = PR_FALSE;
    PRUint32 flen;
    ManifestLineReader reader;
    int headerCount = 0;
    int fileCount = 0;
    int zipItemCount = -1;
    int interfaceCount = 0;
    int i;
    int index;
    int major;
    int minor;
    int flags;
    PRInt64 size;
    PRInt64 date;

    whole = ReadManifestIntoMemory(aMgr, &flen);
    if(!whole)
        return PR_FALSE;

    reader.Init(whole, flen);

    // all exits from on here should be via 'goto out' 
    
    // We are going to scanf into these buffers, let's make them big.
    name = new char[g_MAX_LINE_LEN];
    if(!name)
        goto out;
    
    iidStr = new char[g_MAX_LINE_LEN];
    if(!iidStr)
        goto out;

    // Look for "Header" section

    // This version accepts only version 1,0. We also freak if the header
    // has more than one entry. The rationale is that we want to force an
    // autoreg if the xpti.dat file was written by *any* other version of
    // the software. Future versions may wish to support updating older
    // manifests in some interesting way.

    if(!ReadSectionHeader(reader, g_TOKEN_Header, 1, name, &headerCount))
        goto out;
    
    // get the version

    if(headerCount != 1)
        goto out;

    if(!reader.NextLine())
        goto out;

    if(reader.LineLength() > g_MAX_LINE_LEN)
        goto out;
        
    if(4 != PR_sscanf(reader.LinePtr(), "%d,%[^','],%d,%d", 
                      &index, name, &major, &minor) ||
       0 != index ||
       0 != PL_strcmp(name, g_TOKEN_Version) ||
       major != g_VERSION_MAJOR ||
       minor != g_VERSION_MINOR)
    {
        goto out;    
    }

    // Look for "Files" section

    if(!ReadSectionHeader(reader, g_TOKEN_Files, 1, name, &fileCount))
        goto out;


    // Alloc room in the WorkingSet for the filearray.

    if(!aWorkingSet->NewFileArray(fileCount))   
        goto out;    

    // Read the file records

    for(i = 0; i < fileCount; ++i)
    {
        if(!reader.NextLine())
            goto out;

        if(reader.LineLength() > g_MAX_LINE_LEN)
            goto out;
            
        if(4 != PR_sscanf(reader.LinePtr(), "%d,%[^','],%lld,%lld", 
                          &index, name, &size, &date) ||
           i != index ||
           !*name)
        {
            goto out;    
        }
        
        // Append a new file record to the array.

        aWorkingSet->AppendFile(
            xptiFile(nsInt64(size), nsInt64(date), name, aWorkingSet));
    }

    // Look for "ZipItems" section

    if(!ReadSectionHeader(reader, g_TOKEN_ArchiveItems, 0, name, &zipItemCount))
        goto out;

    // Alloc room in the WorkingSet for the zipItemarray.

    if(zipItemCount)
        if(!aWorkingSet->NewZipItemArray(zipItemCount))   
            goto out;    

    // Read the zipItem records

    for(i = 0; i < zipItemCount; ++i)
    {
        if(!reader.NextLine())
            goto out;

        if(reader.LineLength() > g_MAX_LINE_LEN)
            goto out;
            
        if(2 != PR_sscanf(reader.LinePtr(), "%d,%[^',']", 
                          &index, name) ||
           i != index ||
           !*name)
        {
            goto out;    
        }
        
        // Append a new zipItem record to the array.

        aWorkingSet->AppendZipItem(xptiZipItem(name, aWorkingSet));
    }

    // Look for "Interfaces" section

    if(!ReadSectionHeader(reader, g_TOKEN_Interfaces, 1, name, &interfaceCount))
        goto out;

    // Read the interface records

    for(i = 0; i < interfaceCount; ++i)
    {
        int fileIndex;
        int zipItemIndex;
        nsIID iid;
        xptiInterfaceInfo* info;
        xptiTypelib typelibRecord;

        if(!reader.NextLine())
            goto out;

        if(reader.LineLength() > g_MAX_LINE_LEN)
            goto out;
            
        if(6 != PR_sscanf(reader.LinePtr(), "%d,%[^','],%[^','],%d,%d,%d", 
                          &index, name, iidStr, &fileIndex, &zipItemIndex,
                          &flags) ||
           i != index ||
           !*name     ||
           !*iidStr   ||
           !iid.Parse(iidStr) ||
           fileIndex < 0 ||
           fileIndex >= fileCount ||
           (zipItemIndex != -1 && zipItemIndex >= zipItemCount) ||
           (flags != 0 && flags != 1))
        {
            goto out;    
        }
        
        // Build an InterfaceInfo and hook it in.

        if(zipItemIndex == -1)
            typelibRecord.Init(fileIndex);
        else
            typelibRecord.Init(fileIndex, zipItemIndex);
        info = new xptiInterfaceInfo(name, iid, typelibRecord, aWorkingSet);
        if(!info)
        {
            goto out;    
        }    
        
        NS_ADDREF(info);
        if(!info->IsValid())
        {
            // XXX bad!
            NS_RELEASE(info);
            goto out;    
        }

        info->SetScriptableFlag(flags==1);

        // The name table now owns the reference we AddRef'd above
        PL_HashTableAdd(aWorkingSet->mNameTable, info->GetTheName(), info);
        PL_HashTableAdd(aWorkingSet->mIIDTable, info->GetTheIID(), info);
    }

    // success!

    succeeded = PR_TRUE;

 out:
    if(whole)
        delete [] whole;
    if(name)
        delete [] name;
    if(iidStr)
        delete [] iidStr;

    if(!succeeded)
    {
        // Cleanup the WorkingSet on failure.
        aWorkingSet->InvalidateInterfaceInfos();
        aWorkingSet->ClearHashTables();
        aWorkingSet->ClearFiles();
    }
    return succeeded;
}        


     

