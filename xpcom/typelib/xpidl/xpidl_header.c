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
 * Generate XPCOM headers from XPIDL.
 */

#include "xpidl.h"
#include <ctype.h>

static gboolean write_method_signature(IDL_tree method_tree, FILE *outfile);
static gboolean write_attr_accessor(IDL_tree attr_tree, FILE * outfile,
                                    gboolean getter);

static void
write_header(gpointer key, gpointer value, gpointer user_data)
{
    char *ident = (char *)value;
    TreeState *state = (TreeState *)user_data;
    fprintf(state->file, "#include \"%s.h\"\n",
            ident);
}

static gboolean
pass_1(TreeState *state)
{
    char *define = g_basename(state->basename);
    if (state->tree) {
        fprintf(state->file, "/*\n * DO NOT EDIT.  THIS FILE IS GENERATED FROM"
                " %s.idl\n */\n", state->basename);
        fprintf(state->file, "\n#ifndef __gen_%s_h__\n"
                "#define __gen_%s_h__\n",
                define, define);
        if (g_hash_table_size(state->includes)) {
            fputc('\n', state->file);
            g_hash_table_foreach(state->includes, write_header, state);
        }
    } else {
        fprintf(state->file, "\n#endif /* __gen_%s_h__ */\n", define);
    }
    return TRUE;
}

static void
write_classname_iid_define(FILE *file, const char *className)
{
    const char *iidName;
    if (className[0] == 'n' && className[1] == 's') {
        /* backcompat naming styles */
        fputs("NS_", file);
        iidName = className + 2;
    } else {
        iidName = className;
    }
    while (*iidName)
        fputc(toupper(*iidName++), file);
    fputs("_IID", file);
}

static gboolean
interface(TreeState *state)
{
    IDL_tree iface = state->tree, iter, orig;
    char *className = IDL_IDENT(IDL_INTERFACE(iface).ident).str;
    char *classNameUpper;
    const char *iid;
    const char *name_space;
    struct nsID id;
    char iid_parsed[UUID_LENGTH];

    orig = state->tree;

    fprintf(state->file,   "\n/* starting interface:    %s */\n",
            className);

    name_space = IDL_tree_property_get(IDL_INTERFACE(iface).ident, "namespace");
    iid = IDL_tree_property_get(IDL_INTERFACE(iface).ident, "uuid");

    if (name_space) {
        fprintf(state->file, "/* namespace:             %s */\n",
                name_space);
        fprintf(state->file, "/* fully qualified name:  %s.%s */\n",
                name_space,className);
    }

    if (iid) {
        /* Redundant, but a better error than 'cannot parse.' */
        if (strlen(iid) != 36) {
            IDL_tree_error(state->tree, "IID %s is the wrong length\n", iid);
            return FALSE;
        }

        /*
         * Parse uuid and then output resulting nsID to string, to validate
         * uuid and normalize resulting .h files.
         */
        if (!xpidl_parse_iid(&id, iid)) {
            IDL_tree_error(state->tree, "cannot parse IID %s\n", iid);
            return FALSE;
        }
        if (!xpidl_sprint_iid(&id, iid_parsed)) {
            IDL_tree_error(state->tree, "error formatting IID %s\n", iid);
            return FALSE;
        }

        /* #define NS_ISUPPORTS_IID_STR "00000000-0000-0000-c000-000000000046" */
        fputc('\n', state->file);
        fputs("#define ", state->file);
        write_classname_iid_define(state->file, className);
        fprintf(state->file, "_STR \"%s\"\n", iid_parsed);
        fputc('\n', state->file);

        /* #define NS_ISUPPORTS_IID { {0x00000000 .... 0x46 }} */
        fprintf(state->file, "#define ");
        write_classname_iid_define(state->file, className);
        fprintf(state->file, " \\\n"
                "  {0x%.8x, 0x%.4x, 0x%.4x, \\\n"
                "    { 0x%.2x, 0x%.2x, 0x%.2x, 0x%.2x, "
                "0x%.2x, 0x%.2x, 0x%.2x, 0x%.2x }}\n",
                id.m0, id.m1, id.m2,
                id.m3[0], id.m3[1], id.m3[2], id.m3[3],
                id.m3[4], id.m3[5], id.m3[6], id.m3[7]);
        fputc('\n', state->file);
    }

