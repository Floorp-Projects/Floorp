NOTE: If you used the earlier APIs released by Versit
then you will want to look at the document "migrate.doc"
included with this package. It contains a discussion of
the differences between the old API and this one.

----------------------------------------------------------------

The vCard/vCalendar C interface is implemented in the set 
of files as follows:

vcc.y, yacc source, and vcc.c, the yacc output you will use
implements the core parser

vobject.c implements an API that insulates the caller from
the parser and changes in the vCard/vCalendar BNF

port.h defines compilation environment dependent stuff

vcc.h and vobject.h are header files for their .c counterparts

vcaltmp.h and vcaltmp.c implement vCalendar "macro" functions
which you may find useful.

test.c is a standalone test driver that exercises some of
the features of the APIs provided. Invoke test.exe on a
VCARD/VCALENDAR input text file and you will see the pretty
print output of the internal representation (this pretty print
output should give you a good idea of how the internal 
representation looks like -- there is one such output in the 
following too). Also, a file with the .out suffix is generated 
to show that the internal representation can be written back 
in the original text format.

-----------------------------------------------------------------
			
			  
				VObject for VCard/VCalendar

Table of Contents
=================
1. VObject
2. Internal Representations of VCard/VCalendar
3. Iterating Through VObject's Properties or Values
4. Pretty Printing a VObject Tree
5. Building A VObject Representation of A VCard/VCalendar
6. Converting A VObject Representation Into Its Textual Representation
7. Miscellaneous Notes On VObject APIs usages
8. Brief descriptions of each APIs
9. Additional Programming Notes.

This document is mainly about the VObject and its APIs. The main
use of a VObject is to represent a VCard or a VCalendar inside
a program. However, its use is not limited to aforemention as it
can represent an arbitrary information that makes up of a tree or
forest of properties/values.

1. VObject
   =======
A VObject can have a name (id) and a list of associated properties and
a value. Each property is itself a VObject.

2. Internal Representations of VCard/VCalendar
   ===========================================
A list of VCard or a VCalendar is represented by a list of VObjects.
The name (id) of the VObjects in the list is either VCCardProp or
VCCalProp. Each of these VObjects can have a list of properties.
Since a property is represented as a VObject, each of these properties
can have a name, a list of properties, and a value.

For example, the input file "vobject.vcf":
	
BEGIN:VCARD
N:Alden;Roland
FN:Roland H. Alden
ORG:AT&T;Versit Project Office
TITLE:Consultant
EMAIL;WORK;PREF;INTERNET:sf!rincon!ralden@alden.attmail.com
EMAIL;INTERNET:ralden@sfgate.com
EMAIL;MCIMail:242-2200
LABEL;DOM;POSTAL;PARCEL;HOME;WORK;QUOTED-PRINTABLE:Roland H. Alden=0A=
Suite 2208=0A=
One Pine Street=0A=
San Francisco, CA 94111
LABEL;POSTAL;PARCEL;HOME;WORK;QUOTED-PRINTABLE:Roland H. Alden=0A=
Suite 2208=0A=
One Pine Street=0A=
San Francisco, CA 94111=0A=
U.S.A.
TEL;WORK;PREF;MSG:+1 415 296 9106
TEL;WORK;FAX:+1 415 296 9016
TEL;MSG;CELL:+1 415 608 5981
ADR:;Suite 2208;One Pine Street;San Francisco;CA;94111;U.S.A.
SOUND:ROW-LAND H ALL-DIN
LOGO;GIF;BASE64:
    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

BEGIN:VCALENDAR
DCREATED:19960523T100522
PRODID:-//Alden Roland/Hand Crafted In North Carolina//NONSGML Made By Hand//EN
VERSION:0.3
BEGIN:VEVENT
START:19960523T120000
END:19960523T130000
SUBTYPE:PHONE CALL
SUMMARY:VERSIT PDI PR Teleconference/Interview
DESCRIPTION:VERSIT PDI PR Teleconference/Interview With Tom Streeter and Alden Roland
END:VEVENT
BEGIN:VEVENT
START:19960523T113000
END:19960523T115500
SUBTYPE:LUNCH
SUMMARY:Eat in the cafeteria today
END:VEVENT
END:VCALENDAR

