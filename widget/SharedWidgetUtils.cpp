/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidgetUtils.h"

#include "mozilla/TextEvents.h"

#include "nsIBaseWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"

namespace mozilla {
namespace widget {

// static
void
WidgetUtils::Shutdown()
{
  WidgetKeyboardEvent::Shutdown();
}

// static
already_AddRefed<nsIWidget>
WidgetUtils::DOMWindowToWidget(nsIDOMWindow *aDOMWindow)
{
  nsCOMPtr<nsIWidget> widget;

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aDOMWindow);
  if (window) {
    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(window->GetDocShell()));

    while (!widget && baseWin) {
      baseWin->GetParentWidget(getter_AddRefs(widget));
      if (!widget) {
        nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(baseWin));
        if (!docShellAsItem)
          return nullptr;

        nsCOMPtr<nsIDocShellTreeItem> parent;
        docShellAsItem->GetParent(getter_AddRefs(parent));

        window = do_GetInterface(parent);
        if (!window)
          return nullptr;

        baseWin = do_QueryInterface(window->GetDocShell());
      }
    }
  }

  return widget.forget();
}

// static
uint32_t
WidgetUtils::ComputeKeyCodeFromChar(uint32_t aCharCode)
{
  if (aCharCode >= 'A' && aCharCode <= 'Z') {
    return aCharCode - 'A' + NS_VK_A;
  }
  if (aCharCode >= 'a' && aCharCode <= 'z') {
    return aCharCode - 'a' + NS_VK_A;
  }
  if (aCharCode >= '0' && aCharCode <= '9') {
    return aCharCode - '0' + NS_VK_0;
  }
  switch (aCharCode) {
    case ' ': return NS_VK_SPACE;
    case '\t': return NS_VK_TAB;
    case ':': return NS_VK_COLON;
    case ';': return NS_VK_SEMICOLON;
    case '<': return NS_VK_LESS_THAN;
    case '=': return NS_VK_EQUALS;
    case '>': return NS_VK_GREATER_THAN;
    case '?': return NS_VK_QUESTION_MARK;
    case '@': return NS_VK_AT;
    case '^': return NS_VK_CIRCUMFLEX;
    case '!': return NS_VK_EXCLAMATION;
    case '"': return NS_VK_DOUBLE_QUOTE;
    case '#': return NS_VK_HASH;
    case '$': return NS_VK_DOLLAR;
    case '%': return NS_VK_PERCENT;
    case '&': return NS_VK_AMPERSAND;
    case '_': return NS_VK_UNDERSCORE;
    case '(': return NS_VK_OPEN_PAREN;
    case ')': return NS_VK_CLOSE_PAREN;
    case '*': return NS_VK_ASTERISK;
    case '+': return NS_VK_PLUS;
    case '|': return NS_VK_PIPE;
    case '-': return NS_VK_HYPHEN_MINUS;
    case '{': return NS_VK_OPEN_CURLY_BRACKET;
    case '}': return NS_VK_CLOSE_CURLY_BRACKET;
    case '~': return NS_VK_TILDE;
    case ',': return NS_VK_COMMA;
    case '.': return NS_VK_PERIOD;
    case '/': return NS_VK_SLASH;
    case '`': return NS_VK_BACK_QUOTE;
    case '[': return NS_VK_OPEN_BRACKET;
    case '\\': return NS_VK_BACK_SLASH;
    case ']': return NS_VK_CLOSE_BRACKET;
    case '\'': return NS_VK_QUOTE;
  }
  return 0;
}

