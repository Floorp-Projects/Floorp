/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
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
 * The Original Code is SniffURI.
 *
 * The Initial Developer of the Original Code is
 * Erik van der Poel <erik@vanderpoel.org>.
 * Portions created by the Initial Developer are Copyright (C) 1998-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

#if HAVE_LIBPTHREAD
typedef pthread_t Thread;
#endif

#if HAVE_LIBTHREAD
typedef thread_t Thread;
#endif

#if WINDOWS
typedef HANDLE Thread;
#endif

void threadCancel(Thread thr);
void threadCondBroadcast(void);
void threadCondSignal(void);
void threadCondWait(void);
int threadCreate(Thread *thr, void *(*start)(void *), void *arg);
int threadInit(void);
void threadJoin(Thread thr);
void threadMutexLock(void);
void threadMutexUnlock(void);
void threadYield(void);