END:VCARD


will conceptually be be represented as
    vcard
	VCNameProp
	    VCFamilyNameProp=Alden
	    VCGivenNameProp=Roland
	VCFullNameProp=Roland H.Alden
	....
	
note that
    EMAIL;WORK;PREF;INTERNET:sf!rincon!ralden@alden.attmail.com
will be represented as:
	VCEmailAddress=sf!rincon!ralden@alden.attmail.com
	    VCWork
	    VCPreferred
	    VCInternet
where the lower level properties are properties of the property
VCEmailAddress.

Groupings are flattened out in the VObject representation such
that:
	a.b:blah
	a.c:blahblah
are represented as:
	b=blah
	    VCGrouping=a
	c=blahblah
	    VCGrouping=a
i.e. one can read the above as:
	 the property "b" has value "blah" and property "VCGrouping"
		with the value "a".
	 the property "c" has value "blahblah" and property "VCGrouping"
		with the value "a".
likewise, multi-level groupings are flatten similarly. e.g.
	a.b.c:blah
	a.b.e:blahblah
-->
	c=blah
	    VCGrouping=b
		VCGrouping=a
	e=blahblah
	    VCGrouping=b
		VCGrouping=a
which read:
	 the property "c" has value "blah" and property "VCGrouping"
		with the value "b" which has property "VCGrouping"
		with value "a".
	 the property "e" has value "blahblah" and property "VCGrouping"
		with the value "b" which has property "VCGrouping"
		with value "a".

3. Iterating Through VObject's Properties or Values
   ================================================
The following is a skeletal form of iterating through
all properties of a vobject, o:

    // assume the object of interest, o, is of type VObject
    VObjectIterator i;
    initPropIterator(&i,o);
    while (moreIteration(&i)) {
	VObject *each = nextVObject(&i);
	// ... do something with "each" property 
	}

Use the API vObjectName() to access a VObject's name.
Use the API vObjectValueType() to determine if a VObject has
	a value. For VCard/VCalendar application, you
	should not need this function as practically
	all values are either of type VCVT_USTRINGZ or
	VCVT_RAW (i.e set by setVObjectUStringZValue and
	setVObjectAnyValue APIs respectively), and the
	value returned by calls to vObjectUStringZValue
	and vObjectAnyValue are 0 if a VObject has no
	value. (There is a minor exception where VObject with
	VCDataSizeProp has value that is set by
	setVObjectLongValue).
Use the APIs vObject???Value() to access a VObject's value.
	where ??? is the expected type.
Use the APIs setvObject???Value() to set or modify a VObject's value.
	where ??? is the expected type.
Use the API isAPropertyOf() to query if a name match the name of
	a property of a VObject. Since isAPropertyOf() return
	the matching property, we can use that to retrieve
	a property and subsequently the value of the property.

4. Pretty Printing a VObject Tree
   ==============================
VObject tree can be pretty printed with the printVObject() function.
The output of pretty printing a VObject representation of the input
test file "vobject.vcf" is shown below. Note that the indentation
indicates the tree hirerarchy where the immediate children nodes
of a parent node is all at the same indentation level and the
immediate children nodes are the immediate properties of the
associated parent nodes.  In the following, {N,FN,ORG,TITLE,...}
are immediate properties of VCARD. {F and G} are properties of N
with value {"Alden" and "Roland"} respectively; FN has no property
but has the value "Roland H. Alden"; EMAIL has value and
the properties WORK, PREF, and INTERNET.


