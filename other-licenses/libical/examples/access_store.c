

void acess_cap(void) {
    
    /* Note, all routines that are prefixed with "caller_" are
       implemented by the caller of libical */
    
/* 1 Open a connection to a store.  */

    /* The caller is responsible for getting a socket to the server
       and negotiating the first stages of the CAP exchange. These can
       be fairly complex and varied for different operating systems,
       local vs remote usage, and for different authentication
       schemes, so the API does not try to simplify them.  */

    int sock = caller_create_socket_to_server();
    icalcstp *cstp = icalcstp_new(0,sock,sock);
    
    caller_authenticate(cstp);
    
    icalcsdb *csdb = icalcsdb_new(cstp);

/* 2 Create a new calendar for which user Bill can read and user Mary can
read and write. See CAP draft 7.2.1.1.1. for the text of this example*/

    /* This case requires setting up a TARGET, multiple OWNERs and
       multiple VCARs, so it creates a component and uses CSTP that
       than the CSDB interface.

    icalcomponent *create = icalcaputil_new_create();

    icalcomponent_add_property(create,
			       icalproperty_new_target(
				   strdup("cap://cal.example.com/relcal8")
				   ));

    icalcomponent *cal = 
	icalcomponent_vanew_vcalendar(
	    icalproperty_new_relcalid(strdup("relcalid")),
	    icalproperty_new_name(strdup("Bill & Mary's cal")),
	    icalproperty_new_owner(strdup("bill")),
	    icalproperty_new_owner(strdup("mary")),
	    icalproperty_new_calmaster(strdup("mailto:bill@example.com")),
	    icalcomponent_vanew_vcar(
		icalproperty_new_grant(strdup("UPN=bill;ACTION=*;OBJECT=*")),
		icalproperty_new_grant(strdup("UPN=bill;ACTION=*;OBJECT=*"))
		0)
	    0);

    error = icalcomponent_add_component(create,cal);

    /* Send the data */
    error = icalcstp_senddata(cstp,10,create);


    /* Get the response */
    icalcstp_response response = icalcstp_get_first_response(cstp);

    /* Do something with the response*/

    if(icalenum_reqstat_major(response.code) != 2){
	/* do something with the error */
    }

    icalcomponent_free(create);
    

/* 3 Create several new calendars */

    /* Same as #2, but insert more TARGET properties and read more responses*/

/* 4 Delete a calendar */

    error = icalcsdb_delete(csdb,"uid12345-example.com");

/* 5 Change the calid of a calendar */

    erorr = icalcsdb_move(csdb,"uid12345-old-example.com",
			  "uid12345-new-example.com");


/* 6 Delete all calendars belonging to user bob */

    icalproperty *p;
    /* First expand bob's UPN into a set of CALIDs */
    icalcomponent *calids = icalcsdb_expand_upn("bob@example.com");

    /* Then, create a message to delete all of them */
    icalcomponent *delete = icalcaputil_new_create();

    
    for(p = icalcomponent_get_first_property(calids,ICAL_CALID_PROPERTY);
	p != 0;
	p = icalcomponent_get_next_property(calids,ICAL_CALID_PROPERTY)){

	char* = icalproperty_get_calid(p);

	icalcomponent_add_target(delete,p);

    }

    /* Send the message */

    error = icalcstp_senddata(cstp,10,delete);

    /* Finally, read the responses */

    for(response = icalcstp_get_first_response(cstp);
	response.code !=  ICAL_UNKNOWN_STATUS;
	response = icalcstp_get_next_response(cstp)){
	
	if(icalenum_reqstat_major(response.code) != 2){
	    /* do something with the error */
	}
    }
	

/* 7 Get three new UIDs from the store */

    /* libical owns the returned memory. Copy before using */
    char* uid1 = icalcsdb_generateuid(csdb);
    char* uid2 = icalcsdb_generateuid(csdb);
    char* uid3 = icalcsdb_generateuid(csdb);

/* 8 Store a new VEVENT in the store.  */
    
    /* Very similar to case #2 */

/* 9 Find all components for which the LOCATION is "West Conference
Room" and change them to "East Conference Room"  */

    icalcomponent *modify = icalcaputil_new_modify();

    icalcaputil_modify_add_old_prop(modify,
				    icalproperty_new_location(
					strdup("West Conference Room")));

    icalcaputil_modify_add_new_prop(modify,
				    icalproperty_new_location(
					strdup("East Conference Room")));

    icalcaputil_add_target(modify,"relcal2");

    /* Send the component */
    error = icalcstp_senddata(cstp,10,delete);

   /* Get the response */
    icalcstp_response response = icalcstp_get_first_response(cstp);

    /* Do something with the response*/

    if(icalenum_reqstat_major(response.code) != 2){
	/* do something with the error */
    }

    icalcomponent_free(modify);
    
/* 10 Find the component with UID X and add a GEO property to it.  */


    icalcomponent *modify = icalcaputil_new_modify();

    icalcaputil_modify_add_query(modify,
				 "SELECT UID FROM VEVENT WHERE UID = 'X'");   

    icalcaputil_modify_add_new_prop(modify,
				    icalproperty_new_geo(
					strdup("-117;32")));

    icalcaputil_add_target(modify,"relcal2");

    /* Send the component */
    error = icalcstp_senddata(cstp,10,delete);

   /* Get the response */
    icalcstp_response response = icalcstp_get_first_response(cstp);

    /* Do something with the response*/

    if(icalenum_reqstat_major(response.code) != 2){
	/* do something with the error */
    }

    icalcomponent_free(modify);
    

/* 11 Delete all VEVENTS which have a METHOD that is not CREATED */


/* 12 Retrieve all VEVENTS which have a METHOD that is not CREATED */

    /* Nearly the same at #11 */

/* 13 Retrieve the capabilities of the store */

/* 14 Retrieve/Modify/Add/Delete properties of a store */

/* 15 Retrieve/Modify/Add/Delete VCARs of a store */

/* 16 Retrieve/Modify/Add/Delete VTIMEZONEs of a store */

/* 17 Retrieve/Modify/Add/Delete properties of a calendar */

/* 18 Retrieve/Modify/Add/Delete VCARs of a calendar */

/* 19 Retrieve/Modify/Add/Delete VTIMEZONEs of a calendar */

/* 20 Translate a CALID into one or more UPNs */

/* 21 Expand a group UPN into all of the members of the group */
