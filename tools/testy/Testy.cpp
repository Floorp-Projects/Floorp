/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "prlink.h"
#include "prio.h"
#include "plstr.h"

#include "TestySupport.h"

#define LOG(args) printf args

#ifdef XP_WIN
static const char kPathSep = '\\';
#else
static const char kPathSep = '/';
#endif
static const char kTestsDirectory[] = "tch_tests";

typedef int (* EntryPoint)(void);
static const char kInitMethod[]     = "Testy_Init";
static const char kTestMethod[]     = "Testy_RunTest";
static const char kShutdownMethod[] = "Testy_Shutdown";

static void
ProcessModule(const char *modulesDir, const char *fileName)
{
    int dLen = strlen(modulesDir);
    int fLen = strlen(fileName);

    char *buf = (char *) malloc(dLen + 1 + fLen + 1);
    memcpy(buf, modulesDir, dLen);
    buf[dLen] = kPathSep;
    memcpy(buf + dLen + 1, fileName, fLen);
    buf[dLen + 1 + fLen] = '\0';

    PRLibrary *lib = PR_LoadLibrary(buf);
    if (lib) {
        EntryPoint initFunc     = (EntryPoint) PR_FindFunctionSymbol(lib, kInitMethod);
        EntryPoint testFunc     = (EntryPoint) PR_FindFunctionSymbol(lib, kTestMethod);
        EntryPoint shutdownFunc = (EntryPoint) PR_FindFunctionSymbol(lib, kShutdownMethod);

        if (testFunc) {
            int rv = 0;
            if (initFunc)
                rv = initFunc();
            // don't run test case if init fails.
            if (rv == 0)
                 testFunc();
            if (shutdownFunc)
                shutdownFunc();
        }
        PR_UnloadLibrary(lib);
    }

    free(buf);
}

static void
RunTests(const char *exePath)
{
    if (!(exePath && *exePath))
        return;

    //
    // load test modules
    //
    char *p = strrchr(exePath, kPathSep);
    if (p == NULL) {
        LOG(("unexpected exe path\n"));
        return;
    }

    int baseLen = p - exePath;
    int finalLen = baseLen + 1 + sizeof(kTestsDirectory);

    // build full path to ipc modules
    char *modulesDir = (char*) malloc(finalLen);
    memcpy(modulesDir, exePath, baseLen);
    modulesDir[baseLen] = kPathSep;
    memcpy(modulesDir + baseLen + 1, kTestsDirectory, sizeof(kTestsDirectory));

    LOG(("loading libraries in %s\n", modulesDir));
    // 
    // scan directory for IPC modules
    //
    PRDir *dir = PR_OpenDir(modulesDir);
    if (dir) {
        PRDirEntry *ent;
        while ((ent = PR_ReadDir(dir, PR_SKIP_BOTH)) != NULL) {
            // 
            // locate extension, and check if dynamic library
            //
            char *p = strrchr(ent->name, '.');
            if (p && PL_strcasecmp(p, MOZ_DLL_SUFFIX) == 0)
                ProcessModule(modulesDir, ent->name);
        }
        PR_CloseDir(dir);
    }

    free(modulesDir);
}

int main(int argc, char **argv)
{
  Testy_LogInit("tch.log");

  RunTests(argv[0]);

  Testy_LogShutdown();
  return 0;
}
