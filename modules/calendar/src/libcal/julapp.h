/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil -*- */
/* 
 * julapp.h
 * John Sun
 * 4/28/98 9:47:49 AM
 */
#ifndef __JULIANAPPLICATION_H_
#define __JULIANAPPLICATION_H_

/**
 *  This class is created to appialize the NLS_DATA_DIRECTORY correctly
 *  without setting the NLS_DATA_DIRECTORY environment variable.  
 *  To do this, the app function should locate somehow the location of
 *  the nls folder in the tree.  Currently, the location is in
 *  $(MOZ_SRC)/ns/dist/nls.  I don't know how to get the $(MOZ_SRC) though.
 */
class JulianApplication
{
private:
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
    static void init();
public:
    JulianApplication();
    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/
    /*----------------------------- 
    ** ACCESSORS (GET AND SET) 
    **---------------------------*/ 
    /*----------------------------- 
    ** UTILITIES 
    **---------------------------*/ 
    /*----------------------------- 
    ** STATIC METHODS 
    **---------------------------*/ 
    /*----------------------------- 
    ** OVERLOADED OPERATORS 
    **---------------------------*/ 
};

#endif /* __JULIANAPPLICATION_H_ */