VCARD
    N
        F="Alden"
        G="Roland"
    FN="Roland H. Alden"
    ORG
        ORGNAME="AT&T"
        OUN="Versit Project Office"
    TITLE="Consultant"
    EMAIL="sf!rincon!ralden@alden.attmail.com"
        WORK
        PREF
        INTERNET
    EMAIL="ralden@sfgate.com"
        INTERNET
    EMAIL="242-2200"
        MCIMail
    LABEL="Roland H. Alden
            Suite 2208
            One Pine Street
            San Francisco, CA 94111"
        DOM
        POSTAL
        PARCEL
        HOME
        WORK
        QP
    LABEL="Roland H. Alden
            Suite 2208
            One Pine Street
            San Francisco, CA 94111
            U.S.A."
        POSTAL
        PARCEL
        HOME
        WORK
        QP
    TEL="+1 415 296 9106"
        WORK
        PREF
        MSG
    TEL="+1 415 296 9016"
        WORK
        FAX
    TEL="+1 415 608 5981"
        MSG
        CELL
    ADR
        EXT ADD="Suite 2208"
        STREET="One Pine Street"
        L="San Francisco"
        R="CA"
        PC="94111"
        C="U.S.A."
    SOUND="ROW-LAND H ALL-DIN"
    LOGO=[raw data]
        GIF
        BASE64
        DataSize=1482
VCALENDAR
    DCREATED="19960523T100522"
    PRODID="-//Alden Roland/Hand Crafted In North Carolina//NONSGML Made By Hand//EN"
    VERSION="0.3"
    VEVENT
        START="19960523T120000"
        END="19960523T130000"
        SUBTYPE="PHONE CALL"
        SUMMARY="VERSIT PDI PR Teleconference/Interview"
        DESCRIPTION="VERSIT PDI PR Teleconference/Interview With Tom Streeter and Alden Roland"
    VEVENT
        START="19960523T113000"
        END="19960523T115500"
        SUBTYPE="LUNCH"
        SUMMARY="Eat in the cafeteria today"

5. Building A VObject Representation of A VCard/VCalendar
   ======================================================
The parser in vcc.y converts an input file with one or more
VCard/VCalendar that is in their textual representation
into their corresponding VObject representation.

VObject representation of a VCard/VCalendar can also be built
directly with calls to the VObject building APIs. e.g.

	VObject *prop;
	VObject *vcard = newVObject(VCCardProp);
	prop = addProp(vcard,VCNameProp);
		addPropValue(prop,VCFamilyNameProp,"Alden");
		addPropValue(prop,VCGivenNameProp,"Roland");
	addPropValue(vcard,VCFullNameProp,"Roland H. Alden");
	....

6. Converting A VObject Representation Into Its Textual Representation
   ===================================================================
The VObject representation can be converted back to its textual
representation via the call to writeVObject() or writeMemVObject()
API. e.g.
   a. to write to a file:
	// assume vcard is of type VObject
	FILE *fp = fopen("alden.vcf","w");
	writeVObject(fp,vcard);
   a. to write to memory, and let the API allocate the required memory.
	char* clipboard = writeVObject(0,0,vcard);
	... do something to clipboard
	free(clipboard);
   b. to write to a user allocated buffer:
	char clipboard[16384];
	int len = 16384;
	char *buf  = writeVObject(clipboard,&len,vcard);
	... buf will be equal to clipboard if the write
	is successful otherwise 0.

In the case of writing to memory, the memory buffer can be either
allocated by the API or the user. If the user allocate the
memory for the buffer, then the length of the buffer needs to be
communicated to the API via a variable. The variable passed as
the length argument will be overwritten with the actual size
of the text output. A 0 return value from writeMemVObject()
indicates an error which could be caused by overflowing the
size of the buffer or lack of heap memory.

7. Miscellaneous Notes On VObject APIs usages
   ==========================================
