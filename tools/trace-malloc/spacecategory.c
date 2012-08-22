/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** spacecategory.c
**
** Cagtegorizes each allocation using a predefined set of rules
** and presents a tree of categories for browsing.
*/

/*
** Required include files.
*/
#include "spacetrace.h"
#include <ctype.h>
#include <string.h>

#include "nsQuickSort.h"

#if defined(HAVE_BOUTELL_GD)
/*
** See http://www.boutell.com/gd for the GD graphics library.
** Ports for many platorms exist.
** Your box may already have the lib (mine did, redhat 7.1 workstation).
*/
#include <gd.h>
#include <gdfontt.h>
#include <gdfonts.h>
#include <gdfontmb.h>
#endif /* HAVE_BOUTELL_GD */

/*
** AddRule
**
** Add a rule into the list of rules maintainted in global
*/
int
AddRule(STGlobals * g, STCategoryRule * rule)
{
    if (g->mNRules % ST_ALLOC_STEP == 0) {
        /* Need more space */
        STCategoryRule **newrules;

        newrules = (STCategoryRule **) realloc(g->mCategoryRules,
                                               (g->mNRules +
                                                ST_ALLOC_STEP) *
                                               sizeof(STCategoryRule *));
        if (!newrules) {
            REPORT_ERROR(__LINE__, AddRule_No_Memory);
            return -1;
        }
        g->mCategoryRules = newrules;
    }
    g->mCategoryRules[g->mNRules++] = rule;
    return 0;
}

/*
** AddChild
**
** Add the node as a child of the parent node
*/
int
AddChild(STCategoryNode * parent, STCategoryNode * child)
{
    if (parent->nchildren % ST_ALLOC_STEP == 0) {
        /* need more space */
        STCategoryNode **newnodes;

        newnodes = (STCategoryNode **) realloc(parent->children,
                                               (parent->nchildren +
                                                ST_ALLOC_STEP) *
                                               sizeof(STCategoryNode *));
        if (!newnodes) {
            REPORT_ERROR(__LINE__, AddChild_No_Memory);
            return -1;
        }
        parent->children = newnodes;
    }
    parent->children[parent->nchildren++] = child;
    return 0;
}

int
Reparent(STCategoryNode * parent, STCategoryNode * child)
{
    uint32_t i;

    if (child->parent == parent)
        return 0;

    /* Remove child from old parent */
    if (child->parent) {
        for (i = 0; i < child->parent->nchildren; i++) {
            if (child->parent->children[i] == child) {
                /* Remove child from list */
                if (i + 1 < child->parent->nchildren)
                    memmove(&child->parent->children[i],
                            &child->parent->children[i + 1],
                            (child->parent->nchildren - i -
                             1) * sizeof(STCategoryNode *));
                child->parent->nchildren--;
                break;
            }
        }
    }

    /* Add child into new parent */
    AddChild(parent, child);

    return 0;
}

/*
** findCategoryNode
**
** Given a category name, finds the Node corresponding to the category
*/
STCategoryNode *
findCategoryNode(const char *catName, STGlobals * g)
{
    uint32_t i;

    for (i = 0; i < g->mNCategoryMap; i++) {
        if (!strcmp(g->mCategoryMap[i]->categoryName, catName))
            return g->mCategoryMap[i]->node;
    }

    /* Check if we are looking for the root node */
    if (!strcmp(catName, ST_ROOT_CATEGORY_NAME))
        return &g->mCategoryRoot;

    return NULL;
}

