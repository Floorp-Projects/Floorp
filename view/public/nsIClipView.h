/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIClipView_h___
#define nsIClipView_h___

#include "nsISupports.h"

// IID for the nsIClipView interface
#define NS_ICLIPVIEW_IID    \
{ 0x4cc36160, 0xd282, 0x11d2, \
{ 0x90, 0x67, 0x00, 0x60, 0xb0, 0xf1, 0x99, 0xa2 } }

/**
 * this is here so that we can query a view to see if it
 * exists for clipping.
 *
 * this is such a hack that i think it deserves a song 
 * of it's own: (from monty python)
 *
 *                    Philosophers song
 *
 *
 *            Immanual Kant was a real pissant
 *              Who was very rarely stable
 *
 *            Heidegger, Heidegger was a boozy beggar
 *              Who could think you under the table
 *
 *            David Hume could out consume
 *              Schopenhauer and Hegel
 *
 *            And Wittgenstein was a beery swine
 *              Who was just as schloshed as Schlegel
 *
 *             There's nothing Nietzche couldn't teach ya
 *              'Bout the raising of the wrist
 *             Socrates, himself, was permanently pissed
 *
 *
 *            John Stuart Mill, of his own free will
 *              On half a pint of shandy was particularly ill
 *
 *           Plato they say, could stick it away
 *              Half a crate of whiskey every day
 *
 *            Aristotle, Aristotle was a bugger for the bottle
 *              Hobbes was fond of his dram
 *
 *            And Rene' Descartes was a drunken fart
 *              "I drink, therefore I am"
 *
 *             Yes, Socrates, himself, is particularly missed
 *               A lovely little thinker
 *             But a bugger when he's pissed
 *
 */
class nsIClipView : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICLIPVIEW_IID)
};

#endif
