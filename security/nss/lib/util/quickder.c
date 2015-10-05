/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
    Optimized ASN.1 DER decoder
    
*/

#include "secerr.h"
#include "secasn1.h" /* for SEC_ASN1GetSubtemplate */
#include "secitem.h"

/*
 * simple definite-length ASN.1 decoder
 */

static unsigned char* definite_length_decoder(const unsigned char *buf,
                                              const unsigned int buf_length,
                                              unsigned int *out_data_length,
                                              PRBool includeTag)
{
    unsigned char tag;
    unsigned int used_length = 0;
    unsigned int data_length = 0;
    unsigned char length_field_len = 0;
    unsigned char byte;
    unsigned int i;

    if (used_length >= buf_length)
    {
        /* Tag field was not found! */
        return NULL;
    }
    tag = buf[used_length++];

    if (tag == 0)
    {
        /* End-of-contents octects should not be present in DER because
           DER doesn't use the indefinite length form. */
        return NULL;
    }

    if ((tag & 0x1F) == 0x1F)
    {
        /* High tag number (a tag number > 30) is not supported */
        return NULL;
    }

    if (used_length >= buf_length)
    {
        /* Length field was not found! */
        return NULL;
    }
    byte = buf[used_length++];

    if (!(byte & 0x80))
    {
        /* Short form: The high bit is not set. */
        data_length = byte; /* clarity; we're returning a 32-bit int. */
    }
    else
    {
        /* Long form. Extract the field length */
        length_field_len = byte & 0x7F;
        if (length_field_len == 0)
        {
            /* DER doesn't use the indefinite length form. */
            return NULL;
        }

        if (length_field_len > sizeof(data_length))
        {
            /* We don't support an extended length field  longer than
               4 bytes (2^32) */
            return NULL;
        }

        if (length_field_len > (buf_length - used_length))
        {
            /* Extended length field was not found */
            return NULL;
        }

        /* Iterate across the extended length field */
        for (i = 0; i < length_field_len; i++)
        {
            byte = buf[used_length++];
            data_length = (data_length << 8) | byte;

            if (i == 0)
            {
                PRBool too_long = PR_FALSE;
                if (length_field_len == 1)
                {
                    too_long = ((byte & 0x80) == 0); /* Short form suffices */
                }
                else
                {
                    too_long = (byte == 0); /* This zero byte can be omitted */
                }
                if (too_long)
                {
                    /* The length is longer than needed. */
                    return NULL;
                }
            }
        }
    }

    if (data_length > (buf_length - used_length))
    {
        /* The decoded length exceeds the available buffer */
        return NULL;
    }

    if (includeTag)
    {
        data_length += used_length;
    }

    *out_data_length = data_length;
    return ((unsigned char*)buf + (includeTag ? 0 : used_length));
}

