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

#include "nsIAccessibleTable.h"
#include "nsMaiInterfaceTable.h"

/* helpers */
static MaiInterfaceTable *getTable (AtkTable *aIface);

G_BEGIN_DECLS

/* table interface callbacks */
static void interfaceInitCB(AtkTableIface *aIface);
static AtkObject* refAtCB(AtkTable *aTable, gint aRow, gint aColumn);
static gint getIndexAtCB(AtkTable *aTable, gint aRow, gint aColumn);
static gint getColumnAtIndexCB(AtkTable *aTable, gint aIndex);
static gint getRowAtIndexCB(AtkTable *aTable, gint aIndex);
static gint getColumnCountCB(AtkTable *aTable);
static gint getRowCountCB(AtkTable *aTable);
static gint getColumnExtentAtCB(AtkTable *aTable, gint aRow, gint aColumn);
static gint getRowExtentAtCB(AtkTable *aTable, gint aRow, gint aColumn);
static AtkObject* getCaptionCB(AtkTable *aTable);
static const gchar* getColumnDescriptionCB(AtkTable *aTable, gint aColumn);
static AtkObject* getColumnHeaderCB(AtkTable *aTable, gint aColumn);
static const gchar* getRowDescriptionCB(AtkTable *aTable, gint aRow);
static AtkObject* getRowHeaderCB(AtkTable *aTable, gint aRow);
static AtkObject* getSummaryCB(AtkTable *aTable);
static gint getSelectedColumnsCB(AtkTable *aTable, gint **aSelected);
static gint getSelectedRowsCB(AtkTable *aTable, gint **aSelected);
static gboolean isColumnSelectedCB(AtkTable *aTable, gint aColumn);
static gboolean isRowSelectedCB(AtkTable *aTable, gint aRow);
static gboolean isCellSelectedCB(AtkTable *aTable, gint aRow, gint aColumn);

/* what are missing now for atk table */

/* ==================================================
   void              (* set_caption)              (AtkTable      *aTable,
   AtkObject     *caption);
   void              (* set_column_description)   (AtkTable      *aTable,
   gint          aColumn,
   const gchar   *description);
   void              (* set_column_header)        (AtkTable      *aTable,
   gint          aColumn,
   AtkObject     *header);
   void              (* set_row_description)      (AtkTable      *aTable,
   gint          aRow,
   const gchar   *description);
   void              (* set_row_header)           (AtkTable      *aTable,
   gint          aRow,
   AtkObject     *header);
   void              (* set_summary)              (AtkTable      *aTable,
   AtkObject     *accessible);
   gboolean          (* add_row_selection)        (AtkTable      *aTable,
   gint          aRow);
   gboolean          (* remove_row_selection)     (AtkTable      *aTable,
   gint          aRow);
   gboolean          (* add_column_selection)     (AtkTable      *aTable,
   gint          aColumn);
   gboolean          (* remove_column_selection)  (AtkTable      *aTable,
   gint          aColumn);

   ////////////////////////////////////////
   // signal handlers
   //
   void              (* row_inserted)           (AtkTable      *aTable,
   gint          aRow,
   gint          num_inserted);
   void              (* column_inserted)        (AtkTable      *aTable,
   gint          aColumn,
   gint          num_inserted);
   void              (* row_deleted)              (AtkTable      *aTable,
   gint          aRow,
   gint          num_deleted);
   void              (* column_deleted)           (AtkTable      *aTable,
   gint          aColumn,
   gint          num_deleted);
   void              (* row_reordered)            (AtkTable      *aTable);
   void              (* column_reordered)         (AtkTable      *aTable);
   void              (* model_changed)            (AtkTable      *aTable);

   * ==================================================
   */
G_END_DECLS

MaiInterfaceTable::MaiInterfaceTable(MaiWidget *aMaiWidget):
    MaiInterface(aMaiWidget)
{
}

MaiInterfaceTable::~MaiInterfaceTable()
{
}

MaiInterfaceType
MaiInterfaceTable::GetType()
{
    return MAI_INTERFACE_TABLE;
}

const GInterfaceInfo *
MaiInterfaceTable::GetInterfaceInfo()
{
    static const GInterfaceInfo atk_if_table_info = {
        (GInterfaceInitFunc)interfaceInitCB,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };
    return &atk_if_table_info;
}

#define MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, value) \
    nsCOMPtr<nsIAccessibleTable> \
        accessIface(do_QueryInterface(GetNSAccessible())); \
    if (!(accessIface)) \
        return (value)

