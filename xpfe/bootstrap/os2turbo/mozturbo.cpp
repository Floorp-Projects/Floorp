 /* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
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
 * The Original Code is OS/2 specific turbo mode
 *
 * The Initial Developer of the Original Code is IBM Corporation. 
 * Portions created by IBM Corporation are Copyright (C) 2002
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corp.
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

/*******************************************************************************
This program implements a module preloader for the OS/2 version of the Mozilla
Web Browser.

The way this is implemented is by loading each DLL using DosLoadModule and then
queying the first ordinal (entry point) using DosQueryProcAddr. This entry point
is then accessed so that its memory becomes paged in and resident in memory.
Once this is done, the program suspends execution by waiting on a named
semaphore so the modules are held in memory.

The list of module names was determined by loading Mozilla and then
seeing which DLLs were in use at that time.
*******************************************************************************/

#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>
#include <stdio.h>
#include <string.h>

/* Older versions of the toolkit, as well as GCC do not have this - from bsedos.h */
extern "C" {
   APIRET APIENTRY  DosQueryModFromEIP(HMODULE *phMod,
                                        ULONG *pObjNum,
                                        ULONG BuffLen,
                                        PCHAR pBuff,
                                        ULONG *pOffset,
                                        ULONG Address);
}

/* BIN directory */
char *bindir[] = {
 "GKGFX.DLL",
 "JSJ.DLL",
 "MOZJS.DLL",
 "MOZZ.DLL",
 "MSGBSUTL.DLL",
 "NSPR4.DLL",
 "PLC4.DLL",
 "PLDS4.DLL",
 "XPCOM.DLL",
 "XPCOMCOR.DLL",
 "XPCOMCT.DLL",
 0
 };

/* COMPONENTS directory */
char *compdir[] = {
 "APPCOMPS.DLL",
 "APPSHELL.DLL",
 "CAPS.DLL",
 "CHROME.DLL",
 "COOKIE.DLL",
 "DOCSHELL.DLL",
 "EDITOR.DLL",
 "EMBEDCMP.DLL",
// "GFX_OS2.DLL", Can't load GFX_OS2.DLL because it loads PMWINX.DLL which horks SHUTDOWN */
 "GKLAYOUT.DLL",
 "GKPARSER.DLL",
 "GKPLUGIN.DLL",
 "I18N.DLL",
 "IMGLIB2.DLL",
 "JAR50.DLL",
 "MAILNEWS.DLL",
 "MORK.DLL",
 "MOZUCONV.DLL",
 "MSGNEWS.DLL",
 "NECKO.DLL",
 "OJI.DLL",
 "PIPBOOT.DLL",
 "PREF.DLL",
 "PROFILE.DLL",
 "RDF.DLL",
 "TXMGR.DLL",
 "TYPAHEAD.DLL",
 "WDGTOS2.DLL",
 "WEBBRWSR.DLL",
 "XPCOMCTC.DLL",
 "XPCONECT.DLL",
 0,
 };

#define SEMNAME "\\SEM32\\MOZTURBO\\MOZTURBO"

void ForceModuleLoad(HMODULE hmodule);

