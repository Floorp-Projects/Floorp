/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * Generate typelib files for use with InterfaceInfo.
 * http://www.mozilla.org/scriptable/typelib_file.html
 */

#include "xpidl.h"
#include <xpt_xdr.h>
#include <xpt_struct.h>

struct priv_data {
    XPTHeader *header;
    uint16 ifaces;
    GHashTable *interface_map;
    XPTInterfaceDescriptor *current;
    uint16 next_method;
};

#define HEADER(state)    (((struct priv_data *)state->priv)->header)
#define IFACES(state)    (((struct priv_data *)state->priv)->ifaces)
#define IFACE_MAP(state) (((struct priv_data *)state->priv)->interface_map)
#define CURRENT(state)   (((struct priv_data *)state->priv)->current)
#define NEXT_METH(state) (((struct priv_data *)state->priv)->next_method)

/*
 * If p is an ident for an interface, and we don't have an entry in the
 * interface map yet, add one.
 */
static gboolean
add_interface_maybe(IDL_tree p, IDL_tree scope, gpointer user_data)
{
    TreeState *state = user_data;
    IDL_tree up;
    if (IDL_NODE_TYPE(p) == IDLN_IDENT) {
        if ((IDL_NODE_TYPE((up = IDL_NODE_UP(p))) == IDLN_INTERFACE)) {
            char *iface = IDL_IDENT(p).str;
            if (!g_hash_table_lookup(IFACE_MAP(state), iface)) {
                /* XXX should we parse here and store a struct nsID *? */
                char *iid = (char *)IDL_tree_property_get(up, "uuid");
                if (iid)
                    iid = strdup(iid);
                g_hash_table_insert(IFACE_MAP(state), iface, iid);
                IFACES(state)++;
#ifdef DEBUG_shaver
                fprintf(stderr, "adding interface #%d: %s/%s\n", IFACES(state),
                        iface, iid ? iid : "<unresolved>");
#endif
            }
        } else {
#ifdef DEBUG_shaver
            fprintf(stderr, "ident %s isn't an interface (%s)\n",
                    IDL_IDENT(p).str, IDL_NODE_TYPE_NAME(up));
#endif
        }
    }

    return PR_TRUE;
}

/* Find all the interfaces referenced in the tree (uses add_interface_maybe) */
static gboolean
find_interfaces(IDL_tree p, IDL_tree scope, gpointer user_data)
{
    IDL_tree node = NULL;

    switch (IDL_NODE_TYPE(p)) {
      case IDLN_ATTR_DCL:
        node = IDL_ATTR_DCL(p).param_type_spec;
        break;
      case IDLN_OP_DCL:
        IDL_tree_walk_in_order(IDL_OP_DCL(p).parameter_dcls, find_interfaces,
                               user_data);
        node = IDL_OP_DCL(p).op_type_spec;
        break;
      case IDLN_PARAM_DCL:
        node = IDL_PARAM_DCL(p).param_type_spec;
        break;
      case IDLN_INTERFACE:
        node = IDL_INTERFACE(p).inheritance_spec;
        if (node)
            xpidl_list_foreach(node, add_interface_maybe, user_data);
        node = IDL_INTERFACE(p).ident;
        break;
      default:
        node = NULL;
    }

    if (node && IDL_NODE_TYPE(node) == IDLN_IDENT)
        add_interface_maybe(node, scope, user_data);

    return TRUE;
}

/* parse str and fill id */
static gboolean
fill_iid(struct nsID *id, char *str)
{
    /* XXX parse! */
    memset(id, 0, sizeof(struct nsID));
    return TRUE;
}

/* fill the interface_directory IDE table from the interface_map */
static gboolean
fill_ide_table(gpointer key, gpointer value, gpointer user_data)
{
    TreeState *state = user_data;
    char *interface = key, *iid = value;
    struct nsID id = {
        0x00112233,
        0x4455,
        0x6677,
        {0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}
    };
    XPTInterfaceDirectoryEntry *ide;

#if 0    
    if (!fill_iid(&id, iid)) {
        /* XXX report error */
        return FALSE;
    }
#endif

    ide = &(HEADER(state)->interface_directory[IFACES(state)]);
    if (!XPT_FillInterfaceDirectoryEntry(ide, &id, interface, NULL, NULL)) {
        /* XXX report error */
        return FALSE;;
    }

    IFACES(state)++;
    if (iid)
        free(iid);
    return TRUE;
}

