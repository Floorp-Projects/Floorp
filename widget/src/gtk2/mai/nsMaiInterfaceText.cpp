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

#include "nsIAccessibleText.h"
#include "nsMaiInterfaceText.h"

/* helpers */
static MaiInterfaceText *getText(AtkText *aIface);

G_BEGIN_DECLS

static void interfaceInitCB(AtkTextIface *aIface);

/* text interface callbacks */
static gchar *getTextCB(AtkText *aText,
                        gint aStartOffset, gint aEndOffset);
static gchar *getTextAfterOffsetCB(AtkText *aText, gint aOffset,
                                   AtkTextBoundary aBoundaryType,
                                   gint *aStartOffset, gint *aEndOffset);
static gchar *getTextAtOffsetCB(AtkText *aText, gint aOffset,
                                AtkTextBoundary aBoundaryType,
                                gint *aStartOffset, gint *aEndOffset);
static gunichar getCharacterAtOffsetCB(AtkText *aText, gint aOffset);
static gchar *getTextBeforeOffsetCB(AtkText *aText, gint aOffset,
                                    AtkTextBoundary aBoundaryType,
                                    gint *aStartOffset, gint *aEndOffset);
static gint getCaretOffsetCB(AtkText *aText);
static AtkAttributeSet *getRunAttributesCB(AtkText *aText, gint aOffset,
                                           gint *aStartOffset,
                                           gint *aEndOffset);
static AtkAttributeSet* getDefaultAttributesCB(AtkText *aText);
static void getCharacterExtentsCB(AtkText *aText, gint aOffset,
                                  gint *aX, gint *aY,
                                  gint *aWidth, gint *aHeight,
                                  AtkCoordType aCoords);
static gint getCharacterCountCB(AtkText *aText);
static gint getOffsetAtPointCB(AtkText *aText,
                               gint aX, gint aY,
                               AtkCoordType aCoords);
static gint getSelectionCountCB(AtkText *aText);
static gchar *getSelectionCB(AtkText *aText, gint aSelectionNum,
                             gint *aStartOffset, gint *aEndOffset);

// set methods
static gboolean addSelectionCB(AtkText *aText,
                               gint aStartOffset,
                               gint aEndOffset);
static gboolean removeSelectionCB(AtkText *aText,
                                  gint aSelectionNum);
static gboolean setSelectionCB(AtkText *aText, gint aSelectionNum,
                               gint aStartOffset, gint aEndOffset);
static gboolean setCaretOffsetCB(AtkText *aText, gint aOffset);

/*************************************************
 // signal handlers
 //
    static void TextChangedCB(AtkText *aText, gint aPosition, gint aLength);
    static void TextCaretMovedCB(AtkText *aText, gint aLocation);
    static void TextSelectionChangedCB(AtkText *aText);
*/
G_END_DECLS

MaiInterfaceText::MaiInterfaceText(MaiWidget *aMaiWidget):
    MaiInterface(aMaiWidget)
{
}

MaiInterfaceText::~MaiInterfaceText()
{
}

MaiInterfaceType
MaiInterfaceText::GetType()
{
    return MAI_INTERFACE_TEXT;
}

const GInterfaceInfo *
MaiInterfaceText::GetInterfaceInfo()
{
    static const GInterfaceInfo atk_if_text_info = {
        (GInterfaceInitFunc)interfaceInitCB,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };
    return &atk_if_text_info;
}

#define MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, value) \
    nsCOMPtr<nsIAccessibleText> \
        accessIface(do_QueryInterface(GetNSAccessible())); \
    if (!(accessIface)) \
        return (value)

#define MAI_IFACE_RETURN_IF_FAIL(accessIface) \
    nsCOMPtr<nsIAccessibleText> \
        accessIface(do_QueryInterface(GetNSAccessible())); \
    if (!(accessIface)) \
        return

const gchar *
MaiInterfaceText::GetText(gint aStartOffset, gint aEndOffset)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    nsAutoString autoStr;
    nsresult rv = accessIface->GetText(aStartOffset, aEndOffset, autoStr);
    if (NS_FAILED(rv))
        return NULL;
    mText = NS_ConvertUCS2toUTF8(autoStr);
    return mText.get();
}

