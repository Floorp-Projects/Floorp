/* -*- Mode: C; tab-width: 4; c-basic-offset: 8; -*-
  ======================================================================
  FILE: icalparser.c
  CREATOR: eric 04 August 1999
  
  $Id: icalparser.c,v 1.7 2004/09/15 20:30:09 mostafah%oeone.com Exp $
  $Locker:  $
    
 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.
 

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The Initial Developer of the Original Code is Eric Busboom

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
 ======================================================================*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "icalparser.h"
#include "pvl.h"
#include "icalmemory.h"
#include "icalerror.h"
#include "icalvalue.h"
#include "icalderivedparameter.h"
#include "icalparameter.h"
#include "icalproperty.h"
#include "icalcomponent.h"

#include <string.h> /* For strncpy & size_t */
#include <stdio.h> /* For FILE and fgets and sprintf */
#include <stdlib.h> /* for free */
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#else
#include <ctype.h>
#define iswspace isspace
#endif
#ifdef WIN32
#define snprintf      _snprintf
#define strcasecmp    stricmp
#endif


extern icalvalue* icalparser_yy_value;
void set_parser_value_state(icalvalue_kind kind);
int ical_yyparse(void);

char* icalparser_get_next_char(char c, char *str, int qm);
char* icalparser_get_next_parameter(char* line,char** end);
char* icalparser_get_next_value(char* line, char **end, icalvalue_kind kind);
char* icalparser_get_prop_name(char* line, char** end);
char* icalparser_get_param_name(char* line, char **end);

#define TMP_BUF_SIZE 80

struct icalparser_impl 
{
    int buffer_full; /* flag indicates that temp is smaller that 
                        data being read into it*/
    int continuation_line; /* last line read was a continuation line */
    size_t tmp_buf_size; 
    char temp[TMP_BUF_SIZE];
    icalcomponent *root_component;
    int version; 
    int level;
    int lineno;
    icalparser_state state;
    pvl_list components;
    
    void *line_gen_data;
    
};


