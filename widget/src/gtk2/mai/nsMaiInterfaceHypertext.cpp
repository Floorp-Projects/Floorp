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

#include "nsIAccessibleHyperText.h"
#include "nsMaiInterfaceHypertext.h"

/* helpers */
static MaiInterfaceHypertext *getHypertext(AtkHypertext *aIface);

G_BEGIN_DECLS

static void interfaceInitCB(AtkHypertextIface *aIface);

/* hypertext interface callbacks */
static AtkHyperlink *getLinkCB(AtkHypertext *aText, gint aLinkIndex);
static gint getLinkCountCB(AtkHypertext *aText);
static gint getLinkIndexCB(AtkHypertext *aText, gint aCharIndex);

G_END_DECLS

MaiInterfaceHypertext::MaiInterfaceHypertext(MaiWidget *aMaiWidget):
    MaiInterface(aMaiWidget)
{
    mMaiHyperlink = NULL;
}

MaiInterfaceHypertext::~MaiInterfaceHypertext()
{
    if (mMaiHyperlink) {
        g_object_unref(G_OBJECT(mMaiHyperlink->GetAtkHyperlink()));
        mMaiHyperlink = NULL;
    }
}

MaiInterfaceType
MaiInterfaceHypertext::GetType()
{
    return MAI_INTERFACE_HYPERTEXT;
}

const GInterfaceInfo *
MaiInterfaceHypertext::GetInterfaceInfo()
{
    static const GInterfaceInfo atk_if_hypertext_info = {
        (GInterfaceInitFunc)interfaceInitCB,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };
    return &atk_if_hypertext_info;
}

#define MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, value) \
    nsCOMPtr<nsIAccessibleHyperText> \
        accessIface(do_QueryInterface(GetNSAccessible())); \
    if (!(accessIface)) \
        return (value)

#define MAI_IFACE_RETURN_IF_FAIL(accessIface) \
    nsCOMPtr<nsIAccessibleHyperText> \
        accessIface(do_QueryInterface(GetNSAccessible())); \
    if (!(accessIface)) \
        return

MaiHyperlink *
MaiInterfaceHypertext::GetLink(gint aLinkIndex)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    nsCOMPtr<nsIAccessibleHyperLink> hyperLink;
    nsresult rv = accessIface->GetLink(aLinkIndex, getter_AddRefs(hyperLink));
    if (NS_FAILED(rv) || !hyperLink)
        return NULL;

    //release our ref to the previous one
    if (mMaiHyperlink)
        g_object_unref(G_OBJECT(mMaiHyperlink->GetAtkHyperlink()));

    mMaiHyperlink = new MaiHyperlink(hyperLink);
    NS_ASSERTION(mMaiHyperlink, "Fail to create MaiHyperlink object");
    return mMaiHyperlink;
}

gint
MaiInterfaceHypertext::GetLinkCount()
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, -1);

    PRInt32 count = -1;
    nsresult rv = accessIface->GetLinks(&count);
    if (NS_FAILED(rv))
        return -1;
    return NS_STATIC_CAST(gint, count);
}

gint
MaiInterfaceHypertext::GetLinkIndex(gint aCharIndex)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, -1);

    PRInt32 index = -1;
    nsresult rv = accessIface->GetLinkIndex(aCharIndex, &index);
    if (NS_FAILED(rv))
        return -1;
    return NS_STATIC_CAST(gint, index);
}

/* statics */

/* do general checking for callbacks functions
 * return the MaiInterfaceHypertext extracted from atk hypertext
 */
MaiInterfaceHypertext *
getHypertext(AtkHypertext *aHypertext)
{
    g_return_val_if_fail(MAI_IS_ATK_WIDGET(aHypertext), NULL);
    MaiWidget *maiWidget =
        NS_STATIC_CAST(MaiWidget*, (MAI_ATK_OBJECT(aHypertext)->maiObject));
    g_return_val_if_fail(maiWidget != NULL, NULL);
    g_return_val_if_fail(maiWidget->GetAtkObject() == (AtkObject*)aHypertext,
                         NULL);
    MaiInterfaceHypertext *maiInterfaceHypertext =
        NS_STATIC_CAST(MaiInterfaceHypertext*,
                       maiWidget->GetMaiInterface(MAI_INTERFACE_HYPERTEXT));
    return maiInterfaceHypertext;
}

void
interfaceInitCB(AtkHypertextIface *aIface)
{
    g_return_if_fail(aIface != NULL);

    aIface->get_link = getLinkCB;
    aIface->get_n_links = getLinkCountCB;
    aIface->get_link_index = getLinkIndexCB;
}

AtkHyperlink *
getLinkCB(AtkHypertext *aText, gint aTextIndex)
{
    MaiInterfaceHypertext *maiIfaceHypertext = getHypertext(aText);
    if (!maiIfaceHypertext)
        return NULL;
    MaiHyperlink *maiHyperlink = maiIfaceHypertext->GetLink(aTextIndex);
    if (!maiHyperlink)
        return NULL;

    /* we should not addref because it is "get" not "ref" */
    return maiHyperlink->GetAtkHyperlink();
}

gint
getLinkCountCB(AtkHypertext *aText)
{
    MaiInterfaceHypertext *maiIfaceHypertext = getHypertext(aText);
    if (!maiIfaceHypertext)
        return -1;
    return maiIfaceHypertext->GetLinkCount();
}

gint
getLinkIndexCB(AtkHypertext *aText, gint aCharIndex)
{
    MaiInterfaceHypertext *maiIfaceHypertext = getHypertext(aText);
    if (!maiIfaceHypertext)
        return -1;
    return maiIfaceHypertext->GetLinkIndex(aCharIndex);
}
