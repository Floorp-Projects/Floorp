/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MODUTIL_H
#define MODUTIL_H

#include <stdio.h>
#include <string.h>

#include <prio.h>
#include <prprf.h>
#include <prinit.h>
#include <prlock.h>
#include <prmem.h>
#include <plarena.h>

#include "seccomon.h"
#include "secmod.h"
#include "secutil.h"

#include "error.h"

Error LoadMechanismList(void);
Error FipsMode(char *arg);
Error ChkFipsMode(char *arg);
Error AddModule(char *moduleName, char *libFile, char *ciphers,
      char *mechanisms, char* modparms);
Error DeleteModule(char *moduleName);
Error ListModule(char *moduleName);
Error ListModules();
Error ChangePW(char *tokenName, char *pwFile, char *newpwFile);
Error EnableModule(char *moduleName, char *slotName, PRBool enable);
Error RawAddModule(char *dbmodulespec, char *modulespec);
Error RawListModule(char *modulespec);
Error SetDefaultModule(char *moduleName, char *slotName, char *mechanisms);
Error UnsetDefaultModule(char *moduleName, char *slotName, char *mechanisms);
void out_of_memory(void);

#endif /*MODUTIL_H*/
