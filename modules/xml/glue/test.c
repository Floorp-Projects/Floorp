

/* this is to test the expat parser and dom builder in standalone mode
   (i.e., without the rest of communicator).
   We read in an XML file, build the dom and write out the parse
   structure */

#include <stdio.h>
#include "xmlglue.h"

#define testIn  "foo.xml"
#define testOut "foo.out"

void outputXMLParseTree (FILE *out, XMLElement el) ;

void main () {
  FILE *stream = fopen(testIn, "r");
  FILE *out    = fopen(testOut, "w");
  char *string = getMem(512);
  XMLFile xmlf = getMem(sizeof(XMLFileStruct));
  xmlf->parser = XML_ParserCreate(NULL) ;
  XML_SetElementHandler(xmlf->parser, 
			(void (*)(void*, const char*, const char**))XMLDOM_StartHandler, 
			(void (*)(void*, const char*))XMLDOM_EndHandler);
  XML_SetCharacterDataHandler(xmlf->parser, 
			      (void (*)(void*, const char*, int))XMLDOM_CharHandler);
  XML_SetProcessingInstructionHandler(xmlf->parser, 
				      (void (*)(void*, const char*, const char*))XMLDOM_PIHandler);
  XML_SetUserData(xmlf->parser, xmlf);
  xmlf->status = 1;
  while (fgets(string, 512, stream)) {
    if (xmlf->status == 1) xmlf->status = XML_Parse(xmlf->parser, string, 512, 0);	
    memset(string, '\0', 512);
  }
  if (xmlf->status != 1) {
    const char* error = XML_ErrorString(XML_GetErrorCode(xmlf->parser));
    char*  buff  = getMem(150);
    int n = XML_GetErrorLineNumber(xmlf->parser);
    fprintf(out, "<B>Bad XML at line %i :</B><BR><P> ERROR = ", n);
    fprintf(out, error);
  } else {
    outputXMLParseTree(out, xmlf->top);
  }
  fclose(out);
  fclose(stream);
}

void outputXMLParseTree (FILE *out, XMLElement el) {
  XMLElement child = el->child;
  if (strcmp(el->tag, "xml:content") == 0) {
    fprintf(out, "\n\nContent : %s\n", el->content);
  } else {
    fprintf(out, "\n\nElement name = %s\n", el->tag);
    if (el->attributes) {
      int32 n = 0;
      fprintf(out, "Attributes : \n");
      while (*(el->attributes + n)) {
	char* attname = *(el->attributes + n);
	char* attval  = *(el->attributes + n + 1);
	fprintf(out, "%s = %s\n", attname, attval);
	n = n + 2;
      }
    }
  }
  while (el->child) {
    outputXMLParseTree(out, child);
    child = child->next;
  }
}


