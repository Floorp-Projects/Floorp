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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


#include <stdio.h>
#include <string.h>

#if defined(WIN32)
#undef __STDC__
#include "fcntl.h"
#include "io.h"
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/fcntl.h>
#endif

#include "secutil.h"


#include "nspr.h"
#include "prtypes.h"
#include "prtime.h"
#include "prlong.h"

#include "pkcs11.h"

#include "pk11util.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

CK_ULONG systemFlags;
#define FLAG_NEGATE 0x80000000
#define FLAG_Verify 0x00000001
#define FLAG_VerifyFile 0x00000002

int ArgSize(ArgType type);
const char *constLookup(const char *bp, CK_ULONG *value, ConstType *type);

int
isNum(char c)
{
    return (c >= '0' && c <= '9');
}

int
isConst(const char *c)
{
    CK_ULONG value;
    ConstType type;

    constLookup(c, &value, &type);
    return type != ConstNone;
}

/*
 * see if the variable is really a 'size' function. This
 * function may modify var if it is a size function.
 */
char *
isSize(char *var, int *isArray)
{
    char *ptr = NULL;
    char *end;
    int array = 0;

    if (PL_strncasecmp(var,"sizeof(",/*)*/ 7) == 0) {
	ptr = var + 7;
    } else if (PL_strncasecmp(var,"size(",/*)*/ 5) == 0) {
	ptr = var + 5;
    } else if (PL_strncasecmp(var,"sizeofarray(",/*)*/ 12) == 0) {
	ptr = var + 12;
	array = 1;
    } else if (PL_strncasecmp(var,"sizea(",/*)*/ 6) == 0) {
	ptr = var + 6;
	array = 1;
    } else {
	return NULL;
    }
    end = strchr(ptr,/*(*/ ')') ;
    if (end == NULL) {
	return NULL;
    }
    if (isArray) *isArray = array;
    *end = 0;
    return ptr;
}
 
void
printConst(CK_ULONG value, ConstType type, int newLine)
{
    int i;

    for (i=0; i < constCount; i++) {
	if (consts[i].type == type && consts[i].value == value) {
	    printf("%s",consts[i].name);
	    break;
	}
	if (type == ConstNone && consts[i].value == value) {
	    printf("%s",consts[i].name);
	    break;
	}
    }
    if (i == constCount) {
	if ((type == ConstAvailableSizes) || (type == ConstCurrentSize)) {
	    printf("%lu",value);
	} else {
	    printf("Unknown %s (%lu:0x%lx)",constTypeString[type],value,value);
	}
    }
    if (newLine) {
	printf("\n");
    }
}

ConstType
getConstFromAttribute(CK_ATTRIBUTE_TYPE type)
{
    int i;

    for (i=0; i < constCount; i++) {
	if (consts[i].type == ConstAttribute && consts[i].value == type) {
	    return consts[i].attrType;
	}
    }
    return ConstNone;
}

void
printChars(const char *name, CK_ULONG size)
{
    CK_ULONG i;
    for (i=0; i < size; i++) {
	if (name[i] == 0) {
		break;
	}
	printf("%c",name[i]);
    }
    printf("\n");
}

#define DUMP_LEN 16
void printDump(const unsigned char *buf, int size)
{
    int i,j;

    for(i=0; i < size; i+= DUMP_LEN) {
	printf(" ");
	for (j=0; j< DUMP_LEN; j++) {
	    if (i+j < size) {
		printf("%02x ",buf[i+j]);
	    } else {
		printf("   ");
	    }
	} 
	for (j=0; j< DUMP_LEN; j++) {
	    if (i+j < size) {
		if (buf[i+j] < ' ' || buf[i+j] >= 0x7f) {
		    printf(".");
		} else {
		    printf("%c",buf[i+j]);
		}
	    } else {
		printf(" ");
	    }
	} 
	printf("\n");
    }
}

/*
 * free an argument structure
 */
void
argFreeData(Value *arg)
{
    if (arg->data && ((arg->type & ArgStatic) == 0)) {
	if ((arg->type & ArgMask) == ArgAttribute) {
	    int i;
	    CK_ATTRIBUTE *template = (CK_ATTRIBUTE *)arg->data;

	    for (i=0; i < arg->arraySize; i++) {
		free(template[i].pValue);
	    }
	}
	free(arg->data);
    }
    arg->type &= ~ArgStatic;
    arg->data = NULL;
}

void
argFree(Value *arg)
{
    if (arg == NULL) return;

    arg->reference--;
    if (arg->reference == 0) {
	if (arg->type & ArgFile) {
	    free(arg->filename);
	}
	argFreeData(arg);
	free (arg);
    }
}

/*
 * free and argument list
 */
void
parseFree(Value **ap)
{
    int i;
    for (i=0 ; i < MAX_ARGS; i++) {
	argFree(ap[i]);
    }
}

/*
 * getEnd: how for to the end of this argmument list?
 */
int
getEnd(const char *bp)
{
    int count = 0;

    while (*bp) {
        if (*bp == ' ' || *bp == '\t' || *bp == '\n') return count;
	count++;
	bp++;
    }
    return (count);
}


/*
 * strip: return the first none white space character
 */
const char *
strip(const char *bp)
{
    while (*bp && (*bp == ' ' || *bp == '\t' || *bp == '\n')) bp++;
    return bp;
}

/*
 * read in the next argument into dp ... don't overflow
 */
const char *
readChars(const char *bp, char *dp, int max )
{
    int count = 1;
    while (*bp) {
        if (*bp == ' ' || *bp == '\t' || *bp == '\n' ) {
	    *dp = 0;
	    return bp;
	}
	*dp++ = *bp++;
	if (++count == max) break;
    }
    while (*bp && (*bp != ' ' && *bp != '\t' && *bp != '\n')) bp++;
    *dp = 0;
    return (bp);
}

Value * varLookup(const char *bp, char *vname, int max, int *error);

CK_ULONG
getValue(const char *v, int *error)
{
    Value * varVal = NULL;
    CK_ULONG retVal = 0;
    ConstType type;
    char tvar[512];

    *error = 0;

    varVal = varLookup( v, tvar, sizeof(tvar), error);

    if (varVal) {
	if ((varVal->type & ArgMask) == ArgULong) {
	    retVal = *(CK_ULONG *)varVal->data;
	} else {
	    fprintf(stderr,"%s: is not a ulong\n", v);
	    *error = 1;
	}
	argFree(varVal);
	return retVal;
    }
    constLookup(v, &retVal, &type);
    return retVal;
}

Value *
NewValue(ArgType type, CK_ULONG arraySize)
{
    Value *value;

    value = (Value *)malloc(sizeof(Value));
    if (!value) return NULL;
    value->size = ArgSize(type)*arraySize;
    value->type = type;
    value->filename = NULL;
    value->constType = ConstNone;
    value->data = (void *)malloc(value->size);
    if (!value->data) {
	free(value);
	return NULL;
    }
    value->reference = 1;
    value->arraySize = arraySize;

    memset(value->data, 0, value->size);
    return value;
}

