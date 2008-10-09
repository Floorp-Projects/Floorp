/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef testproxyclasses_h__
#define testproxyclasses_h__


/***************************************************************************/
/* nsTestXPCFoo                                                            */
/***************************************************************************/
class nsTestXPCFoo : public nsITestProxy
{
    NS_DECL_ISUPPORTS

    NS_IMETHOD Test(PRInt32 p1, PRInt32 p2, PRInt32 *_retval NS_OUTPARAM);
    NS_IMETHOD Test2(nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test3(nsISupports *p1, nsISupports **p2 NS_OUTPARAM, nsresult *_retval NS_OUTPARAM);

    NS_IMETHOD Test1_1(PRInt32 *p1 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test1_2(PRInt64 *p1 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test1_3(float *p1 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test1_4(double *p1 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test1_5(nsISupports **p1 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);

    NS_IMETHOD Test2_1(PRInt32 *p1 NS_INOUTPARAM, PRInt32 *p2 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test2_2(PRInt64 *p1 NS_INOUTPARAM, PRInt64 *p2 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test2_3(float *p1 NS_INOUTPARAM, float *p2 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test2_4(double *p1 NS_INOUTPARAM, double *p2 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test2_5(nsISupports **p1 NS_INOUTPARAM, nsISupports **p2 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);

    NS_IMETHOD Test3_1(PRInt32 *p1 NS_INOUTPARAM, PRInt32 *p2 NS_INOUTPARAM, PRInt32 *p3 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test3_2(PRInt64 *p1 NS_INOUTPARAM, PRInt64 *p2 NS_INOUTPARAM, PRInt64 *p3 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test3_3(float *p1 NS_INOUTPARAM, float *p2 NS_INOUTPARAM, float *p3 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test3_4(double *p1 NS_INOUTPARAM, double *p2 NS_INOUTPARAM, double *p3 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test3_5(nsISupports **p1 NS_INOUTPARAM, nsISupports **p2 NS_INOUTPARAM, nsISupports **p3 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);

    NS_IMETHOD Test4_1(PRInt32 *p1 NS_INOUTPARAM, PRInt32 *p2 NS_INOUTPARAM, PRInt32 *p3 NS_INOUTPARAM, PRInt32 *p4 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test4_2(PRInt64 *p1 NS_INOUTPARAM, PRInt64 *p2 NS_INOUTPARAM, PRInt64 *p3 NS_INOUTPARAM, PRInt64 *p4 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test4_3(float *p1 NS_INOUTPARAM, float *p2 NS_INOUTPARAM, float *p3 NS_INOUTPARAM, float *p4 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test4_4(double *p1 NS_INOUTPARAM, double *p2 NS_INOUTPARAM, double *p3 NS_INOUTPARAM, double *p4 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test4_5(nsISupports **p1 NS_INOUTPARAM, nsISupports **p2 NS_INOUTPARAM, nsISupports **p3 NS_INOUTPARAM, nsISupports **p4 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);

    NS_IMETHOD Test5_1(PRInt32 *p1 NS_INOUTPARAM, PRInt32 *p2 NS_INOUTPARAM, PRInt32 *p3 NS_INOUTPARAM, PRInt32 *p4 NS_INOUTPARAM, PRInt32 *p5 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test5_2(PRInt64 *p1 NS_INOUTPARAM, PRInt64 *p2 NS_INOUTPARAM, PRInt64 *p3 NS_INOUTPARAM, PRInt64 *p4 NS_INOUTPARAM, PRInt64 *p5 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test5_3(float *p1 NS_INOUTPARAM, float *p2 NS_INOUTPARAM, float *p3 NS_INOUTPARAM, float *p4 NS_INOUTPARAM, float *p5 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test5_4(double *p1 NS_INOUTPARAM, double *p2 NS_INOUTPARAM, double *p3 NS_INOUTPARAM, double *p4 NS_INOUTPARAM, double *p5 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test5_5(nsISupports **p1 NS_INOUTPARAM, nsISupports **p2 NS_INOUTPARAM, nsISupports **p3 NS_INOUTPARAM, nsISupports **p4 NS_INOUTPARAM, nsISupports **p5 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);

    NS_IMETHOD Test6_1(PRInt32 *p1 NS_INOUTPARAM, PRInt32 *p2 NS_INOUTPARAM, PRInt32 *p3 NS_INOUTPARAM, PRInt32 *p4 NS_INOUTPARAM, PRInt32 *p5 NS_INOUTPARAM, PRInt32 *p6 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test6_2(PRInt64 *p1 NS_INOUTPARAM, PRInt64 *p2 NS_INOUTPARAM, PRInt64 *p3 NS_INOUTPARAM, PRInt64 *p4 NS_INOUTPARAM, PRInt64 *p5 NS_INOUTPARAM, PRInt64 *p6 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test6_3(float *p1 NS_INOUTPARAM, float *p2 NS_INOUTPARAM, float *p3 NS_INOUTPARAM, float *p4 NS_INOUTPARAM, float *p5 NS_INOUTPARAM, float *p6 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test6_4(double *p1 NS_INOUTPARAM, double *p2 NS_INOUTPARAM, double *p3 NS_INOUTPARAM, double *p4 NS_INOUTPARAM, double *p5 NS_INOUTPARAM, double *p6 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test6_5(nsISupports **p1 NS_INOUTPARAM, nsISupports **p2 NS_INOUTPARAM, nsISupports **p3 NS_INOUTPARAM, nsISupports **p4 NS_INOUTPARAM, nsISupports **p5 NS_INOUTPARAM, nsISupports **p6 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);

    NS_IMETHOD Test7_1(PRInt32 *p1 NS_INOUTPARAM, PRInt32 *p2 NS_INOUTPARAM, PRInt32 *p3 NS_INOUTPARAM, PRInt32 *p4 NS_INOUTPARAM, PRInt32 *p5 NS_INOUTPARAM, PRInt32 *p6 NS_INOUTPARAM, PRInt32 *p7 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test7_2(PRInt64 *p1 NS_INOUTPARAM, PRInt64 *p2 NS_INOUTPARAM, PRInt64 *p3 NS_INOUTPARAM, PRInt64 *p4 NS_INOUTPARAM, PRInt64 *p5 NS_INOUTPARAM, PRInt64 *p6 NS_INOUTPARAM, PRInt64 *p7 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test7_3(float *p1 NS_INOUTPARAM, float *p2 NS_INOUTPARAM, float *p3 NS_INOUTPARAM, float *p4 NS_INOUTPARAM, float *p5 NS_INOUTPARAM, float *p6 NS_INOUTPARAM, float *p7 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test7_4(double *p1 NS_INOUTPARAM, double *p2 NS_INOUTPARAM, double *p3 NS_INOUTPARAM, double *p4 NS_INOUTPARAM, double *p5 NS_INOUTPARAM, double *p6 NS_INOUTPARAM, double *p7 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test7_5(nsISupports **p1 NS_INOUTPARAM, nsISupports **p2 NS_INOUTPARAM, nsISupports **p3 NS_INOUTPARAM, nsISupports **p4 NS_INOUTPARAM, nsISupports **p5 NS_INOUTPARAM, nsISupports **p6 NS_INOUTPARAM, nsISupports **p7 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);

    NS_IMETHOD Test8_1(PRInt32 *p1 NS_INOUTPARAM, PRInt32 *p2 NS_INOUTPARAM, PRInt32 *p3 NS_INOUTPARAM, PRInt32 *p4 NS_INOUTPARAM, PRInt32 *p5 NS_INOUTPARAM, PRInt32 *p6 NS_INOUTPARAM, PRInt32 *p7 NS_INOUTPARAM, PRInt32 *p8 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test8_2(PRInt64 *p1 NS_INOUTPARAM, PRInt64 *p2 NS_INOUTPARAM, PRInt64 *p3 NS_INOUTPARAM, PRInt64 *p4 NS_INOUTPARAM, PRInt64 *p5 NS_INOUTPARAM, PRInt64 *p6 NS_INOUTPARAM, PRInt64 *p7 NS_INOUTPARAM, PRInt64 *p8 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test8_3(float *p1 NS_INOUTPARAM, float *p2 NS_INOUTPARAM, float *p3 NS_INOUTPARAM, float *p4 NS_INOUTPARAM, float *p5 NS_INOUTPARAM, float *p6 NS_INOUTPARAM, float *p7 NS_INOUTPARAM, float *p8 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test8_4(double *p1 NS_INOUTPARAM, double *p2 NS_INOUTPARAM, double *p3 NS_INOUTPARAM, double *p4 NS_INOUTPARAM, double *p5 NS_INOUTPARAM, double *p6 NS_INOUTPARAM, double *p7 NS_INOUTPARAM, double *p8 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test8_5(nsISupports **p1 NS_INOUTPARAM, nsISupports **p2 NS_INOUTPARAM, nsISupports **p3 NS_INOUTPARAM, nsISupports **p4 NS_INOUTPARAM, nsISupports **p5 NS_INOUTPARAM, nsISupports **p6 NS_INOUTPARAM, nsISupports **p7 NS_INOUTPARAM, nsISupports **p8 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);

    NS_IMETHOD Test9_1(PRInt32 *p1 NS_INOUTPARAM, PRInt32 *p2 NS_INOUTPARAM, PRInt32 *p3 NS_INOUTPARAM, PRInt32 *p4 NS_INOUTPARAM, PRInt32 *p5 NS_INOUTPARAM, PRInt32 *p6 NS_INOUTPARAM, PRInt32 *p7 NS_INOUTPARAM, PRInt32 *p8 NS_INOUTPARAM, PRInt32 *p9 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test9_2(PRInt64 *p1 NS_INOUTPARAM, PRInt64 *p2 NS_INOUTPARAM, PRInt64 *p3 NS_INOUTPARAM, PRInt64 *p4 NS_INOUTPARAM, PRInt64 *p5 NS_INOUTPARAM, PRInt64 *p6 NS_INOUTPARAM, PRInt64 *p7 NS_INOUTPARAM, PRInt64 *p8 NS_INOUTPARAM, PRInt64 *p9 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test9_3(float *p1 NS_INOUTPARAM, float *p2 NS_INOUTPARAM, float *p3 NS_INOUTPARAM, float *p4 NS_INOUTPARAM, float *p5 NS_INOUTPARAM, float *p6 NS_INOUTPARAM, float *p7 NS_INOUTPARAM, float *p8 NS_INOUTPARAM, float *p9 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test9_4(double *p1 NS_INOUTPARAM, double *p2 NS_INOUTPARAM, double *p3 NS_INOUTPARAM, double *p4 NS_INOUTPARAM, double *p5 NS_INOUTPARAM, double *p6 NS_INOUTPARAM, double *p7 NS_INOUTPARAM, double *p8 NS_INOUTPARAM, double *p9 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test9_5(nsISupports **p1 NS_INOUTPARAM, nsISupports **p2 NS_INOUTPARAM, nsISupports **p3 NS_INOUTPARAM, nsISupports **p4 NS_INOUTPARAM, nsISupports **p5 NS_INOUTPARAM, nsISupports **p6 NS_INOUTPARAM, nsISupports **p7 NS_INOUTPARAM, nsISupports **p8 NS_INOUTPARAM, nsISupports **p9 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);

    NS_IMETHOD Test10_1(PRInt32 *p1 NS_INOUTPARAM, PRInt32 *p2 NS_INOUTPARAM, PRInt32 *p3 NS_INOUTPARAM, PRInt32 *p4 NS_INOUTPARAM, PRInt32 *p5 NS_INOUTPARAM, PRInt32 *p6 NS_INOUTPARAM, PRInt32 *p7 NS_INOUTPARAM, PRInt32 *p8 NS_INOUTPARAM, PRInt32 *p9 NS_INOUTPARAM, PRInt32 *p10 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test10_2(PRInt64 *p1 NS_INOUTPARAM, PRInt64 *p2 NS_INOUTPARAM, PRInt64 *p3 NS_INOUTPARAM, PRInt64 *p4 NS_INOUTPARAM, PRInt64 *p5 NS_INOUTPARAM, PRInt64 *p6 NS_INOUTPARAM, PRInt64 *p7 NS_INOUTPARAM, PRInt64 *p8 NS_INOUTPARAM, PRInt64 *p9 NS_INOUTPARAM, PRInt64 *p10 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test10_3(float *p1 NS_INOUTPARAM, float *p2 NS_INOUTPARAM, float *p3 NS_INOUTPARAM, float *p4 NS_INOUTPARAM, float *p5 NS_INOUTPARAM, float *p6 NS_INOUTPARAM, float *p7 NS_INOUTPARAM, float *p8 NS_INOUTPARAM, float *p9 NS_INOUTPARAM, float *p10 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test10_4(double *p1 NS_INOUTPARAM, double *p2 NS_INOUTPARAM, double *p3 NS_INOUTPARAM, double *p4 NS_INOUTPARAM, double *p5 NS_INOUTPARAM, double *p6 NS_INOUTPARAM, double *p7 NS_INOUTPARAM, double *p8 NS_INOUTPARAM, double *p9 NS_INOUTPARAM, double *p10 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);
    NS_IMETHOD Test10_5(nsISupports **p1 NS_INOUTPARAM, nsISupports **p2 NS_INOUTPARAM, nsISupports **p3 NS_INOUTPARAM, nsISupports **p4 NS_INOUTPARAM, nsISupports **p5 NS_INOUTPARAM, nsISupports **p6 NS_INOUTPARAM, nsISupports **p7 NS_INOUTPARAM, nsISupports **p8 NS_INOUTPARAM, nsISupports **p9 NS_INOUTPARAM, nsISupports **p10 NS_INOUTPARAM, nsresult *_retval NS_OUTPARAM);

    nsTestXPCFoo();
};




/***************************************************************************/
/* nsTestXPCFoo2                                                           */
/***************************************************************************/
class nsTestXPCFoo2 : public nsITestProxy2
{
	NS_DECL_ISUPPORTS

	NS_IMETHOD Test(PRInt32 p1, PRInt32 p2, PRInt32 *_retval NS_OUTPARAM);
	NS_IMETHOD Test2(nsresult *_retval NS_OUTPARAM);
	NS_IMETHOD Test3(nsISupports *p1, nsISupports **p2 NS_OUTPARAM, nsresult *_retval NS_OUTPARAM);

	nsTestXPCFoo2();
};



#endif

