/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_GTK_MPRIS_INTERFACE_DESCRIPTION_H_
#define WIDGET_GTK_MPRIS_INTERFACE_DESCRIPTION_H_

#include <gio/gio.h>

extern const gchar introspection_xml[] =
    // adopted from https://github.com/freedesktop/mpris-spec/blob/master/spec/org.mpris.MediaPlayer2.xml
    // everything starting with tp can be removed, as it is used for HTML Spec Documentation Generation
    "<node>"
        "<interface name=\"org.mpris.MediaPlayer2\">"
            "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "<method name=\"Raise\"/>"
            "<method name=\"Quit\"/>"
            "<property name=\"CanQuit\" type=\"b\" access=\"read\"/>"
            "<property name=\"CanRaise\" type=\"b\" access=\"read\"/>"
            "<property name=\"HasTrackList\" type=\"b\" access=\"read\"/>"
            "<property name=\"Identity\" type=\"s\" access=\"read\"/>"
            "<property name=\"DesktopEntry\" type=\"s\" access=\"read\"/>"
            "<property name=\"SupportedUriSchemes\" type=\"as\" access=\"read\"/>"
            "<property name=\"SupportedMimeTypes\" type=\"as\" access=\"read\"/>"
        "</interface>"
            // Note that every property emits a changed signal (which is default) apart from Position.
        "<interface name=\"org.mpris.MediaPlayer2.Player\">"
            "<method name=\"Next\"/>"
            "<method name=\"Previous\"/>"
            "<method name=\"Pause\"/>"
            "<method name=\"PlayPause\"/>"
            "<method name=\"Stop\"/>"
            "<method name=\"Play\"/>"
            "<method name=\"Seek\">"
                "<arg direction=\"in\" type=\"x\" name=\"Offset\"/>"
            "</method>"
            "<method name=\"SetPosition\">"
                "<arg direction=\"in\" type=\"o\" name=\"TrackId\"/>"
                "<arg direction=\"in\" type=\"x\" name=\"Position\"/>"
            "</method>"
            "<method name=\"OpenUri\">"
                "<arg direction=\"in\" type=\"s\" name=\"Uri\"/>"
            "</method>"
            "<property name=\"PlaybackStatus\" type=\"s\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"Rate\" type=\"d\" access=\"readwrite\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"Metadata\" type=\"a{sv}\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"Volume\" type=\"d\" access=\"readwrite\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"Position\" type=\"x\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"false\"/>"
            "</property>"
            "<property name=\"MinimumRate\" type=\"d\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"MaximumRate\" type=\"d\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"CanGoNext\" type=\"b\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"CanGoPrevious\" type=\"b\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"CanPlay\" type=\"b\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"CanPause\" type=\"b\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"CanSeek\" type=\"b\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"true\"/>"
            "</property>"
            "<property name=\"CanControl\" type=\"b\" access=\"read\">"
                "<annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"false\"/>"
            "</property>"
            "<signal name=\"Seeked\">"
                "<arg name=\"Position\" type=\"x\"/>"
            "</signal>"
        "</interface>"
    "</node>";

#endif  // WIDGET_GTK_MPRIS_INTERFACE_DESCRIPTION_H_