    /* The interface declaration itself. */
    fprintf(state->file, "class %s", className);
    if ((iter = IDL_INTERFACE(iface).inheritance_spec)) {
        fputs(" : ", state->file);
        do {
            fprintf(state->file, "public %s",
                    IDL_IDENT(IDL_LIST(iter).data).str);
            if (IDL_LIST(iter).next)
                fputs(", ", state->file);
        } while ((iter = IDL_LIST(iter).next));
    }
    fputs(" {\n"
          " public: \n", state->file);
    if (iid) {
        fputs("  NS_DEFINE_STATIC_IID_ACCESSOR(", state->file);
        write_classname_iid_define(state->file, className);
        fputs(")\n", state->file);
    }

    state->tree = IDL_INTERFACE(iface).body;

    if (state->tree && !xpidl_process_node(state))
        return FALSE;

    fputs("};\n", state->file);
    fputc('\n', state->file);

    /*
     * #define NS_DECL_NSIFOO - create method prototypes that can be used in
     * class definitions that support this interface.
     *
     * Walk the tree explicitly to prototype a reworking of xpidl to get rid of
     * the callback mechanism.
     */
    state->tree = orig;
    fputs("/* Use this macro when declaring classes that implement this "
          "interface. */\n", state->file);
    fputs("#define NS_DECL_", state->file);
    classNameUpper = className;
    while (*classNameUpper != '\0')
        fputc(toupper(*classNameUpper++), state->file);
    fputs(" \\\n", state->file);
    if (IDL_INTERFACE(state->tree).body == NULL)
        fputs("/* no methods */", state->file);

    for (iter = IDL_INTERFACE(state->tree).body;
         iter != NULL;
         iter = IDL_LIST(iter).next)
    {
        IDL_tree data = IDL_LIST(iter).data;

        switch(IDL_NODE_TYPE(data)) {
          case IDLN_OP_DCL:
            fputs("  ", state->file); /* XXX abstract to 'indent' */
            write_method_signature(data, state->file);
            break;

          case IDLN_ATTR_DCL:
            fputs("  ", state->file);
            if (!write_attr_accessor(data, state->file, TRUE))
                return FALSE;
            if (!IDL_ATTR_DCL(state->tree).f_readonly) {
                fputs("; \\\n", state->file); /* Terminate the previous one. */
                fputs("  ", state->file);
                if (!write_attr_accessor(data, state->file, FALSE))
                    return FALSE;
                /* '; \n' at end will clean up. */
            }
            break;

          case IDLN_CONST_DCL:
              /* ignore it here; it doesn't contribute to the macro. */
              continue;

          case IDLN_CODEFRAG:
/* IDL_tree_warning crashes in libIDL 0.6.5, sometimes */
#if !(LIBIDL_MAJOR_VERSION == 0 && LIBIDL_MINOR_VERSION == 6 && \
      LIBIDL_MICRO_VERSION == 5)
              IDL_tree_warning(iter, IDL_WARNING1,
                               "%%{ .. %%} code fragment within interface "
                               "ignored when generating NS_DECL_IFOO macro; "
                               "if the code fragment contains method "
                               "declarations, the macro probably isn't "
                               "complete.");
#endif
              break;

          default:
            IDL_tree_error(iter,
                           "unexpected node type %d! "
                           "Please file a bug against the xpidl component.",
                           IDL_NODE_TYPE(data));
            return FALSE;
        }

        if (IDL_LIST(iter).next != NULL) {
            fprintf(state->file, "; \\\n");
        } else {
            fprintf(state->file, "; \n");
        }
    }
    fputc('\n', state->file);

    return TRUE;
}