#define MAI_IFACE_RETURN_IF_FAIL(accessIface) \
    nsCOMPtr<nsIAccessibleTable> \
        accessIface(do_QueryInterface(GetNSAccessible())); \
    if (!(accessIface)) \
        return

MaiWidget *
MaiInterfaceTable::RefAt(gint aRow, gint aColumn)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    nsCOMPtr<nsIAccessible> cell;
    nsresult rv = accessIface->CellRefAt(aRow, aColumn,getter_AddRefs(cell));
    if (NS_FAILED(rv) || !cell)
        return NULL;

    return MaiWidget::CreateAndCache(cell);
}

gint
MaiInterfaceTable::GetIndexAt(gint aRow, gint aColumn)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, -1);

    PRInt32 index;
    nsresult rv = accessIface->GetIndexAt(aRow, aColumn, &index);
    if (NS_FAILED(rv))
        return -1;
    return NS_STATIC_CAST(gint, index);
}

gint
MaiInterfaceTable::GetColumnAtIndex(gint aIndex)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, -1);

    PRInt32 col;
    nsresult rv = accessIface->GetColumnAtIndex(aIndex, &col);
    if (NS_FAILED(rv))
        return -1;
    return NS_STATIC_CAST(gint, col);
}

gint
MaiInterfaceTable::GetRowAtIndex(gint aIndex)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, -1);

    PRInt32 row;
    nsresult rv = accessIface->GetRowAtIndex(aIndex, &row);
    if (NS_FAILED(rv))
        return -1;
    return NS_STATIC_CAST(gint, row);
}

gint
MaiInterfaceTable::GetColumnCount()
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, -1);

    PRInt32 count;
    nsresult rv = accessIface->GetColumns(&count);
    if (NS_FAILED(rv))
        return -1;
    return NS_STATIC_CAST(gint, count);
}

gint
MaiInterfaceTable::GetRowCount()
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, -1);

    PRInt32 count;
    nsresult rv = accessIface->GetRows(&count);
    if (NS_FAILED(rv))
        return -1;
    return NS_STATIC_CAST(gint, count);
}

gint
MaiInterfaceTable::GetColumnExtentAt(gint aRow, gint aColumn)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, -1);

    PRInt32 extent;
    nsresult rv = accessIface->GetColumnExtentAt(aRow, aColumn, &extent);
    if (NS_FAILED(rv))
        return -1;
    return NS_STATIC_CAST(gint, extent);
}

gint
MaiInterfaceTable::GetRowExtentAt(gint aRow, gint aColumn)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, -1);

    PRInt32 extent;
    nsresult rv = accessIface->GetRowExtentAt(aRow, aColumn, &extent);
    if (NS_FAILED(rv))
        return -1;
    return NS_STATIC_CAST(gint, extent);
}

MaiWidget*
MaiInterfaceTable::GetCaption()
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    nsCOMPtr<nsIAccessible> caption;
    nsresult rv = accessIface->GetCaption(getter_AddRefs(caption));
    if (NS_FAILED(rv) || !caption)
        return NULL;

    return MaiWidget::CreateAndCache(caption);
}

const gchar*
MaiInterfaceTable::GetColumnDescription(gint aColumn)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    if (!mColumnDescription.IsEmpty())
        return mColumnDescription.get();

    nsAutoString autoStr;
    nsresult rv = accessIface->GetColumnDescription(aColumn, autoStr);
    if (NS_FAILED(rv))
        return NULL;
    mColumnDescription = NS_ConvertUCS2toUTF8(autoStr);
    return mColumnDescription.get();
}

MaiWidget*
MaiInterfaceTable::GetColumnHeader(gint aColumn)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    nsCOMPtr<nsIAccessibleTable> header;
    nsresult rv = accessIface->GetColumnHeader(getter_AddRefs(header));
    if (NS_FAILED(rv))
        return NULL;

    nsCOMPtr<nsIAccessible> accHeader(do_QueryInterface(header));
    if (!accHeader)
        return NULL;

    return MaiWidget::CreateAndCache(accHeader);
}

const gchar*
MaiInterfaceTable::GetRowDescription(gint aRow)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    if (!mRowDescription.IsEmpty())
        return mRowDescription.get();

    nsAutoString autoStr;
    nsresult rv = accessIface->GetRowDescription(aRow, autoStr);
    if (NS_FAILED(rv))
        return NULL;
    mRowDescription = NS_ConvertUCS2toUTF8(autoStr);
    return mRowDescription.get();
}

