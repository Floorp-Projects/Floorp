/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   John Wolfe <wolfe@lobo.us>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include <stdio.h>

#include "nsXPCOM.h"
#include "nsXPCOMCIDInternal.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIServiceManager.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"

#include "nscore.h"
#include "nspr.h"
#include "prmon.h"

#include "nsITestProxy.h"

#include "nsIRunnable.h"
#include "nsIProxyObjectManager.h"
#include "nsIThreadPool.h"
#include "nsXPCOMCIDInternal.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"


//#include "prlog.h"
//#ifdef PR_LOGGING
//extern PRLogModuleInfo *sLog = PR_NewLogModule("Test");
//#define LOG(args) PR_LOG(sLog, PR_LOG_DEBUG, args)
//#else
//#define LOG(args) printf args
//#endif



/***************************************************************************/
/* nsTestXPCFoo                                                            */
/***************************************************************************/
nsTestXPCFoo::nsTestXPCFoo()
{
	NS_ADDREF_THIS();
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsTestXPCFoo, nsITestProxy)

NS_IMETHODIMP nsTestXPCFoo::Test(PRInt32 p1, PRInt32 p2, PRInt32 *_retval)
{
	LOG(("TEST: Thread (%d) Test Called successfully! Party on...\n", p1));
	*_retval = p1+p2;
	return NS_OK;
}


NS_IMETHODIMP nsTestXPCFoo::Test2(nsresult *_retval)
{
	LOG(("TEST: The quick brown netscape jumped over the old lazy ie..\n"));

	return NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test3(nsISupports *p1, nsISupports **p2, nsresult *_retval)
{
	if (p1 != nsnull)
	{
		nsresult r;
		nsITestProxy *test;

		p1->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);

		test->Test2(&r);
		PRInt32 a;
		test->Test(1, 2, &a);
		LOG(("TEST: \n1+2=%d\n",a));
	}

	*p2 = new nsTestXPCFoo();
	return NS_OK;
}