// static
void
WidgetUtils::GetLatinCharCodeForKeyCode(uint32_t aKeyCode,
                                        bool aIsCapsLock,
                                        uint32_t* aUnshiftedCharCode,
                                        uint32_t* aShiftedCharCode)
{
  MOZ_ASSERT(aUnshiftedCharCode && aShiftedCharCode,
             "aUnshiftedCharCode and aShiftedCharCode must not be NULL");

  if (aKeyCode >= NS_VK_A && aKeyCode <= NS_VK_Z) {
    *aUnshiftedCharCode = *aShiftedCharCode = aKeyCode;
    if (aIsCapsLock) {
      *aShiftedCharCode += 0x20;
    } else {
      *aUnshiftedCharCode += 0x20;
    }
    return;
  }

  // aShiftedCharCode must be zero for non-alphabet keys.
  *aShiftedCharCode = 0;

  if (aKeyCode >= NS_VK_0 && aKeyCode <= NS_VK_9) {
    *aUnshiftedCharCode = aKeyCode;
    return;
  }

  switch (aKeyCode) {
    case NS_VK_SPACE:               *aUnshiftedCharCode = ' '; break;
    case NS_VK_COLON:               *aUnshiftedCharCode = ':'; break;
    case NS_VK_SEMICOLON:           *aUnshiftedCharCode = ';'; break;
    case NS_VK_LESS_THAN:           *aUnshiftedCharCode = '<'; break;
    case NS_VK_EQUALS:              *aUnshiftedCharCode = '='; break;
    case NS_VK_GREATER_THAN:        *aUnshiftedCharCode = '>'; break;
    case NS_VK_QUESTION_MARK:       *aUnshiftedCharCode = '?'; break;
    case NS_VK_AT:                  *aUnshiftedCharCode = '@'; break;
    case NS_VK_CIRCUMFLEX:          *aUnshiftedCharCode = '^'; break;
    case NS_VK_EXCLAMATION:         *aUnshiftedCharCode = '!'; break;
    case NS_VK_DOUBLE_QUOTE:        *aUnshiftedCharCode = '"'; break;
    case NS_VK_HASH:                *aUnshiftedCharCode = '#'; break;
    case NS_VK_DOLLAR:              *aUnshiftedCharCode = '$'; break;
    case NS_VK_PERCENT:             *aUnshiftedCharCode = '%'; break;
    case NS_VK_AMPERSAND:           *aUnshiftedCharCode = '&'; break;
    case NS_VK_UNDERSCORE:          *aUnshiftedCharCode = '_'; break;
    case NS_VK_OPEN_PAREN:          *aUnshiftedCharCode = '('; break;
    case NS_VK_CLOSE_PAREN:         *aUnshiftedCharCode = ')'; break;
    case NS_VK_ASTERISK:            *aUnshiftedCharCode = '*'; break;
    case NS_VK_PLUS:                *aUnshiftedCharCode = '+'; break;
    case NS_VK_PIPE:                *aUnshiftedCharCode = '|'; break;
    case NS_VK_HYPHEN_MINUS:        *aUnshiftedCharCode = '-'; break;
    case NS_VK_OPEN_CURLY_BRACKET:  *aUnshiftedCharCode = '{'; break;
    case NS_VK_CLOSE_CURLY_BRACKET: *aUnshiftedCharCode = '}'; break;
    case NS_VK_TILDE:               *aUnshiftedCharCode = '~'; break;
    case NS_VK_COMMA:               *aUnshiftedCharCode = ','; break;
    case NS_VK_PERIOD:              *aUnshiftedCharCode = '.'; break;
    case NS_VK_SLASH:               *aUnshiftedCharCode = '/'; break;
    case NS_VK_BACK_QUOTE:          *aUnshiftedCharCode = '`'; break;
    case NS_VK_OPEN_BRACKET:        *aUnshiftedCharCode = '['; break;
    case NS_VK_BACK_SLASH:          *aUnshiftedCharCode = '\\'; break;
    case NS_VK_CLOSE_BRACKET:       *aUnshiftedCharCode = ']'; break;
    case NS_VK_QUOTE:               *aUnshiftedCharCode = '\''; break;
    default:                        *aUnshiftedCharCode = 0; break;
  }
}

} // namespace widget
} // namespace mozilla
