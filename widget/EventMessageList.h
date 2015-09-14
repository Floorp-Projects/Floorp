/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This header file lists up all event messages and their values.
 * Before including this header file, you should define
 * NS_EVENT_MESSAGE(aMessage, aValue)
 */

NS_EVENT_MESSAGE(eVoidEvent,            0)

// This is a dummy event message for all event listener implementation in
// EventListenerManager.
NS_EVENT_MESSAGE(eAllEvents,            1)

NS_EVENT_MESSAGE(eWindowEventFirst,     100)
// Widget may be destroyed
NS_EVENT_MESSAGE(eWindowClose,          eWindowEventFirst + 1)

NS_EVENT_MESSAGE(eKeyPress,             eWindowEventFirst + 31)
NS_EVENT_MESSAGE(eKeyUp,                eWindowEventFirst + 32)
NS_EVENT_MESSAGE(eKeyDown,              eWindowEventFirst + 33)

NS_EVENT_MESSAGE(eBeforeKeyDown,        eWindowEventFirst + 34)
NS_EVENT_MESSAGE(eAfterKeyDown,         eWindowEventFirst + 35)
NS_EVENT_MESSAGE(eBeforeKeyUp,          eWindowEventFirst + 36)
NS_EVENT_MESSAGE(eAfterKeyUp,           eWindowEventFirst + 37)

NS_EVENT_MESSAGE(eResize,               eWindowEventFirst + 60)
NS_EVENT_MESSAGE(eScroll,               eWindowEventFirst + 61)

// A plugin was clicked or otherwise focused. ePluginActivate should be
// used when the window is not active. ePluginFocus should be used when
// the window is active. In the latter case, the dispatcher of the event
// is expected to ensure that the plugin's widget is focused beforehand.
NS_EVENT_MESSAGE(ePluginActivate,       eWindowEventFirst + 62)
NS_EVENT_MESSAGE(ePluginFocus,          eWindowEventFirst + 63)

NS_EVENT_MESSAGE(eOffline,              eWindowEventFirst + 64)
NS_EVENT_MESSAGE(eOnline,               eWindowEventFirst + 65)

// NS_BEFORERESIZE_EVENT used to be here (eWindowEventFirst + 66)

NS_EVENT_MESSAGE(eLanguageChange,       eWindowEventFirst + 70)

NS_EVENT_MESSAGE(eMouseEventFirst,      300)
NS_EVENT_MESSAGE(eMouseMove,            eMouseEventFirst)
NS_EVENT_MESSAGE(eMouseUp,              eMouseEventFirst + 1)
NS_EVENT_MESSAGE(eMouseDown,            eMouseEventFirst + 2)
NS_EVENT_MESSAGE(eMouseEnterIntoWidget, eMouseEventFirst + 22)
NS_EVENT_MESSAGE(eMouseExitFromWidget,  eMouseEventFirst + 23)
NS_EVENT_MESSAGE(eMouseDoubleClick,     eMouseEventFirst + 24)
NS_EVENT_MESSAGE(eMouseClick,           eMouseEventFirst + 27)
// eMouseActivate is fired when the widget is activated by a click.
NS_EVENT_MESSAGE(eMouseActivate,        eMouseEventFirst + 30)
NS_EVENT_MESSAGE(eMouseOver,            eMouseEventFirst + 31)
NS_EVENT_MESSAGE(eMouseOut,             eMouseEventFirst + 32)
NS_EVENT_MESSAGE(eMouseHitTest,         eMouseEventFirst + 33)
NS_EVENT_MESSAGE(eMouseEnter,           eMouseEventFirst + 34)
NS_EVENT_MESSAGE(eMouseLeave,           eMouseEventFirst + 35)
NS_EVENT_MESSAGE(eMouseLongTap,         eMouseEventFirst + 36)