NS_IMETHODIMP nsTestXPCFoo::Test1_1(PRInt32 *p1, nsresult *_retval)
{
	LOG(("TEST: 1_1: [%d] => ",
	     *p1));

	*p1 = *p1 + *p1;

	LOG(("TEST: 1_1:                                          => [%d]\n",
	     *p1));

	*_retval = 1010;
	return 1010;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test1_2(PRInt64 *p1, nsresult *_retval)
{
	LOG(("TEST: 1_2: [%g] => ",
	     *p1));

	*p1 = *p1 + *p1;

	LOG(("TEST: 1_2:                                                    => [%g]\n",
	     *p1));

	*_retval = 1020;
	return 1020;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test1_3(float *p1, nsresult *_retval)
{
	LOG(("TEST: 1_3: [%lf] => ",
	     *p1));

	*p1 = *p1 + *p1;

	LOG(("TEST: 1_3:                                                    => [%lf]\n",
	     *p1));

	*_retval = 1030;
	return 1030;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test1_4(double *p1, nsresult *_retval)
{
	LOG(("TEST: 1_4: [%le] => ",
	     *p1));

	*p1 = *p1 + *p1;

	LOG(("TEST: 1_4:                                                    => [%le]\n",
	     *p1));

	*_retval = 1040;
	return 1040;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test1_5(nsISupports **p1, nsresult *_retval)
{
	*p1 = new nsTestXPCFoo();

	LOG(("TEST: 1_5:                                                    => [0x%08X]\n",
	     *p1));

	*_retval = 1050;
	return 1050;	//NS_OK;
}




NS_IMETHODIMP nsTestXPCFoo::Test2_1(PRInt32 *p1, PRInt32 *p2, nsresult *_retval)
{
	LOG(("TEST: 2_1: [%d, %d] => ",
	     *p1, *p2));

	*p1 = *p1 + *p2;

	*p2 += 1;

	LOG(("TEST: 2_1:                                          => [%d, %d]\n",
	     *p1, *p2));

	*_retval = 2010;
	return 2010;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test2_2(PRInt64 *p1, PRInt64 *p2, nsresult *_retval)
{
	LOG(("TEST: 2_2: [%g, %g] => ",
	     *p1, *p2));

	*p1 = *p1 + *p2;

	*p2 += 1;

	LOG(("TEST: 2_2:                                                    => [%g, %g]\n",
	     *p1, *p2));

	*_retval = 2020;
	return 2020;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test2_3(float *p1, float *p2, nsresult *_retval)
{
	LOG(("TEST: 2_3: [%lf, %lf] => ",
	     *p1, *p2));

	*p1 = *p1 + *p2;

	*p2 += 1.0f;

	LOG(("TEST: 2_3:                                                    => [%lf, %lf]\n",
	     *p1, *p2));

	*_retval = 2030;
	return 2030;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test2_4(double *p1, double *p2, nsresult *_retval)
{
	LOG(("TEST: 2_4: [%le, %le] => ",
	     *p1, *p2));

	*p1 = *p1 + *p2;

	*p2 += 1;

	LOG(("TEST: 2_4:                                                    => [%le, %le]\n",
	     *p1, *p2));

	*_retval = 2040;
	return 2040;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test2_5(nsISupports **p1, nsISupports **p2, nsresult *_retval)
{
	if (p1 != nsnull && *p1 != nsnull)
	{
		nsITestProxy *test;
		(*p1)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2;
		nsresult retval = NS_ERROR_BASE;
		q1 = 100, q2=200;
		LOG(("TEST: 2_5: 1: [%d, %d] => ",
		     q1, q2));
		test->Test2_1(&q1, &q2, &retval);
		LOG(("TEST: 2_5: 1:                                          => [%d, %d]\n",
		     q1, q2));
	}

	*p2 = new nsTestXPCFoo();

	LOG(("TEST: 2_5:                                                    => [0x%08X]\n",
	     *p2));

	*_retval = 2050;
	return 2050;	//NS_OK;
}





NS_IMETHODIMP nsTestXPCFoo::Test3_1(PRInt32 *p1, PRInt32 *p2, PRInt32 *p3, nsresult *_retval)
{
	LOG(("TEST: 3_1: [%d, %d, %d] => ",
	     *p1, *p2, *p3));

	*p1 = *p1 + *p2 + *p3;

	*p2 += 1;   *p3 += 1;

	LOG(("TEST: 3_1:                                          => [%d, %d, %d]\n",
	     *p1, *p2, *p3));

	*_retval = 3010;
	return 3010;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test3_2(PRInt64 *p1, PRInt64 *p2, PRInt64 *p3, nsresult *_retval)
{
	LOG(("TEST: 3_2: [%g, %g, %g] => ",
	     *p1, *p2, *p3));

	*p1 = *p1 + *p2 + *p3;

	*p2 += 1;   *p3 += 1;

	LOG(("TEST: 3_2:                                                    => [%g, %g, %g]\n",
	     *p1, *p2, *p3));

	*_retval = 3020;
	return 3020;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test3_3(float *p1, float *p2, float *p3, nsresult *_retval)
{
	LOG(("TEST: 3_3: [%lf, %lf, %lf] => ",
	     *p1, *p2, *p3));

	*p1 = *p1 + *p2 + *p3;

	*p2 += 1.0f;   *p3 += 1.0f;

	LOG(("TEST: 3_3:                                                    => [%lf, %lf, %lf]\n",
	     *p1, *p2, *p3));

	*_retval = 3030;
	return 3030;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test3_4(double *p1, double *p2, double *p3, nsresult *_retval)
{
	LOG(("TEST: 3_4: [%le, %le, %le] => ",
	     *p1, *p2, *p3));

	*p1 = *p1 + *p2 + *p3;

	*p2 += 1;   *p3 += 1;

	LOG(("TEST: 3_4:                                                    => [%le, %le, %le]\n",
	     *p1, *p2, *p3));

	*_retval = 3040;
	return 3040;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test3_5(nsISupports **p1, nsISupports **p2, nsISupports **p3, nsresult *_retval)
{
	if (p1 != nsnull && *p1 != nsnull)
	{
		nsITestProxy *test;
		(*p1)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2, q3;
		nsresult retval = NS_ERROR_BASE;
		q1 = 100, q2=200, q3=300;
		LOG(("TEST: 3_5: 1: [%d, %d, %d] => ",
		     q1, q2, q3));
		test->Test3_1(&q1, &q2, &q3, &retval);
		LOG(("TEST: 3_5: 1:                                          => [%d, %d, %d]\n",
		     q1, q2, q3));
	}

	if (p2 != nsnull && *p2 != nsnull)
	{
		nsITestProxy *test;
		(*p2)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt64 q1, q2, q3;
		nsresult retval = NS_ERROR_BASE;
		q1 = 1000, q2=2000, q3=3000;
		LOG(("TEST: 3_5: 2: [%g, %g, %g] => ",
		     q1, q2, q3));
		test->Test3_2(&q1, &q2, &q3, &retval);
		LOG(("TEST: 3_5: 2:                                                    => [%g, %g, %g]\n",
		     q1, q2, q3));
	}

	*p3 = new nsTestXPCFoo();

	LOG(("TEST: 3_5:                                                    => [0x%08X]\n",
	     *p3));

	*_retval = 3050;
	return 3050;	//NS_OK;
}





NS_IMETHODIMP nsTestXPCFoo::Test4_1(PRInt32 *p1, PRInt32 *p2, PRInt32 *p3, PRInt32 *p4, nsresult *_retval)
{
	LOG(("TEST: 4_1: [%d, %d, %d, %d] => ",
	     *p1, *p2, *p3, *p4));

	*p1 = *p1 + *p2 + *p3 + *p4;

	*p2 += 1;   *p3 += 1;   *p4 += 1;

	LOG(("TEST: 4_1:                                          => [%d, %d, %d, %d]\n",
	     *p1, *p2, *p3, *p4));

	*_retval = 4010;
	return 4010;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test4_2(PRInt64 *p1, PRInt64 *p2, PRInt64 *p3, PRInt64 *p4, nsresult *_retval)
{
	LOG(("TEST: 4_2: [%g, %g, %g, %g] => ",
	     *p1, *p2, *p3, *p4));

	*p1 = *p1 + *p2 + *p3 + *p4;

	*p2 += 1;   *p3 += 1;   *p4 += 1;

	LOG(("TEST: 4_2:                                                    => [%g, %g, %g, %g]\n",
	     *p1, *p2, *p3, *p4));

	*_retval = 4020;
	return 4020;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test4_3(float *p1, float *p2, float *p3, float *p4, nsresult *_retval)
{
	LOG(("TEST: 4_3: [%lf, %lf, %lf, %lf] => ",
	     *p1, *p2, *p3, *p4));

	     *p1 = *p1 + *p2 + *p3 + *p4;

	*p2 += 1.0f;   *p3 += 1.0f;   *p4 += 1.0f;

	LOG(("TEST: 4_3:                                                    => [%lf, %lf, %lf, %lf]\n",
	     *p1, *p2, *p3, *p4));

	*_retval = 4030;
	return 4030;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test4_4(double *p1, double *p2, double *p3, double *p4, nsresult *_retval)
{
	LOG(("TEST: 4_4: [%le, %le, %le, %le] => ",
	     *p1, *p2, *p3, *p4));

	*p1 = *p1 + *p2 + *p3 + *p4;

	*p2 += 1;   *p3 += 1;   *p4 += 1;

	LOG(("TEST: 4_4:                                                    => [%le, %le, %le, %le]\n",
	     *p1, *p2, *p3, *p4));

	*_retval = 4040;
	return 4040;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test4_5(nsISupports **p1, nsISupports **p2, nsISupports **p3, nsISupports **p4, nsresult *_retval)
{
	if (p1 != nsnull && *p1 != nsnull)
	{
		nsITestProxy *test;
		(*p1)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2, q3, q4;
		nsresult retval = NS_ERROR_BASE;
		q1 = 100, q2=200, q3=300, q4=400;
		LOG(("TEST: 4_5: 1: [%d, %d, %d, %d] => ",
		     q1, q2, q3, q4));
		test->Test4_1(&q1, &q2, &q3, &q4, &retval);
		LOG(("TEST: 4_5: 1:                                          => [%d, %d, %d, %d]\n",
		     q1, q2, q3, q4));
	}

	if (p2 != nsnull && *p2 != nsnull)
	{
		nsITestProxy *test;
		(*p2)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt64 q1, q2, q3, q4;
		nsresult retval = NS_ERROR_BASE;
		q1 = 1000, q2=2000, q3=3000, q4=4000;
		LOG(("TEST: 4_5: 2: [%g, %g, %g, %g] => ",
		     q1, q2, q3, q4));
		test->Test4_2(&q1, &q2, &q3, &q4, &retval);
		LOG(("TEST: 4_5: 2:                                                    => [%g, %g, %g, %g]\n",
		     q1, q2, q3, q4));
	}

	if (p3 != nsnull && *p3 != nsnull)
	{
		nsITestProxy *test;
		(*p3)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		float q1, q2, q3, q4;
		nsresult retval = NS_ERROR_BASE;
		q1 = 10000, q2=20000, q3=30000, q4=40000;
		LOG(("TEST: 4_5: 3: [%lf, %lf, %lf, %lf] => ",
		     q1, q2, q3, q4));
		test->Test4_3(&q1, &q2, &q3, &q4, &retval);
		LOG(("TEST: 4_5: 3:                                                    => [%lf, %lf, %lf, %lf]\n",
		     q1, q2, q3, q4));
	}
		
	*p4 = new nsTestXPCFoo();

	LOG(("TEST: 4_5:                                                    => [0x%08X]\n",
	     *p4));

	*_retval = 4050;
	return 4050;	//NS_OK;
}





NS_IMETHODIMP nsTestXPCFoo::Test5_1(PRInt32 *p1, PRInt32 *p2, PRInt32 *p3, PRInt32 *p4, PRInt32 *p5, nsresult *_retval)
{
	LOG(("TEST: 5_1: [%d, %d, %d, %d, %d] => ",
	     *p1, *p2, *p3, *p4, *p5));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;

	LOG(("TEST: 5_1:                                          => [%d, %d, %d, %d, %d]\n",
	     *p1, *p2, *p3, *p4, *p5));

	*_retval = 5010;
	return 5010;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test5_2(PRInt64 *p1, PRInt64 *p2, PRInt64 *p3, PRInt64 *p4, PRInt64 *p5, nsresult *_retval)
{
	LOG(("TEST: 5_2: [%g, %g, %g, %g, %g] => ",
	     *p1, *p2, *p3, *p4, *p5));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;

	LOG(("TEST: 5_2:                                                    => [%g, %g, %g, %g, %g]\n",
	     *p1, *p2, *p3, *p4, *p5));

	*_retval = 5020;
	return 5020;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test5_3(float *p1, float *p2, float *p3, float *p4, float *p5, nsresult *_retval)
{
	LOG(("TEST: 5_3: [%lf, %lf, %lf, %lf, %lf] => ",
	     *p1, *p2, *p3, *p4, *p5));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5;

	*p2 += 1.0f;   *p3 += 1.0f;   *p4 += 1.0f;   *p5 += 1.0;

	LOG(("TEST: 5_3:                                                    => [%lf, %lf, %lf, %lf, %lf]\n",
	     *p1, *p2, *p3, *p4, *p5));

	*_retval = 5030;
	return 5030;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test5_4(double *p1, double *p2, double *p3, double *p4, double *p5, nsresult *_retval)
{
	LOG(("TEST: 5_4: [%le, %le, %le, %le, %le] => ",
	     *p1, *p2, *p3, *p4, *p5));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;

	LOG(("TEST: 5_4:                                                    => [%le, %le, %le, %le, %le]\n",
	     *p1, *p2, *p3, *p4, *p5));

	*_retval = 5040;
	return 5040;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test5_5(nsISupports **p1, nsISupports **p2, nsISupports **p3, nsISupports **p4, nsISupports **p5, nsresult *_retval)
{
	if (p1 != nsnull && *p1 != nsnull)
	{
		nsITestProxy *test;
		(*p1)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2, q3, q4, q5;
		nsresult retval = NS_ERROR_BASE;
		q1 = 100, q2=200, q3=300, q4=400, q5=500;
		LOG(("TEST: 5_5: 1: [%d, %d, %d, %d, %d] => ",
		     q1, q2, q3, q4, q5));
		test->Test5_1(&q1, &q2, &q3, &q4, &q5, &retval);
		LOG(("TEST: 5_5: 1:                                          => [%d, %d, %d, %d, %d]\n",
		     q1, q2, q3, q4, q5));
	}

	if (p2 != nsnull && *p2 != nsnull)
	{
		nsITestProxy *test;
		(*p2)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt64 q1, q2, q3, q4, q5;
		nsresult retval = NS_ERROR_BASE;
		q1 = 1000, q2=2000, q3=3000, q4=4000, q5=5000;
		LOG(("TEST: 5_5: 2: [%g, %g, %g, %g, %g] => ",
		     q1, q2, q3, q4, q5));
		test->Test5_2(&q1, &q2, &q3, &q4, &q5, &retval);
		LOG(("TEST: 5_5: 2:                                                    => [%g, %g, %g, %g, %g]\n",
		     q1, q2, q3, q4, q5));
	}

	if (p3 != nsnull && *p3 != nsnull)
	{
		nsITestProxy *test;
		(*p3)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		float q1, q2, q3, q4, q5;
		nsresult retval = NS_ERROR_BASE;
		q1 = 10000, q2=20000, q3=30000, q4=40000, q5=50000;
		LOG(("TEST: 5_5: 3: [%lf, %lf, %lf, %lf, %lf] => ",
		     q1, q2, q3, q4, q5));
		test->Test5_3(&q1, &q2, &q3, &q4, &q5, &retval);
		LOG(("TEST: 5_5: 3:                                                    => [%lf, %lf, %lf, %lf, %lf]\n",
		     q1, q2, q3, q4, q5));
	}

	if (p4 != nsnull && *p4 != nsnull)
	{
		nsITestProxy *test;
		(*p4)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		double q1, q2, q3, q4, q5;
		nsresult retval = NS_ERROR_BASE;
		q1 = 10000, q2=20000, q3=30000, q4=40000, q5=50000;
		LOG(("TEST: 5_5: 4: [%le, %le, %le, %le, %le] => ",
		     q1, q2, q3, q4, q5));
		test->Test5_4(&q1, &q2, &q3, &q4, &q5, &retval);
		LOG(("TEST: 5_5: 4:                                                    => [%le, %le, %le, %le, %le]\n",
		     q1, q2, q3, q4, q5));
	}

	*p5 = new nsTestXPCFoo();

	LOG(("TEST: 5_5:                                                    => [0x%08X]\n",
	     *p5));

	*_retval = 5050;
	return 5050;	//NS_OK;
}





NS_IMETHODIMP nsTestXPCFoo::Test6_1(PRInt32 *p1, PRInt32 *p2, PRInt32 *p3, PRInt32 *p4, PRInt32 *p5, PRInt32 *p6, nsresult *_retval)
{
	LOG(("TEST: 6_1: [%d, %d, %d, %d, %d, %d] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;

	LOG(("TEST: 6_1:                                          => [%d, %d, %d, %d, %d, %d]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6));

	*_retval = 6010;
	return 6010;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test6_2(PRInt64 *p1, PRInt64 *p2, PRInt64 *p3, PRInt64 *p4, PRInt64 *p5, PRInt64 *p6, nsresult *_retval)
{
	LOG(("TEST: 6_2: [%g, %g, %g, %g, %g, %g] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;

	LOG(("TEST: 6_2:                                                    => [%g, %g, %g, %g, %g, %g]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6));

	*_retval = 6020;
	return 6020;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test6_3(float *p1, float *p2, float *p3, float *p4, float *p5, float *p6, nsresult *_retval)
{
	LOG(("TEST: 6_3: [%lf, %lf, %lf, %lf, %lf, %lf] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6;

	*p2 += 1.0f;   *p3 += 1.0f;   *p4 += 1.0f;   *p5 += 1.0f;
	*p6 += 1.0f;

	LOG(("TEST: 6_3:                                                    => [%lf, %lf, %lf, %lf, %lf, %lf]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6));

	*_retval = 6030;
	return 6030;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test6_4(double *p1, double *p2, double *p3, double *p4, double *p5, double *p6, nsresult *_retval)
{
	LOG(("TEST: 6_4: [%le, %le, %le, %le, %le, %le] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;

	LOG(("TEST: 6_4:                                                    => [%le, %le, %le, %le, %le, %le]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6));

	*_retval = 6040;
	return 6040;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test6_5(nsISupports **p1, nsISupports **p2, nsISupports **p3, nsISupports **p4, nsISupports **p5, nsISupports **p6, nsresult *_retval)
{
	if (p1 != nsnull && *p1 != nsnull)
	{
		nsITestProxy *test;
		(*p1)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2, q3, q4, q5, q6;
		nsresult retval = NS_ERROR_BASE;
		q1 = 100, q2=200, q3=300, q4=400, q5=500, q6=600;
		LOG(("TEST: 6_5: 1: [%d, %d, %d, %d, %d, %d] => ",
		     q1, q2, q3, q4, q5, q6));
		test->Test6_1(&q1, &q2, &q3, &q4, &q5, &q6, &retval);
		LOG(("TEST: 6_5: 1:                                          => [%d, %d, %d, %d, %d, %d]\n",
		     q1, q2, q3, q4, q5, q6));
	}

	if (p2 != nsnull && *p2 != nsnull)
	{
		nsITestProxy *test;
		(*p2)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt64 q1, q2, q3, q4, q5, q6;
		nsresult retval = NS_ERROR_BASE;
		q1 = 1000, q2=2000, q3=3000, q4=4000, q5=5000, q6=6000;
		LOG(("TEST: 6_5: 2: [%g, %g, %g, %g, %g, %g] => ",
		     q1, q2, q3, q4, q5, q6));
		test->Test6_2(&q1, &q2, &q3, &q4, &q5, &q6, &retval);
		LOG(("TEST: 6_5: 2:                                                    => [%g, %g, %g, %g, %g, %g]\n",
		     q1, q2, q3, q4, q5, q6));
	}

	if (p3 != nsnull && *p3 != nsnull)
	{
		nsITestProxy *test;
		(*p3)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		float q1, q2, q3, q4, q5, q6;
		nsresult retval = NS_ERROR_BASE;
		q1 = 10000, q2=20000, q3=30000, q4=40000, q5=50000, q6=60000;
		LOG(("TEST: 6_5: 3: [%lf, %lf, %lf, %lf, %lf, %lf] => ",
		     q1, q2, q3, q4, q5, q6));
		test->Test6_3(&q1, &q2, &q3, &q4, &q5, &q6, &retval);
		LOG(("TEST: 6_5: 3:                                                    => [%lf, %lf, %lf, %lf, %lf, %lf]\n",
		     q1, q2, q3, q4, q5, q6));
	}

	if (p4 != nsnull && *p4 != nsnull)
	{
		nsITestProxy *test;
		(*p4)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		double q1, q2, q3, q4, q5, q6;
		nsresult retval = NS_ERROR_BASE;
		q1 = 10000, q2=20000, q3=30000, q4=40000, q5=50000, q6=60000;
		LOG(("TEST: 6_5: 4: [%le, %le, %le, %le, %le, %le] => ",
		     q1, q2, q3, q4, q5, q6));
		test->Test6_4(&q1, &q2, &q3, &q4, &q5, &q6, &retval);
		LOG(("TEST: 6_5: 4:                                                    => [%le, %le, %le, %le, %le, %le]\n",
		     q1, q2, q3, q4, q5, q6));
	}

	if (p5 != nsnull && *p5 != nsnull)
	{
		nsITestProxy *test;
		(*p5)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2, q3, q4, q5, q6;
		nsresult retval = NS_ERROR_BASE;
		q1 = 150, q2=250, q3=350, q4=450, q5=550, q6=650;
		LOG(("TEST: 6_5: 5: [%d, %d, %d, %d, %d, %d] => ",
		     q1, q2, q3, q4, q5, q6));
		test->Test6_1(&q1, &q2, &q3, &q4, &q5, &q6, &retval);
		LOG(("TEST: 6_5: 5:                                          => [%d, %d, %d, %d, %d, %d]\n",
		     q1, q2, q3, q4, q5, q6));
	}

	*p6 = new nsTestXPCFoo();

	LOG(("TEST: 6_5:                                                    => [0x%08X]\n",
	     *p6));

	*_retval = 6050;
	return 6050;	//NS_OK;
}





NS_IMETHODIMP nsTestXPCFoo::Test7_1(PRInt32 *p1, PRInt32 *p2, PRInt32 *p3, PRInt32 *p4, PRInt32 *p5, PRInt32 *p6, PRInt32 *p7, nsresult *_retval)
{
	LOG(("TEST: 7_1: [%d, %d, %d, %d, %d, %d, %d] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;   *p7 += 1;

	LOG(("TEST: 7_1:                                          => [%d, %d, %d, %d, %d, %d, %d]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7));

	*_retval = 7010;
	return 7010;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test7_2(PRInt64 *p1, PRInt64 *p2, PRInt64 *p3, PRInt64 *p4, PRInt64 *p5, PRInt64 *p6, PRInt64 *p7, nsresult *_retval)
{
	LOG(("TEST: 7_2: [%g, %g, %g, %g, %g, %g, %g] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;   *p7 += 1;

	LOG(("TEST: 7_2:                                                    => [%g, %g, %g, %g, %g, %g, %g]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7));

	*_retval = 7020;
	return 7020;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test7_3(float *p1, float *p2, float *p3, float *p4, float *p5, float *p6, float *p7, nsresult *_retval)
{
	LOG(("TEST: 7_3: [%lf, %lf, %lf, %lf, %lf, %lf, %lf] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7;

	*p2 += 1.0f;   *p3 += 1.0f;   *p4 += 1.0f;   *p5 += 1.0f;
	*p6 += 1.0f;   *p7 += 1.0f;

	LOG(("TEST: 7_3:                                                    => [%lf, %lf, %lf, %lf, %lf, %lf, %lf]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7));

	*_retval = 7030;
	return 7030;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test7_4(double *p1, double *p2, double *p3, double *p4, double *p5, double *p6, double *p7, nsresult *_retval)
{
	LOG(("TEST: 7_4: [%le, %le, %le, %le, %le, %le, %le] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;   *p7 += 1;

	LOG(("TEST: 7_4:                                                    => [%le, %le, %le, %le, %le, %le, %le]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7));

	*_retval = 7040;
	return 7040;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test7_5(nsISupports **p1, nsISupports **p2, nsISupports **p3, nsISupports **p4, nsISupports **p5, nsISupports **p6, nsISupports **p7, nsresult *_retval)
{
	if (p1 != nsnull && *p1 != nsnull)
	{
		nsITestProxy *test;
		(*p1)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2, q3, q4, q5, q6, q7;
		nsresult retval = NS_ERROR_BASE;
		q1 = 100, q2=200, q3=300, q4=400, q5=500, q6=600, q7=700;
		LOG(("TEST: 7_5: 1: [%d, %d, %d, %d, %d, %d, %d] => ",
		     q1, q2, q3, q4, q5, q6, q7));
		test->Test7_1(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &retval);
		LOG(("TEST: 7_5: 1:                                          => [%d, %d, %d, %d, %d, %d, %d]\n",
		     q1, q2, q3, q4, q5, q6, q7));
	}

	if (p2 != nsnull && *p2 != nsnull)
	{
		nsITestProxy *test;
		(*p2)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt64 q1, q2, q3, q4, q5, q6, q7;
		nsresult retval = NS_ERROR_BASE;
		q1 = 1000, q2=2000, q3=3000, q4=4000, q5=5000, q6=6000, q7=7000;
		LOG(("TEST: 7_5: 2: [%g, %g, %g, %g, %g, %g, %g] => ",
		     q1, q2, q3, q4, q5, q6, q7));
		test->Test7_2(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &retval);
		LOG(("TEST: 7_5: 2:                                                    => [%g, %g, %g, %g, %g, %g, %g]\n",
		     q1, q2, q3, q4, q5, q6, q7));
	}

	if (p3 != nsnull && *p3 != nsnull)
	{
		nsITestProxy *test;
		(*p3)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		float q1, q2, q3, q4, q5, q6, q7;
		nsresult retval = NS_ERROR_BASE;
		q1 = 10000, q2=20000, q3=30000, q4=40000, q5=50000, q6=60000, q7=70000;
		LOG(("TEST: 7_5: 3: [%lf, %lf, %lf, %lf, %lf, %lf, %lf] => ",
		     q1, q2, q3, q4, q5, q6, q7));
		test->Test7_3(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &retval);
		LOG(("TEST: 7_5: 3:                                                    => [%lf, %lf, %lf, %lf, %lf, %lf, %lf]\n",
		     q1, q2, q3, q4, q5, q6, q7));
	}

	if (p4 != nsnull && *p4 != nsnull)
	{
		nsITestProxy *test;
		(*p4)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		double q1, q2, q3, q4, q5, q6, q7;
		nsresult retval = NS_ERROR_BASE;
		q1 = 10000, q2=20000, q3=30000, q4=40000, q5=50000, q6=60000, q7=70000;
		LOG(("TEST: 7_5: 4: [%le, %le, %le, %le, %le, %le, %le] => ",
		     q1, q2, q3, q4, q5, q6, q7));
		test->Test7_4(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &retval);
		LOG(("TEST: 7_5: 4:                                                    => [%le, %le, %le, %le, %le, %le, %le]\n",
		     q1, q2, q3, q4, q5, q6, q7));
	}

	if (p5 != nsnull && *p5 != nsnull)
	{
		nsITestProxy *test;
		(*p5)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2, q3, q4, q5, q6, q7;
		nsresult retval = NS_ERROR_BASE;
		q1 = 150, q2=250, q3=350, q4=450, q5=550, q6=650, q7=750;
		LOG(("TEST: 7_5: 5: [%d, %d, %d, %d, %d, %d, %d] => ",
		     q1, q2, q3, q4, q5, q6, q7));
		test->Test7_1(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &retval);
		LOG(("TEST: 7_5: 5:                                          => [%d, %d, %d, %d, %d, %d, %d]\n",
		     q1, q2, q3, q4, q5, q6, q7));
	}

	if (p6 != nsnull && *p6 != nsnull)
	{
		nsITestProxy *test;
		(*p6)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt64 q1, q2, q3, q4, q5, q6, q7;
		nsresult retval = NS_ERROR_BASE;
		q1 = 1500, q2=2500, q3=3500, q4=4500, q5=5500, q6=6500, q7=7500;
		LOG(("TEST: 7_5: 6: [%g, %g, %g, %g, %g, %g, %g] => ",
		     q1, q2, q3, q4, q5, q6, q7));
		test->Test7_2(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &retval);
		LOG(("TEST: 7_5: 6:                                                    => [%g, %g, %g, %g, %g, %g, %g]\n",
		     q1, q2, q3, q4, q5, q6, q7));
	}

	*p7 = new nsTestXPCFoo();

	LOG(("TEST: 7_5:                                                    => [0x%08X]\n",
	     *p7));

	*_retval = 7050;
	return 7050;	//NS_OK;
}





NS_IMETHODIMP nsTestXPCFoo::Test8_1(PRInt32 *p1, PRInt32 *p2, PRInt32 *p3, PRInt32 *p4, PRInt32 *p5, PRInt32 *p6, PRInt32 *p7, PRInt32 *p8, nsresult *_retval)
{
	LOG(("TEST: 8_1: [%d, %d, %d, %d, %d, %d, %d, %d] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7 + *p8;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;   *p7 += 1;   *p8 += 1;

	LOG(("TEST: 8_1:                                          => [%d, %d, %d, %d, %d, %d, %d, %d]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8));

	*_retval = 8010;
	return 8010;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test8_2(PRInt64 *p1, PRInt64 *p2, PRInt64 *p3, PRInt64 *p4, PRInt64 *p5, PRInt64 *p6, PRInt64 *p7, PRInt64 *p8, nsresult *_retval)
{
	LOG(("TEST: 8_2: [%g, %g, %g, %g, %g, %g, %g, %g] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7 + *p8;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;   *p7 += 1;   *p8 += 1;

	LOG(("TEST: 8_2:                                                    => [%g, %g, %g, %g, %g, %g, %g, %g]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8));

	*_retval = 8020;
	return 8020;	//NS_OK;
}



NS_IMETHODIMP nsTestXPCFoo::Test8_3(float *p1, float *p2, float *p3, float *p4, float *p5, float *p6, float *p7, float *p8, nsresult *_retval)
{
	LOG(("TEST: 8_3: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7 + *p8;

	*p2 += 1.0f;   *p3 += 1.0f;   *p4 += 1.0f;   *p5 += 1.0f;
	*p6 += 1.0f;   *p7 += 1.0f;   *p8 += 1.0f;

	LOG(("TEST: 8_3:                                                    => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8));

	*_retval = 8030;
	return 8030;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test8_4(double *p1, double *p2, double *p3, double *p4, double *p5, double *p6, double *p7, double *p8, nsresult *_retval)
{
	LOG(("TEST: 8_4: [%le, %le, %le, %le, %le, %le, %le, %le] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7 + *p8;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;   *p7 += 1;   *p8 += 1;

	LOG(("TEST: 8_4:                                                    => [%le, %le, %le, %le, %le, %le, %le, %le]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8));

	*_retval = 8040;
	return 8040;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test8_5(nsISupports **p1, nsISupports **p2, nsISupports **p3, nsISupports **p4, nsISupports **p5, nsISupports **p6, nsISupports **p7, nsISupports **p8, nsresult *_retval)
{
	if (p1 != nsnull && *p1 != nsnull)
	{
		nsITestProxy *test;
		(*p1)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2, q3, q4, q5, q6, q7, q8;
		nsresult retval = NS_ERROR_BASE;
		q1 = 100, q2=200, q3=300, q4=400, q5=500, q6=600, q7=700, q8=800;
		LOG(("TEST: 8_5: 1: [%d, %d, %d, %d, %d, %d, %d, %d] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8));
		test->Test8_1(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &retval);
		LOG(("TEST: 8_5: 1:                                          => [%d, %d, %d, %d, %d, %d, %d, %d]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8));
	}

	if (p2 != nsnull && *p2 != nsnull)
	{
		nsITestProxy *test;
		(*p2)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt64 q1, q2, q3, q4, q5, q6, q7, q8;
		nsresult retval = NS_ERROR_BASE;
		q1 = 1000, q2=2000, q3=3000, q4=4000, q5=5000, q6=6000, q7=7000, q8=8000;
		LOG(("TEST: 8_5: 2: [%g, %g, %g, %g, %g, %g, %g, %g] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8));
		test->Test8_2(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &retval);
		LOG(("TEST: 8_5: 2:                                                    => [%g, %g, %g, %g, %g, %g, %g, %g]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8));
	}

	if (p3 != nsnull && *p3 != nsnull)
	{
		nsITestProxy *test;
		(*p3)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		float q1, q2, q3, q4, q5, q6, q7, q8;
		nsresult retval = NS_ERROR_BASE;
		q1 = 10000, q2=20000, q3=30000, q4=40000, q5=50000, q6=60000, q7=70000, q8=80000;
		LOG(("TEST: 8_5: 3: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8));
		test->Test8_3(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &retval);
		LOG(("TEST: 8_5: 3:                                                    => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8));
	}

	if (p4 != nsnull && *p4 != nsnull)
	{
		nsITestProxy *test;
		(*p4)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		double q1, q2, q3, q4, q5, q6, q7, q8;
		nsresult retval = NS_ERROR_BASE;
		q1 = 10000, q2=20000, q3=30000, q4=40000, q5=50000, q6=60000, q7=70000, q8=80000;
		LOG(("TEST: 8_5: 4: [%le, %le, %le, %le, %le, %le, %le, %le] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8));
		test->Test8_4(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &retval);
		LOG(("TEST: 8_5: 4:                                                    => [%le, %le, %le, %le, %le, %le, %le, %le]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8));
	}

	if (p5 != nsnull && *p5 != nsnull)
	{
		nsITestProxy *test;
		(*p5)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2, q3, q4, q5, q6, q7, q8;
		nsresult retval = NS_ERROR_BASE;
		q1 = 150, q2=250, q3=350, q4=450, q5=550, q6=650, q7=750, q8=850;
		LOG(("TEST: 8_5: 5: [%d, %d, %d, %d, %d, %d, %d, %d] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8));
		test->Test8_1(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &retval);
		LOG(("TEST: 8_5: 5:                                          => [%d, %d, %d, %d, %d, %d, %d, %d]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8));
	}

	if (p6 != nsnull && *p6 != nsnull)
	{
		nsITestProxy *test;
		(*p6)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt64 q1, q2, q3, q4, q5, q6, q7, q8;
		nsresult retval = NS_ERROR_BASE;
		q1 = 1500, q2=2500, q3=3500, q4=4500, q5=5500, q6=6500, q7=7500, q8=8500;
		LOG(("TEST: 8_5: 6: [%g, %g, %g, %g, %g, %g, %g, %g] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8));
		test->Test8_2(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &retval);
		LOG(("TEST: 8_5: 6:                                                    => [%g, %g, %g, %g, %g, %g, %g, %g]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8));
	}

	if (p7 != nsnull && *p7 != nsnull)
	{
		nsITestProxy *test;
		(*p7)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		float q1, q2, q3, q4, q5, q6, q7, q8;
		nsresult retval = NS_ERROR_BASE;
		q1 = 15000, q2=25000, q3=35000, q4=45000, q5=55000, q6=65000, q7=75000, q8=85000;
		LOG(("TEST: 8_5: 7: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8));
		test->Test8_3(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &retval);
		LOG(("TEST: 8_5: 7:                                                    => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8));
	}


	*p8 = new nsTestXPCFoo();

	LOG(("TEST: 8_5:                                                    => [0x%08X]\n",
	     *p8));

	*_retval = 8050;
	return 8050;	//NS_OK;
}





NS_IMETHODIMP nsTestXPCFoo::Test9_1(PRInt32 *p1, PRInt32 *p2, PRInt32 *p3, PRInt32 *p4, PRInt32 *p5, PRInt32 *p6, PRInt32 *p7, PRInt32 *p8, PRInt32 *p9, nsresult *_retval)
{
	LOG(("TEST: 9_1: [%d, %d, %d, %d, %d, %d, %d, %d, %d] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7 + *p8 + *p9;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;   *p7 += 1;   *p8 += 1;   *p9 += 1;

	LOG(("TEST: 9_1:                                          => [%d, %d, %d, %d, %d, %d, %d, %d, %d]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9));

	*_retval = 9010;
	return 9010;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test9_2(PRInt64 *p1, PRInt64 *p2, PRInt64 *p3, PRInt64 *p4, PRInt64 *p5, PRInt64 *p6, PRInt64 *p7, PRInt64 *p8, PRInt64 *p9, nsresult *_retval)
{
	LOG(("TEST: 9_2: [%g, %g, %g, %g, %g, %g, %g, %g, %g] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7 + *p8 + *p9;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;   *p7 += 1;   *p8 += 1;   *p9 += 1;

	LOG(("TEST: 9_2:                                                    => [%g, %g, %g, %g, %g, %g, %g, %g, %g]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9));

	*_retval = 9020;
	return 9020;	//NS_OK;
}


NS_IMETHODIMP nsTestXPCFoo::Test9_3(float *p1, float *p2, float *p3, float *p4, float *p5, float *p6, float *p7, float *p8, float *p9, nsresult *_retval)
{
	LOG(("TEST: 9_3: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7 + *p8 + *p9;

	*p2 += 1.0f;   *p3 += 1.0f;   *p4 += 1.0f;   *p5 += 1.0f;
	*p6 += 1.0f;   *p7 += 1.0f;   *p8 += 1.0f;   *p9 += 1.0f;

	LOG(("TEST: 9_3:                                                    => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9));

	*_retval = 9030;
	return 9030;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test9_4(double *p1, double *p2, double *p3, double *p4, double *p5, double *p6, double *p7, double *p8, double *p9, nsresult *_retval)
{
	LOG(("TEST: 9_4: [%le, %le, %le, %le, %le, %le, %le, %le, %le] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7 + *p8 + *p9;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;   *p7 += 1;   *p8 += 1;   *p9 += 1;

	LOG(("TEST: 9_4:                                                    => [%le, %le, %le, %le, %le, %le, %le, %le, %le]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9));

	*_retval = 9040;
	return 9040;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test9_5(nsISupports **p1, nsISupports **p2, nsISupports **p3, nsISupports **p4, nsISupports **p5, nsISupports **p6, nsISupports **p7, nsISupports **p8, nsISupports **p9, nsresult *_retval)
{
	if (p1 != nsnull && *p1 != nsnull)
	{
		nsITestProxy *test;
		(*p1)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2, q3, q4, q5, q6, q7, q8, q9;
		nsresult retval = NS_ERROR_BASE;
		q1 = 100, q2=200, q3=300, q4=400, q5=500, q6=600, q7=700, q8=800, q9=900;
		LOG(("TEST: 9_5: 1: [%d, %d, %d, %d, %d, %d, %d, %d, %d] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
		test->Test9_1(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &retval);
		LOG(("TEST: 9_5: 1:                                          => [%d, %d, %d, %d, %d, %d, %d, %d, %d]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
	}

	if (p2 != nsnull && *p2 != nsnull)
	{
		nsITestProxy *test;
		(*p2)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt64 q1, q2, q3, q4, q5, q6, q7, q8, q9;
		nsresult retval = NS_ERROR_BASE;
		q1 = 1000, q2=2000, q3=3000, q4=4000, q5=5000, q6=6000, q7=7000, q8=8000, q9=9000;
		LOG(("TEST: 9_5: 2: [%g, %g, %g, %g, %g, %g, %g, %g, %g] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
		test->Test9_2(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &retval);
		LOG(("TEST: 9_5: 2:                                                    => [%g, %g, %g, %g, %g, %g, %g, %g, %g]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
	}

	if (p3 != nsnull && *p3 != nsnull)
	{
		nsITestProxy *test;
		(*p3)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		float q1, q2, q3, q4, q5, q6, q7, q8, q9;
		nsresult retval = NS_ERROR_BASE;
		q1 = 10000, q2=20000, q3=30000, q4=40000, q5=50000, q6=60000, q7=70000, q8=80000, q9=90000;
		LOG(("TEST: 9_5: 3: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
		test->Test9_3(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &retval);
		LOG(("TEST: 9_5: 3:                                                    => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
	}


	if (p4 != nsnull && *p4 != nsnull)
	{
		nsITestProxy *test;
		(*p4)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		double q1, q2, q3, q4, q5, q6, q7, q8, q9;
		nsresult retval = NS_ERROR_BASE;
		q1 = 10000, q2=20000, q3=30000, q4=40000, q5=50000, q6=60000, q7=70000, q8=80000, q9=90000;
		LOG(("TEST: 9_5: 4: [%le, %le, %le, %le, %le, %le, %le, %le, %le] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
		test->Test9_4(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &retval);
		LOG(("TEST: 9_5: 4:                                                    => [%le, %le, %le, %le, %le, %le, %le, %le, %le]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
	}

	if (p5 != nsnull && *p5 != nsnull)
	{
		nsITestProxy *test;
		(*p5)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2, q3, q4, q5, q6, q7, q8, q9;
		nsresult retval = NS_ERROR_BASE;
		q1 = 150, q2=250, q3=350, q4=450, q5=550, q6=650, q7=750, q8=850, q9=950;
		LOG(("TEST: 9_5: 5: [%d, %d, %d, %d, %d, %d, %d, %d, %d] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
		test->Test9_1(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &retval);
		LOG(("TEST: 9_5: 5:                                          => [%d, %d, %d, %d, %d, %d, %d, %d, %d]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
	}

	if (p6 != nsnull && *p6 != nsnull)
	{
		nsITestProxy *test;
		(*p6)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt64 q1, q2, q3, q4, q5, q6, q7, q8, q9;
		nsresult retval = NS_ERROR_BASE;
		q1 = 1500, q2=2500, q3=3500, q4=4500, q5=5500, q6=6500, q7=7500, q8=8500, q9=9500;
		LOG(("TEST: 9_5: 6: [%g, %g, %g, %g, %g, %g, %g, %g, %g] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
		test->Test9_2(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &retval);
		LOG(("TEST: 9_5: 6:                                                    => [%g, %g, %g, %g, %g, %g, %g, %g, %g]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
	}

	if (p7 != nsnull && *p7 != nsnull)
	{
		nsITestProxy *test;
		(*p7)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		float q1, q2, q3, q4, q5, q6, q7, q8, q9;
		nsresult retval = NS_ERROR_BASE;
		q1 = 15000, q2=25000, q3=35000, q4=45000, q5=55000, q6=65000, q7=75000, q8=85000, q9=95000;
		LOG(("TEST: 9_5: 7: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
		test->Test9_3(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &retval);
		LOG(("TEST: 9_5: 7:                                                    => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
	}


	if (p8 != nsnull && *p8 != nsnull)
	{
		nsITestProxy *test;
		(*p8)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		double q1, q2, q3, q4, q5, q6, q7, q8, q9;
		nsresult retval = NS_ERROR_BASE;
		q1 = 16000, q2=26000, q3=36000, q4=46000, q5=56000, q6=66000, q7=76000, q8=86000, q9=96000;
		LOG(("TEST: 9_5: 8: [%le, %le, %le, %le, %le, %le, %le, %le, %le] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
		test->Test9_4(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &retval);
		LOG(("TEST: 9_5: 8:                                                    => [%le, %le, %le, %le, %le, %le, %le, %le, %le]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9));
	}

	*p9 = new nsTestXPCFoo();

	LOG(("TEST: 9_5:                                                    => [0x%08X]\n",
	     *p9));

	*_retval = 9050;
	return 9050;	//NS_OK;
}





NS_IMETHODIMP nsTestXPCFoo::Test10_1(PRInt32 *p1, PRInt32 *p2, PRInt32 *p3, PRInt32 *p4, PRInt32 *p5, PRInt32 *p6, PRInt32 *p7, PRInt32 *p8, PRInt32 *p9, PRInt32 *p10, nsresult *_retval)
{
	LOG(("TEST: 10_1: [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7 + *p8 + *p9 + *p10;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;   *p7 += 1;   *p8 += 1;   *p9 += 1;   *p10 += 1;

	LOG(("TEST: 10_1:                                          => [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10));

	*_retval = 10010;
	return 10010;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test10_2(PRInt64 *p1, PRInt64 *p2, PRInt64 *p3, PRInt64 *p4, PRInt64 *p5, PRInt64 *p6, PRInt64 *p7, PRInt64 *p8, PRInt64 *p9, PRInt64 *p10, nsresult *_retval)
{
	LOG(("TEST: 10_2: [%g, %g, %g, %g, %g, %g, %g, %g, %g, %g] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7 + *p8 + *p9 + *p10;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;   *p7 += 1;   *p8 += 1;   *p9 += 1;   *p10 += 1;

	LOG(("TEST: 10_2:                                                    => [%g, %g, %g, %g, %g, %g, %g, %g, %g, %g]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10));

	*_retval = 10020;
	return 10020;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test10_3(float *p1, float *p2, float *p3, float *p4, float *p5, float *p6, float *p7, float *p8, float *p9, float *p10, nsresult *_retval)
{
	LOG(("TEST: 10_3: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7 + *p8 + *p9 + *p10;

	*p2 += 1.0f;   *p3 += 1.0f;   *p4 += 1.0f;   *p5 += 1.0f;
	*p6 += 1.0f;   *p7 += 1.0f;   *p8 += 1.0f;   *p9 += 1.0f;   *p10 += 1.0f;

	LOG(("TEST: 10_3:                                                    => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10));

	*_retval = 10030;
	return 10030;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test10_4(double *p1, double *p2, double *p3, double *p4, double *p5, double *p6, double *p7, double *p8, double *p9, double *p10, nsresult *_retval)
{
	LOG(("TEST: 10_4: [%le, %le, %le, %le, %le, %le, %le, %le, %le, %le] => ",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10));

	*p1 = *p1 + *p2 + *p3 + *p4 + *p5 + *p6 + *p7 + *p8 + *p9 + *p10;

	*p2 += 1;   *p3 += 1;   *p4 += 1;   *p5 += 1;
	*p6 += 1;   *p7 += 1;   *p8 += 1;   *p9 += 1;   *p10 += 1;

	LOG(("TEST: 10_4:                                                    => [%le, %le, %le, %le, %le, %le, %le, %le, %le, %le]\n",
	     *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10));

	*_retval = 10040;
	return 10040;	//NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test10_5(nsISupports **p1, nsISupports **p2, nsISupports **p3, nsISupports **p4, nsISupports **p5, nsISupports **p6, nsISupports **p7, nsISupports **p8, nsISupports **p9, nsISupports **p10, nsresult *_retval)
{
	if (p1 != nsnull && *p1 != nsnull)
	{
		nsITestProxy *test;
		(*p1)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2, q3, q4, q5, q6, q7, q8, q9, q10;
		nsresult retval = NS_ERROR_BASE;
		q1 = 100, q2=200, q3=300, q4=400, q5=500, q6=600, q7=700, q8=800, q9=900, q10=1000;
		LOG(("TEST: 10_5: 1: [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
		test->Test10_1(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &q10, &retval);
		LOG(("TEST: 10_5: 1:                                          => [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
	}

	if (p2 != nsnull && *p2 != nsnull)
	{
		nsITestProxy *test;
		(*p2)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt64 q1, q2, q3, q4, q5, q6, q7, q8, q9, q10;
		nsresult retval = NS_ERROR_BASE;
		q1 = 1000, q2=2000, q3=3000, q4=4000, q5=5000, q6=6000, q7=7000, q8=8000, q9=9000, q10=10000;
		LOG(("TEST: 10_5: 2: [%g, %g, %g, %g, %g, %g, %g, %g, %g, %g] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
		test->Test10_2(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &q10, &retval);
		LOG(("TEST: 10_5: 2:                                                    => [%g, %g, %g, %g, %g, %g, %g, %g, %g, %g]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
	}

	if (p3 != nsnull && *p3 != nsnull)
	{
		nsITestProxy *test;
		(*p3)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		float q1, q2, q3, q4, q5, q6, q7, q8, q9, q10;
		nsresult retval = NS_ERROR_BASE;
		q1 = 10000, q2=20000, q3=30000, q4=40000, q5=50000, q6=60000, q7=70000, q8=80000, q9=90000, q10=100000;
		LOG(("TEST: 10_5: 3: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
		test->Test10_3(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &q10, &retval);
		LOG(("TEST: 10_5: 3:                                                    => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
	}


	if (p4 != nsnull && *p4 != nsnull)
	{
		nsITestProxy *test;
		(*p4)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		double q1, q2, q3, q4, q5, q6, q7, q8, q9, q10;
		nsresult retval = NS_ERROR_BASE;
		q1 = 10000, q2=20000, q3=30000, q4=40000, q5=50000, q6=60000, q7=70000, q8=80000, q9=90000, q10=100000;
		LOG(("TEST: 10_5: 4: [%le, %le, %le, %le, %le, %le, %le, %le, %le, %le] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
		test->Test10_4(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &q10, &retval);
		LOG(("TEST: 10_5: 4:                                                    => [%le, %le, %le, %le, %le, %le, %le, %le, %le, %le]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
	}

	if (p5 != nsnull && *p5 != nsnull)
	{
		nsITestProxy *test;
		(*p5)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2, q3, q4, q5, q6, q7, q8, q9, q10;
		nsresult retval = NS_ERROR_BASE;
		q1 = 150, q2=250, q3=350, q4=450, q5=550, q6=650, q7=750, q8=850, q9=950, q10=1050;
		LOG(("TEST: 10_5: 5: [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
		test->Test10_1(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &q10, &retval);
		LOG(("TEST: 10_5: 5:                                          => [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
	}

	if (p6 != nsnull && *p6 != nsnull)
	{
		nsITestProxy *test;
		(*p6)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt64 q1, q2, q3, q4, q5, q6, q7, q8, q9, q10;
		nsresult retval = NS_ERROR_BASE;
		q1 = 1500, q2=2500, q3=3500, q4=4500, q5=5500, q6=6500, q7=7500, q8=8500, q9=9500, q10=10500;
		LOG(("TEST: 10_5: 6: [%g, %g, %g, %g, %g, %g, %g, %g, %g, %g] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
		test->Test10_2(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &q10, &retval);
		LOG(("TEST: 10_5: 6:                                                    => [%g, %g, %g, %g, %g, %g, %g, %g, %g, %g]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
	}

	if (p7 != nsnull && *p7 != nsnull)
	{
		nsITestProxy *test;
		(*p7)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		float q1, q2, q3, q4, q5, q6, q7, q8, q9, q10;
		nsresult retval = NS_ERROR_BASE;
		q1 = 15000, q2=25000, q3=35000, q4=45000, q5=55000, q6=65000, q7=75000, q8=85000, q9=95000, q10=105000;
		LOG(("TEST: 10_5: 7: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
		test->Test10_3(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &q10, &retval);
		LOG(("TEST: 10_5: 7:                                                    => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
	}


	if (p8 != nsnull && *p8 != nsnull)
	{
		nsITestProxy *test;
		(*p8)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		double q1, q2, q3, q4, q5, q6, q7, q8, q9, q10;
		nsresult retval = NS_ERROR_BASE;
		q1 = 16000, q2=26000, q3=36000, q4=46000, q5=56000, q6=66000, q7=76000, q8=86000, q9=96000, q10=106000;
		LOG(("TEST: 10_5: 8: [%le, %le, %le, %le, %le, %le, %le, %le, %le, %le] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
		test->Test10_4(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &q10, &retval);
		LOG(("TEST: 10_5: 8:                                                    => [%le, %le, %le, %le, %le, %le, %le, %le, %le, %le]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
	}


	if (p9 != nsnull && *p9 != nsnull)
	{
		nsITestProxy *test;
		(*p9)->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
		PRInt32 q1, q2, q3, q4, q5, q6, q7, q8, q9, q10;
		nsresult retval = NS_ERROR_BASE;
		q1 = 175, q2=275, q3=375, q4=475, q5=575, q6=675, q7=775, q8=875, q9=975, q10=1075;
		LOG(("TEST: 10_5: 9: [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d] => ",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
		test->Test10_1(&q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &q10, &retval);
		LOG(("TEST: 10_5: 9:                                          => [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d]\n",
		     q1, q2, q3, q4, q5, q6, q7, q8, q9, q10));
	}

	*p10 = new nsTestXPCFoo();

	LOG(("TEST: 10_5:                                                    => [0x%08X]\n",
	     *p10));

	*_retval = 10050;
	return 10050;	//NS_OK;
}










/***************************************************************************/
/* nsTestXPCFoo2                                                           */
/***************************************************************************/
nsTestXPCFoo2::nsTestXPCFoo2()
{
	NS_ADDREF_THIS();
}


NS_IMPL_THREADSAFE_ISUPPORTS1(nsTestXPCFoo2, nsITestProxy2)

NS_IMETHODIMP nsTestXPCFoo2::Test(PRInt32 p1, PRInt32 p2, PRInt32 *_retval)
{
	LOG(("TEST: calling back to caller!\n"));

	nsCOMPtr<nsIProxyObjectManager> manager =
		do_GetService(NS_XPCOMPROXY_CONTRACTID);

	LOG(("TEST: ProxyObjectManager: %p \n", (void *) manager.get()));

	PR_ASSERT(manager);

	nsCOMPtr<nsIThread> thread;
	GetThreadFromPRThread((PRThread *) p1, getter_AddRefs(thread));
	NS_ENSURE_STATE(thread);

	nsresult r;
	nsCOMPtr<nsITestProxy> proxyObject;
	manager->GetProxyForObject(thread, NS_GET_IID(nsITestProxy), this, NS_PROXY_SYNC, (void**)&proxyObject);
	proxyObject->Test3(nsnull, nsnull, &r);

	LOG(("TEST: Deleting Proxy Object\n"));
	return NS_OK;
}


NS_IMETHODIMP nsTestXPCFoo2::Test2(nsresult *_retval)
{
	LOG(("TEST: nsTestXPCFoo2::Test2() called\n"));
	*_retval = 0x11223344;
	return NS_OK;
}


NS_IMETHODIMP nsTestXPCFoo2::Test3(nsISupports *p1, nsISupports **p2 NS_OUTPARAM, nsresult *_retval)
{
	LOG(("TEST: Got called"));
	return NS_OK;
}