static gboolean
list(TreeState *state)
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
write_type(IDL_tree type_tree, FILE *outfile)
{
    if (!type_tree) {
        fputs("void", outfile);
        return TRUE;
    }

    switch (IDL_NODE_TYPE(type_tree)) {
      case IDLN_TYPE_INTEGER: {
        gboolean sign = IDL_TYPE_INTEGER(type_tree).f_signed;
        switch (IDL_TYPE_INTEGER(type_tree).f_type) {
          case IDL_INTEGER_TYPE_SHORT:
            fputs(sign ? "PRInt16" : "PRUint16", outfile);
            break;
          case IDL_INTEGER_TYPE_LONG:
            fputs(sign ? "PRInt32" : "PRUint32", outfile);
            break;
          case IDL_INTEGER_TYPE_LONGLONG:
            fputs(sign ? "PRInt64" : "PRUint64", outfile);
            break;
          default:
            g_error("Unknown integer type %d\n",
                    IDL_TYPE_INTEGER(type_tree).f_type);
            return FALSE;
        }
        break;
      }
      case IDLN_TYPE_CHAR:
        fputs("char", outfile);
        break;
      case IDLN_TYPE_WIDE_CHAR:
        fputs("PRUnichar", outfile); /* wchar_t? */
        break;
      case IDLN_TYPE_WIDE_STRING:
        fputs("PRUnichar *", outfile);
        break;
      case IDLN_TYPE_STRING:
        fputs("char *", outfile);
        break;
      case IDLN_TYPE_BOOLEAN:
        fputs("PRBool", outfile);
        break;
      case IDLN_TYPE_OCTET:
        fputs("PRUint8", outfile);
        break;
      case IDLN_TYPE_FLOAT:
        switch (IDL_TYPE_FLOAT(type_tree).f_type) {
          case IDL_FLOAT_TYPE_FLOAT:
            fputs("float", outfile);
            break;
          case IDL_FLOAT_TYPE_DOUBLE:
            fputs("double", outfile);
            break;
          /* XXX 'long double' just ignored, or what? */
          default:
            fprintf(outfile, "unknown_type_%d", IDL_NODE_TYPE(type_tree));
            break;
        }
        break;
      case IDLN_IDENT:
        if (UP_IS_NATIVE(type_tree)) {
            fputs(IDL_NATIVE(IDL_NODE_UP(type_tree)).user_type, outfile);
            if (IDL_tree_property_get(type_tree, "ptr")) {
                fputs(" *", outfile);
            } else if (IDL_tree_property_get(type_tree, "ref")) {
                fputs(" &", outfile);
            }
        } else {
            fputs(IDL_IDENT(type_tree).str, outfile);
        }
        if (UP_IS_AGGREGATE(type_tree))
            fputs(" *", outfile);
        break;
      default:
        fprintf(outfile, "unknown_type_%d", IDL_NODE_TYPE(type_tree));
        break;
    }
    return TRUE;
}

/*
 * An attribute declaration looks like:
 *
 * [ IDL_ATTR_DCL]
 *   - param_type_spec [IDL_TYPE_* or NULL for void]
 *   - simple_declarations [IDL_LIST]
 *     - data [IDL_IDENT]
 *     - next [IDL_LIST or NULL if no more idents]
 *       - data [IDL_IDENT]
 */

#define ATTR_IDENT(tree) (IDL_IDENT(IDL_LIST(IDL_ATTR_DCL(tree).simple_declarations).data))
#define ATTR_TYPE_DECL(tree) (IDL_ATTR_DCL(tree).param_type_spec)
#define ATTR_TYPE(tree) (IDL_NODE_TYPE(ATTR_TYPE_DECL(tree)))

static gboolean
write_attr_accessor(IDL_tree attr_tree, FILE * outfile, gboolean getter)
{
    char *attrname = ATTR_IDENT(attr_tree).str;

    fprintf(outfile, "NS_IMETHOD %cet%c%s(",
            getter ? 'G' : 'S',
            toupper(attrname[0]), attrname + 1);
    if (!write_type(ATTR_TYPE_DECL(attr_tree), outfile))
        return FALSE;
    fprintf(outfile, "%s%sa%c%s)",
            (STARRED_TYPE(attr_tree) ? "" : " "),
            getter ? "*" : "",
            toupper(attrname[0]), attrname + 1);
    return TRUE;
}

static gboolean
attr_dcl(TreeState *state)
{
    xpidl_write_comment(state, 2);

    fputs("  ", state->file);
    if (!write_attr_accessor(state->tree, state->file, TRUE))
        return FALSE;
    fputs(" = 0;\n", state->file);

    if (!IDL_ATTR_DCL(state->tree).f_readonly) {
        fputs("  ", state->file);
        if (!write_attr_accessor(state->tree, state->file, FALSE))
            return FALSE;
        fputs(" = 0;\n", state->file);
    }
    return TRUE;
}

static gboolean
do_enum(TreeState *state)
{
    IDL_tree_warning(state->tree, IDL_WARNING1,
                     "enums not supported, enum \'%s\' ignored",
                     IDL_IDENT(IDL_TYPE_ENUM(state->tree).ident).str);
    return TRUE;
#if 0
    IDL_tree enumb = state->tree, iter;

    fprintf(state->file, "enum %s {\n",
            IDL_IDENT(IDL_TYPE_ENUM(enumb).ident).str);

    for (iter = IDL_TYPE_ENUM(enumb).enumerator_list;
         iter;
         iter = IDL_LIST(iter).next) {
        fprintf(state->file, "  %s%s\n", IDL_IDENT(IDL_LIST(iter).data).str,
                IDL_LIST(iter).next ? ",": "");
    }

    fputs("};\n\n", state->file);
    return TRUE;
#endif
}