icalparser* icalparser_new(void)
{
    struct icalparser_impl* impl = 0;
    if ( ( impl = (struct icalparser_impl*)
	   malloc(sizeof(struct icalparser_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }
    
    impl->root_component = 0;
    impl->components  = pvl_newlist();  
    impl->level = 0;
    impl->state = ICALPARSER_SUCCESS;
    impl->tmp_buf_size = TMP_BUF_SIZE;
    impl->buffer_full = 0;
    impl->lineno = 0;
    impl->continuation_line = 0;
    memset(impl->temp,0, TMP_BUF_SIZE);

    return (icalparser*)impl;
}


void icalparser_free(icalparser* parser)
{
    struct icalparser_impl* impl = (struct icalparser_impl*)parser;
    icalcomponent *c;

    if (impl->root_component != 0){
	icalcomponent_free(impl->root_component);
    }

    while( (c=pvl_pop(impl->components)) != 0){
	icalcomponent_free(c);
    }
    
    pvl_free(impl->components);
    
    free(impl);
}

void icalparser_set_gen_data(icalparser* parser, void* data)
{
    struct icalparser_impl* impl = (struct icalparser_impl*)parser;

    impl->line_gen_data  = data;
}


icalvalue* icalvalue_new_From_string_with_error(icalvalue_kind kind, 
                                                char* str, 
                                                icalproperty **error);



char* icalparser_get_next_char(char c, char *str, int qm)
{
    int quote_mode = 0;
    char* p;
    

    for(p=str; *p!=0; p++){
	    if (qm == 1) {
	if ( quote_mode == 0 && *p=='"' && *(p-1) != '\\' ){
	    quote_mode =1;
	    continue;
	}

	if ( quote_mode == 1 && *p=='"' && *(p-1) != '\\' ){
	    quote_mode =0;
	    continue;
	}
	    }

	if (quote_mode == 0 &&  *p== c  && *(p-1) != '\\' ){
	    return p;
	}

    }

    return 0;
}

/* make a new tmp buffer out of a substring */
char* make_segment(char* start, char* end)
{
    char *buf, *tmp;
    size_t size = (size_t)end - (size_t)start;
    
    buf = icalmemory_tmp_buffer(size+1);
    

    strncpy(buf,start,size);
    *(buf+size) = 0;

	tmp = (buf+size);
	while ( *tmp == '\0' || iswspace(*tmp) )
	{
		*tmp = 0;
		tmp--;
	}
    
    return buf;
    
}

const char* input_buffer;
const char* input_buffer_p;
#define min(a,b) ((a) < (b) ? (a) : (b))   

int icalparser_flex_input(char* buf, int max_size)
{
    int n = min(max_size,strlen(input_buffer_p));

    if (n > 0){
	memcpy(buf, input_buffer_p, n);
	input_buffer_p += n;
	return n;
    } else {
	return 0;
    }
}

void icalparser_clear_flex_input(void)
{
    input_buffer_p = input_buffer+strlen(input_buffer);
}

/* Call the flex/bison parser to parse a complex value */

icalvalue*  icalparser_parse_value(icalvalue_kind kind,
                                   const char* str, icalproperty** error)
{
    int r;
    input_buffer_p = input_buffer = str;

    set_parser_value_state(kind);
    icalparser_yy_value = 0;

    r = ical_yyparse();

    /* Error. Parse failed */
    if( icalparser_yy_value == 0 || r != 0){

	if(icalparser_yy_value !=0){
	    icalvalue_free(icalparser_yy_value);
	    icalparser_yy_value = 0;
	}

	return 0;
    }

    if (error != 0){
	*error = 0;
    }

    return icalparser_yy_value;
}

char* icalparser_get_prop_name(char* line, char** end)
{
    char* p;
    char* v;
    char *str;

    p = icalparser_get_next_char(';',line,1); 
    v = icalparser_get_next_char(':',line,1); 
    if (p== 0 && v == 0) {
	return 0;
    }

    /* There is no ';' or, it is after the ';' that marks the beginning of
       the value */
    if (v!=0 && ( p == 0 || p > v)){
	str = make_segment(line,v);
	*end = v+1;
    } else {
	str = make_segment(line,p);
	*end = p+1;
    }

    return str;
}

char* icalparser_get_param_name(char* line, char **end)
{
    
    char* next; 
    char* quote; 
    char *str;

    next = icalparser_get_next_char('=',line,1);

    if (next == 0) {
	return 0;
    }

    str = make_segment(line,next);
    *end = next+1;
    if (**end == '"') {
        *end = *end+1;
	    next = icalparser_get_next_char('"',*end,0);
	    if (next == 0) {
		    return 0;
	    }

	    *end = make_segment(*end,next);
    }

    return str;
   
}

char* icalparser_get_next_paramvalue(char* line, char **end)
{
    
    char* next; 
    char *str;

    next = icalparser_get_next_char(',',line,1);

    if (next == 0){
	next = (char*)(size_t)line+(size_t)strlen(line);\
    }

    if (next == line){
	return 0;
    } else {
	str = make_segment(line,next);
	*end = next+1;
	return str;
    }
   
}

/* A property may have multiple values, if the values are seperated by
   commas in the content line. This routine will look for the next
   comma after line and will set the next place to start searching in
   end. */

char* icalparser_get_next_value(char* line, char **end, icalvalue_kind kind)
{
    
    char* next;
    char *p;
    char *str;
    size_t length = strlen(line);

    p = line;
    while(1){

	next = icalparser_get_next_char(',',p,1);

	/* Unforunately, RFC2445 says that for the RECUR value, COMMA
	   can both seperate digits in a list, and it can seperate
	   multiple recurrence specifications. This is not a friendly
	   part of the spec. This weirdness tries to
	   distinguish the two uses. it is probably a HACK*/
      
	if( kind == ICAL_RECUR_VALUE ) {
	    if ( next != 0 &&
		 (*end+length) > next+5 &&
		 strncmp(next,"FREQ",4) == 0
		) {
		/* The COMMA was followed by 'FREQ', is it a real seperator*/
		/* Fall through */
	    } else if (next != 0){
		/* Not real, get the next COMMA */
		p = next+1;
		next = 0;
		continue;
	    } 
	}

	/* If the comma is preceeded by a '\', then it is a literal and
	   not a value seperator*/  
      
	if ( (next!=0 && *(next-1) == '\\') ||
	     (next!=0 && *(next-3) == '\\')
	    ) 
	    /*second clause for '/' is on prev line. HACK may be out of bounds */
	{
	    p = next+1;
	} else {
	    break;
	}

    }

    if (next == 0){
	next = (char*)(size_t)line+length;
	*end = next;
    } else {
	*end = next+1;
    }

    if (next == line){
	return 0;
    } 
	

    str = make_segment(line,next);
    return str;
   
}

char* icalparser_get_next_parameter(char* line,char** end)
{
    char *next;
    char *v;
    char *str;

    v = icalparser_get_next_char(':',line,1); 
    next = icalparser_get_next_char(';', line,1);
    
    /* There is no ';' or, it is after the ':' that marks the beginning of
       the value */

    if (next == 0 || next > v) {
	next = icalparser_get_next_char(':', line,1);
    }

    if (next != 0) {
	str = make_segment(line,next);
	*end = next+1;
	return str;
    } else {
	*end = line;
	return 0;
    }
}

/* Get a single property line, from the property name through the
   final new line, and include any continuation lines */

char* icalparser_get_line(icalparser *parser,
                          char* (*line_gen_func)(char *s, size_t size, void *d))
{
    char *line;
    char *line_p;
    struct icalparser_impl* impl = (struct icalparser_impl*)parser;
    size_t buf_size = impl->tmp_buf_size;

    
    line_p = line = icalmemory_new_buffer(buf_size);
    line[0] = '\0';
   
    /* Read lines by calling line_gen_func and putting the data into
       impl->temp. If the line is a continuation line ( begins with a
       space after a newline ) then append the data onto line and read
       again. Otherwise, exit the loop. */

    while(1) {

        /* The first part of the loop deals with the temp buffer,
           which was read on he last pass through the loop. The
           routine is split like this because it has to read lone line
           ahead to determine if a line is a continuation line. */


	/* The tmp buffer is not clear, so transfer the data in it to the
	   output. This may be left over from a previous call */
	if (impl->temp[0] != '\0' ) {

	    /* If the last position in the temp buffer is occupied,
               mark the buffer as full. The means we will do another
               read later, because the line is not finished */
	    if (impl->temp[impl->tmp_buf_size-1] == 0 &&
		impl->temp[impl->tmp_buf_size-2] != '\n'&& 
                impl->temp[impl->tmp_buf_size-2] != 0 ){
		impl->buffer_full = 1;
	    } else {
		impl->buffer_full = 0;
	    }

	    /* Copy the temp to the output and clear the temp buffer. */
            if(impl->continuation_line==1){
                /* back up the pointer to erase the continuation characters */
                impl->continuation_line = 0;
                line_p--;
                
                if ( *(line_p-1) == '\r'){
                    line_p--;
                }
                
                /* copy one space up to eliminate the leading space*/
                icalmemory_append_string(&line,&line_p,&buf_size,
                                         impl->temp+1);

            } else {
                icalmemory_append_string(&line,&line_p,&buf_size,impl->temp);
            }

            impl->temp[0] = '\0' ;
	}
	
	impl->temp[impl->tmp_buf_size-1] = 1; /* Mark end of buffer */

        /****** Here is where the routine gets string data ******************/
	if ((*line_gen_func)(impl->temp,impl->tmp_buf_size,impl->line_gen_data)
	    ==0){/* Get more data */

	    /* If the first position is clear, it means we didn't get
               any more data from the last call to line_ge_func*/
	    if (impl->temp[0] == '\0'){

		if(line[0] != '\0'){
		    /* There is data in the output, so fall trhough and process it*/
		    break;
		} else {
		    /* No data in output; return and signal that there
                       is no more input*/
		    free(line);
		    return 0;
		}
	    }
	}
	
 
	/* If the output line ends in a '\n' and the temp buffer
	   begins with a ' ', then the buffer holds a continuation
	   line, so keep reading.  */

	if  ( line_p > line+1 && *(line_p-1) == '\n' && impl->temp[0] == ' ') {

            impl->continuation_line = 1;

	} else if ( impl->buffer_full == 1 ) {
	    
	    /* The buffer was filled on the last read, so read again */

	} else {

	    /* Looks like the end of this content line, so break */
	    break;
	}

	
    }

    /* Erase the final newline and/or carriage return*/
    if ( line_p > line+1 && *(line_p-1) == '\n') {	
	*(line_p-1) = '\0';
	if ( *(line_p-2) == '\r'){
	    *(line_p-2) = '\0';
	}

    } else {
	*(line_p) = '\0';
    }
    while ( (*line_p == '\0' || iswspace(*line_p)) && line_p > line )
	{
		*line_p = '\0';
		line_p--;
	}

    return line;

}

void insert_error(icalcomponent* comp, char* text, 
		  char* message, icalparameter_xlicerrortype type)
{
    char temp[1024];
    
    if (text == 0){
	snprintf(temp,1024,"%s:",message);
    } else {
	snprintf(temp,1024,"%s: %s",message,text);
    }	
    
    icalcomponent_add_property
	(comp,
	 icalproperty_vanew_xlicerror(
	     temp,
	     icalparameter_new_xlicerrortype(type),
	     0));   
}

int line_is_blank(char* line){
    int i=0;

    for(i=0; *(line+i)!=0; i++){
	char c = *(line+i);

	if(c != ' ' && c != '\n' && c != '\t'){
	    return 0;
	}
    }
    
    return 1;
}

icalcomponent* icalparser_parse(icalparser *parser,
				char* (*line_gen_func)(char *s, size_t size, 
						       void* d))
{

    char* line; 
    icalcomponent *c=0; 
    icalcomponent *root=0;
    struct icalparser_impl *impl = (struct icalparser_impl*)parser;
    icalerrorstate es = icalerror_get_error_state(ICAL_MALFORMEDDATA_ERROR);
	int cont;

    icalerror_check_arg_rz((parser !=0),"parser");

    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,ICAL_ERROR_NONFATAL);

    do{
	    line = icalparser_get_line(parser, line_gen_func);

	if ((c = icalparser_add_line(parser,line)) != 0){

	    if(icalcomponent_get_parent(c) !=0){
		/* This is bad news... assert? */
	    }	    
	    
	    assert(impl->root_component == 0);
	    assert(pvl_count(impl->components) ==0);

	    if (root == 0){
		/* Just one component */
		root = c;
	    } else if(icalcomponent_isa(root) != ICAL_XROOT_COMPONENT) {
		/*Got a second component, so move the two components under
		  an XROOT container */
		icalcomponent *tempc = icalcomponent_new(ICAL_XROOT_COMPONENT);
		icalcomponent_add_component(tempc, root);
		icalcomponent_add_component(tempc, c);
		root = tempc;
	    } else if(icalcomponent_isa(root) == ICAL_XROOT_COMPONENT) {
		/* Already have an XROOT container, so add the component
		   to it*/
		icalcomponent_add_component(root, c);
		
	    } else {
		/* Badness */
		assert(0);
	    }

	    c = 0;

        }
	cont = 0;
	if(line != 0){
	    free(line);
		cont = 1;
	}
    } while ( cont );

    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,es);

    return root;

}


icalcomponent* icalparser_add_line(icalparser* parser,
                                       char* line)
{ 
    char *p; 
    char *str;
    char *end;
    int vcount = 0;
    icalproperty *prop;
    icalproperty_kind prop_kind;
    icalvalue *value;
    icalvalue_kind value_kind = ICAL_NO_VALUE;


    struct icalparser_impl *impl = (struct icalparser_impl*)parser;
    icalerror_check_arg_rz((parser != 0),"parser");


    if (line == 0)
    {
	impl->state = ICALPARSER_ERROR;
	return 0;
    }

    if(line_is_blank(line) == 1){
	return 0;
    }

    /* Begin by getting the property name at the start of the line. The
       property name may end up being "BEGIN" or "END" in which case it
       is not really a property, but the marker for the start or end of
       a component */

    end = 0;
    str = icalparser_get_prop_name(line, &end);

    if (str == 0 || strlen(str) == 0 ){
	/* Could not get a property name */
	icalcomponent *tail = pvl_data(pvl_tail(impl->components));

	if (tail){
	    insert_error(tail,line,
			 "Got a data line, but could not find a property name or component begin tag",
			 ICAL_XLICERRORTYPE_COMPONENTPARSEERROR);
	}
	tail = 0;
	impl->state = ICALPARSER_ERROR;
	return 0; 
    }

    /**********************************************************************
     * Handle begin and end of components
     **********************************************************************/								       
    /* If the property name is BEGIN or END, we are actually
       starting or ending a new component */

    if(strcmp(str,"BEGIN") == 0){
	icalcomponent *c;
        icalcomponent_kind comp_kind;

	impl->level++;
	str = icalparser_get_next_value(end,&end, value_kind);
	    

        comp_kind = icalenum_string_to_component_kind(str);

        if (comp_kind == ICAL_NO_COMPONENT){
	    c = icalcomponent_new(ICAL_XLICINVALID_COMPONENT);
	    insert_error(c,str,"Parse error in component name",
			 ICAL_XLICERRORTYPE_COMPONENTPARSEERROR);
        }

	c  =  icalcomponent_new(comp_kind);

	if (c == 0){
	    c = icalcomponent_new(ICAL_XLICINVALID_COMPONENT);
	    insert_error(c,str,"Parse error in component name",
			 ICAL_XLICERRORTYPE_COMPONENTPARSEERROR);
	}
	    
	pvl_push(impl->components,c);

	impl->state = ICALPARSER_BEGIN_COMP;
	return 0;

    } else if (strcmp(str,"END") == 0 ) {
	icalcomponent* tail;

	impl->level--;
	str = icalparser_get_next_value(end,&end, value_kind);

	/* Pop last component off of list and add it to the second-to-last*/
	impl->root_component = pvl_pop(impl->components);

	tail = pvl_data(pvl_tail(impl->components));

	if(tail != 0){
	    icalcomponent_add_component(tail,impl->root_component);
	} 

	tail = 0;

	/* Return the component if we are back to the 0th level */
	if (impl->level == 0){
	    icalcomponent *rtrn; 

	    if(pvl_count(impl->components) != 0){
	    /* There are still components on the stack -- this means
               that one of them did not have a proper "END" */
		pvl_push(impl->components,impl->root_component);
		icalparser_clean(parser); /* may reset impl->root_component*/
	    }

	    assert(pvl_count(impl->components) == 0);

	    impl->state = ICALPARSER_SUCCESS;
	    rtrn = impl->root_component;
	    impl->root_component = 0;
	    return rtrn;

	} else {
	    impl->state = ICALPARSER_END_COMP;
	    return 0;
	}
    }


    /* There is no point in continuing if we have not seen a
       component yet */

    if(pvl_data(pvl_tail(impl->components)) == 0){
	impl->state = ICALPARSER_ERROR;
	return 0;
    }


    /**********************************************************************
     * Handle property names
     **********************************************************************/
								       
    /* At this point, the property name really is a property name,
       (Not a component name) so make a new property and add it to
       the component */

    
    prop_kind = icalproperty_string_to_kind(str);

    prop = icalproperty_new(prop_kind);

    if (prop != 0){
	icalcomponent *tail = pvl_data(pvl_tail(impl->components));

        if(prop_kind==ICAL_X_PROPERTY){
            icalproperty_set_x_name(prop,str);
        }

	icalcomponent_add_property(tail, prop);

	/* Set the value kind for the default for this type of
	   property. This may be re-set by a VALUE parameter */
	value_kind = icalproperty_kind_to_value_kind(icalproperty_isa(prop));

    } else {
	icalcomponent* tail = pvl_data(pvl_tail(impl->components));

	insert_error(tail,str,"Parse error in property name",
		     ICAL_XLICERRORTYPE_PROPERTYPARSEERROR);
	    
	tail = 0;
	impl->state = ICALPARSER_ERROR;
	return 0;
    }

    /**********************************************************************
     * Handle parameter values
     **********************************************************************/								       

    /* Now, add any parameters to the last property */

    p = 0;
    while(1) {

	if (*(end-1) == ':'){
	    /* if the last seperator was a ":" and the value is a
	       URL, icalparser_get_next_parameter will find the
	       ':' in the URL, so better break now. */
	    break;
	}

	str = icalparser_get_next_parameter(end,&end);

	if (str != 0){
	    char* name;
	    char* pvalue;
        
	    icalparameter *param = 0;
	    icalparameter_kind kind;
	    icalcomponent *tail = pvl_data(pvl_tail(impl->components));

	    name = icalparser_get_param_name(str,&pvalue);

	    if (name == 0){
		/* 'tail' defined above */
		insert_error(tail, str, "Cant parse parameter name",
			     ICAL_XLICERRORTYPE_PARAMETERNAMEPARSEERROR);
		tail = 0;
		break;
	    }

	    kind = icalparameter_string_to_kind(name);

	    if(kind == ICAL_X_PARAMETER){
		param = icalparameter_new(ICAL_X_PARAMETER);
		
		if(param != 0){
		    icalparameter_set_xname(param,name);
		    icalparameter_set_xvalue(param,pvalue);
		}


	    } else if (kind != ICAL_NO_PARAMETER){
		param = icalparameter_new_from_value_string(kind,pvalue);
	    } else {
		/* Error. Failed to parse the parameter*/
		/* 'tail' defined above */
                /* The following ifdef block has been added by Mozilla Calendar in order to
                have the option of being flexible towards unsupported parameters */
                #ifndef ICAL_ERRORS_ARE_FATAL
                continue;
                #endif;
		insert_error(tail, str, "Cant parse parameter name",
			     ICAL_XLICERRORTYPE_PARAMETERNAMEPARSEERROR);
		tail = 0;
		impl->state = ICALPARSER_ERROR;
		return 0;
	    }

	    if (param == 0){
		/* 'tail' defined above */
		insert_error(tail,str,"Cant parse parameter value",
			     ICAL_XLICERRORTYPE_PARAMETERVALUEPARSEERROR);
		    
		tail = 0;
		impl->state = ICALPARSER_ERROR;
		continue;
	    }

	    /* If it is a VALUE parameter, set the kind of value*/
	    if (icalparameter_isa(param)==ICAL_VALUE_PARAMETER){

		value_kind = (icalvalue_kind)
                    icalparameter_value_to_value_kind(
                                icalparameter_get_value(param)
                                );

		if (value_kind == ICAL_NO_VALUE){

		    /* Ooops, could not parse the value of the
		       parameter ( it was not one of the defined
		       values ), so reset the value_kind */
			
		    insert_error(
			tail, str, 
			"Got a VALUE parameter with an unknown type",
			ICAL_XLICERRORTYPE_PARAMETERVALUEPARSEERROR);
		    icalparameter_free(param);
			
		    value_kind = 
			icalproperty_kind_to_value_kind(
			    icalproperty_isa(prop));
			
		    icalparameter_free(param);
		    tail = 0;
		    impl->state = ICALPARSER_ERROR;
		    return 0;
		} 
	    }

	    /* Everything is OK, so add the parameter */
	    icalproperty_add_parameter(prop,param);
	    tail = 0;

	} else { /* if ( str != 0)  */
	    /* If we did not get a param string, go on to looking
	       for a value */
	    break;
	} /* if ( str != 0)  */

    } /* while(1) */	    
	
    /**********************************************************************
     * Handle values
     **********************************************************************/								       

    /* Look for values. If there are ',' characters in the values,
       then there are multiple values, so clone the current
       parameter and add one part of the value to each clone */

    vcount=0;
    while(1) {
	str = icalparser_get_next_value(end,&end, value_kind);

	if (str != 0){
		
	    if (vcount > 0){
		/* Actually, only clone after the second value */
		icalproperty* clone = icalproperty_new_clone(prop);
		icalcomponent* tail = pvl_data(pvl_tail(impl->components));
		    
		icalcomponent_add_property(tail, clone);
		prop = clone;		    
		tail = 0;
	    }
		
	    value = icalvalue_new_from_string(value_kind, str);
		
	    /* Don't add properties without value */
	    if (value == 0){
		char temp[200]; /* HACK */

		icalproperty_kind prop_kind = icalproperty_isa(prop);
		icalcomponent* tail = pvl_data(pvl_tail(impl->components));

		sprintf(temp,"Cant parse as %s value in %s property. Removing entire property",
			icalvalue_kind_to_string(value_kind),
			icalproperty_kind_to_string(prop_kind));

		insert_error(tail, str, temp,
			     ICAL_XLICERRORTYPE_VALUEPARSEERROR);

		/* Remove the troublesome property */
		icalcomponent_remove_property(tail,prop);
		icalproperty_free(prop);
		prop = 0;
		tail = 0;
		impl->state = ICALPARSER_ERROR;
		return 0;
		    
	    } else {
		vcount++;
		icalproperty_set_value(prop, value);
	    }


	} else {
	    if (vcount == 0){
		char temp[200]; /* HACK */
		
		icalproperty_kind prop_kind = icalproperty_isa(prop);
		icalcomponent *tail = pvl_data(pvl_tail(impl->components));
		
		sprintf(temp,"No value for %s property. Removing entire property",
			icalproperty_kind_to_string(prop_kind));

		insert_error(tail, str, temp,
			     ICAL_XLICERRORTYPE_VALUEPARSEERROR);

		/* Remove the troublesome property */
		icalcomponent_remove_property(tail,prop);
		icalproperty_free(prop);
		prop = 0;
		tail = 0;
		impl->state = ICALPARSER_ERROR;
		return 0;
	    } else {

		break;
	    }
	}
    }
	
    /****************************************************************
     * End of component parsing. 
     *****************************************************************/

    if (pvl_data(pvl_tail(impl->components)) == 0 &&
	impl->level == 0){
	/* HACK. Does this clause ever get executed? */
	impl->state = ICALPARSER_SUCCESS;
	assert(0);
	return impl->root_component;
    } else {
	impl->state = ICALPARSER_IN_PROGRESS;
	return 0;
    }

}

icalparser_state icalparser_get_state(icalparser* parser)
{
    struct icalparser_impl* impl = (struct icalparser_impl*) parser;
    return impl->state;

}

icalcomponent* icalparser_clean(icalparser* parser)
{
    struct icalparser_impl* impl = (struct icalparser_impl*) parser;
    icalcomponent *tail; 

    icalerror_check_arg_rz((parser != 0 ),"parser");

    /* We won't get a clean exit if some components did not have an
       "END" tag. Clear off any component that may be left in the list */
    
    while((tail=pvl_data(pvl_tail(impl->components))) != 0){

	insert_error(tail," ",
		     "Missing END tag for this component. Closing component at end of input.",
		     ICAL_XLICERRORTYPE_COMPONENTPARSEERROR);
	

	impl->root_component = pvl_pop(impl->components);
	tail=pvl_data(pvl_tail(impl->components));

	if(tail != 0){
	    if(icalcomponent_get_parent(impl->root_component)!=0){
		icalerror_warn("icalparser_clean is trying to attach a component for the second time");
	    } else {
		icalcomponent_add_component(tail,impl->root_component);
	    }
	}
	    
    }

    return impl->root_component;

}

struct slg_data {
	const char* pos;
	const char* str;
};

char* string_line_generator(char *out, size_t buf_size, void *d)
{
    char *n;
    size_t size;
    struct slg_data* data = (struct slg_data*)d;

    if(data->pos==0){
	data->pos=data->str;
    }

    /* If the pointer is at the end of the string, we are done */
    if (*(data->pos)==0){
	return 0;
    }

    n = strchr(data->pos,'\n');
    
    if (n == 0){
	size = strlen(data->pos);
    } else {
	n++; /* include newline in output */
	size = (n-data->pos);	
    }

    if (size > buf_size-1){
	size = buf_size-1;
    }
    

    strncpy(out,data->pos,size);
    
    *(out+size) = '\0';
    
    data->pos += size;

    return out;    
}

icalcomponent* icalparser_parse_string(const char* str)
{
    icalcomponent *c;
    struct slg_data d;
    icalparser *p;

    icalerrorstate es = icalerror_get_error_state(ICAL_MALFORMEDDATA_ERROR);

    d.pos = 0;
    d.str = str;

    p = icalparser_new();
    icalparser_set_gen_data(p,&d);

    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,ICAL_ERROR_NONFATAL);

    c = icalparser_parse(p,string_line_generator);

    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,es);

    icalparser_free(p);

    return c;

}
