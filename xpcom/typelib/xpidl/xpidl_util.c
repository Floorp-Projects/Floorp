/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
 * Utility functions called by various backends.
 */ 

#include "xpidl.h"

/* XXXbe static */ char OOM[] = "ERROR: out of memory\n";

void *
xpidl_malloc(size_t nbytes)
{
    void *p = malloc(nbytes);
    if (!p) {
        fputs(OOM, stderr);
        exit(1);
    }
    return p;
}

#ifdef XP_MAC
static char *strdup(const char *c)
{
	char	*newStr = malloc(strlen(c) + 1);
	if (newStr)
	{
		strcpy(newStr, c);
	}
	return newStr;
}
#endif

char *
xpidl_strdup(const char *s)
{
    char *ns = strdup(s);
    if (!ns) {
        fputs(OOM, stderr);
        exit(1);
    }
    return ns;
}

void
xpidl_write_comment(TreeState *state, int indent)
{
    fprintf(state->file, "%*s/* ", indent, "");
    IDL_tree_to_IDL(state->tree, state->ns, state->file,
                    IDLF_OUTPUT_NO_NEWLINES |
                    IDLF_OUTPUT_NO_QUALIFY_IDENTS |
                    IDLF_OUTPUT_PROPERTIES);
    fputs(" */\n", state->file);
}

/*
 * Print an iid to into a supplied buffer; the buffer should be at least
 * UUID_LENGTH bytes.
 */
gboolean
xpidl_sprint_iid(nsID *id, char iidbuf[])
{
    int printed;

    printed = sprintf(iidbuf,
                       "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                       (PRUint32) id->m0, (PRUint32) id->m1,(PRUint32) id->m2,
                       (PRUint32) id->m3[0], (PRUint32) id->m3[1],
                       (PRUint32) id->m3[2], (PRUint32) id->m3[3],
                       (PRUint32) id->m3[4], (PRUint32) id->m3[5],
                       (PRUint32) id->m3[6], (PRUint32) id->m3[7]);

#ifdef SPRINTF_RETURNS_STRING
    return (printed && strlen((char *)printed) == 36);
#else
    return (printed == 36);
#endif
}

/* We only parse the {}-less format. */
static const char nsIDFmt2[] =
  "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x";

/*
 * Parse a uuid string into an nsID struct.  We cannot link against libxpcom,
 * so we re-implement nsID::Parse here.
 */
gboolean
xpidl_parse_iid(nsID *id, const char *str)
{
    PRInt32 count = 0;
    PRInt32 n1, n2, n3[8];
    PRInt32 n0, i;

    assert(str != NULL);
    
    if (strlen(str) != 36) {
        return FALSE;
    }
     
#ifdef DEBUG_shaver_iid
    fprintf(stderr, "parsing iid   %s\n", str);
#endif

    count = sscanf(str, nsIDFmt2,
                   &n0, &n1, &n2,
                   &n3[0],&n3[1],&n3[2],&n3[3],
                   &n3[4],&n3[5],&n3[6],&n3[7]);

    id->m0 = (PRInt32) n0;
    id->m1 = (PRInt16) n1;
    id->m2 = (PRInt16) n2;
    for (i = 0; i < 8; i++) {
      id->m3[i] = (PRInt8) n3[i];
    }

#ifdef DEBUG_shaver_iid
    if (count == 11) {
        fprintf(stderr, "IID parsed to ");
        print_IID(id, stderr);
        fputs("\n", stderr);
    }
#endif
    return (gboolean)(count == 11);
}