/*
** AddCategoryNode
**
** Adds a mapping between a category and its Node into the categoryMap
*/
int
AddCategoryNode(STCategoryNode * node, STGlobals * g)
{
    if (g->mNCategoryMap % ST_ALLOC_STEP == 0) {
        /* Need more space */
        STCategoryMapEntry **newmap =
            (STCategoryMapEntry **) realloc(g->mCategoryMap,
                                            (g->mNCategoryMap +
                                             ST_ALLOC_STEP) *
                                            sizeof(STCategoryMapEntry *));
        if (!newmap) {
            REPORT_ERROR(__LINE__, AddCategoryNode_No_Memory);
            return -1;
        }
        g->mCategoryMap = newmap;

    }
    g->mCategoryMap[g->mNCategoryMap] =
        (STCategoryMapEntry *) calloc(1, sizeof(STCategoryMapEntry));
    if (!g->mCategoryMap[g->mNCategoryMap]) {
        REPORT_ERROR(__LINE__, AddCategoryNode_No_Memory);
        return -1;
    }
    g->mCategoryMap[g->mNCategoryMap]->categoryName = node->categoryName;
    g->mCategoryMap[g->mNCategoryMap]->node = node;
    g->mNCategoryMap++;
    return 0;
}

/*
** NewCategoryNode
**
** Creates a new category node for category name 'catname' and makes
** 'parent' its parent.
*/
STCategoryNode *
NewCategoryNode(const char *catName, STCategoryNode * parent, STGlobals * g)
{
    STCategoryNode *node;

    node = (STCategoryNode *) calloc(1, sizeof(STCategoryNode));
    if (!node)
        return NULL;

    node->runs =
        (STRun **) calloc(g->mCommandLineOptions.mContexts, sizeof(STRun *));
    if (NULL == node->runs) {
        free(node);
        return NULL;
    }

    node->categoryName = catName;

    /* Set parent of child */
    node->parent = parent;

    /* Set child in parent */
    AddChild(parent, node);

    /* Add node into mapping table */
    AddCategoryNode(node, g);

    return node;
}

/*
** ProcessCategoryLeafRule
**
** Add this into the tree as a leaf node. It doesn't know who its parent is. For now we make
** root as its parent
*/
int
ProcessCategoryLeafRule(STCategoryRule * leafRule, STCategoryNode * root,
                        STGlobals * g)
{
    STCategoryRule *rule;
    STCategoryNode *node;

    rule = (STCategoryRule *) calloc(1, sizeof(STCategoryRule));
    if (!rule)
        return -1;

    /* Take ownership of all elements of rule */
    *rule = *leafRule;

    /* Find/Make a STCategoryNode and add it into the tree */
    node = findCategoryNode(rule->categoryName, g);
    if (!node)
        node = NewCategoryNode(rule->categoryName, root, g);

    /* Make sure rule knows which node to access */
    rule->node = node;

    /* Add rule into rulelist */
    AddRule(g, rule);

    return 0;
}

/*
** ProcessCategoryParentRule
**
** Rule has all the children of category as patterns. Sets up the tree so that
** the parent child relationship is honored.
*/
int
ProcessCategoryParentRule(STCategoryRule * parentRule, STCategoryNode * root,
                          STGlobals * g)
{
    STCategoryNode *node;
    STCategoryNode *child;
    uint32_t i;

    /* Find the parent node in the tree. If not make one and add it into the tree */
    node = findCategoryNode(parentRule->categoryName, g);
    if (!node) {
        node = NewCategoryNode(parentRule->categoryName, root, g);
        if (!node)
            return -1;
    }

    /* For every child node, Find/Create it and make it the child of this node */
    for (i = 0; i < parentRule->npats; i++) {
        child = findCategoryNode(parentRule->pats[i], g);
        if (!child) {
            child = NewCategoryNode(parentRule->pats[i], node, g);
            if (!child)
                return -1;
        }
        else {
            /* Reparent child to node. This is because when we created the node
             ** we would have created it as the child of root. Now we need to
             ** remove it from root's child list and add it into this node
             */
            Reparent(node, child);
        }
    }

    return 0;
}

