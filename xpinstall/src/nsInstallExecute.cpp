/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *   Douglas Turner <dougt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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



#include "nsCRT.h"
#include "prmem.h"
#include "VerReg.h"
#include "nsInstallExecute.h"
#include "nsInstallResources.h"
#include "ScheduledTasks.h"

#include "nsInstall.h"
#include "nsIDOMInstallVersion.h"
#include "nsProcess.h"
#include "nsReadableUtils.h"

static NS_DEFINE_CID(kIProcessCID, NS_PROCESS_CID); 

MOZ_DECL_CTOR_COUNTER(nsInstallExecute)

// Chop the command-line up in place into an array of arguments
//   by replacing spaces in the command-line string with null
//   terminators and pointing the array elements to the 
//   characters following the null terminators.
//
// aArgsString is a string containing the complete command-line.
// aArgs points to an array which will be filled with the 
//   individual arguments from the command-line.
// aArgsAvailable is the size of aArgs, i.e. the maximum number of
//   individual command-line arguments which can be stored in the array.
//
// Returns the count of the number of command-line arguments actually 
//   stored into the array aArgs or -1 if it fails.
PRInt32 xpi_PrepareProcessArguments(const char *aArgsString, char **aArgs, PRInt32 aArgsAvailable)
{
   int   argc;
   char *c;
   char *p; // look ahead
   PRBool quoted = PR_FALSE;

   aArgs[0] = (char *)aArgsString;
   if (!aArgs[0])
      return -1;

   // Strip leading spaces from command-line string.
   argc = 0;
   c = aArgs[argc];
   while (*c == ' ') ++c;
   aArgs[argc++] = c;

   for (; *c && argc < aArgsAvailable; ++c) 
   {
      switch(*c) {

      // Only handle escaped double quote and escaped backslash.
      case '\\':
         // See if next character is backslash or dquote
         if ( *(c+1) == '\\' || *(c+1) == '\"' )
         {
            // Eat escape character (i.e., backslash) by
            //   shifting all characters to the left one.
            for (p=c; *p != 0; ++p)
               *p = *(p+1);
         }
         break;

      case '\"':
         *c = 0; // terminate current arg
         if (quoted) 
         {
            p = c+1; // look ahead
            while (*p == ' ')
               ++p; // eat spaces
            if (*p)
               aArgs[argc++] = p; //not at end, set next arg
            c = p-1;

            quoted = PR_FALSE;
         }
         else 
         {
            quoted = PR_TRUE;

            if (aArgs[argc-1] == c)
              // Quote is at beginning so 
              //   start current argument after the quote.
              aArgs[argc-1] = c+1;
            else
              // Quote is embedded so 
              //   start a new argument after the quote.
              aArgs[argc++] = c+1;
         }
         break;

      case ' ':
         if (!quoted) 
         {
            *c = 0; // terminate current arg
            p = c+1; // look ahead
            while (*p == ' ')
               ++p; // eat spaces
            if (*p)
               aArgs[argc++] = p; //not at end, set next arg
            c = p-1;
         }
         break;

      default:
         break;  // nothing to do
      }
   }
   return argc;
}

nsInstallExecute:: nsInstallExecute(  nsInstall* inInstall,
                                      const nsString& inJarLocation,
                                      const nsString& inArgs,
                                      const PRBool inBlocking,
                                      PRInt32 *error)

: nsInstallObject(inInstall)
{
    MOZ_COUNT_CTOR(nsInstallExecute);

    if ((inInstall == nsnull) || (inJarLocation.IsEmpty()) )
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }

    mJarLocation        = inJarLocation;
    mArgs               = inArgs;
    mExecutableFile     = nsnull;
    mBlocking           = inBlocking;
    mPid                = nsnull;
}


nsInstallExecute::~nsInstallExecute()
{

    MOZ_COUNT_DTOR(nsInstallExecute);
}



PRInt32 nsInstallExecute::Prepare()
{
    if (mInstall == NULL || mJarLocation.IsEmpty()) 
        return nsInstall::INVALID_ARGUMENTS;

    return mInstall->ExtractFileFromJar(mJarLocation, nsnull, getter_AddRefs(mExecutableFile));
}

PRInt32 nsInstallExecute::Complete()
{
   #define ARG_SLOTS       256

   PRInt32 result = NS_OK;
   PRInt32 rv = nsInstall::SUCCESS;
   char *cArgs[ARG_SLOTS];
   int   argcount = 0;

   if (mExecutableFile == nsnull)
      return nsInstall::INVALID_ARGUMENTS;

   nsCOMPtr<nsIProcess> process = do_CreateInstance(kIProcessCID);

   char *arguments = nsnull;
   if (!mArgs.IsEmpty())
   {
      arguments = ToNewCString(mArgs);
      argcount = xpi_PrepareProcessArguments(arguments, cArgs, ARG_SLOTS);
   }
   if (argcount >= 0)
   {
      result = process->Init(mExecutableFile);
      if (NS_SUCCEEDED(result))
      {
         result = process->Run(mBlocking, (const char**)&cArgs, argcount, mPid);
         if (NS_SUCCEEDED(result))
         {
            if (mBlocking)
            {
               process->GetExitValue(&result);
               if (result != 0)
                  rv = nsInstall::EXECUTION_ERROR;

               // should be OK to delete now since execution done
               DeleteFileNowOrSchedule( mExecutableFile );
            }
            else
            {
               // don't try to delete now since execution is async
               ScheduleFileForDeletion( mExecutableFile );
            }
         }
         else
            rv = nsInstall::EXECUTION_ERROR;
      }
      else
         rv = nsInstall::EXECUTION_ERROR;
   }
   else
      rv = nsInstall::UNEXPECTED_ERROR;

   if(arguments)
      Recycle(arguments);

   return rv;
}

void nsInstallExecute::Abort()
{
    /* Get the names */
    if (mExecutableFile == nsnull) 
        return;

    DeleteFileNowOrSchedule(mExecutableFile);
}

char* nsInstallExecute::toString()
{
    char* buffer = new char[1024];
    char* rsrcVal = nsnull;

    if (buffer == nsnull || !mInstall)
        return nsnull;

    // if the FileSpec is NULL, just us the in jar file name.

    if (mExecutableFile == nsnull)
    {
        char *tempString = ToNewCString(mJarLocation);
        rsrcVal = mInstall->GetResourcedString(NS_LITERAL_STRING("Execute"));

        if (rsrcVal)
        {
            sprintf( buffer, rsrcVal, tempString);
            nsCRT::free(rsrcVal);
        }
        
        if (tempString)
            Recycle(tempString);
    }
    else
    {
        rsrcVal = mInstall->GetResourcedString(NS_LITERAL_STRING("Execute"));

        if (rsrcVal)
        {
            nsCAutoString temp;
            mExecutableFile->GetNativePath(temp);
            sprintf( buffer, rsrcVal, temp.get());
            nsCRT::free(rsrcVal);
        }
    }
    return buffer;
}


PRBool
nsInstallExecute::CanUninstall()
{
    return PR_FALSE;
}

PRBool
nsInstallExecute::RegisterPackageNode()
{
    return PR_FALSE;
}

