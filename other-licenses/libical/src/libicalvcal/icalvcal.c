/*======================================================================
  FILE: icalvcal.c
  CREATOR: eric 25 May 00
  
  $Id: icalvcal.c,v 1.4 2002/03/14 15:17:59 mikep%oeone.com Exp $


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalvcal.c



  The icalvcal_convert routine calls icalvcal_traverse_objects to do
  its work.s his routine steps through through all of the properties
  and components of a VObject. For each name of a property or a
  component, icalvcal_traverse_objects looks up the name in
  conversion_table[]. This table indicates wether the name is of a
  component or a property, lists a routine to handle conversion, and
  has extra data for the conversion.

  The conversion routine will create new iCal components or properties
  and add them to the iCal component structure. 

  The most common conversion routine is dc_prop. This routine converts
  properties for which the text representation of the vCal component
  is identical the iCal representation.

  ======================================================================*/

#include "icalvcal.h"
#include <string.h>   

#ifdef WIN32
#define snprintf	_snprintf
#define strcasecmp	stricmp
#endif

enum datatype {
    COMPONENT,
    PROPERTY,
    PARAMETER,
    UNSUPPORTED
};


struct conversion_table_struct {
	char* vcalname;
	enum datatype type;
	void* (*conversion_func)(int icaltype, VObject *o);
	int icaltype;
};

struct conversion_table_struct conversion_table[];
void* dc_prop(int icaltype, VObject *object);

static void icalvcal_traverse_objects(VObject *object,icalcomponent* last_comp,
			     icalproperty* last_prop)
{
    VObjectIterator iterator;
    char* name = "[No Name]";
    icalcomponent* subc = 0;
    int i;
    
    if ( vObjectName(object)== 0){
	printf("ERROR, object has no name");
	assert(0);
	return;
    }

    name = (char*)vObjectName(object);

    /* Lookup this object in the conversion table */
    for (i = 0; conversion_table[i].vcalname != 0; i++){
	if(strcmp(conversion_table[i].vcalname, name) == 0){
	    break;
	}
    }
    
    /* Did not find the object. It may be an X-property, or an unknown
       property */
    if (conversion_table[i].vcalname == 0){

	/* Handle X properties */
	if(strncmp(name, "X-",2) == 0){
	   icalproperty* prop = (icalproperty*)dc_prop(ICAL_X_PROPERTY,object);
	   icalproperty_set_x_name(prop,name);
	   icalcomponent_add_property(last_comp,prop);
	} else {
	    assert(0);
	    return;
	}

    } else {
	
	/* The vCal property is in the table, and it is not an X
           property, so try to convert it to an iCal component,
           property or parameter. */
	
	switch(conversion_table[i].type){
	    
	    
	    case COMPONENT: {
		subc = 
		    (icalcomponent*)(conversion_table[i].conversion_func
				     (conversion_table[i].icaltype,
				      object));
		
		icalcomponent_add_component(last_comp,subc);
		
		assert(subc!=0);
		
		break;
	    }
	    
	    case PROPERTY: {
		
		if (vObjectValueType(object) &&
		    conversion_table[i].conversion_func != 0 ) {
		    
		    icalproperty* prop = 
			(icalproperty*)(conversion_table[i].conversion_func
					(conversion_table[i].icaltype,
					 object));		
		    
		    icalcomponent_add_property(last_comp,prop);
		    last_prop = prop;
		    
		}
		break;
	    }
	    
	    case PARAMETER: {
		break;
	    }
	    
	    case UNSUPPORTED: {

		/* If the property is listed as UNSUPPORTED, insert a
                   X_LIC_ERROR property to note this fact. */

		char temp[1024];
		char* message = "Unsupported vCal property";
		icalparameter *error_param;
		icalproperty *error_prop;

		snprintf(temp,1024,"%s: %s",message,name);
		
		error_param = icalparameter_new_xlicerrortype(
		    ICAL_XLICERRORTYPE_UNKNOWNVCALPROPERROR
		    );

		error_prop = icalproperty_new_xlicerror(temp);
		icalproperty_add_parameter(error_prop, error_param);

		icalcomponent_add_property(last_comp,error_prop);

		break;
	    }
	}
    }


    /* Now, step down into the next vCalproperty */

    initPropIterator(&iterator,object);
    while (moreIteration(&iterator)) {
	VObject *eachProp = nextVObject(&iterator);

	/* If 'object' is a component, then the next traversal down
           should use it as the 'last_comp' */

	if(subc!=0){
	    icalvcal_traverse_objects(eachProp,subc,last_prop);
	
	} else {
	    icalvcal_traverse_objects(eachProp,last_comp,last_prop);
	}
    }
}

