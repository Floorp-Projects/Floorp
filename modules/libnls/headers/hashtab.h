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
* File hashtab.h
*
* Created by: Alan Liu
* Based on hashtab2.java by Mark Davis
*
* Modification History:
*
*	Date        Name        Description
*	04/29/97	aliu		Added key and value deletion protocol.  Add class and method
*							documentation paragraphs.  Made size() and isEmpty() inline.
*****************************************************************************************
*/

#ifndef _HASHTAB
#define _HASHTAB

#ifndef _PTYPES
#include "ptypes.h"
#endif

#ifndef _UNISTRING
#include "unistring.h"
#endif

//-------------------------------------------------------------------------------
// Hashkey is the abstract base class for keys in a Hashtable.

#ifdef NLS_MAC
#pragma export on
#endif

class T_UTILITY_API Hashkey
{
public:

	virtual			~Hashkey() {}
	virtual t_int32	hashCode() const = 0;
	virtual t_bool	operator==(const Hashkey&) const = 0;
	virtual ClassID	getDynamicClassID() const = 0;
};

//--------------------------------------------------------------------------------
// UnicodeStringKey interface
// This class is a key which is a UnicodeString.
//--------------------------------------------------------------------------------

// It would be nice to make this class multiply-inherit from Hashkey and
// UnicodeString, but that's going to create too many problems.

// Idea for future change:  For more flexibility, we could add a constructor
// that adopted a UnicodeString*, and then add a bool to indicate whether the
// string was owned or not.

class T_UTILITY_API UnicodeStringKey : public Hashkey
{
public:
	UnicodeStringKey() {}
	UnicodeStringKey(const UnicodeString&);
	virtual ~UnicodeStringKey();

	// Hashkey protocol
	virtual t_int32	hashCode() const;
	virtual t_bool	operator==(const Hashkey&) const;

	inline const UnicodeString* getString() const { return &fString; }
	inline void operator=(const UnicodeString& s) { fString=s; }

	virtual ClassID	getDynamicClassID() const { return (ClassID)&fgClassID; }
	static ClassID	getStaticClassID() { return (ClassID)&fgClassID; }

private:
	static char		fgClassID;
	UnicodeString	fString;
};

//--------------------------------------------------------------------------------
// LongKey interface
// This class is a key which is a long.
//--------------------------------------------------------------------------------

class T_UTILITY_API LongKey : public Hashkey
{
public:
	LongKey(t_int32 value);
	virtual ~LongKey();

	// Hashkey protocol
	virtual t_int32	hashCode() const;
	virtual t_bool	operator==(const Hashkey&) const;

	inline t_int32	getLong() const { return fLong; }

	virtual ClassID	getDynamicClassID() const { return (ClassID)&fgClassID; }
	static ClassID	getStaticClassID() { return (ClassID)&fgClassID; }

private:
	static char		fgClassID;
	t_int32			fLong;
};

//--------------------------------------------------------------------------------
// Hashtable
//--------------------------------------------------------------------------------

/**
 * Hashtable stores key-value pairs and does efficient lookup based on keys.
 * It also provides a protocol for enumerating through the key-value pairs
 * (although it does so in no particular order).  Hashtable objects may
 * optionally own the keys or the values it contains.  Keys must derive
 * from the abstract base class Hashkey.  Values are stored as void* pointers.
 */
class T_UTILITY_API Hashtable
{
public:
	typedef void* Object;
	typedef void (*ValueDeleter)(Object valuePointer);

public:
	/**
	 * Construct a new hashtable.  By default, a Hashtable does not own
	 * its keys or values.  If deleteKeys is true, then keys will be deleted.
	 * If deleteFunction is nonzero, then values will be deleted by calling
	 * deleteFunction(valuePointer).  Ownershipt of keys and values effects
	 * the behavior of the destructor and the put() and remove() methods.
	 */
				Hashtable();
				Hashtable(t_bool deleteKeys, ValueDeleter deleteFunction = 0);
				Hashtable(t_int32 initialSize);

	/**
	 * Destroy this object.  If values are to be deleted, delete all contained
	 * values.  If keys are to be deleted, delete all contained keys.
	 */
	virtual		~Hashtable();