// Pointer spec events
NS_EVENT_MESSAGE(ePointerEventFirst,    4400)
NS_EVENT_MESSAGE(ePointerMove,          ePointerEventFirst)
NS_EVENT_MESSAGE(ePointerUp,            ePointerEventFirst + 1)
NS_EVENT_MESSAGE(ePointerDown,          ePointerEventFirst + 2)
NS_EVENT_MESSAGE(ePointerOver,          ePointerEventFirst + 22)
NS_EVENT_MESSAGE(ePointerOut,           ePointerEventFirst + 23)
NS_EVENT_MESSAGE(ePointerEnter,         ePointerEventFirst + 24)
NS_EVENT_MESSAGE(ePointerLeave,         ePointerEventFirst + 25)
NS_EVENT_MESSAGE(ePointerCancel,        ePointerEventFirst + 26)
NS_EVENT_MESSAGE(ePointerGotCapture,    ePointerEventFirst + 27)
NS_EVENT_MESSAGE(ePointerLostCapture,   ePointerEventFirst + 28)
NS_EVENT_MESSAGE(ePointerEventLast,     ePointerLostCapture)

NS_EVENT_MESSAGE(eContextMenuFirst,     500)
NS_EVENT_MESSAGE(eContextMenu,          eContextMenuFirst)

NS_EVENT_MESSAGE(eStreamEventFirst,     1100)
NS_EVENT_MESSAGE(eLoad,                 eStreamEventFirst)
NS_EVENT_MESSAGE(eUnload,               eStreamEventFirst + 1)
NS_EVENT_MESSAGE(eHashChange,           eStreamEventFirst + 2)
NS_EVENT_MESSAGE(eImageAbort,           eStreamEventFirst + 3)
NS_EVENT_MESSAGE(eLoadError,            eStreamEventFirst + 4)
NS_EVENT_MESSAGE(ePopState,             eStreamEventFirst + 5)
NS_EVENT_MESSAGE(eBeforeUnload,         eStreamEventFirst + 6)
NS_EVENT_MESSAGE(eReadyStateChange,     eStreamEventFirst + 8)
 
NS_EVENT_MESSAGE(eFormEventFirst,       1200)
NS_EVENT_MESSAGE(eFormSubmit,           eFormEventFirst)
NS_EVENT_MESSAGE(eFormReset,            eFormEventFirst + 1)
NS_EVENT_MESSAGE(eFormChange,           eFormEventFirst + 2)
NS_EVENT_MESSAGE(eFormSelect,           eFormEventFirst + 3)
NS_EVENT_MESSAGE(eFormInvalid,          eFormEventFirst + 4)

//Need separate focus/blur notifications for non-native widgets
NS_EVENT_MESSAGE(eFocusEventFirst,      1300)
NS_EVENT_MESSAGE(eFocus,                eFocusEventFirst)
NS_EVENT_MESSAGE(eBlur,                 eFocusEventFirst + 1)

NS_EVENT_MESSAGE(eDragDropEventFirst,   1400)
NS_EVENT_MESSAGE(eDragEnter,            eDragDropEventFirst)
NS_EVENT_MESSAGE(eDragOver,             eDragDropEventFirst + 1)
NS_EVENT_MESSAGE(eDragExit,             eDragDropEventFirst + 2)
NS_EVENT_MESSAGE(eLegacyDragDrop,       eDragDropEventFirst + 3)
NS_EVENT_MESSAGE(eLegacyDragGesture,    eDragDropEventFirst + 4)
NS_EVENT_MESSAGE(eDrag,                 eDragDropEventFirst + 5)
NS_EVENT_MESSAGE(eDragEnd,              eDragDropEventFirst + 6)
NS_EVENT_MESSAGE(eDragStart,            eDragDropEventFirst + 7)
NS_EVENT_MESSAGE(eDrop,                 eDragDropEventFirst + 8)
NS_EVENT_MESSAGE(eDragLeave,            eDragDropEventFirst + 9)
NS_EVENT_MESSAGE(eDragDropEventLast,    eDragLeave)

// XUL specific events
NS_EVENT_MESSAGE(eXULEventFirst,        1500)
NS_EVENT_MESSAGE(eXULPopupShowing,      eXULEventFirst)
NS_EVENT_MESSAGE(eXULPopupShown,        eXULEventFirst + 1)
NS_EVENT_MESSAGE(eXULPopupHiding,       eXULEventFirst + 2)
NS_EVENT_MESSAGE(eXULPopupHidden,       eXULEventFirst + 3)
NS_EVENT_MESSAGE(eXULBroadcast,         eXULEventFirst + 5)
NS_EVENT_MESSAGE(eXULCommandUpdate,     eXULEventFirst + 6)