/*
** initCategories
**
** Initialize all categories. This reads in a file that says how to categorize
** each callsite, creates a tree of these categories and makes a list of these
** patterns in order for matching
*/
int
initCategories(STGlobals * g)
{
    FILE *fp;
    char buf[1024], *in;
    int n;
    PRBool inrule, leaf;
    STCategoryRule rule;

    fp = fopen(g->mCommandLineOptions.mCategoryFile, "r");
    if (!fp) {
        /* It isn't an error to not have a categories file */
        REPORT_INFO("No categories file.");
        return -1;
    }

    inrule = PR_FALSE;
    leaf = PR_FALSE;

    memset(&rule, 0, sizeof(rule));

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        /* Lose the \n */
        n = strlen(buf);
        if (buf[n - 1] == '\n')
            buf[--n] = '\0';
        in = buf;

        /* skip comments */
        if (*in == '#')
            continue;

        /* skip empty lines. If we are in a rule, end the rule. */
        while (*in && isspace(*in))
            in++;
        if (*in == '\0') {
            if (inrule) {
                /* End the rule : leaf or non-leaf */
                if (leaf)
                    ProcessCategoryLeafRule(&rule, &g->mCategoryRoot, g);
                else
                    /* non-leaf */
                    ProcessCategoryParentRule(&rule, &g->mCategoryRoot, g);
                inrule = PR_FALSE;
                memset(&rule, 0, sizeof(rule));
            }
            continue;
        }

        /* if we are in a rule acculumate */
        if (inrule) {
            rule.pats[rule.npats] = strdup(in);
            rule.patlen[rule.npats++] = strlen(in);
        }
        else if (*in == '<') {
            /* Start a category */
            inrule = PR_TRUE;
            leaf = PR_TRUE;

            /* Get the category name */
            in++;
            n = strlen(in);
            if (in[n - 1] == '>')
                in[n - 1] = '\0';
            rule.categoryName = strdup(in);
        }
        else {
            /* this is a non-leaf category. Should be of the form CategoryName
             ** followed by list of child category names one per line
             */
            inrule = PR_TRUE;
            leaf = PR_FALSE;
            rule.categoryName = strdup(in);
        }
    }

    /* If we were in a rule when processing the last line, end the rule */
    if (inrule) {
        /* End the rule : leaf or non-leaf */
        if (leaf)
            ProcessCategoryLeafRule(&rule, &g->mCategoryRoot, g);
        else
            /* non-leaf */
            ProcessCategoryParentRule(&rule, &g->mCategoryRoot, g);
    }

    /* Add the final "uncategorized" category. We make new memory locations
     ** for all these to conform to the general principle of all strings are allocated
     ** so it makes release logic very simple.
     */
    memset(&rule, 0, sizeof(rule));
    rule.categoryName = strdup("uncategorized");
    rule.pats[0] = strdup("");
    rule.patlen[0] = 0;
    rule.npats = 1;
    ProcessCategoryLeafRule(&rule, &g->mCategoryRoot, g);

    return 0;
}

/*
** callsiteMatchesRule
**
** Returns the corresponding node if callsite matches the rule. Rule is a sequence
** of patterns that must match contiguously the callsite.
*/
int
callsiteMatchesRule(tmcallsite * aCallsite, STCategoryRule * aRule)
{
    uint32_t patnum = 0;
    const char *methodName = NULL;

    while (patnum < aRule->npats && aCallsite && aCallsite->method) {
        methodName = tmmethodnode_name(aCallsite->method);
        if (!methodName)
            return 0;
        if (!*aRule->pats[patnum]
            || !strncmp(methodName, aRule->pats[patnum],
                        aRule->patlen[patnum])) {
            /* We have matched so far. Proceed up the stack and to the next pattern */
            patnum++;
            aCallsite = aCallsite->parent;
        }
        else {
            /* Deal with mismatch */
            if (patnum > 0) {
                /* contiguous mismatch. Stop */
                return 0;
            }
            /* We still haven't matched the first pattern. Proceed up the stack without
             ** moving to the next pattern.
             */
            aCallsite = aCallsite->parent;
        }
    }

    if (patnum == aRule->npats) {
        /* all patterns matched. We have a winner. */
#if defined(DEBUG_dp) && 0
        fprintf(stderr, "[%s] match\n", aRule->categoryName);
#endif
        return 1;
    }

    return 0;
}

