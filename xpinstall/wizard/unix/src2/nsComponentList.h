/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef _NS_COMPONENTLIST_H_
#define _NS_COMPONENTLIST_H_

#include "XIErrors.h"

class nsComponent;

class nsComponentList
{
public:
    nsComponentList();
    ~nsComponentList();

    /**     
     * GetHead
     *
     * Initializes the next ptr to the second item and 
     * returns the head item.
     *
     * @return mHead    the head of the singly-linked list
     */
    nsComponent *   GetHead();

    /**
     * GetNext
     * 
     * Returns the next available item. GetHead() has to have 
     * been called prior calling this and after the last time
     * the entire list was iterated over.
     *
     * @return mNext    the next available component
     */
    nsComponent *   GetNext(); 

    /**
     * GetTail
     *
     * Returns the tail item of the list.
     *
     * @return mTail    the tail item of the list
     */
    nsComponent *   GetTail();
    
    /**
     * GetLength
     *
     * Returns the number of components held by this list.
     *
     * @return mLength  the size of this list
     */
    int             GetLength();

    /**
     * GetLengthVisible
     * 
     * Returns the number of visible components held by this list.
     *
     * @return numVisible   the number of visible components
     */
    int             GetLengthVisible();

    /**
     * GetLengthSelected
     *
     * Returns the number of selected components held by this list.
     *
     * @return numSleected  the number of selected components
     */
    int             GetLengthSelected();

    /**
     * AddComponent
     *
     * Adds the supplied component to the list's tail.
     *
     * @param aComponent    the component to add
     * @return err          integer err code (zero means OK)
     */
    int             AddComponent(nsComponent *aComponent);

    /**
     * RemoveComponent
     *
     * Searches the list and removes the first component that
     * matches the supplied component.
     *
     * @param aComponent    the component to remove
     * @return err          integer error code (zero means OK)
     */
    int             RemoveComponent(nsComponent *aComponent);

    /**
     * GetCompByIndex
     *
     * Searches the list and returns the first component that
     * matches the supplied index.
     *
     * @param aIndex        the index of the component being sought
     * @return comp         the component matching the index
     */
    nsComponent     *GetCompByIndex(int aIndex);

    /**
     * GetCompByArchive
     * 
     * Searches the list and returns the first component that matches
     * the archive name supplied.
     * 
     * @param aArchive      the archive name of the component
     * @return comp         the component matching the archive
     */
    nsComponent     *GetCompByArchive(char *aArchive);

    /**
     * GetCompByShortDesc
     *
     * Searches the list and returns the first component that matches
     * the short description supplied.
     *
     * @param aShortDesc    the short description of the component
     * @return comp         the component matching the short description
     */
    nsComponent     *GetCompByShortDesc(char *aShortDesc);

    /**
     * GetFirstVisible
     *
     * Returns the first component that doesn't have the invisible
     * attribute set.
     *
     * @return comp     the first visible component in this list
     */
    nsComponent     *GetFirstVisible();

private:
    nsComponent     *mHead; 
    nsComponent     *mTail;
    nsComponent     *mNext;
    int             mLength;
};

#endif /* _NS_COMPONENTLIST_H_ */
