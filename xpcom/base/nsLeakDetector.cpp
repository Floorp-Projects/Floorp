/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick C. Beard <beard@netscape.com>
 *   Scott Collins <scc@mozilla.org>
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

#if defined(GC_LEAK_DETECTOR)

#include "nsLeakDetector.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsILeakDetector.h"
#include "nsICollection.h"

#include <stdio.h>
#include <time.h>

#include "gc.h"

extern "C" {
extern FILE *GC_stdout, *GC_stderr;
extern void GC_trace_object(GC_PTR object, int verbose);
extern void GC_mark_object(GC_PTR object, GC_word mark);
}

static nsresult nextLeakFile()
{
	if (GC_stderr != NULL)
		fclose(GC_stderr);
	
	// generate a time stamped report name.
	time_t timer;
	time(&timer);
	tm* now = localtime(&timer);
	
	char reportName[256];
	sprintf(reportName, "Leaks%02d%02d%02d",
			now->tm_hour, now->tm_min, now->tm_sec);
	GC_stderr = fopen(reportName, "w");
	
	return NS_OK;
}

static FILE* openTraceFile()
{
	// generate a time stamped report name.
	time_t timer;
	time(&timer);
	tm* now = localtime(&timer);
	
	char reportName[256];
	sprintf(reportName, "Trace%02d%02d%02d",
			now->tm_hour, now->tm_min, now->tm_sec);
	return fopen(reportName, "w");
}

class nsLeakDetector : public nsILeakDetector {
public:
	nsLeakDetector();
	
	NS_DECL_ISUPPORTS	
	NS_DECL_NSILEAKDETECTOR
private:
	~nsLeakDetector() {}
};

NS_IMPL_ISUPPORTS1(nsLeakDetector, nsILeakDetector)

nsLeakDetector::nsLeakDetector() {
   }

NS_METHOD nsLeakDetector::DumpLeaks()
{
	GC_gcollect();

	return nextLeakFile();
}

NS_METHOD nsLeakDetector::TraceObject(nsISupports* object, PRBool verbose)
{
    FILE* trace = openTraceFile();
    if (trace != NULL) {
        FILE* old_stderr = GC_stderr;
        GC_stderr = trace;
        GC_trace_object(object, (verbose ? 1 : 0));
        GC_stderr = old_stderr;
        fclose(trace);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_METHOD nsLeakDetector::TraceCollection(nsICollection* objects, PRBool verbose)
{
    PRUint32 count;
    if (NS_FAILED(objects->Count(&count)))
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsISupports>* elements = new nsCOMPtr<nsISupports>[count];
    if (elements == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    for (PRUint32 i = 0; i < count; ++i)
        objects->GetElementAt(i, getter_AddRefs(elements[i]));

    nsresult rv = NS_ERROR_FAILURE;
    FILE* trace = openTraceFile();
    if (trace != NULL) {
        FILE* old_stderr = GC_stderr;
        GC_stderr = trace;
        GC_trace_object(elements, (verbose ? 1 : 0));
        GC_stderr = old_stderr;
        fclose(trace);
        rv = NS_OK;
    }
    
    delete[] elements;
    
    return rv;
}

NS_METHOD nsLeakDetector::MarkObject(nsISupports* object, PRBool marked)
{
    GC_mark_object(object, (marked ? 1 : 0));
    return NS_OK;
}

NS_METHOD nsLeakDetector::GetServices(nsISupports* *result)
{
    return NS_GetServiceManager((nsIServiceManager**)result);
}

#define NS_CLEAKDETECTOR_CID_STR "bb1ba360-1dd1-11b2-b30e-aa2314429f54"
#define NS_CLEAKDETECTOR_CID {0xbb1ba360, 0x1dd1, 0x11b2, {0xb3, 0x0e, 0xaa, 0x23, 0x14, 0x42, 0x9f, 0x54}}
#define NS_CLEAKDETECTOR_CONTRACTID "@mozilla.org/xpcom/leakdetector;1"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsLeakDetector)

static NS_DEFINE_CID(kCLeakDetectorCID, NS_CLEAKDETECTOR_CID);

nsresult NS_InitLeakDetector()
{
	nsresult rv;

	// open the first leak file.
	rv = nextLeakFile();
	if (NS_FAILED(rv))
		return rv;

    static const nsModuleComponentInfo info = {
        "Leak Detector", kCLeakDetectorCID, NS_CLEAKDETECTOR_CONTRACTID, nsLeakDetectorConstructor
    };

	// create a generic factory for the leak detector.
	nsCOMPtr<nsIGenericFactory> factory;
	rv = NS_NewGenericFactory(getter_AddRefs(factory), &info);
	if (NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIComponentRegistrar> registrar;
	NS_GetComponentRegistrar(getter_AddRefs(registrar));

	return registrar->RegisterFactory(info.mCID, info.mDescription, info.mContractID, factory);
}

#ifdef XP_MAC
#define SHUTDOWN_LEAKS_EARLY
#undef SHUTDOWN_LEAKS_MEDIUM
#undef SHUTDOWN_LEAKS_LATE
#else
#undef SHUTDOWN_LEAKS_EARLY
#undef SHUTDOWN_LEAKS_MEDIUM
#define SHUTDOWN_LEAKS_LATE
#endif

class LeakDetectorFinalizer
{
public:
	~LeakDetectorFinalizer();
};

#ifdef SHUTDOWN_LEAKS_LATE
// do shutdown leaks when XPCOM library is unloaded
LeakDetectorFinalizer gLeakDetectorFinalizer;
#endif

LeakDetectorFinalizer::~LeakDetectorFinalizer()
{
	GC_gcollect();

#if 0
	nextLeakFile();
	if (GC_stdout != NULL) {
		fprintf(GC_stdout, "ShutDown Leaks\n");
		GC_clear_roots();
		GC_gcollect();
	}
#endif
}

nsresult NS_ShutdownLeakDetector()
{
#if defined(SHUTDOWN_LEAKS_MEDIUM)
    // Make this the first atexit() called so it's before the atexit() crashes
    // see http://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=23552
    static LeakDetectorFinalizer trick;
#elif defined(SHUTDOWN_LEAKS_EARLY)
    // do shutdown leaks now
    LeakDetectorFinalizer trick;
#endif
	return NS_OK;
}

#endif /* defined(GC_LEAK_DETECTOR) */