static SECStatus GetItem(SECItem* src, SECItem* dest, PRBool includeTag)
{
    if ( (!src) || (!dest) || (!src->data && src->len) )
    {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (!src->len)
    {
        /* reaching the end of the buffer is not an error */
        dest->data = NULL;
        dest->len = 0;
        return SECSuccess;
    }

    dest->data = definite_length_decoder(src->data,  src->len, &dest->len,
        includeTag);
    if (dest->data == NULL)
    {
        PORT_SetError(SEC_ERROR_BAD_DER);
        return SECFailure;
    }
    src->len -= (dest->data - src->data) + dest->len;
    src->data = dest->data + dest->len;
    return SECSuccess;
}

/* check if the actual component's type matches the type in the template */

static SECStatus MatchComponentType(const SEC_ASN1Template* templateEntry,
                                    SECItem* item, PRBool* match, void* dest)
{
    unsigned long kind = 0;
    unsigned char tag = 0;

    if ( (!item) || (!item->data && item->len) || (!templateEntry) || (!match) )
    {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (!item->len)
    {
        *match = PR_FALSE;
        return SECSuccess;
    }

    kind = templateEntry->kind;
    tag = *(unsigned char*) item->data;

    if ( ( (kind & SEC_ASN1_INLINE) ||
           (kind & SEC_ASN1_POINTER) ) &&
           (0 == (kind & SEC_ASN1_TAG_MASK) ) )
    {
        /* These cases are special because the template's "kind" does not
           give us the information for the ASN.1 tag of the next item. It can
           only be figured out from the subtemplate. */
        if (!(kind & SEC_ASN1_OPTIONAL))
        {
            /* This is a required component. If there is a type mismatch,
               the decoding of the subtemplate will fail, so assume this
               is a match at the parent level and let it fail later. This
               avoids a redundant check in matching cases */
            *match = PR_TRUE;
            return SECSuccess;
        }
        else
        {
            /* optional component. This is the hard case. Now we need to
               look at the subtemplate to get the expected kind */
            const SEC_ASN1Template* subTemplate = 
                SEC_ASN1GetSubtemplate (templateEntry, dest, PR_FALSE);
            if (!subTemplate)
            {
                PORT_SetError(SEC_ERROR_BAD_TEMPLATE);
                return SECFailure;
            }
            if ( (subTemplate->kind & SEC_ASN1_INLINE) ||
                 (subTemplate->kind & SEC_ASN1_POINTER) )
            {
                /* disallow nesting SEC_ASN1_POINTER and SEC_ASN1_INLINE,
                   otherwise you may get a false positive due to the recursion
                   optimization above that always matches the type if the
                   component is required . Nesting these should never be
                   required, so that no one should miss this ability */
                PORT_SetError(SEC_ERROR_BAD_TEMPLATE);
                return SECFailure;
            }
            return MatchComponentType(subTemplate, item, match,
                                      (void*)((char*)dest + templateEntry->offset));
        }
    }

    if (kind & SEC_ASN1_CHOICE)
    {
        /* we need to check the component's tag against each choice's tag */
        /* XXX it would be nice to save the index of the choice here so that
           DecodeChoice wouldn't have to do this again. However, due to the
           recursivity of MatchComponentType, we don't know if we are in a
           required or optional component, so we can't write anywhere in
           the destination within this function */
        unsigned choiceIndex = 1;
        const SEC_ASN1Template* choiceEntry;
        while ( (choiceEntry = &templateEntry[choiceIndex++]) && (choiceEntry->kind))
        {
            if ( (SECSuccess == MatchComponentType(choiceEntry, item, match,
                                (void*)((char*)dest + choiceEntry->offset))) &&
                 (PR_TRUE == *match) )
            {
                return SECSuccess;
            }
        }
	/* no match, caller must decide if this is BAD DER, or not. */
        *match = PR_FALSE;
        return SECSuccess;
    }

    if (kind & SEC_ASN1_ANY)
    {
        /* SEC_ASN1_ANY always matches */
        *match = PR_TRUE;
        return SECSuccess;
    }

    if ( (0 == ((unsigned char)kind & SEC_ASN1_TAGNUM_MASK)) &&
         (!(kind & SEC_ASN1_EXPLICIT)) &&
         ( ( (kind & SEC_ASN1_SAVE) ||
             (kind & SEC_ASN1_SKIP) ) &&
           (!(kind & SEC_ASN1_OPTIONAL)) 
         )
       )
    {
        /* when saving or skipping a required component,  a type is not
           required in the template. This is for legacy support of
           SEC_ASN1_SAVE and SEC_ASN1_SKIP only. XXX I would like to
           deprecate these usages and always require a type, as this
           disables type checking, and effectively forbids us from
           transparently ignoring optional components we aren't aware of */
        *match = PR_TRUE;
        return SECSuccess;
    }

    /* first, do a class check */
    if ( (tag & SEC_ASN1_CLASS_MASK) !=
         (((unsigned char)kind) & SEC_ASN1_CLASS_MASK) )
    {
#ifdef DEBUG
        /* this is only to help debugging of the decoder in case of problems */
        unsigned char tagclass = tag & SEC_ASN1_CLASS_MASK;
        unsigned char expectedclass = (unsigned char)kind & SEC_ASN1_CLASS_MASK;
        tagclass = tagclass;
        expectedclass = expectedclass;
#endif
        *match = PR_FALSE;
        return SECSuccess;
    }

    /* now do a tag check */
    if ( ((unsigned char)kind & SEC_ASN1_TAGNUM_MASK) !=
         (tag & SEC_ASN1_TAGNUM_MASK))
    {
        *match = PR_FALSE;
        return SECSuccess;
    }

    /* now, do a method check. This depends on the class */
    switch (tag & SEC_ASN1_CLASS_MASK)
    {
    case SEC_ASN1_UNIVERSAL:
        /* For types of the SEC_ASN1_UNIVERSAL class, we know which must be
           primitive or constructed based on the tag */
        switch (tag & SEC_ASN1_TAGNUM_MASK)
        {
        case SEC_ASN1_SEQUENCE:
        case SEC_ASN1_SET:
        case SEC_ASN1_EMBEDDED_PDV:
            /* this component must be a constructed type */
            /* XXX add any new universal constructed type here */
            if (tag & SEC_ASN1_CONSTRUCTED)
            {
                *match = PR_TRUE;
                return SECSuccess;
            }
            break;

        default:
            /* this component must be a primitive type */
            if (! (tag & SEC_ASN1_CONSTRUCTED))
            {
                *match = PR_TRUE;
                return SECSuccess;
            }
            break;
        }
        break;

    default:
        /* for all other classes, we check the method based on the template */
        if ( (unsigned char)(kind & SEC_ASN1_METHOD_MASK) ==
             (tag & SEC_ASN1_METHOD_MASK) )
        {
            *match = PR_TRUE;
            return SECSuccess;
        }
        /* method does not match between template and component */
        break;
    }

    *match = PR_FALSE;
    return SECSuccess;
}

#ifdef DEBUG

static SECStatus CheckSequenceTemplate(const SEC_ASN1Template* sequenceTemplate)
{
    SECStatus rv = SECSuccess;
    const SEC_ASN1Template* sequenceEntry = NULL;
    unsigned long seqIndex = 0;
    unsigned long lastEntryIndex = 0;
    unsigned long ambiguityIndex = 0;
    PRBool foundAmbiguity = PR_FALSE;

    do
    {
        sequenceEntry = &sequenceTemplate[seqIndex++];
        if (sequenceEntry->kind)
        {
            /* ensure that we don't have an optional component of SEC_ASN1_ANY
               in the middle of the sequence, since we could not handle it */
            /* XXX this function needs to dig into the subtemplates to find
               the next tag */
            if ( (PR_FALSE == foundAmbiguity) &&
                 (sequenceEntry->kind & SEC_ASN1_OPTIONAL) &&
                 (sequenceEntry->kind & SEC_ASN1_ANY) )
            {
                foundAmbiguity = PR_TRUE;
                ambiguityIndex = seqIndex - 1;
            }
        }
    } while (sequenceEntry->kind);

    lastEntryIndex = seqIndex - 2;

    if (PR_FALSE != foundAmbiguity)
    {
        if (ambiguityIndex < lastEntryIndex)
        {
            /* ambiguity can only be tolerated on the last entry */
            PORT_SetError(SEC_ERROR_BAD_TEMPLATE);
            rv = SECFailure;
        }
    }

    /* XXX also enforce ASN.1 requirement that tags be
       distinct for consecutive optional components */

    return rv;
}

#endif

static SECStatus DecodeItem(void* dest,
                     const SEC_ASN1Template* templateEntry,
                     SECItem* src, PLArenaPool* arena, PRBool checkTag);

static SECStatus DecodeSequence(void* dest,
                     const SEC_ASN1Template* templateEntry,
                     SECItem* src, PLArenaPool* arena)
{
    SECStatus rv = SECSuccess;
    SECItem source;
    SECItem sequence;
    const SEC_ASN1Template* sequenceTemplate = &(templateEntry[1]);
    const SEC_ASN1Template* sequenceEntry = NULL;
    unsigned long seqindex = 0;

#ifdef DEBUG
    /* for a sequence, we need to validate the template. */
    rv = CheckSequenceTemplate(sequenceTemplate);
#endif

    source = *src;

    /* get the sequence */
    if (SECSuccess == rv)
    {
        rv = GetItem(&source, &sequence, PR_FALSE);
    }

    /* process it */
    if (SECSuccess == rv)
    do
    {
        sequenceEntry = &sequenceTemplate[seqindex++];
        if ( (sequenceEntry && sequenceEntry->kind) &&
             (sequenceEntry->kind != SEC_ASN1_SKIP_REST) )
        {
            rv = DecodeItem(dest, sequenceEntry, &sequence, arena, PR_TRUE);
        }
    } while ( (SECSuccess == rv) &&
              (sequenceEntry->kind &&
               sequenceEntry->kind != SEC_ASN1_SKIP_REST) );
    /* we should have consumed all the bytes in the sequence by now
       unless the caller doesn't care about the rest of the sequence */
    if (SECSuccess == rv && sequence.len &&
        sequenceEntry && sequenceEntry->kind != SEC_ASN1_SKIP_REST)
    {
        /* it isn't 100% clear whether this is a bad DER or a bad template.
           The problem is that logically, they don't match - there is extra
           data in the DER that the template doesn't know about */
        PORT_SetError(SEC_ERROR_BAD_DER);
        rv = SECFailure;
    }

    return rv;
}

static SECStatus DecodeInline(void* dest,
                     const SEC_ASN1Template* templateEntry,
                     SECItem* src, PLArenaPool* arena, PRBool checkTag)
{
    const SEC_ASN1Template* inlineTemplate = 
        SEC_ASN1GetSubtemplate (templateEntry, dest, PR_FALSE);
    return DecodeItem((void*)((char*)dest + templateEntry->offset),
                            inlineTemplate, src, arena, checkTag);
}

static SECStatus DecodePointer(void* dest,
                     const SEC_ASN1Template* templateEntry,
                     SECItem* src, PLArenaPool* arena, PRBool checkTag)
{
    const SEC_ASN1Template* ptrTemplate = 
        SEC_ASN1GetSubtemplate (templateEntry, dest, PR_FALSE);
    void* subdata = PORT_ArenaZAlloc(arena, ptrTemplate->size);
    *(void**)((char*)dest + templateEntry->offset) = subdata;
    if (subdata)
    {
        return DecodeItem(subdata, ptrTemplate, src, arena, checkTag);
    }
    else
    {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
}

static SECStatus DecodeImplicit(void* dest,
                     const SEC_ASN1Template* templateEntry,
                     SECItem* src, PLArenaPool* arena)
{
    if (templateEntry->kind & SEC_ASN1_POINTER)
    {
        return DecodePointer((void*)((char*)dest ),
                             templateEntry, src, arena, PR_FALSE);
    }
    else
    {
        return DecodeInline((void*)((char*)dest ),
                             templateEntry, src, arena, PR_FALSE);
    }
}

static SECStatus DecodeChoice(void* dest,
                     const SEC_ASN1Template* templateEntry,
                     SECItem* src, PLArenaPool* arena)
{
    SECStatus rv = SECSuccess;
    SECItem choice;
    const SEC_ASN1Template* choiceTemplate = &(templateEntry[1]);
    const SEC_ASN1Template* choiceEntry = NULL;
    unsigned long choiceindex = 0;

    /* XXX for a choice component, we should validate the template to make
       sure the tags are distinct, in debug builds. This hasn't been
       implemented yet */
    /* rv = CheckChoiceTemplate(sequenceTemplate); */

    /* process it */
    do
    {
        choice = *src;
        choiceEntry = &choiceTemplate[choiceindex++];
        if (choiceEntry->kind)
        {
            rv = DecodeItem(dest, choiceEntry, &choice, arena, PR_TRUE);
        }
    } while ( (SECFailure == rv) && (choiceEntry->kind));

    if (SECFailure == rv)
    {
        /* the component didn't match any of the choices */
        PORT_SetError(SEC_ERROR_BAD_DER);
    }
    else
    {
        /* set the type in the union here */
        int *which = (int *)((char *)dest + templateEntry->offset);
        *which = (int)choiceEntry->size;
    }

    /* we should have consumed all the bytes by now */
    /* fail if we have not */
    if (SECSuccess == rv && choice.len)
    {
        /* there is extra data that isn't listed in the template */
        PORT_SetError(SEC_ERROR_BAD_DER);
        rv = SECFailure;
    }
    return rv;
}

static SECStatus DecodeGroup(void* dest,
                     const SEC_ASN1Template* templateEntry,
                     SECItem* src, PLArenaPool* arena)
{
    SECStatus rv = SECSuccess;
    SECItem source;
    SECItem group;
    PRUint32 totalEntries = 0;
    PRUint32 entryIndex = 0;
    void** entries = NULL;

    const SEC_ASN1Template* subTemplate =
        SEC_ASN1GetSubtemplate (templateEntry, dest, PR_FALSE);

    source = *src;

    /* get the group */
    if (SECSuccess == rv)
    {
        rv = GetItem(&source, &group, PR_FALSE);
    }

    /* XXX we should check the subtemplate in debug builds */
    if (SECSuccess == rv)
    {
        /* first, count the number of entries. Benchmarking showed that this
           counting pass is more efficient than trying to allocate entries as
           we read the DER, even if allocating many entries at a time
        */
        SECItem counter = group;
        do
        {
            SECItem anitem;
            rv = GetItem(&counter, &anitem, PR_TRUE);
            if (SECSuccess == rv && (anitem.len) )
            {
                totalEntries++;
            }
        }  while ( (SECSuccess == rv) && (counter.len) );

        if (SECSuccess == rv)
        {
            /* allocate room for pointer array and entries */
            /* we want to allocate the array even if there is 0 entry */
            entries = (void**)PORT_ArenaZAlloc(arena, sizeof(void*)*
                                          (totalEntries + 1 ) + /* the extra one is for NULL termination */
                                          subTemplate->size*totalEntries); 

            if (entries)
            {
                entries[totalEntries] = NULL; /* terminate the array */
            }
            else
            {
                PORT_SetError(SEC_ERROR_NO_MEMORY);
                rv = SECFailure;
            }
            if (SECSuccess == rv)
            {
                void* entriesData = (unsigned char*)entries + (unsigned long)(sizeof(void*)*(totalEntries + 1 ));
                /* and fix the pointers in the array */
                PRUint32 entriesIndex = 0;
                for (entriesIndex = 0;entriesIndex<totalEntries;entriesIndex++)
                {
                    entries[entriesIndex] =
                        (char*)entriesData + (subTemplate->size*entriesIndex);
                }
            }
        }
    }

    if (SECSuccess == rv && totalEntries)
    do
    {
        if (!(entryIndex<totalEntries))
        {
            rv = SECFailure;
            break;
        }
        rv = DecodeItem(entries[entryIndex++], subTemplate, &group, arena, PR_TRUE);
    } while ( (SECSuccess == rv) && (group.len) );
    /* we should be at the end of the set by now */    
    /* save the entries where requested */
    memcpy(((char*)dest + templateEntry->offset), &entries, sizeof(void**));

    return rv;
}

static SECStatus DecodeExplicit(void* dest,
                     const SEC_ASN1Template* templateEntry,
                     SECItem* src, PLArenaPool* arena)
{
    SECStatus rv = SECSuccess;
    SECItem subItem;
    SECItem constructed = *src;

    rv = GetItem(&constructed, &subItem, PR_FALSE);

    if (SECSuccess == rv)
    {
        if (templateEntry->kind & SEC_ASN1_POINTER)
        {
            rv = DecodePointer(dest, templateEntry, &subItem, arena, PR_TRUE);
        }
        else
        {
            rv = DecodeInline(dest, templateEntry, &subItem, arena, PR_TRUE);
        }
    }

    return rv;
}

/* new decoder implementation. This is a recursive function */

static SECStatus DecodeItem(void* dest,
                     const SEC_ASN1Template* templateEntry,
                     SECItem* src, PLArenaPool* arena, PRBool checkTag)
{
    SECStatus rv = SECSuccess;
    SECItem temp;
    SECItem mark;
    PRBool pop = PR_FALSE;
    PRBool decode = PR_TRUE;
    PRBool save = PR_FALSE;
    unsigned long kind;
    PRBool match = PR_TRUE;
    PRBool optional = PR_FALSE;

    PR_ASSERT(src && dest && templateEntry && arena);
#if 0
    if (!src || !dest || !templateEntry || !arena)
    {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        rv = SECFailure;
    }
#endif

    if (SECSuccess == rv)
    {
        /* do the template validation */
        kind = templateEntry->kind;
        optional = (0 != (kind & SEC_ASN1_OPTIONAL));
        if (!kind)
        {
            PORT_SetError(SEC_ERROR_BAD_TEMPLATE);
            rv = SECFailure;
        }
    }

    if (SECSuccess == rv)
    {
#ifdef DEBUG
        if (kind & SEC_ASN1_DEBUG_BREAK)
        {
            /* when debugging the decoder or a template that fails to
            decode, put SEC_ASN1_DEBUG in the component that gives you
            trouble. The decoder will then get to this block and assert.
            If you want to debug the rest of the code, you can set a
            breakpoint and set dontassert to PR_TRUE, which will let
            you skip over the assert and continue the debugging session
            past it. */
            PRBool dontassert = PR_FALSE;
            PR_ASSERT(dontassert); /* set bkpoint here & set dontassert*/
        }
#endif

        if ((kind & SEC_ASN1_SKIP) ||
            (kind & SEC_ASN1_SAVE))
        {
            /* if skipping or saving this component, don't decode it */
            decode = PR_FALSE;
        }
    
        if (kind & (SEC_ASN1_SAVE | SEC_ASN1_OPTIONAL))
        {
            /* if saving this component, or if it is optional, we may not want to
               move past it, so save the position in case we have to rewind */
            mark = *src;
            if (kind & SEC_ASN1_SAVE)
            {
                save = PR_TRUE;
                if (0 == (kind & SEC_ASN1_SKIP))
                {
                    /* we will for sure have to rewind when saving this
                       component and not skipping it. This is true for all
                       legacy uses of SEC_ASN1_SAVE where the following entry
                       in the template would causes the same component to be
                       processed again */
                    pop = PR_TRUE;
                }
            }
        }

        rv = GetItem(src, &temp, PR_TRUE);
    }

    if (SECSuccess == rv)
    {
        /* now check if the component matches what we expect in the template */

        if (PR_TRUE == checkTag)

        {
            rv = MatchComponentType(templateEntry, &temp, &match, dest);
        }

        if ( (SECSuccess == rv) && (PR_TRUE != match) )
        {
            if (kind & SEC_ASN1_OPTIONAL)
            {

                /* the optional component is missing. This is not fatal. */
                /* Rewind, don't decode, and don't save */
                pop = PR_TRUE;
                decode = PR_FALSE;
                save = PR_FALSE;
            }
            else
            {
                /* a required component is missing. abort */
                PORT_SetError(SEC_ERROR_BAD_DER);
                rv = SECFailure;
            }
        }
    }

    if ((SECSuccess == rv) && (PR_TRUE == decode))
    {
        /* the order of processing here is is the tricky part */
        /* we start with our special cases */
        /* first, check the component class */
        if (kind & SEC_ASN1_INLINE)
        {
            /* decode inline template */
            rv = DecodeInline(dest, templateEntry, &temp , arena, PR_TRUE);
        }

        else
        if (kind & SEC_ASN1_EXPLICIT)
        {
            rv = DecodeExplicit(dest, templateEntry, &temp, arena);
        }
        else
        if ( (SEC_ASN1_UNIVERSAL != (kind & SEC_ASN1_CLASS_MASK)) &&

              (!(kind & SEC_ASN1_EXPLICIT)))
        {

            /* decode implicitly tagged components */
            rv = DecodeImplicit(dest, templateEntry, &temp , arena);
        }
        else
        if (kind & SEC_ASN1_POINTER)
        {
            rv = DecodePointer(dest, templateEntry, &temp, arena, PR_TRUE);
        }
        else
        if (kind & SEC_ASN1_CHOICE)
        {
            rv = DecodeChoice(dest, templateEntry, &temp, arena);
        }
        else
        if (kind & SEC_ASN1_ANY)
        {
            /* catch-all ANY type, don't decode */
            save = PR_TRUE;
            if (kind & SEC_ASN1_INNER)
            {
                /* skip the tag and length */
                SECItem newtemp = temp;
                rv = GetItem(&newtemp, &temp, PR_FALSE);
            }
        }
        else
        if (kind & SEC_ASN1_GROUP)
        {
            if ( (SEC_ASN1_SEQUENCE == (kind & SEC_ASN1_TAGNUM_MASK)) ||
                 (SEC_ASN1_SET == (kind & SEC_ASN1_TAGNUM_MASK)) )
            {
                rv = DecodeGroup(dest, templateEntry, &temp , arena);
            }
            else
            {
                /* a group can only be a SET OF or SEQUENCE OF */
                PORT_SetError(SEC_ERROR_BAD_TEMPLATE);
                rv = SECFailure;
            }
        }
        else
        if (SEC_ASN1_SEQUENCE == (kind & SEC_ASN1_TAGNUM_MASK))
        {
            /* plain SEQUENCE */
            rv = DecodeSequence(dest, templateEntry, &temp , arena);
        }
        else
        {
            /* handle all other types as "save" */
            /* we should only get here for primitive universal types */
            SECItem newtemp = temp;
            rv = GetItem(&newtemp, &temp, PR_FALSE);
            save = PR_TRUE;
            if ((SECSuccess == rv) &&
                SEC_ASN1_UNIVERSAL == (kind & SEC_ASN1_CLASS_MASK))
            {
                unsigned long tagnum = kind & SEC_ASN1_TAGNUM_MASK;
                if ( temp.len == 0 && (tagnum == SEC_ASN1_BOOLEAN ||
                                       tagnum == SEC_ASN1_INTEGER ||
                                       tagnum == SEC_ASN1_BIT_STRING ||
                                       tagnum == SEC_ASN1_OBJECT_ID ||
                                       tagnum == SEC_ASN1_ENUMERATED ||
                                       tagnum == SEC_ASN1_UTC_TIME ||
                                       tagnum == SEC_ASN1_GENERALIZED_TIME) )
                {
                    /* these types MUST have at least one content octet */
                    PORT_SetError(SEC_ERROR_BAD_DER);
                    rv = SECFailure;
                }
                else
                switch (tagnum)
                {
                /* special cases of primitive types */
                case SEC_ASN1_INTEGER:
                    {
                        /* remove leading zeroes if the caller requested
                           siUnsignedInteger
                           This is to allow RSA key operations to work */
                        SECItem* destItem = (SECItem*) ((char*)dest +
                                            templateEntry->offset);
                        if (destItem && (siUnsignedInteger == destItem->type))
                        {
                            while (temp.len > 1 && temp.data[0] == 0)
                            {              /* leading 0 */
                                temp.data++;
                                temp.len--;
                            }
                        }
                        break;
                    }

                case SEC_ASN1_BIT_STRING:
                    {
                        /* change the length in the SECItem to be the number
                           of bits */
                        temp.len = (temp.len-1)*8 - (temp.data[0] & 0x7);
                        temp.data++;
                        break;
                    }

                default:
                    {
                        break;
                    }
                }
            }
        }
    }

    if ((SECSuccess == rv) && (PR_TRUE == save))
    {
        SECItem* destItem = (SECItem*) ((char*)dest + templateEntry->offset);
        if (destItem)
        {
            /* we leave the type alone in the destination SECItem.
               If part of the destination was allocated by the decoder, in
               cases of POINTER, SET OF and SEQUENCE OF, then type is set to
               siBuffer due to the use of PORT_ArenaZAlloc*/
            destItem->data = temp.len ? temp.data : NULL;
            destItem->len = temp.len;
        }
        else
        {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            rv = SECFailure;
        }
    }

    if (PR_TRUE == pop)
    {
        /* we don't want to move ahead, so restore the position */
        *src = mark;
    }
    return rv;
}

/* the function below is the public one */

SECStatus SEC_QuickDERDecodeItem(PLArenaPool* arena, void* dest,
                     const SEC_ASN1Template* templateEntry,
                     const SECItem* src)
{
    SECStatus rv = SECSuccess;
    SECItem newsrc;

    if (!arena || !templateEntry || !src)
    {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        rv = SECFailure;
    }

    if (SECSuccess == rv)
    {
        newsrc = *src;
        rv = DecodeItem(dest, templateEntry, &newsrc, arena, PR_TRUE);
        if (SECSuccess == rv && newsrc.len)
        {
            rv = SECFailure;
            PORT_SetError(SEC_ERROR_EXTRA_INPUT);
        }
    }

    return rv;
}

