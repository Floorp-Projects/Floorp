/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
private:
  NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;
  NS_IMETHOD_(nsrefcnt) Release(void) = 0;
};

#endif