a. vcc.h -- contains basic interfaces to the parser:
    VObject* Parse_MIME(const char *input, unsigned long len);
    VObject* Parse_MIME_FromFile(FILE *file);
	-- both of this return a null-terminated list of
	   VObject that is either a VCARD or VCALENDAR.
	   To iterate through this list, do
		VObject *t, *v;
		v = Parse_Mime_FromFile(fp);
		while (v) {
		    // ... do something to v.
		    t = v;
		    v = nextVObjectInList(v);
		    cleanVObject(t);
		    }
	    note that call to cleanVObject will release
	    resource used to represent the VObject.

b. vobject.h -- contains basic interfaces to the VObject APIs.
	see the header for more details.
	The structure of VObject is purposely (hiddened) not exposed
	to the user. Every access has to be done via
	the APIs. This way, if we need to change the
	structure or implementation, the client need not
	recompile as long as the interfaces remain the
	same.

c. values of a property is determined by the property definition
	itself. The vobject APIs does not attempt to enforce
	any of such definition. It is the consumer responsibility
	to know what value is expected from a property. e.g
	most properties have unicode string value, so to access
	the value of these type of properties, you will use
	the vObjectUStringZValue() to read the value and
	setVObjectUStringZValue() to set or modify the value.
	Refer to the VCard and VCalendar specifications for
	the definition of each property.

d. properties name (id) are case incensitive.