const gchar *
MaiInterfaceText::GetTextAfterOffset(gint aOffset,
                                     AtkTextBoundary aBoundaryType,
                                     gint *aStartOffset, gint *aEndOffset)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    nsAutoString autoStr;
    PRInt32 startOffset = 0, endOffset = 0;
    nsresult rv =
        accessIface->GetTextAfterOffset(aOffset, aBoundaryType,
                                        &startOffset, &endOffset, autoStr);
    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

    if (NS_FAILED(rv))
        return NULL;
    mText = NS_ConvertUCS2toUTF8(autoStr);
    return mText.get();
}

const gchar *
MaiInterfaceText::GetTextAtOffset(gint aOffset,
                                  AtkTextBoundary aBoundaryType,
                                  gint *aStartOffset, gint *aEndOffset)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    nsAutoString autoStr;
    PRInt32 startOffset = 0, endOffset = 0;
    nsresult rv =
        accessIface->GetTextAtOffset(aOffset, aBoundaryType,
                                     &startOffset, &endOffset, autoStr);
    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

    if (NS_FAILED(rv))
        return NULL;
    mText = NS_ConvertUCS2toUTF8(autoStr);
    return mText.get();
}

gunichar
MaiInterfaceText::GetCharacterAtOffset(gint aOffset)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, gunichar(0));

    /* PRUnichar is unsigned short in Mozilla */
    /* gnuichar is guint32 in glib */
    PRUnichar uniChar;
    nsresult rv =
        accessIface->GetCharacterAtOffset(aOffset, &uniChar);
    return (NS_FAILED(rv)) ? 0 : NS_STATIC_CAST(gunichar, uniChar);
}

const gchar *
MaiInterfaceText::GetTextBeforeOffset(gint aOffset,
                                      AtkTextBoundary aBoundaryType,
                                      gint *aStartOffset, gint *aEndOffset)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    nsAutoString autoStr;
    PRInt32 startOffset = 0, endOffset = 0;
    nsresult rv =
        accessIface->GetTextBeforeOffset(aOffset, aBoundaryType,
                                         &startOffset, &endOffset, autoStr);
    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

    if (NS_FAILED(rv))
        return NULL;
    mText = NS_ConvertUCS2toUTF8(autoStr);
    return mText.get();
}

gint
MaiInterfaceText::GetCaretOffset(void)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, 0);

    PRInt32 offset;
    nsresult rv = accessIface->GetCaretOffset(&offset);
    return (NS_FAILED(rv)) ? 0 : NS_STATIC_CAST(gint, offset);
}

AtkAttributeSet *
MaiInterfaceText::GetRunAttributes(gint aOffset,
                                   gint *aStartOffset,
                                   gint *aEndOffset)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    nsCOMPtr<nsISupports> attrSet;
    PRInt32 startOffset = 0, endOffset = 0;
    nsresult rv = accessIface->GetAttributeRange(aOffset,
                                                 &startOffset, &endOffset,
                                                 getter_AddRefs(attrSet));
    *aStartOffset = startOffset;
    *aEndOffset = endOffset;
    if (NS_FAILED(rv))
        return NULL;

    /* what to do with the nsISupports ? ??? */
    return NULL;
}

AtkAttributeSet *
MaiInterfaceText::GetDefaultAttributes(void)
{
    /* not supported ??? */
    return NULL;
}

void
MaiInterfaceText::GetCharacterExtents(gint aOffset,
                                      gint *aX, gint *aY,
                                      gint *aWidth, gint *aHeight,
                                      AtkCoordType aCoords)
{
    MAI_IFACE_RETURN_IF_FAIL(accessIface);

    PRInt32 extY = 0, extX = 0;
    PRInt32 extWidth = 0, extHeight = 0;
    nsresult rv = accessIface->GetCharacterExtents(aOffset, &extX, &extY,
                                                   &extWidth, &extHeight,
                                                   aCoords);
    *aX = extX;
    *aY = extY;
    *aWidth = extWidth;
    *aHeight = extHeight;
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "MaiInterfaceText::GetCharacterExtents, failed\n");
}

gint
MaiInterfaceText::GetCharacterCount(void)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, 0);

    PRInt32 count = 0;
    nsresult rv = accessIface->GetCharacterCount(&count);
    return (NS_FAILED(rv)) ? 0 : NS_STATIC_CAST(gint, count);
}

