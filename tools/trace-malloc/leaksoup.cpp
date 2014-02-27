/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "adreader.h"

#include <stdio.h>
#include "plhash.h"

#include "nsTArray.h"
#include "nsQuickSort.h"
#include "nsXPCOM.h"

const uint32_t kPointersDefaultSize = 8;

/*
 * Read in an allocation dump, presumably one taken at shutdown (using
 * the --shutdown-leaks=file option, which must be used along with
 * --trace-malloc=tmlog), and treat the memory in the dump as leaks.
 * Find the leak roots, including cycles that are roots, by finding the
 * strongly connected components in the graph.  Print output to stdout
 * as HTML.
 */

struct AllocationNode {
    const ADLog::Entry *entry;

    // Other |AllocationNode| objects whose memory has a pointer to
    // this object.
    nsAutoTArray<AllocationNode*, kPointersDefaultSize> pointers_to;

    // The reverse.
    nsAutoTArray<AllocationNode*, kPointersDefaultSize> pointers_from;

    // Early on in the algorithm, the pre-order index from a DFS.
    // Later on, set to the index of the strongly connected component to
    // which this node belongs.
    uint32_t index;

    bool reached;
    bool is_root;
};

static PLHashNumber hash_pointer(const void *key)
{
    return (PLHashNumber) NS_PTR_TO_INT32(key);
}

static int sort_by_index(const void* e1, const void* e2, void*)
{
    const AllocationNode *n1 = *static_cast<const AllocationNode*const*>(e1);
    const AllocationNode *n2 = *static_cast<const AllocationNode*const*>(e2);
    return n1->index - n2->index;
}

static int sort_by_reverse_index(const void* e1, const void* e2, void*)
{
    const AllocationNode *n1 = *static_cast<const AllocationNode*const*>(e1);
    const AllocationNode *n2 = *static_cast<const AllocationNode*const*>(e2);
    return n2->index - n1->index;
}

static void print_escaped(FILE *aStream, const char* aData)
{
    char c;
    char buf[1000];
    char *p = buf;
    while ((c = *aData++)) {
        switch (c) {
#define CH(char) *p++ = char
            case '<':
                CH('&'); CH('l'); CH('t'); CH(';');
                break;
            case '>':
                CH('&'); CH('g'); CH('t'); CH(';');
                break;
            case '&':
                CH('&'); CH('a'); CH('m'); CH('p'); CH(';');
                break;
            default:
                CH(c);
                break;
#undef CH
        }
        if (p + 10 > buf + sizeof(buf)) {
            *p = '\0';
            fputs(buf, aStream);
            p = buf;
        }
    }
    *p = '\0';
    fputs(buf, aStream);
}

