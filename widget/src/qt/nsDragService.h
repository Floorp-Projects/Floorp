/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsDragService_h__
#define nsDragService_h__

#include "nsBaseDragService.h"


/**
 * Native QT DragService wrapper
 */

class nsDragService : public nsBaseDragService
{

public:
    nsDragService();
    virtual ~nsDragService();

    //nsISupports
    NS_DECL_ISUPPORTS_INHERITED
  
  
    //nsIDragService
    NS_IMETHOD StartDragSession (nsITransferable * aTransferable, 
                                 PRUint32 aActionType);

#if 0
    // Native Impl.
    NS_IMETHOD GetData (nsITransferable * aTransferable, PRUint32 aItemIndex);

    static void SetTopLevelWidget(QWidget* w);

    static Qwidget  *sWidget;

protected:
    static PRBool gHaveDrag;

    static void DragLeave(QWidget      *widget,
                          QDragContext *context,
                          unsigned int time);

    static PRBool DragMotion(QWidget      *widget,
                             QDragContext *context,
                             int          x,
                             int          y,
                             unsigned int time);

    static PRBool DragDrop(QWidget      *widget,
                           QDragContext *context,
                           int          x,
                           int          y,
                           unsigned int time);

    static void DragDataReceived(QWidget        *widget,
                                 QDragContext   *context,
                                 int            x,
                                 int            y,
                                 QSelectionData *data,
                                 unsigned int   info,
                                 unsigned int   time);

    static void DragDataGet(QWidget          *widget,
                            QDragContext     *context,
                            QSelectionData   *selection_data,
                            unsigned int     info,
                            unsigned int     time,
                            void*            data);

    static void  DragDataDelete(QWidget          *widget,
                                QDragContext     *context,
                                void*            data);
#endif
};

#endif // nsDragService_h__