gboolean
verify_attribute_declaration(IDL_tree attr_tree)
{
    IDL_tree iface;
    IDL_tree ident;
    IDL_tree attr_type;
    gboolean scriptable_interface;

    /* 
     * Verify that we've been called on an interface, and decide if the
     * interface was marked [scriptable].
     */
    if (IDL_NODE_UP(attr_tree) && IDL_NODE_UP(IDL_NODE_UP(attr_tree)) &&
        IDL_NODE_TYPE(iface = IDL_NODE_UP(IDL_NODE_UP(attr_tree))) 
        == IDLN_INTERFACE)
    {
        scriptable_interface =
            (IDL_tree_property_get(IDL_INTERFACE(iface).ident, "scriptable")
             != NULL);
    } else {
        IDL_tree_error(attr_tree,
                    "verify_attribute_declaration called on a non-interface?");
        return FALSE;
    }

    /*
     * Grab the first of the list of idents and hope that it'll
     * say scriptable or no.
     */
    ident = IDL_LIST(IDL_ATTR_DCL(attr_tree).simple_declarations).data;

    /*
     * If the interface isn't scriptable, or the attribute is marked noscript,
     * there's no need to check.
     */
    if (!scriptable_interface ||
        IDL_tree_property_get(ident, "noscript") != NULL)
        return TRUE;

    /*
     * If it should be scriptable, check that the type is non-native.  nsid
     * is exempted.
     */
    attr_type = IDL_ATTR_DCL(attr_tree).param_type_spec;

    if (attr_type != NULL && UP_IS_NATIVE(attr_type) &&
        IDL_tree_property_get(attr_type, "nsid") == NULL)
    {
        IDL_tree_error(attr_tree,
                       "attributes in [scriptable] interfaces which are "
                       "non-scriptable because they refer to native "
                       "types must be marked [noscript]\n");
        return FALSE;
    }
    return TRUE;
}

/*
 * Find the underlying type of an identifier typedef.
 * 
 * All the needed tree-walking seems pretty shaky; isn't there something in
 * libIDL to automate this?
 * Might want to export this to handle the bug against no typedef'd const.
 */
IDL_tree /* IDL_TYPE_DCL */
find_underlying_type(IDL_tree typedef_ident)
{
    IDL_tree up;

    if (typedef_ident == NULL || IDL_NODE_TYPE(typedef_ident) != IDLN_IDENT)
        return NULL;

    up = IDL_NODE_UP(typedef_ident);
    if (up == NULL || IDL_NODE_TYPE(up) != IDLN_LIST)
        return NULL;
    up = IDL_NODE_UP(up);
    if (up == NULL || IDL_NODE_TYPE(up) != IDLN_TYPE_DCL)
        return NULL;

    return IDL_TYPE_DCL(up).type_spec;
}

static IDL_tree /* IDL_PARAM_DCL */
find_named_parameter(IDL_tree method_tree, const char *param_name)
{
    IDL_tree iter;
    for (iter = IDL_OP_DCL(method_tree).parameter_dcls; iter;
         iter = IDL_LIST(iter).next)
    {
        IDL_tree param = IDL_LIST(iter).data;
        IDL_tree simple_decl = IDL_PARAM_DCL(param).simple_declarator;
        const char *current_name = IDL_IDENT(simple_decl).str;
        if (strcmp(current_name, param_name) == 0)
            return param;
    }
    return NULL;
}

typedef enum ParamAttrType {
    IID_IS,
    LENGTH_IS,
    SIZE_IS
} ParamAttrType;

/*
 * Check that parameters referred to by attributes such as size_is exist and
 * refer to parameters of the appropriate type.
 */
