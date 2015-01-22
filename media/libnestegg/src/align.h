/*
 *	Copyright (c) 2004-2010 Alex Pankratov. All rights reserved.
 *
 *	Hierarchical memory allocator, 1.2.1
 *	http://swapped.cc/halloc
 */

/*
 *	The program is distributed under terms of BSD license. 
 *	You can obtain the copy of the license by visiting:
 *	
 *	http://www.opensource.org/licenses/bsd-license.php
 */

#ifndef _LIBP_ALIGN_H_
#define _LIBP_ALIGN_H_

#ifdef _MSC_VER

/*
 * MSVC defines max_align_t as a double.
 */
typedef double max_align_t;

#else

/*
 *	a type with the most strict alignment requirements
 */
union max_align
{
	char   c;
	short  s;
	long   l;
	int    i;
	float  f;
	double d;
	void * v;
	void (*q)(void);
};

typedef union max_align max_align_t;

#endif

#endif

