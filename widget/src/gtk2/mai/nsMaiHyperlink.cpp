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

#include "nsIURI.h"
#include "nsMaiHyperlink.h"
#include "nsMaiWidget.h"

G_BEGIN_DECLS
/* callbacks for AtkHyperlink */
static void classInitCB(AtkHyperlinkClass *aClass);
static void finalizeCB(GObject *aObj);

/* callbacks for AtkHyperlink virtual functions */
static gchar *getUriCB(AtkHyperlink *aLink, gint aLinkIndex);
static AtkObject *getObjectCB(AtkHyperlink *aLink, gint aLinkIndex);
static gint getEndIndexCB(AtkHyperlink *aLink);
static gint getStartIndexCB(AtkHyperlink *aLink);
static gboolean isValidCB(AtkHyperlink *aLink);
static gint getAnchorCountCB(AtkHyperlink *aLink);
G_END_DECLS

static gpointer parent_class = NULL;

GType
mai_atk_hyperlink_get_type(void)
{
    static GType type = 0;

    if (!type) {
        static const GTypeInfo tinfo = {
            sizeof(MaiAtkHyperlinkClass),
            (GBaseInitFunc)NULL,
            (GBaseFinalizeFunc)NULL,
            (GClassInitFunc)classInitCB,
            (GClassFinalizeFunc)NULL,
            NULL, /* class data */
            sizeof(MaiAtkHyperlink), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc)NULL,
            NULL /* value table */
        };

        type = g_type_register_static(ATK_TYPE_HYPERLINK,
                                      "MaiAtkHyperlink",
                                      &tinfo, GTypeFlags(0));
    }
    return type;
}

MaiHyperlink::MaiHyperlink(nsIAccessibleHyperLink *aAcc)
{
    mHyperlink = aAcc;
    mMaiAtkHyperlink = NULL;
    mURI = (char*)NULL;
}

MaiHyperlink::~MaiHyperlink()
{
    NS_ASSERTION((mMaiAtkHyperlink == NULL),
                 "Cannot Del MaiHyperlink: alive atk hyperlink");
}

AtkHyperlink *
MaiHyperlink::GetAtkHyperlink(void)
{
    g_return_val_if_fail(mHyperlink != NULL, NULL);

    if (mMaiAtkHyperlink)
        return (AtkHyperlink*)mMaiAtkHyperlink;

    nsCOMPtr<nsIAccessibleHyperLink> accessIf(do_QueryInterface(mHyperlink));
    if (!accessIf)
        return NULL;

    mMaiAtkHyperlink = (MaiAtkHyperlink*)
        g_object_new(mai_atk_hyperlink_get_type(), NULL);
    g_return_val_if_fail(mMaiAtkHyperlink != NULL, NULL);

    /* be sure to initialize it with "this" */
    MaiHyperlink::Initialize(mMaiAtkHyperlink, this);

    return (AtkHyperlink*)mMaiAtkHyperlink;
}

void
MaiHyperlink::Finalize(void)
{
    //here we know that the action is originated from the MaiAtkHyperlink,
    //and the MaiAtkHyperlink itself will be destroyed.
    //Mark MaiAtkHyperlink to nil
    mMaiAtkHyperlink = NULL;

    delete this;
}

const gchar *
MaiHyperlink::GetUri(gint aLinkIndex)
{
    g_return_val_if_fail(mHyperlink != NULL, NULL);

    if (!mURI.IsEmpty())
        return (char*)mURI.get();
    nsCOMPtr<nsIURI> uri;
    nsresult rv = mHyperlink->GetURI(aLinkIndex,getter_AddRefs(uri));
    if (NS_FAILED(rv) || !uri)
        return NULL;
    rv = uri->GetSpec(mURI);
    return (NS_FAILED(rv)) ? NULL : mURI.get();
}

MaiObject *
MaiHyperlink::GetObject(gint aLinkIndex)
{
    g_return_val_if_fail(mHyperlink != NULL, NULL);

    nsCOMPtr<nsIAccessible> accObj;
    nsresult rv = mHyperlink->GetObject(aLinkIndex, getter_AddRefs(accObj));
    if (NS_FAILED(rv) || !accObj)
        return NULL;

    /* ??? when the new one get freed? */
    MaiWidget *maiObj = new MaiWidget(accObj);
    return maiObj;
}

gint
MaiHyperlink::GetEndIndex()
{
    g_return_val_if_fail(mHyperlink != NULL, -1);

    PRInt32 endIndex = -1;
    nsresult rv = mHyperlink->GetEndIndex(&endIndex);

    return (NS_FAILED(rv)) ? -1 : NS_STATIC_CAST(gint, endIndex);
}

gint
MaiHyperlink::GetStartIndex()
{
    g_return_val_if_fail(mHyperlink != NULL, -1);

    PRInt32 startIndex = -1;
    nsresult rv = mHyperlink->GetStartIndex(&startIndex);

    return (NS_FAILED(rv)) ? -1 : NS_STATIC_CAST(gint, startIndex);
}

gboolean
MaiHyperlink::IsValid()
{
    g_return_val_if_fail(mHyperlink != NULL, FALSE);

    PRBool isValid = PR_FALSE;
    nsresult rv = mHyperlink->IsValid(&isValid);
    return (NS_FAILED(rv)) ? FALSE : NS_STATIC_CAST(gboolean, isValid);
}