	/**
	 * Return the number of key-value pairs in this hashtable.
	 */
	t_int32		size();

	/**
	 * Return true if there are no key-value pairs in this hashtable.
	 */
	t_bool		isEmpty();

	/**
	 * Add a key-value pair to this hashtable.  If a pair already exists with a
	 * key which is equal to the new key (as determined by operator==()), then
	 * the previous key-value pair are handled according to whether or not keys
	 * and values are owned by this object.  If keys are owned, then the
	 * previous key is deleted; otherwise, the previous key is considered the
	 * client's responsibility -- it is dropped on the floor.  The client must
	 * delete it using a previously saved pointer to it.  If values are owned,
	 * then the previous value is deleted and null is returned; otherwise, the
	 * previous value is returned.  If there is no pre-existing key which matches
	 * then null is always returned.
	 */
	Object		put(Hashkey* key, Object value);

	/**
	 * Return the value previously put() with a matching key.  A key matches
	 * if its operator==() method returns true for the specified key.  If there
	 * is no matching key, return null.
	 */
	Object		get(const Hashkey& key) const;

	/**
	 * Remove and return the value previously put() with a matching key.  A key
	 * matches if its operator==() method returns true for the specified key.
	 * If there is no matching key, return null.  The pre-existing key and value
	 * are handled according to whether or not keys and values are owned by this
	 * object.  See documentation for put() for details.
	 */
	Object		remove(const Hashkey& key);

	/**
	 * Specify whether keys are to be deleted or not.  This effects the behavior
	 * of the destructor and the methods put() and remove().
	 */
	void		setDeleteKeys(t_bool deleteKeys);

	/**
	 * Specify whether values are to be deleted or not.  This effects the behavior
	 * of the destructor and the methods put() and remove().  If deleteFunction
	 * is null, then values are not deleted.  Otherwise, deleteFunction(value) is
	 * called for each value to be deleted.
	 */
	void		setValueDeleter(ValueDeleter deleteFunction);

public:
	class T_UTILITY_API Enumeration
	{
	public:
					Enumeration(const Hashtable& h);
		t_bool		hasMoreElements();
		t_bool		nextElement(Hashkey*& fillInKey, Object& fillInValue);
		Object		removeElement(); // Removes the last element returned by nextElement()

	private:
		Enumeration(const Enumeration&); // Disallowed, not implemented
		Enumeration& operator=(const Enumeration&); // Disallowed, not implemented

		Hashtable&	fHashtable; // Don't make this const; AIX compiler chokes
		t_int32		fPosition;
		t_int32		fRemainder;
	};

	friend class Enumeration;

	Enumeration* createEnumeration() const;
		
private:
	void		initialize(t_int32 primeIndex);
	void		rehash();
	void		putInternal(Hashkey* key, t_int32 hash, Object value);
	t_int32		find(const Hashkey& key, t_int32 hash) const;
	static t_int32	leastGreaterPrimeIndex(t_int32 source);

private:
	t_int32		primeIndex;
	t_int32		highWaterMark;
	t_int32		lowWaterMark;
	float		highWaterFactor;
	float		lowWaterFactor;
	t_int32		count;

	// We use three arrays to minimize allocations
	t_int32*	hashes;
	Object*		values;
	Hashkey**	keyList;
	t_int32		length; // Length of hashes, values, keyList

	// Deletion protocol
	t_bool		fDeleteKeys;
	ValueDeleter fValueDeleter;
	
	static const t_int32 PRIMES[];
	static const t_int32 PRIMES_LENGTH;

	// Special hash values
	static const t_int32 DELETED;
	static const t_int32 EMPTY;
	static const t_int32 MAX_UNUSED;
};

#ifdef NLS_MAC
#pragma export off
#endif

//--------------------------------------------------------------------------------
// Inline Hashtable methods
//--------------------------------------------------------------------------------

inline t_int32 Hashtable::size()
{
	return count;
}

inline t_bool Hashtable::isEmpty()
{
	return count == 0;
}

#endif //_HASHTAB
//eof
