/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This header file lists up all event messages.
 * Before including this header file, you should define:
 * NS_EVENT_MESSAGE(aMessage)
 *
 * Additionally, you can specify following macro for e*First and e*Last.
 * NS_EVENT_MESSAGE_FIRST_LAST(aMessage, aFirst, aLast)
 * This is optional, if you need only actual event messages, you don't need
 * to define this macro.
 *
 * Naming rules of the event messages:
 * 0. Starting with "e" prefix and use camelcase.
 * 1. Basically, use same name as the DOM name which is fired at dispatching
 *    the event.
 * 2. If the event message name becomes too generic, e.g., "eInvalid", that may
 *    conflict with another enum's item name, append something after the "e"
 *    prefix, e.g., "eFormInvalid".
 */

#ifndef NS_EVENT_MESSAGE_FIRST_LAST
#define UNDEF_NS_EVENT_MESSAGE_FIRST_LAST 1
#define NS_EVENT_MESSAGE_FIRST_LAST(aMessage, aFirst, aLast)
#endif

NS_EVENT_MESSAGE(eVoidEvent)

// This is a dummy event message for all event listener implementation in
// EventListenerManager.
NS_EVENT_MESSAGE(eAllEvents)

// Widget may be destroyed
NS_EVENT_MESSAGE(eWindowClose)

NS_EVENT_MESSAGE(eKeyPress)
NS_EVENT_MESSAGE(eKeyUp)
NS_EVENT_MESSAGE(eKeyDown)

// These messages are dispatched when PluginInstaceChild receives native
// keyboard events directly and it posts the information to the widget.
// These messages shouldn't be handled by content and non-reserved chrome
// event handlers.
NS_EVENT_MESSAGE(eKeyDownOnPlugin)
NS_EVENT_MESSAGE(eKeyUpOnPlugin)

NS_EVENT_MESSAGE(eBeforeKeyDown)
NS_EVENT_MESSAGE(eAfterKeyDown)
NS_EVENT_MESSAGE(eBeforeKeyUp)
NS_EVENT_MESSAGE(eAfterKeyUp)

// This message is sent after a content process handles a key event or accesskey
// to indicate that an potential accesskey was not found. The parent process may
// then respond by, for example, opening menus and processing other shortcuts.
// It inherits its properties from a keypress event.
NS_EVENT_MESSAGE(eAccessKeyNotFound)

NS_EVENT_MESSAGE(eResize)
NS_EVENT_MESSAGE(eScroll)

// Application installation
NS_EVENT_MESSAGE(eInstall)

// A plugin was clicked or otherwise focused. ePluginActivate should be
// used when the window is not active. ePluginFocus should be used when
// the window is active. In the latter case, the dispatcher of the event
// is expected to ensure that the plugin's widget is focused beforehand.
NS_EVENT_MESSAGE(ePluginActivate)
NS_EVENT_MESSAGE(ePluginFocus)

NS_EVENT_MESSAGE(eOffline)
NS_EVENT_MESSAGE(eOnline)

NS_EVENT_MESSAGE(eLanguageChange)

NS_EVENT_MESSAGE(eMouseMove)
NS_EVENT_MESSAGE(eMouseUp)
NS_EVENT_MESSAGE(eMouseDown)
NS_EVENT_MESSAGE(eMouseEnterIntoWidget)
NS_EVENT_MESSAGE(eMouseExitFromWidget)
NS_EVENT_MESSAGE(eMouseDoubleClick)
NS_EVENT_MESSAGE(eMouseClick)
// eMouseActivate is fired when the widget is activated by a click.
NS_EVENT_MESSAGE(eMouseActivate)
NS_EVENT_MESSAGE(eMouseOver)
NS_EVENT_MESSAGE(eMouseOut)
NS_EVENT_MESSAGE(eMouseHitTest)
NS_EVENT_MESSAGE(eMouseEnter)
NS_EVENT_MESSAGE(eMouseLeave)
NS_EVENT_MESSAGE(eMouseLongTap)
NS_EVENT_MESSAGE_FIRST_LAST(eMouseEvent, eMouseMove, eMouseLongTap)

