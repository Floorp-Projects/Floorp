/*
 * Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved.
 * 
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.

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

#ifndef nsIDNKitWrapper_h__
#define nsIDNKitWrapper_h__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * libidnkit result code.
 */
typedef enum {
	idn_success,
	idn_notfound,
	idn_invalid_encoding,
	idn_invalid_syntax,
	idn_invalid_name,
	idn_invalid_message,
	idn_invalid_action,
	idn_invalid_codepoint,
	idn_invalid_length,
	idn_buffer_overflow,
	idn_noentry,
	idn_nomemory,
	idn_nofile,
	idn_nomapping,
	idn_context_required,
	idn_prohibited,
	idn_failure	/* !!This must be the last one!! */
} idn_result_t;

/*
 * BIDI type codes.
 */      
typedef enum {
	idn_biditype_r_al,
	idn_biditype_l,
	idn_biditype_others
} idn_biditype_t;

/*
 * A Handle for nameprep operations.
 */
typedef struct idn_nameprep *idn_nameprep_t;


/*
 * The latest version of nameprep.
 */
#define IDN_NAMEPREP_CURRENT	"nameprep-11"

#undef assert
#define assert(a)
#define TRACE(a)


/* race.c */
idn_result_t	race_decode_decompress(const char *from,
					       uint16_t *buf,
					       size_t buflen);
idn_result_t	race_compress_encode(const uint16_t *p,
					     int compress_mode,
					     char *to, size_t tolen);
int		get_compress_mode(uint16_t *p);


/* nameprep.c */

/*
 * Create a handle for nameprep operations.
 * The handle is stored in '*handlep', which is used other functions
 * in this module.
 * The version of the NAMEPREP specification can be specified with
 * 'version' parameter.  If 'version' is nullptr, the latest version
 * is used.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_notfound		-- specified version not found.
 */
idn_result_t
idn_nameprep_create(const char *version, idn_nameprep_t *handlep);

/*
 * Close a handle, which was created by 'idn_nameprep_create'.
 */
void
idn_nameprep_destroy(idn_nameprep_t handle);

/*
 * Perform character mapping on an UCS4 string specified by 'from', and
 * store the result into 'to', whose length is specified by 'tolen'.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_buffer_overflow	-- result buffer is too small.
 */
idn_result_t
idn_nameprep_map(idn_nameprep_t handle, const uint32_t *from,
		 uint32_t *to, size_t tolen);

/*
 * Check if an UCS4 string 'str' contains any prohibited characters specified
 * by the draft.  If found, the pointer to the first such character is stored
 * into '*found'.  Otherwise '*found' will be nullptr.
 *
 * Returns:
 *	idn_success		-- check has been done properly. (But this
 *				   does not mean that no prohibited character
 *				   was found.  Check '*found' to see the
 *				   result.)
 */
idn_result_t
idn_nameprep_isprohibited(idn_nameprep_t handle, const uint32_t *str,
			  const uint32_t **found);

/*
 * Check if an UCS4 string 'str' contains any unassigned characters specified
 * by the draft.  If found, the pointer to the first such character is stored
 * into '*found'.  Otherwise '*found' will be nullptr.
 *
 * Returns:
 *	idn_success		-- check has been done properly. (But this
 *				   does not mean that no unassinged character
 *				   was found.  Check '*found' to see the
 *				   result.)
 */
idn_result_t
idn_nameprep_isunassigned(idn_nameprep_t handle, const uint32_t *str,
			  const uint32_t **found);

/*
 * Check if an UCS4 string 'str' is valid string specified by ``bidi check''
 * of the draft.  If it is not valid, the pointer to the first invalid
 * character is stored into '*found'.  Otherwise '*found' will be nullptr.
 *
 * Returns:
 *	idn_success		-- check has been done properly. (But this
 *				   does not mean that the string was valid.
 *				   Check '*found' to see the result.)
 */
idn_result_t
idn_nameprep_isvalidbidi(idn_nameprep_t handle, const uint32_t *str,
			 const uint32_t **found);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* nsIDNKitWrapper_h__ */