8. Brief descriptions of each APIs
   ===============================
   * the predefined properties' names (id) are listed under vobject.h
	each is of the form VC*Prop. e.g.
		#define VC7bitProp		"7BIT"
		#define VCAAlarmProp	"AALARM"
		....

   * consumer of a VObject can only define pointers to VObject.

   * a variable of type VObjectIterator, say "i", can be used to iterate
	through a VObject's properties, say "o". The APIs related to
	VObjectIterator are:
		void initPropIterator(VObjectIterator *i, VObject *o);
			-- e.g. usage
				initPropIterator(&i,o);
		int moreIteration(VObjectIterator *i);
			-- e.g. usage
				while (moreIteration(&i)) { ... }
		VObject* nextVObject(VObjectIterator *i);
			-- e.g. usage
				while (moreIteration(&i)) {
				    VObject *each = nextVObject(&i);
				    }

    * VObject can be chained together to form a list. e.g. of such
	use is in the parser where the return value of the parser is
	a link list of VObject. A link list of VObject can be
	built by:
		void addList(VObject **o, VObject *p);
	and iterated by
		VObject* nextVObjectInList(VObject *o);
			-- next VObjectInList return 0 if the list
				is exhausted.

    * the following APIs are mainly used to construct a VObject tree:
	VObject* newVObject(const char *id);
	    -- used extensively internally by VObject APIs but when
		used externally, its use is mainly limited to the
		construction of top level object (e.g. an object
		with VCCardProp or VCCalendarProp id).
	
	void deleteVObject(VObject *p);
	    -- to deallocate single VObject, for most user, use
		cleanVObject(VObject *o) instead for freeing all
		resources associated with the VObject.

	char* dupStr(const char *s, unsigned int size);
	    -- duplicate a string s. If size is 0, the string is
		assume to be a null-terminated. 

	void deleteStr(const char *p);
	    -- used to deallocate a string allocated by dupStr();

	void setVObjectName(VObject *o, const char* id);
	    -- set the id of VObject o. This function is not
		normally used by the user. The setting of id
		is normally done as part of other APIs (e.g.
		addProp()).

	void setVObjectStringZValue(VObject *o, const char *s);
	    -- set a string value of a VObject.

	void setVObjectUStringZValue(VObject *o, const wchar_t *s);
	    -- set a Unicode string value of a VObject.

	void setVObjectIntegerValue(VObject *o, unsigned int i);
	    -- set an integer value of a VObject.

	void setVObjectLongValue(VObject *o, unsigned long l);
	    -- set an long integer value of a VObject.

	void setVObjectAnyValue(VObject *o, void *t);
	    -- set any value of a VObject. The value type is
		unspecified.

	VObject* setValueWithSize(VObject *prop, void *val, unsigned int size);
	    -- set a raw data (stream of bytes) value of a VObject
		whose size is size. The internal VObject representation
		is
			this object = val
			    VCDataSizeProp=size
		i.e. the value val will be attached to the VObject prop
		and a property of VCDataSize whose value is size
		is also added to the object.
		
	void setVObjectVObjectValue(VObject *o, VObject *p);
	    -- set a VObject as the value of another VObject.

	const char* vObjectName(VObject *o);
	    -- retrieve the VObject's Name (i.e. id).

	const char* vObjectStringZValue(VObject *o);
	    -- retrieve the VObject's value interpreted as
		null-terminated string.

	const wchar_t* vObjectUStringZValue(VObject *o);
	    -- retrieve the VObject's value interpreted as
		null-terminated unicode string.

	unsigned int vObjectIntegerValue(VObject *o);
	    -- retrieve the VObject's value interpreted as
		integer.

	unsigned long vObjectLongValue(VObject *o);
	    -- retrieve the VObject's value interpreted as
		long integer.

	void* vObjectAnyValue(VObject *o);
	    -- retrieve the VObject's value interpreted as
		any value.

	VObject* vObjectVObjectValue(VObject *o);
	    -- retrieve the VObject's value interpreted as
		a VObject.

	VObject* addVObjectProp(VObject *o, VObject *p);
	    -- add a VObject p as a property of VObject o.
		(not normally used externally for building a
		VObject).

	VObject* addProp(VObject *o, const char *id);
	    -- add a property whose name is id to VObject o.

	VObject* addPropValue(VObject *o, const char *id, const char *v);
	    -- add a property whose name is id and whose value
		is a null-terminated string to VObject o.

	VObject* addPropSizedValue(VObject *o, const char *id,
			const char *v, unsigned int size);
	    -- add a property whose name is id and whose value
		is a stream of bytes of size size, to VObject o.

	VObject* addGroup(VObject *o, const char *g);
	    -- add a group g to VObject o.
		e.g. if g is a.b.c, you will have
		    o
			c
			   VCGroupingProp=b
			       VCGroupingProp=a
		and the object c is returned.
		
	VObject* isAPropertyOf(VObject *o, const char *id);
	     -- query if a property by the name id is in o and
		return the VObject that represent that property.

	void printVObject(VObject *o);
	     -- pretty print VObject o to stdout (for debugging use).

	void writeVObject(FILE *fp, VObject *o);
	     -- convert VObject o to its textual representation and
		write it to file.

	char* writeMemVObject(char *s, int *len, VObject *o);
	     -- convert VObject o to its textual representation and
		write it to memory. If s is 0, then memory required
		to hold the textual representation will be allocated
		by this API. If a variable len is passed, len will
		be overwriten with the byte size of the textual
		representation. If s is non-zero, then s has to
		be a user allocated buffer whose size has be passed
		in len as a variable. Memory allocated by the API
		has to be freed with call to free. The return value
		of this API is either the user supplied buffer,
		the memory allocated by the API, or 0 (in case of
		failure).

	void cleanStrTbl();
		-- this function has to be called when all
		VObject has been destroyed.

	void cleanVObject(VObject *o);
		-- release all resources used by VObject o.

	wchar_t* fakeUnicode(const char *ps, int *bytes);
		-- convert char* to wchar_t*.

	extern int uStrLen(const wchar_t *u);
		-- length of unicode u.

	char *fakeCString(const wchar_t *u);
		-- convert wchar_t to CString (blindly assumes that
		this could be done).

9. Additional Programming Notes
   ============================
In the following notes, please refers to the listing
of Example.vcf and its VObject Representation
(shown at the end of this section).

* Handling the Return Value of the VCard/VCalendar Parser
    The example input text file contains two root VObjects
    (a VCalendar and a VCard). The output of the VCard/VCalendar
    parser is a null-terminated list of VObjects. For this
    particular input file, the list will have two VObjects.
    The following shows a template for iterating through the
    output of the Parser:

	VObject *t, *v;
	v = Parse_Mime_fromFileName("example.vcf");
	while (v) {
	    // currently, v will either be a VCard or a VCalendar
	    //	do whatever your application need to do to
	    //  v here ...
	    t = v;
	    v = nextVObjectInList(v);
	    cleanVObject(t);
	    }

