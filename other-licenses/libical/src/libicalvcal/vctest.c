
#include <stdio.h>
#include <string.h>
#include "vcc.h"

FILE *cfp;

void myMimeErrorHandler(char *s)
{
    printf("%s\n", s);
}

void main(int argc, char **argv)
{
    int testmem = 0;

	char * foo[2] = {"foo","alden.vcf"};

argc = 2;
argv = foo;

#ifdef _CONSOLE
    cfp = stdout;
    registerMimeErrorHandler(myMimeErrorHandler);
#else
    cfp = fopen("vctest.out", "w");
    if (!cfp) return;
#endif
    ++argv;
    while (--argc) {
	FILE *fp;
	if (strcmp(*argv,"-testmem") == 0) {
	    testmem = 1;
	    argv++;
	    continue;
	    }
	fprintf(cfp,"processing %s\n",*argv);
	fp = fopen(*argv,"r");
	if (!fp) {
	    fprintf(cfp,"error opening file\n");
	    }
	else {
	    VObject *v, *t;
	    FILE *ofp;
	    char buf[256];
	    char *p;
	    strcpy(buf,*argv);
	    p = strchr(buf,'.');
	    if (p) *p = 0;
	    strcat(buf,".out");
	    fprintf(cfp,"reading text input from '%s'...\n", *argv);
	    /*v = Parse_MIME_FromFile(fp); */
	    v = Parse_MIME_FromFileName(*argv);
		writeVObjectToFile(buf,v);
		cleanVObject(v);

		/*
	    fprintf(cfp,"pretty print internal format of '%s'...\n", *argv);
	    ofp = fopen(buf,"w");
	    while (v) {
		printVObject(cfp,v);
		if (testmem) {
		    char *s, *p;
		    fprintf(cfp,"test writing to mem...\n");
		    p = s  = writeMemVObject(0,0,v);
		    if (s) {
			while (*s) {
			    fputc(*s,ofp);
			    s++;
			    }
			free(p);
			}
		    }
		else {
		    writeVObject(ofp,v);
		    }
		t = v;
		v = nextVObjectInList(v);
		cleanVObject(t);
		}
		
	    fclose(ofp);
	    fclose(fp);
		*/
	    }
		
	cleanStrTbl();
	argv++;
	
	}
	
    if (cfp != stdout) fclose(cfp);

}

