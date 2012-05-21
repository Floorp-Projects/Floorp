/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZQWIDGETFAST_H
#define MOZQWIDGETFAST_H

#include <QtCore/QObject>
#include "moziqwidget.h"

class MozQWidgetFast : public IMozQWidget
{
public:
    MozQWidgetFast(nsWindow* aReceiver, QGraphicsItem *aParent);
    ~MozQWidgetFast() {}

protected:
    virtual void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*);

private:
    QPixmap mToolbar;
    QPixmap mIcon;
    QString mUrl;
};

#endif
