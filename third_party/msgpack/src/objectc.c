/*
 * MessagePack for C dynamic typing routine
 *
 * Copyright (C) 2008-2009 FURUHASHI Sadayuki
 *
 *    Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *    http://www.boost.org/LICENSE_1_0.txt)
 */
#include "msgpack/object.h"
#include "msgpack/pack.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER)
#if _MSC_VER >= 1800
#include <inttypes.h>
#else
#define PRIu64 "I64u"
#define PRIi64 "I64i"
#define PRIi8 "i"
#endif
#else
#include <inttypes.h>
#endif


int msgpack_pack_object(msgpack_packer* pk, msgpack_object d)
{
    switch(d.type) {
    case MSGPACK_OBJECT_NIL:
        return msgpack_pack_nil(pk);

    case MSGPACK_OBJECT_BOOLEAN:
        if(d.via.boolean) {
            return msgpack_pack_true(pk);
        } else {
            return msgpack_pack_false(pk);
        }

    case MSGPACK_OBJECT_POSITIVE_INTEGER:
        return msgpack_pack_uint64(pk, d.via.u64);

    case MSGPACK_OBJECT_NEGATIVE_INTEGER:
        return msgpack_pack_int64(pk, d.via.i64);

    case MSGPACK_OBJECT_FLOAT32:
        return msgpack_pack_float(pk, (float)d.via.f64);

    case MSGPACK_OBJECT_FLOAT64:
        return msgpack_pack_double(pk, d.via.f64);

    case MSGPACK_OBJECT_STR:
        {
            int ret = msgpack_pack_str(pk, d.via.str.size);
            if(ret < 0) { return ret; }
            return msgpack_pack_str_body(pk, d.via.str.ptr, d.via.str.size);
        }

    case MSGPACK_OBJECT_BIN:
        {
            int ret = msgpack_pack_bin(pk, d.via.bin.size);
            if(ret < 0) { return ret; }
            return msgpack_pack_bin_body(pk, d.via.bin.ptr, d.via.bin.size);
        }

    case MSGPACK_OBJECT_EXT:
        {
            int ret = msgpack_pack_ext(pk, d.via.ext.size, d.via.ext.type);
            if(ret < 0) { return ret; }
            return msgpack_pack_ext_body(pk, d.via.ext.ptr, d.via.ext.size);
        }

    case MSGPACK_OBJECT_ARRAY:
        {
            int ret = msgpack_pack_array(pk, d.via.array.size);
            if(ret < 0) {
                return ret;
            }
            else {
                msgpack_object* o = d.via.array.ptr;
                msgpack_object* const oend = d.via.array.ptr + d.via.array.size;
                for(; o != oend; ++o) {
                    ret = msgpack_pack_object(pk, *o);
                    if(ret < 0) { return ret; }
                }

                return 0;
            }
        }

    case MSGPACK_OBJECT_MAP:
        {
            int ret = msgpack_pack_map(pk, d.via.map.size);
            if(ret < 0) {
                return ret;
            }
            else {
                msgpack_object_kv* kv = d.via.map.ptr;
                msgpack_object_kv* const kvend = d.via.map.ptr + d.via.map.size;
                for(; kv != kvend; ++kv) {
                    ret = msgpack_pack_object(pk, kv->key);
                    if(ret < 0) { return ret; }
                    ret = msgpack_pack_object(pk, kv->val);
                    if(ret < 0) { return ret; }
                }

                return 0;
            }
        }

    default:
        return -1;
    }
}


static void msgpack_object_bin_print(FILE* out, const char *ptr, size_t size)
{
    size_t i;
    for (i = 0; i < size; ++i) {
        if (ptr[i] == '"') {
            fputs("\\\"", out);
        } else if (isprint((unsigned char)ptr[i])) {
            fputc(ptr[i], out);
        } else {
            fprintf(out, "\\x%02x", (unsigned char)ptr[i]);
        }
    }
}

static int msgpack_object_bin_print_buffer(char *buffer, size_t buffer_size, const char *ptr, size_t size)
{
    size_t i;
    char *aux_buffer = buffer;
    size_t aux_buffer_size = buffer_size;
    int ret;

    for (i = 0; i < size; ++i) {
        if (ptr[i] == '"') {
            ret = snprintf(aux_buffer, aux_buffer_size, "\\\"");
            aux_buffer = aux_buffer + ret;
            aux_buffer_size = aux_buffer_size - ret;
        } else if (isprint((unsigned char)ptr[i])) {
            if (aux_buffer_size > 0) {
                memcpy(aux_buffer, ptr + i, 1);
                aux_buffer = aux_buffer + 1;
                aux_buffer_size = aux_buffer_size - 1;
            }
        } else {
            ret = snprintf(aux_buffer, aux_buffer_size, "\\x%02x", (unsigned char)ptr[i]);
            aux_buffer = aux_buffer + ret;
            aux_buffer_size = aux_buffer_size - ret;
        }
    }

    return buffer_size - aux_buffer_size;
}