// Legacy mouse scroll (wheel) events
NS_EVENT_MESSAGE(eLegacyMouseScrollEventFirst, 1600)
NS_EVENT_MESSAGE(eLegacyMouseLineOrPageScroll, eLegacyMouseScrollEventFirst)
NS_EVENT_MESSAGE(eLegacyMousePixelScroll,      eLegacyMouseScrollEventFirst + 1)

NS_EVENT_MESSAGE(eScrollPortEventFirst, 1700)
NS_EVENT_MESSAGE(eScrollPortUnderflow,  eScrollPortEventFirst)
NS_EVENT_MESSAGE(eScrollPortOverflow,   eScrollPortEventFirst + 1)

NS_EVENT_MESSAGE(eLegacyMutationEventFirst,       1800)
NS_EVENT_MESSAGE(eLegacySubtreeModified,          eLegacyMutationEventFirst)
NS_EVENT_MESSAGE(eLegacyNodeInserted,             eLegacyMutationEventFirst + 1)
NS_EVENT_MESSAGE(eLegacyNodeRemoved,              eLegacyMutationEventFirst + 2)
NS_EVENT_MESSAGE(eLegacyNodeRemovedFromDocument,  eLegacyMutationEventFirst + 3)
NS_EVENT_MESSAGE(eLegacyNodeInsertedIntoDocument, eLegacyMutationEventFirst + 4)
NS_EVENT_MESSAGE(eLegacyAttrModified,             eLegacyMutationEventFirst + 5)
NS_EVENT_MESSAGE(eLegacyCharacterDataModified,    eLegacyMutationEventFirst + 6)
NS_EVENT_MESSAGE(eLegacyMutationEventLast,        eLegacyCharacterDataModified)

NS_EVENT_MESSAGE(eUnidentifiedEvent,    2000)
 
// composition events
NS_EVENT_MESSAGE(eCompositionEventFirst, 2200)
NS_EVENT_MESSAGE(eCompositionStart,      eCompositionEventFirst)
// eCompositionEnd is the message for DOM compositionend event.
// This event should NOT be dispatched from widget if eCompositionCommit
// is available.
NS_EVENT_MESSAGE(eCompositionEnd,        eCompositionEventFirst + 1)
// eCompositionUpdate is the message for DOM compositionupdate event.
// This event should NOT be dispatched from widget since it will be dispatched
// by mozilla::TextComposition automatically if eCompositionChange event
// will change composition string.
NS_EVENT_MESSAGE(eCompositionUpdate,     eCompositionEventFirst + 2)
// eCompositionChange is the message for representing a change of
// composition string.  This should be dispatched from widget even if
// composition string isn't changed but the ranges are changed.  This causes
// a DOM "text" event which is a non-standard DOM event.
NS_EVENT_MESSAGE(eCompositionChange,     eCompositionEventFirst + 3)
// eCompositionCommitAsIs is the message for representing a commit of
// composition string.  TextComposition will commit composition with the
// last data.  TextComposition will dispatch this event to the DOM tree as
// eCompositionChange without clause information.  After that,
// eCompositionEnd will be dispatched automatically.
// Its mData and mRanges should be empty and nullptr.
NS_EVENT_MESSAGE(eCompositionCommitAsIs, eCompositionEventFirst + 4)
// eCompositionCommit is the message for representing a commit of
// composition string with its mData value.  TextComposition will dispatch this
// event to the DOM tree as eCompositionChange without clause information.
// After that, eCompositionEnd will be dispatched automatically.
// Its mRanges should be nullptr.
NS_EVENT_MESSAGE(eCompositionCommit,     eCompositionEventFirst + 5)

// Following events are defined for deprecated DOM events which are using
// InternalUIEvent class.
NS_EVENT_MESSAGE(eLegacyUIEventFirst,   2500)
// DOMActivate (mapped with the DOM event and used internally)
NS_EVENT_MESSAGE(eLegacyDOMActivate,    eLegacyUIEventFirst)
// DOMFocusIn (only mapped with the DOM event)
NS_EVENT_MESSAGE(eLegacyDOMFocusIn,     eLegacyUIEventFirst + 1)
// DOMFocusOut (only mapped with the DOM event)
NS_EVENT_MESSAGE(eLegacyDOMFocusOut,    eLegacyUIEventFirst + 2)

