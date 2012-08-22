/*
 * Copyright (c) 2001,2002 Japan Network Information Center.
 * All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */


#include <stdlib.h>
#include <string.h>

#include "nsIDNKitInterface.h"

#define UCS_MAX		0x7fffffff
#define UNICODE_MAX	0x10ffff


/*
 * Load NAMEPREP compiled tables.
 */
#include "nameprepdata.c"

/*
 * Define mapping/checking functions for each version of the draft.
 */

#define VERSION id11
#include "nameprep_template.c"
#undef VERSION

typedef const char	*(*nameprep_mapproc)(uint32_t v);
typedef int		(*nameprep_checkproc)(uint32_t v);
typedef idn_biditype_t	(*nameprep_biditypeproc)(uint32_t v);

static struct idn_nameprep {
	char *version;
	nameprep_mapproc map_proc;
	nameprep_checkproc prohibited_proc;
	nameprep_checkproc unassigned_proc;
	nameprep_biditypeproc biditype_proc;
} nameprep_versions[] = {
#define MAKE_NAMEPREP_HANDLE(version, id) \
	{ version, \
	  compose_sym2(nameprep_map_, id), \
	  compose_sym2(nameprep_prohibited_, id), \
	  compose_sym2(nameprep_unassigned_, id), \
	  compose_sym2(nameprep_biditype_, id), }
	MAKE_NAMEPREP_HANDLE("nameprep-11", id11),
	{ NULL, NULL, NULL, NULL, NULL },
};

static idn_result_t	idn_nameprep_check(nameprep_checkproc proc,
					   const uint32_t *str,
					   const uint32_t **found);

idn_result_t
idn_nameprep_create(const char *version, idn_nameprep_t *handlep) {
	idn_nameprep_t handle;

	assert(handlep != NULL);

	TRACE(("idn_nameprep_create(version=%-.50s)\n",
	       version == NULL ? "<NULL>" : version));

	if (version == NULL)
		version = IDN_NAMEPREP_CURRENT;

	/*
	 * Lookup table for the specified version.  Since the number of
	 * versions won't be large (I don't want see draft-23 or such :-),
	 * simple linear search is OK.
	 */
	for (handle = nameprep_versions; handle->version != NULL; handle++) {
		if (strcmp(handle->version, version) == 0) {
			*handlep = handle;
			return (idn_success);
		}
	}
	return (idn_notfound);
}

void
idn_nameprep_destroy(idn_nameprep_t handle) {
	assert(handle != NULL);

	TRACE(("idn_nameprep_destroy()\n"));

	/* Nothing to do. */
}

idn_result_t
idn_nameprep_map(idn_nameprep_t handle, const uint32_t *from,
		 uint32_t *to, size_t tolen) {
	assert(handle != NULL && from != NULL && to != NULL);

	TRACE(("idn_nameprep_map(ctx=%s, from=\"%s\")\n",
	       handle->version, idn__debug_ucs4xstring(from, 50)));

	while (*from != '\0') {
		uint32_t v = *from;
		const char *mapped;

		if (v > UCS_MAX) {
			/* This cannot happen, but just in case.. */
			return (idn_invalid_codepoint);
		} else if (v > UNICODE_MAX) {
			/* No mapping is possible. */
			mapped = NULL;
		} else {
			/* Try mapping. */
			mapped = (*handle->map_proc)(v);
		}

		if (mapped == NULL) {
			/* No mapping. Just copy verbatim. */
			if (tolen < 1)
				return (idn_buffer_overflow);
			*to++ = v;
			tolen--;
		} else {
			const unsigned char *mappeddata;
			size_t mappedlen;

			mappeddata = (const unsigned char *)mapped + 1;
			mappedlen = *mapped;

			if (tolen < (mappedlen + 3) / 4)
				return (idn_buffer_overflow);
			tolen -= (mappedlen + 3) / 4;
			while (mappedlen >= 4) {
				*to  = *mappeddata++;
				*to |= *mappeddata++ <<  8;
				*to |= *mappeddata++ << 16;
				*to |= *mappeddata++ << 24;
				mappedlen -= 4;
				to++;
			}
			if (mappedlen > 0) {
				*to  = *mappeddata++;
				*to |= (mappedlen >= 2) ?
				       *mappeddata++ <<  8: 0;
				*to |= (mappedlen >= 3) ?
				       *mappeddata++ << 16: 0;
				to++;
			}
		}
		from++;
	}
	if (tolen == 0)
		return (idn_buffer_overflow);
	*to = '\0';
	return (idn_success);
}