#define INVALID_INDEX 0xffffffff

CK_ULONG
handleArray(char *vname, int *error)
{
    char *bracket;
    CK_ULONG index = INVALID_INDEX;

    if ((bracket = strchr(vname,'[')) != 0) {
	char *tmpv = bracket+1;
	*bracket = 0;
	bracket = strchr(tmpv,']');

	if (bracket == 0) {
	    fprintf(stderr,"%s: missing closing brace\n", vname);
	    return INVALID_INDEX;
	}
	*bracket = 0;

	index = getValue(tmpv, error);
	if (*error == 1) {
	    return INVALID_INDEX;
	} else if (index == INVALID_INDEX) {
	    fprintf(stderr, "%s: 0x%x is an invalid index\n",vname,index);
	    *error = 1;
	}
    }
    return index;
}

void *
makeArrayTarget(const char *vname, const Value *value, CK_ULONG index)
{
    char * target;
    CK_ULONG elementSize;

    if (index >= (CK_ULONG)value->arraySize) {
	fprintf(stderr, "%s[%d]: index larger than array size (%d)\n",
		vname, index, value->arraySize);
	return NULL;
    }

    target = (char *)value->data;
    elementSize = value->size/value->arraySize;
    target += index * elementSize;
    return target;
}

/*
 * look up a variable from the variable chain
 */
static Variable *varHead = NULL;
Value *
varLookup(const char *bp, char *vname, int max, int *error)
{
    Variable *current;
    CK_ULONG index = INVALID_INDEX;
    int isArray = 0;
    char *ptr;
    *error = 0;

    if (bp != NULL) {
	readChars(bp, vname, max);
    } 

    /* don't make numbers into variables */
    if (isNum(vname[0])) {
	return NULL;
    }
    /* nor consts */
    if (isConst(vname)) {
	return NULL;
    }
    /* handle sizeof() */
    if ((ptr = isSize(vname, &isArray)) != NULL) {
	CK_ULONG size;
	Value  *targetValue = NULL;
	Value  *sourceValue = varLookup(NULL, ptr, 0, error);
	if (!sourceValue) {
	   if (*error == 0) {
		/* just didn't find it */
		*error = 1;
		fprintf(stderr,"Couldn't find variable %s to take size of\n",
			ptr);
		return NULL;
	   }
	}
	size = isArray ? sourceValue->arraySize : sourceValue->size;
	targetValue = NewValue(ArgULong,1);
	memcpy(targetValue->data, &size, sizeof(size));

	return targetValue;
    }

    /* modifies vname */
    index = handleArray(vname, error);
    if (*error == 1) {
	return NULL;
    }

    for (current = varHead; current; current = current->next) {
	if (PL_strcasecmp(current->vname, vname) == 0) {
	    char *target;
	    if (index == INVALID_INDEX) {
		(current->value->reference)++;
		return current->value;
	    }
	    target = makeArrayTarget(vname, current->value, index);
	    if (target) {
		Value *element = NewValue(current->value->type, 1);
		if (!element) {
		    fprintf(stderr, "MEMORY ERROR!\n");
		    *error = 1;
		}
		argFreeData(element);
		element->data = target;
		element->type |= ArgStatic;
		return element;
	    }
	    *error = 1;
	    return NULL;
	}
    }
    return NULL;
}

static CK_RV 
list(void)
{
    Variable *current;

    if (varHead) {
    	printf(" %10s\t%16s\t%8s\tSize\tElements\n","Name","Type","Const");
    } else {
    	printf(" no variables set\n");
    }

    for (current = varHead; current; current = current->next) {
    	printf(" %10s\t%16s\t%8s\t%d\t%d\n", current->vname,
	    valueString[current->value->type&ArgMask],
	    constTypeString[current->value->constType],
	    current->value->size, current->value->arraySize);
    }
    return CKR_OK;
}

CK_RV
printFlags(const char *s, CK_ULONG flags, ConstType type)
{
    CK_ULONG i;
    int needComma = 0;

    printf("%s",s);
    for (i=1; i ; i=i << 1) {
	if (flags & i) {
	   printf("%s",needComma?",":"");
	   printConst(i, type, 0);
	   needComma=1;
	}
    }
    if (!needComma) {
	printf("Empty");
    }
    printf("\n");
    return CKR_OK;
}

/*
 * add a new variable to the chain
 */
const char *
AddVariable(const char *bp, Value **ptr)
{
    char vname[512];
    Variable *current;
    int index = INVALID_INDEX;
    int size;
    int error = 0;

    bp = readChars(bp,vname,sizeof(vname));

    /* don't make numbers into variables */
    if (isNum(vname[0])) {
	return bp;
    }
    /* or consts */
    if (isConst(vname)) {
	return bp;
    }
    /* or NULLs */
    if (vname[0] == 0) {
	return bp;
    }
    /* or sizeof */
    if (isSize(vname, NULL)) {
	return bp;
    }
    /* arrays values should be written back to the original */
    index = handleArray(vname, &error);
    if (error == 1) {
	return bp;
    }


    for (current = varHead; current; current = current->next) {
	if (PL_strcasecmp(current->vname,vname) == 0) {
	    char *target;
	    /* found a complete object, return the found one */
	    if (index == INVALID_INDEX) {
		argFree(*ptr);
		*ptr = current->value;
		return bp;
	    }
	    /* found an array, update the array element */
	    target = makeArrayTarget(vname, current->value, index);
	    if (target) {
		memcpy(target, (*ptr)->data, (*ptr)->size);
		argFreeData(*ptr);
		(*ptr)->data = target;
		(*ptr)->type |= ArgStatic;
	    }
	    return bp;
	}
    }

    /* we are looking for an array and didn't find one */
    if (index != INVALID_INDEX) {
	return bp;
    }


    current = (Variable *)malloc(sizeof(Variable));
    size = strlen(vname);
    current->vname = (char *)malloc(size+1);
    strcpy(current->vname,vname);
    current->value = *ptr;
    (*ptr)->reference++;

    current->next = varHead;
    varHead = current;
    return bp;
}

ArgType
FindTypeByName(const char *typeName)
{
    int i;

    for (i=0; i < valueCount; i++) {
	if (PL_strcasecmp(typeName,valueString[i]) == 0) {
	    return (ArgType) i;
	}
	if (valueString[i][0] == 'C' && valueString[i][1] == 'K' &&
	   valueString[i][2] == '_' && 
			(PL_strcasecmp(typeName,&valueString[i][3]) == 0)) {
	    return (ArgType) i;
	}
    }
    return ArgNone;
}

CK_RV 
ArrayVariable(const char *bp, const char *typeName, CK_ULONG count)
{
    ArgType type;
    Value *value; /* new Value */

    type = FindTypeByName(typeName);
    if (type == ArgNone) {
	fprintf(stderr,"Invalid type (%s)\n", typeName);
	return CKR_FUNCTION_FAILED;
    }
    value = NewValue(type, count);
    (void) AddVariable(bp, &value);
    return CKR_OK;
}