void msgpack_object_print(FILE* out, msgpack_object o)
{
    switch(o.type) {
    case MSGPACK_OBJECT_NIL:
        fprintf(out, "nil");
        break;

    case MSGPACK_OBJECT_BOOLEAN:
        fprintf(out, (o.via.boolean ? "true" : "false"));
        break;

    case MSGPACK_OBJECT_POSITIVE_INTEGER:
#if defined(PRIu64)
        fprintf(out, "%" PRIu64, o.via.u64);
#else
        if (o.via.u64 > ULONG_MAX)
            fprintf(out, "over 4294967295");
        else
            fprintf(out, "%lu", (unsigned long)o.via.u64);
#endif
        break;

    case MSGPACK_OBJECT_NEGATIVE_INTEGER:
#if defined(PRIi64)
        fprintf(out, "%" PRIi64, o.via.i64);
#else
        if (o.via.i64 > LONG_MAX)
            fprintf(out, "over +2147483647");
        else if (o.via.i64 < LONG_MIN)
            fprintf(out, "under -2147483648");
        else
            fprintf(out, "%ld", (signed long)o.via.i64);
#endif
        break;

    case MSGPACK_OBJECT_FLOAT32:
    case MSGPACK_OBJECT_FLOAT64:
        fprintf(out, "%f", o.via.f64);
        break;

    case MSGPACK_OBJECT_STR:
        fprintf(out, "\"");
        fwrite(o.via.str.ptr, o.via.str.size, 1, out);
        fprintf(out, "\"");
        break;

    case MSGPACK_OBJECT_BIN:
        fprintf(out, "\"");
        msgpack_object_bin_print(out, o.via.bin.ptr, o.via.bin.size);
        fprintf(out, "\"");
        break;

    case MSGPACK_OBJECT_EXT:
#if defined(PRIi8)
        fprintf(out, "(ext: %" PRIi8 ")", o.via.ext.type);
#else
        fprintf(out, "(ext: %d)", (int)o.via.ext.type);
#endif
        fprintf(out, "\"");
        msgpack_object_bin_print(out, o.via.ext.ptr, o.via.ext.size);
        fprintf(out, "\"");
        break;

    case MSGPACK_OBJECT_ARRAY:
        fprintf(out, "[");
        if(o.via.array.size != 0) {
            msgpack_object* p = o.via.array.ptr;
            msgpack_object* const pend = o.via.array.ptr + o.via.array.size;
            msgpack_object_print(out, *p);
            ++p;
            for(; p < pend; ++p) {
                fprintf(out, ", ");
                msgpack_object_print(out, *p);
            }
        }
        fprintf(out, "]");
        break;

    case MSGPACK_OBJECT_MAP:
        fprintf(out, "{");
        if(o.via.map.size != 0) {
            msgpack_object_kv* p = o.via.map.ptr;
            msgpack_object_kv* const pend = o.via.map.ptr + o.via.map.size;
            msgpack_object_print(out, p->key);
            fprintf(out, "=>");
            msgpack_object_print(out, p->val);
            ++p;
            for(; p < pend; ++p) {
                fprintf(out, ", ");
                msgpack_object_print(out, p->key);
                fprintf(out, "=>");
                msgpack_object_print(out, p->val);
            }
        }
        fprintf(out, "}");
        break;

    default:
        // FIXME
#if defined(PRIu64)
        fprintf(out, "#<UNKNOWN %i %" PRIu64 ">", o.type, o.via.u64);
#else
        if (o.via.u64 > ULONG_MAX)
            fprintf(out, "#<UNKNOWN %i over 4294967295>", o.type);
        else
            fprintf(out, "#<UNKNOWN %i %lu>", o.type, (unsigned long)o.via.u64);
#endif

    }
}