/* sort the IDE block as per the typelib spec: IID order, unresolved first */
static void
sort_ide_block(TreeState *state)
{
    XPTInterfaceDirectoryEntry *ide; 
    int i;

    /* XXX we should sort, but for now just enumerate */
    for (i = 0; i < IFACES(state); i++) {
        ide = HEADER(state)->interface_directory + i;
        g_hash_table_insert(IFACE_MAP(state), ide->name, (void *)(i + 1));
    }
    
    return;
}

static gboolean
typelib_list(TreeState *state)
{
    IDL_tree iter;
    for (iter = state->tree; iter; iter = IDL_LIST(iter).next) {
        state->tree = IDL_LIST(iter).data;
        if (!xpidl_process_node(state))
            return FALSE;
    }
    return TRUE;
}

static gboolean
pass_1(TreeState *state)
{
    gboolean ok = FALSE;
    if (state->tree) {
        state->priv = calloc(1, sizeof(struct priv_data));
        if (!state->priv)
            return PR_FALSE;
        IFACES(state) = 0;
        IFACE_MAP(state) = g_hash_table_new(g_str_hash, g_str_equal);
        if (!IFACE_MAP(state)) {
            /* XXX report error */
            free(state->priv);
            return FALSE;
        }

        /* find all interfaces, top-level and referenced by others */
#ifdef DEBUG_shaver
        fprintf(stderr, "finding interfaces\n");
#endif
        IDL_tree_walk_in_order(state->tree, find_interfaces, state);
#ifdef DEBUG_shaver
        fprintf(stderr, "found %d interfaces\n", IFACES(state));
#endif
        HEADER(state) = XPT_NewHeader(IFACES(state));
        HEADER(state)->annotations =
            XPT_NewAnnotation(XPT_ANN_LAST | XPT_ANN_PRIVATE,
                              XPT_NewStringZ("xpidl 0.99.1"),
                              XPT_NewStringZ("I should put something here."));

        /* fill IDEs from hash table */
        IFACES(state) = 0;
#ifdef DEBUG_shaver
        fprintf(stderr, "filling IDE table\n");
#endif
        g_hash_table_foreach_remove(IFACE_MAP(state), fill_ide_table, state);

        /* sort the IDEs by IID order and store indices in the interface map */
        sort_ide_block(state);
        
        ok = TRUE;
    } else {
        /* write the typelib */
        char *data;
        uint32 len, header_sz;
        XPTState *xstate = XPT_NewXDRState(XPT_ENCODE, NULL, 0);
        XPTCursor curs, *cursor = &curs;
#ifdef DEBUG_shaver
        fprintf(stderr, "writing the typelib\n");
#endif

        header_sz = XPT_SizeOfHeaderBlock(HEADER(state));

        if (!xstate ||
            !XPT_MakeCursor(xstate, XPT_HEADER, header_sz, cursor))
            goto destroy_header;

        if (!XPT_DoHeader(cursor, &HEADER(state)))
            goto destroy;
        
        XPT_GetXDRData(xstate, XPT_HEADER, &data, &len);
        fwrite(data, len, 1, state->file);
        XPT_GetXDRData(xstate, XPT_DATA, &data, &len);
        fwrite(data, len, 1, state->file);

        ok = TRUE;

    destroy:
        XPT_DestroyXDRState(xstate);
    destroy_header:
        /* XXX XPT_DestroyHeader(HEADER(state)) */

#ifdef DEBUG_shaver
        fprintf(stderr, "writing typelib was %ssuccessful\n",
                ok ? "" : "not ");
#endif
    }

    return ok;

}