// Pointer spec events
NS_EVENT_MESSAGE(ePointerMove)
NS_EVENT_MESSAGE(ePointerUp)
NS_EVENT_MESSAGE(ePointerDown)
NS_EVENT_MESSAGE(ePointerOver)
NS_EVENT_MESSAGE(ePointerOut)
NS_EVENT_MESSAGE(ePointerEnter)
NS_EVENT_MESSAGE(ePointerLeave)
NS_EVENT_MESSAGE(ePointerCancel)
NS_EVENT_MESSAGE(ePointerGotCapture)
NS_EVENT_MESSAGE(ePointerLostCapture)
NS_EVENT_MESSAGE_FIRST_LAST(ePointerEvent, ePointerMove, ePointerLostCapture)

NS_EVENT_MESSAGE(eContextMenu)

NS_EVENT_MESSAGE(eLoad)
NS_EVENT_MESSAGE(eUnload)
NS_EVENT_MESSAGE(eHashChange)
NS_EVENT_MESSAGE(eImageAbort)
NS_EVENT_MESSAGE(eLoadError)
NS_EVENT_MESSAGE(eLoadEnd)
NS_EVENT_MESSAGE(ePopState)
NS_EVENT_MESSAGE(eStorage)
NS_EVENT_MESSAGE(eBeforeUnload)
NS_EVENT_MESSAGE(eReadyStateChange)

NS_EVENT_MESSAGE(eFormSubmit)
NS_EVENT_MESSAGE(eFormReset)
NS_EVENT_MESSAGE(eFormChange)
NS_EVENT_MESSAGE(eFormSelect)
NS_EVENT_MESSAGE(eFormInvalid)

//Need separate focus/blur notifications for non-native widgets
NS_EVENT_MESSAGE(eFocus)
NS_EVENT_MESSAGE(eBlur)

NS_EVENT_MESSAGE(eDragEnter)
NS_EVENT_MESSAGE(eDragOver)
NS_EVENT_MESSAGE(eDragExit)
NS_EVENT_MESSAGE(eDrag)
NS_EVENT_MESSAGE(eDragEnd)
NS_EVENT_MESSAGE(eDragStart)
NS_EVENT_MESSAGE(eDrop)
NS_EVENT_MESSAGE(eDragLeave)
NS_EVENT_MESSAGE_FIRST_LAST(eDragDropEvent, eDragEnter, eDragLeave)

// XUL specific events
NS_EVENT_MESSAGE(eXULPopupShowing)
NS_EVENT_MESSAGE(eXULPopupShown)
NS_EVENT_MESSAGE(eXULPopupPositioned)
NS_EVENT_MESSAGE(eXULPopupHiding)
NS_EVENT_MESSAGE(eXULPopupHidden)
NS_EVENT_MESSAGE(eXULBroadcast)
NS_EVENT_MESSAGE(eXULCommandUpdate)

// Legacy mouse scroll (wheel) events
NS_EVENT_MESSAGE(eLegacyMouseLineOrPageScroll)
NS_EVENT_MESSAGE(eLegacyMousePixelScroll)

NS_EVENT_MESSAGE(eScrollPortUnderflow)
NS_EVENT_MESSAGE(eScrollPortOverflow)

NS_EVENT_MESSAGE(eLegacySubtreeModified)
NS_EVENT_MESSAGE(eLegacyNodeInserted)
NS_EVENT_MESSAGE(eLegacyNodeRemoved)
NS_EVENT_MESSAGE(eLegacyNodeRemovedFromDocument)
NS_EVENT_MESSAGE(eLegacyNodeInsertedIntoDocument)
NS_EVENT_MESSAGE(eLegacyAttrModified)
NS_EVENT_MESSAGE(eLegacyCharacterDataModified)
NS_EVENT_MESSAGE_FIRST_LAST(eLegacyMutationEvent,
  eLegacySubtreeModified, eLegacyCharacterDataModified)

NS_EVENT_MESSAGE(eUnidentifiedEvent)