#ifdef DEBUG_dp
PRIntervalTime _gMatchTime = 0;
uint32_t _gMatchCount = 0;
uint32_t _gMatchRules = 0;
#endif

/*
** matchAllocation
**
** Runs through all rules and returns the node corresponding to
** a match of the allocation.
*/
STCategoryNode *
matchAllocation(STGlobals * g, STAllocation * aAllocation)
{
#ifdef DEBUG_dp
    PRIntervalTime start = PR_IntervalNow();
#endif
    uint32_t rulenum;
    STCategoryNode *node = NULL;
    STCategoryRule *rule;

    for (rulenum = 0; rulenum < g->mNRules; rulenum++) {
#ifdef DEBUG_dp
        _gMatchRules++;
#endif
        rule = g->mCategoryRules[rulenum];
        if (callsiteMatchesRule(aAllocation->mEvents[0].mCallsite, rule)) {
            node = rule->node;
            break;
        }
    }
#ifdef DEBUG_dp
    _gMatchCount++;
    _gMatchTime += PR_IntervalNow() - start;
#endif
    return node;
}

/*
** categorizeAllocation
**
** Given an allocation, it adds it into the category tree at the right spot
** by comparing the allocation to the rules and adding into the right node.
** Also, does propogation of cost upwards in the tree.
** The root of the tree is in the globls as the tree is dependent on the
** category file (options) rather than the run.
*/
int
categorizeAllocation(STOptions * inOptions, STContext * inContext,
                     STAllocation * aAllocation, STGlobals * g)
{
    /* Run through the rules in order to see if this allcation matches
     ** any of them.
     */
    STCategoryNode *node;

    node = matchAllocation(g, aAllocation);
    if (!node) {
        /* ugh! it should atleast go into the "uncategorized" node. wierd!
         */
        REPORT_ERROR(__LINE__, categorizeAllocation);
        return -1;
    }

    /* Create run for node if not already */
    if (!node->runs[inContext->mIndex]) {
        /*
         ** Create run with positive timestamp as we can harvest it later
         ** for callsite details summarization
         */
        node->runs[inContext->mIndex] =
            createRun(inContext, PR_IntervalNow());
        if (!node->runs[inContext->mIndex]) {
            REPORT_ERROR(__LINE__, categorizeAllocation_No_Memory);
            return -1;
        }
    }

    /* Add allocation into node. We expand the table of allocations in steps */
    if (node->runs[inContext->mIndex]->mAllocationCount % ST_ALLOC_STEP == 0) {
        /* Need more space */
        STAllocation **allocs;

        allocs =
            (STAllocation **) realloc(node->runs[inContext->mIndex]->
                                      mAllocations,
                                      (node->runs[inContext->mIndex]->
                                       mAllocationCount +
                                       ST_ALLOC_STEP) *
                                      sizeof(STAllocation *));
        if (!allocs) {
            REPORT_ERROR(__LINE__, categorizeAllocation_No_Memory);
            return -1;
        }
        node->runs[inContext->mIndex]->mAllocations = allocs;
    }
    node->runs[inContext->mIndex]->mAllocations[node->
                                                runs[inContext->mIndex]->
                                                mAllocationCount++] =
        aAllocation;

    /*
     ** Make sure run's stats are calculated. We don't go update the parents of allocation
     ** at this time. That will happen when we focus on this category. This updating of
     ** stats will provide us fast categoryreports.
     */
    recalculateAllocationCost(inOptions, inContext,
                              node->runs[inContext->mIndex], aAllocation,
                              PR_FALSE);

    /* Propagate upwards the statistics */
    /* XXX */
#if defined(DEBUG_dp) && 0
    fprintf(stderr, "DEBUG: [%s] match\n", node->categoryName);
#endif
    return 0;
}

typedef PRBool STCategoryNodeProcessor(STRequest * inRequest,
                                       STOptions * inOptions,
                                       STContext * inContext,
                                       void *clientData,
                                       STCategoryNode * node);