static const char *allocation_format =
  (sizeof(ADLog::Pointer) == 4) ? "0x%08zX" :
  (sizeof(ADLog::Pointer) == 8) ? "0x%016zX" :
  "UNEXPECTED sizeof(void*)";

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr,
                "Expected usage:  %s <sd-leak-file>\n"
                "  sd-leak-file: Output of --shutdown-leaks=<file> option.\n",
                argv[0]);
        return 1;
    }

    NS_InitXPCOM2(nullptr, nullptr, nullptr);

    ADLog log;
    if (!log.Read(argv[1])) {
        fprintf(stderr,
                "%s: Error reading input file %s.\n", argv[0], argv[1]);
    }

    const size_t count = log.count();

    PLHashTable *memory_map =
        PL_NewHashTable(count * 8, hash_pointer, PL_CompareValues,
                        PL_CompareValues, 0, 0);
    if (!memory_map) {
        fprintf(stderr, "%s: Out of memory.\n", argv[0]);
        return 1;
    }

    // Create one |AllocationNode| object for each log entry, and create
    // entries in the hashtable pointing to it for each byte it occupies.
    AllocationNode *nodes = new AllocationNode[count];
    if (!nodes) {
        fprintf(stderr, "%s: Out of memory.\n", argv[0]);
        return 1;
    }

    {
        AllocationNode *cur_node = nodes;
        for (ADLog::const_iterator entry = log.begin(), entry_end = log.end();
             entry != entry_end; ++entry, ++cur_node) {
            const ADLog::Entry *e = cur_node->entry = *entry;
            cur_node->reached = false;

            for (ADLog::Pointer p = e->address,
                            p_end = e->address + e->datasize;
                 p != p_end; ++p) {
                PLHashEntry *he = PL_HashTableAdd(memory_map, p, cur_node);
                if (!he) {
                    fprintf(stderr, "%s: Out of memory.\n", argv[0]);
                    return 1;
                }
            }
        }
    }

    // Construct graph based on pointers.
    for (AllocationNode *node = nodes, *node_end = nodes + count;
         node != node_end; ++node) {
        const ADLog::Entry *e = node->entry;
        for (const char *d = e->data, *d_end = e->data + e->datasize -
                                         e->datasize % sizeof(ADLog::Pointer);
             d != d_end; d += sizeof(ADLog::Pointer)) {
            AllocationNode *target = (AllocationNode*)
                PL_HashTableLookup(memory_map, *(void**)d);
            if (target) {
                target->pointers_from.AppendElement(node);
                node->pointers_to.AppendElement(target);
            }
        }
    }

    // Do a depth-first search on the graph (i.e., by following
    // |pointers_to|) and assign the post-order index to |index|.
    {
        uint32_t dfs_index = 0;
        nsTArray<AllocationNode*> stack;

        for (AllocationNode *n = nodes, *n_end = nodes+count; n != n_end; ++n) {
            if (n->reached) {
                continue;
            }
            stack.AppendElement(n);

            do {
                uint32_t pos = stack.Length() - 1;
                AllocationNode *n = stack[pos];
                if (n->reached) {
                    n->index = dfs_index++;
                    stack.RemoveElementAt(pos);
                } else {
                    n->reached = true;

                    // When doing post-order processing, we have to be
                    // careful not to put reached nodes into the stack.
                    for (int32_t i = n->pointers_to.Length() - 1; i >= 0; --i) {
                        AllocationNode* e = n->pointers_to[i];
                        if (!e->reached) {
                            stack.AppendElement(e);
                        }
                    }
                }
            } while (stack.Length() > 0);
        }
    }

    // Sort the nodes by their DFS index, in reverse, so that the first
    // node is guaranteed to be in a root SCC.
    AllocationNode **sorted_nodes = new AllocationNode*[count];
    if (!sorted_nodes) {
        fprintf(stderr, "%s: Out of memory.\n", argv[0]);
        return 1;
    }

    {
        for (size_t i = 0; i < count; ++i) {
            sorted_nodes[i] = nodes + i;
        }
        NS_QuickSort(sorted_nodes, count, sizeof(AllocationNode*),
                     sort_by_reverse_index, 0);
    }

    // Put the nodes into their strongly-connected components.
    uint32_t num_sccs = 0;
    {
        for (size_t i = 0; i < count; ++i) {
            nodes[i].reached = false;
        }
        nsTArray<AllocationNode*> stack;
        for (AllocationNode **sn = sorted_nodes,
                        **sn_end = sorted_nodes + count; sn != sn_end; ++sn) {
            if ((*sn)->reached) {
                continue;
            }

            // We found a new strongly connected index.
            stack.AppendElement(*sn);
            do {
                uint32_t pos = stack.Length() - 1;
                AllocationNode *n = stack[pos];
                stack.RemoveElementAt(pos);

                if (!n->reached) {
                    n->reached = true;
                    n->index = num_sccs;
                    stack.AppendElements(n->pointers_from);
                }
            } while (stack.Length() > 0);
            ++num_sccs;
        }
    }

    // Identify which nodes are leak roots by using DFS, and watching
    // for component transitions.
    uint32_t num_root_nodes = count;
    {
        for (size_t i = 0; i < count; ++i) {
            nodes[i].is_root = true;
        }

        nsTArray<AllocationNode*> stack;
        for (AllocationNode *n = nodes, *n_end = nodes+count; n != n_end; ++n) {
            if (!n->is_root) {
                continue;
            }

            // Loop through pointers_to, and add any that are in a
            // different SCC to stack:
            for (int i = n->pointers_to.Length() - 1; i >= 0; --i) {
                AllocationNode *target = n->pointers_to[i];
                if (n->index != target->index) {
                    stack.AppendElement(target);
                }
            }

            while (stack.Length() > 0) {
                uint32_t pos = stack.Length() - 1;
                AllocationNode *n = stack[pos];
                stack.RemoveElementAt(pos);

                if (n->is_root) {
                    n->is_root = false;
                    --num_root_nodes;
                    stack.AppendElements(n->pointers_to);
                }
            }
        }
    }

    // Sort the nodes by their SCC index.
    NS_QuickSort(sorted_nodes, count, sizeof(AllocationNode*),
                 sort_by_index, 0);

    // Print output.
    {
        printf("<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\">\n"
               "<html>\n"
               "<head>\n"
               "<title>Leak analysis</title>\n"
               "<style type=\"text/css\">\n"
               "  .root { background: white; color: black; }\n"
               "  .nonroot { background: #ccc; color: black; }\n"
               "</style>\n"
               "</head>\n");
        printf("<body>\n\n"
               "<p>Generated %zd entries (%d in root SCCs) and %d SCCs.</p>\n\n",
               count, num_root_nodes, num_sccs);

        for (size_t i = 0; i < count; ++i) {
            nodes[i].reached = false;
        }

        // Loop over the sorted nodes twice, first printing the roots
        // and then the non-roots.
        for (int32_t root_type = true;
             root_type == true || root_type == false; --root_type) {
            if (root_type) {
                printf("\n\n"
                       "<div class=\"root\">\n"
                       "<h1 id=\"root\">Root components</h1>\n");
            } else {
                printf("\n\n"
                       "<div class=\"nonroot\">\n"
                       "<h1 id=\"nonroot\">Non-root components</h1>\n");
            }
            uint32_t component = (uint32_t)-1;
            bool one_object_component;
            for (const AllocationNode *const* sn = sorted_nodes,
                                  *const* sn_end = sorted_nodes + count;
                 sn != sn_end; ++sn) {
                const AllocationNode *n = *sn;
                if (n->is_root != root_type)
                    continue;
                const ADLog::Entry *e = n->entry;

                if (n->index != component) {
                    component = n->index;
                    one_object_component =
                        sn + 1 == sn_end || (*(sn+1))->index != component;
                    if (!one_object_component)
                        printf("\n\n<h2 id=\"c%d\">Component %d</h2>\n",
                               component, component);
                }

                if (one_object_component) {
                    printf("\n\n<div id=\"c%d\">\n", component);
                    printf("<h2 id=\"o%td\">Object %td "
                           "(single-object component %d)</h2>\n",
                           n-nodes, n-nodes, component);
                } else {
                    printf("\n\n<h3 id=\"o%td\">Object %td</h3>\n",
                           n-nodes, n-nodes);
                }
                printf("<pre>\n");
                printf("%p &lt;%s&gt; (%zd)\n",
                       e->address, e->type, e->datasize);
                for (size_t d = 0; d < e->datasize;
                     d += sizeof(ADLog::Pointer)) {
                    AllocationNode *target = (AllocationNode*)
                        PL_HashTableLookup(memory_map, *(void**)(e->data + d));
                    if (target) {
                        printf("        <a href=\"#o%td\">",
                               target - nodes);
                        printf(allocation_format,
                               *(size_t*)(e->data + d));
                        printf("</a> &lt;%s&gt;",
                               target->entry->type);
                        if (target->index != n->index) {
                            printf(", component %d", target->index);
                        }
                        printf("\n");
                    } else {
                        printf("        ");
                        printf(allocation_format,
                               *(size_t*)(e->data + d));
                        printf("\n");
                    }
                }

                if (n->pointers_from.Length()) {
                    printf("\nPointers from:\n");
                    for (uint32_t i = 0, i_end = n->pointers_from.Length();
                         i != i_end; ++i) {
                        AllocationNode *t = n->pointers_from[i];
                        const ADLog::Entry *te = t->entry;
                        printf("    <a href=\"#o%td\">%s</a> (Object %td, ",
                               t - nodes, te->type, t - nodes);
                        if (t->index != n->index) {
                            printf("component %d, ", t->index);
                        }
                        if (t == n) {
                            printf("self)\n");
                        } else {
                            printf("%p)\n", te->address);
                        }
                    }
                }

                print_escaped(stdout, e->allocation_stack);

                printf("</pre>\n");
                if (one_object_component) {
                    printf("</div>\n");
                }
            }
            printf("</div>\n");
        }
        printf("</body>\n"
               "</html>\n");
    }

    delete [] sorted_nodes;
    delete [] nodes;

    NS_ShutdownXPCOM(nullptr);

    return 0;
}
