/* 
 *           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
 *              NETSCAPE COMMUNICATIONS CORPORATION
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.  Use of this Source Code is subject to the terms of the
 * applicable license agreement from Netscape Communications Corporation.
 * The copyright notice(s) in this Source Code does not indicate actual or
 * intended publication of this Source Code.
 */

/* 
 *	resbdl.h		William Nolan (wnolan@netscape.com), 6/23/97
 */

#ifndef _RESBDL_H
#define _RESBDL_H

#include "nlsxp.h"
#include "locid.h"
#include "hashtab.h"
#include "zip.h"
#include "fmtable.h"
#include "choicfmt.h"
#include "msgfmt.h"

enum HashNodeType { HASHNODE_INDIRECT, HASHNODE_NONEXISTENT, 
					HASHNODE_HASHTABLE };

#define DEFAULT_ENCODING_PROPERTY "$TARGET_ENCODING$"

class pathInfo {
public:

	// CONSTRUCTOR
	pathInfo();

	// DESTRUCTOR 
	~pathInfo();

	char		*fName;			// name of the path or .jar file
	nlsBool		fIsAJar;			// 0 = not a jar, 1 = is a jar
	zip_t		*fJarInfo;		// pointer to zip/jar data (only relevant if this is a zip/jar)
	pathInfo	*fNext;		// pointer to next guy in the list
};

class NLSRESAPI_PUBLIC_CLASS HashNode {
public:
	// CONSTRUCTORS
	HashNode();
	HashNode(const char *name);

	// DESTRUCTOR
	~HashNode();

	// GETTERS
	NLS_ErrorCode				status() const;

protected:
	NLS_ErrorCode	fStatus;

public:
	Hashtable		*fData;
	uint32			fRefcount;
	char			*fName;
	HashNode		*fNext, *fPtr;
	HashNodeType	fType;
};

class NLSRESAPI_PUBLIC_CLASS HashTableRef {
  friend class PropertyResourceBundle;
public:

	// CONSTRUCTORS
	HashTableRef();
	HashTableRef(const char *packagename, const char *name);

	// DESTRUCTOR
	~HashTableRef();

	// GETTERS
	const UnicodeString*		getString(const char* key);
	NLS_ErrorCode				status() const;

	//
	// this function deletes any no-existent hash nodes
	//
	static void deleteNonExistentNodes(void);

	// TERMINATOR
        static void Terminate(void);

protected:
	NLS_ErrorCode	fStatus;
	static		HashNode *fgRoot;

private:

	HashNode	*fNode;
	nlsBool		fileExists(const char* filename);		// Check for a file existing
	HashNode	*searchForData(const char *packagename, const char *name);
	HashNode	*searchHashNodeList(const char *name); 
	void		resolveNames(int i, int numGuys, char *names[4], HashNode *ptrs[4]);
};



class NLSRESAPI_PUBLIC_CLASS PropertyResourceBundle
{
public:
	// CONSTRUCTORS
	// All of these constructors create a new PropertyResourceBundle based on
	// packageName and some variant of either a pointer to a Locale object
	// or an acceptLanguage string.
	PropertyResourceBundle();
	PropertyResourceBundle(const char* packageName);
	PropertyResourceBundle(const char* packageName, const Locale * locale);
	PropertyResourceBundle(const char* packageName, const char* acceptLanguage);

	PropertyResourceBundle(PropertyResourceBundle &copy);

	// DESTRUCTOR
	~PropertyResourceBundle();

	// GETTERS
	NLS_ErrorCode				status() const;		// Get error status

	// Looks up a value based on the key passed in.  Returns NULL and
	// sets status to NLS_RESOURCE_NOT_FOUND if not found.
	const UnicodeString*		getString(const char* key);
	// Returns a char* in specified encoding. If the encoding is null, 
	// the property $TARGET_ENCODING$ will be used to identify the encoding.
	// The char* is allocated with malloc, it should be release with free()
	char*						getString(const char* key, const char* to_charset);

	// Returns a pointer to the current data directory setting.
	static const char*			getDataDirectory();

	// These two functions test if a resource bundle, within the 
	// degree of specification provided, exists.  It doesn't count 
	// any fallback property files as valid "exists" files.
	static  nlsBool				exists(const char* packageName);
	static  nlsBool				exists(const char* packageName, const Locale * locale);

	static Hashtable*			searchPathForData(const char *name);

	// SETTERS

	// This sets the data directory for constructors to use when
	// searching for property files.
	static NLS_ErrorCode		setDataDirectory(const char* path);

	static void filenamePlatformConvert(char *filename);
	static void filenameJarConvert(char *filename);
	static void replaceChar(char* thestring, char from, char to);
	static nlsBool addPropertyFile(Hashtable *fData, const char *filename);
	static nlsBool addPropertyFileFromJar(Hashtable *fData, 
									      const char *jarFileName, 
									      const char *propertyFileName,
									      uint32 len);
	static nlsBool fileExists(const char* filename);		// Check for a file existing
	static NLS_ErrorCode addSearchPath(const char* path);

        // CLEANUP
        static void Terminate(void);

protected:
	PropertyResourceBundle(const char* packageName, nlsBool useFullFallback);
	static	char*				fgPath;
	static	char*				fgClassPath;
	static	nlsBool				fgCheckedPath;
	static  pathInfo*			fgPathJarList;

	NLS_ErrorCode				fStatus;
	HashTableRef				*fData;

private:
	void Initialize(const char* packageName, 
					const Locale* locale,
					nlsBool useFullFallbackPath);
	void initializeSearchPath(void);


	void filenamePlatformUnconvert(char *filename);
	nlsBool removeUnderscore(char* thestring);
	void buildPathJarList(char *bigpathstring);
	void searchPathForJars(char *path);
	void insertPathInfoAtEnd(pathInfo *myPI);
	void copyJarDirectory(pathInfo *tempPI, const char *sptr);
};

// This is the hashtable's deleting function.
void stringDeleter(Hashtable::Object valuePointer);  



#endif /* _RESBDL_H */
