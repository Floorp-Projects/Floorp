/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1998 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

// OS/2 plugin-loading code.

#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include "nsPluginsDir.h"
#include "prlink.h"
#include "plstr.h"
#include "prmem.h"
#include "nsSpecialSystemDirectory.h"
#include "nsPluginDefs.h"

/* Load a string stored as RCDATA in a resource segment */
/* Returned string needs to be PR_Free'd by caller */
static char *LoadRCDATAString( HMODULE hMod, ULONG resid)
{
   APIRET rc;
   ULONG  ulSize = 0;
   char  *string = 0;

   rc = DosQueryResourceSize( hMod, RT_RCDATA, resid, &ulSize);

   if( rc == NO_ERROR)
   {
      char *readOnlyString = 0;
      rc = DosGetResource( hMod, RT_RCDATA, resid, (void**) &readOnlyString);

      /* allow for 0-termination if user hasn't got it right */
      if( readOnlyString[ ulSize - 1] != '\0')
         ulSize++;

      if( rc == NO_ERROR)
      {
         /* copy string & zero-terminate */
         string = (char*) PR_Malloc( ulSize);
         memcpy( string, readOnlyString, ulSize - 1);
         string[ ulSize - 1] = '\0';

         DosFreeResource( readOnlyString);
      }
   }

   return string;
}

/* Take a string of the form "foo|bar|baz" and return an  */
/* array of pointers *into that string* to foo, bar, baz. */
/* There may be embedded nulls after | characters, thanks */
/* to the way the resource compiler works.  Technically   */
/* the flat string ought to end with a double null, but   */
/* I'm not checking here.                                 */
/* The 'flat' string is altered, ['\0'/'|']               */
/* The array returned needs to be PR_Free'd.              */
/* Also return the size of the array!                     */
static char **BuildStringArray( char *aFlat, PRUint32 &aSize)
{
   char **array = 0;

   aSize = 1;
   /* make two passes through the string 'cos I'm lazy */
   char *c = aFlat;
   while( 0 != (c = PL_strchr( c, '|')))
   {
      /* skip past <= 1 null char */
      c++; if( !*c) c++;
      aSize++;
   }

   PRInt32 index = 0;

   array = (char**) PR_Malloc( aSize * sizeof( char *));

   c = aFlat;

   for(;;) // do-while-do
   {
      array[ index++] = c;
      c = PL_strchr( c, '|');
      if( !c) break;
      *c++ = '\0';
   }

   return array;
}

// nsPluginsDir class

nsPluginsDir::nsPluginsDir(PRUint16 location)
{
   // XXX This isn't right for the embedded case, but it's as close
   //     as we can do right now.
   nsSpecialSystemDirectory appdir( nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
   appdir += "plugins";

   if( !appdir.Exists())
      appdir.CreateDirectory();

   *(nsFileSpec*)this = appdir;
}

nsPluginsDir::~nsPluginsDir()
{
}

PRBool nsPluginsDir::IsPluginFile( const nsFileSpec &fileSpec)
{
   PRBool rc = PR_FALSE;

   const char *leafname = fileSpec.GetLeafName();

   if( nsnull != leafname)
   {
      int len = strlen( leafname);
      if( len > 6 &&                 // np*.dll
          (0 == strnicmp( &(leafname[len - 4]), ".dll", 4)) &&
          (0 == strnicmp( leafname, "np", 2)))
      {
         rc = PR_TRUE;
      }

      delete [] leafname;
   }

   return rc;
}

// nsPluginFile implementation

nsPluginFile::nsPluginFile( const nsFileSpec &spec)
             : nsFileSpec(spec)
{}

nsPluginFile::~nsPluginFile()
{}

// Loads the plugin into memory using NSPR's shared-library loading
nsresult nsPluginFile::LoadPlugin( PRLibrary *&outLibrary)
{
   nsNSPRPath nsprpath( *this);
   outLibrary = PR_LoadLibrary( nsprpath);
	return outLibrary == nsnull ? NS_ERROR_FAILURE : NS_OK;
}

// Obtains all of the information currently available for this plugin.
nsresult nsPluginFile::GetPluginInfo( nsPluginInfo &info)
{
   nsresult   rc = NS_ERROR_FAILURE;
   nsNSPRPath nsprpath( *this);
   HMODULE    hPlug = 0; // Need a HMODULE to query resource statements
   char       failure[ CCHMAXPATH] = "";
   APIRET     ret;

   const char *pszNative = nsprpath;
   ret = DosLoadModule( failure, CCHMAXPATH, pszNative, &hPlug);
 
   while( ret == NO_ERROR)
   {
      info.fPluginInfoSize = sizeof( nsPluginInfo);

      char *leaf = GetLeafName();
      info.fName = PL_strdup( leaf);
      delete [] leaf;

      // get description (doesn't matter if it's missing)...
      info.fDescription = LoadRCDATAString( hPlug, NS_INFO_FileDescription);

      PRUint32 variants = 0;

      // ...mime types...
      info.fMimeType = LoadRCDATAString( hPlug, NS_INFO_MIMEType);
      if( nsnull == info.fMimeType) break;
      info.fMimeTypeArray = BuildStringArray( info.fMimeType, variants);
      if( info.fMimeTypeArray == nsnull) break;

      // (other lists must be same length as this one)
      info.fVariantCount = variants;

      // ...bizarre `description' thingy...
      info.fMimeDescription = LoadRCDATAString( hPlug, NS_INFO_FileOpenName);
      if( nsnull == info.fMimeDescription) break;
      info.fMimeDescriptionArray =
                          BuildStringArray( info.fMimeDescription, variants);
      if( nsnull == info.fMimeDescriptionArray) break;
      if( variants != info.fVariantCount) break;

      // ...file extensions...
      info.fExtensions = LoadRCDATAString( hPlug, NS_INFO_FileExtents);
      if( nsnull == info.fExtensions) break;
      info.fExtensionArray = BuildStringArray( info.fExtensions, variants);
      if( nsnull == info.fExtensionArray) break;
      if( variants != info.fVariantCount) break;

      rc = NS_OK;
      break;
   }

   if( 0 != hPlug)
      DosFreeModule( hPlug);

   return rc;
}

nsresult nsPluginFile::FreePluginInfo(nsPluginInfo& info)
{
  return NS_OK;
}