static gboolean
do_const_dcl(TreeState *state)
{
    struct _IDL_CONST_DCL *dcl = &IDL_CONST_DCL(state->tree);
    const char *name = IDL_IDENT(dcl->ident).str;
    gboolean success;

    /* const -> list -> interface */
    if (!IDL_NODE_UP(IDL_NODE_UP(state->tree)) ||
        IDL_NODE_TYPE(IDL_NODE_UP(IDL_NODE_UP(state->tree)))
        != IDLN_INTERFACE) {
        IDL_tree_warning(state->tree, IDL_WARNING1,
                         "const decl \'%s\' not inside interface, ignored",
                         name);
        return TRUE;
    }

    success = (IDLN_TYPE_INTEGER == IDL_NODE_TYPE(dcl->const_type));
    if(success) {
        switch(IDL_TYPE_INTEGER(dcl->const_type).f_type) {
        case IDL_INTEGER_TYPE_SHORT:
        case IDL_INTEGER_TYPE_LONG:
            break;
        default:
            success = FALSE;
        }
    }

    if(success) {
        fprintf(state->file, "\n  enum { %s = %d };\n",
                             name, (int) IDL_INTEGER(dcl->const_exp).value);
    } else {
        IDL_tree_warning(state->tree, IDL_WARNING1,
            "const decl \'%s\' was not of type short or long, ignored", name);
    }
    return TRUE;
}

static gboolean
do_typedef(TreeState *state)
{
    IDL_tree type = IDL_TYPE_DCL(state->tree).type_spec,
        dcls = IDL_TYPE_DCL(state->tree).dcls,
        complex;

    if (IDL_NODE_TYPE(type) == IDLN_TYPE_SEQUENCE) {
        IDL_tree_warning(state->tree, IDL_WARNING1,
                         "sequences not supported, ignored");
    } else {
        state->tree = type;
        fputs("typedef ", state->file);
        if (!write_type(state->tree, state->file))
            return FALSE;
        fputs(" ", state->file);
        if (IDL_NODE_TYPE(complex = IDL_LIST(dcls).data) == IDLN_TYPE_ARRAY) {
            fprintf(state->file, "%s[%ld]",
                    IDL_IDENT(IDL_TYPE_ARRAY(complex).ident).str,
                 (long)IDL_INTEGER(IDL_LIST(IDL_TYPE_ARRAY(complex).size_list).
                                data).value);
        } else {
            fputs(" ", state->file);
            fputs(IDL_IDENT(IDL_LIST(dcls).data).str, state->file);
        }
        fputs(";\n", state->file);
    }
    return TRUE;
}

/*
 * param generation:
 * in string foo        -->     nsString *foo
 * out string foo       -->     nsString **foo;
 * inout string foo     -->     nsString **foo;
 */

static gboolean
write_param(IDL_tree param_tree, FILE *outfile)
{
    IDL_tree param_type_spec = IDL_PARAM_DCL(param_tree).param_type_spec;

    /* in string, wstring, nsid, and any explicitly marked [const] are const */
    if (IDL_PARAM_DCL(param_tree).attr == IDL_PARAM_IN &&
        (IDL_NODE_TYPE(param_type_spec) == IDLN_TYPE_STRING ||
         IDL_NODE_TYPE(param_type_spec) == IDLN_TYPE_WIDE_STRING ||
         IDL_tree_property_get(IDL_PARAM_DCL(param_tree).simple_declarator,
                               "const") ||
         IDL_tree_property_get(param_type_spec, "nsid"))) {
        fputs("const ", outfile);
    }
    else if (IDL_PARAM_DCL(param_tree).attr == IDL_PARAM_OUT &&
             IDL_tree_property_get(IDL_PARAM_DCL(param_tree).simple_declarator, 
                                   "shared")) {
        fputs("const ", outfile);
    }

    if (!write_type(param_type_spec, outfile))
        return FALSE;

    /* unless the type ended in a *, add a space */
    if (!STARRED_TYPE(param_type_spec))
        fputc(' ', outfile);

    /* out and inout params get a bonus *! */
    if (IDL_PARAM_DCL(param_tree).attr != IDL_PARAM_IN)
        fputc('*', outfile);

    fputs(IDL_IDENT(IDL_PARAM_DCL(param_tree).simple_declarator).str, outfile);
    return TRUE;
}

/*
 * A forward declaration, usually an interface.
 */