#define MAX_TEMPLATE 25

CK_RV 
ArrayTemplate(const char *bp, char *attributes)
{
    char aname[512];
    CK_ULONG attributeTypes[MAX_TEMPLATE];
    CK_ATTRIBUTE *template;
    Value *value; /* new Value */
    char *ap;
    int i, count = 0;

    memcpy(aname,attributes,strlen(attributes)+1);

    for (ap = aname, count = 0; ap && *ap && count < MAX_TEMPLATE; count++) {
	char *cur = ap;
	ConstType type;

	ap = strchr(ap,',');
	if (ap) {
	    *ap++ = 0;
	}

	(void)constLookup(cur, &attributeTypes[count], &type);
	if ((type != ConstAttribute) && (type != ConstNone)) {
	   fprintf(stderr, "Unknown Attribute %s\n", cur);
	   return CKR_FUNCTION_FAILED;
	}
    }

    value = NewValue(ArgAttribute, count);

    template = (CK_ATTRIBUTE *)value->data;
    for (i=0; i < count ; i++) {
	template[i].type = attributeTypes[i];
    }
    (void) AddVariable(bp, &value);
    return CKR_OK;
}

CK_RV
BuildTemplate(Value *vp)
{
    CK_ATTRIBUTE *template = (CK_ATTRIBUTE *)vp->data;
    int i;

    for (i=0; i < vp->arraySize; i++) {
	if (((signed long)template[i].ulValueLen) > 0) {
	    if (template[i].pValue) free(template[i].pValue);
	    template[i].pValue = malloc(template[i].ulValueLen);
	}
    }
    return CKR_OK;
}

CK_RV
SetTemplate(Value *vp, CK_ULONG index, CK_ULONG value)
{
    CK_ATTRIBUTE *template = (CK_ATTRIBUTE *)vp->data;
    int isbool = 0;
    CK_ULONG len;
    ConstType attrType;

    if (index >= (CK_ULONG) vp->arraySize) {
	fprintf(stderr,"index (%lu) greater than array (%d)\n", 
						index, vp->arraySize);
	return CKR_ARGUMENTS_BAD;
    }
    attrType =  getConstFromAttribute(template[index].type);

    if (attrType == ConstNone) {
	fprintf(stderr,"can't set index (%lu) because ", index);
	printConst(template[index].type,ConstAttribute, 0);
	fprintf(stderr, " is not a CK_BBOOL or CK_ULONG\n");
	return CKR_ARGUMENTS_BAD;
    }
    isbool = (attrType == ConstBool);
    len = isbool ? sizeof (CK_BBOOL) : sizeof(CK_ULONG);
    if ((template[index].ulValueLen != len) || (template[index].pValue)) {
	free(template[index].pValue);
	template[index].pValue = malloc(len);
	template[index].ulValueLen = len;
    }
    if (isbool) {
	*(CK_BBOOL *)template[index].pValue = (CK_BBOOL) value;
    } else {
	*(CK_ULONG *)template[index].pValue = (CK_ULONG) value;
    }
    return CKR_OK;

}

CK_RV
NewMechanism(const char *bp, CK_ULONG mechType)
{
    Value *value; /* new Value */
    CK_MECHANISM *mechanism;

    value = NewValue(ArgMechanism, 1);
    mechanism = (CK_MECHANISM *)value->data;
    mechanism->mechanism = mechType;
    mechanism->pParameter = NULL;
    mechanism->ulParameterLen = 0;
    (void) AddVariable(bp, &value);
    return CKR_OK;
}

/*
 * add a new variable to the chain
 */
CK_RV
DeleteVariable(const char *bp)
{
    char vname[512];
    Variable **current;

    bp = readChars(bp,vname,sizeof(vname));

    for (current = &varHead; *current; current = &(*current)->next) {
	if (PL_strcasecmp((*current)->vname,vname) == 0) {
	        argFree((*current)->value);
		*current = (*current)->next;
		break;
	}
    }
    return CKR_OK;
}

/*
 * convert an octal value to integer
 */   
CK_ULONG
otoi(const char *o)
{
    CK_ULONG value = 0;

    while (*o) {
	if ((*o >= '0') && (*o <= '7')) {
	    value = (value << 3) | (unsigned)(*o - '0');
	} else {
	    break;
	}
    }
    return value;
}

/*
 * convert a hex value to integer
 */   
CK_ULONG
htoi(const char *x)
{
    CK_ULONG value = 0;

    while (*x) {
	if ((*x >= '0') && (*x <= '9')) {
	    value = (value << 4) | (unsigned)(*x - '0');
	} else if ((*x >= 'a') && (*x <= 'f')) {
	    value = (value << 4) | (unsigned)(*x - 'a');
	} else if ((*x >= 'A') && (*x <= 'F')) {
	    value = (value << 4) | (unsigned)(*x - 'A');
	} else {
	    break;
	}
    }
    return value;
}


/*
 * look up or decode a constant value
 */
const char *
constLookup(const char *bp, CK_ULONG *value, ConstType *type)
{
    char vname[512];
    int i;

    bp = readChars(bp,vname,sizeof(vname));

    for (i=0; i < constCount; i++) {
	if ((PL_strcasecmp(consts[i].name,vname) == 0) ||
		PL_strcasecmp(consts[i].name+5,vname) == 0) {
	    *value = consts[i].value;
	    *type = consts[i].type;
	    return bp;
	}
    }

    *type = ConstNone;
    if (vname[0] == '0' && vname[1] == 'X') {
	*value = htoi(&vname[2]);
    } else if (vname[0] == '0') {
	*value = otoi(&vname[1]);
    } else {
    	*value = atoi(vname);
    }
    return bp;
}

int
ArgSize(ArgType type)
{
	int size=0;
	type &= ArgMask;

	switch (type) {
    	case ArgNone:
	    size = 0;
	    break;
    	case ArgULong:
	    size = sizeof(CK_ULONG);
	    break;
    	case ArgVar:
	    size = 1; /* get's changed later */
	    break;
    	case ArgChar:
    	case ArgUTF8:
	    size = 1;
	    break;
    	case ArgInfo:
	    size = sizeof(CK_INFO);
	    break;
    	case ArgSlotInfo:
	    size = sizeof(CK_SLOT_INFO);
	    break;
    	case ArgTokenInfo:
	    size = sizeof(CK_TOKEN_INFO);
	    break;
    	case ArgSessionInfo:
	    size = sizeof(CK_SESSION_INFO);
	    break;
    	case ArgAttribute:
	    size = sizeof(CK_ATTRIBUTE);
	    break;
    	case ArgMechanism:
	    size = sizeof(CK_MECHANISM);
	    break;
    	case ArgMechanismInfo:
	    size = sizeof(CK_MECHANISM_INFO);
	    break;
	case ArgInitializeArgs:
	    size = sizeof(CK_C_INITIALIZE_ARGS);
	    break;
	case ArgFunctionList:
	    size = sizeof(CK_FUNCTION_LIST);
	    break;
	default:
	    break;
	}

	return (size);
}