gint
MaiInterfaceText::GetOffsetAtPoint(gint aX, gint aY,
                                   AtkCoordType aCoords)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, 0);

    PRInt32 offset = 0;
    nsresult rv = accessIface->GetOffsetAtPoint(aX, aY, aCoords, &offset);
    return (NS_FAILED(rv)) ? 0 : NS_STATIC_CAST(gint, offset);
}

gint
MaiInterfaceText::GetSelectionCount(void)
{
    /* no implemetation in nsIAccessibleText??? */

    //new attribuate will be added in nsIAccessibleText.idl
    //readonly attribute long selectionCount;

    return 0;
}

const gchar *
MaiInterfaceText::GetSelection(gint aSelectionNum,
                               gint *aStartOffset, gint *aEndOffset)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    PRInt32 startOffset = 0, endOffset = 0;
    nsresult rv = accessIface->GetSelectionBounds(aSelectionNum,
                                                  &startOffset, &endOffset);

    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

    if (NS_FAILED(rv))
        return NULL;
    return GetText(*aStartOffset, *aEndOffset);
}


// set methods
gboolean
MaiInterfaceText::AddSelection(gint aStartOffset, gint aEndOffset)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, FALSE);

    nsresult rv = accessIface->AddSelection(aStartOffset, aEndOffset);

    return NS_SUCCEEDED(rv);
}

gboolean
MaiInterfaceText::RemoveSelection(gint aSelectionNum)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, FALSE);

    nsresult rv = accessIface->RemoveSelection(aSelectionNum);

    return NS_SUCCEEDED(rv);
}

gboolean
MaiInterfaceText::SetSelection(gint aSelectionNum,
                               gint aStartOffset, gint aEndOffset)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, FALSE);

    nsresult rv = accessIface->SetSelectionBounds(aSelectionNum,
                                                  aStartOffset, aEndOffset);
    return NS_SUCCEEDED(rv);
}

gboolean
MaiInterfaceText::SetCaretOffset(gint aOffset)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, FALSE);

    nsresult rv = accessIface->SetCaretOffset(aOffset);
    return NS_SUCCEEDED(rv);
}

/* statics */

/* do general checking for callbacks functions
 * return the MaiInterfaceText extracted from atk text
 */
MaiInterfaceText *
getText(AtkText *aText)
{
    g_return_val_if_fail(MAI_IS_ATK_WIDGET(aText), NULL);
    MaiWidget *maiWidget =
        NS_STATIC_CAST(MaiWidget*, (MAI_ATK_OBJECT(aText)->maiObject));
    g_return_val_if_fail(maiWidget != NULL, NULL);
    g_return_val_if_fail(maiWidget->GetAtkObject() == (AtkObject*)aText,
                         NULL);
    MaiInterfaceText *maiInterfaceText =
        NS_STATIC_CAST(MaiInterfaceText*,
                       (maiWidget->GetMaiInterface(MAI_INTERFACE_TEXT)));
    return maiInterfaceText;
}

void
interfaceInitCB(AtkTextIface *aIface)
{
    g_return_if_fail(aIface != NULL);

    aIface->get_text = getTextCB;
    aIface->get_text_after_offset = getTextAfterOffsetCB;
    aIface->get_text_at_offset = getTextAtOffsetCB;
    aIface->get_character_at_offset = getCharacterAtOffsetCB;
    aIface->get_text_before_offset = getTextBeforeOffsetCB;
    aIface->get_caret_offset = getCaretOffsetCB;
    aIface->get_run_attributes = getRunAttributesCB;
    aIface->get_default_attributes = getDefaultAttributesCB;
    aIface->get_character_extents = getCharacterExtentsCB;
    aIface->get_character_count = getCharacterCountCB;
    aIface->get_offset_at_point = getOffsetAtPointCB;
    aIface->get_n_selections = getSelectionCountCB;
    aIface->get_selection = getSelectionCB;

    // set methods
    aIface->add_selection = addSelectionCB;
    aIface->remove_selection = removeSelectionCB;
    aIface->set_selection = setSelectionCB;
    aIface->set_caret_offset = setCaretOffsetCB;
}

