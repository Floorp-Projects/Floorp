/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

package netscape.rdf;

import java.util.Enumeration;

/**
 * Interface to an RDF database.
 * <p>
 * Use RDFFactory to get an instance of an RDF database.
 * @see netscape.rdf.RDFFactory
 */
public interface IRDF { 
	/** 
     * Relate nodes <i>src</i> and <i>target</i> in the RDF database with <i>arcLabel</i>
     * This creates a positive relationship in the database: <i>target</i> is
     * related to <i>src</i> by the relationship named <i>arcLabel</i>.
     *
     * @param src
     *		the src node in the RDF graph
     * @param arcLabel
     * 		the label of the arc connecting </i>target</i> and <i>src</i>
     * @param target
     *		the target node in the RDF graph
     * @return <code>null</code> to indicate success or a resource describing 
     *		the failure
     */
    public Resource assert(Resource src, Resource arcLabel, Integer target);
    public Resource assert(Resource src, Resource arcLabel, String target);
    public Resource assert(Resource src, Resource arcLabel, Resource target);

    /** 
     * Relate nodes <i>src</i> and <i>target</i> in the RDF database with <i>arcLabel</i>
     * This creates a negative relationship in the database: <i>target</i> is not
     * related to <i>src</i> by the relationship named <i>arcLabel</i>.
     *
     * @param src
     *		the src node in the RDF graph
     * @param arcLabel
     *		the label of the arc connecting <i>target</i> and <i>src</i>
     * @param target
     *		the target node in the RDF graph
     * @return <code>null</code> to indicate success or a resource describing 
     *		the failure
     */
    public Resource assertFalse(Resource src, Resource arcLabel, Integer target);
    public Resource assertFalse(Resource src, Resource arcLabel, String target);
    public Resource assertFalse(Resource src, Resource arcLabel, Resource target);

    /** 
     * Make it so that as seen from the local layer, <i>src->arcLabel->target</i>is
     * not true. If <i>src->arcLabel->target</i> is true in a modifiable layer, modify it.
     * Otherwise add something in the local layer so that this gets blocked.
     *
     * @param src
     *		the src node in the graph
     * @param arcLabel
     *		the arc class to remove
     * @param target
     *		the target node in the graph
     * @return <code>null</code> to indicate success or a resource describing 
     *		the failure
     */
    public Resource unassert(Resource src, Resource arcLabel, Integer target);
    public Resource unassert(Resource src, Resource arcLabel, String target);
    public Resource unassert(Resource src, Resource arcLabel, Resource target);

    /** 
     * Returns true if <i>src->arcLabel->target</i> has the truth value <i>isTrue</i>
     *
     * @param src
     *		the src node in the graph
     * @param arcLabel
     *		the label of arc to query
     * @param target
     *		the target node in the graph
     * @param isTrue
     *		the truth value associated with <i>arcLabel</i>
     * @return true if assertion exists with <i>isTrue</i> truth value, false otherwise.
    */
    public boolean hasAssertion(Resource src, Resource arcLabel, Integer target, boolean isTrue);
    public boolean hasAssertion(Resource src, Resource arcLabel, String target, boolean isTrue);
    public boolean hasAssertion(Resource src, Resource arcLabel, Resource target, boolean isTrue);

    /** 
     * Returns an enumeration Resources which are the targets of <i>src</i> 
     * using arc <i>arcLabel</i> which have the truth value <i>isTrue</i>.
     *
     * @param src
     *		the src node in the graph
     * @param arcLabel
     *		the label of arc to query
     * @param isTrue
     *		the truth value associated with <i>arcLabel</i>
     * @return an enumeration of Resources, or NULL if none exist.
     */
    public Enumeration getTargets(Resource src, Resource arcLabel, boolean isTrue);

    /** 
     * Returns the first target of <i>src</i> using 
     * arc <i>arcLabel</i> which has the truth value <i>isTrue</i>.
     *
     * @param src
     *		the src node in the graph
     * @param arcLabel
     *		the label of arc to query
     * @param isTrue
     *		the truth value associated with <i>arcLabel</i>
     * @return an Object of type Resource, Integer or String or <code>null</code>
     */
    public Object getTarget(Resource src, Resource arcLabel, boolean isTrue);

    /** 
     * Returns an enumeration of Resources which are the sources
     * of <i>target</i> using arc <i>arcLabel</i> which have the truth value <i>isTrue</i>.
     *
     * @param arcLabel
     *		the label of arc to query
     * @param target
     *		the target node in the graph
     * @param isTrue
     *		the truth value associated with <i>arcLabel</i>
     * @return an enumeration of Resources, or <code>null</code>
     */
    public Enumeration getSources(Resource arcLabel, Integer target, boolean isTrue);
    public Enumeration getSources(Resource arcLabel, String target, boolean isTrue);
    public Enumeration getSources(Resource arcLabel, Resource target, boolean isTrue);

    /** 
     * Returns the Resource which is the first source of <i>target</i> using 
     * arc <i>arcLabel</i> which has the truth value <i>isTrue</i>.
     *
     * @param arcLabel
     *		the label of arc to query
     * @param target
     *		the target node in the graph
     * @param isTrue
     *		the truth value associated with <i>arcLabel</i>
     *
     * @return The requested Resource, or <code>null</code>
     */
    public Resource getSource(Resource arcLabel, Integer target, boolean isTrue);
    public Resource getSource(Resource arcLabel, String target, boolean isTrue);
    public Resource getSource(Resource arcLabel, Resource target, boolean isTrue);

    /** 
     * Clients call this method to register an object to be notified of events
     * happening within the RDF database.  The Vector <i>events</i> must contain subclasses
     * of RDFEvent, if it does not an instance of RDFNotifiableException is thrown
     *
     * @param obs
     *		the object to call when an event specified in <i>eventmask</i> occurs within the RDF database
     * @param events
     *		a vector of the RDFEvents of interest
     *
     * @return true if <i>obs</i> was successfully installed as a notifiable
     * @see netscape.rdf.RDFEvent
     */
    public boolean addNotifiable(IRDFNotifiable obs, RDFEvent event);

    /** 
     * Clients call this method to unregister an object from receiving notifications.
     * The Vector <i>events</i> must contain subclasses of RDFEvent, if it does not 
     * an instance of RDFNotifiableException is thrown
     *
     * @param obs
     *		the object to remove from the list of observers.
     * @param events
     *		the RDFEvents to stop listening to
     * @see netscape.rdf.RDFEvent
     */
    public void deleteNotifiable(IRDFNotifiable obs, RDFEvent event);

    /**
     * Returns the URL(s) used to create this RDF database.
     *
     * @return A vector of URLs.
     * @see netscape.rdf.RDFFactory
     */
    public String[] getRDFStores();

}