CK_RV
restore(const char *filename,Value *ptr)
{
    int fd,size;

    fd = open(filename,O_RDONLY|O_BINARY);
    if (fd < 0) {
	perror(filename);
	return CKR_FUNCTION_FAILED;
    }

    size = read(fd,ptr->data,ptr->size);
    if (systemFlags & FLAG_VerifyFile) {
	printDump(ptr->data,ptr->size);
    }
    if (size < 0) {
	perror(filename);
	return CKR_FUNCTION_FAILED;
    } else if (size != ptr->size) {
	fprintf(stderr,"%s: only read %d bytes, needed to read %d bytes\n",
			filename,size,ptr->size);
	return CKR_FUNCTION_FAILED;
    }
    close(fd);
    return CKR_OK;
}

CK_RV
save(const char *filename,Value *ptr)
{
    int fd,size;

    fd = open(filename,O_WRONLY|O_BINARY|O_CREAT,0666);
    if (fd < 0) {
	perror(filename);
	return CKR_FUNCTION_FAILED;
    }

    size = write(fd,ptr->data,ptr->size);
    if (size < 0) {
	perror(filename);
	return CKR_FUNCTION_FAILED;
    } else if (size != ptr->size) {
	fprintf(stderr,"%s: only wrote %d bytes, need to write %d bytes\n",
			filename,size,ptr->size);
	return CKR_FUNCTION_FAILED;
    }
    close(fd);
    return CKR_OK;
}