* Iterating Through a VCard/VCalendar VObject
    From the VObject APIs point of view, a VCard VObject
    is the same as a VCalendar VObject. However, the application
    needs to know what are in a VCard or a VCalendar.
    For example, A VCalendar VObject can have VCDCreatedProp,
    a VCGEOLocationProp, etc, and one or more VCEventProp and
    or VCTodoProp. The VCEventProp and VCTodoProp can have
    many properties of their own, which in turn could have
    more properties (e.g. VCDAlarmProp can be a VCEventProp
    VObject's property, and VCRunTimeProp can be a
    VCDAlarmProp VObject's property. Because a VObject tree
    can be arbitrarily complex, in general, to process all
    properties and values of a VObject tree, a recursive walk
    is desirable. An example recursive VObject tree walk
    can be found in the vobject.c source lines for printVObject*
    and writeVObject* APIs. Depending on what the application need
    to do with a VCard or a VCalendar, a recursive walk
    of the VObject tree may or may not be desirable. An example
    template of a non-recursive walk is shown below:
	
    void processVCardVCalendar(char *inputFile)
    {
	VObject *t, *v;
	v = Parse_Mime_fromFileName(inputFile);
	while (v) {
	    char *n = vObjectName(v);
	    if (strcmp(n,VCCardProp) == 0) {
		do_VCard(v);
		}
	    else if (strcmp(n,VCCalendarProp) == 0) {
		do_VCalendar(v);
		}
	    else {
		// don't know how to handle anything else!
		}
	    t = v;
	    v = nextVObjectInList(v);
	    cleanVObject(t);
	    }
    }

    void  do_VCard(VObject *vcard)
    {
	VObjectIterator t;
	initPropIterator(&t,vcard);
	while (moreIteration(&t)) {
	    VObject *eachProp = nextVObject(&t);
	    //	The primarly purpose of this example is to
	    //  show how to iterate through a VCard VObject,
	    //	it is not meant to be efficient at all.
	    char *n = vObjectName(eachProp);
	    if (strcmp(n,VCNameProp)==0) {
		do_name(eachProp);
		}
	    else if (strcmp(n,VCEmailProp)==0) {
		do_email(eachProp);
		}
	    else if (strcmp(n,VCLabelProp)==0) {
		do_label(eachProp);
		}
	    else if ....
	    }
    }

    void  do_VCalendar(VObject *vcal)
    {
	VObjectIterator t;
	initPropIterator(&t,vcard);
	while (moreIteration(&t)) {
	    VObject *eachProp = nextVObject(&t);
	    //	The primarly purpose of this example is to
	    //  show how to iterate through a VCalendar VObject,
	    //	it is not meant to be efficient at all.
	    char *n = vObjectName(eachProp);
	    if (strcmp(n,VCDCreatedProp)==0) {
		do_DCreated(eachProp);
		}
	    else if (strcmp(n,VCVersionProp)==0) {
		do_Version(eachProp);
		}
	    else if (strcmp(n,VCTodoProp)==0) {
		do_Todo(eachProp);
		}
	    else if (strcmp(n,VCEventProp)==0) {
		do_Event(eachProp);
		}
	    else if ....
	    }
    }

    void do_Todo(VObject *vtodo) { ... }

    void do_Event(VObject *vevent) { ... }

    ...
    
* Property's Values and Properties
    The VObject APIs do not attempt to check for the
    correctness of the values of a property. Nor do they
    will prevent the user from attaching a non-VCard/VCalendar
    standard property to a VCard/VCalendar property. Take
    the example of line [11] of the example, "O.K" is not
    a valid value of VCStatusProp.  It is up to the application
    to accept or reject the value of a property.
    
* Output of printVObject
    PrintVObject pretty prints a VObject tree in human
    readable form. See the listing at the end of the file
    for an example output of printVObject on the example
    input file "Example.vcf".

    Note that binary data are not shown in the output of
    printVObject. Instead, a note is made ([raw data]) to
    indicate that there exists such a binary data.

* Note on Binary Data
    When the value of a property is a binary data, it is only
    useful to know the size of the binary data.

    In the case of the VCard/VCalendar parser, it chooses
    to represent the size information as a separate property
    called VCDataSizeProp whose value is the size of the binary
    data. The APIs sequence to construct the VObject subtree
    of line [44] of Example.vcf is

	    // VObject *vcard;
	    VObject *p1 = addProp(vcard,VCLogoProp);
	    (void) addProp(p1,VCGIFProp);
	    (void) addProp(p1,VCBASE64Prop);
	    VObject *p2 = addProp(p1,VCDataSizeProp);
	    (void) setVObjectLongValue(p2,1482);
	    setVObjectAnyValue(vcard,...pointer to binary data);
	
    Note the presence of VCBase64Prop will cause the
    writeVObject API to output the binary data as BASE64 text.
    For VCard/VCalendar application, having the VCBase64Prop
    property is pratically always neccessary for property with
    binary data as its value.
	
* Note on Quoted-Printable String
    String value with embedded newline are written out as
    quoted-prinatable string. It is therefore important
    to mark a property with a string value that has
    one or more embedded newlines, with the VCQutedPrintableProp
    property. e.g.
			
	// VObject *root;
	char *msg="To be\nor\nnot to be";
	VObject *p = addPropValue(root,VCDescriptionProp,msg);
	    // the following is how you mark a property with
	    //	a property. In this case, the marker is
	    //  VCQuotedPrintableProp
	    addProp(p,VCQuotedPrintableProp);

* Note on Unicode
    Although, the current parser takes ASCII text file only,
    string values are all stored as Unicode in the VObject tree.
    For now, when using the VObject APIs to construct a
    VObject tree, one should always convert ASCII string value
    to a Unicode string value:

	// VObject *root;
	VObject *p = addProp(root,VCSomeProp);
	setVObjectUStringZValue(p,fakeUnicode(someASCIIStringZvalue));

    An API is provided to simplify the above process:

	addPropValue(root,VCSomeProp,someASCIIStringZValue);

    Note that someASCIISTringZValue is automatically converted to
    Unicode by addPropValue API, where as, the former code
    sequence do an explicit call to fakeUnicode.

    To read back the value, one should use the vObjectUStringZValue
    API not vObjectStringZValue API. The value returned by the
    vObjectUStringZValue API is a Unicode string. If the application
    do not know how to handle Unicode string, it can use the
    fakeCString API to convert it back to ASCII string (as long
    as the conversion is meaningful).

    Note that fakeCString return a heap allocated memory. It is
    important to call deleteStr on fakeCString return value if
    it is not longer required (or there will be memory leak).

    NOTE: Unfortunately, at the point when this document is written,
    there is still no consensus on how Unicode is to be handled
    in the textual representation of VCard/VCalendar. So, there
    is no version of writeVObject and the parser to output and
    input Unicode textual representation of VCard/VCalendar.
   

Example.vcf
-----------
line
number Input Text (example.vcf)
------ ----------
1      BEGIN:VCALENDAR
2      DCREATED:19961102T100522
3      GEO:0,0
4      VERSION:1.0
5      BEGIN:VEVENT
6      DTSTART:19961103T000000
7      DTEND:20000101T000000
8      DESCRIPTION;QUOTED-PRINTABLE:To be =0A=
9      or =0A=
10     not to be
11     STATUS:O.K.
12     X-ACTION:No action required
13     DALARM:19961103T114500;5;3;Enjoy
14     MALARM:19970101T120000;;;johny@nowhere.com;Call Mom.
15     END:VEVENT
16
17     BEGIN:VTODO
18     DUE:19960614T0173000
19     DESCRIPTION:Relex.
20     END:VTODO
21
22     END:VCALENDAR
23
24     BEGIN:VCARD
25     N:Alden;Roland
26     FN:Roland H. Alden
27     ORG:AT&T;Versit Project Office
28     TITLE:Consultant
29     EMAIL;WORK;PREF;INTERNET:ralden@ralden.com
30     LABEL;DOM;POSTAL;PARCEL;HOME;WORK;QUOTED-PRINTABLE:Roland H. Alden=0A=
31     Suite 2208=0A=
32     One Pine Street=0A=
33     San Francisco, CA 94111
34     LABEL;POSTAL;PARCEL;HOME;WORK;QUOTED-PRINTABLE:Roland H. Alden=0A=
35     Suite 2208=0A=
36     One Pine Street=0A=
37     San Francisco, CA 94111=0A=
38     U.S.A.
39     TEL;WORK;PREF;MSG:+1 415 296 9106
40     TEL;WORK;FAX:+1 415 296 9016
41     TEL;MSG;CELL:+1 415 608 5981
42     ADR:;Suite 2208;One Pine Street;San Francisco;CA;94111;U.S.A.
43     SOUND:ROW-LAND H ALL-DIN
44     LOGO;GIF;BASE64:
45         R0lGODdhpgBOAMQAAP///+/v797e3s7Ozr29va2trZycnIyMjHt7e2NjY1JSUkJC
    ... 30 lines of BASE64 data not shown here.
76     END:VCARD


VObject Representation of Example.vcf:
-------------------------------------
line
in
text
file  VObject Tree as Printed by printVObject API
----  -------------------------------------------
1     VCALENDAR
2         DCREATED="19961102T100522"
3         GEO="0,0"
4         VERSION="1.0"
5         VEVENT
6             DTSTART="19961103T000000"
7             DTEND="20000101T000000"
8             DESCRIPTION="To be
9                     or
10                    not to be"
8                 QUOTED-PRINTABLE
11            STATUS="O.K."
12            X-ACTION="No action required"
13            DALARM
13                RUNTIME="19961103T114500"
13                SNOOZETIME="5"
13                REPEATCOUNT="3"
13                DISPLAYSTRING="Enjoy"
14            MALARM
14                RUNTIME="19970101T120000"
14                EMAIL="johny@nowhere.com"
14                NOTE="Call Mom"
17        VTODO
18            DUE="19960614T0173000"
19            DESCRIPTION="Relex."
24    VCARD
25        N
25            F="Alden"
25            G="Roland"
26        FN="Roland H. Alden"
27        ORG
27            ORGNAME="AT&T"
27            OUN="Versit Project Office"
28        TITLE="Consultant"
29        EMAIL="ralden@alden.com"
29            WORK
29            PREF
29            INTERNET
30        LABEL="Roland H. Alden
31                Suite 2208
32                One Pine Street
33                San Francisco, CA 94111"
30            DOM
30            POSTAL
30            PARCEL
30            HOME
30            WORK
30            QUOTED-PRINTABLE
34        LABEL="Roland H. Alden
35                Suite 2208
36                One Pine Street
37                San Francisco, CA 94111
38                U.S.A."
34            POSTAL
34            PARCEL
34            HOME
34            WORK
34            QUOTED-PRINTABLE
39        TEL="+1 415 296 9106"
39            WORK
39            PREF
39            MSG
40        TEL="+1 415 296 9016"
40            WORK
40            FAX
41        TEL="+1 415 608 5981"
41            MSG
41            CELL
42        ADR
42            EXT ADD="Suite 2208"
42            STREET="One Pine Street"
42            L="San Francisco"
42            R="CA"
42            PC="94111"
42            C="U.S.A."
43        SOUND="ROW-LAND H ALL-DIN"
44        LOGO=[raw data]
44            GIF
44            BASE64
44            DATASIZE=1482

