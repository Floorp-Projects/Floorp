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

#include "nsString.h"
#include "nsIAccessibleEditableText.h"
#include "nsMaiInterfaceEditableText.h"

/* helpers */
static MaiInterfaceEditableText *getEditableText(AtkEditableText *aIface);

G_BEGIN_DECLS

static void interfaceInitCB(AtkEditableTextIface *aIface);

/* editabletext interface callbacks */
static gboolean setRunAttributesCB(AtkEditableText *aText,
                                   AtkAttributeSet *aAttribSet,
                                   gint aStartOffset,
                                   gint aEndOffset);
static void setTextContentsCB(AtkEditableText *aText, const gchar *aString);
static void insertTextCB(AtkEditableText *aText,
                         const gchar *aString, gint aLength, gint *aPosition);
static void copyTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos);
static void cutTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos);
static void deleteTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos);
static void pasteTextCB(AtkEditableText *aText, gint aPosition);

G_END_DECLS

MaiInterfaceEditableText::MaiInterfaceEditableText(MaiWidget *aMaiWidget):
    MaiInterface(aMaiWidget)
{
}

MaiInterfaceEditableText::~MaiInterfaceEditableText()
{
}

MaiInterfaceType
MaiInterfaceEditableText::GetType()
{
    return MAI_INTERFACE_EDITABLE_TEXT;
}

const GInterfaceInfo *
MaiInterfaceEditableText::GetInterfaceInfo()
{
    static const GInterfaceInfo atk_if_editabletext_info = {
        (GInterfaceInitFunc)interfaceInitCB,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };
    return &atk_if_editabletext_info;
}

#define MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, value) \
    nsCOMPtr<nsIAccessibleEditableText> \
        accessIface(do_QueryInterface(GetNSAccessible())); \
    if (!(accessIface)) \
        return (value)

#define MAI_IFACE_RETURN_IF_FAIL(accessIface) \
    nsCOMPtr<nsIAccessibleEditableText> \
        accessIface(do_QueryInterface(GetNSAccessible())); \
    if (!(accessIface)) \
        return

gboolean
MaiInterfaceEditableText::SetRunAttributes(AtkAttributeSet *aAttribSet,
                                           gint aStartOffset, gint aEndOffset)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, FALSE);

    nsCOMPtr<nsISupports> attrSet;
    /* how to insert attributes into nsISupports ??? */

    nsresult rv = accessIface->SetAttributes(aStartOffset, aEndOffset,
                                             attrSet);
    return NS_FAILED(rv) ? FALSE : TRUE;
}

void
MaiInterfaceEditableText::SetTextContents(const gchar *aString)
{
    MAI_IFACE_RETURN_IF_FAIL(accessIface);

    NS_ConvertUTF8toUCS2 strContent(aString);
    nsresult rv = accessIface->SetTextContents(strContent);

    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "MaiInterfaceEditableText::SetTextContents, failed\n");
}

void
MaiInterfaceEditableText::InsertText(const gchar *aString, gint aLength,
                                     gint *aPosition)
{
    MAI_IFACE_RETURN_IF_FAIL(accessIface);

    NS_ConvertUTF8toUCS2 strContent(aString);

    /////////////////////////////////////////////////////////////////////
    // interface changed in nsIAccessibleEditabelText.idl ???
    //
    // PRInt32 pos = *aPosition;
    // nsresult rv = accessIface->InsertText(strContent, aLength, &pos);
    // *aPosition = pos;
    /////////////////////////////////////////////////////////////////////

    nsresult rv = accessIface->InsertText(strContent, *aPosition);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "MaiInterfaceEditableText::InsertText, failed\n");
}

void
MaiInterfaceEditableText::CopyText(gint aStartPos, gint aEndPos)
{
    MAI_IFACE_RETURN_IF_FAIL(accessIface);

    nsresult rv = accessIface->CopyText(aStartPos, aEndPos);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "MaiInterfaceEditableText::CopyText, failed\n");
}

void
MaiInterfaceEditableText::CutText(gint aStartPos, gint aEndPos)
{
    MAI_IFACE_RETURN_IF_FAIL(accessIface);

    nsresult rv = accessIface->CutText(aStartPos, aEndPos);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "MaiInterfaceEditableText::CutText, failed\n");
}