CK_RV
printArg(Value *ptr,int arg_number)
{
    ArgType type = ptr->type & ArgMask;
    CK_INFO *info;
    CK_SLOT_INFO    *slotInfo;
    CK_TOKEN_INFO   *tokenInfo;
    CK_SESSION_INFO *sessionInfo;
    CK_ATTRIBUTE    *attribute;
    CK_MECHANISM    *mechanism;
    CK_MECHANISM_INFO    *mechanismInfo;
    CK_C_INITIALIZE_ARGS *initArgs;
    CK_FUNCTION_LIST *functionList;
    CK_RV ckrv = CKR_OK;
    ConstType constType;

    if (arg_number) {
	printf("Arg %d: \n",arg_number);
    }
    if (ptr->arraySize > 1) {
	Value element;
	int i;
	int elementSize = ptr->size/ptr->arraySize;
	char *dp = (char *)ptr->data;

	/* build a temporary Value to hold a single element */
	element.type = type;
	element.constType = ptr->constType;
	element.size = elementSize;
	element.filename = ptr->filename;
	element.reference = 1;
	element.arraySize = 1;
	for (i=0; i < ptr->arraySize; i++) {
	    printf(" -----[ %d ] -----\n", i);
	    element.data = (void *) &dp[i*elementSize];
	    (void) printArg(&element, 0);
	}
	return ckrv;
    }
    if (ptr->data == NULL) {
	printf(" NULL ptr to a %s\n", valueString[type]);
	return ckrv;
    }
    switch (type) {
    case ArgNone:
	printf(" None\n");
	break;
    case ArgULong:
	printf(" %lu (0x%lx)\n", *((CK_ULONG *)ptr->data),
			*((CK_ULONG *)ptr->data));
	if (ptr->constType != ConstNone) {
	    printf(" ");
	    printConst(*(CK_ULONG *)ptr->data,ptr->constType,1);
	}
	break;
    case ArgVar:
	printf(" %s\n",(char *)ptr->data);
	break;
    case ArgUTF8:
	printf(" %s\n",(char *)ptr->data);
	break;
    case ArgChar:
	printDump(ptr->data,ptr->size);
	break;
    case ArgInfo:
#define VERSION(x) (x).major, (x).minor
	info = (CK_INFO *)ptr->data;
	printf(" Cryptoki Version: %d.%02d\n",
		VERSION(info->cryptokiVersion));
	printf(" Manufacturer ID: ");
	printChars(info->manufacturerID,sizeof(info->manufacturerID));
	printFlags(" Flags: ", info->flags, ConstInfoFlags);
	printf(" Library Description: ");
	printChars(info->libraryDescription,sizeof(info->libraryDescription));
	printf(" Library Version: %d.%02d\n",
		VERSION(info->libraryVersion));
	break;
    case ArgSlotInfo:
	slotInfo = (CK_SLOT_INFO *)ptr->data;
	printf(" Slot Description: ");
	printChars(slotInfo->slotDescription,sizeof(slotInfo->slotDescription));
	printf(" Manufacturer ID: ");
	printChars(slotInfo->manufacturerID,sizeof(slotInfo->manufacturerID));
	printFlags(" Flags: ", slotInfo->flags, ConstSlotFlags);
	printf(" Hardware Version: %d.%02d\n",
		VERSION(slotInfo->hardwareVersion));
	printf(" Firmware Version: %d.%02d\n",
		VERSION(slotInfo->firmwareVersion));
	break;
    case ArgTokenInfo:
	tokenInfo = (CK_TOKEN_INFO *)ptr->data;
	printf(" Label: ");
	printChars(tokenInfo->label,sizeof(tokenInfo->label));
	printf(" Manufacturer ID: ");
	printChars(tokenInfo->manufacturerID,sizeof(tokenInfo->manufacturerID));
	printf(" Model: ");
	printChars(tokenInfo->model,sizeof(tokenInfo->model));
	printf(" Serial Number: ");
	printChars(tokenInfo->serialNumber,sizeof(tokenInfo->serialNumber));
	printFlags(" Flags: ", tokenInfo->flags, ConstTokenFlags);
	printf(" Max Session Count: ");
	printConst(tokenInfo->ulMaxSessionCount, ConstAvailableSizes, 1);
	printf(" Session Count: ");
	printConst(tokenInfo->ulSessionCount, ConstCurrentSize, 1);
	printf(" RW Session Count: ");
	printConst(tokenInfo->ulMaxRwSessionCount, ConstAvailableSizes, 1);
	printf(" Max Pin Length : ");
	printConst(tokenInfo->ulMaxPinLen, ConstCurrentSize, 1);
	printf(" Min Pin Length : ");
	printConst(tokenInfo->ulMinPinLen, ConstCurrentSize, 1);
	printf(" Total Public Memory: ");
	printConst(tokenInfo->ulTotalPublicMemory, ConstAvailableSizes, 1);
	printf(" Free Public Memory: ");
	printConst(tokenInfo->ulFreePublicMemory, ConstCurrentSize, 1);
	printf(" Total Private Memory: ");
	printConst(tokenInfo->ulTotalPrivateMemory, ConstAvailableSizes, 1);
	printf(" Free Private Memory: ");
	printConst(tokenInfo->ulFreePrivateMemory, ConstCurrentSize, 1);
	printf(" Hardware Version: %d.%02d\n",
		VERSION(tokenInfo->hardwareVersion));
	printf(" Firmware Version: %d.%02d\n",
		VERSION(tokenInfo->firmwareVersion));
	printf(" UTC Time: ");
	printChars(tokenInfo->utcTime,sizeof(tokenInfo->utcTime));
	break;
    case ArgSessionInfo:
	sessionInfo = (CK_SESSION_INFO *)ptr->data;
	printf(" SlotID: 0x%08lx\n", sessionInfo->slotID);
	printf(" State: ");
	printConst(sessionInfo->state, ConstSessionState, 1);
	printFlags(" Flags: ", sessionInfo->flags, ConstSessionFlags);
	printf(" Device error: %lu 0x%08lx\n",sessionInfo->ulDeviceError,
			sessionInfo->ulDeviceError);
	break;
    case ArgAttribute:
	attribute = (CK_ATTRIBUTE *)ptr->data;
	printf(" Attribute Type: ");
	printConst(attribute->type, ConstAttribute, 1);
	printf(" Attribute Data: ");
	if (attribute->pValue == NULL) {
	    printf("NULL\n");
	    printf("Attribute Len: %lu\n",attribute->ulValueLen);
	} else {
	    constType = getConstFromAttribute(attribute->type);
	    if (constType != ConstNone) {
		CK_ULONG value = (constType == ConstBool) ?
		    *(CK_BBOOL *)attribute->pValue :
		    *(CK_ULONG *)attribute->pValue;
		printConst(value, constType, 1);
	    } else {
		printf("\n");
		printDump(attribute->pValue, attribute->ulValueLen);
	    }
	}
	break;
    case ArgMechanism:
	mechanism = (CK_MECHANISM *)ptr->data;
	printf(" Mechanism Type: ");
	printConst(mechanism->mechanism, ConstMechanism, 1);
	printf(" Mechanism Data:\n");
	printDump(mechanism->pParameter, mechanism->ulParameterLen);
	break;
    case ArgMechanismInfo:
	mechanismInfo = (CK_MECHANISM_INFO *)ptr->data;
	printf(" Minimum Key Size: %ld\n",mechanismInfo->ulMinKeySize);
	printf(" Maximum Key Size: %ld\n",mechanismInfo->ulMaxKeySize);
	printFlags(" Flags: ", mechanismInfo->flags, ConstMechanismFlags);
	break;
    case ArgInitializeArgs:
	initArgs = (CK_C_INITIALIZE_ARGS *)ptr->data;
	printFlags(" Flags: ", initArgs->flags, ConstInitializeFlags);
    case ArgFunctionList:
	functionList = (CK_FUNCTION_LIST *)ptr->data;
	printf(" Version: %d.%02d\n", VERSION(functionList->version));
#ifdef notdef
#undef CK_NEED_ARG_LIST
#define CK_PKCS11_FUNCTION_INFO(func) \
	printf(" %s: 0x%08lx\n", #func, (unsigned long) functionList->func );
#include "pkcs11f.h"
#undef CK_NEED_ARG_LIST
#undef CK_PKCS11_FUNCTION_INFO
#endif
    default:
	ckrv = CKR_ARGUMENTS_BAD;
	break;
    }

    return ckrv;
}


/*
 * Feeling ambitious? turn this whole thing into lexx yacc parser
 * with full expressions.
 */
Value **
parseArgs(int index, const char * bp)
{
    const Commands *cp = &commands[index];
    int size = strlen(cp->fname);
    int i;
    CK_ULONG value;
    char vname[512];
    Value **argList,*possible;
    ConstType constType;

    /*
     * skip pass the command
     */
    if ((cp->fname[0] == 'C') && (cp->fname[1] == '_') && (bp[1] != '_')) {
	size -= 2;
    }
    bp += size;

    /*
     * Initialize our argument list
     */
    argList = (Value **)malloc(sizeof(Value*)*MAX_ARGS);
    for (i=0; i < MAX_ARGS; i++) { argList[i] = NULL; }

    /*
     * Walk the argument list parsing it...
     */
    for (i=0 ;i < MAX_ARGS; i++) {
	ArgType type = cp->args[i] & ArgMask;
	int error;

        /* strip blanks */
        bp = strip(bp);

	/* if we hit ArgNone, we've nabbed all the arguments we need */
	if (type == ArgNone) {
	    break;
	}

	/* if we run out of space in the line, we weren't given enough
	 * arguments... */
	if (*bp == '\0') {
	    /* we're into optional arguments, ok to quit now */
	    if (cp->args[i] & ArgOpt) {
		break;
	    }
	    fprintf(stderr,"%s: only %d args found,\n",cp->fname,i);
	    parseFree(argList);
	    return NULL;
	}

	/*
	 * look up the argument in our variable list first... only 
	 * exception is the new argument type for set...
	 */
	error = 0;
	if ((cp->args[i] != (ArgVar|ArgNew)) && 
			(possible = varLookup(bp,vname,sizeof(vname),&error))) {
	   /* ints are only compatible with other ints... all other types
	    * are interchangeable... */
	   if (type != ArgVar) { /* ArgVar's match anyone */
		if ((type == ArgULong) ^ 
				((possible->type & ArgMask) == ArgULong)) {
		    fprintf(stderr,"%s: Arg %d incompatible type with <%s>\n",
				cp->fname,i+1,vname);
		    argFree(possible);
		    parseFree(argList);
		    return NULL;
		}
		/*
		 * ... that is as long as they are big enough...
		 */
		if (ArgSize(type) > possible->size) {
		    fprintf(stderr,
	        "%s: Arg %d %s is too small (%d bytes needs to be %d bytes)\n",
	    		cp->fname,i+1,vname,possible->size,ArgSize(type));
		    argFree(possible);
		    parseFree(argList);
		    return NULL;
		}
	   }
	
	   /* everything looks kosher here, use it */	
	   argList[i] = possible;

	   bp = readChars(bp,vname,sizeof(vname));
	   if (cp->args[i] & ArgOut) {
		possible->type |= ArgOut;
	   }
	   continue;
	}

	if (error == 1) {
	    parseFree(argList);
	    return NULL;
	}

	/* create space for our argument */
	argList[i] = NewValue(type, 1);

        if ((PL_strncasecmp(bp, "null", 4) == 0)  && ((bp[4] == 0) 
		|| (bp[4] == ' ') || (bp[4] =='\t') || (bp[4] =='\n'))) {
	    if (cp->args[i] == ArgULong) {
		fprintf(stderr, "%s: Arg %d CK_ULONG can't be NULL\n",
	    							cp->fname,i+1);
		parseFree(argList);
		return NULL;
	    }
	    argFreeData(argList[i]);
	    argList[i]->data = NULL;
	    argList[i]->size = 0;
	    bp += 4;
	    if (*bp) bp++;
	    continue;
        }

	/* if we're an output variable, we need to add it */
	if (cp->args[i] & ArgOut) {
            if (PL_strncasecmp(bp,"file(",5) == 0 /* ) */ ) {
	        char filename[512];
		bp = readChars(bp+5,filename,sizeof(filename));
		size = PL_strlen(filename);
	    	if ((size > 0) && (/* ( */filename[size-1] == ')')) {
		    filename[size-1] = 0; 
		}
		filename[size] = 0; 
		argList[i]->filename = (char *)malloc(size+1);

		PL_strcpy(argList[i]->filename,filename);

	    	argList[i]->type |= ArgOut|ArgFile;
		break;
	    }
	    bp = AddVariable(bp,&argList[i]);
	    argList[i]->type |= ArgOut;
	    continue;
	} 

        if (PL_strncasecmp(bp, "file(", 5) == 0 /* ) */ ) {
	    char filename[512];

	    bp = readChars(bp+5,filename,sizeof(filename));
	    size = PL_strlen(filename);
	    if ((size > 0) && ( /* ( */ filename[size-1] == ')')) {
		filename[size-1] = 0; 
	    }

	    if (restore(filename,argList[i]) != CKR_OK) {
		parseFree(argList);
		return NULL;
	    }
	    continue;
	}

	switch (type) {
    	case ArgULong:
	     bp = constLookup(bp, &value, &constType);
	     *(int *)argList[i]->data = value;
	     argList[i]->constType = constType;
	     break;
    	case ArgVar:
	     argFreeData(argList[i]);
	     size = getEnd(bp)+1;
	     argList[i]->data = (void *)malloc(size);
	     argList[i]->size = size;
	     /* fall through */
    	case ArgInfo:
    	case ArgSlotInfo:
    	case ArgTokenInfo:
    	case ArgSessionInfo:
    	case ArgAttribute:
    	case ArgMechanism:
    	case ArgMechanismInfo:
    	case ArgInitializeArgs:
	case ArgUTF8:
	case ArgChar:
	     bp = readChars(bp,(char *)argList[i]->data,argList[i]->size);
    	case ArgNone:
	default:
	     break;
	}
    }

    return argList;
}

/* lookup the command in the array */
int
lookup(const char *buf)
{
    int size,i;
    int buflen;

    buflen = PL_strlen(buf);

    for ( i = 0; i < commandCount; i++) {
	size = PL_strlen(commands[i].fname);

	if (size <= buflen) {
	    if (PL_strncasecmp(buf,commands[i].fname,size) == 0) {
		return i;
	    }
	}
	if (size-2 <= buflen) {
	    if (commands[i].fname[0] == 'C' && commands[i].fname[1] == '_' &&
		(PL_strncasecmp(buf,&commands[i].fname[2],size-2) == 0)) {
		return i;
	    }
	}
    }
    fprintf(stderr,"Can't find command %s\n",buf);
    return -1;
}

void
putOutput(Value **ptr)
{
    int i;

    for (i=0; i < MAX_ARGS; i++) {
	ArgType type;

	if (ptr[i] == NULL) break;

	type  = ptr[i]->type;

	ptr[i]->type &= ~ArgOut;
	if (type == ArgNone) {
	    break;
	}
	if (type & ArgOut) {
	    (void) printArg(ptr[i],i+1);
	}
	if (type & ArgFile) {
	    save(ptr[i]->filename,ptr[i]);
	    free(ptr[i]->filename);
	    ptr[i]->filename= NULL; /* paranoia */
	}
    }
}
	   
CK_RV
unloadModule(Module *module)
{
   
   if (module->library) {
	PR_UnloadLibrary(module->library);
   }

   module->library = NULL;
   module->functionList = NULL;

   return CKR_OK;
}

CK_RV
loadModule(Module *module, char *library)
{
   PRLibrary *newLibrary;
   CK_C_GetFunctionList getFunctionList;
   CK_FUNCTION_LIST *functionList;
   CK_RV ckrv;

   newLibrary = PR_LoadLibrary(library);
   if (!newLibrary) {
	fprintf(stderr,"Couldn't load library %s\n",library);
	return CKR_FUNCTION_FAILED;
   }
   getFunctionList = (CK_C_GetFunctionList) 
			PR_FindSymbol(newLibrary,"C_GetFunctionList");
   if (!getFunctionList) {
	fprintf(stderr,"Couldn't find \"C_GetFunctionList\" in %s\n",library);
	return CKR_FUNCTION_FAILED;
   }

   ckrv = (*getFunctionList)(&functionList);
   if (ckrv != CKR_OK) {
	return ckrv;
   }
   
   if (module->library) {
	PR_UnloadLibrary(module->library);
   }

   module->library = newLibrary;
   module->functionList = functionList;

   return CKR_OK;
}

static void
printHelp(int index, int full)
{
   int j;
   printf(" %s", commands[index].fname);
   for (j=0; j < MAX_ARGS; j++) {
	ArgType type = commands[index].args[j] & ArgMask;
	if (type == ArgNone) {
	    break;
	}
	printf(" %s", valueString[type]);
   }
   printf("\n");
   printf(" %s\n",commands[index].helpString);
}

/* add Topical help here ! */
static CK_RV
printTopicHelp(char *topic)
{
    return CKR_DATA_INVALID;
}

static CK_RV
printGeneralHelp(void)
{
    int i;
    printf(" To get help on commands, select from the list below:");
    for ( i = 0; i < commandCount; i++) {
       if (i % 5 == 0) printf("\n");
       printf("%s,", commands[i].fname);
    }
    printf("\n");
    /* print help topics */
   return CKR_OK;
}

CK_RV run(char *); 

/*
 * Actually dispatch the function... Bad things happen
 * if these don't match the commands array.
 */
CK_RV
do_func(int index, Value **a)
{
    int value, helpIndex;
    static Module module = { NULL, NULL} ;
    CK_FUNCTION_LIST *func = module.functionList;

    switch (commands[index].fType) {
    case F_C_Initialize:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_Initialize((void *)a[0]->data);
    case F_C_Finalize:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_Finalize((void *)a[0]->data);
    case F_C_GetInfo:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_GetInfo((CK_INFO *)a[0]->data);
    case F_C_GetFunctionList:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_GetFunctionList((CK_FUNCTION_LIST **)a[0]->data);
    case F_C_GetSlotList:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_GetSlotList((CK_BBOOL)*(CK_ULONG *)a[0]->data,
					(CK_SLOT_ID *)a[1]->data,
					(CK_LONG *)a[2]->data);
    case F_C_GetSlotInfo:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_GetSlotInfo(*(CK_ULONG *)a[0]->data,
					(CK_SLOT_INFO *)a[1]->data);
    case F_C_GetTokenInfo:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_GetTokenInfo(*(CK_ULONG *)a[0]->data,
					(CK_TOKEN_INFO *)a[1]->data);
    case F_C_GetMechanismList:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	if (a[1]->data) {
	    a[1]->constType = ConstMechanism;
	}
	return func->C_GetMechanismList(*(CK_ULONG *)a[0]->data,
					(CK_MECHANISM_TYPE*)a[1]->data,
					(CK_ULONG *)a[2]->data);
    case F_C_GetMechanismInfo:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_GetMechanismInfo(*(CK_ULONG *)a[0]->data,
					*(CK_ULONG *)a[1]->data,
					(CK_MECHANISM_INFO *)a[2]->data);
    case F_C_InitToken:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_InitToken(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data);
    case F_C_InitPIN:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_InitPIN(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data);
    case F_C_SetPIN:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_SetPIN(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					*(CK_ULONG *)a[4]->data);
    case F_C_OpenSession:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_OpenSession(*(CK_ULONG *)a[0]->data,
					*(CK_ULONG *)a[1]->data,
					(void *)NULL,
					(CK_NOTIFY) NULL,
					(CK_ULONG *)a[2]->data);
    case F_C_CloseSession:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_CloseSession(*(CK_ULONG *)a[0]->data);
    case F_C_CloseAllSessions:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_CloseAllSessions(*(CK_ULONG *)a[0]->data);
    case F_C_GetSessionInfo:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_GetSessionInfo(*(CK_ULONG *)a[0]->data,
					(CK_SESSION_INFO *)a[1]->data);
    case F_C_GetOperationState:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_GetOperationState(*(CK_ULONG *)a[0]->data,
					(CK_BYTE *)a[1]->data,
					(CK_ULONG *)a[2]->data);
    case F_C_SetOperationState:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_SetOperationState(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					*(CK_ULONG *)a[3]->data,
					*(CK_ULONG *)a[4]->data);
    case F_C_Login:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_Login(*(CK_ULONG *)a[0]->data,
					*(CK_ULONG *)a[1]->data,
					(CK_CHAR *)a[2]->data,
					*(CK_ULONG *)a[3]->data);
    case F_C_Logout:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_Logout(*(CK_ULONG *)a[0]->data);
    case F_C_CreateObject:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_CreateObject(*(CK_ULONG *)a[0]->data,
					(CK_ATTRIBUTE *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_ULONG *)a[3]->data);
    case F_C_CopyObject:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_CopyObject(*(CK_ULONG *)a[0]->data,
					*(CK_ULONG *)a[0]->data,
					(CK_ATTRIBUTE *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_ULONG *)a[3]->data);
    case F_C_DestroyObject:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_DestroyObject(*(CK_ULONG *)a[0]->data,
					*(CK_ULONG *)a[1]->data);
    case F_C_GetObjectSize:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_GetObjectSize(*(CK_ULONG *)a[0]->data,
					*(CK_ULONG *)a[1]->data,
					(CK_ULONG *)a[2]->data);
    case F_C_GetAttributeValue:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_GetAttributeValue(*(CK_ULONG *)a[0]->data,
					*(CK_ULONG *)a[1]->data,
					(CK_ATTRIBUTE *)a[2]->data,
					*(CK_ULONG *)a[3]->data);
    case F_C_SetAttributeValue:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_SetAttributeValue(*(CK_ULONG *)a[0]->data,
					*(CK_ULONG *)a[1]->data,
					(CK_ATTRIBUTE *)a[2]->data,
					*(CK_ULONG *)a[3]->data);
    case F_C_FindObjectsInit:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_FindObjectsInit(*(CK_ULONG *)a[0]->data,
					(CK_ATTRIBUTE *)a[1]->data,
					*(CK_ULONG *)a[2]->data);
    case F_C_FindObjects:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_FindObjects(*(CK_ULONG *)a[0]->data,
					(CK_ULONG *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_ULONG *)a[3]->data);
    case F_C_FindObjectsFinal:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_FindObjectsFinal(*(CK_ULONG *)a[0]->data);
    case F_C_EncryptInit:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_EncryptInit(*(CK_ULONG *)a[0]->data,
					(CK_MECHANISM *)a[1]->data,
					*(CK_ULONG *)a[2]->data);
    case F_C_Encrypt:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_Encrypt(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					(CK_ULONG *)a[4]->data);
    case F_C_EncryptUpdate:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_EncryptUpdate(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					(CK_ULONG *)a[4]->data);
    case F_C_EncryptFinal:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_EncryptFinal(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					(CK_ULONG *)a[2]->data);
    case F_C_DecryptInit:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_DecryptInit(*(CK_ULONG *)a[0]->data,
					(CK_MECHANISM *)a[1]->data,
					*(CK_ULONG *)a[2]->data);
    case F_C_Decrypt:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_Decrypt(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					(CK_ULONG *)a[4]->data);
    case F_C_DecryptUpdate:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_DecryptUpdate(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					(CK_ULONG *)a[4]->data);
    case F_C_DecryptFinal:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_DecryptFinal(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					(CK_ULONG *)a[2]->data);
    case F_C_DigestInit:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_DigestInit(*(CK_ULONG *)a[0]->data,
					(CK_MECHANISM *)a[1]->data);
    case F_C_Digest:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_Digest(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					(CK_ULONG *)a[4]->data);
    case F_C_DigestUpdate:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_DigestUpdate(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data);
    case F_C_DigestKey:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_DigestKey(*(CK_ULONG *)a[0]->data,
					*(CK_ULONG *)a[1]->data);
    case F_C_DigestFinal:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_DigestFinal(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					(CK_ULONG *)a[2]->data);
    case F_C_SignInit:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_SignInit(*(CK_ULONG *)a[0]->data,
					(CK_MECHANISM *)a[1]->data,
					*(CK_ULONG *)a[2]->data);
    case F_C_Sign:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_Sign(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					(CK_ULONG *)a[4]->data);
    case F_C_SignUpdate:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_SignUpdate(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data);
    case F_C_SignFinal:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_SignFinal(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					(CK_ULONG *)a[2]->data);

    case F_C_SignRecoverInit:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_SignRecoverInit(*(CK_ULONG *)a[0]->data,
					(CK_MECHANISM *)a[1]->data,
					*(CK_ULONG *)a[2]->data);
    case F_C_SignRecover:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_SignRecover(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					(CK_ULONG *)a[4]->data);
    case F_C_VerifyInit:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_VerifyInit(*(CK_ULONG *)a[0]->data,
					(CK_MECHANISM *)a[1]->data,
					*(CK_ULONG *)a[2]->data);
    case F_C_Verify:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_Verify(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					*(CK_ULONG *)a[4]->data);
    case F_C_VerifyUpdate:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_VerifyUpdate(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data);
    case F_C_VerifyFinal:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_VerifyFinal(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data);

    case F_C_VerifyRecoverInit:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_VerifyRecoverInit(*(CK_ULONG *)a[0]->data,
					(CK_MECHANISM *)a[1]->data,
					*(CK_ULONG *)a[2]->data);
    case F_C_VerifyRecover:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_VerifyRecover(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					(CK_ULONG *)a[4]->data);
    case F_C_DigestEncryptUpdate:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_DigestEncryptUpdate(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					(CK_ULONG *)a[4]->data);
    case F_C_DecryptDigestUpdate:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_DecryptDigestUpdate(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					(CK_ULONG *)a[4]->data);
    case F_C_SignEncryptUpdate:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_SignEncryptUpdate(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					(CK_ULONG *)a[4]->data);
    case F_C_DecryptVerifyUpdate:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_DecryptVerifyUpdate(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					(CK_ULONG *)a[4]->data);
    case F_C_GenerateKey:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_GenerateKey(*(CK_ULONG *)a[0]->data,
					(CK_MECHANISM *)a[1]->data,
					(CK_ATTRIBUTE *)a[2]->data,
					*(CK_ULONG *)a[3]->data,
					(CK_ULONG *)a[4]->data);
    case F_C_GenerateKeyPair:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_GenerateKeyPair(*(CK_ULONG *)a[0]->data,
					(CK_MECHANISM *)a[1]->data,
					(CK_ATTRIBUTE *)a[2]->data,
					*(CK_ULONG *)a[3]->data,
					(CK_ATTRIBUTE *)a[4]->data,
					*(CK_ULONG *)a[5]->data,
					(CK_ULONG *)a[6]->data,
					(CK_ULONG *)a[7]->data);
    case F_C_WrapKey:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_WrapKey(*(CK_ULONG *)a[0]->data,
					(CK_MECHANISM *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					*(CK_ULONG *)a[3]->data,
					(CK_CHAR *)a[5]->data,
					(CK_ULONG *)a[6]->data);
    case F_C_UnwrapKey:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_UnwrapKey(*(CK_ULONG *)a[0]->data,
					(CK_MECHANISM *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_CHAR *)a[3]->data,
					*(CK_ULONG *)a[4]->data,
					(CK_ATTRIBUTE *)a[5]->data,
					*(CK_ULONG *)a[6]->data,
					(CK_ULONG *)a[7]->data);
    case F_C_DeriveKey:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_DeriveKey (*(CK_ULONG *)a[0]->data,
					(CK_MECHANISM *)a[1]->data,
					*(CK_ULONG *)a[2]->data,
					(CK_ATTRIBUTE *)a[3]->data,
					*(CK_ULONG *)a[4]->data,
					(CK_ULONG *)a[5]->data);
    case F_C_SeedRandom:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_SeedRandom(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data);
    case F_C_GenerateRandom:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_GenerateRandom(*(CK_ULONG *)a[0]->data,
					(CK_CHAR *)a[1]->data,
					*(CK_ULONG *)a[2]->data);
    case F_C_GetFunctionStatus:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_GetFunctionStatus(*(CK_ULONG *)a[0]->data);
    case F_C_CancelFunction:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_CancelFunction(*(CK_ULONG *)a[0]->data);
    case F_C_WaitForSlotEvent:
	if (!func) return CKR_CRYPTOKI_NOT_INITIALIZED;
	return func->C_WaitForSlotEvent(*(CK_ULONG *)a[0]->data,
					(CK_ULONG *)a[1]->data,
					(void *)a[2]->data);
    /* set a variable */
    case F_SetVar:	
    case F_SetStringVar:	
	(void) DeleteVariable(a[0]->data);
	(void) AddVariable(a[0]->data,&a[1]);
	return CKR_OK;
    /* print a value */
    case F_Print:
	return printArg(a[0],0);
    case F_SaveVar:
	return save(a[0]->data,a[1]);
    case F_RestoreVar:
	return restore(a[0]->data,a[1]);
    case F_Delete:
	return DeleteVariable(a[0]->data);
    case F_List:
	return list();
    case F_Run:
	return run(a[0]->data);
    case F_Load:
	return loadModule(&module,a[0]->data);
    case F_Unload:
	return unloadModule(&module);
    case F_NewArray:
	(void) DeleteVariable(a[0]->data);
	return ArrayVariable(a[0]->data,a[1]->data,*(CK_ULONG *)a[2]->data);
    case F_NewTemplate:
	(void) DeleteVariable(a[0]->data);
	return ArrayTemplate(a[0]->data,a[1]->data);
    case F_BuildTemplate:
	return BuildTemplate(a[0]);
    case F_SetTemplate:
	return SetTemplate(a[0],
		*(CK_ULONG *)a[1]->data,
		*(CK_ULONG *)a[2]->data);
    case F_NewMechanism:
	(void) DeleteVariable(a[0]->data);
	return NewMechanism(a[0]->data,*(CK_ULONG *)a[1]->data);
    case F_System:
        value = *(int *)a[0]->data;
	if (value & 0x80000000) {
	    systemFlags &= ~value;
	} else {
	    systemFlags |= value;
	}
	return CKR_OK;
    case F_Help:
	if (a[0]) {
	    helpIndex = lookup(a[0]->data);
	    if (helpIndex < 0) {
		return printTopicHelp(a[0]->data);
	    }
	    printHelp(helpIndex, 1);
	    return CKR_OK;
	}
	return printGeneralHelp();
    case F_Quit:
	return 0x80000000;
    default:
	fprintf(stderr,
		"Function %s not yet supported\n",commands[index].fname );
	return CKR_OK;
    }
    /* Not Reached */
    return CKR_OK;
}


CK_RV
process(FILE *inFile,int user)
{
    char buf[2048];
    Value **arglist;
    CK_RV error;
    CK_RV ckrv = CKR_OK;

    if (user) { printf("pkcs11> "); fflush(stdout); }

    while (fgets(buf,2048,inFile) != NULL) {
	int index;
	const char *bp;

	if (!user) printf("* %s",buf);
	bp = strip(buf);
	/* allow comments in scripts */
	if (*bp == '#') {
	    goto done;
	}


	index = lookup(bp);

	if (index < 0) {
	    goto done;
	}

	arglist = parseArgs(index,bp);
	if (arglist == NULL) {
	    goto done;
	}

	error = do_func(index,arglist);
	if (error == 0x80000000) {
	    parseFree(arglist);
	    break;
	}
	if (error) {
	    ckrv = error;
	    printf(">> Error : ");
	    printConst(error, ConstResult, 1);
	}

	putOutput(arglist);

	parseFree(arglist);
done:
	if (user) { 
	    printf("pkcs11> "); fflush(stdout); 
	}
    }
    return ckrv;
}

CK_RV  run(char *filename) 
{
    FILE *infile;
    CK_RV ckrv;

    infile = fopen(filename,"r");

    if (infile == NULL) {
	perror(filename);
	return CKR_FUNCTION_FAILED;
    }

    ckrv = process(infile, 0);

    fclose(infile);
    return ckrv;
}

int
main(int argc, char **argv)
{
    /* I suppose that some day we could parse some arguments */
    (void) process(stdin, 1);
    return 0;
}
