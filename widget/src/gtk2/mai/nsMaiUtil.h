/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2002 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Bolian Yin (bolian.yin@sun.com)
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

#ifndef __MAI_UTIL_H__
#define __MAI_UTIL_H__

#include <atk/atk.h>

#define MAI_TYPE_UTIL              (mai_util_get_type ())
#define MAI_UTIL(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                    MAI_TYPE_UTIL, MaiUtil))
#define MAI_UTIL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                    MAI_TYPE_UTIL, MaiUtilClass))
#define MAI_IS_UTIL(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                    MAI_TYPE_UTIL))
#define MAI_IS_UTIL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                    MAI_TYPE_UTIL))
#define MAI_UTIL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                    MAI_TYPE_UTIL, MaiUtilClass))

typedef struct _MaiUtil                  MaiUtil;
typedef struct _MaiUtilClass             MaiUtilClass;

class MaiAppRoot;
class MaiCache;
  
struct _MaiUtil
{
    AtkUtil parent;
    GList *listener_list;
};

GType mai_util_get_type (void);

struct _MaiUtilClass
{
    AtkUtilClass parent_class;
};

#define MAI_VERSION "0.0.6"
#define MAI_NAME "MAI-Mozilla Atk Interface"

MaiAppRoot *mai_get_root(void);
MaiCache *mai_get_cache(void);
gboolean mai_init(void);

G_BEGIN_DECLS
int gtk_module_init(gint *argc, char** argv[]);
G_END_DECLS

#endif /* __MAI_UTIL_H__ */
