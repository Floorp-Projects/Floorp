/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozqglwidgetwrapper.h"
#include <QGraphicsView>
#include <QtOpenGL/QGLWidget>
#include <QtOpenGL/QGLContext>

MozQGLWidgetWrapper::MozQGLWidgetWrapper()
  : mWidget(new QGLWidget())
{
}

MozQGLWidgetWrapper::~MozQGLWidgetWrapper()
{
    delete mWidget;
}

void MozQGLWidgetWrapper::makeCurrent()
{
    mWidget->makeCurrent();
}

void MozQGLWidgetWrapper::setViewport(QGraphicsView* aView)
{
    aView->setViewport(mWidget);
}

bool MozQGLWidgetWrapper::hasGLContext(QGraphicsView* aView)
{
    return aView && qobject_cast<QGLWidget*>(aView->viewport());
}

bool MozQGLWidgetWrapper::isRGBAContext()
{
    QGLContext* context = const_cast<QGLContext*>(QGLContext::currentContext());
    return context && context->format().alpha();
}