// composition events
NS_EVENT_MESSAGE(eCompositionStart)
// eCompositionEnd is the message for DOM compositionend event.
// This event should NOT be dispatched from widget if eCompositionCommit
// is available.
NS_EVENT_MESSAGE(eCompositionEnd)
// eCompositionUpdate is the message for DOM compositionupdate event.
// This event should NOT be dispatched from widget since it will be dispatched
// by mozilla::TextComposition automatically if eCompositionChange event
// will change composition string.
NS_EVENT_MESSAGE(eCompositionUpdate)
// eCompositionChange is the message for representing a change of
// composition string.  This should be dispatched from widget even if
// composition string isn't changed but the ranges are changed.  This causes
// a DOM "text" event which is a non-standard DOM event.
NS_EVENT_MESSAGE(eCompositionChange)
// eCompositionCommitAsIs is the message for representing a commit of
// composition string.  TextComposition will commit composition with the
// last data.  TextComposition will dispatch this event to the DOM tree as
// eCompositionChange without clause information.  After that,
// eCompositionEnd will be dispatched automatically.
// Its mData and mRanges should be empty and nullptr.
NS_EVENT_MESSAGE(eCompositionCommitAsIs)
// eCompositionCommit is the message for representing a commit of
// composition string with its mData value.  TextComposition will dispatch this
// event to the DOM tree as eCompositionChange without clause information.
// After that, eCompositionEnd will be dispatched automatically.
// Its mRanges should be nullptr.
NS_EVENT_MESSAGE(eCompositionCommit)

// Following events are defined for deprecated DOM events which are using
// InternalUIEvent class.
// DOMActivate (mapped with the DOM event and used internally)
NS_EVENT_MESSAGE(eLegacyDOMActivate)
// DOMFocusIn (only mapped with the DOM event)
NS_EVENT_MESSAGE(eLegacyDOMFocusIn)
// DOMFocusOut (only mapped with the DOM event)
NS_EVENT_MESSAGE(eLegacyDOMFocusOut)

// pagetransition events
NS_EVENT_MESSAGE(ePageShow)
NS_EVENT_MESSAGE(ePageHide)

// SVG events
NS_EVENT_MESSAGE(eSVGLoad)
NS_EVENT_MESSAGE(eSVGUnload)
NS_EVENT_MESSAGE(eSVGResize)
NS_EVENT_MESSAGE(eSVGScroll)

// SVG Zoom events
NS_EVENT_MESSAGE(eSVGZoom)

// XUL command events
NS_EVENT_MESSAGE(eXULCommand)

// Cut, copy, paste events
NS_EVENT_MESSAGE(eCopy)
NS_EVENT_MESSAGE(eCut)
NS_EVENT_MESSAGE(ePaste)

// Query for the selected text information, it return the selection offset,
// selection length and selected text.
NS_EVENT_MESSAGE(eQuerySelectedText)
// Query for the text content of specified range, it returns actual lengh (if
// the specified range is too long) and the text of the specified range.
// Returns the entire text if requested length > actual length.
NS_EVENT_MESSAGE(eQueryTextContent)
// Query for the caret rect of nth insertion point. The offset of the result is
// relative position from the top level widget.
NS_EVENT_MESSAGE(eQueryCaretRect)
// Query for the bounding rect of a range of characters. This works on any
// valid character range given offset and length. Result is relative to top
// level widget coordinates
NS_EVENT_MESSAGE(eQueryTextRect)
// Query for the bounding rect array of a range of characters.
// Thiis similar event of eQueryTextRect.
NS_EVENT_MESSAGE(eQueryTextRectArray)
// Query for the bounding rect of the current focused frame. Result is relative
// to top level widget coordinates
NS_EVENT_MESSAGE(eQueryEditorRect)
// Query for the current state of the content. The particular members of
// mReply that are set for each query content event will be valid on success.
NS_EVENT_MESSAGE(eQueryContentState)
// Query for the selection in the form of a nsITransferable.
NS_EVENT_MESSAGE(eQuerySelectionAsTransferable)
// Query for character at a point.  This returns the character offset, its
// rect and also tentative caret point if the point is clicked.  The point is
// specified by Event::mRefPoint.
NS_EVENT_MESSAGE(eQueryCharacterAtPoint)
// Query if the DOM element under Event::mRefPoint belongs to our widget
// or not.
NS_EVENT_MESSAGE(eQueryDOMWidgetHittest)

