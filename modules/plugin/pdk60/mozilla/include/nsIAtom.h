/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIAtom.idl
 */

#ifndef __gen_nsIAtom_h__
#define __gen_nsIAtom_h__

#include "nsISupports.h"

class nsISizeOfHandler; /* forward declaration */

class nsString;

/* starting interface:    nsIAtom */
#define NS_IATOM_IID_STR "3d1b15b0-93b4-11d1-895b-006008911b81"

#define NS_IATOM_IID \
  {0x3d1b15b0, 0x93b4, 0x11d1, \
    { 0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81 }}

class nsIAtom : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IATOM_IID)

  /**
   * Translate the unicode string into the stringbuf.
   */
  /* [noscript] void ToString (in nsStringRef aString); */
  NS_IMETHOD ToString(nsString & aString) = 0;

  /**
   * Return a pointer to a zero terminated unicode string.
   */
  /* void GetUnicode ([shared, retval] out wstring aResult); */
  NS_IMETHOD GetUnicode(const PRUnichar **aResult) = 0;

  /**
   * Get the size, in bytes, of the atom.
   */
  /* PRUint32 SizeOf (in nsISizeOfHandler aHandler); */
  NS_IMETHOD SizeOf(nsISizeOfHandler *aHandler, PRUint32 *_retval) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIATOM \
  NS_IMETHOD ToString(nsString & aString); \
  NS_IMETHOD GetUnicode(const PRUnichar **aResult); \
  NS_IMETHOD SizeOf(nsISizeOfHandler *aHandler, PRUint32 *_retval); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIATOM(_to) \
  NS_IMETHOD ToString(nsString & aString) { return _to ## ToString(aString); } \
  NS_IMETHOD GetUnicode(const PRUnichar **aResult) { return _to ## GetUnicode(aResult); } \
  NS_IMETHOD SizeOf(nsISizeOfHandler *aHandler, PRUint32 *_retval) { return _to ## SizeOf(aHandler, _retval); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsAtom : public nsIAtom
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIATOM

  nsAtom();
  virtual ~nsAtom();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsAtom, nsIAtom)

nsAtom::nsAtom()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsAtom::~nsAtom()
{
  /* destructor code */
}

/* [noscript] void ToString (in nsStringRef aString); */
NS_IMETHODIMP nsAtom::ToString(nsString & aString)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void GetUnicode ([shared, retval] out wstring aResult); */
NS_IMETHODIMP nsAtom::GetUnicode(const PRUnichar **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRUint32 SizeOf (in nsISizeOfHandler aHandler); */
NS_IMETHODIMP nsAtom::SizeOf(nsISizeOfHandler *aHandler, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif

/**
 * Find an atom that matches the given iso-latin1 C string. The
 * C string is translated into it's unicode equivalent.
 */
extern NS_COM nsIAtom* NS_NewAtom(const char* isolatin1);
/**
 * Find an atom that matches the given unicode string. The string is assumed
 * to be zero terminated.
 */
extern NS_COM nsIAtom* NS_NewAtom(const PRUnichar* unicode);
/**
 * Find an atom that matches the given string.
 */
extern NS_COM nsIAtom* NS_NewAtom(const nsString& aString);
/**
 * Return a count of the total number of atoms currently
 * alive in the system.
 */
extern NS_COM nsrefcnt NS_GetNumberOfAtoms(void);
extern NS_COM void NS_PurgeAtomTable(void);

#endif /* __gen_nsIAtom_h__ */