MaiWidget*
MaiInterfaceTable::GetRowHeader(gint aRow)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    nsCOMPtr<nsIAccessibleTable> header;
    nsresult rv = accessIface->GetRowHeader(getter_AddRefs(header));
    if (NS_FAILED(rv) || !header)
        return NULL;

    nsCOMPtr<nsIAccessible> accHeader(do_QueryInterface(header));
    if (!accHeader)
        return NULL;

    return MaiWidget::CreateAndCache(accHeader);
}

MaiWidget*
MaiInterfaceTable::GetSummary()
{
    /* ??? in nsIAccessibleTable, it returns a nsAString */
    return NULL;
}

gint
MaiInterfaceTable::GetSelectedColumns(gint **aSelected)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    PRUint32 size = 0;
    PRInt32 *columns = NULL;
    nsresult rv = accessIface->GetSelectedColumns(&size, &columns);
    if (NS_FAILED(rv) || (size == 0) || !columns) {
        *aSelected = NULL;
        return 0;
    }

    gint *atkColumns = g_new(gint, size);
    NS_ASSERTION(atkColumns, "Fail to get memory for columns");

    //copy
    for (int index = 0; index < size; ++index)
        atkColumns[index] = NS_STATIC_CAST(gint, columns[index]);
    nsMemory::Free(columns);

    *aSelected = atkColumns;
    return size;
}

gint
MaiInterfaceTable::GetSelectedRows(gint **aSelected)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, NULL);

    PRUint32 size = 0;
    PRInt32 *rows = NULL;
    nsresult rv = accessIface->GetSelectedRows(&size, &rows);
    if (NS_FAILED(rv) || (size == 0) || !rows) {
        *aSelected = NULL;
        return 0;
    }

    gint *atkRows = g_new(gint, size);
    NS_ASSERTION(atkRows, "Fail to get memory for rows");

    //copy
    for (int index = 0; index < size; ++index)
        atkRows[index] = NS_STATIC_CAST(gint, rows[index]);
    nsMemory::Free(rows);

    *aSelected = atkRows;
    return size;
}

gboolean
MaiInterfaceTable::IsColumnSelected(gint aColumn)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, FALSE);

    PRBool outValue;
    nsresult rv = accessIface->IsColumnSelected(aColumn, &outValue);
    return NS_FAILED(rv) ? FALSE : NS_STATIC_CAST(gboolean, outValue);
}

gboolean
MaiInterfaceTable::IsRowSelected(gint aRow)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, FALSE);

    PRBool outValue;
    nsresult rv = accessIface->IsRowSelected(aRow, &outValue);
    return NS_FAILED(rv) ? FALSE : NS_STATIC_CAST(gboolean, outValue);
}

gboolean
MaiInterfaceTable::IsCellSelected(gint aRow, gint aColumn)
{
    MAI_IFACE_RETURN_VAL_IF_FAIL(accessIface, FALSE);

    PRBool outValue;
    nsresult rv = accessIface->IsCellSelected(aRow, aColumn, &outValue);
    return NS_FAILED(rv) ? FALSE : NS_STATIC_CAST(gboolean, outValue);
}

/* static functions */

/* do general checking for callbacks functions
 * return the MaiInterfaceTable extracted from atk table
 */
MaiInterfaceTable *
getTable(AtkTable *aTable)
{
    g_return_val_if_fail(MAI_IS_ATK_WIDGET(aTable), NULL);
    MaiWidget *maiWidget =
        NS_STATIC_CAST(MaiWidget*, (MAI_ATK_OBJECT(aTable)->maiObject));
    g_return_val_if_fail(maiWidget != NULL, NULL);
    g_return_val_if_fail(maiWidget->GetAtkObject() == (AtkObject*)aTable,
                         NULL);
    MaiInterfaceTable *maiInterfaceTable =
        NS_STATIC_CAST(MaiInterfaceTable*,
                       maiWidget->GetMaiInterface(MAI_INTERFACE_TABLE));
    return maiInterfaceTable;
}

void
interfaceInitCB(AtkTableIface *aIface)

{
    g_return_if_fail(aIface != NULL);

    aIface->ref_at = refAtCB;
    aIface->get_index_at = getIndexAtCB;
    aIface->get_column_at_index = getColumnAtIndexCB;
    aIface->get_row_at_index = getRowAtIndexCB;
    aIface->get_n_columns = getColumnCountCB;
    aIface->get_n_rows = getRowCountCB;
    aIface->get_column_extent_at = getColumnExtentAtCB;
    aIface->get_row_extent_at = getRowExtentAtCB;
    aIface->get_caption = getCaptionCB;
    aIface->get_column_description = getColumnDescriptionCB;
    aIface->get_column_header = getColumnHeaderCB;
    aIface->get_row_description = getRowDescriptionCB;
    aIface->get_row_header = getRowHeaderCB;
    aIface->get_summary = getSummaryCB;
    aIface->get_selected_columns = getSelectedColumnsCB;
    aIface->get_selected_rows = getSelectedRowsCB;
    aIface->is_column_selected = isColumnSelectedCB;
    aIface->is_row_selected = isRowSelectedCB;
    aIface->is_selected = isCellSelectedCB;
}

