/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifndef INSTALL_DS_H
#define INSTALL_DS_H

#include <stdio.h>
#include <prio.h>
#include <prmem.h>

extern PRFileDesc *Pk11Install_FD;
extern int Pk11Install_yylex();
extern int Pk11Install_yylinenum;
extern char *Pk11Install_yyerrstr;

typedef enum { STRING_VALUE, PAIR_VALUE } ValueType;

typedef struct Pk11Install_Pair_str Pk11Install_Pair;
typedef union Pk11Install_Pointer_str Pk11Install_Pointer;
typedef struct Pk11Install_Value_str Pk11Install_Value;
typedef struct Pk11Install_ValueList_str Pk11Install_ValueList;
typedef struct Pk11Install_ListIter_str Pk11Install_ListIter;
typedef struct Pk11Install_File_str Pk11Install_File;
typedef struct Pk11Install_PlatformName_str Pk11Install_PlatformName;
typedef struct Pk11Install_Platform_str Pk11Install_Platform;
typedef struct Pk11Install_Info_str Pk11Install_Info;

extern Pk11Install_Pointer Pk11Install_yylval;
extern Pk11Install_ValueList* Pk11Install_valueList;

/*
//////////////////////////////////////////////////////////////////////////
// Pk11Install_Pair
//////////////////////////////////////////////////////////////////////////
*/

struct Pk11Install_Pair_str {
	char * key;
	Pk11Install_ValueList *list;

};

Pk11Install_Pair* 
Pk11Install_Pair_new_default();
Pk11Install_Pair* 
Pk11Install_Pair_new( char* _key, Pk11Install_ValueList* _list);
void 
Pk11Install_Pair_delete(Pk11Install_Pair* _this);
void
Pk11Install_Pair_Print(Pk11Install_Pair* _this, int pad);

/*
//////////////////////////////////////////////////////////////////////////
// Pk11Install_Pointer
//////////////////////////////////////////////////////////////////////////
*/
union Pk11Install_Pointer_str {
	Pk11Install_ValueList *list;
	Pk11Install_Value *value;
	Pk11Install_Pair *pair;
	char *string;
};

/*
//////////////////////////////////////////////////////////////////////////
// Pk11Install_Value
//////////////////////////////////////////////////////////////////////////
*/
struct Pk11Install_Value_str {

	ValueType type;
	char *string;	
	Pk11Install_Pair *pair;
	struct Pk11Install_Value_str *next;
};

Pk11Install_Value* 
Pk11Install_Value_new_default();
Pk11Install_Value*
Pk11Install_Value_new(ValueType _type, Pk11Install_Pointer ptr);
void
Pk11Install_Value_delete(Pk11Install_Value* _this);
void
Pk11Install_Value_Print(Pk11Install_Value* _this, int pad);

/*
//////////////////////////////////////////////////////////////////////////
// Pk11Install_ValueList
//////////////////////////////////////////////////////////////////////////
*/
struct Pk11Install_ValueList_str {
	int numItems;
	int numPairs;
	int numStrings;
	Pk11Install_Value *head;
};

Pk11Install_ValueList* 
Pk11Install_ValueList_new();
void
Pk11Install_ValueList_delete(Pk11Install_ValueList* _this);
void
Pk11Install_ValueList_AddItem(Pk11Install_ValueList* _this,
                              Pk11Install_Value* item);
void
Pk11Install_ValueList_Print(Pk11Install_ValueList* _this, int pad);


/*
//////////////////////////////////////////////////////////////////////////
// Pk11Install_ListIter
//////////////////////////////////////////////////////////////////////////
*/
struct Pk11Install_ListIter_str {
	const Pk11Install_ValueList *list;
	Pk11Install_Value *current;
};

Pk11Install_ListIter* 
Pk11Install_ListIter_new_default();
void
Pk11Install_ListIter_init(Pk11Install_ListIter* _this);
Pk11Install_ListIter*
Pk11Install_ListIter_new(const Pk11Install_ValueList* _list);
void
Pk11Install_ListIter_delete(Pk11Install_ListIter* _this);
void
Pk11Install_ListIter_reset(Pk11Install_ListIter* _this);
Pk11Install_Value*
Pk11Install_ListIter_nextItem(Pk11Install_ListIter* _this);

