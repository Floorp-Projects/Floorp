/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Generate XPCOM headers from XPIDL.
 */

#include "xpidl.h"
#include <ctype.h>

#define AS_DECL 0
#define AS_CALL 1
#define AS_IMPL 2

static gboolean write_method_signature(IDL_tree method_tree, FILE *outfile,
                                       int mode, const char *className);
static gboolean write_attr_accessor(IDL_tree attr_tree, FILE * outfile,
                                    gboolean getter, 
                                    int mode, const char *className);

static void
write_indent(FILE *outfile) {
    fputs("  ", outfile);
}

static gboolean
header_prolog(TreeState *state)
{
    const char *define = g_basename(state->basename);
    fprintf(state->file, "/*\n * DO NOT EDIT.  THIS FILE IS GENERATED FROM"
            " %s.idl\n */\n", state->basename);
    fprintf(state->file,
            "\n#ifndef __gen_%s_h__\n"
            "#define __gen_%s_h__\n",
            define, define);
    if (state->base_includes != NULL) {
        guint len = g_slist_length(state->base_includes);
        guint i;

        fputc('\n', state->file);
        for (i = 0; i < len; i++) {
            char *ident, *dot;
            
            ident = (char *)g_slist_nth_data(state->base_includes, i);
            
            /* suppress any trailing .extension */
            
            /* XXX use g_basename instead ? ? */
            
            dot = strrchr(ident, '.');
            if (dot != NULL)
                *dot = '\0';
            

            /* begin include guard */            
            fprintf(state->file,
                    "\n#ifndef __gen_%s_h__\n",
                     ident);

            fprintf(state->file, "#include \"%s.h\"\n",
                    (char *)g_slist_nth_data(state->base_includes, i));

            fprintf(state->file, "#endif\n");
            
        }
        if (i > 0)
            fputc('\n', state->file);
    }
    /*
     * Support IDL files that don't include a root IDL file that defines
     * NS_NO_VTABLE.
     */
    fprintf(state->file,
            "/* For IDL files that don't want to include root IDL files. */\n"
            "#ifndef NS_NO_VTABLE\n"
            "#define NS_NO_VTABLE\n"
            "#endif\n");
    
    return TRUE;
}

