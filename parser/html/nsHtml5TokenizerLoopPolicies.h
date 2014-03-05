/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5TokenizerLoopPolicies_h
#define nsHtml5TokenizerLoopPolicies_h

/**
 * This policy does not report tokenizer transitions anywhere. To be used
 * when _not_ viewing source.
 */
struct nsHtml5SilentPolicy
{
  static const bool reportErrors = false;
  static int32_t transition(nsHtml5Highlighter* aHighlighter,
                            int32_t aState,
                            bool aReconsume,
                            int32_t aPos)
  {
    return aState;
  }
  static void completedNamedCharacterReference(nsHtml5Highlighter* aHighlighter)
  {
  }
};

/**
 * This policy reports the tokenizer transitions to a highlighter. To be used
 * when viewing source.
 */
struct nsHtml5ViewSourcePolicy
{
  static const bool reportErrors = true;
  static int32_t transition(nsHtml5Highlighter* aHighlighter,
                            int32_t aState,
                            bool aReconsume,
                            int32_t aPos)
  {
    return aHighlighter->Transition(aState, aReconsume, aPos);
  }
  static void completedNamedCharacterReference(nsHtml5Highlighter* aHighlighter)
  {
    aHighlighter->CompletedNamedCharacterReference();
  }
};

#endif // nsHtml5TokenizerLoopPolicies_h