PRBool
freeNodeRunProcessor(STRequest * inRequest, STOptions * inOptions,
                     STContext * inContext, void *clientData,
                     STCategoryNode * node)
{
    if (node->runs && node->runs[inContext->mIndex]) {
        freeRun(node->runs[inContext->mIndex]);
        node->runs[inContext->mIndex] = NULL;
    }
    return PR_TRUE;
}

PRBool
freeNodeRunsProcessor(STRequest * inRequest, STOptions * inOptions,
                      STContext * inContext, void *clientData,
                      STCategoryNode * node)
{
    if (node->runs) {
        uint32_t loop = 0;

        for (loop = 0; loop < globals.mCommandLineOptions.mContexts; loop++) {
            if (node->runs[loop]) {
                freeRun(node->runs[loop]);
                node->runs[loop] = NULL;
            }
        }

        free(node->runs);
        node->runs = NULL;
    }

    return PR_TRUE;
}

#if defined(DEBUG_dp)
PRBool
printNodeProcessor(STRequest * inRequest, STOptions * inOptions,
                   STContext * inContext, void *clientData,
                   STCategoryNode * node)
{
    STCategoryNode *root = (STCategoryNode *) clientData;

    fprintf(stderr, "%-25s [ %9s size", node->categoryName,
            FormatNumber(node->run ? node->run->mStats[inContext->mIndex].
                         mSize : 0));
    fprintf(stderr, ", %5.1f%%",
            node->run ? ((double) node->run->mStats[inContext->mIndex].mSize /
                         root->run->mStats[inContext->mIndex].mSize *
                         100) : 0);
    fprintf(stderr, ", %7s allocations ]\n",
            FormatNumber(node->run ? node->run->mStats[inContext->mIndex].
                         mCompositeCount : 0));
    return PR_TRUE;
}

#endif

typedef struct __struct_optcon
{
    STOptions *mOptions;
    STContext *mContext;
}
optcon;

/*
** compareNode
**
** qsort callback.
** Compare the nodes as specified by the options.
*/
int
compareNode(const void *aNode1, const void *aNode2, void *aContext)
{
    int retval = 0;
    STCategoryNode *node1, *node2;
    uint32_t a, b;
    optcon *oc = (optcon *) aContext;

    if (!aNode1 || !aNode2 || !oc->mOptions || !oc->mContext)
        return 0;

    node1 = *((STCategoryNode **) aNode1);
    node2 = *((STCategoryNode **) aNode2);

    if (node1 && node2) {
        if (oc->mOptions->mOrderBy == ST_COUNT) {
            a = (node1->runs[oc->mContext->mIndex]) ? node1->runs[oc->
                                                                  mContext->
                                                                  mIndex]->
                mStats[oc->mContext->mIndex].mCompositeCount : 0;
            b = (node2->runs[oc->mContext->mIndex]) ? node2->runs[oc->
                                                                  mContext->
                                                                  mIndex]->
                mStats[oc->mContext->mIndex].mCompositeCount : 0;
        }
        else {
            /* Default is by size */
            a = (node1->runs[oc->mContext->mIndex]) ? node1->runs[oc->
                                                                  mContext->
                                                                  mIndex]->
                mStats[oc->mContext->mIndex].mSize : 0;
            b = (node2->runs[oc->mContext->mIndex]) ? node2->runs[oc->
                                                                  mContext->
                                                                  mIndex]->
                mStats[oc->mContext->mIndex].mSize : 0;
        }
        if (a < b)
            retval = __LINE__;
        else
            retval = -__LINE__;
    }
    return retval;
}

PRBool
sortNodeProcessor(STRequest * inRequest, STOptions * inOptions,
                  STContext * inContext, void *clientData,
                  STCategoryNode * node)
{
    if (node->nchildren) {
        optcon context;

        context.mOptions = inOptions;
        context.mContext = inContext;

        NS_QuickSort(node->children, node->nchildren,
                     sizeof(STCategoryNode *), compareNode, &context);
    }

    return PR_TRUE;
}


