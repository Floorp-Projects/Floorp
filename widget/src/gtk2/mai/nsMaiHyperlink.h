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

#ifndef __MAI_HYPERLINK_H__
#define __MAI_HYPERLINK_H__


#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIAccessibleHyperLink.h"

#include "nsMaiObject.h"

class MaiHyperlink;

/* MaiAtkHyperlink */

#define MAI_TYPE_ATK_HYPERLINK      (mai_atk_hyperlink_get_type ())
#define MAI_ATK_HYPERLINK(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
                                     MAI_TYPE_ATK_HYPERLINK, MaiAtkHyperlink))
#define MAI_ATK_HYPERLINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),\
                                 MAI_TYPE_ATK_HYPERLINK, MaiAtkHyperlinkClass))
#define MAI_IS_ATK_HYPERLINK(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj),\
                                        MAI_TYPE_ATK_HYPERLINK))
#define MAI_IS_ATK_HYPERLINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),\
                                        MAI_TYPE_ATK_HYPERLINK))
#define MAI_ATK_HYPERLINK_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                 MAI_TYPE_ATK_HYPERLINK, MaiAtkHyperlinkClass))

typedef struct _MaiAtkHyperlink                MaiAtkHyperlink;
typedef struct _MaiAtkHyperlinkClass           MaiAtkHyperlinkClass;

/**
 * This MaiAtkHyperlink is a thin wrapper, in the MAI namespace,
 * for AtkHyperlink
 */

struct _MaiAtkHyperlink
{
    AtkHyperlink parent;

    /*
     * The MaiHyperlink whose properties and features are exported via this
     * hyperlink instance.
     */
    MaiHyperlink *maiHyperlink;
};

struct _MaiAtkHyperlinkClass
{
    AtkHyperlinkClass parent_class;
};

GType mai_atk_hyperlink_get_type(void);

/*
 * MaiHyperlink is a auxiliary class for MaiInterfaceHyperText.
 */

class MaiHyperlink
{
public:
    MaiHyperlink(nsIAccessibleHyperLink *aAcc);
    ~MaiHyperlink();

public:
    AtkHyperlink *GetAtkHyperlink(void);

    /* functions called by callbacks */
    void Finalize(void);

    const gchar *GetUri(gint aLinkIndex);
    MaiObject *GetObject(gint aLinkIndex);
    gint GetEndIndex(void);
    gint GetStartIndex(void);
    gboolean IsValid(void);
    gint GetAnchorCount(void);

protected:
    nsCOMPtr<nsIAccessibleHyperLink> mHyperlink;
    MaiAtkHyperlink *mMaiAtkHyperlink;
    nsCString mURI;
public:
    static void Initialize(MaiAtkHyperlink *aObj, MaiHyperlink *aClass);
};
#endif /* __MAI_HYPERLINK_H__ */