idn_result_t
idn_nameprep_isprohibited(idn_nameprep_t handle, const uint32_t *str,
			  const uint32_t **found) {
	assert(handle != NULL && str != NULL && found != NULL);

	TRACE(("idn_nameprep_isprohibited(ctx=%s, str=\"%s\")\n",
	       handle->version, idn__debug_ucs4xstring(str, 50)));

	return (idn_nameprep_check(handle->prohibited_proc, str, found));
}
		
idn_result_t
idn_nameprep_isunassigned(idn_nameprep_t handle, const uint32_t *str,
			  const uint32_t **found) {
	assert(handle != NULL && str != NULL && found != NULL);

	TRACE(("idn_nameprep_isunassigned(handle->version, str=\"%s\")\n",
	       handle->version, idn__debug_ucs4xstring(str, 50)));

	return (idn_nameprep_check(handle->unassigned_proc, str, found));
}
		
static idn_result_t
idn_nameprep_check(nameprep_checkproc proc, const uint32_t *str,
		   const uint32_t **found) {
	uint32_t v;

	while (*str != '\0') {
		v = *str;

		if (v > UCS_MAX) {
			/* This cannot happen, but just in case.. */
			return (idn_invalid_codepoint);
		} else if (v > UNICODE_MAX) {
			/* It is invalid.. */
			*found = str;
			return (idn_success);
		} else if ((*proc)(v)) {
			*found = str;
			return (idn_success);
		}
		str++;
	}
	*found = NULL;
	return (idn_success);
}

idn_result_t
idn_nameprep_isvalidbidi(idn_nameprep_t handle, const uint32_t *str,
			 const uint32_t **found) {
	uint32_t v;
	idn_biditype_t first_char;
	idn_biditype_t last_char;
	int found_r_al;

	assert(handle != NULL && str != NULL && found != NULL);

	TRACE(("idn_nameprep_isvalidbidi(ctx=%s, str=\"%s\")\n",
	       handle->version, idn__debug_ucs4xstring(str, 50)));

	if (*str == '\0') {
		*found = NULL;
		return (idn_success);
	}

	/*
	 * check first character's type and initialize variables.
	 */
	found_r_al = 0;
	if (*str > UCS_MAX) {
		/* This cannot happen, but just in case.. */
		return (idn_invalid_codepoint);
	} else if (*str > UNICODE_MAX) {
		/* It is invalid.. */
		*found = str;
		return (idn_success);
	}
	first_char = last_char = (*(handle->biditype_proc))(*str);
	if (first_char == idn_biditype_r_al) {
		found_r_al = 1;
	}
	str++;

	/*
	 * see whether string is valid or not.
	 */
	while (*str != '\0') {
		v = *str;

		if (v > UCS_MAX) {
			/* This cannot happen, but just in case.. */
			return (idn_invalid_codepoint);
		} else if (v > UNICODE_MAX) {
			/* It is invalid.. */
			*found = str;
			return (idn_success);
		} else { 
			last_char = (*(handle->biditype_proc))(v);
			if (found_r_al && last_char == idn_biditype_l) {
				*found = str;
				return (idn_success);
			}
			if (first_char != idn_biditype_r_al && last_char == idn_biditype_r_al) {
				*found = str;
				return (idn_success);
			}
			if (last_char == idn_biditype_r_al) {
				found_r_al = 1;
			}
		}
		str++;
	}

	if (found_r_al) {
		if (last_char != idn_biditype_r_al) {
			*found = str - 1;
			return (idn_success);
		}
	}

	*found = NULL;
	return (idn_success);
}

idn_result_t
idn_nameprep_createproc(const char *parameter, void **handlep) {
	return idn_nameprep_create(parameter, (idn_nameprep_t *)handlep);
}

void
idn_nameprep_destroyproc(void *handle) {
	idn_nameprep_destroy((idn_nameprep_t)handle);
}

idn_result_t
idn_nameprep_mapproc(void *handle, const uint32_t *from,
		      uint32_t *to, size_t tolen) {
	return idn_nameprep_map((idn_nameprep_t)handle, from, to, tolen);
}

idn_result_t
idn_nameprep_prohibitproc(void *handle, const uint32_t *str,
			   const uint32_t **found) {
	return idn_nameprep_isprohibited((idn_nameprep_t)handle, str, found);
}

idn_result_t
idn_nameprep_unassignedproc(void *handle, const uint32_t *str,
			     const uint32_t **found) {
	return idn_nameprep_isunassigned((idn_nameprep_t)handle, str, found);
}

idn_result_t
idn_nameprep_bidiproc(void *handle, const uint32_t *str,
		      const uint32_t **found) {
	return idn_nameprep_isvalidbidi((idn_nameprep_t)handle, str, found);
}