void
MaiInterfaceEditableText::DeleteText(gint aStartPos, gint aEndPos)
{
    MAI_IFACE_RETURN_IF_FAIL(accessIface);

    nsresult rv = accessIface->DeleteText(aStartPos, aEndPos);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "MaiInterfaceEditableText::DeleteText, failed\n");
}

void
MaiInterfaceEditableText::PasteText(gint aPosition)
{
    MAI_IFACE_RETURN_IF_FAIL(accessIface);

    nsresult rv = accessIface->PasteText(aPosition);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "MaiInterfaceEditableText::PasteText, failed\n");
}

/* statics */

/* do general checking for callbacks functions
 * return the MaiInterfaceEditableText extracted from atk editabletext
 */
MaiInterfaceEditableText *
getEditableText(AtkEditableText *aEditable)
{
    g_return_val_if_fail(MAI_IS_ATK_WIDGET(aEditable), NULL);
    MaiWidget *maiWidget =
        NS_STATIC_CAST(MaiWidget*, (MAI_ATK_OBJECT(aEditable)->maiObject));
    g_return_val_if_fail(maiWidget != NULL, NULL);
    g_return_val_if_fail(maiWidget->GetAtkObject() == (AtkObject*)aEditable,
                         NULL);
    MaiInterfaceEditableText *ifaceEditableText =
        NS_STATIC_CAST(MaiInterfaceEditableText*,
                       maiWidget->GetMaiInterface(MAI_INTERFACE_EDITABLE_TEXT));
    return ifaceEditableText;
}

void
interfaceInitCB(AtkEditableTextIface *aIface)
{
    g_return_if_fail(aIface != NULL);

    aIface->set_run_attributes = setRunAttributesCB;
    aIface->set_text_contents = setTextContentsCB;
    aIface->insert_text = insertTextCB;
    aIface->copy_text = copyTextCB;
    aIface->cut_text = cutTextCB;
    aIface->delete_text = deleteTextCB;
    aIface->paste_text = pasteTextCB;
}

/* static, callbacks for atkeditabletext virutal functions */

gboolean
setRunAttributesCB(AtkEditableText *aText, AtkAttributeSet *aAttribSet,
                   gint aStartOffset, gint aEndOffset)

{
    MaiInterfaceEditableText *maiIfaceEditableText = getEditableText(aText);
    if (!maiIfaceEditableText)
        return FALSE;
    return maiIfaceEditableText->SetRunAttributes(aAttribSet,
                                                  aStartOffset, aEndOffset);
}

void
setTextContentsCB(AtkEditableText *aText, const gchar *aString)
{
    MaiInterfaceEditableText *maiIfaceEditableText = getEditableText(aText);
    if (!maiIfaceEditableText)
        return;
    maiIfaceEditableText->SetTextContents(aString);
}

void
insertTextCB(AtkEditableText *aText,
             const gchar *aString, gint aLength, gint *aPosition)
{
    MaiInterfaceEditableText *maiIfaceEditableText = getEditableText(aText);
    if (!maiIfaceEditableText)
        return;
    maiIfaceEditableText->InsertText(aString, aLength, aPosition);
}

void
copyTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos)
{
    MaiInterfaceEditableText *maiIfaceEditableText = getEditableText(aText);
    if (!maiIfaceEditableText)
        return;
    maiIfaceEditableText->CopyText(aStartPos, aEndPos);
}

void
cutTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos)
{
    MaiInterfaceEditableText *maiIfaceEditableText = getEditableText(aText);
    if (!maiIfaceEditableText)
        return;
    maiIfaceEditableText->CutText(aStartPos, aEndPos);
}

void
deleteTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos)
{
    MaiInterfaceEditableText *maiIfaceEditableText = getEditableText(aText);
    if (!maiIfaceEditableText)
        return;
    maiIfaceEditableText->DeleteText(aStartPos, aEndPos);
}

void
pasteTextCB(AtkEditableText *aText, gint aPosition)
{
    MaiInterfaceEditableText *maiIfaceEditableText = getEditableText(aText);
    if (!maiIfaceEditableText)
        return;
    maiIfaceEditableText->PasteText(aPosition);
}