// pagetransition events
NS_EVENT_MESSAGE(ePageTransitionEventFirst, 2700)
NS_EVENT_MESSAGE(ePageShow,                 ePageTransitionEventFirst + 1)
NS_EVENT_MESSAGE(ePageHide,                 ePageTransitionEventFirst + 2)

// SVG events
NS_EVENT_MESSAGE(eSVGEventFirst,        2800)
NS_EVENT_MESSAGE(eSVGLoad,              eSVGEventFirst)
NS_EVENT_MESSAGE(eSVGUnload,            eSVGEventFirst + 1)
NS_EVENT_MESSAGE(eSVGResize,            eSVGEventFirst + 4)
NS_EVENT_MESSAGE(eSVGScroll,            eSVGEventFirst + 5)

// SVG Zoom events
NS_EVENT_MESSAGE(eSVGZoomEventFirst,    2900)
NS_EVENT_MESSAGE(eSVGZoom,              eSVGZoomEventFirst)

// XUL command events
NS_EVENT_MESSAGE(eXULCommandEventFirst, 3000)
NS_EVENT_MESSAGE(eXULCommand,           eXULCommandEventFirst)

// Cut, copy, paste events
NS_EVENT_MESSAGE(eClipboardEventFirst,  3100)
NS_EVENT_MESSAGE(eCopy,                 eClipboardEventFirst)
NS_EVENT_MESSAGE(eCut,                  eClipboardEventFirst + 1)
NS_EVENT_MESSAGE(ePaste,                eClipboardEventFirst + 2)

// Query the content information
NS_EVENT_MESSAGE(eQueryContentEventFirst,       3200)
// Query for the selected text information, it return the selection offset,
// selection length and selected text.
NS_EVENT_MESSAGE(eQuerySelectedText,            eQueryContentEventFirst)
// Query for the text content of specified range, it returns actual lengh (if
// the specified range is too long) and the text of the specified range.
// Returns the entire text if requested length > actual length.
NS_EVENT_MESSAGE(eQueryTextContent,             eQueryContentEventFirst + 1)
// Query for the caret rect of nth insertion point. The offset of the result is
// relative position from the top level widget.
NS_EVENT_MESSAGE(eQueryCaretRect,               eQueryContentEventFirst + 3)
// Query for the bounding rect of a range of characters. This works on any
// valid character range given offset and length. Result is relative to top
// level widget coordinates
NS_EVENT_MESSAGE(eQueryTextRect,                eQueryContentEventFirst + 4)
// Query for the bounding rect of the current focused frame. Result is relative
// to top level widget coordinates
NS_EVENT_MESSAGE(eQueryEditorRect,              eQueryContentEventFirst + 5)
// Query for the current state of the content. The particular members of
// mReply that are set for each query content event will be valid on success.
NS_EVENT_MESSAGE(eQueryContentState,            eQueryContentEventFirst + 6)
// Query for the selection in the form of a nsITransferable.
NS_EVENT_MESSAGE(eQuerySelectionAsTransferable, eQueryContentEventFirst + 7)
// Query for character at a point.  This returns the character offset, its
// rect and also tentative caret point if the point is clicked.  The point is
// specified by Event::refPoint.
NS_EVENT_MESSAGE(eQueryCharacterAtPoint,        eQueryContentEventFirst + 8)
// Query if the DOM element under Event::refPoint belongs to our widget
// or not.
NS_EVENT_MESSAGE(eQueryDOMWidgetHittest,        eQueryContentEventFirst + 9)

