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
 * The Original Code is HTML Parser C++ Translator code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#ifndef nsAHtml5FragmentParser_h_
#define nsAHtml5FragmentParser_h_

#include "nsIParser.h"

/**
 * The purpose of this class is to avoid changing the signatures in nsIParser.h
 * after the Firefox 4.0 interface freeze. In later releases this can be folded
 * into nsIParser.h.
 */
class nsAHtml5FragmentParser : public nsIParser {
  public:

    /**
     * Invoke the fragment parsing algorithm (innerHTML).
     *
     * @param aSourceBuffer the string being set as innerHTML
     * @param aTargetNode the target container
     * @param aContextLocalName local name of context node
     * @param aContextNamespace namespace of context node
     * @param aQuirks true to make <table> not close <p>
     * @param aPreventScriptExecution true to prevent scripts from executing;
     * don't set to false when parsing into a target node that has been bound
     * to tree.
     */
    NS_IMETHOD ParseHtml5Fragment(const nsAString& aSourceBuffer,
                                  nsIContent* aTargetNode,
                                  nsIAtom* aContextLocalName,
                                  PRInt32 aContextNamespace,
                                  PRBool aQuirks,
                                  PRBool aPreventScriptExecution) = 0;

};

#endif // nsAHtml5FragmentParser_h_