static XPTInterfaceDirectoryEntry *
FindInterfaceByName(XPTInterfaceDirectoryEntry *ides, uint16 num_interfaces,
                    const char *name)
{
    int i;
    for (i = 0; i < num_interfaces; i++) {
        if (!strcmp(ides[i].name, name))
            return &ides[i];
    }
    return NULL;
}

static gboolean
typelib_interface(TreeState *state)
{
    IDL_tree iface = state->tree, iter;
    char *name = IDL_IDENT(IDL_INTERFACE(iface).ident).str;
    XPTInterfaceDirectoryEntry *ide;
    XPTInterfaceDescriptor *id;
    uint16 parent_id = 0;

    ide = FindInterfaceByName(HEADER(state)->interface_directory,
                              HEADER(state)->num_interfaces, name);
    if (!ide) {
        fprintf(stderr,
                "ERROR: didn't find interface %s in IDE block. Giving up.\n",
                name);
        return FALSE;
    }

    if ((iter = IDL_INTERFACE(iface).inheritance_spec)) {
        char *parent;
        if (IDL_LIST(iter).next) {
            fprintf(stderr, "ERROR: more than one parent interface for %s\n",
                    name);
            return FALSE;
        }
        parent = IDL_IDENT(IDL_LIST(iter).data).str;
        parent_id = (uint16)(uint32)g_hash_table_lookup(IFACE_MAP(state),
                                                        parent);
        if (!parent_id) {
            fprintf(stderr, "ERROR: no index found for %s. Giving up.\n",
                    parent);
            return FALSE;
        }
    }

    id = XPT_NewInterfaceDescriptor(parent_id, 0, 0);
    if (!id)
        return FALSE;
    
    CURRENT(state) = ide->interface_descriptor = id;
    NEXT_METH(state) = 0;

    state->tree = IDL_INTERFACE(iface).body;
    if (state->tree && !xpidl_process_node(state))
        return FALSE;
    return TRUE;
}

static gboolean
fill_pd_from_type(TreeState *state, XPTParamDescriptor *pd, uint8 flags,
                  IDL_tree type)
{
    IDL_tree up;

    pd->flags = flags;
    if (type) {
        switch (IDL_NODE_TYPE(type)) {
          case IDLN_TYPE_INTEGER: {
              gboolean sign = IDL_TYPE_INTEGER(type).f_type;
              switch(IDL_TYPE_INTEGER(type).f_type) {
                case IDL_INTEGER_TYPE_SHORT:
                  pd->type.prefix.flags = sign ? TD_INT16 : TD_UINT16;
                  break;
                case IDL_INTEGER_TYPE_LONG:
                  pd->type.prefix.flags = sign ? TD_INT32 : TD_UINT32;
                  break;
                case IDL_INTEGER_TYPE_LONGLONG:
                  pd->type.prefix.flags = sign ? TD_INT64 : TD_UINT64;
                  break;
              }
              break;
          }
          case IDLN_TYPE_CHAR:
            pd->type.prefix.flags = TD_CHAR;
            break;
          case IDLN_TYPE_WIDE_CHAR:
            pd->type.prefix.flags = TD_WCHAR;
            break;
          case IDLN_TYPE_STRING:
            pd->type.prefix.flags = TD_PSTRING; /* XXXshaver string-type? */
            break;
          case IDLN_TYPE_WIDE_STRING:
            pd->type.prefix.flags = TD_PWSTRING;
            break;
          case IDLN_TYPE_BOOLEAN:
            pd->type.prefix.flags = TD_BOOL;
            break;
          case IDLN_NATIVE:
            pd->type.prefix.flags = TD_VOID | XPT_TDP_POINTER;
            break;
          case IDLN_IDENT:
            if (!(up = IDL_NODE_UP(type))) {
                fprintf(stderr, "FATAL: orphan ident %s in param list\n",
                        IDL_IDENT(type).str);
                return FALSE;
            }
            switch (IDL_NODE_TYPE(up)) {
              case IDLN_INTERFACE: {
                  XPTInterfaceDirectoryEntry *ide,
                      *ides = HEADER(state)->interface_directory;
                  uint16 num_ifaces = HEADER(state)->num_interfaces;
                  char *className = IDL_IDENT(IDL_INTERFACE(up).ident).str;
                  
                  pd->type.prefix.flags = TD_INTERFACE_TYPE;
                  ide = FindInterfaceByName(ides, num_ifaces, className);
                  if (!ide || ide < ides || ide > ides + num_ifaces) {
                      fprintf(stderr, "FATAL: unknown iface %s in params\n",
                              className);
                      return FALSE;
                  }
                  pd->type.type.interface = ide - ides + 1;
#ifdef DEBUG_shaver
                  fprintf(stderr, "DBG: index %d for %s\n",
                          pd->type.type.interface, className);
#endif
                  break;
              }
              default:
                fprintf(stderr, "Can't yet handle %s ident in param list\n",
                        IDL_NODE_TYPE_NAME(up));
                return FALSE;
            }
            break;
          default:
            fprintf(stderr, "Can't yet handle %s in param list\n",
                    IDL_NODE_TYPE_NAME(type));
            return FALSE;
        }
    } else {
        pd->type.prefix.flags = TD_VOID;
    }

    return TRUE;
}

