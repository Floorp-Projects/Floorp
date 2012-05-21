/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDragService_h__
#define nsDragService_h__

#include <qdrag.h>

#include "nsBaseDragService.h"

/* Header file */
class nsDragService : public nsBaseDragService
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDRAGSERVICE

    nsDragService();

private:
    ~nsDragService();

protected:
    /* additional members */
    NS_IMETHODIMP SetupDragSession(nsISupportsArray *aTransferables, PRUint32 aActionType);
    NS_IMETHODIMP SetDropActionType(PRUint32 aActionType);
    NS_IMETHODIMP ExecuteDrag();

    QDrag *mDrag;
    Qt::DropActions mDropAction;
    QWidget *mHiddenWidget;
};

#endif // nsDragService_h__