int main(int argc, char *argv[]) {

  int do_load,do_unload,do_help,do_path;
  do_load=do_unload=do_help=do_path=0;

  char basepath[CCHMAXPATH];

  if (argc == 1)
    do_help = 1;
  else {
    for (int i=1; i < argc; i++) {
      if (strnicmp(argv[i],"-l", 2) == 0)
        do_load = 1;
      else if (strnicmp(argv[i],"-u", 2) == 0)
        do_unload = 1;
      else if (strnicmp(argv[i],"-h", 2) == 0) 
        do_help = 1;
      else if (strnicmp(argv[i],"-?", 2) == 0)
        do_help = 1;
      else if (strnicmp(argv[i],"-p", 2) == 0) {
        if (argc > i+1) {
          strcpy(basepath, argv[i+1]);
          if (basepath[strlen(basepath)] !='\\') {
            strcat(basepath, "\\");
          }
        do_path = 1;
        } else {
          do_help = 1;
        }
      }
    }
  }


  if (do_help) {
    printf("Mozilla for OS/2 preloader\n"\
           "\n"\
           "Usage: %s [-h] [-l | -u] [-p path]\n"\ 
           "       -h display this help\n"\ 
           "       -l load modules\n"\ 
           "       -u unload modules\n"\ 
           "       -p specify fully qualified path to directory where EXE is located\n", argv[0]);
    return(1);
  }

  if (do_unload) {
    HEV hev = NULLHANDLE;
    if (DosOpenEventSem(SEMNAME, &hev) == NO_ERROR) {
      if (DosPostEventSem(hev) == NO_ERROR) {
        if (DosCloseEventSem(hev) == NO_ERROR) {
          return(0);
        }
      }
    }
    printf("Mozilla for OS/2 preloader is not running\n");
    return(1);
  }

  if (do_path == 0) {
    /* Get the name of this EXE and use its location as the path */
    HMODULE hmodule;
    DosQueryModFromEIP(&hmodule, NULL, 0, NULL, NULL, (ULONG)ForceModuleLoad);
    DosQueryModuleName(hmodule, CCHMAXPATH, basepath);
    char *pchar = strrchr(basepath, '\\');
    pchar++;
    *pchar = '\0';
  }

  if (do_load) {
    ULONG ulCurMaxFH;
    LONG ulReqFH = 40;
    DosSetRelMaxFH(&ulReqFH, &ulCurMaxFH);

    HEV hev;
    if (DosCreateEventSem(SEMNAME, &hev, DC_SEM_SHARED, FALSE) != NO_ERROR) {
      printf("Mozilla for OS/2 preloader is already running\n");
      return(1);
    }

    /* Add directory where EXE is located to LIBPATH */
    DosSetExtLIBPATH(basepath, BEGIN_LIBPATH);

    /* loop through list loading named modules */
    char filepath[CCHMAXPATH];
    HMODULE hmod;

    int i = 0, nummodules = 0;
    while (bindir[i]) {
      strcpy(filepath,basepath);
      strcat(filepath,bindir[i]);
   
      if (DosLoadModule(NULL, 0, filepath, &hmod) == NO_ERROR) {
        ForceModuleLoad(hmod);
        nummodules++;
      }
      i++;
    }

    i = 0;
    while (compdir[i]) {
      strcpy(filepath, basepath);
      strcat(filepath, "COMPONENTS\\");
      strcat(filepath, compdir[i]);

      if (DosLoadModule(NULL, 0, filepath, &hmod) == NO_ERROR) {
        ForceModuleLoad(hmod);
        nummodules++;
      }
      i++;
    }
   
    if (nummodules > 0) {
      if (DosWaitEventSem(hev, SEM_INDEFINITE_WAIT) != NO_ERROR) {
        printf("DosWaitEventSem failed\n");
        return(1);
      }

      if (DosCloseEventSem(hev) != NO_ERROR) {
        printf("DosCloseEventSem failed\n");
        return(1);
      }
    } else {
      printf("No modules available to load\n");
    }
  }

 return(0);
}

/* This function forces a module load by accessing the code pointed */
/* to by the first entry point in a module */
void ForceModuleLoad(HMODULE hmodule)
{
  /* DosQueryMem */
  unsigned long memsize=0;
  unsigned long memend=0;
  unsigned long memflags=0;
  /* DosQueryProcAddr */
  PFN modaddr;

  volatile unsigned char cpybuf;
  unsigned int base=0;
  unsigned char* baseptr=0;

  if (DosQueryProcAddr(hmodule,1,0,&modaddr) == NO_ERROR) {
    /* calc 64K aligned addr previous to entry point */
    base=(( (unsigned long)modaddr) & 0xFFFF0000);
   
    /* get size and flags for this memory area */
    memsize=0x0fffffff;
    DosQueryMem((void*)base,&memsize,&memflags);
   
    /* if not first page of object, back off addr and retry */
    while (memflags < PAG_BASE) {
      base=base - PAG_BASE;
      memsize=0x0fffffff;
      DosQueryMem((void*)base,&memsize,&memflags);
    }
  
    /* finally, now loop through object pages, force page-in */
    memend=base+memsize;
    while(base<memend) {
      baseptr=(unsigned char*)base;
      cpybuf=*baseptr;
      base+=4096;
    }
  }
}