icalcomponent* icalvcal_convert(VObject *object){

   char* name =  (char*)vObjectName(object);
   icalcomponent* container = icalcomponent_new(ICAL_XROOT_COMPONENT);
   icalcomponent* root;

   icalerror_check_arg_rz( (object!=0),"Object");

   /* The root object must be a VCALENDAR */
   if(*name==0 || strcmp(name,VCCalProp) != 0){
       return 0; /* HACK. Should return an error */
   }


   icalvcal_traverse_objects(object,container,0);
   
   /* HACK. I am using the extra 'container' component because I am
      lazy. I know there is a way to get rid of it, but I did not care
      to find it. */

   root = icalcomponent_get_first_component(container,ICAL_ANY_COMPONENT);

   icalcomponent_remove_component(container, root);
   icalcomponent_free(container);

   return root;

}

/* comp() is useful for most components, but alarm, daylight and
 * timezone are different. In vcal, they are properties, and in ical,
 * they are components. Although because of the way that vcal treats
 * everything as a property, alarm_comp() daylight_comp() and
 * timezone_comp() may not really be necessary, I think it would be
 * easier to use them. */

void* comp(int icaltype, VObject *o)
{
    icalcomponent_kind kind = (icalcomponent_kind)icaltype;

    icalcomponent* c = icalcomponent_new(kind);

    return (void* )c;
}

void* alarm_comp(int icaltype, VObject *o)
{
    icalcomponent_kind kind = (icalcomponent_kind)icaltype;

    icalcomponent* c = icalcomponent_new(kind);

    return (void*)c;
}

void* daylight_comp(int icaltype, VObject *o)
{
    icalcomponent_kind kind = (icalcomponent_kind)icaltype;

    icalcomponent* c = icalcomponent_new(kind);
 
    return (void*)c;
}

void* timezone_comp(int icaltype, VObject *o)
{
    icalcomponent_kind kind = (icalcomponent_kind)icaltype;

    icalcomponent* c = icalcomponent_new(kind);

    return (void*)c;
}


/* These #defines indicate conversion routines that are not defined yet. */

#define categories_prop 0
#define transp_prop 0   
#define status_prop 0   

#define parameter 0   
#define rsvp_parameter 0   



/* directly convertable property. The string representation of vcal is
   the same as ical */

void* dc_prop(int icaltype, VObject *object)
{
    icalproperty_kind kind = (icalproperty_kind)icaltype;
    icalproperty *prop; 
    icalvalue *value;
    icalvalue_kind value_kind;
    char *s,*t=0;

    prop = icalproperty_new(kind);

    value_kind = 
	icalenum_property_kind_to_value_kind(
	    icalproperty_isa(prop));


    switch (vObjectValueType(object)) {
	case VCVT_USTRINGZ: {
	    s = t = fakeCString(vObjectUStringZValue(object));
	    break;
	}
	case VCVT_STRINGZ: {
	    s = (char*)vObjectStringZValue(object);
	    break;
	}
    }

    value = icalvalue_new_from_string(value_kind,s);

    if(t!=0){
	deleteStr(t);
    }

    icalproperty_set_value(prop,value);

    return (void*)prop;
}


/* My extraction program screwed up, so this table does not have all
of the vcal properties in it. I didn't feel like re-doing the entire
table, so you'll have to find the missing properties the hard way --
the code will assert */

