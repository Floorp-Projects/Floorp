/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* Designed and original implementation by Gagan Saksena '98 */

#include "CacheStubs.h"
#include "nsCacheManager.h"
#include "nsDiskModule.h"
#include "nsMemModule.h"

#define CM nsCacheManager::GetInstance()

PRBool
CacheManager_Contains(const char* i_url)
{
    return CM->Entries();
}

PRUint32 
CacheManager_Entries()
{
    return CM->Entries();
}

PRBool
CacheManager_IsOffline(void)
{
    return CM->IsOffline();
}

void
CacheManager_Offline(PRBool bSet)
{
    CM->Offline(bSet);
}

PRUint32
CacheManager_WorstCaseTime(void)
{
    return CM->WorstCaseTime();
}

#define DM nsCacheManager::GetInstance()->GetDiskModule()

PRBool
DiskModule_Contains(const char* i_url)
{
    return DM->Contains(i_url);
}

PRUint32
DiskModule_Entries(void)
{
    return DM->Entries();
}

PRUint32
DiskModule_GetSize(void)
{
    return DM->Size();
}

PRBool
DiskModule_IsEnabled(void)
{
    return DM->IsEnabled();
}

PRBool
DiskModule_Remove(const char* i_url)
{
    return DM->Remove(i_url);
}

void
DiskModule_SetSize(PRUint32 i_Size)
{
    DM->Size(i_Size);
}

#define MM nsCacheManager::GetInstance()->GetMemModule()

PRBool
MemModule_Contains(const char* i_url)
{
    return MM->Contains(i_url);
}

PRUint32
MemModule_Entries(void)
{
    return MM->Entries();
}

PRUint32
MemModule_GetSize(void)
{
    return MM->Size();
}

PRBool
MemModule_IsEnabled(void)
{
    return MM->IsEnabled();
}

PRBool
MemModule_Remove(const char* i_url)
{
    return MM->Remove(i_url);
}

void
MemModule_SetSize(PRUint32 i_Size)
{
    MM->Size(i_Size);
}