int msgpack_object_print_buffer(char *buffer, size_t buffer_size, msgpack_object o)
{
    char *aux_buffer = buffer;
    size_t aux_buffer_size = buffer_size;
    int ret;
    switch(o.type) {
    case MSGPACK_OBJECT_NIL:
        ret = snprintf(aux_buffer, aux_buffer_size, "nil");
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
        break;

    case MSGPACK_OBJECT_BOOLEAN:
        ret = snprintf(aux_buffer, aux_buffer_size, (o.via.boolean ? "true" : "false"));
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
        break;

    case MSGPACK_OBJECT_POSITIVE_INTEGER:
#if defined(PRIu64)
        ret = snprintf(aux_buffer, aux_buffer_size, "%" PRIu64, o.via.u64);
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
#else
        if (o.via.u64 > ULONG_MAX) {
            ret = snprintf(aux_buffer, aux_buffer_size, "over 4294967295");
            aux_buffer = aux_buffer + ret;
            aux_buffer_size = aux_buffer_size - ret;
        } else {
            ret = snprintf(aux_buffer, aux_buffer_size, "%lu", (unsigned long)o.via.u64);
            aux_buffer = aux_buffer + ret;
            aux_buffer_size = aux_buffer_size - ret;
        }
#endif
        break;

    case MSGPACK_OBJECT_NEGATIVE_INTEGER:
#if defined(PRIi64)
        ret = snprintf(aux_buffer, aux_buffer_size, "%" PRIi64, o.via.i64);
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
#else
        if (o.via.i64 > LONG_MAX) {
            ret = snprintf(aux_buffer, aux_buffer_size, "over +2147483647");
            aux_buffer = aux_buffer + ret;
            aux_buffer_size = aux_buffer_size - ret;
        } else if (o.via.i64 < LONG_MIN) {
            ret = snprintf(aux_buffer, aux_buffer_size, "under -2147483648");
            aux_buffer = aux_buffer + ret;
            aux_buffer_size = aux_buffer_size - ret;
        } else {
            ret = snprintf(aux_buffer, aux_buffer_size, "%ld", (signed long)o.via.i64);
            aux_buffer = aux_buffer + ret;
            aux_buffer_size = aux_buffer_size - ret;
        }
#endif
        break;

    case MSGPACK_OBJECT_FLOAT32:
    case MSGPACK_OBJECT_FLOAT64:
        ret = snprintf(aux_buffer, aux_buffer_size, "%f", o.via.f64);
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
        break;

    case MSGPACK_OBJECT_STR:
        ret = snprintf(aux_buffer, aux_buffer_size, "\"");
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
        ret = snprintf(aux_buffer, aux_buffer_size, "%.*s", (int)o.via.str.size, o.via.str.ptr);
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
        ret = snprintf(aux_buffer, aux_buffer_size, "\"");
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
        break;

    case MSGPACK_OBJECT_BIN:
        ret = snprintf(aux_buffer, aux_buffer_size, "\"");
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;

        ret = msgpack_object_bin_print_buffer(aux_buffer, aux_buffer_size, o.via.bin.ptr, o.via.bin.size);
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;

        ret = snprintf(aux_buffer, aux_buffer_size, "\"");
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
        break;

    case MSGPACK_OBJECT_EXT:
#if defined(PRIi8)
        ret = snprintf(aux_buffer, aux_buffer_size, "(ext: %" PRIi8 ")", o.via.ext.type);
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
#else
        ret = snprintf(aux_buffer, aux_buffer_size, "(ext: %d)", (int)o.via.ext.type);
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
#endif
        ret = snprintf(aux_buffer, aux_buffer_size, "\"");
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;

        ret = msgpack_object_bin_print_buffer(aux_buffer, aux_buffer_size, o.via.ext.ptr, o.via.ext.size);
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;

        ret = snprintf(aux_buffer, aux_buffer_size, "\"");
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
        break;

    case MSGPACK_OBJECT_ARRAY:
        ret = snprintf(aux_buffer, aux_buffer_size, "[");
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
        if(o.via.array.size != 0) {
            msgpack_object* p = o.via.array.ptr;
            msgpack_object* const pend = o.via.array.ptr + o.via.array.size;
            ret = msgpack_object_print_buffer(aux_buffer, aux_buffer_size, *p);
            aux_buffer = aux_buffer + ret;
            aux_buffer_size = aux_buffer_size - ret;
            ++p;
            for(; p < pend; ++p) {
                ret = snprintf(aux_buffer, aux_buffer_size, ", ");
                aux_buffer = aux_buffer + ret;
                aux_buffer_size = aux_buffer_size - ret;
                ret = msgpack_object_print_buffer(aux_buffer, aux_buffer_size, *p);
                aux_buffer = aux_buffer + ret;
                aux_buffer_size = aux_buffer_size - ret;
            }
        }
        ret = snprintf(aux_buffer, aux_buffer_size, "]");
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
        break;

    case MSGPACK_OBJECT_MAP:
        ret = snprintf(aux_buffer, aux_buffer_size, "{");
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
        if(o.via.map.size != 0) {
            msgpack_object_kv* p = o.via.map.ptr;
            msgpack_object_kv* const pend = o.via.map.ptr + o.via.map.size;
            ret = msgpack_object_print_buffer(aux_buffer, aux_buffer_size, p->key);
            aux_buffer = aux_buffer + ret;
            aux_buffer_size = aux_buffer_size - ret;
            ret = snprintf(aux_buffer, aux_buffer_size, "=>");
            aux_buffer = aux_buffer + ret;
            aux_buffer_size = aux_buffer_size - ret;
            ret = msgpack_object_print_buffer(aux_buffer, aux_buffer_size, p->val);
            aux_buffer = aux_buffer + ret;
            aux_buffer_size = aux_buffer_size - ret;
            ++p;
            for(; p < pend; ++p) {
                ret = snprintf(aux_buffer, aux_buffer_size, ", ");
                aux_buffer = aux_buffer + ret;
                aux_buffer_size = aux_buffer_size - ret;
                ret = msgpack_object_print_buffer(aux_buffer, aux_buffer_size, p->key);
                aux_buffer = aux_buffer + ret;
                aux_buffer_size = aux_buffer_size - ret;
                ret = snprintf(aux_buffer, aux_buffer_size, "=>");
                aux_buffer = aux_buffer + ret;
                aux_buffer_size = aux_buffer_size - ret;
                ret = msgpack_object_print_buffer(aux_buffer, aux_buffer_size, p->val);
                aux_buffer = aux_buffer + ret;
                aux_buffer_size = aux_buffer_size - ret;
            }
        }
        ret = snprintf(aux_buffer, aux_buffer_size, "}");
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
        break;

    default:
    // FIXME
#if defined(PRIu64)
        ret = snprintf(aux_buffer, aux_buffer_size, "#<UNKNOWN %i %" PRIu64 ">", o.type, o.via.u64);
        aux_buffer = aux_buffer + ret;
        aux_buffer_size = aux_buffer_size - ret;
#else
        if (o.via.u64 > ULONG_MAX) {
            ret = snprintf(aux_buffer, aux_buffer_size, "#<UNKNOWN %i over 4294967295>", o.type);
            aux_buffer = aux_buffer + ret;
            aux_buffer_size = aux_buffer_size - ret;
        } else {
            ret = snprintf(aux_buffer, aux_buffer_size, "#<UNKNOWN %i %lu>", o.type, (unsigned long)o.via.u64);
            aux_buffer = aux_buffer + ret;
            aux_buffer_size = aux_buffer_size - ret;
        }
#endif
    }

    return buffer_size - aux_buffer_size;
}


