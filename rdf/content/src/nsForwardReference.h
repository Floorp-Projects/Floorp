/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsForwardReference_h__
#define nsForwardReference_h__

class nsForwardReference
{
protected:
    nsForwardReference() {}

public:
    virtual ~nsForwardReference() {}

    /**
     * Priority codes returned from GetPriority()
     */
    enum State {
        /** A dummy marker, used to indicate unstarted resolution */
        eStart,

        /** The initial pass, after which the content model will be
            fully built */
        eConstruction,

        /** A second pass, after which all 'magic attribute' hookup
            will have been performed */
        eHookup,

        /** A dummy marker, used in kPasses to indicate termination */
        eDone
    };

    /**
     * Forward references are categorized by 'priority', and all
     * forward references in a higher priority are resolved before any
     * reference in a lower priority. This variable specifies this
     * ordering. The last Priority is guaranteed to be eDone.
     */
    static const State kPasses[];

    /**
     * Get the state in which the forward reference should be resolved.
     * 'eConstruction' references are all resolved before 'eHookup' references
     * are resolved.
     *
     * @return the State in which the reference needs to be resolved
     */
    virtual State GetState() = 0;

    /**
     * Result codes returned from Resolve()
     */
    enum Result {
        /** Resolution succeeded, I'm done. */
        eResolve_Succeeded,

        /** Couldn't resolve, but try me later. */
        eResolve_Later,

        /** Something bad happened, don't try again. */
        eResolve_Error
    };

    /**
     * Attempt to resolve the forward reference.
     *
     * @return a Result that tells the resolver how to treat
     * the reference.
     */
    virtual Result Resolve() = 0;
};

#endif // nsForwardReference_h__
