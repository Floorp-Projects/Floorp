/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * mozilla.org
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
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

#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsMemoryReporterManager.h"

#include "nsArrayEnumerator.h"

/**
 ** memory reporter implementation for jemalloc and OSX malloc,
 ** to obtain info on total memory in use (that we know about,
 ** at least -- on OSX, there are sometimes other zones in use).
 **/

/* Because of the way that jemalloc is linked on linux, we can't
 * get to jemalloc_stats().  So just do this on Windows until
 * that's fixed.
 */
#if defined(MOZ_MEMORY) && defined(XP_WIN)
#define HAVE_JEMALLOC_STATS 1
#else
#undef HAVE_JEMALLOC_STATS
#endif

#if defined(HAVE_JEMALLOC_STATS)
#define HAVE_MALLOC_REPORTERS 1
#include "jemalloc.h"

PRInt64 getMallocMapped(void *) {
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (PRInt64) stats.mapped;
}

PRInt64 getMallocAllocated(void *) {
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (PRInt64) stats.allocated;
}

PRInt64 getMallocCommitted(void *) {
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (PRInt64) stats.committed;
}

PRInt64 getMallocDirty(void *) {
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (PRInt64) stats.dirty;
}

#elif defined(XP_MACOSX) && !defined(MOZ_MEMORY)
#define HAVE_MALLOC_REPORTERS 1
#include <malloc/malloc.h>

static PRInt64 getMallocAllocated(void *) {
    struct mstats stats = mstats();
    return (PRInt64) stats.bytes_used;
}

static PRInt64 getMallocMapped(void *) {
    struct mstats stats = mstats();
    return (PRInt64) stats.bytes_total;
}

static PRInt64 getMallocDefaultCommitted(void *) {
    malloc_statistics_t stats;
    malloc_zone_statistics(malloc_default_zone(), &stats);
    return stats.size_in_use;
}

static PRInt64 getMallocDefaultAllocated(void *) {
    malloc_statistics_t stats;
    malloc_zone_statistics(malloc_default_zone(), &stats);
    return stats.size_allocated;
}

#endif


#ifdef HAVE_MALLOC_REPORTERS
NS_MEMORY_REPORTER_IMPLEMENT(MallocAllocated,
                             "malloc/allocated",
                             "Malloc bytes allocated (in use by application)",
                             getMallocAllocated,
                             NULL)

NS_MEMORY_REPORTER_IMPLEMENT(MallocMapped,
                             "malloc/mapped",
                             "Malloc bytes mapped (not necessarily committed)",
                             getMallocMapped,
                             NULL)

#if defined(HAVE_JEMALLOC_STATS)
NS_MEMORY_REPORTER_IMPLEMENT(MallocCommitted,
                             "malloc/committed",
                             "Malloc bytes committed (readable/writable)",
                             getMallocCommitted,
                             NULL)

NS_MEMORY_REPORTER_IMPLEMENT(MallocDirty,
                             "malloc/dirty",
                             "Malloc bytes dirty (committed unused pages)",
                             getMallocDirty,
                             NULL)
#elif defined(XP_MACOSX) && !defined(MOZ_MEMORY)
NS_MEMORY_REPORTER_IMPLEMENT(MallocDefaultCommitted,
                             "malloc/zone0/committed",
                             "Malloc bytes committed (r/w) in default zone",
                             getMallocDefaultCommitted,
                             NULL)

NS_MEMORY_REPORTER_IMPLEMENT(MallocDefaultAllocated,
                             "malloc/zone0/allocated",
                             "Malloc bytes allocated (in use) in default zone",
                             getMallocDefaultAllocated,
                             NULL)
#endif

#endif

/**
 ** nsMemoryReporterManager implementation
 **/

NS_IMPL_ISUPPORTS1(nsMemoryReporterManager, nsIMemoryReporterManager)

NS_IMETHODIMP
nsMemoryReporterManager::Init()
{
    nsCOMPtr<nsIMemoryReporter> mr;

    /*
     * Register our core jemalloc/malloc reporters
     */
#ifdef HAVE_MALLOC_REPORTERS
    mr = new NS_MEMORY_REPORTER_NAME(MallocAllocated);
    RegisterReporter(mr);

    mr = new NS_MEMORY_REPORTER_NAME(MallocMapped);
    RegisterReporter(mr);

#if defined(HAVE_JEMALLOC_STATS)
    mr = new NS_MEMORY_REPORTER_NAME(MallocCommitted);
    RegisterReporter(mr);

    mr = new NS_MEMORY_REPORTER_NAME(MallocDirty);
    RegisterReporter(mr);
#elif defined(XP_MACOSX) && !defined(MOZ_MEMORY)
    mr = new NS_MEMORY_REPORTER_NAME(MallocDefaultCommitted);
    RegisterReporter(mr);

    mr = new NS_MEMORY_REPORTER_NAME(MallocDefaultAllocated);
    RegisterReporter(mr);
#endif
#endif

    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::EnumerateReporters(nsISimpleEnumerator **result)
{
    return NS_NewArrayEnumerator(result, mReporters);
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterReporter(nsIMemoryReporter *reporter)
{
    if (mReporters.IndexOf(reporter) != -1)
        return NS_ERROR_FAILURE;

    mReporters.AppendObject(reporter);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::UnregisterReporter(nsIMemoryReporter *reporter)
{
    if (!mReporters.RemoveObject(reporter))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_COM nsresult
NS_RegisterMemoryReporter (nsIMemoryReporter *reporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nsnull)
        return NS_ERROR_FAILURE;
    return mgr->RegisterReporter(reporter);
}

NS_COM nsresult
NS_UnregisterMemoryReporter (nsIMemoryReporter *reporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nsnull)
        return NS_ERROR_FAILURE;
    return mgr->UnregisterReporter(reporter);
}

