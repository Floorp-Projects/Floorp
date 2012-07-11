/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set expandtab shiftwidth=2 tabstop=2: */
 
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY          0
#define XEMBED_WINDOW_ACTIVATE          1
#define XEMBED_WINDOW_DEACTIVATE        2
#define XEMBED_REQUEST_FOCUS            3
#define XEMBED_FOCUS_IN                 4
#define XEMBED_FOCUS_OUT                5
#define XEMBED_FOCUS_NEXT               6
#define XEMBED_FOCUS_PREV               7
#define XEMBED_GRAB_KEY                 8
#define XEMBED_UNGRAB_KEY               9
#define XEMBED_MODALITY_ON              10
#define XEMBED_MODALITY_OFF             11

/* Non standard messages*/
#define XEMBED_GTK_GRAB_KEY             108 
#define XEMBED_GTK_UNGRAB_KEY           109

/* Details for  XEMBED_FOCUS_IN: */
#define XEMBED_FOCUS_CURRENT            0
#define XEMBED_FOCUS_FIRST              1
#define XEMBED_FOCUS_LAST               2

/* Flags for _XEMBED_INFO */
#define XEMBED_MAPPED                   (1 << 0)
