/*======================================================================
 FILE: pvl.h
 CREATOR: eric November, 1995


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
======================================================================*/


#ifndef __PVL_H__
#define __PVL_H__

typedef void* pvl_list;
typedef void* pvl_elem;

/*
  struct pvl_elem_t

  This type is private. Always use pvl_elem instead. The struct would
  not even appear in this header except to make code in the USE_MACROS
  blocks work

  */
typedef struct pvl_elem_t
{
	int MAGIC;			/* Magic Identifier */
	void *d;			/* Pointer to data user is storing */
	struct pvl_elem_t *next;	/* Next element */
	struct pvl_elem_t *prior;	/* prior element */
} pvl_elem_t;



/* This global is incremented for each call to pvl_new_element(); it gives each
 * list a unique identifer */

extern int  pvl_elem_count;
extern int  pvl_list_count;

/* Create new lists or elements */
pvl_elem pvl_new_element(void* d, pvl_elem next,pvl_elem prior);
pvl_list pvl_newlist(void);
void pvl_free(pvl_list);

/* Add, remove, or get the head of the list */
void pvl_unshift(pvl_list l,void *d);
void* pvl_shift(pvl_list l);
pvl_elem pvl_head(pvl_list);

/* Add, remove or get the tail of the list */
void pvl_push(pvl_list l,void *d);
void* pvl_pop(pvl_list l);
pvl_elem pvl_tail(pvl_list);

/* Insert elements in random places */
typedef int (*pvl_comparef)(void* a, void* b); /* a, b are of the data type*/
void pvl_insert_ordered(pvl_list l,pvl_comparef f,void *d);
void pvl_insert_after(pvl_list l,pvl_elem e,void *d);
void pvl_insert_before(pvl_list l,pvl_elem e,void *d);

/* Remove an element, or clear the entire list */
void* pvl_remove(pvl_list,pvl_elem); /* Remove element, return data */
void pvl_clear(pvl_list); /* Remove all elements, de-allocate all data */

int pvl_count(pvl_list);

/* Navagate the list */
pvl_elem pvl_next(pvl_elem e);
pvl_elem pvl_prior(pvl_elem e);

/* get the data in the list */
#ifndef PVL_USE_MACROS
void* pvl_data(pvl_elem);
#else
#define pvl_data(x) x==0 ? 0 : ((struct pvl_elem_t *)x)->d;
#endif


/* Find an element for which a function returns true */
typedef int (*pvl_findf)(void* a, void* b); /*a is list elem, b is other data*/
pvl_elem pvl_find(pvl_list l,pvl_findf f,void* v);
pvl_elem pvl_find_next(pvl_list l,pvl_findf f,void* v);

/* Pass each element in the list to a function */
typedef void (*pvl_applyf)(void* a, void* b); /*a is list elem, b is other data*/
void pvl_apply(pvl_list l,pvl_applyf f, void *v);


#endif /* __PVL_H__ */