static gboolean
check_param_attribute(IDL_tree method_tree, IDL_tree param,
                      ParamAttrType whattocheck)
{
    const char *method_name = IDL_IDENT(IDL_OP_DCL(method_tree).ident).str;
    const char *referred_name = NULL;
    IDL_tree param_type = IDL_PARAM_DCL(param).param_type_spec;
    IDL_tree simple_decl = IDL_PARAM_DCL(param).simple_declarator;
    const char *param_name = IDL_IDENT(simple_decl).str;
    const char *attr_name;
    const char *needed_type;

    if (whattocheck == IID_IS) {
        attr_name = "iid_is";
        needed_type = "IID";
    } else if (whattocheck == LENGTH_IS) {
        attr_name = "length_is";
        needed_type = "unsigned long (or PRUint32)";
    } else if (whattocheck == SIZE_IS) {
        attr_name = "size_is";
        needed_type = "unsigned long (or PRUint32)";
    } else {
        assert("asked to check an unknown attribute type!");
        return TRUE;
    }
    
    referred_name = IDL_tree_property_get(simple_decl, attr_name);
    if (referred_name != NULL) {
        IDL_tree referred_param = find_named_parameter(method_tree,
                                                       referred_name);
        IDL_tree referred_param_type;
        if (referred_param == NULL) {
            IDL_tree_error(method_tree,
                           "attribute [%s(%s)] refers to missing "
                           "parameter \"%s\"",
                           attr_name, referred_name, referred_name);
            return FALSE;
        }
        if (referred_param == param) {
            IDL_tree_error(method_tree,
                           "attribute [%s(%s)] refers to it's own parameter",
                           attr_name, referred_name);
            return FALSE;
        }
        
        referred_param_type = IDL_PARAM_DCL(referred_param).param_type_spec;
        if (whattocheck == IID_IS) {
            /* require IID type */
            if (IDL_tree_property_get(referred_param_type, "nsid") == NULL) {
                IDL_tree_error(method_tree,
                               "target \"%s\" of [%s(%s)] attribute "
                               "must be of %s type",
                               referred_name, attr_name, referred_name,
                               needed_type);
                return FALSE;
            }
        } else if (whattocheck == LENGTH_IS || whattocheck == SIZE_IS) {
            /* require PRUint32 type */
            IDL_tree real_type;

            /* Could be a typedef; try to map it to the real type. */
            real_type = find_underlying_type(referred_param_type);
            real_type = real_type ? real_type : referred_param_type;

            if (IDL_NODE_TYPE(real_type) != IDLN_TYPE_INTEGER ||
                IDL_TYPE_INTEGER(real_type).f_signed != FALSE ||
                IDL_TYPE_INTEGER(real_type).f_type != IDL_INTEGER_TYPE_LONG)
            {
                IDL_tree_error(method_tree,
                               "target \"%s\" of [%s(%s)] attribute "
                               "must be of %s type",
                               referred_name, attr_name, referred_name,
                               needed_type);

                return FALSE;
            }
        }
    }

    return TRUE;
}


/*
 * Common method verification code, called by *op_dcl in the various backends.
 */