gchar *
getTextCB(AtkText *aText, gint aStartOffset, gint aEndOffset)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return NULL;
    return NS_CONST_CAST(gchar*,
                         maiIfaceText->GetText(aStartOffset, aEndOffset));
}

gchar *
getTextAfterOffsetCB(AtkText *aText, gint aOffset,
                     AtkTextBoundary aBoundaryType,
                     gint *aStartOffset, gint *aEndOffset)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return NULL;
    return NS_CONST_CAST(gchar*,
                         maiIfaceText->GetTextAfterOffset(aOffset,
                                                          aBoundaryType,
                                                          aStartOffset,
                                                          aEndOffset));
}

gchar *
getTextAtOffsetCB(AtkText *aText, gint aOffset,
                  AtkTextBoundary aBoundaryType,
                  gint *aStartOffset, gint *aEndOffset)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return NULL;
    return NS_CONST_CAST(gchar*,
                         maiIfaceText->GetTextAtOffset(aOffset,
                                                       aBoundaryType,
                                                       aStartOffset,
                                                       aEndOffset));
}

gunichar
getCharacterAtOffsetCB(AtkText *aText, gint aOffset)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return 0;
    return maiIfaceText->GetCharacterAtOffset(aOffset);
}

gchar *
getTextBeforeOffsetCB(AtkText *aText, gint aOffset,
                      AtkTextBoundary aBoundaryType,
                      gint *aStartOffset, gint *aEndOffset)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return NULL;
    return NS_CONST_CAST(gchar*,
                         maiIfaceText->GetTextBeforeOffset(aOffset,
                                                           aBoundaryType,
                                                           aStartOffset,
                                                           aEndOffset));
}

gint
getCaretOffsetCB(AtkText *aText)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return 0;
    return maiIfaceText->GetCaretOffset();
}

AtkAttributeSet *
getRunAttributesCB(AtkText *aText, gint aOffset,
                   gint *aStartOffset,
                   gint *aEndOffset)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return NULL;
    return maiIfaceText->GetRunAttributes(aOffset, aStartOffset, aEndOffset);
}

AtkAttributeSet *
getDefaultAttributesCB(AtkText *aText)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return NULL;
    return maiIfaceText->GetDefaultAttributes();
}

void
getCharacterExtentsCB(AtkText *aText, gint aOffset,
                      gint *aX, gint *aY,
                      gint *aWidth, gint *aHeight,
                      AtkCoordType aCoords)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return;
    maiIfaceText->GetCharacterExtents(aOffset, aX, aY,
                                      aWidth, aHeight, aCoords);
}

gint
getCharacterCountCB(AtkText *aText)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return 0;
    return maiIfaceText->GetCharacterCount();
}

gint
getOffsetAtPointCB(AtkText *aText,
                   gint aX, gint aY,
                   AtkCoordType coords)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return 0;
    return maiIfaceText->GetOffsetAtPoint(aX, aY, coords);
}

gint
getSelectionCountCB(AtkText *aText)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return 0;
    return maiIfaceText->GetSelectionCount();
}

gchar *
getSelectionCB(AtkText *aText, gint aSelectionNum,
               gint *aStartOffset, gint *aEndOffset)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return NULL;
    return NS_CONST_CAST(gchar*,
                         maiIfaceText->GetSelection(aSelectionNum,
                                                    aStartOffset,
                                                    aEndOffset));
}

// set methods
gboolean
addSelectionCB(AtkText *aText,
               gint aStartOffset,
               gint aEndOffset)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return FALSE;
    return maiIfaceText->AddSelection(aStartOffset, aEndOffset);
}

gboolean
removeSelectionCB(AtkText *aText,
                  gint aSelectionNum)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return FALSE;
    return maiIfaceText->RemoveSelection(aSelectionNum);
}

gboolean
setSelectionCB(AtkText *aText, gint aSelectionNum,
               gint aStartOffset, gint aEndOffset)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return FALSE;
    return maiIfaceText->SetSelection(aSelectionNum, aStartOffset, aEndOffset);
}

gboolean
setCaretOffsetCB(AtkText *aText, gint aOffset)
{
    MaiInterfaceText *maiIfaceText = getText(aText);
    if (!maiIfaceText)
        return FALSE;
    return maiIfaceText->SetCaretOffset(aOffset);
}
