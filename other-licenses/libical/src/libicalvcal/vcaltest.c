#include <stdio.h>
#include <string.h>
#include "vcaltmp.h"

#if 0
This testcase would generate a file call "frankcal.vcf" with
the following content:

BEGIN:VCALENDAR
DCREATED:19960523T100522
GEO:37.24,-17.87
PRODID:-//Frank Dawson/Hand Crafted In North Carolina//NONSGML Made By Hand//EN
VERSION:0.3
BEGIN:VEVENT
DTSTART:19960523T120000
DTEND:19960523T130000
DESCRIPTION;QUOTED-PRINTABLE:VERSIT PDI PR Teleconference/Interview =0A=
With Tom Streeter and Frank Dawson - Discuss VERSIT PDI project and vCard and vCalendar=0A=
activities with European Press representatives.
SUMMARY:VERSIT PDI PR Teleconference/Interview
SUBTYPE:PHONE CALL
STATUS:CONFIRMED
TRANSP:19960523T100522-4000F100582713-009251
UID:http://www.ibm.com/raleigh/fdawson/~c:\or2\orgfiles\versit.or2
DALARM:19960523T114500;5;3;Your Telecon Starts At Noon!!!;
MALARM:19960522T120000;;;fdawson@raleigh.ibm.com;Remember 05/23 Noon Telecon!!!;
PALARM:19960523T115500;;;c:\or2\organize.exe c:\or2\orgfiles\versit.or2;
X-LDC-OR2-OLE:c:\temp\agenda.doc
END:VEVENT

BEGIN:VTODO
DUE:19960614T0173000
DESCRIPTION:Review VCalendar helper API.
END:VTODO

END:VCALENDAR

#endif

FILE *cfp;

void testVcalAPIs() {
    FILE *fp;
    VObject *vcal, *vevent;
#if _CONSOLE
    cfp = stdout;
#else
    cfp = fopen("vcaltest.out","w");
#endif
    if (cfp == 0) return;
    vcal = vcsCreateVCal(
	"19960523T100522",
	"37.24,-17.87",
	"-//Frank Dawson/Hand Crafted In North Carolina//NONSGML Made By Hand//EN",
	0,
	"0.3"
	);

    vevent = vcsAddEvent(
	vcal,
	"19960523T120000",
	"19960523T130000",
	"VERSIT PDI PR Teleconference/Interview \nWith Tom Streeter and Frank Dawson - Discuss VERSIT PDI project and vCard and vCalendar\nactivities with European Press representatives.",
	"VERSIT PDI PR Teleconference/Interview",
	"PHONE CALL",
	0,
	"CONFIRMED",
	"19960523T100522-4000F100582713-009251",
	"http://www.ibm.com/raleigh/fdawson/~c:\\or2\\orgfiles\\versit.or2",
	0
	);
    
    vcsAddDAlarm(vevent, "19960523T114500", "5", "3",
	    "Your Telecon Starts At Noon!!!");
    vcsAddMAlarm(vevent, "19960522T120000", 0, 0, "fdawson@raleigh.ibm.com",
	    "Remember 05/23 Noon Telecon!!!");
    vcsAddPAlarm(vevent, "19960523T115500", 0 ,0,
	    "c:\\or2\\organize.exe c:\\or2\\orgfiles\\versit.or2");
    
    addPropValue(vevent, "X-LDC-OR2-OLE", "c:\\temp\\agenda.doc");

    vcsAddTodo(
	vcal,
	0,
	"19960614T0173000",
	0,
	"Review VCalendar helper API.",
	0,
	0,
	0,
	0,
	0,
	0
	);

    /* now do something to the resulting VObject */
    /* pretty print on stdout for fun */
    printVObject(cfp,vcal);
    /* open the output text file */

#define OUTFILE "frankcal.vcf"

    fp = fopen(OUTFILE, "w");
    if (fp) {
	/* write it in text form */
	writeVObject(fp,vcal);
	fclose(fp);
	}
    else {
	fprintf(cfp,"open output file '%s' failed\n", OUTFILE);
	}
    if (cfp != stdout) fclose(cfp);
    }

void main() {
    testVcalAPIs();
    }

