/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is spacecategory.c code, released
 * Apr 12, 2002.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Suresh Duddi <dp@netscape.com>, 12-April-2002
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 */

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
** Forward declarations
**/
#if defined(DEBUG_dp)
extern void printCategoryTree(STCategoryNode* root);
#endif

/*
** AddRule
**
** Add a rule into the list of rules maintainted in global
*/
int AddRule(STGlobals *g, STCategoryRule *rule)
{
    if (g->mNRules % ST_ALLOC_STEP == 0)
    {
        /* Need more space */
        STCategoryRule** newrules;
        newrules = (STCategoryRule **) realloc(g->mCategoryRules,
                                               (g->mNRules + ST_ALLOC_STEP)*sizeof(STCategoryRule *));
        if (!newrules)
        {
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
int AddChild(STCategoryNode* parent, STCategoryNode* child)
{
    if (parent->nchildren % ST_ALLOC_STEP == 0)
    {
        /* need more space */
        STCategoryNode** newnodes;
        newnodes = (STCategoryNode **) realloc(parent->children,
                                               (parent->nchildren + ST_ALLOC_STEP)*sizeof(STCategoryNode *));
        if (!newnodes)
        {
            REPORT_ERROR(__LINE__, AddChild_No_Memory);
            return -1;
        }
        parent->children = newnodes;
    }
    parent->children[parent->nchildren++] = child;
    return 0;
}

int ReParent(STCategoryNode* parent, STCategoryNode* child)
{
    PRUint32 i;
    if (child->parent == parent)
        return 0;

    /* Remove child from old parent */
    if (child->parent)
    {
        for (i = 0; i < child->parent->nchildren; i++)
        {
            if (child->parent->children[i] == child)
            {
                /* Remove child from list */
                if (i+1 < child->parent->nchildren)
                    memmove(&child->parent->children[i], &child->parent->children[i+1],
                            (child->parent->nchildren - i - 1) * sizeof(STCategoryNode*));
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
STCategoryNode* findCategoryNode(const char *catName, STGlobals *g)
{
    PRUint32 i;
    for(i = 0; i < g->mNCategoryMap; i++)
    {
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
int AddCategoryNode(STCategoryNode* node, STGlobals* g)
{
    if (g->mNCategoryMap % ST_ALLOC_STEP == 0)
    {
        /* Need more space */
        STCategoryMapEntry **newmap = (STCategoryMapEntry **) realloc(g->mCategoryMap,
                                                                      (g->mNCategoryMap + ST_ALLOC_STEP) * sizeof(STCategoryMapEntry *));
        if (!newmap)
        {
            REPORT_ERROR(__LINE__, AddCategoryNode_No_Memory);
            return -1;
        }
        g->mCategoryMap = newmap;

    }
    g->mCategoryMap[g->mNCategoryMap] = (STCategoryMapEntry *) calloc(1, sizeof(STCategoryMapEntry));
    if (!g->mCategoryMap[g->mNCategoryMap])
    {
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
STCategoryNode* NewCategoryNode(const char* catName, STCategoryNode* parent, STGlobals* g)
{
    int i;
    STCategoryNode* node;

    node = (STCategoryNode *) calloc(1, sizeof(STCategoryNode));
    if (!node)
        return NULL;
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
** Add this into the tree as a leaf node. It doesnt know who its parent is. For now we make
** root as its parent
*/
int ProcessCategoryLeafRule(STCategoryRule* leafRule, STCategoryNode *root, STGlobals *g)
{
    STCategoryRule* rule;
    STCategoryNode* node;

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
int ProcessCategoryParentRule(STCategoryRule* parentRule, STCategoryNode *root, STGlobals* g)
{
    STCategoryNode* node;
    STCategoryNode* child;
    PRUint32 i;

    /* Find the parent node in the tree. If not make one and add it into the tree */
    node = findCategoryNode(parentRule->categoryName, g);
    if (!node)
    {
        node = NewCategoryNode(parentRule->categoryName, root, g);
        if (!node)
            return -1;
    }

    /* For every child node, Find/Create it and make it the child of this node */
    for(i = 0; i < parentRule->npats; i++)
    {
        child = findCategoryNode(parentRule->pats[i], g);
        if (!child)
        {
            child = NewCategoryNode(parentRule->pats[i], node, g);
            if (!child)
                return -1;
        }
        else
        {
            /* Reparent child to node. This is because when we created the node
            ** we would have created it as the child of root. Now we need to
            ** remove it from root's child list and add it into this node
            */
            ReParent(node, child);
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
int initCategories(STGlobals* g)
{
    FILE *fp;
    char buf[1024], *in;
    int n, i;
    PRBool inrule, leaf;
    STCategoryRule rule;

    fp = fopen(g->mOptions.mCategoryFile, "r");
    if (!fp)
    {
        /* It isnt an error to not have a categories file */
        REPORT_INFO("No categories file.");
        return -1;
    }

    inrule = PR_FALSE;
    leaf = PR_FALSE;

    memset(&rule, 0, sizeof(rule));

    while (fgets(buf, sizeof(buf), fp) != NULL)
    {
        /* Lose the \n */
        n = strlen(buf);
        if (buf[n-1] == '\n')
            buf[--n] = '\0';
        in = buf;

        /* skip comments */
        if (*in == '#')
            continue;

        /* skip empty lines. If we are in a rule, end the rule. */
        while(*in && isspace(*in))
            in++;
        if (*in == '\0') {
            if (inrule)
            {
                /* End the rule : leaf or non-leaf*/
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
        if (inrule)
        {
            rule.pats[rule.npats] = strdup(in);
            rule.patlen[rule.npats++] = strlen(in);
        }
        else if (*in == '<')
        {
            /* Start a category */
            inrule = PR_TRUE;
            leaf = PR_TRUE;

            /* Get the category name */
            in++;
            n = strlen(in);
            if (in[n-1] == '>')
                in[n-1] = '\0';
            rule.categoryName = strdup(in);
        }
        else
        {
            /* this is a non-leaf category. Should be of the form CategoryName
            ** followed by list of child category names one per line
            */
            inrule = PR_TRUE;
            leaf = PR_FALSE;
            rule.categoryName = strdup(in);
        }
    }

    /* If we were in a rule when processing the last line, end the rule */
    if (inrule)
    {
        /* End the rule : leaf or non-leaf*/
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
int callsiteMatchesRule(tmcallsite* aCallsite, STCategoryRule* aRule)
{
    PRUint32 patnum = 0;
    const char *methodName = NULL;

    while (patnum < aRule->npats && aCallsite && aCallsite->method)
    {
        methodName = tmmethodnode_name(aCallsite->method);
        if (!methodName)
            return 0;
        if (!*aRule->pats[patnum] || !strncmp(methodName, aRule->pats[patnum], aRule->patlen[patnum]))
        {
            /* We have matched so far. Proceed up the stack and to the next pattern */
            patnum++;
            aCallsite = aCallsite->parent;
        }
        else
        {
            /* Deal with mismatch */
            if (patnum > 0)
            {
                /* contiguous mismatch. Stop */
                return 0;
            }
            /* We still haven't matched the first pattern. Proceed up the stack without
            ** moving to the next pattern.
            */
            aCallsite = aCallsite->parent;
        }
    }

    if (patnum == aRule->npats)
    {
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
PRUint32 _gMatchCount = 0;
PRUint32 _gMatchRules = 0;
#endif

/*
** matchAllocation
**
** Runs through all rules and returns the node corresponding to
** a match of the allocation.
*/
STCategoryNode* matchAllocation(STGlobals* g, STAllocation* aAllocation)
{
#ifdef DEBUG_dp
    PRIntervalTime start = PR_IntervalNow();
#endif
    PRUint32 rulenum;
    STCategoryNode* node = NULL;
    STCategoryRule* rule;

    for (rulenum = 0; rulenum < g->mNRules; rulenum++)
    {
#ifdef DEBUG_dp
        _gMatchRules++;
#endif
        rule = g->mCategoryRules[rulenum];
        if (callsiteMatchesRule(aAllocation->mEvents[0].mCallsite, rule))
        {
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
int categorizeAllocation(STAllocation* aAllocation, STGlobals* g)
{
    /* Run through the rules in order to see if this allcation matches
    ** any of them.
    */
    STCategoryNode* node;

    node = matchAllocation(g, aAllocation);
    if (!node)
    {
        /* ugh! it should atleast go into the "uncategorized" node. wierd!
        */
        REPORT_ERROR(__LINE__, categorizeAllocation);
        return -1;
    }

    /* Create run for node if not already */
    if (!node->run)
    {
        /*
        ** Create run with positive timestamp as we can harvest it later
        ** for callsite details summarization
        */
        node->run = createRun(PR_IntervalNow());
        if (!node->run)
        {
            REPORT_ERROR(__LINE__, categorizeAllocation_No_Memory);
            return -1;
        }
    }

    /* Add allocation into node. We expand the table of allocations in steps */
    if (node->run->mAllocationCount % ST_ALLOC_STEP == 0)
    {
        /* Need more space */
        STAllocation** allocs;
        allocs = (STAllocation**) realloc(node->run->mAllocations,
                                          (node->run->mAllocationCount + ST_ALLOC_STEP) * sizeof(STAllocation*));
        if (!allocs)
        {
            REPORT_ERROR(__LINE__, categorizeAllocation_No_Memory);
            return -1;
        }
        node->run->mAllocations = allocs;
    }
    node->run->mAllocations[node->run->mAllocationCount++] = aAllocation;

    /*
    ** Make sure run's stats are calculated. We dont go update the parents of allocation
    ** at this time. That will happen when we focus on this category. This updating of
    ** stats will provide us fast categoryreports.
    */
    recalculateAllocationCost(node->run, aAllocation, PR_FALSE);

    /* Propogate upwards the statistics */
    /* XXX */
#if defined(DEBUG_dp) && 0
    fprintf(stderr, "DEBUG: [%s] match\n", node->categoryName);
#endif
    return 0;
}

typedef PRBool STCategoryNodeProcessor(void* clientData, STCategoryNode* node);

PRBool freeNodeRunProcessor(void* clientData, STCategoryNode* node)
{
    if (node->run)
    {
        freeRun(node->run);
        node->run = NULL;
    }
    return PR_TRUE;
}

#if defined(DEBUG_dp)
PRBool printNodeProcessor(void* clientData, STCategoryNode* node)
{
    STCategoryNode* root = (STCategoryNode*) clientData;
    if (node->nchildren)
        fprintf(stderr, "%-20s [%d children]\n", node->categoryName, node->nchildren);
    else
        fprintf(stderr, "%-20s [ %7d allocations, %7d size, %4.1f%% ]\n", node->categoryName,
                node->run ? node->run->mStats.mCompositeCount:0,
                node->run ? node->run->mStats.mSize:0,
                node->run ? ((double)node->run->mStats.mSize / root->run->mStats.mSize * 100):0
                );
    return PR_TRUE;
}

#endif

/*
** walkTree
**
** General purpose tree walker. Issues callback for each node.
** If 'maxdepth' > 0, then stops after processing that depth. Root is
** depth 0, the nodes below it are depth 1 etc...
*/
#define MODINC(n, mod) ((n+1) % mod)

void walkTree(STCategoryNode* root, STCategoryNodeProcessor func, void *clientData, int maxdepth)
{
    STCategoryNode* nodes[1024], *node;
    PRUint32 begin, end, i;
    int ret = 0;
    int curdepth = 0, ncurdepth = 0;

    nodes[0] = root;
    begin = 0; end = 1;
    ncurdepth = 1;
    while (begin != end)
    {
        node = nodes[begin];
        ret = (*func)(clientData, node);
        if (ret == PR_FALSE)
        {
            /* Abort */
            break;
        }
        begin = MODINC(begin, 1024);
        for (i = 0; i < node->nchildren; i++)
        {
            nodes[end] = node->children[i];
            end = MODINC(end, 1024);
        }
        /* Depth tracking. Do it only if walkTree is contrained by a maxdepth */
        if (maxdepth > 0 && --ncurdepth == 0)
        {
            /*
            ** No more children in current depth. The rest of the nodes
            ** we have in our list should be nodes in the depth below us.
            */
            ncurdepth = (begin < end) ? (end - begin) : (1024 - begin + end);
            if (++curdepth > maxdepth)
            {
                /*
                ** Gone too deep. Stop.
                */
                break;
            }
        }
    }
    return;
}

int freeRule(STCategoryRule* rule)
{
    PRUint32 i;
    char *p = (char *)rule->categoryName;
    PR_FREEIF(p);

    for (i = 0; i < rule->npats; i++)
        free(rule->pats[i]);

    free(rule);
    return 0;
}

void freeNodeRun(STCategoryNode* root)
{
   walkTree(root, freeNodeRunProcessor, NULL, 0);
}

void freeNodeMap(STGlobals* g)
{
    PRUint32 i;

    /* all nodes are in the map table. Just delete all of those. */
    for(i = 0; i < g->mNCategoryMap; i++)
    {
        free(g->mCategoryMap[i]->node);
        free(g->mCategoryMap[i]);
    }
    free(g->mCategoryMap);
}

int freeCategories(STGlobals* g)
{
    PRUint32 i;

    /*
    ** walk the tree and free runs held in nodes
    */
    freeNodeRun(&g->mCategoryRoot);

    /*
    ** delete nodemap. This is the where nodes get deleted.
    */
    freeNodeMap(g);

    /*
    ** delete rule stuff
    */
    for (i = 0; i < g->mNRules; i++)
    {
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
int categorizeRun(const STRun* aRun, STGlobals* g)
{
    PRUint32 i;
#if defined(DEBUG_dp)
    PRIntervalTime start = PR_IntervalNow();
    fprintf(stderr, "DEBUG: categorizing run...\n");
#endif

    /*
    ** First, cleanup our tree
    */
    walkTree(&g->mCategoryRoot, freeNodeRunProcessor, NULL, 0);

    if (g->mNCategoryMap > 0)
    {
        for (i = 0; i < aRun->mAllocationCount; i++)
        {
            categorizeAllocation(aRun->mAllocations[i], g);
        }
    }

    /*
    ** the run is always going to be the one corresponding to the root node
    */
    g->mCategoryRoot.run = (STRun *) aRun;
    g->mCategoryRoot.categoryName = ST_ROOT_CATEGORY_NAME;

#if defined(DEBUG_dp)
    walkTree(&g->mCategoryRoot, printNodeProcessor, &g->mCategoryRoot, 0);
    fprintf(stderr, "DEBUG: categorizing ends: %dms [%d rules, %d allocations]\n",
            PR_IntervalToMilliseconds(PR_IntervalNow() - start), g->mNRules, aRun->mAllocationCount);
    fprintf(stderr, "DEBUG: match : %dms [%d calls, %d rule-compares]\n",
            PR_IntervalToMilliseconds(_gMatchTime),
            _gMatchCount, _gMatchRules);
#endif

    return 0;
}


/*
** displayCategoryReport
**
** Generate the category report - a list of all categories and details about each
** depth parameter controls how deep we traverse the category tree.
*/
PRBool displayCategoryNodeProcessor(void* clientData, STCategoryNode* node)
{
    STCategoryNode* root = (STCategoryNode *) clientData;
    PRUint32 byteSize = 0, heapCost = 0, count = 0;
    double percent = 0;
    char buf[256];

    if (node->run)
    {
        /*
        ** Byte size
        */
        byteSize = node->run->mStats.mSize;

        /*
        ** Composite count
        */
        count = node->run->mStats.mCompositeCount;

        /*
        ** Heap operation cost
        **/
        heapCost = node->run->mStats.mHeapRuntimeCost;

        /*
        ** % of total size
        */
        if (root->run)
        {
            percent = ((double) byteSize) / root->run->mStats.mSize * 100;
        }
    }

    PR_fprintf(globals.mRequest.mFD,
               " <tr>\n"
               "  <td>");
    /* a link to topcallsites report with focus on category */
    PR_snprintf(buf, sizeof(buf), "top_callsites.html?mCategory=%s", node->categoryName);
    htmlAnchor(buf, node->categoryName, NULL);
    PR_fprintf(globals.mRequest.mFD,
               "</td>\n"
               "  <td align=right>%u</td>\n"
               "  <td align=right>%4.1f%%</td>\n"
               "  <td align=right>%u</td>\n"
               "  <td align=right>" ST_MICROVAL_FORMAT "</td>\n"
               " </tr>\n",
               byteSize, percent, count,
               ST_MICROVAL_PRINTABLE(heapCost));

    return PR_TRUE;
}


int displayCategoryReport(STCategoryNode *root, int depth)
{
    PR_fprintf(globals.mRequest.mFD,
               "<table border=1>\n"
               " <tr>\n"
               "  <th>Category</th>\n"
               "  <th>Composite Byte Size</th>\n"
               "  <th>%% of Total Size</th>\n"
               "  <th>Heap Object Count</th>\n"
               "  <th>Composite Heap Operations Seconds</th>\n"
               " </tr>\n"
               );

    walkTree(root, displayCategoryNodeProcessor, root, depth);

    PR_fprintf(globals.mRequest.mFD, "</table>\n");

    return 0;
}