/*
** walkTree
**
** General purpose tree walker. Issues callback for each node.
** If 'maxdepth' > 0, then stops after processing that depth. Root is
** depth 0, the nodes below it are depth 1 etc...
*/
#define MODINC(n, mod) ((n+1) % mod)

void
walkTree(STCategoryNode * root, STCategoryNodeProcessor func,
         STRequest * inRequest, STOptions * inOptions, STContext * inContext,
         void *clientData, int maxdepth)
{
    STCategoryNode *nodes[1024], *node;
    uint32_t begin, end, i;
    int ret = 0;
    int curdepth = 0, ncurdepth = 0;

    nodes[0] = root;
    begin = 0;
    end = 1;
    ncurdepth = 1;
    while (begin != end) {
        node = nodes[begin];
        ret = (*func) (inRequest, inOptions, inContext, clientData, node);
        if (!ret) {
            /* Abort */
            break;
        }
        begin = MODINC(begin, 1024);
        for (i = 0; i < node->nchildren; i++) {
            nodes[end] = node->children[i];
            end = MODINC(end, 1024);
        }
        /* Depth tracking. Do it only if walkTree is contrained by a maxdepth */
        if (maxdepth > 0 && --ncurdepth == 0) {
            /*
             ** No more children in current depth. The rest of the nodes
             ** we have in our list should be nodes in the depth below us.
             */
            ncurdepth = (begin < end) ? (end - begin) : (1024 - begin + end);
            if (++curdepth > maxdepth) {
                /*
                 ** Gone too deep. Stop.
                 */
                break;
            }
        }
    }
    return;
}

int
freeRule(STCategoryRule * rule)
{
    uint32_t i;
    char *p = (char *) rule->categoryName;

    PR_FREEIF(p);

    for (i = 0; i < rule->npats; i++)
        free(rule->pats[i]);

    free(rule);
    return 0;
}

void
freeNodeRuns(STCategoryNode * root)
{
    walkTree(root, freeNodeRunsProcessor, NULL, NULL, NULL, NULL, 0);
}

void
freeNodeMap(STGlobals * g)
{
    uint32_t i;

    /* all nodes are in the map table. Just delete all of those. */
    for (i = 0; i < g->mNCategoryMap; i++) {
        free(g->mCategoryMap[i]->node);
        free(g->mCategoryMap[i]);
    }
    free(g->mCategoryMap);
}

int
freeCategories(STGlobals * g)
{
    uint32_t i;

    /*
     ** walk the tree and free runs held in nodes
     */
    freeNodeRuns(&g->mCategoryRoot);

    /*
     ** delete nodemap. This is the where nodes get deleted.
     */
    freeNodeMap(g);

    /*
     ** delete rule stuff
     */
    for (i = 0; i < g->mNRules; i++) {
        freeRule(g->mCategoryRules[i]);
    }
    free(g->mCategoryRules);

    return 0;
}


