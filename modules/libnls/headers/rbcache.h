/*
*****************************************************************************************
*                                                                                       *
* COPYRIGHT:                                                                            *
*   (C) Copyright Taligent, Inc.,  1997                                                 *
*   (C) Copyright International Business Machines Corporation,  1997                    *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.                  *
*   US Government Users Restricted Rights - Use, duplication, or disclosure             *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                              *
*                                                                                       *
*****************************************************************************************
*
* File rbcache.h
*
* Modification History:
*
*	Date		Name		Description
*	03/20/97	aliu		Creation.
*	04/29/97	aliu		Convert to use new Hashtable protocol.
*****************************************************************************************
*/

#include "hashtab.h"

/**
 * A class which represents an ordinary Hashtable which deletes its contents when it
 * is destroyed.  This class stores UnicodeStringKeys as its keys, and
 * ResourceBundleData objects as its values.
 */
 
#ifdef NLS_MAC
#pragma export on
#endif
 
class T_UTILITY_API ResourceBundleCache : public Hashtable // Not really external; just making the compiler happy
{
public:
    ResourceBundleCache();
private:
	static void deleteValue(Hashtable::Object value);
};

/**
 * A hashtable which owns its keys and values and deletes them when it is destroyed.
 * This class stored UnicodeStringKeys as its keys, and the value 1 as its objects.
 * in other words, the objects are just (void*)1.  The only real information is
 * whether or not a key is present.  Semantically, if a key is present, it means
 * that the corresponding filename has been visited already.
 */
class T_UTILITY_API VisitedFileCache : public Hashtable // Not really external; just making the compiler happy
{
public:
    VisitedFileCache();
	t_bool wasVisited(const UnicodeString& filename) const;
	void markAsVisited(const UnicodeString& filename);
};

#ifdef NLS_MAC
#pragma export off
#endif


inline t_bool VisitedFileCache::wasVisited(const UnicodeString& filename) const
{
	UnicodeStringKey key(filename);
	return get(key) != 0;
}

inline void VisitedFileCache::markAsVisited(const UnicodeString& filename)
{
	put(new UnicodeStringKey(filename), (Hashtable::Object)1);
}

//eof
