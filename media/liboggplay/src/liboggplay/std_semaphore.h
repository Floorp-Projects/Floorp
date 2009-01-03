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
 * The Initial Developer of the Original Code is
 * CSIRO
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Shane Stephens, Michael Martin
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

#ifndef _STD_SEMAPHORE_H
#define _STD_SEMAPHORE_H
#if defined(linux) || defined(SOLARIS)
#include <semaphore.h>
#define SEM_CREATE(p,s) sem_init(&(p), 1, s)
#define SEM_SIGNAL(p)   sem_post(&(p))
#define SEM_WAIT(p)     sem_wait(&(p))
#define SEM_CLOSE(p)    sem_destroy(&(p))
typedef sem_t           semaphore;
#elif defined(WIN32)
#include <windows.h>
#define SEM_CREATE(p,s) p = CreateSemaphore(NULL, (long)(s), (long)(s), NULL)
#define SEM_SIGNAL(p)   ReleaseSemaphore(p, 1, NULL)
#define SEM_WAIT(p)     WaitForSingleObject(p, INFINITE)
#define SEM_TEST(p,s)   p = WaitForSingleObject(s, 0)
#define SEM_CLOSE(p)    CloseHandle(p)
typedef HANDLE          semaphore;
#elif defined(__APPLE__)
#include <Multiprocessing.h>
#define SEM_CREATE(p,s) MPCreateSemaphore(s, s, &(p))
#define SEM_SIGNAL(p)   MPSignalSemaphore(p)
#define SEM_WAIT(p)     MPWaitOnSemaphore(p, kDurationForever)
#define SEM_CLOSE(p)    MPDeleteSemaphore(p)
typedef MPSemaphoreID   semaphore;
#endif
#endif

/*
#if defined(XP_UX)
  sem_init(&(pointers->sem), 1, LIBOGGPLAY_BUFFER_SIZE);
#elif defined(XP_WIN)
  pointers->sem = CreateSemaphore(NULL, (long)LIBOGGPLAY_BUFFER_SIZE,
    (long)LIBOGGPLAY_BUFFER_SIZE, NULL);
#elif defined(XP_MACOSX)
  MPCreateSemaphore(LIBOGGPLAY_BUFFER_SIZE, LIBOGGPLAY_BUFFER_SIZE,
    &(pointers->sem));
#endif

#if defined(XP_UX)
  sem_post(&(ptrs->sem));
#elif defined(XP_WIN)
  ReleaseSemaphore(ptrs->sem, 1, NULL);
#elif defined(XP_MACOSX)
  MPSignalSemaphore(ptrs->sem);
#endif

#if defined(XP_UX)
    sem_wait(&(pointers->sem));
#elif defined(XP_WIN)
    WaitForSingleObject(pointers->sem, INFINITE);
#elif defined(XP_MACOSX)
    MPWaitOnSemaphore(pointers->sem, kDurationForever);
#endif

#if defined(XP_UX)
  sem_destroy(&(pointers->sem));
#elif defined(XP_WIN)
  CloseHandle(pointers->sem);
#elif defined(XP_MACOSX)
  MPDeleteSemaphore(pointers->sem);
#endif
*/

