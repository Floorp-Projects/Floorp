/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is HTML5 View Source code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsHtml5TokenizerLoopPolicies_h_
#define nsHtml5TokenizerLoopPolicies_h_

/**
 * This policy does not report tokenizer transitions anywhere. To be used
 * when _not_ viewing source.
 */
struct nsHtml5SilentPolicy
{
  static const bool reportErrors = false;
  static PRInt32 transition(nsHtml5Highlighter* aHighlighter,
                            PRInt32 aState,
                            bool aReconsume,
                            PRInt32 aPos) {
    return aState;
  }
  static void completedNamedCharacterReference(nsHtml5Highlighter* aHighlighter) {
  }
};

/**
 * This policy reports the tokenizer transitions to a highlighter. To be used
 * when viewing source.
 */
struct nsHtml5ViewSourcePolicy
{
  static const bool reportErrors = true;
  static PRInt32 transition(nsHtml5Highlighter* aHighlighter,
                            PRInt32 aState,
                            bool aReconsume,
                            PRInt32 aPos) {
    return aHighlighter->Transition(aState, aReconsume, aPos);
  }
  static void completedNamedCharacterReference(nsHtml5Highlighter* aHighlighter) {
    aHighlighter->CompletedNamedCharacterReference();
  }
};

#endif // nsHtml5TokenizerLoopPolicies_h_