/* static */
AtkObject*
refAtCB(AtkTable *aTable, gint aRow, gint aColumn)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return NULL;
    MaiWidget *maiWidget = maiIfaceTable->RefAt(aRow, aColumn);
    if (!maiWidget)
        return NULL;
    AtkObject *atkObj = maiWidget->GetAtkObject();
    if (!atkObj)
        return NULL;
    g_object_ref(atkObj);
    return atkObj;
}

gint
getIndexAtCB(AtkTable *aTable, gint aRow, gint aColumn)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return -1;
    return maiIfaceTable->GetIndexAt(aRow, aColumn);
}

gint
getColumnAtIndexCB(AtkTable *aTable, gint aIndex)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return -1;
    return maiIfaceTable->GetColumnAtIndex(aIndex);
}

gint
getRowAtIndexCB(AtkTable *aTable, gint aIndex)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return -1;
    return maiIfaceTable->GetRowAtIndex(aIndex);
}

gint
getColumnCountCB(AtkTable *aTable)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return -1;
    return maiIfaceTable->GetColumnCount();
}

gint
getRowCountCB(AtkTable *aTable)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return -1;
    return maiIfaceTable->GetRowCount();
}

gint
getColumnExtentAtCB(AtkTable *aTable,
                    gint aRow, gint aColumn)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return -1;
    return maiIfaceTable->GetColumnExtentAt(aRow, aColumn);
}

gint
getRowExtentAtCB(AtkTable *aTable,
                 gint aRow, gint aColumn)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return -1;
    return maiIfaceTable->GetRowExtentAt(aRow, aColumn);
}

AtkObject*
getCaptionCB(AtkTable *aTable)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return NULL;
    MaiWidget *maiWidget = maiIfaceTable->GetCaption();
    if (!maiWidget)
        return NULL;
    return maiWidget->GetAtkObject();
}

const gchar*
getColumnDescriptionCB(AtkTable *aTable, gint aColumn)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return NULL;
    return maiIfaceTable->GetColumnDescription(aColumn);
}

AtkObject*
getColumnHeaderCB(AtkTable *aTable, gint aColumn)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return NULL;
    MaiWidget *maiWidget = maiIfaceTable->GetColumnHeader(aColumn);
    if (!maiWidget)
        return NULL;
    return maiWidget->GetAtkObject();
}

const gchar*
getRowDescriptionCB(AtkTable *aTable, gint aRow)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return NULL;
    return maiIfaceTable->GetRowDescription(aRow);
}

AtkObject*
getRowHeaderCB(AtkTable *aTable, gint aRow)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return NULL;
    MaiWidget *maiWidget = maiIfaceTable->GetRowHeader(aRow);
    if (!maiWidget)
        return NULL;
    return maiWidget->GetAtkObject();
}

AtkObject*
getSummaryCB(AtkTable *aTable)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return NULL;
    MaiWidget *maiWidget = maiIfaceTable->GetSummary();
    if (!maiWidget)
        return NULL;
    return maiWidget->GetAtkObject();
}

gint
getSelectedColumnsCB(AtkTable *aTable, gint **aSelected)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return 0;
    return maiIfaceTable->GetSelectedColumns(aSelected);
}

gint
getSelectedRowsCB(AtkTable *aTable, gint **aSelected)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return 0;
    return maiIfaceTable->GetSelectedRows(aSelected);
}

gboolean
isColumnSelectedCB(AtkTable *aTable, gint aColumn)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return FALSE;
    return maiIfaceTable->IsColumnSelected(aColumn);
}

gboolean
isRowSelectedCB(AtkTable *aTable, gint aRow)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return FALSE;
    return maiIfaceTable->IsRowSelected(aRow);
}

gboolean
isCellSelectedCB(AtkTable *aTable, gint aRow, gint aColumn)
{
    MaiInterfaceTable *maiIfaceTable = getTable(aTable);
    if (!maiIfaceTable)
        return FALSE;
    return maiIfaceTable->IsCellSelected(aRow, aColumn);
}
