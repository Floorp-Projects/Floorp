/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This header file lists up all event classes and related structs.
 * Define NS_EVENT_CLASS(aPrefix, aName) and NS_ROOT_EVENT_CLASS(aPrefix, aName)
 * before including this.
 * If an event name is WidgetInputEvent, aPrefix is "Widget" and aName is
 * "InputEvent".  NS_ROOT_EVENT_CLASS() is only used for WidgetEvent for
 * allowing special handling for it.  If you don't need such special handling,
 * you can define it as:
 * #define NS_ROOT_EVENT_CLASS(aPrefix, aName) NS_EVENT_CLASS(aPrefix, aName)
 */

// BasicEvents.h
NS_ROOT_EVENT_CLASS(Widget, Event)
NS_EVENT_CLASS(Widget, GUIEvent)
NS_EVENT_CLASS(Widget, InputEvent)
NS_EVENT_CLASS(Internal, UIEvent)

// TextEvents.h
NS_EVENT_CLASS(Widget, KeyboardEvent)
NS_EVENT_CLASS(Widget, TextEvent)
NS_EVENT_CLASS(Widget, CompositionEvent)
NS_EVENT_CLASS(Widget, QueryContentEvent)
NS_EVENT_CLASS(Widget, SelectionEvent)

// MouseEvents.h
NS_EVENT_CLASS(Widget, MouseEventBase)
NS_EVENT_CLASS(Widget, MouseEvent)
NS_EVENT_CLASS(Widget, DragEvent)
NS_EVENT_CLASS(Widget, MouseScrollEvent)
NS_EVENT_CLASS(Widget, WheelEvent)

// TouchEvents.h
NS_EVENT_CLASS(Widget, GestureNotifyEvent)
NS_EVENT_CLASS(Widget, SimpleGestureEvent)
NS_EVENT_CLASS(Widget, TouchEvent)

// ContentEvents.h
NS_EVENT_CLASS(Internal, ScriptErrorEvent)
NS_EVENT_CLASS(Internal, ScrollPortEvent)
NS_EVENT_CLASS(Internal, ScrollAreaEvent)
NS_EVENT_CLASS(Internal, FormEvent)
NS_EVENT_CLASS(Internal, ClipboardEvent)
NS_EVENT_CLASS(Internal, FocusEvent)
NS_EVENT_CLASS(Internal, TransitionEvent)
NS_EVENT_CLASS(Internal, AnimationEvent)

// MiscEvents.h
NS_EVENT_CLASS(Widget, CommandEvent)
NS_EVENT_CLASS(Widget, ContentCommandEvent)
NS_EVENT_CLASS(Widget, PluginEvent)

// MutationEvent.h (content/events/public)
NS_EVENT_CLASS(Internal, MutationEvent)
