/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_textinputdispatcherlistener_h_
#define mozilla_textinputdispatcherlistener_h_

#include "nsWeakReference.h"

namespace mozilla {
namespace widget {

class TextEventDispatcher;
struct IMENotification;

#define NS_TEXT_INPUT_PROXY_LISTENER_IID \
{ 0xf2226f55, 0x6ddb, 0x40d5, \
  { 0x8a, 0x24, 0xce, 0x4d, 0x5b, 0x38, 0x15, 0xf0 } };

class TextEventDispatcherListener : public nsSupportsWeakReference
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_TEXT_INPUT_PROXY_LISTENER_IID)

  /**
   * NotifyIME() is called by TextEventDispatcher::NotifyIME().  This is a
   * notification or request to IME.  See document of nsIWidget::NotifyIME()
   * for the detail.
   */
  NS_IMETHOD NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                       const IMENotification& aNotification) = 0;

  /**
   * OnRemovedFrom() is called when the TextEventDispatcher stops working and
   * is releasing the listener.
   */
  NS_IMETHOD_(void) OnRemovedFrom(
                      TextEventDispatcher* aTextEventDispatcher) = 0;

  /**
   * WillDispatchKeyboardEvent() may be called immediately before
   * TextEventDispatcher dispatching a keyboard event.  This is called only
   * during calling TextEventDispatcher::DispatchKeyboardEvent() or
   * TextEventDispatcher::MaybeDispatchKeypressEvents().  But this may not
   * be called if TextEventDispatcher thinks that the keyboard event doesn't
   * need alternative char codes.
   *
   * This method can overwrite any members of aKeyboardEvent which is already
   * initialized by TextEventDispatcher.  If it's necessary, this method should
   * overwrite the charCode when Control key is pressed.  TextEventDispatcher
   * computes charCode from mKeyValue.  However, when Control key is pressed,
   * charCode should be an ASCII char.  In such case, this method needs to
   * overwrite it properly.
   *
   * @param aTextEventDispatcher    Pointer to the caller.
   * @param aKeyboardEvent          The event trying to dispatch.
   *                                This is already initialized, but if it's
   *                                necessary, this method should overwrite the
   *                                members and set alternative char codes.
   * @param aIndexOfKeypress        When aKeyboardEvent is eKeyPress event,
   *                                it may be a sequence of keypress events
   *                                if the key causes multiple characters.
   *                                In such case, this indicates the index from
   *                                first keypress event.
   *                                If aKeyboardEvent is the first eKeyPress or
   *                                other events, this value is 0.
   * @param aData                   The pointer which was specified at calling
   *                                the method of TextEventDispatcher.
   *                                For example, if you do:
   *                                |TextEventDispatcher->DispatchKeyboardEvent(
   *                                   eKeyDown, event, status, this);|
   *                                Then, aData of this method becomes |this|.
   *                                Finally, you can use it like:
   *                                |static_cast<NativeEventHandler*>(aData)|
   */
  NS_IMETHOD_(void) WillDispatchKeyboardEvent(
                      TextEventDispatcher* aTextEventDispatcher,
                      WidgetKeyboardEvent& aKeyboardEvent,
                      uint32_t aIndexOfKeypress,
                      void* aData) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(TextEventDispatcherListener,
                              NS_TEXT_INPUT_PROXY_LISTENER_IID)

} // namespace widget
} // namespace mozilla

#endif // #ifndef mozilla_textinputdispatcherlistener_h_
