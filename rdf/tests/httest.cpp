/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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

#include <iostream>
#include "rdf.h"
#include "nsRDFCID.h"
#include "nsINetService.h"

#include "nsITreeDMItem.h"
#include "nsITreeDataModel.h"
#include "nsITreeColumn.h"

#include "nsIToolbarDataModel.h"

#include "nsRepository.h"

#include "plstr.h"
#include "plevent.h"

// hackery to get stuff up and running
#include "nsRDFTreeDataModel.h"
#include "rdf-int.h"

#if defined(XP_PC)
#define NETLIB_DLL "netlib.dll"
#define RDF_DLL    "rdf.dll"
#elif defined(XP_UNIX)
#define NETLIB_DLL "libnetlib.so"
#define RDF_DLL    "librdf.so"
#elif defined(XP_MAC)
#else
#error "unknown platform"
#endif

static NS_DEFINE_IID(kNetServiceCID,           NS_NETSERVICE_CID);
static NS_DEFINE_IID(kCRDFTreeDataModelCID,    NS_RDFTREEDATAMODEL_CID);
static NS_DEFINE_IID(kCRDFToolbarDataModelCID, NS_RDFTOOLBARDATAMODEL_CID);
static NS_DEFINE_IID(kITreeDataModelIID,       NS_ITREEDATAMODEL_IID);
static NS_DEFINE_IID(kITreeDMItemIID,          NS_ITREEDMITEM_IID);


int
main(int argc, char** argv)
{
    PL_InitializeEventsLib("");
    nsRepository::RegisterFactory(kNetServiceCID, NETLIB_DLL, PR_FALSE, PR_FALSE);

    nsRepository::RegisterFactory(kCRDFTreeDataModelCID, RDF_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCRDFToolbarDataModelCID, RDF_DLL, PR_FALSE, PR_FALSE);

    nsITreeDataModel* tree;
    nsresult res =
        nsRepository::CreateInstance(kCRDFTreeDataModelCID,
                                     NULL,
                                     kITreeDataModelIID,
                                     (void**) &tree);

    if (NS_SUCCEEDED(res)) {
        tree->InitFromURL(nsString("rdf:bookmarks"));

        PRUint32 numColumns;
        tree->GetColumnCount(numColumns);

        nsITreeColumn** columns = new nsITreeColumn*[numColumns];
        {
            for (int i = 0; i < numColumns; ++i)
                tree->GetNthColumn(columns[i], i);
        }

        {
            PRUint32 size;
            tree->GetItemCount(size);

            nsIDMItem* item;
            for (int i = 0; i < size; ++i) {
                if (NS_SUCCEEDED(tree->GetNthItem(item, i))) {
                    nsITreeDMItem* treeItem;
                    if (NS_SUCCEEDED(item->QueryInterface(kITreeDMItemIID, (void**) &treeItem))) {
                        std::cout << "Item " << i << ": ";

                        for (int j = 0; j < numColumns; ++j) {
                            nsString columnName;
                            columns[j]->GetColumnName(columnName);

                            nsString valueText;
                            tree->GetItemTextForColumn(valueText, treeItem, columns[j]);

                            char buf[256];
                            std::cout << columnName.ToCString(buf, sizeof buf);
                            std::cout << "=\"" << valueText.ToCString(buf, sizeof buf) << "\" ";
                        }

                        std::cout << std::endl;
                        treeItem->Release();
                    }
                    item->Release();
                }
            }
        }

        {
            for (int i = 0; i < numColumns; ++i)
                columns[i]->Release();
        }

        delete[] columns;
        tree->Release();
    }

    return 0;
}
