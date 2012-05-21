/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef spacetrace_h__
#define spacetrace_h__

/*
** spacetrace.h
**
** SpaceTrace is meant to take the output of trace-malloc and present
**   a picture of allocations over the run of the application.
*/

/*
** Required includes.
*/
#include "nspr.h"
#include "prlock.h"
#include "prrwlock.h"
#include "nsTraceMalloc.h"
#include "tmreader.h"
#include "formdata.h"

/*
** Turn on to attempt adding support for graphs on your platform.
*/
#if defined(HAVE_BOUTELL_GD)
#define ST_WANT_GRAPHS 1
#endif /* HAVE_BOUTELL_GD */
#if !defined(ST_WANT_GRAPHS)
#define ST_WANT_GRAPHS 0
#endif

/*
** REPORT_ERROR
** REPORT_INFO
**
** Just report errors and stuff in a consistent manner.
*/
#define REPORT_ERROR(code, function) \
        PR_fprintf(PR_STDERR, "error(%d):\t%s\n", code, #function)
#define REPORT_ERROR_MSG(code, msg) \
        PR_fprintf(PR_STDERR, "error(%d):\t%s\n", code, msg)
#define REPORT_INFO(msg) \
        PR_fprintf(PR_STDOUT, "%s: %s\n", globals.mProgramName, (msg))

#if defined(DEBUG_blythe) && 1
#define REPORT_blythe(code, msg) \
        PR_fprintf(PR_STDOUT, "gab(%d):\t%s\n", code, msg)
#else
#define REPORT_blythe(code, msg)
#endif /* DEBUG_blythe */

/*
** CALLSITE_RUN
**
** How to get a callsite run.
** Allows for further indirection if needed later.
*/
#define CALLSITE_RUN(callsite) \
        ((STRun*)((callsite)->data))

/*
** ST_PERMS
** ST_FLAGS
**
** File permissions we desire.
** 0644
*/
#define ST_PERMS (PR_IRUSR | PR_IWUSR | PR_IRGRP | PR_IROTH)
#define ST_FLAGS (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE) 

/*
** Sorting order
*/
#define ST_WEIGHT   0 /* size * timeval */
#define ST_SIZE     1
#define ST_TIMEVAL  2
#define ST_COUNT    3
#define ST_HEAPCOST 4

/*
** Callsite loop direction flags.
*/
#define ST_FOLLOW_SIBLINGS   0
#define ST_FOLLOW_PARENTS    1

/*
** Graph data.
*/
#define STGD_WIDTH           640
#define STGD_HEIGHT          480
#define STGD_MARGIN          75
#define STGD_SPACE_X         (STGD_WIDTH - (2 * STGD_MARGIN))
#define STGD_SPACE_Y         (STGD_HEIGHT - (2 * STGD_MARGIN))

/*
** Minimum lifetime default, in seconds.
*/
#define ST_DEFAULT_LIFETIME_MIN 10

/*
**  Allocations fall to this boundry size by default.
**  Overhead is taken after alignment.
**
**  The msvcrt malloc has an alignment of 16 with an overhead of 8.
**  The win32 HeapAlloc has an alignment of 8 with an overhead of 8.
*/
#define ST_DEFAULT_ALIGNMENT_SIZE 16
#define ST_DEFAULT_OVERHEAD_SIZE 8

/*
** Numer of substring match specifications to allow.
*/
#define ST_SUBSTRING_MATCH_MAX 5

/*
** Max Number of patterns per rule
*/
#define ST_MAX_PATTERNS_PER_RULE 16

/*
** Rule pointers and child pointers are allocated in steps of ST_ALLOC_STEP
*/
#define ST_ALLOC_STEP 16

/*
** Name of the root category. Appears in UI.
*/
#define ST_ROOT_CATEGORY_NAME "All"

/*
**  Size of our option string buffers.
*/
#define ST_OPTION_STRING_MAX 256

/*
** Set the desired resolution of the timevals.
** The resolution is just mimicking what is recorded in the trace-malloc
**  output, and that is currently milliseconds.
*/
#define ST_TIMEVAL_RESOLUTION 1000
#define ST_TIMEVAL_FORMAT "%.3f"
#define ST_TIMEVAL_PRINTABLE(timeval) ((PRFloat64)(timeval) / (PRFloat64)ST_TIMEVAL_RESOLUTION)
#define ST_TIMEVAL_PRINTABLE64(timeval) ((PRFloat64)((PRInt64)(timeval)) / (PRFloat64)ST_TIMEVAL_RESOLUTION)
#define ST_TIMEVAL_MAX ((PRUint32)-1 - ((PRUint32)-1 % ST_TIMEVAL_RESOLUTION))

#define ST_MICROVAL_RESOLUTION 1000000
#define ST_MICROVAL_FORMAT "%.6f"
#define ST_MICROVAL_PRINTABLE(timeval) ((PRFloat64)(timeval) / (PRFloat64)ST_MICROVAL_RESOLUTION)
#define ST_MICROVAL_PRINTABLE64(timeval) ((PRFloat64)((PRInt64)(timeval)) / (PRFloat64)ST_MICROVAL_RESOLUTION)
#define ST_MICROVAL_MAX ((PRUint32)-1 - ((PRUint32)-1 % ST_MICROVAL_RESOLUTION))

/*
** Forward Declaration
*/
typedef struct __struct_STCategoryNode STCategoryNode;
typedef struct __struct_STCategoryRule STCategoryRule;


/*
** STAllocEvent
**
** An event that happens to an allocation (malloc, free, et. al.)
*/
typedef struct __struct_STAllocEvent
{
        /*
        ** The type of allocation event.
        ** This maps directly to the trace malloc events (i.e. TM_EVENT_MALLOC)
        */
        char mEventType;
        
        /*
        ** Each event, foremost, has a chronologically increasing ID in
        **  relation to other allocation events.  This is a time stamp
        **  of sorts.
        */
        PRUint32 mTimeval;
        
        /*
        ** Every event has a heap ID (pointer).
        ** In the event of a realloc, this is the new heap ID.
        ** In the event of a free, this is the previous heap ID value.
        */
        PRUint32 mHeapID;
        
        /*
        ** Every event, along with the heap ID, tells of the size.
        ** In the event of a realloc, this is the new size.
        ** In th event of a free, this is the previous size.
        */
        PRUint32 mHeapSize;

        /*
        ** Every event has a callsite/stack backtrace.
        ** In the event of a realloc, this is the new callsite.
        ** In the event of a free, this is the previous call site.
        */
        tmcallsite* mCallsite;
} STAllocEvent;

/*
** STAllocation
**
** An allocation is a temporal entity in the heap.
** It possibly lives under different heap IDs (pointers) and different
**  sizes during its given time.
** An allocation is defined by the events during its lifetime.
** An allocation's lifetime is defined by the range of event IDs it holds.
*/
typedef struct __struct_STAllocation
{
        /*
        ** The array of events.
        */
        PRUint32 mEventCount;
        STAllocEvent* mEvents;
        
        /*
        ** The lifetime/lifespan of the allocation.
        */
        PRUint32 mMinTimeval;
        PRUint32 mMaxTimeval;

        /*
        ** Index of this allocation in the global run.
        */
        PRUint32 mRunIndex;

        /*
        ** The runtime cost of heap events in this allocation.
        ** The cost is defined as the number of time units recorded as being
        **  spent in heap code (time of malloc, free, et al.).
        ** We do not track individual event cost in order to save space.
        */
        PRUint32 mHeapRuntimeCost;
} STAllocation;

/*
** STCallsiteStats
**
** Stats regarding a run, kept mainly for callsite runs.
*/
typedef struct __struct_STCallsiteStats
{
        /*
        ** Sum timeval of the allocations.
        ** Callsite runs total all allocations below the callsite.
        */
        PRUint64 mTimeval64;

        /*
        ** Sum weight of the allocations.
        ** Callsite runs total all allocations below the callsite.
        */
        PRUint64 mWeight64;

        /*
        ** Sum size of the allocations.
        ** Callsite runs total all allocations below the callsite.
        */
        PRUint32 mSize;

        /*
        ** A stamp, indicated the relevance of the run.
        ** If the stamp does not match the origin value, the
        **  data contained here-in is considered invalid.
        */
        PRUint32 mStamp;

        /*
        ** A sum total of allocations (note, not sizes)  below the callsite.
        ** This is NOT the same as STRun::mAllocationCount which
        **  tracks the STRun::mAllocations array size.
        */
        PRUint32 mCompositeCount;

        /*
        ** A sum total runtime cost of heap operations below the calliste.
        ** The cost is defined as the number of time units recorded as being
        **  spent in heap code (time of malloc, free, et al.).
        */
        PRUint32 mHeapRuntimeCost;
} STCallsiteStats;

/*
** STRun
**
** A run is a closed set of allocations.
** Given a run, we can deduce information about the contained allocations.
** We can also determine if an allocation lives beyond a run (leak).
**
** A run might be used to represent allocations for an entire application.
** A run might also be used to represent allocations from a single callstack.
*/
typedef struct __struct_STRun
{
        /*
        ** The array of allocations.
        */
        PRUint32 mAllocationCount;
        STAllocation** mAllocations;

        /*
        ** Callsites like to keep some information.
        ** As callsites are possibly shared between all contexts, each
        **      different context needs to keep different stats.
        */
        STCallsiteStats *mStats;

} STRun;

/*
** Categorize allocations
**
** The objective is to have a tree of categories with each leaf node of the tree
** matching a set of callsites that belong to the category. Each category can
** signify a functional area like say css and hence the user can browse this
** tree looking for how much of each of these are live at an instant.
*/

/*
** STCategoryNode
*/

struct __struct_STCategoryNode
{
        /*
        ** Category name
        */
        const char *categoryName;

        /*
        ** Pointer to parent node. NULL for Root.
        */
        STCategoryNode *parent;

        /*
        ** For non-leaf nodes, an array of children node pointers.
        ** NULL if leaf node.
        */
        STCategoryNode** children;
        PRUint32 nchildren;

        /*
        ** The Run(s). Valid for both leaf and parent nodes.
        ** One run per --Context to handle multiple data sets.
        ** The relevant index for the particular request will be
        **      mIndex stored by the mContext of the request.
        */
        STRun **runs;
};


struct __struct_STCategoryRule
{
        /*
        ** The pattern for the rule. Patterns are an array of strings.
        ** A callsite needs to pass substring match for all the strings.
        */
        char* pats[ST_MAX_PATTERNS_PER_RULE];
        PRUint32 patlen[ST_MAX_PATTERNS_PER_RULE];
        PRUint32 npats;

        /*
        ** Category name that this rule belongs to
        */
        const char* categoryName;

        /*
        ** The node this should be categorized into
        */
        STCategoryNode* node;
};


/*
** CategoryName to Node mapping table
*/
typedef struct __struct_STCategoryMapEntry {
    STCategoryNode* node;
    const char * categoryName;
} STCategoryMapEntry;

/*
**  Option genres.
**
**  This helps to determine what functionality each option effects.
**  In specific, this will help use determine when and when not to
**      totally recaclulate the sorted run and categories.
**  Be very aware that adding things to a particular genre, or adding a genre,
**      may completely screw up the caching algorithms of SpaceTrace.
**  See contextLookup() or ask someone that knows if you are in doubt.
*/
typedef enum __enum_STOptionGenre
{
    CategoryGenre = 0,
    DataSortGenre,
    DataSetGenre,
    DataSizeGenre,
    UIGenre,
    ServerGenre,
    BatchModeGenre,

    /*
    **  Last one please.
    */
    MaxGenres
}
STOptionGenre;

/*
**  STOptions
**
**  Structure containing the varios options for the code.
**  The definition of these options exists in a different file.
**  We access that definition via macros to inline our structure definition.
*/
#define ST_CMD_OPTION_BOOL(option_name, option_genre, option_help) PRBool m##option_name;
#define ST_CMD_OPTION_STRING(option_name, option_genre, default_value, option_help) char m##option_name[ST_OPTION_STRING_MAX];
#define ST_CMD_OPTION_STRING_ARRAY(option_name, option_genre, array_size, option_help) char m##option_name[array_size][ST_OPTION_STRING_MAX];
#define ST_CMD_OPTION_STRING_PTR_ARRAY(option_name, option_genre, option_help) const char** m##option_name; PRUint32 m##option_name##Count;
#define ST_CMD_OPTION_UINT32(option_name, option_genre, default_value, multiplier, option_help) PRUint32 m##option_name;
#define ST_CMD_OPTION_UINT64(option_name, option_genre, default_value, multiplier, option_help) PRUint64 m##option_name##64;

typedef struct __struct_STOptions
{
#include "stoptions.h"
}
STOptions;

typedef struct __struct_STContext
/*
**  A per request, thread safe, manner of accessing the contained members.
**  A reader/writer lock ensures that the data is properly initialized before
**      readers of the data begin their work.
**
**  mRWLock             reader/writer lock.
**                      writer lock is held to ensure initialization, though
**                          others can be attempting to acquire read locks
**                          at that time.
**                      writer lock is also used in destruction to make sure
**                          there are no more readers of data contained herein.
**                      reader lock is to allow multiple clients to read the
**                          data at the same time; implies is they must not
**                          write anything.
**  mIndex              Consider this much like thread private data or thread
**                          local storage in a few places.
**                      The index is specifically reserved for this context's
**                          usage in other data structure array's provided
**                          for the particular thread/client/context.
**                      This should not be modified after initialization.
**  mSortedRun          A pre sorted run taken from the global run, with our
**                          options applied.
**  mImageLock          An overly simplistic locking mechanism to protect the
**                          shared image cache.
**                      The proper implementation would have a reader/writer
**                          lock per cached image data.
**                      However, this will prove to be simpler for the time
**                          being.
**  mFootprintCached    Whether or not YData contains something useful.
**  mTimevalCached      Whether or not YData contains something useful.
**  mLifespanCached     Whether or not YData contains something useful.
**  mWeightCached       Whether or not YData contains something useful.
**  mFootprintYData     Precomputed cached graph data.
**  mTimevalYData       Precomputed cached graph data.
**  mLifespanYData      Precomputed cached graph data.
**  mWeightYData        Precomputed cached graph data.
*/
{
    PRRWLock* mRWLock;
    PRUint32 mIndex;
    STRun* mSortedRun;
#if ST_WANT_GRAPHS
    PRLock* mImageLock;
    PRBool mFootprintCached;
    PRBool mTimevalCached;
    PRBool mLifespanCached;
    PRBool mWeightCached;
    PRUint32 mFootprintYData[STGD_SPACE_X];
    PRUint32 mTimevalYData[STGD_SPACE_X];
    PRUint32 mLifespanYData[STGD_SPACE_X];
    PRUint64 mWeightYData64[STGD_SPACE_X];
#endif
}
STContext;


typedef struct __struct_STContextCacheItem
/*
**  This basically pools the common items that the context cache will
**      want to track on a per context basis.
**
**  mOptions        What options this item represents.
**  mContext        State/data this cache item is wrapping.
**  mReferenceCount A count of clients currently using this item.
**                  Should this item be 0, then the cache might
**                      decide to evict this context.
**                  Should this item not be 0, once it reaches
**                      zero a condition variable in the context cache
**                      will be signaled to notify the availability.
**  mLastAccessed   A timestamp of when this item was last accessed/released.
**                  Ignore this unless the reference count is 0,
**                  This is used to evict the oldest unused item from
**                      the context cache.
**  mInUse          Mainly PR_FALSE only at the beginning of the process,
**                      but this indicates that the item has not yet been
**                      used at all, and thus shouldn't be evaluated for
**                      a cache hit.
*/
{
    STOptions mOptions;
    STContext mContext;
    PRInt32 mReferenceCount;
    PRIntervalTime mLastAccessed;
    PRBool mInUse;
}
STContextCacheItem;


typedef struct __struct_STContextCache
/*
**  A thread safe, possibly blocking, cache of context items.
**
**  mLock       Must hold the lock to read/access/write to this struct, as
**                  well as any items it holds.
**  mCacheMiss  All items are busy and there were no cache matches.
**              This condition variable is used to wait until an item becomes
**                  "available" to be evicted from the cache.
**  mItems      Array of items.
**  mItemCount  Number of items in array.
**              This is generally the same as the global option's command line
**                  mContexts....
*/
{
    PRLock* mLock;
    PRCondVar* mCacheMiss;
    STContextCacheItem* mItems;
    PRUint32 mItemCount;
}
STContextCache;


/*
** STRequest
**
** Things specific to a request.
*/
typedef struct __struct_STRequest
{
        /*
        ** Sink/where to output.
        */
        PRFileDesc* mFD;

        /*
        ** The filename requested.
        */
        const char* mGetFileName;

        /*
        **  The GET form data, if any.
        */
        const FormData* mGetData;

        /*
        **  Options specific to this request.
        */
        STOptions mOptions;

        /*
        **  The context/data/state of the request.
        */
        STContext* mContext;
} STRequest;


/*
** STGlobals
**
** Various globals we keep around.
*/
typedef struct __struct_STGlobals
{
        /*
        ** The string which identifies this program.
        */
        const char* mProgramName;

        /*
        **  Options derived from the command line.
        **  These are used as defaults, and should remain static during
        **      the run of the application.
        */
        STOptions mCommandLineOptions;

        /*
        **  Context cache.
        **  As clients come in, based on their options, a different context
        **      will be used to service them.
        */
        STContextCache mContextCache;

        /*
        ** Various counters for different types of events.
        */
        PRUint32 mMallocCount;
        PRUint32 mCallocCount;
        PRUint32 mReallocCount;
        PRUint32 mFreeCount;

        /*
        ** Total events, operation counter.
        */
        PRUint32 mOperationCount;

        /*
        ** The "run" of the input.
        */
        STRun mRun;

        /*
        ** Operation minimum/maximum timevals.
        ** So that we can determine the overall timeval of the run.
        ** NOTE:  These are NOT the options to control the data set.
        */
        PRUint32 mMinTimeval;
        PRUint32 mMaxTimeval;

        /*
        ** Calculates peak allocation overall for all allocations.
        */
        PRUint32 mPeakMemoryUsed;
        PRUint32 mMemoryUsed;

        /*
        ** A list of rules for categorization read in from the mCategoryFile
        */
       STCategoryRule** mCategoryRules;
       PRUint32 mNRules;

       /*
       ** CategoryName to Node mapping table
       */
       STCategoryMapEntry** mCategoryMap;
       PRUint32 mNCategoryMap;

       /*
       ** Categorized allocations. For now we support only one tree.
       */
       STCategoryNode mCategoryRoot;

       /*
       **   tmreader hash tables.
       **   Moved into globals since we need to destroy these only after all
       **       client threads are finishes (after PR_Cleanup).
       */
       tmreader* mTMR;
} STGlobals;


/*
** Function prototypes
*/
extern STRun* createRun(STContext* inContext, PRUint32 aStamp);
extern void freeRun(STRun* aRun);
extern int initCategories(STGlobals* g);
extern int categorizeRun(STOptions* inOptions, STContext* inContext, const STRun* aRun, STGlobals* g);
extern STCategoryNode* findCategoryNode(const char *catName, STGlobals *g);
extern int freeCategories(STGlobals* g);
extern int displayCategoryReport(STRequest* inRequest, STCategoryNode *root, int depth);

extern int recalculateAllocationCost(STOptions* inOptions, STContext* inContext, STRun* aRun, STAllocation* aAllocation, PRBool updateParent);
extern void htmlHeader(STRequest* inRequest, const char* aTitle);
extern void htmlFooter(STRequest* inRequest);
extern void htmlAnchor(STRequest* inRequest,
                       const char* aHref,
                       const char* aText,
                       const char* aTarget,
                       const char* aClass,
                       STOptions* inOptions);
extern char *FormatNumber(PRInt32 num);

/*
** shared globals
*/
extern STGlobals globals;

#endif /* spacetrace_h__ */
