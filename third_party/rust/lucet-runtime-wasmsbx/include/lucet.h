#ifndef LUCET_H
#define LUCET_H

#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "lucet_types.h"
#include "lucet_val.h"
#include "lucet_vmctx.h"

#define LUCET_WASM_PAGE_SIZE (64 * 1024)

enum lucet_error lucet_dl_module_load(const char *path, struct lucet_dl_module **mod_out);

void lucet_dl_module_release(const struct lucet_dl_module *module);

const char *lucet_error_name(enum lucet_error e);

bool lucet_instance_check_heap(const struct lucet_instance *inst, const void *ptr, uintptr_t len);

void *lucet_instance_embed_ctx(struct lucet_instance *inst);

enum lucet_error lucet_instance_grow_heap(struct lucet_instance *inst,
                                          uint32_t               additional_pages,
                                          uint32_t *             previous_pages_out);

uint8_t *lucet_instance_heap(struct lucet_instance *inst);

uint32_t lucet_instance_heap_len(const struct lucet_instance *inst);

void lucet_instance_release(struct lucet_instance *inst);

enum lucet_error lucet_instance_reset(struct lucet_instance *inst);

enum lucet_error lucet_instance_run(struct lucet_instance * inst,
                                    const char *            entrypoint,
                                    uintptr_t               argc,
                                    const struct lucet_val *argv,
                                    struct lucet_result *   result_out);

enum lucet_error lucet_instance_run_func_idx(struct lucet_instance * inst,
                                             uint32_t                table_idx,
                                             uint32_t                func_idx,
                                             uintptr_t               argc,
                                             const struct lucet_val *argv,
                                             struct lucet_result *   result_out);

enum lucet_error
lucet_instance_resume(struct lucet_instance *inst, void *val, struct lucet_result *result_out);

enum lucet_error lucet_instance_set_fatal_handler(struct lucet_instance *inst,
                                                  lucet_fatal_handler    fatal_handler);

/**
 * Release or run* must not be called in the body of this function!
 */
enum lucet_error lucet_instance_set_signal_handler(struct lucet_instance *inst,
                                                   lucet_signal_handler   signal_handler);

enum lucet_error lucet_mmap_region_create(uint64_t                         instance_capacity,
                                          const struct lucet_alloc_limits *limits,
                                          struct lucet_region **           region_out);

enum lucet_error lucet_region_new_instance(const struct lucet_region *   region,
                                           const struct lucet_dl_module *module,
                                           struct lucet_instance **      inst_out);

enum lucet_error lucet_region_new_instance_with_ctx(const struct lucet_region *   region,
                                                    const struct lucet_dl_module *module,
                                                    void *                        embed_ctx,
                                                    struct lucet_instance **      inst_out);

void lucet_region_release(const struct lucet_region *region);

float lucet_retval_f32(const struct lucet_untyped_retval *retval);

double lucet_retval_f64(const struct lucet_untyped_retval *retval);

union lucet_retval_gp lucet_retval_gp(const struct lucet_untyped_retval *retval);

const char *lucet_result_tag_name(enum lucet_result_tag tag);

#endif /* LUCET_H */