static gboolean
forward_dcl(TreeState *state)
{
    IDL_tree iface = state->tree;
    char *className = IDL_IDENT(IDL_FORWARD_DCL(iface).ident).str;
    if (!className)
        return FALSE;
    fprintf(state->file, "class %s; /* forward decl */\n", className);
    return TRUE;
}

/*
 * Shared between the interface class declaration and the NS_DECL_IFOO macro
 * provided to aid declaration of implementation classes.
 */
static gboolean
write_method_signature(IDL_tree method_tree, FILE *outfile) {
    struct _IDL_OP_DCL *op = &IDL_OP_DCL(method_tree);
    gboolean no_generated_args = TRUE;
    gboolean op_notxpcom =
        (IDL_tree_property_get(op->ident, "notxpcom") != NULL);
    IDL_tree iter;

    if (op_notxpcom) {
        fputs("NS_IMETHOD_(", outfile);
        if (!write_type(op->op_type_spec, outfile))
            return FALSE;
        fputc(')', outfile);
    } else {
        fputs("NS_IMETHOD", outfile);
    }
    fprintf(outfile, " %s(", IDL_IDENT(op->ident).str);
    for (iter = op->parameter_dcls; iter; iter = IDL_LIST(iter).next) {
        if (!write_param(IDL_LIST(iter).data, outfile))
            return FALSE;
        if ((IDL_LIST(iter).next ||
             (!op_notxpcom && op->op_type_spec) || op->f_varargs))
            fputs(", ", outfile);
        no_generated_args = FALSE;
    }

    /* make IDL return value into trailing out argument */
    if (op->op_type_spec && !op_notxpcom) {
        IDL_tree fake_param = IDL_param_dcl_new(IDL_PARAM_OUT,
                                                op->op_type_spec,
                                                IDL_ident_new("_retval"));
        if (!fake_param)
            return FALSE;
        if (!write_param(fake_param, outfile))
            return FALSE;
        if (op->f_varargs)
            fputs(", ", outfile);
        no_generated_args = FALSE;
    }

    /* varargs go last */
    if (op->f_varargs) {
        fputs("nsVarArgs *_varargs", outfile);
        no_generated_args = FALSE;
    }

    /*
     * If generated method has no arguments, output 'void' to avoid C legacy
     * behavior of disabling type checking.
     */
    if (no_generated_args) {
        fputs("void", outfile);
    }

    fputs(")", outfile);

    return TRUE;
}

/*
 * A method is an `operation', therefore a method decl is an `op dcl'.
 * I blame Elliot.
 */
static gboolean
op_dcl(TreeState *state)
{
    /*
     * Verify that e.g. non-scriptable methods in [scriptable] interfaces
     * are declared so.  Do this in a seperate verification pass?
     */
    if (!verify_method_declaration(state->tree))
        return FALSE;

    xpidl_write_comment(state, 2);

    fputs("  ", state->file);
    if (!write_method_signature(state->tree, state->file))
        return FALSE;
    fputs(" = 0;\n", state->file);

    return TRUE;
}

static void
write_codefrag_line(gpointer data, gpointer user_data)
{
    TreeState *state = (TreeState *)user_data;
    char *line = (char *)data;
    fputs(line, state->file);
    fputc('\n', state->file);
}

static gboolean
codefrag(TreeState *state)
{
    char *desc = IDL_CODEFRAG(state->tree).desc;
    if (strcmp(desc, "C++")) {
        IDL_tree_warning(state->tree, IDL_WARNING1,
                         "ignoring '%%{%s' escape. "
                         "(Use '%%{C++' to escape verbatim code.)", desc);

        return TRUE;
    }
    g_slist_foreach(IDL_CODEFRAG(state->tree).lines, write_codefrag_line,
                    (gpointer)state);
    return TRUE;
}

nodeHandler *
xpidl_header_dispatch(void)
{
    static nodeHandler table[IDLN_LAST];

    if (!table[IDLN_NONE]) {
        table[IDLN_NONE] = pass_1;
        table[IDLN_LIST] = list;
        table[IDLN_ATTR_DCL] = attr_dcl;
        table[IDLN_OP_DCL] = op_dcl;
        table[IDLN_FORWARD_DCL] = forward_dcl;
        table[IDLN_TYPE_ENUM] = do_enum;
        table[IDLN_INTERFACE] = interface;
        table[IDLN_CODEFRAG] = codefrag;
        table[IDLN_TYPE_DCL] = do_typedef;
        table[IDLN_CONST_DCL] = do_const_dcl;
    }

    return table;
}