// Video events
NS_EVENT_MESSAGE(eLoadStart)
NS_EVENT_MESSAGE(eProgress)
NS_EVENT_MESSAGE(eSuspend)
NS_EVENT_MESSAGE(eEmptied)
NS_EVENT_MESSAGE(eStalled)
NS_EVENT_MESSAGE(ePlay)
NS_EVENT_MESSAGE(ePause)
NS_EVENT_MESSAGE(eLoadedMetaData)
NS_EVENT_MESSAGE(eLoadedData)
NS_EVENT_MESSAGE(eWaiting)
NS_EVENT_MESSAGE(ePlaying)
NS_EVENT_MESSAGE(eCanPlay)
NS_EVENT_MESSAGE(eCanPlayThrough)
NS_EVENT_MESSAGE(eSeeking)
NS_EVENT_MESSAGE(eSeeked)
NS_EVENT_MESSAGE(eTimeUpdate)
NS_EVENT_MESSAGE(eEnded)
NS_EVENT_MESSAGE(eRateChange)
NS_EVENT_MESSAGE(eDurationChange)
NS_EVENT_MESSAGE(eVolumeChange)

// paint notification events
NS_EVENT_MESSAGE(eAfterPaint)

// Simple gesture events
NS_EVENT_MESSAGE(eSwipeGestureMayStart)
NS_EVENT_MESSAGE(eSwipeGestureStart)
NS_EVENT_MESSAGE(eSwipeGestureUpdate)
NS_EVENT_MESSAGE(eSwipeGestureEnd)
NS_EVENT_MESSAGE(eSwipeGesture)
NS_EVENT_MESSAGE(eMagnifyGestureStart)
NS_EVENT_MESSAGE(eMagnifyGestureUpdate)
NS_EVENT_MESSAGE(eMagnifyGesture)
NS_EVENT_MESSAGE(eRotateGestureStart)
NS_EVENT_MESSAGE(eRotateGestureUpdate)
NS_EVENT_MESSAGE(eRotateGesture)
NS_EVENT_MESSAGE(eTapGesture)
NS_EVENT_MESSAGE(ePressTapGesture)
NS_EVENT_MESSAGE(eEdgeUIStarted)
NS_EVENT_MESSAGE(eEdgeUICanceled)
NS_EVENT_MESSAGE(eEdgeUICompleted)

// These are used to send native events to plugins.
NS_EVENT_MESSAGE(ePluginInputEvent)

// Events to manipulate selection (WidgetSelectionEvent)
// Clear any previous selection and set the given range as the selection
NS_EVENT_MESSAGE(eSetSelection)

// Events of commands for the contents
NS_EVENT_MESSAGE(eContentCommandCut)
NS_EVENT_MESSAGE(eContentCommandCopy)
NS_EVENT_MESSAGE(eContentCommandPaste)
NS_EVENT_MESSAGE(eContentCommandDelete)
NS_EVENT_MESSAGE(eContentCommandUndo)
NS_EVENT_MESSAGE(eContentCommandRedo)
NS_EVENT_MESSAGE(eContentCommandPasteTransferable)
NS_EVENT_MESSAGE(eContentCommandLookUpDictionary)
// eContentCommandScroll scrolls the nearest scrollable element to the
// currently focused content or latest DOM selection. This would normally be
// the same element scrolled by keyboard scroll commands, except that this event
// will scroll an element scrollable in either direction.  I.e., if the nearest
// scrollable ancestor element can only be scrolled vertically, and horizontal
// scrolling is requested using this event, no scrolling will occur.
NS_EVENT_MESSAGE(eContentCommandScroll)

// Event to gesture notification
NS_EVENT_MESSAGE(eGestureNotify)

NS_EVENT_MESSAGE(eScrolledAreaChanged)

// CSS Transition & Animation events:
NS_EVENT_MESSAGE(eTransitionEnd)
NS_EVENT_MESSAGE(eAnimationStart)
NS_EVENT_MESSAGE(eAnimationEnd)
NS_EVENT_MESSAGE(eAnimationIteration)

// Webkit-prefixed versions of Transition & Animation events, for web compat:
NS_EVENT_MESSAGE(eWebkitTransitionEnd)
NS_EVENT_MESSAGE(eWebkitAnimationStart)
NS_EVENT_MESSAGE(eWebkitAnimationEnd)
NS_EVENT_MESSAGE(eWebkitAnimationIteration)

NS_EVENT_MESSAGE(eSMILBeginEvent)
NS_EVENT_MESSAGE(eSMILEndEvent)
NS_EVENT_MESSAGE(eSMILRepeatEvent)

