#ifndef LUCET_WASI_H
#define LUCET_WASI_H

#include "lucet.h"

struct lucet_wasi_ctx;

struct lucet_wasi_ctx *lucet_wasi_ctx_create(void);

enum lucet_error lucet_wasi_ctx_args(struct lucet_wasi_ctx *wasi_ctx, size_t argc, char **argv);

enum lucet_error lucet_wasi_ctx_inherit_env(struct lucet_wasi_ctx *wasi_ctx);

enum lucet_error lucet_wasi_ctx_inherit_stdio(struct lucet_wasi_ctx *wasi_ctx);

void lucet_wasi_ctx_destroy(struct lucet_wasi_ctx *wasi_ctx);

enum lucet_error lucet_region_new_instance_with_wasi_ctx(const struct lucet_region *   region,
                                                         const struct lucet_dl_module *module,
                                                         struct lucet_wasi_ctx *       wasi_ctx,
                                                         struct lucet_instance **      inst_out);

#endif /* LUCET_WASI_H */