/*
** categorizeRun
**
** categorize all the allocations of the run using the rules into
** a tree rooted at globls.mCategoryRoot
*/
int
categorizeRun(STOptions * inOptions, STContext * inContext,
              const STRun * aRun, STGlobals * g)
{
    uint32_t i;

#if defined(DEBUG_dp)
    PRIntervalTime start = PR_IntervalNow();

    fprintf(stderr, "DEBUG: categorizing run...\n");
#endif

    /*
     ** First, cleanup our tree
     */
    walkTree(&g->mCategoryRoot, freeNodeRunProcessor, NULL, inOptions,
             inContext, NULL, 0);

    if (g->mNCategoryMap > 0) {
        for (i = 0; i < aRun->mAllocationCount; i++) {
            categorizeAllocation(inOptions, inContext, aRun->mAllocations[i],
                                 g);
        }
    }

    /*
     ** the run is always going to be the one corresponding to the root node
     */
    g->mCategoryRoot.runs[inContext->mIndex] = (STRun *) aRun;
    g->mCategoryRoot.categoryName = ST_ROOT_CATEGORY_NAME;

#if defined(DEBUG_dp)
    fprintf(stderr,
            "DEBUG: categorizing ends: %dms [%d rules, %d allocations]\n",
            PR_IntervalToMilliseconds(PR_IntervalNow() - start), g->mNRules,
            aRun->mAllocationCount);
    fprintf(stderr, "DEBUG: match : %dms [%d calls, %d rule-compares]\n",
            PR_IntervalToMilliseconds(_gMatchTime), _gMatchCount,
            _gMatchRules);
#endif

    /*
     ** sort the tree based on our sort criterion
     */
    walkTree(&g->mCategoryRoot, sortNodeProcessor, NULL, inOptions, inContext,
             NULL, 0);

#if defined(DEBUG_dp)
    walkTree(&g->mCategoryRoot, printNodeProcessor, NULL, inOptions,
             inContext, &g->mCategoryRoot, 0);
#endif

    return 0;
}


/*
** displayCategoryReport
**
** Generate the category report - a list of all categories and details about each
** depth parameter controls how deep we traverse the category tree.
*/
PRBool
displayCategoryNodeProcessor(STRequest * inRequest, STOptions * inOptions,
                             STContext * inContext, void *clientData,
                             STCategoryNode * node)
{
    STCategoryNode *root = (STCategoryNode *) clientData;
    uint32_t byteSize = 0, heapCost = 0, count = 0;
    double percent = 0;
    STOptions customOps;

    if (node->runs[inContext->mIndex]) {
        /*
         ** Byte size
         */
        byteSize =
            node->runs[inContext->mIndex]->mStats[inContext->mIndex].mSize;

        /*
         ** Composite count
         */
        count =
            node->runs[inContext->mIndex]->mStats[inContext->mIndex].
            mCompositeCount;

        /*
         ** Heap operation cost
         **/
        heapCost =
            node->runs[inContext->mIndex]->mStats[inContext->mIndex].
            mHeapRuntimeCost;

        /*
         ** % of total size
         */
        if (root->runs[inContext->mIndex]) {
            percent =
                ((double) byteSize) /
                root->runs[inContext->mIndex]->mStats[inContext->mIndex].
                mSize * 100;
        }
    }

    PR_fprintf(inRequest->mFD, " <tr>\n" "  <td>");

    /* a link to topcallsites report with focus on category */
    memcpy(&customOps, inOptions, sizeof(customOps));
    PR_snprintf(customOps.mCategoryName, sizeof(customOps.mCategoryName),
                "%s", node->categoryName);

    htmlAnchor(inRequest, "top_callsites.html", node->categoryName, NULL,
               "category-callsites", &customOps);
    PR_fprintf(inRequest->mFD,
               "</td>\n" "  <td align=right>%u</td>\n"
               "  <td align=right>%4.1f%%</td>\n"
               "  <td align=right>%u</td>\n" "  <td align=right>"
               ST_MICROVAL_FORMAT "</td>\n" " </tr>\n", byteSize, percent,
               count, ST_MICROVAL_PRINTABLE(heapCost));

    return PR_TRUE;
}


int
displayCategoryReport(STRequest * inRequest, STCategoryNode * root, int depth)
{
    PR_fprintf(inRequest->mFD,
               "<table class=\"category-list data\">\n"
               " <tr class=\"row-header\">\n"
               "  <th>Category</th>\n"
               "  <th>Composite Byte Size</th>\n"
               "  <th>%% of Total Size</th>\n"
               "  <th>Heap Object Count</th>\n"
               "  <th>Composite Heap Operations Seconds</th>\n" " </tr>\n");

    walkTree(root, displayCategoryNodeProcessor, inRequest,
             &inRequest->mOptions, inRequest->mContext, root, depth);

    PR_fprintf(inRequest->mFD, "</table>\n");

    return 0;
}