NS_EVENT_MESSAGE(eAudioProcess)
NS_EVENT_MESSAGE(eAudioComplete)

// script notification events
NS_EVENT_MESSAGE(eBeforeScriptExecute)
NS_EVENT_MESSAGE(eAfterScriptExecute)

NS_EVENT_MESSAGE(eBeforePrint)
NS_EVENT_MESSAGE(eAfterPrint)

NS_EVENT_MESSAGE(eMessage)

// Menu open event
NS_EVENT_MESSAGE(eOpen)

// Device motion and orientation
NS_EVENT_MESSAGE(eDeviceOrientation)
NS_EVENT_MESSAGE(eAbsoluteDeviceOrientation)
NS_EVENT_MESSAGE(eDeviceMotion)
NS_EVENT_MESSAGE(eDeviceProximity)
NS_EVENT_MESSAGE(eUserProximity)
NS_EVENT_MESSAGE(eDeviceLight)
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
NS_EVENT_MESSAGE(eOrientationChange)
#endif

// WebVR events
NS_EVENT_MESSAGE(eVRDisplayConnect)
NS_EVENT_MESSAGE(eVRDisplayDisconnect)
NS_EVENT_MESSAGE(eVRDisplayPresentChange)

NS_EVENT_MESSAGE(eShow)

// Fullscreen DOM API
NS_EVENT_MESSAGE(eFullscreenChange)
NS_EVENT_MESSAGE(eFullscreenError)
NS_EVENT_MESSAGE(eMozFullscreenChange)
NS_EVENT_MESSAGE(eMozFullscreenError)

NS_EVENT_MESSAGE(eTouchStart)
NS_EVENT_MESSAGE(eTouchMove)
NS_EVENT_MESSAGE(eTouchEnd)
NS_EVENT_MESSAGE(eTouchCancel)

// Pointerlock DOM API
NS_EVENT_MESSAGE(ePointerLockChange)
NS_EVENT_MESSAGE(ePointerLockError)
NS_EVENT_MESSAGE(eMozPointerLockChange)
NS_EVENT_MESSAGE(eMozPointerLockError)

// eWheel is the event message of DOM wheel event.
NS_EVENT_MESSAGE(eWheel)
// eWheelOperationStart may be dispatched when user starts to operate mouse
// wheel.  This won't be fired on some platforms which don't have corresponding
// native event.
NS_EVENT_MESSAGE(eWheelOperationStart)
// eWheelOperationEnd may be dispatched when user ends or cancels operating
// mouse wheel.  This won't be fired on some platforms which don't have
// corresponding native event.
NS_EVENT_MESSAGE(eWheelOperationEnd)

//System time is changed
NS_EVENT_MESSAGE(eTimeChange)

// Network packet events.
NS_EVENT_MESSAGE(eNetworkUpload)
NS_EVENT_MESSAGE(eNetworkDownload)

// MediaRecorder events.
NS_EVENT_MESSAGE(eMediaRecorderDataAvailable)
NS_EVENT_MESSAGE(eMediaRecorderWarning)
NS_EVENT_MESSAGE(eMediaRecorderStop)

// SpeakerManager events
NS_EVENT_MESSAGE(eSpeakerForcedChange)

#ifdef MOZ_GAMEPAD
// Gamepad input events
NS_EVENT_MESSAGE(eGamepadButtonDown)
NS_EVENT_MESSAGE(eGamepadButtonUp)
NS_EVENT_MESSAGE(eGamepadAxisMove)
NS_EVENT_MESSAGE(eGamepadConnected)
NS_EVENT_MESSAGE(eGamepadDisconnected)
NS_EVENT_MESSAGE_FIRST_LAST(eGamepadEvent,
                            eGamepadButtonDown, eGamepadDisconnected)
#endif

// input and beforeinput events.
NS_EVENT_MESSAGE(eEditorInput)

// selection events
NS_EVENT_MESSAGE(eSelectStart)
NS_EVENT_MESSAGE(eSelectionChange)

// Details element events.
NS_EVENT_MESSAGE(eToggle)

#ifdef UNDEF_NS_EVENT_MESSAGE_FIRST_LAST
#undef UNDEF_NS_EVENT_MESSAGE_FIRST_LAST
#undef NS_EVENT_MESSAGE_FIRST_LAST
#endif
