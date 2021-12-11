/* PipeWire
 *
 * Copyright © 2018 Wim Taymans
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef PIPEWIRE_PROPERTIES_H
#define PIPEWIRE_PROPERTIES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
	
#include <spa/utils/dict.h>

/** \class pw_properties
 *
 * \brief A collection of key/value pairs
 *
 * Properties are used to pass around arbitrary key/value pairs.
 * Both keys and values are strings which keeps things simple.
 * Encoding of arbitrary values should be done by using a string
 * serialization such as base64 for binary blobs.
 */
struct pw_properties {
	struct spa_dict dict;	/**< dictionary of key/values */
	uint32_t flags;		/**< extra flags */
};

struct pw_properties *
pw_properties_new(const char *key, ...) SPA_SENTINEL;

struct pw_properties *
pw_properties_new_dict(const struct spa_dict *dict);

struct pw_properties *
pw_properties_new_string(const char *args);

struct pw_properties *
pw_properties_copy(const struct pw_properties *properties);

int pw_properties_update_keys(struct pw_properties *props,
		     const struct spa_dict *dict, const char *keys[]);

int pw_properties_update(struct pw_properties *oldprops,
		     const struct spa_dict *dict);

int pw_properties_add(struct pw_properties *oldprops,
		     const struct spa_dict *dict);
int pw_properties_add_keys(struct pw_properties *oldprops,
		     const struct spa_dict *dict, const char *keys[]);

void pw_properties_clear(struct pw_properties *properties);

void
pw_properties_free(struct pw_properties *properties);

int
pw_properties_set(struct pw_properties *properties, const char *key, const char *value);

int
pw_properties_setf(struct pw_properties *properties,
		   const char *key, const char *format, ...) SPA_PRINTF_FUNC(3, 4);
int
pw_properties_setva(struct pw_properties *properties,
		   const char *key, const char *format, va_list args) SPA_PRINTF_FUNC(3,0);
const char *
pw_properties_get(const struct pw_properties *properties, const char *key);

const char *
pw_properties_iterate(const struct pw_properties *properties, void **state);

static inline bool pw_properties_parse_bool(const char *value) {
	return (strcmp(value, "true") == 0 || atoi(value) == 1);
}

static inline int pw_properties_parse_int(const char *value) {
	return strtol(value, NULL, 0);
}

static inline int64_t pw_properties_parse_int64(const char *value) {
	return strtoll(value, NULL, 0);
}

static inline uint64_t pw_properties_parse_uint64(const char *value) {
	return strtoull(value, NULL, 0);
}

static inline float pw_properties_parse_float(const char *value) {
	return strtof(value, NULL);
}

static inline double pw_properties_parse_double(const char *value) {
	return strtod(value, NULL);
}

#ifdef __cplusplus
}
#endif

#endif /* PIPEWIRE_PROPERTIES_H */