static gboolean
fill_pd_from_param(TreeState *state, XPTParamDescriptor *pd, IDL_tree tree)
{
    uint8 flags = 0;

    switch (IDL_PARAM_DCL(tree).attr) {
      case IDL_PARAM_IN:
        flags = XPT_PD_IN;
        break;
      case IDL_PARAM_OUT:
        flags = XPT_PD_OUT;
        break;
      case IDL_PARAM_INOUT:
        flags = XPT_PD_IN | XPT_PD_OUT;
        break;
    }
    /* XXXshaver retval */

    return fill_pd_from_type(state, pd, flags,
                             IDL_PARAM_DCL(tree).param_type_spec);
}

static gboolean
typelib_op_dcl(TreeState *state)
{
    XPTInterfaceDescriptor *id = CURRENT(state);
    XPTMethodDescriptor *meth;
    struct _IDL_OP_DCL *op = &IDL_OP_DCL(state->tree);
    IDL_tree iter;
    uint16 num_args = 0;
    uint8 op_flags = 0;

    if (!XPT_InterfaceDescriptorAddMethods(id, 1))
        return PR_FALSE;

    meth = &id->method_descriptors[NEXT_METH(state)];

    for (iter = op->parameter_dcls; iter; iter = IDL_LIST(iter).next)
        num_args++;                       /* count params */
    if (op->f_noscript)
        op_flags |= XPT_MD_HIDDEN;
    if (op->f_varargs)
        op_flags |= XPT_MD_VARARGS;
    /* XXXshaver constructor? */

#ifdef DEBUG_shaver
    fprintf(stdout, "DBG: adding method %s (nargs %d)\n",
            IDL_IDENT(op->ident).str, num_args);
#endif
    if (!XPT_FillMethodDescriptor(meth, op_flags, IDL_IDENT(op->ident).str,
                                  num_args))
        return PR_FALSE;

    for (num_args = 0, iter = op->parameter_dcls; iter;
         iter = IDL_LIST(iter).next, num_args++) {
        if (!fill_pd_from_param(state, &meth->params[num_args],
                                IDL_LIST(iter).data))
            return PR_FALSE;
    }

    if (!fill_pd_from_type(state, meth->result, XPT_PD_RETVAL,
                           op->op_type_spec))
        return PR_FALSE;

    NEXT_METH(state)++;
    return PR_TRUE;
}

nodeHandler *
xpidl_typelib_dispatch(void)
{
  static nodeHandler table[IDLN_LAST];
  static gboolean initialized = FALSE;

  if (!initialized) {
      /* Initialize non-NULL elements */
      table[IDLN_NONE] = pass_1;
      table[IDLN_LIST] = typelib_list;
      table[IDLN_INTERFACE] = typelib_interface;
      table[IDLN_OP_DCL] = typelib_op_dcl;
      initialized = TRUE;
  }
  
  return table;  
}