// Video events
NS_EVENT_MESSAGE(eMediaEventFirst,      3300)
NS_EVENT_MESSAGE(eLoadStart,            eMediaEventFirst)
NS_EVENT_MESSAGE(eProgress,             eMediaEventFirst + 1)
NS_EVENT_MESSAGE(eSuspend,              eMediaEventFirst + 2)
NS_EVENT_MESSAGE(eEmptied,              eMediaEventFirst + 3)
NS_EVENT_MESSAGE(eStalled,              eMediaEventFirst + 4)
NS_EVENT_MESSAGE(ePlay,                 eMediaEventFirst + 5)
NS_EVENT_MESSAGE(ePause,                eMediaEventFirst + 6)
NS_EVENT_MESSAGE(eLoadedMetaData,       eMediaEventFirst + 7)
NS_EVENT_MESSAGE(eLoadedData,           eMediaEventFirst + 8)
NS_EVENT_MESSAGE(eWaiting,              eMediaEventFirst + 9)
NS_EVENT_MESSAGE(ePlaying,              eMediaEventFirst + 10)
NS_EVENT_MESSAGE(eCanPlay,              eMediaEventFirst + 11)
NS_EVENT_MESSAGE(eCanPlayThrough,       eMediaEventFirst + 12)
NS_EVENT_MESSAGE(eSeeking,              eMediaEventFirst + 13)
NS_EVENT_MESSAGE(eSeeked,               eMediaEventFirst + 14)
NS_EVENT_MESSAGE(eTimeUpdate,           eMediaEventFirst + 15)
NS_EVENT_MESSAGE(eEnded,                eMediaEventFirst + 16)
NS_EVENT_MESSAGE(eRateChange,           eMediaEventFirst + 17)
NS_EVENT_MESSAGE(eDurationChange,       eMediaEventFirst + 18)
NS_EVENT_MESSAGE(eVolumeChange,         eMediaEventFirst + 19)

// paint notification events
NS_EVENT_MESSAGE(ePaintEventFirst,      3400)
NS_EVENT_MESSAGE(eAfterPaint,           ePaintEventFirst)

// Simple gesture events
NS_EVENT_MESSAGE(NS_SIMPLE_GESTURE_EVENT_START,    3500)
NS_EVENT_MESSAGE(NS_SIMPLE_GESTURE_SWIPE_MAY_START,NS_SIMPLE_GESTURE_EVENT_START)
NS_EVENT_MESSAGE(NS_SIMPLE_GESTURE_SWIPE_START,    NS_SIMPLE_GESTURE_EVENT_START + 1)
NS_EVENT_MESSAGE(NS_SIMPLE_GESTURE_SWIPE_UPDATE,   NS_SIMPLE_GESTURE_EVENT_START + 2)
NS_EVENT_MESSAGE(NS_SIMPLE_GESTURE_SWIPE_END,      NS_SIMPLE_GESTURE_EVENT_START + 3)
NS_EVENT_MESSAGE(NS_SIMPLE_GESTURE_SWIPE,          NS_SIMPLE_GESTURE_EVENT_START + 4)
NS_EVENT_MESSAGE(NS_SIMPLE_GESTURE_MAGNIFY_START,  NS_SIMPLE_GESTURE_EVENT_START + 5)
NS_EVENT_MESSAGE(NS_SIMPLE_GESTURE_MAGNIFY_UPDATE, NS_SIMPLE_GESTURE_EVENT_START + 6)
NS_EVENT_MESSAGE(NS_SIMPLE_GESTURE_MAGNIFY,        NS_SIMPLE_GESTURE_EVENT_START + 7)
NS_EVENT_MESSAGE(NS_SIMPLE_GESTURE_ROTATE_START,   NS_SIMPLE_GESTURE_EVENT_START + 8)
NS_EVENT_MESSAGE(eRotateGestureUpdate,  NS_SIMPLE_GESTURE_EVENT_START + 9)
NS_EVENT_MESSAGE(eRotateGesture,        NS_SIMPLE_GESTURE_EVENT_START + 10)
NS_EVENT_MESSAGE(eTapGesture,           NS_SIMPLE_GESTURE_EVENT_START + 11)
NS_EVENT_MESSAGE(ePressTapGesture,      NS_SIMPLE_GESTURE_EVENT_START + 12)
NS_EVENT_MESSAGE(eEdgeUIStarted,        NS_SIMPLE_GESTURE_EVENT_START + 13)
NS_EVENT_MESSAGE(eEdgeUICanceled,       NS_SIMPLE_GESTURE_EVENT_START + 14)
NS_EVENT_MESSAGE(eEdgeUICompleted,      NS_SIMPLE_GESTURE_EVENT_START + 15)

// These are used to send native events to plugins.
NS_EVENT_MESSAGE(ePluginEventFirst,     3600)
NS_EVENT_MESSAGE(ePluginInputEvent,     ePluginEventFirst)

// Events to manipulate selection (WidgetSelectionEvent)
NS_EVENT_MESSAGE(eSelectionEventFirst,  3700)
// Clear any previous selection and set the given range as the selection
NS_EVENT_MESSAGE(eSetSelection,         eSelectionEventFirst)