gboolean
verify_method_declaration(IDL_tree method_tree)
{
    struct _IDL_OP_DCL *op = &IDL_OP_DCL(method_tree);
    IDL_tree iface;
    IDL_tree iter;
    gboolean scriptable_interface;
    gboolean scriptable_method;
    gboolean seen_retval = FALSE;
    const char *method_name = IDL_IDENT(IDL_OP_DCL(method_tree).ident).str;

    if (op->f_varargs) {
        /* We don't currently support varargs. */
        IDL_tree_error(method_tree, "varargs are not currently supported");
        return FALSE;
    }

    /* 
     * Verify that we've been called on an interface, and decide if the
     * interface was marked [scriptable].
     */
    if (IDL_NODE_UP(method_tree) && IDL_NODE_UP(IDL_NODE_UP(method_tree)) &&
        IDL_NODE_TYPE(iface = IDL_NODE_UP(IDL_NODE_UP(method_tree))) 
        == IDLN_INTERFACE)
    {
        scriptable_interface =
            (IDL_tree_property_get(IDL_INTERFACE(iface).ident, "scriptable")
             != NULL);
    } else {
        IDL_tree_error(method_tree,
                       "verify_method_declaration called on a non-interface?");
        return FALSE;
    }

    /*
     * Require that any method in an interface marked as [scriptable], that
     * *isn't* scriptable because it refers to some native type, be marked
     * [noscript] or [notxpcom].
     *
     * Also check that iid_is points to nsid, and length_is, size_is points
     * to unsigned long.
     */
    scriptable_method = scriptable_interface &&
        IDL_tree_property_get(op->ident, "notxpcom") == NULL &&
        IDL_tree_property_get(op->ident, "noscript") == NULL;

    /* Loop through the parameters and check. */
    for (iter = op->parameter_dcls; iter; iter = IDL_LIST(iter).next) {
        IDL_tree param = IDL_LIST(iter).data;
        IDL_tree param_type =
            IDL_PARAM_DCL(param).param_type_spec;
        IDL_tree simple_decl =
            IDL_PARAM_DCL(param).simple_declarator;
        const char *param_name = IDL_IDENT(simple_decl).str;
        
        /*
         * Reject this method if it should be scriptable and some parameter is
         * native that isn't marked with either nsid or iid_is.
         */
        if (scriptable_method &&
            UP_IS_NATIVE(param_type) &&
            IDL_tree_property_get(param_type, "nsid") == NULL &&
            IDL_tree_property_get(simple_decl, "iid_is") == NULL)
        {
            IDL_tree_error(method_tree,
                           "methods in [scriptable] interfaces which are "
                           "non-scriptable because they refer to native "
                           "types (parameter \"%s\") must be marked "
                           "[noscript]", param_name);
            return FALSE;
        }

        /*
         * Sanity checks on return values.
         */
        if (IDL_tree_property_get(simple_decl, "retval") != NULL) {
            if (IDL_LIST(iter).next != NULL) {
                IDL_tree_error(method_tree,
                               "only the last parameter can be marked [retval]");
                return FALSE;
            }
            if (op->op_type_spec) {
                IDL_tree_error(method_tree,
                               "can't have [retval] with non-void return type");
                return FALSE;
            }
            /* In case XPConnect relaxes the retval-is-last restriction. */
            if (seen_retval) {
                IDL_tree_error(method_tree,
                               "can't have more than one [retval] parameter");
                return FALSE;
            }
            seen_retval = TRUE;
        }

        /*
         * Confirm that [shared] attributes are only used with string, wstring,
         * or native (but not nsid) and can't be used with [array].
         */
        if (IDL_tree_property_get(simple_decl, "shared") != NULL) {
            IDL_tree real_type;
            real_type = find_underlying_type(param_type);
            real_type = real_type ? real_type : param_type;

            if (IDL_tree_property_get(simple_decl, "array") != NULL) {
                IDL_tree_error(method_tree,
                               "[shared] parameter \"%s\" cannot "
                               "be of array type", param_name);
                return FALSE;
            }                

            if (!(IDL_NODE_TYPE(real_type) == IDLN_TYPE_STRING ||
                  IDL_NODE_TYPE(real_type) == IDLN_TYPE_WIDE_STRING ||
                  (UP_IS_NATIVE(real_type) &&
                   !IDL_tree_property_get(real_type, "nsid"))))
            {
                IDL_tree_error(method_tree,
                               "[shared] parameter \"%s\" must be of type "
                               "string, wstring or native", param_name);
                return FALSE;
            }
        }

        if (!check_param_attribute(method_tree, param, IID_IS) ||
            !check_param_attribute(method_tree, param, LENGTH_IS) ||
            !check_param_attribute(method_tree, param, SIZE_IS))
            return FALSE;
    }
    
    /* XXX q: can return type be nsid? */
    /* Native return type? */
    if (scriptable_method &&
        op->op_type_spec != NULL && UP_IS_NATIVE(op->op_type_spec) &&
        IDL_tree_property_get(op->op_type_spec, "nsid") == NULL)
    {
        IDL_tree_error(method_tree,
                       "methods in [scriptable] interfaces which are "
                       "non-scriptable because they return native "
                       "types must be marked [noscript]");
        return FALSE;
    }

    return TRUE;
}

/*
 * Verify that a native declaration has an associated C++ expression, i.e. that
 * it's of the form native <idl-name>(<c++-name>)
 */
gboolean
check_native(TreeState *state)
{
    char *native_name;
    /* require that native declarations give a native type */
    if (IDL_NATIVE(state->tree).user_type) 
        return TRUE;
    native_name = IDL_IDENT(IDL_NATIVE(state->tree).ident).str;
    IDL_tree_error(state->tree,
                   "``native %s;'' needs C++ type: ``native %s(<C++ type>);''",
                   native_name, native_name);
    return FALSE;
}

/*
 * Print a GSList as char strings to a file.
 */
void
printlist(FILE *outfile, GSList *slist)
{
    guint i;
    guint len = g_slist_length(slist);

    for(i = 0; i < len; i++) {
        fprintf(outfile, 
                "%s\n", (char *)g_slist_nth_data(slist, i));
    }
}

void
xpidl_list_foreach(IDL_tree p, IDL_tree_func foreach, gpointer user_data)
{
    IDL_tree_func_data tfd;

    while (p) {
        struct _IDL_LIST *list = &IDL_LIST(p);
        tfd.tree = list->data;
        if (!foreach(&tfd, user_data))
            return;
        p = list->next;
    }
}