/************************************************************************
 *
 * Pk11Install_File
 */
struct Pk11Install_File_str {
	char *jarPath;
	char *relativePath;
	char *absolutePath;
	PRBool executable;
	int permissions;
};

Pk11Install_File*
Pk11Install_File_new();
void
Pk11Install_File_init(Pk11Install_File* _this);
void
Pk11Install_file_delete(Pk11Install_File* _this);
/*// Parses a syntax tree to obtain all attributes.
// Returns NULL for success, error message if parse error.*/
char*
Pk11Install_File_Generate(Pk11Install_File* _this, 
                          const Pk11Install_Pair* pair);
void
Pk11Install_File_Print(Pk11Install_File* _this, int pad);
void
Pk11Install_File_Cleanup(Pk11Install_File* _this);

/************************************************************************
 *
 * Pk11Install_PlatformName
 */
struct Pk11Install_PlatformName_str {
	char *OS;
	char **verString;
	int numDigits;
	char *arch;
};

Pk11Install_PlatformName*
Pk11Install_PlatformName_new();
void
Pk11Install_PlatformName_init(Pk11Install_PlatformName* _this);
void
Pk11Install_PlatformName_delete(Pk11Install_PlatformName* _this);
char*
Pk11Install_PlatformName_Generate(Pk11Install_PlatformName* _this,
                                  const char* str);
char*
Pk11Install_PlatformName_GetString(Pk11Install_PlatformName* _this);
char*
Pk11Install_PlatformName_GetVerString(Pk11Install_PlatformName* _this);
void
Pk11Install_PlatformName_Print(Pk11Install_PlatformName* _this, int pad);
void
Pk11Install_PlatformName_Cleanup(Pk11Install_PlatformName* _this);
PRBool
Pk11Install_PlatformName_equal(Pk11Install_PlatformName* _this,
                               Pk11Install_PlatformName* cmp);
PRBool
Pk11Install_PlatformName_lteq(Pk11Install_PlatformName* _this,
                              Pk11Install_PlatformName* cmp);
PRBool
Pk11Install_PlatformName_lt(Pk11Install_PlatformName* _this,
                            Pk11Install_PlatformName* cmp);

/************************************************************************
 *
 * Pk11Install_Platform
 */
struct Pk11Install_Platform_str {
	Pk11Install_PlatformName name;
	Pk11Install_PlatformName equivName;
	struct Pk11Install_Platform_str *equiv;
	PRBool usesEquiv;
	char *moduleFile;
	char *moduleName;
	int modFile;
	unsigned long mechFlags;
	unsigned long cipherFlags;
	Pk11Install_File *files;
	int numFiles;
};

Pk11Install_Platform*
Pk11Install_Platform_new();
void
Pk11Install_Platform_init(Pk11Install_Platform* _this);
void
Pk11Install_Platform_delete(Pk11Install_Platform* _this);
/*// Returns NULL for success, error message if parse error.*/
char* 
Pk11Install_Platform_Generate(Pk11Install_Platform* _this,
                              const Pk11Install_Pair *pair);
void 
Pk11Install_Platform_Print(Pk11Install_Platform* _this, int pad);
void 
Pk11Install_Platform_Cleanup(Pk11Install_Platform* _this);

/************************************************************************
 *
 * Pk11Install_Info
 */
struct Pk11Install_Info_str {
	Pk11Install_Platform *platforms;
	int numPlatforms;
	Pk11Install_PlatformName *forwardCompatible;
	int numForwardCompatible;
};

Pk11Install_Info*
Pk11Install_Info_new();
void
Pk11Install_Info_init();
void
Pk11Install_Info_delete(Pk11Install_Info* _this);
/*// Returns NULL for success, error message if parse error.*/
char* 
Pk11Install_Info_Generate(Pk11Install_Info* _this, 
                          const Pk11Install_ValueList *list);
	/*// Returns NULL if there is no matching platform*/
Pk11Install_Platform* 
Pk11Install_Info_GetBestPlatform(Pk11Install_Info* _this, char* myPlatform);
void 
Pk11Install_Info_Print(Pk11Install_Info* _this, int pad);
void 
Pk11Install_Info_Cleanup(Pk11Install_Info* _this);

#endif /* INSTALL_DS_H */