// Events of commands for the contents
NS_EVENT_MESSAGE(eContentCommandEventFirst,        3800)
NS_EVENT_MESSAGE(eContentCommandCut,               eContentCommandEventFirst)
NS_EVENT_MESSAGE(eContentCommandCopy,              eContentCommandEventFirst + 1)
NS_EVENT_MESSAGE(eContentCommandPaste,             eContentCommandEventFirst + 2)
NS_EVENT_MESSAGE(eContentCommandDelete,            eContentCommandEventFirst + 3)
NS_EVENT_MESSAGE(eContentCommandUndo,              eContentCommandEventFirst + 4)
NS_EVENT_MESSAGE(eContentCommandRedo,              eContentCommandEventFirst + 5)
NS_EVENT_MESSAGE(eContentCommandPasteTransferable, eContentCommandEventFirst + 6)
// eContentCommandScroll scrolls the nearest scrollable element to the
// currently focused content or latest DOM selection. This would normally be
// the same element scrolled by keyboard scroll commands, except that this event
// will scroll an element scrollable in either direction.  I.e., if the nearest
// scrollable ancestor element can only be scrolled vertically, and horizontal
// scrolling is requested using this event, no scrolling will occur.
NS_EVENT_MESSAGE(eContentCommandScroll,            eContentCommandEventFirst + 7)

// Event to gesture notification
NS_EVENT_MESSAGE(eGestureNotify,        3900)

NS_EVENT_MESSAGE(eScrolledAreaEventFirst, 4100)
NS_EVENT_MESSAGE(eScrolledAreaChanged,    eScrolledAreaEventFirst)

NS_EVENT_MESSAGE(eTransitionEventFirst, 4200)
NS_EVENT_MESSAGE(eTransitionEnd,        eTransitionEventFirst)

NS_EVENT_MESSAGE(eAnimationEventFirst,  4250)
NS_EVENT_MESSAGE(eAnimationStart,       eAnimationEventFirst)
NS_EVENT_MESSAGE(eAnimationEnd,         eAnimationEventFirst + 1)
NS_EVENT_MESSAGE(eAnimationIteration,   eAnimationEventFirst + 2)

NS_EVENT_MESSAGE(eSMILEventFirst,       4300)
NS_EVENT_MESSAGE(eSMILBeginEvent,       eSMILEventFirst)
NS_EVENT_MESSAGE(eSMILEndEvent,         eSMILEventFirst + 1)
NS_EVENT_MESSAGE(eSMILRepeatEvent,      eSMILEventFirst + 2)

NS_EVENT_MESSAGE(eAudioEventFirst,      4350)
NS_EVENT_MESSAGE(eAudioProcess,         eAudioEventFirst)
NS_EVENT_MESSAGE(eAudioComplete,        eAudioEventFirst + 1)

// script notification events
NS_EVENT_MESSAGE(eScriptEventFirst,     4500)
NS_EVENT_MESSAGE(eBeforeScriptExecute,  eScriptEventFirst)
NS_EVENT_MESSAGE(eAfterScriptExecute,   eScriptEventFirst + 1)

NS_EVENT_MESSAGE(ePrintEventFirst,      4600)
NS_EVENT_MESSAGE(eBeforePrint,          ePrintEventFirst)
NS_EVENT_MESSAGE(eAfterPrint,           ePrintEventFirst + 1)

NS_EVENT_MESSAGE(eMessageEventFirst,    4700)
NS_EVENT_MESSAGE(eMessage,              eMessageEventFirst)

// Open and close events
NS_EVENT_MESSAGE(eOpenCloseEventFirst,  4800)
NS_EVENT_MESSAGE(eOpen,                 eOpenCloseEventFirst)

// Device motion and orientation
NS_EVENT_MESSAGE(eDeviceEventFirst,      4900)
NS_EVENT_MESSAGE(eDeviceOrientation,     eDeviceEventFirst)
NS_EVENT_MESSAGE(eDeviceMotion,          eDeviceEventFirst + 1)
NS_EVENT_MESSAGE(eDeviceProximity,       eDeviceEventFirst + 2)
NS_EVENT_MESSAGE(eUserProximity,         eDeviceEventFirst + 3)
NS_EVENT_MESSAGE(eDeviceLight,           eDeviceEventFirst + 4)

