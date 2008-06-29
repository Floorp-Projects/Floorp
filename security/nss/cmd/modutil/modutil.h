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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MODUTIL_H
#define MODUTIL_H

#include <stdio.h>
#include <prio.h>
#include <prprf.h>
#include <prinit.h>
#include <prmem.h>
#include <plarena.h>
#include <string.h>
#include <seccomon.h>
#include <secmod.h>
#include <secutil.h>

#include <prlock.h>

#include "error.h"

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