static gboolean
header_epilog(TreeState *state)
{
    const char *define = g_basename(state->basename);
    fprintf(state->file, "\n#endif /* __gen_%s_h__ */\n", define);
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
    char *classNameUpper = NULL;
    char *classNameImpl = NULL;
    char *cp;
    gboolean ok = TRUE;
    gboolean keepvtable;
    const char *iid;
    const char *name_space;
    struct nsID id;
    char iid_parsed[UUID_LENGTH];
    GSList *doc_comments = IDL_IDENT(IDL_INTERFACE(iface).ident).comments;

    if (!verify_interface_declaration(iface))
        return FALSE;

#define FAIL    do {ok = FALSE; goto out;} while(0)

    fprintf(state->file,   "\n/* starting interface:    %s */\n",
            className);

    name_space = IDL_tree_property_get(IDL_INTERFACE(iface).ident, "namespace");
    if (name_space) {
        fprintf(state->file, "/* namespace:             %s */\n",
                name_space);
        fprintf(state->file, "/* fully qualified name:  %s.%s */\n",
                name_space,className);
    }

    iid = IDL_tree_property_get(IDL_INTERFACE(iface).ident, "uuid");
    if (iid) {
        /* Redundant, but a better error than 'cannot parse.' */
        if (strlen(iid) != 36) {
            IDL_tree_error(state->tree, "IID %s is the wrong length\n", iid);
            FAIL;
        }

        /*
         * Parse uuid and then output resulting nsID to string, to validate
         * uuid and normalize resulting .h files.
         */
        if (!xpidl_parse_iid(&id, iid)) {
            IDL_tree_error(state->tree, "cannot parse IID %s\n", iid);
            FAIL;
        }
        if (!xpidl_sprint_iid(&id, iid_parsed)) {
            IDL_tree_error(state->tree, "error formatting IID %s\n", iid);
            FAIL;
        }

        /* #define NS_ISUPPORTS_IID_STR "00000000-0000-0000-c000-000000000046" */
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
    } else {
        IDL_tree_error(state->tree, "interface %s lacks a uuid attribute\n", 
            className);
        FAIL;
    }

    if (doc_comments != NULL)
        printlist(state->file, doc_comments);

    /*
     * NS_NO_VTABLE is defined in nsISupportsUtils.h, and defined on windows
     * to __declspec(novtable) on windows.  This optimization is safe
     * whenever the constructor calls no virtual methods.  Writing in IDL
     * almost guarantees this, except for the case when a %{C++ block occurs in
     * the interface.  We detect that case, and emit a macro call that disables
     * the optimization.
     */
    keepvtable = FALSE;
    for (iter = IDL_INTERFACE(state->tree).body;
         iter != NULL;
         iter = IDL_LIST(iter).next)
    {
        IDL_tree data = IDL_LIST(iter).data;
        if (IDL_NODE_TYPE(data) == IDLN_CODEFRAG)
            keepvtable = TRUE;
    }
    
    /* The interface declaration itself. */
    fprintf(state->file,
            "class %s%s",
            (keepvtable ? "" : "NS_NO_VTABLE "), className);
    
    if ((iter = IDL_INTERFACE(iface).inheritance_spec)) {
        fputs(" : ", state->file);
        if (IDL_LIST(iter).next != NULL) {
            IDL_tree_error(iter,
                           "multiple inheritance is not supported by xpidl");
            FAIL;
        }
        fprintf(state->file, "public %s", IDL_IDENT(IDL_LIST(iter).data).str);
    }
    fputs(" {\n"
          " public: \n\n", state->file);
    if (iid) {
        fputs("  NS_DEFINE_STATIC_IID_ACCESSOR(", state->file);
        write_classname_iid_define(state->file, className);
        fputs(")\n\n", state->file);
    }

    orig = state->tree; /* It would be nice to remove this state-twiddling. */

    state->tree = IDL_INTERFACE(iface).body;

    if (state->tree && !xpidl_process_node(state))
        FAIL;

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
    classNameUpper = xpidl_strdup(className);
    for (cp = classNameUpper; *cp != '\0'; cp++)
        *cp = toupper(*cp);
    fprintf(state->file, "%s \\\n", classNameUpper);
    if (IDL_INTERFACE(state->tree).body == NULL) {
        write_indent(state->file);
        fputs("/* no methods! */\n", state->file);
    }

    for (iter = IDL_INTERFACE(state->tree).body;
         iter != NULL;
         iter = IDL_LIST(iter).next)
    {
        IDL_tree data = IDL_LIST(iter).data;

        switch(IDL_NODE_TYPE(data)) {
          case IDLN_OP_DCL:
            write_indent(state->file);
            write_method_signature(data, state->file, AS_DECL, NULL);
            break;

          case IDLN_ATTR_DCL:
            write_indent(state->file);
            if (!write_attr_accessor(data, state->file, TRUE, AS_DECL, NULL))
                FAIL;
            if (!IDL_ATTR_DCL(data).f_readonly) {
                fputs("; \\\n", state->file); /* Terminate the previous one. */
                write_indent(state->file);
                if (!write_attr_accessor(data, state->file,
                                         FALSE, AS_DECL, NULL))
                    FAIL;
                /* '; \n' at end will clean up. */
            }
            break;

          case IDLN_CONST_DCL:
            /* ignore it here; it doesn't contribute to the macro. */
            continue;

          case IDLN_CODEFRAG:
            XPIDL_WARNING((iter, IDL_WARNING1,
                           "%%{ .. %%} code fragment within interface "
                           "ignored when generating NS_DECL_%s macro; "
                           "if the code fragment contains method "
                           "declarations, the macro probably isn't "
                           "complete.", classNameUpper));
            continue;

          default:
            IDL_tree_error(iter,
                           "unexpected node type %d! "
                           "Please file a bug against the xpidl component.",
                           IDL_NODE_TYPE(data));
            FAIL;
        }

        if (IDL_LIST(iter).next != NULL) {
            fprintf(state->file, "; \\\n");
        } else {
            fprintf(state->file, "; \n");
        }
    }
    fputc('\n', state->file);

    /* XXX abstract above and below into one function? */
    /*
     * #define NS_FORWARD_NSIFOO - create forwarding methods that can delegate
     * behavior from in implementation to another object.  As generated by
     * idlc.
     */
    fprintf(state->file,
            "/* Use this macro to declare functions that forward the "
            "behavior of this interface to another object. */\n"
            "#define NS_FORWARD_%s(_to) \\\n",
            classNameUpper);
    if (IDL_INTERFACE(state->tree).body == NULL) {
        write_indent(state->file);
        fputs("/* no methods! */\n", state->file);
    }

    for (iter = IDL_INTERFACE(state->tree).body;
         iter != NULL;
         iter = IDL_LIST(iter).next)
    {
        IDL_tree data = IDL_LIST(iter).data;

        switch(IDL_NODE_TYPE(data)) {
          case IDLN_OP_DCL:
            write_indent(state->file);
            write_method_signature(data, state->file, AS_DECL, NULL);
            fputs(" { return _to ", state->file);
            write_method_signature(data, state->file, AS_CALL, NULL);
            break;

          case IDLN_ATTR_DCL:
            write_indent(state->file);
            if (!write_attr_accessor(data, state->file, TRUE, AS_DECL, NULL))
                FAIL;
            fputs(" { return _to ", state->file);
            if (!write_attr_accessor(data, state->file, TRUE, AS_CALL, NULL))
                FAIL;
            if (!IDL_ATTR_DCL(data).f_readonly) {
                fputs("; } \\\n", state->file); /* Terminate the previous one. */
                write_indent(state->file);
                if (!write_attr_accessor(data, state->file,
                                         FALSE, AS_DECL, NULL))
                    FAIL;
                fputs(" { return _to ", state->file);
                if (!write_attr_accessor(data, state->file,
                                         FALSE, AS_CALL, NULL))
                    FAIL;
                /* '; } \n' at end will clean up. */
            }
            break;

          case IDLN_CONST_DCL:
          case IDLN_CODEFRAG:
              continue;

          default:
            FAIL;
        }

        if (IDL_LIST(iter).next != NULL) {
            fprintf(state->file, "; } \\\n");
        } else {
            fprintf(state->file, "; } \n");
        }
    }
    fputc('\n', state->file);


    /* XXX abstract above and below into one function? */
    /*
     * #define NS_FORWARD_SAFE_NSIFOO - create forwarding methods that can delegate
     * behavior from in implementation to another object.  As generated by
     * idlc.
     */
    fprintf(state->file,
            "/* Use this macro to declare functions that forward the "
            "behavior of this interface to another object in a safe way. */\n"
            "#define NS_FORWARD_SAFE_%s(_to) \\\n",
            classNameUpper);
    if (IDL_INTERFACE(state->tree).body == NULL) {
        write_indent(state->file);
        fputs("/* no methods! */\n", state->file);
    }

    for (iter = IDL_INTERFACE(state->tree).body;
         iter != NULL;
         iter = IDL_LIST(iter).next)
    {
        IDL_tree data = IDL_LIST(iter).data;

        switch(IDL_NODE_TYPE(data)) {
          case IDLN_OP_DCL:
            write_indent(state->file);
            write_method_signature(data, state->file, AS_DECL, NULL);
            fputs(" { return !_to ? NS_ERROR_NULL_POINTER : _to->", state->file);
            write_method_signature(data, state->file, AS_CALL, NULL);
            break;

          case IDLN_ATTR_DCL:
            write_indent(state->file);
            if (!write_attr_accessor(data, state->file, TRUE, AS_DECL, NULL))
                FAIL;
            fputs(" { return !_to ? NS_ERROR_NULL_POINTER : _to->", state->file);
            if (!write_attr_accessor(data, state->file, TRUE, AS_CALL, NULL))
                FAIL;
            if (!IDL_ATTR_DCL(data).f_readonly) {
                fputs("; } \\\n", state->file); /* Terminate the previous one. */
                write_indent(state->file);
                if (!write_attr_accessor(data, state->file,
                                         FALSE, AS_DECL, NULL))
                    FAIL;
                fputs(" { return !_to ? NS_ERROR_NULL_POINTER : _to->", state->file);
                if (!write_attr_accessor(data, state->file,
                                         FALSE, AS_CALL, NULL))
                    FAIL;
                /* '; } \n' at end will clean up. */
            }
            break;

          case IDLN_CONST_DCL:
          case IDLN_CODEFRAG:
              continue;

          default:
            FAIL;
        }

        if (IDL_LIST(iter).next != NULL) {
            fprintf(state->file, "; } \\\n");
        } else {
            fprintf(state->file, "; } \n");
        }
    }
    fputc('\n', state->file);

    /*
     * Build a sample implementation template.
     */
    if (strlen(className) >= 3 && className[2] == 'I') {
        classNameImpl = xpidl_strdup(className);
        if (!classNameImpl)
            FAIL;
        memmove(&classNameImpl[2], &classNameImpl[3], strlen(classNameImpl) - 2);
    } else {
        classNameImpl = xpidl_strdup("_MYCLASS_");
        if (!classNameImpl)
            FAIL;
    }

    fputs("#if 0\n"
          "/* Use the code below as a template for the "
          "implementation class for this interface. */\n"
          "\n"
          "/* Header file */"
          "\n",
          state->file);
    fprintf(state->file, "class %s : public %s\n", classNameImpl, className);
    fputs("{\n"
          "public:\n", state->file);
    write_indent(state->file);
    fputs("NS_DECL_ISUPPORTS\n", state->file);
    write_indent(state->file);
    fprintf(state->file, "NS_DECL_%s\n", classNameUpper);
    fputs("\n", state->file);
    write_indent(state->file);
    fprintf(state->file, "%s();\n", classNameImpl);
    write_indent(state->file);
    fprintf(state->file, "virtual ~%s();\n", classNameImpl);
    write_indent(state->file);
    fputs("/* additional members */\n", state->file);
    fputs("};\n\n", state->file);

    fputs("/* Implementation file */\n", state->file);

    fprintf(state->file, 
            "NS_IMPL_ISUPPORTS1(%s, %s)\n", classNameImpl, className);
    fputs("\n", state->file);
    
    fprintf(state->file, "%s::%s()\n", classNameImpl, classNameImpl);
    fputs("{\n", state->file);
    write_indent(state->file);
    fputs("NS_INIT_ISUPPORTS();\n", state->file);
    write_indent(state->file);
    fputs("/* member initializers and constructor code */\n", state->file);
    fputs("}\n\n", state->file);
    
    fprintf(state->file, "%s::~%s()\n", classNameImpl, classNameImpl);
    fputs("{\n", state->file);
    write_indent(state->file);
    fputs("/* destructor code */\n", state->file);
    fputs("}\n\n", state->file);

    for (iter = IDL_INTERFACE(state->tree).body;
         iter != NULL;
         iter = IDL_LIST(iter).next)
    {
        IDL_tree data = IDL_LIST(iter).data;

        switch(IDL_NODE_TYPE(data)) {
          case IDLN_OP_DCL:
            /* It would be nice to remove this state-twiddling. */
            orig = state->tree; 
            state->tree = data;
            xpidl_write_comment(state, 0);
            state->tree = orig;

            write_method_signature(data, state->file, AS_IMPL, classNameImpl);
            fputs("\n{\n", state->file);
            write_indent(state->file);
            write_indent(state->file);
            fputs("return NS_ERROR_NOT_IMPLEMENTED;\n"
                  "}\n"
                  "\n", state->file);
            break;

          case IDLN_ATTR_DCL:
            /* It would be nice to remove this state-twiddling. */
            orig = state->tree; 
            state->tree = data;
            xpidl_write_comment(state, 0);
            state->tree = orig;
            
            if (!write_attr_accessor(data, state->file, TRUE, 
                                     AS_IMPL, classNameImpl))
                FAIL;
            fputs("\n{\n", state->file);
            write_indent(state->file);
            write_indent(state->file);
            fputs("return NS_ERROR_NOT_IMPLEMENTED;\n"
                  "}\n", state->file);

            if (!IDL_ATTR_DCL(data).f_readonly) {
                if (!write_attr_accessor(data, state->file, FALSE, 
                                         AS_IMPL, classNameImpl))
                    FAIL;
                fputs("\n{\n", state->file);
                write_indent(state->file);
                write_indent(state->file);
                fputs("return NS_ERROR_NOT_IMPLEMENTED;\n"
                      "}\n", state->file);
            }
            fputs("\n", state->file);
            break;

          case IDLN_CONST_DCL:
          case IDLN_CODEFRAG:
              continue;

          default:
            FAIL;
        }
    }

    fputs("/* End of implementation class template. */\n"
          "#endif\n"
          "\n", state->file);

#undef FAIL

out:
    if (classNameUpper)
        free(classNameUpper);
    if (classNameImpl)
        free(classNameImpl);
    return ok;
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
write_type(IDL_tree type_tree, gboolean is_out, FILE *outfile)
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
            if (IDL_tree_property_get(type_tree, "domstring")) {
                fputs("nsAString", outfile);
            } else {
                fputs(IDL_NATIVE(IDL_NODE_UP(type_tree)).user_type, outfile);
            }
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

/*
 *  AS_DECL writes 'NS_IMETHOD foo(string bar, long sil)'
 *  AS_IMPL writes 'NS_IMETHODIMP className::foo(string bar, long sil)'
 *  AS_CALL writes 'foo(bar, sil)'
 */
static gboolean
write_attr_accessor(IDL_tree attr_tree, FILE * outfile,
                    gboolean getter, int mode, const char *className)
{
    char *attrname = ATTR_IDENT(attr_tree).str;

    if (mode == AS_DECL) {
        fputs("NS_IMETHOD ", outfile);
    } else if (mode == AS_IMPL) {
        fprintf(outfile, "NS_IMETHODIMP %s::", className);
    }
    fprintf(outfile, "%cet%c%s(",
            getter ? 'G' : 'S',
            toupper(*attrname), attrname + 1);
    if (mode == AS_DECL || mode == AS_IMPL) {
        /* Setters for string, wstring, nsid, and domstring get const. 
         */
        if (!getter &&
            (IDL_NODE_TYPE(ATTR_TYPE_DECL(attr_tree)) == IDLN_TYPE_STRING ||
             IDL_NODE_TYPE(ATTR_TYPE_DECL(attr_tree)) == IDLN_TYPE_WIDE_STRING ||
             IDL_tree_property_get(ATTR_TYPE_DECL(attr_tree), "nsid") ||
             IDL_tree_property_get(ATTR_TYPE_DECL(attr_tree), "domstring")))
        {
            fputs("const ", outfile);
        }

        if (!write_type(ATTR_TYPE_DECL(attr_tree), getter, outfile))
            return FALSE;
        fprintf(outfile, "%s%s",
                (STARRED_TYPE(attr_tree) ? "" : " "),
                (getter && !DIPPER_TYPE(ATTR_TYPE_DECL(attr_tree)))? "*" : "");
    }
    fprintf(outfile, "a%c%s)", toupper(attrname[0]), attrname + 1);
    return TRUE;
}

static gboolean
attr_dcl(TreeState *state)
{
    GSList *doc_comments;

    if (!verify_attribute_declaration(state->tree))
        return FALSE;

    doc_comments =
        IDL_IDENT(IDL_LIST(IDL_ATTR_DCL
                           (state->tree).simple_declarations).data).comments;

    if (doc_comments != NULL) {
        write_indent(state->file);
        printlist(state->file, doc_comments);
    }

    /*
     * XXX lists of attributes with the same type, e.g.
     * attribute string foo, bar sil;
     * are legal IDL... but we don't do anything with 'em.
     */
    if (IDL_LIST(IDL_ATTR_DCL(state->tree).simple_declarations).next != NULL) {
        XPIDL_WARNING((state->tree, IDL_WARNING1,
                       "multiple attributes in a single declaration aren't "
                       "currently supported by xpidl"));
    }

    xpidl_write_comment(state, 2);

    write_indent(state->file);
    if (!write_attr_accessor(state->tree, state->file, TRUE, AS_DECL, NULL))
        return FALSE;
    fputs(" = 0;\n", state->file);

    if (!IDL_ATTR_DCL(state->tree).f_readonly) {
        write_indent(state->file);
        if (!write_attr_accessor(state->tree, state->file, FALSE, AS_DECL, NULL))
            return FALSE;
        fputs(" = 0;\n", state->file);
    }
    fputc('\n', state->file);

    return TRUE;
}

static gboolean
do_enum(TreeState *state)
{
    IDL_tree_error(state->tree, "enums not supported, "
                   "see http://bugzilla.mozilla.org/show_bug.cgi?id=8781");
    return FALSE;
}

static gboolean
do_const_dcl(TreeState *state)
{
    struct _IDL_CONST_DCL *dcl = &IDL_CONST_DCL(state->tree);
    const char *name = IDL_IDENT(dcl->ident).str;
    gboolean is_signed;
    GSList *doc_comments = IDL_IDENT(dcl->ident).comments;
    IDL_tree real_type;
    const char *const_format;

    if (!verify_const_declaration(state->tree))
        return FALSE;

    if (doc_comments != NULL) {
        write_indent(state->file);
        printlist(state->file, doc_comments);
    }

    /* Could be a typedef; try to map it to the real type. */
    real_type = find_underlying_type(dcl->const_type);
    real_type = real_type ? real_type : dcl->const_type;
    is_signed = IDL_TYPE_INTEGER(real_type).f_signed;

    const_format = is_signed ? "%" IDL_LL "d" : "%" IDL_LL "uU";
    write_indent(state->file);
    fprintf(state->file, "enum { %s = ", name);
    fprintf(state->file, const_format, IDL_INTEGER(dcl->const_exp).value);
    fprintf(state->file, " };\n\n");

    return TRUE;
}

static gboolean
do_typedef(TreeState *state)
{
    IDL_tree type = IDL_TYPE_DCL(state->tree).type_spec;
    IDL_tree dcls = IDL_TYPE_DCL(state->tree).dcls;
    IDL_tree complex;
    GSList *doc_comments;

    if (IDL_NODE_TYPE(type) == IDLN_TYPE_SEQUENCE) {
        XPIDL_WARNING((state->tree, IDL_WARNING1,
                       "sequences not supported, ignored"));
    } else {
        if (IDL_NODE_TYPE(complex = IDL_LIST(dcls).data) == IDLN_TYPE_ARRAY) {
            IDL_tree dim = IDL_TYPE_ARRAY(complex).size_list;
            doc_comments = IDL_IDENT(IDL_TYPE_ARRAY(complex).ident).comments;

            if (doc_comments != NULL)
                printlist(state->file, doc_comments);

            fputs("typedef ", state->file);
            if (!write_type(type, FALSE, state->file))
                return FALSE;
            fputs(" ", state->file);

            fprintf(state->file, "%s",
                    IDL_IDENT(IDL_TYPE_ARRAY(complex).ident).str);
            do {
                fputc('[', state->file);
                if (IDL_LIST(dim).data) {
                    fprintf(state->file, "%ld",
                            (long)IDL_INTEGER(IDL_LIST(dim).data).value);
                }
                fputc(']', state->file);
            } while ((dim = IDL_LIST(dim).next) != NULL);
        } else {
            doc_comments = IDL_IDENT(IDL_LIST(dcls).data).comments;

            if (doc_comments != NULL)
                printlist(state->file, doc_comments);

            fputs("typedef ", state->file);
            if (!write_type(type, FALSE, state->file))
                return FALSE;
            fputs(" ", state->file);
            fputs(IDL_IDENT(IDL_LIST(dcls).data).str, state->file);
        }
        fputs(";\n\n", state->file);
    }
    return TRUE;
}

/*
 * param generation:
 * in string foo        -->     nsString *foo
 * out string foo       -->     nsString **foo;
 * inout string foo     -->     nsString **foo;
 */

/* If notype is true, just write the param name. */
static gboolean
write_param(IDL_tree param_tree, FILE *outfile)
{
    IDL_tree param_type_spec = IDL_PARAM_DCL(param_tree).param_type_spec;
    gboolean is_in = IDL_PARAM_DCL(param_tree).attr == IDL_PARAM_IN;
    /* in string, wstring, nsid, domstring, and any 
     * explicitly marked [const] are const 
     */

    if (is_in &&
        (IDL_NODE_TYPE(param_type_spec) == IDLN_TYPE_STRING ||
         IDL_NODE_TYPE(param_type_spec) == IDLN_TYPE_WIDE_STRING ||
         IDL_tree_property_get(IDL_PARAM_DCL(param_tree).simple_declarator,
                               "const") ||
         IDL_tree_property_get(param_type_spec, "nsid") ||
         IDL_tree_property_get(param_type_spec, "domstring"))) {
        fputs("const ", outfile);
    }
    else if (IDL_PARAM_DCL(param_tree).attr == IDL_PARAM_OUT &&
             IDL_tree_property_get(IDL_PARAM_DCL(param_tree).simple_declarator, 
                                   "shared")) {
        fputs("const ", outfile);
    }

    if (!write_type(param_type_spec, !is_in, outfile))
        return FALSE;

    /* unless the type ended in a *, add a space */
    if (!STARRED_TYPE(param_type_spec))
        fputc(' ', outfile);

    /* out and inout params get a bonus '*' (unless this is type that has a 
     * 'dipper' class that is passed in to receive 'out' data) 
     */
    if (IDL_PARAM_DCL(param_tree).attr != IDL_PARAM_IN &&
        !DIPPER_TYPE(param_type_spec)) {
        fputc('*', outfile);
    }
    /* arrays get a bonus * too */
    /* XXX Should this be a leading '*' or a trailing "[]" ?*/
    if (IDL_tree_property_get(IDL_PARAM_DCL(param_tree).simple_declarator,
                              "array"))
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
    const char *className = IDL_IDENT(IDL_FORWARD_DCL(iface).ident).str;
    GSList *doc_comments = IDL_IDENT(IDL_INTERFACE(iface).ident).comments;

    if (!className)
        return FALSE;

    if (doc_comments != NULL)
        printlist(state->file, doc_comments);

    fprintf(state->file, "class %s; /* forward declaration */\n\n", className);
    return TRUE;
}

/*
 * Shared between the interface class declaration and the NS_DECL_IFOO macro
 * provided to aid declaration of implementation classes.  
 * mode...
 *  AS_DECL writes 'NS_IMETHOD foo(string bar, long sil)'
 *  AS_IMPL writes 'NS_IMETHODIMP className::foo(string bar, long sil)'
 *  AS_CALL writes 'foo(bar, sil)'
 */
static gboolean
write_method_signature(IDL_tree method_tree, FILE *outfile, int mode,
                       const char *className)
{
    struct _IDL_OP_DCL *op = &IDL_OP_DCL(method_tree);
    gboolean no_generated_args = TRUE;
    gboolean op_notxpcom =
        (IDL_tree_property_get(op->ident, "notxpcom") != NULL);
    const char *name;
    IDL_tree iter;

    if (mode == AS_DECL) {
        if (op_notxpcom) {
            fputs("NS_IMETHOD_(", outfile);
            if (!write_type(op->op_type_spec, FALSE, outfile))
                return FALSE;
            fputc(')', outfile);
        } else {
            fputs("NS_IMETHOD", outfile);
        }
        fputc(' ', outfile);
    }
    else if (mode == AS_IMPL) {
        if (op_notxpcom) {
            fputs("NS_IMETHODIMP_(", outfile);
            if (!write_type(op->op_type_spec, FALSE, outfile))
                return FALSE;
            fputc(')', outfile);
        } else {
            fputs("NS_IMETHODIMP", outfile);
        }
        fputc(' ', outfile);
    }
    name = IDL_IDENT(op->ident).str;
    if (mode == AS_IMPL) {
        fprintf(outfile, "%s::%c%s(", className, toupper(*name), name + 1);
    } else {
        fprintf(outfile, "%c%s(", toupper(*name), name + 1);
    }
    for (iter = op->parameter_dcls; iter; iter = IDL_LIST(iter).next) {
        if (mode == AS_DECL || mode == AS_IMPL) {
            if (!write_param(IDL_LIST(iter).data, outfile))
                return FALSE;
        } else {
            fputs(IDL_IDENT(IDL_PARAM_DCL(IDL_LIST(iter).data)
                            .simple_declarator).str,
                  outfile);
        }
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
        if (mode == AS_DECL || mode == AS_IMPL) {
            if (!write_param(fake_param, outfile))
                return FALSE;
        } else {
            fputs("_retval", outfile);
        }
        if (op->f_varargs)
            fputs(", ", outfile);
        no_generated_args = FALSE;
    }

    /* varargs go last */
    if (op->f_varargs) {
        if (mode == AS_DECL || mode == AS_IMPL) {
            fputs("nsVarArgs *", outfile);
        }
        fputs("_varargs", outfile);
        no_generated_args = FALSE;
    }

    /*
     * If generated method has no arguments, output 'void' to avoid C legacy
     * behavior of disabling type checking.
     */
    if (no_generated_args && mode == AS_DECL) {
        fputs("void", outfile);
    }

    fputc(')', outfile);

    return TRUE;
}

/*
 * A method is an `operation', therefore a method decl is an `op dcl'.
 * I blame Elliot.
 */
static gboolean
op_dcl(TreeState *state)
{
    GSList *doc_comments = IDL_IDENT(IDL_OP_DCL(state->tree).ident).comments;

    /*
     * Verify that e.g. non-scriptable methods in [scriptable] interfaces
     * are declared so.  Do this in a separate verification pass?
     */
    if (!verify_method_declaration(state->tree))
        return FALSE;

    if (doc_comments != NULL) {
        write_indent(state->file);
        printlist(state->file, doc_comments);
    }
    xpidl_write_comment(state, 2);

    write_indent(state->file);
    if (!write_method_signature(state->tree, state->file, AS_DECL, NULL))
        return FALSE;
    fputs(" = 0;\n\n", state->file);

    return TRUE;
}

static void
write_codefrag_line(gpointer data, gpointer user_data)
{
    TreeState *state = (TreeState *)user_data;
    const char *line = (const char *)data;
    fputs(line, state->file);
    fputc('\n', state->file);
}

static gboolean
codefrag(TreeState *state)
{
    const char *desc = IDL_CODEFRAG(state->tree).desc;
    GSList *lines = IDL_CODEFRAG(state->tree).lines;
    guint fragment_length;
    
    if (strcmp(desc, "C++") && /* libIDL bug? */ strcmp(desc, "C++\r")) {
        XPIDL_WARNING((state->tree, IDL_WARNING1,
                       "ignoring '%%{%s' escape. "
                       "(Use '%%{C++' to escape verbatim C++ code.)", desc));

        return TRUE;
    }

    /*
     * Emit #file directive to point debuggers back to the original .idl file
     * for the duration of the code fragment.  We look at internal IDL node
     * properties _file, _line to do this; hopefully they won't change.
     *
     * _line seems to refer to the line immediately after the closing %}, so
     * we backtrack to get the proper line for the beginning of the block.
     */
    /*
     * Looks like getting this right means maintaining an accurate line
     * count of everything generated, so we can set the file back to the
     * correct line in the generated file afterwards.  Skipping for now...
     */

    fragment_length = g_slist_length(lines);
/*      fprintf(state->file, "#line %d \"%s\"\n", */
/*              state->tree->_line - fragment_length - 1, */
/*              state->tree->_file); */

    g_slist_foreach(lines, write_codefrag_line, (gpointer)state);

    return TRUE;
}

backend *
xpidl_header_dispatch(void)
{
    static backend result;
    static nodeHandler table[IDLN_LAST];
    static gboolean initialized = FALSE;
    
    result.emit_prolog = header_prolog;
    result.emit_epilog = header_epilog;

    if (!initialized) {
        table[IDLN_LIST] = list;
        table[IDLN_ATTR_DCL] = attr_dcl;
        table[IDLN_OP_DCL] = op_dcl;
        table[IDLN_FORWARD_DCL] = forward_dcl;
        table[IDLN_TYPE_ENUM] = do_enum;
        table[IDLN_INTERFACE] = interface;
        table[IDLN_CODEFRAG] = codefrag;
        table[IDLN_TYPE_DCL] = do_typedef;
        table[IDLN_CONST_DCL] = do_const_dcl;
        table[IDLN_NATIVE] = check_native;
        initialized = TRUE;
    }

    result.dispatch_table = table;
    return &result;
}