NS_EVENT_MESSAGE(eShow,                  5000)

// Fullscreen DOM API
NS_EVENT_MESSAGE(eFullscreenEventFirst,  5100)
NS_EVENT_MESSAGE(eFullscreenChange,      eFullscreenEventFirst)
NS_EVENT_MESSAGE(eFullscreenError,       eFullscreenEventFirst + 1)

NS_EVENT_MESSAGE(eTouchEventFirst,       5200)
NS_EVENT_MESSAGE(eTouchStart,            eTouchEventFirst)
NS_EVENT_MESSAGE(eTouchMove,             eTouchEventFirst + 1)
NS_EVENT_MESSAGE(eTouchEnd,              eTouchEventFirst + 2)
NS_EVENT_MESSAGE(eTouchCancel,           eTouchEventFirst + 3)

// Pointerlock DOM API
NS_EVENT_MESSAGE(ePointerLockEventFirst, 5300)
NS_EVENT_MESSAGE(ePointerLockChange,     ePointerLockEventFirst)
NS_EVENT_MESSAGE(ePointerLockError,      ePointerLockEventFirst + 1)

NS_EVENT_MESSAGE(eWheelEventFirst,       5400)
// eWheel is the event message of DOM wheel event.
NS_EVENT_MESSAGE(eWheel,                 eWheelEventFirst)
// eWheelOperationStart may be dispatched when user starts to operate mouse
// wheel.  This won't be fired on some platforms which don't have corresponding
// native event.
NS_EVENT_MESSAGE(eWheelOperationStart,   eWheelEventFirst + 1)
// eWheelOperationEnd may be dispatched when user ends or cancels operating
// mouse wheel.  This won't be fired on some platforms which don't have
// corresponding native event.
NS_EVENT_MESSAGE(eWheelOperationEnd,     eWheelEventFirst + 2)

//System time is changed
NS_EVENT_MESSAGE(eTimeChange,            5500)

// Network packet events.
NS_EVENT_MESSAGE(eNetworkEventFirst,     5600)
NS_EVENT_MESSAGE(eNetworkUpload,         eNetworkEventFirst + 1)
NS_EVENT_MESSAGE(eNetworkDownload,       eNetworkEventFirst + 2)

// MediaRecorder events.
NS_EVENT_MESSAGE(eMediaRecorderEventFirst,    5700)
NS_EVENT_MESSAGE(eMediaRecorderDataAvailable, eMediaRecorderEventFirst + 1)
NS_EVENT_MESSAGE(eMediaRecorderWarning,       eMediaRecorderEventFirst + 2)
NS_EVENT_MESSAGE(eMediaRecorderStop,          eMediaRecorderEventFirst + 3)

// SpeakerManager events
NS_EVENT_MESSAGE(eSpeakerManagerEventFirst, 5800)
NS_EVENT_MESSAGE(eSpeakerForcedChange,      eSpeakerManagerEventFirst + 1)

#ifdef MOZ_GAMEPAD
// Gamepad input events
NS_EVENT_MESSAGE(eGamepadEventFirst,     6000)
NS_EVENT_MESSAGE(eGamepadButtonDown,     eGamepadEventFirst)
NS_EVENT_MESSAGE(eGamepadButtonUp,       eGamepadEventFirst + 1)
NS_EVENT_MESSAGE(eGamepadAxisMove,       eGamepadEventFirst + 2)
NS_EVENT_MESSAGE(eGamepadConnected,      eGamepadEventFirst + 3)
NS_EVENT_MESSAGE(eGamepadDisconnected,   eGamepadEventFirst + 4)
NS_EVENT_MESSAGE(eGamepadEventLast,      eGamepadDisconnected)
#endif

// input and beforeinput events.
NS_EVENT_MESSAGE(eEditorEventFirst,      6100)
NS_EVENT_MESSAGE(eEditorInput,           eEditorEventFirst)

// selection events
NS_EVENT_MESSAGE(eSelectEventFirst,      6200)
NS_EVENT_MESSAGE(eSelectStart,           eSelectEventFirst)
NS_EVENT_MESSAGE(eSelectionChange,       eSelectEventFirst + 1)