gint
MaiHyperlink::GetAnchorCount()
{
    g_return_val_if_fail(mHyperlink != NULL, -1);

    PRInt32 count = -1;
    nsresult rv = mHyperlink->GetAnchors(&count);
    return (NS_FAILED(rv)) ? -1 : NS_STATIC_CAST(gint, count);
}

/* static */

/* remember to call this static function when a MaiAtkHyperlink
 * is created
 */

void
MaiHyperlink::Initialize(MaiAtkHyperlink *aObj, MaiHyperlink *aHyperlink)
{
    g_return_if_fail(MAI_IS_ATK_HYPERLINK(aObj));
    g_return_if_fail(aHyperlink != NULL);

    /* initialize hyperlink */
    MAI_ATK_HYPERLINK(aObj)->maiHyperlink = aHyperlink;
}

/* static functions for ATK callbacks */

void
classInitCB(AtkHyperlinkClass *aClass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(aClass);

    parent_class = g_type_class_peek_parent(aClass);

    aClass->get_uri = getUriCB;
    aClass->get_object = getObjectCB;
    aClass->get_end_index = getEndIndexCB;
    aClass->get_start_index = getStartIndexCB;
    aClass->is_valid = isValidCB;
    aClass->get_n_anchors = getAnchorCountCB;

    gobject_class->finalize = finalizeCB;
}

void
finalizeCB(GObject *aObj)
{
    g_return_if_fail(MAI_IS_ATK_HYPERLINK(aObj));

    MaiHyperlink *maiHyperlink = MAI_ATK_HYPERLINK(aObj)->maiHyperlink;
    if (maiHyperlink)
        maiHyperlink->Finalize();

    // never call MaiHyperlink later
    MAI_ATK_HYPERLINK(aObj)->maiHyperlink = NULL;

    /* call parent finalize function */
    if (G_OBJECT_CLASS (parent_class)->finalize)
        G_OBJECT_CLASS (parent_class)->finalize(aObj);
}

#define MAI_ATK_HYPERLINK_RETURN_VAL_IF_FAIL(obj, val) \
    do {\
        g_return_val_if_fail(MAI_IS_ATK_HYPERLINK(obj), val);\
        MaiHyperlink * tmpMaiHyperlinkIn = \
            MAI_ATK_HYPERLINK(obj)->maiHyperlink;\
        g_return_val_if_fail(tmpMaiHyperlinkIn != NULL, val);\
        g_return_val_if_fail(tmpMaiHyperlinkIn->GetAtkHyperlink() ==\
                             (AtkHyperlink*)obj, val);\
    } while (0)

gchar *
getUriCB(AtkHyperlink *aLink, gint aLinkIndex)
{
    MAI_ATK_HYPERLINK_RETURN_VAL_IF_FAIL(aLink, NULL);

    MaiHyperlink *maiHyperlink = MAI_ATK_HYPERLINK(aLink)->maiHyperlink;

    /* it has been checked that maiHyperlink is non-null */
    return NS_CONST_CAST(gchar*, (maiHyperlink->GetUri(aLinkIndex)));
}

AtkObject *
getObjectCB(AtkHyperlink *aLink, gint aLinkIndex)
{
    MAI_ATK_HYPERLINK_RETURN_VAL_IF_FAIL(aLink, NULL);

    MaiHyperlink *maiHyperlink = MAI_ATK_HYPERLINK(aLink)->maiHyperlink;

    MaiObject *maiObj = maiHyperlink->GetObject(aLinkIndex);
    if (!maiObj)
        return NULL;

    //no need to add ref it, because it is "get" not "ref" ???
    return maiObj->GetAtkObject();
}

gint
getEndIndexCB(AtkHyperlink *aLink)
{
    MAI_ATK_HYPERLINK_RETURN_VAL_IF_FAIL(aLink, -1);

    MaiHyperlink *maiHyperlink = MAI_ATK_HYPERLINK(aLink)->maiHyperlink;

    return maiHyperlink->GetEndIndex();
}

gint
getStartIndexCB(AtkHyperlink *aLink)
{
    MAI_ATK_HYPERLINK_RETURN_VAL_IF_FAIL(aLink, -1);

    MaiHyperlink *maiHyperlink = MAI_ATK_HYPERLINK(aLink)->maiHyperlink;

    return maiHyperlink->GetStartIndex();
}

gboolean
isValidCB(AtkHyperlink *aLink)
{
    MAI_ATK_HYPERLINK_RETURN_VAL_IF_FAIL(aLink, FALSE);

    MaiHyperlink *maiHyperlink = MAI_ATK_HYPERLINK(aLink)->maiHyperlink;

    return maiHyperlink->IsValid();
}

gint
getAnchorCountCB(AtkHyperlink *aLink)
{
    MAI_ATK_HYPERLINK_RETURN_VAL_IF_FAIL(aLink, FALSE);

    MaiHyperlink *maiHyperlink = MAI_ATK_HYPERLINK(aLink)->maiHyperlink;

    return maiHyperlink->GetAnchorCount();
}