bool msgpack_object_equal(const msgpack_object x, const msgpack_object y)
{
    if(x.type != y.type) { return false; }

    switch(x.type) {
    case MSGPACK_OBJECT_NIL:
        return true;

    case MSGPACK_OBJECT_BOOLEAN:
        return x.via.boolean == y.via.boolean;

    case MSGPACK_OBJECT_POSITIVE_INTEGER:
        return x.via.u64 == y.via.u64;

    case MSGPACK_OBJECT_NEGATIVE_INTEGER:
        return x.via.i64 == y.via.i64;

    case MSGPACK_OBJECT_FLOAT32:
    case MSGPACK_OBJECT_FLOAT64:
        return x.via.f64 == y.via.f64;

    case MSGPACK_OBJECT_STR:
        return x.via.str.size == y.via.str.size &&
            memcmp(x.via.str.ptr, y.via.str.ptr, x.via.str.size) == 0;

    case MSGPACK_OBJECT_BIN:
        return x.via.bin.size == y.via.bin.size &&
            memcmp(x.via.bin.ptr, y.via.bin.ptr, x.via.bin.size) == 0;

    case MSGPACK_OBJECT_EXT:
        return x.via.ext.size == y.via.ext.size &&
            x.via.ext.type == y.via.ext.type &&
            memcmp(x.via.ext.ptr, y.via.ext.ptr, x.via.ext.size) == 0;

    case MSGPACK_OBJECT_ARRAY:
        if(x.via.array.size != y.via.array.size) {
            return false;
        } else if(x.via.array.size == 0) {
            return true;
        } else {
            msgpack_object* px = x.via.array.ptr;
            msgpack_object* const pxend = x.via.array.ptr + x.via.array.size;
            msgpack_object* py = y.via.array.ptr;
            do {
                if(!msgpack_object_equal(*px, *py)) {
                    return false;
                }
                ++px;
                ++py;
            } while(px < pxend);
            return true;
        }

    case MSGPACK_OBJECT_MAP:
        if(x.via.map.size != y.via.map.size) {
            return false;
        } else if(x.via.map.size == 0) {
            return true;
        } else {
            msgpack_object_kv* px = x.via.map.ptr;
            msgpack_object_kv* const pxend = x.via.map.ptr + x.via.map.size;
            msgpack_object_kv* py = y.via.map.ptr;
            do {
                if(!msgpack_object_equal(px->key, py->key) || !msgpack_object_equal(px->val, py->val)) {
                    return false;
                }
                ++px;
                ++py;
            } while(px < pxend);
            return true;
        }

    default:
        return false;
    }
}
