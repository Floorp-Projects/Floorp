/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZQGLWIDGETWRAPPER_H
#define MOZQGLWIDGETWRAPPER_H

/*
 * qgl.h and GLDefs.h has conflicts in type defines
 * QGLWidget wrapper class helps to avoid including qgl.h with mozilla gl includes
 */

class QGLWidget;
class QGraphicsView;
class MozQGLWidgetWrapper
{
public:
    MozQGLWidgetWrapper();
    ~MozQGLWidgetWrapper();
    void makeCurrent();
    void setViewport(QGraphicsView*);
    static bool hasGLContext(QGraphicsView*);
    static bool isRGBAContext();
private:
    QGLWidget* mWidget;
};

#endif
