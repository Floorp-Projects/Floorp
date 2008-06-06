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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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


/*
 *
 * RWLock tests
 *
 *	Several threads are created to access and modify data arrays using
 * 	PRRWLocks for synchronization. Two data arrays, array_A and array_B, are
 *	initialized with random data and a third array, array_C, is initialized
 *	with the sum of the first 2 arrays.
 *
 *	Each one of the threads acquires a read lock to verify that the sum of
 *	the arrays A and B is equal to array C, and acquires a write lock to
 *	consistently update arrays A and B so that their is equal to array C.
 *		
 */
 
#include "nspr.h"
#include "plgetopt.h"
#include "prrwlock.h"

static int _debug_on;
static void rwtest(void *args);
static PRInt32 *array_A,*array_B,*array_C;
static void update_array(void);
static void check_array(void);

typedef struct thread_args {
	PRRWLock	*rwlock;
	PRInt32		loop_cnt;
} thread_args;

PRFileDesc  *output;
PRFileDesc  *errhandle;

#define	DEFAULT_THREAD_CNT	4
#define	DEFAULT_LOOP_CNT	100
#define	TEST_ARRAY_SIZE		100

PRIntn main(PRIntn argc, char **argv)
{
    PRInt32 cnt;
	PRStatus rc;
	PRInt32 i;

	PRInt32 thread_cnt = DEFAULT_THREAD_CNT;
	PRInt32 loop_cnt = DEFAULT_LOOP_CNT;
	PRThread **threads;
	thread_args *params;
	PRRWLock	*rwlock1;

	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "dt:c:");

	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
			_debug_on = 1;
            break;
        case 't':  /* thread count */
            thread_cnt = atoi(opt->value);
            break;
        case 'c':  /* loop count */
            loop_cnt = atoi(opt->value);
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);

	PR_SetConcurrency(4);

    output = PR_GetSpecialFD(PR_StandardOutput);
    errhandle = PR_GetSpecialFD(PR_StandardError);

	rwlock1 = PR_NewRWLock(0,"Lock 1");
	if (rwlock1 == NULL) {
		PR_fprintf(errhandle, "PR_NewRWLock failed - error %d\n",
								PR_GetError());
		return 1;
	}

	threads = (PRThread**) PR_CALLOC(sizeof(PRThread*) * thread_cnt);
	params = (thread_args *) PR_CALLOC(sizeof(thread_args) * thread_cnt);

	/*
	 * allocate and initialize data arrays
	 */
	array_A =(PRInt32 *) PR_MALLOC(sizeof(PRInt32) * TEST_ARRAY_SIZE);
	array_B =(PRInt32 *) PR_MALLOC(sizeof(PRInt32) * TEST_ARRAY_SIZE);
	array_C =(PRInt32 *) PR_MALLOC(sizeof(PRInt32) * TEST_ARRAY_SIZE);
	cnt = 0;
	for (i=0; i < TEST_ARRAY_SIZE;i++) {
		array_A[i] = cnt++;
		array_B[i] = cnt++;
		array_C[i] = array_A[i] + array_B[i];
	}

	if (_debug_on)
		PR_fprintf(output,"%s: thread_cnt = %d loop_cnt = %d\n", argv[0],
							thread_cnt, loop_cnt);
	for(cnt = 0; cnt < thread_cnt; cnt++) {
		PRThreadScope scope;

		params[cnt].rwlock = rwlock1;
		params[cnt].loop_cnt = loop_cnt;

		/*
		 * create LOCAL and GLOBAL threads alternately
		 */
		if (cnt & 1)
			scope = PR_LOCAL_THREAD;
		else
			scope = PR_GLOBAL_THREAD;

		threads[cnt] = PR_CreateThread(PR_USER_THREAD,
						  rwtest, &params[cnt],
						  PR_PRIORITY_NORMAL,
						  scope,
						  PR_JOINABLE_THREAD,
						  0);
		if (threads[cnt] == NULL) {
			PR_fprintf(errhandle, "PR_CreateThread failed - error %d\n",
								PR_GetError());
			PR_ProcessExit(2);
		}
		if (_debug_on)
			PR_fprintf(output,"%s: created thread = 0x%x\n", argv[0],
										threads[cnt]);
	}

	for(cnt = 0; cnt < thread_cnt; cnt++) {
    	rc = PR_JoinThread(threads[cnt]);
		PR_ASSERT(rc == PR_SUCCESS);

	}

	PR_DELETE(threads);
	PR_DELETE(params);

	PR_DELETE(array_A);	
	PR_DELETE(array_B);	
	PR_DELETE(array_C);	

	PR_DestroyRWLock(rwlock1);

	
	printf("PASS\n");
	return 0;
}

static void rwtest(void *args)
{
    PRInt32 index;
	thread_args *arg = (thread_args *) args;


	for (index = 0; index < arg->loop_cnt; index++) {

		/*
		 * verify sum, update arrays and verify sum again
		 */

		PR_RWLock_Rlock(arg->rwlock);
		check_array();
		PR_RWLock_Unlock(arg->rwlock);

		PR_RWLock_Wlock(arg->rwlock);
		update_array();
		PR_RWLock_Unlock(arg->rwlock);

		PR_RWLock_Rlock(arg->rwlock);
		check_array();
		PR_RWLock_Unlock(arg->rwlock);
	}
	if (_debug_on)
		PR_fprintf(output,
		"Thread[0x%x] lock = 0x%x exiting\n",
				PR_GetCurrentThread(), arg->rwlock);

}

static void check_array(void)
{
PRInt32 i;

	for (i=0; i < TEST_ARRAY_SIZE;i++)
		if (array_C[i] != (array_A[i] + array_B[i])) {
			PR_fprintf(output, "Error - data check failed\n");
			PR_ProcessExit(1);
		}
}

static void update_array(void)
{
PRInt32 i;

	for (i=0; i < TEST_ARRAY_SIZE;i++) {
		array_A[i] += i;
		array_B[i] -= i;
	}
}