struct conversion_table_struct conversion_table[] = 
{
{VCCalProp,            COMPONENT, comp,         ICAL_VCALENDAR_COMPONENT},
{VCTodoProp,           COMPONENT, comp,         ICAL_VTODO_COMPONENT},
{VCEventProp,          COMPONENT, comp,         ICAL_VEVENT_COMPONENT},
{VCAAlarmProp,         COMPONENT, alarm_comp,   ICAL_XAUDIOALARM_COMPONENT},
{VCDAlarmProp,         COMPONENT, alarm_comp,   ICAL_XDISPLAYALARM_COMPONENT},
{VCMAlarmProp,         COMPONENT, alarm_comp,   ICAL_XEMAILALARM_COMPONENT},
{VCPAlarmProp,         COMPONENT, alarm_comp,   ICAL_XPROCEDUREALARM_COMPONENT},
{VCDayLightProp,       COMPONENT, daylight_comp,0},
{VCTimeZoneProp,       COMPONENT, timezone_comp, ICAL_VTIMEZONE_COMPONENT},
{VCProdIdProp,         PROPERTY,  dc_prop,       ICAL_PRODID_PROPERTY},
{VCClassProp,          PROPERTY,  dc_prop,       ICAL_CLASS_PROPERTY},
{VCDCreatedProp,       PROPERTY,  dc_prop,       ICAL_CREATED_PROPERTY},
{VCDescriptionProp,    PROPERTY,  dc_prop,       ICAL_DESCRIPTION_PROPERTY},
{VCAttendeeProp,       PROPERTY,  dc_prop,       ICAL_ATTENDEE_PROPERTY},
{VCCategoriesProp,     PROPERTY,  categories_prop,ICAL_CATEGORIES_PROPERTY},
{VCDTendProp,          PROPERTY,  dc_prop,       ICAL_DTEND_PROPERTY},
{VCDTstartProp,        PROPERTY,  dc_prop,       ICAL_DTSTART_PROPERTY},
{VCDueProp,            PROPERTY,  dc_prop,       ICAL_DUE_PROPERTY},
{VCLocationProp,       PROPERTY,  dc_prop,       ICAL_LOCATION_PROPERTY},
{VCSummaryProp,        PROPERTY,  dc_prop,       ICAL_SUMMARY_PROPERTY},
{VCVersionProp,        PROPERTY,  dc_prop,       ICAL_VERSION_PROPERTY},
{VCTranspProp,         PROPERTY,  transp_prop,   ICAL_TRANSP_PROPERTY},
{VCUniqueStringProp,   PROPERTY,  dc_prop,       ICAL_UID_PROPERTY},
{VCURLProp,            PROPERTY,  dc_prop,       ICAL_URL_PROPERTY},
{VCLastModifiedProp,   PROPERTY,  dc_prop,       ICAL_LASTMODIFIED_PROPERTY},
{VCSequenceProp,       PROPERTY,  dc_prop,       ICAL_SEQUENCE_PROPERTY},
{VCPriorityProp,       PROPERTY,  dc_prop,       ICAL_PRIORITY_PROPERTY},
{VCStatusProp,         PROPERTY,  status_prop,   ICAL_STATUS_PROPERTY},
{VCRSVPProp,           UNSUPPORTED, rsvp_parameter,ICAL_RSVP_PARAMETER  },
{VCEncodingProp,       UNSUPPORTED, parameter,     ICAL_ENCODING_PARAMETER},
{VCRoleProp,           UNSUPPORTED, parameter,     ICAL_ROLE_PARAMETER},
{VCStatusProp,         UNSUPPORTED, parameter,     ICAL_STATUS_PROPERTY},
{VCQuotedPrintableProp,UNSUPPORTED,0,            0},
{VC7bitProp,           UNSUPPORTED,0,            0},
{VC8bitProp,           UNSUPPORTED,0,            0},
{VCAdditionalNamesProp,UNSUPPORTED,0,            0},
{VCAdrProp,            UNSUPPORTED,0,            0},
{VCAgentProp,          UNSUPPORTED,0,            0},
{VCAIFFProp,           UNSUPPORTED,0,            0},
{VCAOLProp,            UNSUPPORTED,0,            0},
{VCAppleLinkProp,      UNSUPPORTED,0,            0},
{VCAttachProp,         UNSUPPORTED,0,            0},
{VCATTMailProp,        UNSUPPORTED,0,            0},
{VCAudioContentProp,   UNSUPPORTED,0,            0},
{VCAVIProp,            UNSUPPORTED,0,            0},
{VCBase64Prop,         UNSUPPORTED,0,            0},
{VCBBSProp,            UNSUPPORTED,0,            0},
{VCBirthDateProp,      UNSUPPORTED,0,            0},
{VCBMPProp,            UNSUPPORTED,0,            0},
{VCBodyProp,           UNSUPPORTED,0,            0},
{VCCaptionProp,        UNSUPPORTED,0,            0},
{VCCarProp,            UNSUPPORTED,0,            0},
{VCCellularProp,       UNSUPPORTED,0,            0},
{VCCGMProp,            UNSUPPORTED,0,            0},
{VCCharSetProp,        UNSUPPORTED,0,            0},
{VCCIDProp,            UNSUPPORTED,0,            0},
{VCCISProp,            UNSUPPORTED,0,            0},
{VCCityProp,           UNSUPPORTED,0,            0},
{VCCommentProp,        UNSUPPORTED,0,            0},
{VCCompletedProp,      UNSUPPORTED,0,            0},
{VCCountryNameProp,    UNSUPPORTED,0,            0},
{VCDataSizeProp,       UNSUPPORTED,0,            0},
{VCDeliveryLabelProp,  UNSUPPORTED,0,            0},
{VCDIBProp,            UNSUPPORTED,0,            0},
{VCDisplayStringProp,  UNSUPPORTED,0,            0},
{VCDomesticProp,       UNSUPPORTED,0,            0},
{VCEmailAddressProp,   UNSUPPORTED,0,            0},
{VCEndProp,            UNSUPPORTED,0,            0},
{VCEWorldProp,         UNSUPPORTED,0,            0},
{VCExNumProp,          UNSUPPORTED,0,            0},
{VCExpDateProp,        UNSUPPORTED,0,            0},
{VCExpectProp,         UNSUPPORTED,0,            0},
{VCFamilyNameProp,     UNSUPPORTED,0,            0},
{VCFaxProp,            UNSUPPORTED,0,            0},
{VCFullNameProp,       UNSUPPORTED,0,            0},
{VCGeoProp,            UNSUPPORTED,0,            0},
{VCGeoLocationProp,    UNSUPPORTED,0,            0},
{VCGIFProp,            UNSUPPORTED,0,            0},
{VCGivenNameProp,      UNSUPPORTED,0,            0},
{VCGroupingProp,       UNSUPPORTED,0,            0},
{VCHomeProp,           UNSUPPORTED,0,            0},
{VCIBMMailProp,        UNSUPPORTED,0,            0},
{VCInlineProp,         UNSUPPORTED,0,            0},
{VCInternationalProp,  UNSUPPORTED,0,            0},
{VCInternetProp,       UNSUPPORTED,0,            0},
{VCISDNProp,           UNSUPPORTED,0,            0},
{VCJPEGProp,           UNSUPPORTED,0,            0},
{VCLanguageProp,       UNSUPPORTED,0,            0},
{VCLastRevisedProp,    UNSUPPORTED,0,            0},
{VCLogoProp,           UNSUPPORTED,0,            0},
{VCMailerProp,         UNSUPPORTED,0,            0},
{VCMCIMailProp,        UNSUPPORTED,0,            0},
{VCMessageProp,        UNSUPPORTED,0,            0},
{VCMETProp,            UNSUPPORTED,0,            0},
{VCModemProp,          UNSUPPORTED,0,            0},
{VCMPEG2Prop,          UNSUPPORTED,0,            0},
{VCMPEGProp,           UNSUPPORTED,0,            0},
{VCMSNProp,            UNSUPPORTED,0,            0},
{VCNamePrefixesProp,   UNSUPPORTED,0,            0},
{VCNameProp,           UNSUPPORTED,0,            0},
{VCNameSuffixesProp,   UNSUPPORTED,0,            0},
{VCNoteProp,           UNSUPPORTED,0,            0},
{VCOrgNameProp,        UNSUPPORTED,0,            0},
{VCOrgProp,            UNSUPPORTED,0,            0},
{VCOrgUnit2Prop,       UNSUPPORTED,0,            0},
{VCOrgUnit3Prop,       UNSUPPORTED,0,            0},
{VCOrgUnit4Prop,       UNSUPPORTED,0,            0},
{VCOrgUnitProp,        UNSUPPORTED,0,            0},
{VCPagerProp,          UNSUPPORTED,0,            0},
{VCParcelProp,         UNSUPPORTED,0,            0},
{VCPartProp,           UNSUPPORTED,0,            0},
{VCPCMProp,            UNSUPPORTED,0,            0},
{VCPDFProp,            UNSUPPORTED,0,            0},
{VCPGPProp,            UNSUPPORTED,0,            0},
{VCPhotoProp,          UNSUPPORTED,0,            0},
{VCPICTProp,           UNSUPPORTED,0,            0},
{VCPMBProp,            UNSUPPORTED,0,            0},
{VCPostalBoxProp,      UNSUPPORTED,0,            0},
{VCPostalCodeProp,     UNSUPPORTED,0,            0},
{VCPostalProp,         UNSUPPORTED,0,            0},
{VCPowerShareProp,     UNSUPPORTED,0,            0},
{VCPreferredProp,      UNSUPPORTED,0,            0},
{VCProcedureNameProp,  UNSUPPORTED,0,            0},
{VCProdigyProp,        UNSUPPORTED,0,            0},
{VCPronunciationProp,  UNSUPPORTED,0,            0},
{VCPSProp,             UNSUPPORTED,0,            0},
{VCPublicKeyProp,      UNSUPPORTED,0,            0},
{VCQPProp,             UNSUPPORTED,0,            0},
{VCQuickTimeProp,      UNSUPPORTED,0,            0},
{VCRDateProp,          UNSUPPORTED,0,            0},
{VCRegionProp,         UNSUPPORTED,0,            0},
{VCRepeatCountProp,    UNSUPPORTED,0,            0},
{VCResourcesProp,      UNSUPPORTED,0,            0},
{VCRNumProp,           UNSUPPORTED,0,            0},
{VCRRuleProp,          UNSUPPORTED,0,            0},
{VCRunTimeProp,        UNSUPPORTED,0,            0},
{VCSnoozeTimeProp,     UNSUPPORTED,0,            0},
{VCStartProp,          UNSUPPORTED,0,            0},
{VCStreetAddressProp,  UNSUPPORTED,0,            0},
{VCSubTypeProp,        UNSUPPORTED,0,            0},
{VCTelephoneProp,      UNSUPPORTED,0,            0},
{VCTIFFProp,           UNSUPPORTED,0,            0},
{VCTitleProp,          UNSUPPORTED,0,            0},
{VCTLXProp,            UNSUPPORTED,0,            0},
{VCURLValueProp,       UNSUPPORTED,0,            0},
{VCValueProp,          UNSUPPORTED,0,            0},
{VCVideoProp,          UNSUPPORTED,0,            0},
{VCVoiceProp,          UNSUPPORTED,0,            0},
{VCWAVEProp,           UNSUPPORTED,0,            0},
{VCWMFProp,            UNSUPPORTED,0,            0},
{VCWorkProp,           UNSUPPORTED,0,            0},
{VCX400Prop,           UNSUPPORTED,0,            0},
{VCX509Prop,           UNSUPPORTED,0,            0},
{VCXRuleProp,          UNSUPPORTED,0,            0},
{0,0,0,0}
};


#if 0
		switch (vObjectValueType(object)) {
		    case VCVT_USTRINGZ: {
			char c;
			char *t,*s;
			s = t = fakeCString(vObjectUStringZValue(object));
			printf(" ustringzstring:%s\n",s);
			deleteStr(s);
			break;
		    }
		    case VCVT_STRINGZ: {
			char c;
			const char *s = vObjectStringZValue(object);
			printf(" stringzstring:%s\n",s);
			break;
		    }
		    case VCVT_UINT:
		    {
			int i = vObjectIntegerValue(object);
			printf(" int:%d\n",i);
			break;
		    }
		    case VCVT_ULONG:
		    {
			long l = vObjectLongValue(object); 
			printf(" int:%d\n",l);
			break;
		    }
		    case VCVT_VOBJECT:
		    {
			printf("ERROR, should not get here\n");
			break;
		    }
		    case VCVT_RAW:
		    case 0:
		    default:
			break;
		}

#endif
