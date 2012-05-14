/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/*
 *
 * Test atomic stack operations
 *		
 *		Two stacks are created and threads add data items (each containing
 *		one of the first n integers) to the first stack, remove data items
 *		from the first stack and add them to the second stack. The primordial
 *		thread compares the sum of the first n integers to the sum of the
 *		integers in the data items in the second stack. The test succeeds if
 *		they are equal.
 */
 
#include "nspr.h"
#include "plgetopt.h"

typedef struct _DataRecord {
	PRInt32	data;
	PRStackElem	link;
} DataRecord;

#define RECORD_LINK_PTR(lp) ((DataRecord*) ((char*) (lp) - offsetof(DataRecord,link)))

#define MAX_THREAD_CNT		100
#define DEFAULT_THREAD_CNT	4
#define DEFAULT_DATA_CNT	100
#define DEFAULT_LOOP_CNT	10000

/*
 * sum of the first n numbers using the formula n*(n+1)/2
 */
#define SUM_OF_NUMBERS(n) ((n & 1) ? (((n + 1)/2) * n) : ((n/2) * (n+1)))

typedef struct stack_data {
	PRStack		*list1;
	PRStack		*list2;
	PRInt32		initial_data_value;
	PRInt32		data_cnt;
	PRInt32		loops;
} stack_data;

static void stackop(void *arg);

static int _debug_on;

PRFileDesc  *output;
PRFileDesc  *errhandle;

int main(int argc, char **argv)
{
#if !(defined(SYMBIAN) && defined(__WINS__))
    PRInt32 rv, cnt, sum;
	DataRecord	*Item;
	PRStack		*list1, *list2;
	PRStackElem	*node;
	PRStatus rc;

	PRInt32 thread_cnt = DEFAULT_THREAD_CNT;
	PRInt32 data_cnt = DEFAULT_DATA_CNT;
	PRInt32 loops = DEFAULT_LOOP_CNT;
	PRThread **threads;
	stack_data *thread_args;

	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "dt:c:l:");

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
        case 'c':  /* data count */
            data_cnt = atoi(opt->value);
            break;
        case 'l':  /* loop count */
            loops = atoi(opt->value);
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);

	PR_SetConcurrency(4);

    output = PR_GetSpecialFD(PR_StandardOutput);
    errhandle = PR_GetSpecialFD(PR_StandardError);
	list1 = PR_CreateStack("Stack_1");
	if (list1 == NULL) {
		PR_fprintf(errhandle, "PR_CreateStack failed - error %d\n",
								PR_GetError());
		return 1;
	}

	list2 = PR_CreateStack("Stack_2");
	if (list2 == NULL) {
		PR_fprintf(errhandle, "PR_CreateStack failed - error %d\n",
								PR_GetError());
		return 1;
	}


	threads = (PRThread**) PR_CALLOC(sizeof(PRThread*) * thread_cnt);
	thread_args = (stack_data *) PR_CALLOC(sizeof(stack_data) * thread_cnt);

	if (_debug_on)
		PR_fprintf(output,"%s: thread_cnt = %d data_cnt = %d\n", argv[0],
							thread_cnt, data_cnt);
	for(cnt = 0; cnt < thread_cnt; cnt++) {
		PRThreadScope scope;

		thread_args[cnt].list1 = list1;
		thread_args[cnt].list2 = list2;
		thread_args[cnt].loops = loops;
		thread_args[cnt].data_cnt = data_cnt;	
		thread_args[cnt].initial_data_value = 1 + cnt * data_cnt;

		if (cnt & 1)
			scope = PR_GLOBAL_THREAD;
		else
			scope = PR_LOCAL_THREAD;


		threads[cnt] = PR_CreateThread(PR_USER_THREAD,
						  stackop, &thread_args[cnt],
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

	node = PR_StackPop(list1);
	/*
	 * list1 should be empty
	 */
	if (node != NULL) {
		PR_fprintf(errhandle, "Error - Stack 1 not empty\n");
		PR_ASSERT(node == NULL);
		PR_ProcessExit(4);
	}

	cnt = data_cnt * thread_cnt;
	sum = 0;
	while (cnt-- > 0) {
		node = PR_StackPop(list2);
		/*
		 * There should be at least 'cnt' number of records
		 */
		if (node == NULL) {
			PR_fprintf(errhandle, "Error - PR_StackPop returned NULL\n");
			PR_ProcessExit(3);
		}
		Item = RECORD_LINK_PTR(node);
		sum += Item->data;
	}
	node = PR_StackPop(list2);
	/*
	 * there should be exactly 'cnt' number of records
	 */
	if (node != NULL) {
		PR_fprintf(errhandle, "Error - Stack 2 not empty\n");
		PR_ASSERT(node == NULL);
		PR_ProcessExit(4);
	}
	PR_DELETE(threads);
	PR_DELETE(thread_args);

	PR_DestroyStack(list1);
	PR_DestroyStack(list2);

	if (sum == SUM_OF_NUMBERS(data_cnt * thread_cnt)) {
		PR_fprintf(output, "%s successful\n", argv[0]);
		PR_fprintf(output, "\t\tsum = 0x%x, expected = 0x%x\n", sum,
							SUM_OF_NUMBERS(thread_cnt * data_cnt));
		return 0;
	} else {
		PR_fprintf(output, "%s failed: sum = 0x%x, expected = 0x%x\n",
							argv[0], sum,
								SUM_OF_NUMBERS(data_cnt * thread_cnt));
		return 2;
	}
#endif
}

static void stackop(void *thread_arg)
{
    PRInt32 val, cnt, index, loops;
	DataRecord	*Items, *Item;
	PRStack		*list1, *list2;
	PRStackElem	*node;
	stack_data *arg = (stack_data *) thread_arg;

	val = arg->initial_data_value;
	cnt = arg->data_cnt;
	loops = arg->loops;
	list1 = arg->list1;
	list2 = arg->list2;

	/*
	 * allocate memory for the data records
	 */
	Items = (DataRecord *) PR_CALLOC(sizeof(DataRecord) * cnt);
	PR_ASSERT(Items != NULL);
	index = 0;

	if (_debug_on)
		PR_fprintf(output,
		"Thread[0x%x] init_val = %d cnt = %d data1 = 0x%x datan = 0x%x\n",
				PR_GetCurrentThread(), val, cnt, &Items[0], &Items[cnt-1]);


	/*
	 * add the data records to list1
	 */
	while (cnt-- > 0) {
		Items[index].data = val++;
		PR_StackPush(list1, &Items[index].link);
		index++;
	}

	/*
	 * pop data records from list1 and add them back to list1
	 * generates contention for the stack accesses
	 */
	while (loops-- > 0) {
		cnt = arg->data_cnt;
		while (cnt-- > 0) {
			node = PR_StackPop(list1);
			if (node == NULL) {
				PR_fprintf(errhandle, "Error - PR_StackPop returned NULL\n");
				PR_ASSERT(node != NULL);
				PR_ProcessExit(3);
			}
			PR_StackPush(list1, node);
		}
	}
	/*
	 * remove the data records from list1 and add them to list2
	 */
	cnt = arg->data_cnt;
	while (cnt-- > 0) {
		node = PR_StackPop(list1);
		if (node == NULL) {
			PR_fprintf(errhandle, "Error - PR_StackPop returned NULL\n");
			PR_ASSERT(node != NULL);
			PR_ProcessExit(3);
		}
		PR_StackPush(list2, node);
	}
	if (_debug_on)
		PR_fprintf(output,
		"Thread[0x%x] init_val = %d cnt = %d exiting\n",
				PR_GetCurrentThread(), val, cnt);

}

