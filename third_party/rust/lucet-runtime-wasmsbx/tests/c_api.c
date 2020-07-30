#include <stdbool.h>
#include <stdio.h>

#include "lucet.h"

bool lucet_runtime_test_expand_heap(struct lucet_dl_module *mod)
{
    struct lucet_region *     region;
    struct lucet_alloc_limits limits = {
        .heap_memory_size        = 4 * 1024 * 1024,
        .heap_address_space_size = 8 * 1024 * 1024,
        .stack_size              = 64 * 1024,
        .globals_size            = 4096,
        .signal_stack_size       = 12 * 1024,
    };

    enum lucet_error err;

    err = lucet_mmap_region_create(1, &limits, &region);
    if (err != lucet_error_ok) {
        fprintf(stderr, "failed to create region\n");
        goto fail1;
    }

    struct lucet_instance *inst;
    err = lucet_region_new_instance(region, mod, &inst);
    if (err != lucet_error_ok) {
        fprintf(stderr, "failed to create instance\n");
        goto fail2;
    }

    uint32_t newpage_start;
    err = lucet_instance_grow_heap(inst, 1, &newpage_start);
    if (err != lucet_error_ok) {
        fprintf(stderr, "failed to grow memory\n");
        goto fail3;
    }

    lucet_instance_release(inst);
    lucet_region_release(region);
    lucet_dl_module_release(mod);

    return true;

fail3:
    lucet_instance_release(inst);
fail2:
    lucet_region_release(region);
fail1:
    lucet_dl_module_release(mod);
    return false;
}

enum yield_resume_tag {
    yield_resume_tag_mult,
    yield_resume_tag_result,
};

struct yield_resume_mult {
    uint64_t x;
    uint64_t y;
};

union yield_resume_val_inner {
    struct yield_resume_mult mult;
    uint64_t                 result;
};

struct yield_resume_val {
    enum yield_resume_tag        tag;
    union yield_resume_val_inner val;
};

uint64_t lucet_runtime_test_hostcall_yield_resume(struct lucet_vmctx *vmctx, uint64_t n)
{
    if (n <= 1) {
        struct yield_resume_val result_val = { .tag = yield_resume_tag_result,
                                               .val = { .result = 1 } };
        lucet_vmctx_yield(vmctx, &result_val);
        return 1;
    } else {
        uint64_t                n_rec      = lucet_runtime_test_hostcall_yield_resume(vmctx, n - 1);
        struct yield_resume_val mult_val   = { .tag = yield_resume_tag_mult,
                                             .val = { .mult = { .x = n, .y = n_rec } } };
        uint64_t                n          = *(uint64_t *) lucet_vmctx_yield(vmctx, &mult_val);
        struct yield_resume_val result_val = { .tag = yield_resume_tag_result,
                                               .val = { .result = n } };
        lucet_vmctx_yield(vmctx, &result_val);
        return n;
    }
}

bool lucet_runtime_test_yield_resume(struct lucet_dl_module *mod)
{
    struct lucet_region *     region;
    struct lucet_alloc_limits limits = {
        .heap_memory_size        = 4 * 1024 * 1024,
        .heap_address_space_size = 8 * 1024 * 1024,
        .stack_size              = 64 * 1024,
        .globals_size            = 4096,
        .signal_stack_size       = 12 * 1024,
    };

    enum lucet_error err;

    err = lucet_mmap_region_create(1, &limits, &region);
    if (err != lucet_error_ok) {
        fprintf(stderr, "failed to create region\n");
        goto fail1;
    }

    struct lucet_instance *inst;
    err = lucet_region_new_instance(region, mod, &inst);
    if (err != lucet_error_ok) {
        fprintf(stderr, "failed to create instance\n");
        goto fail2;
    }

    uint64_t results[5] = { 0 };
    size_t   i          = 0;

    struct lucet_result res;
    lucet_instance_run(inst, "f", 0, (const struct lucet_val[]){}, &res);
    while (res.tag == lucet_result_tag_yielded) {
        if (i >= 5) {
            fprintf(stderr, "hostcall yielded too many results\n");
            goto fail3;
        }

        struct yield_resume_val val = *(struct yield_resume_val *) res.val.yielded.val;

        switch (val.tag) {
        case yield_resume_tag_mult: {
            uint64_t mult_result = val.val.mult.x * val.val.mult.y;
            lucet_instance_resume(inst, &mult_result, &res);
            continue;
        }
        case yield_resume_tag_result: {
            results[i++] = val.val.result;
            lucet_instance_resume(inst, NULL, &res);
            continue;
        }
        default: {
            fprintf(stderr, "unexpected yield_resume_tag\n");
            goto fail3;
        }
        }
    }
    if (err != lucet_error_ok) {
        fprintf(stderr, "instance finished with non-ok error: %s\n", lucet_error_name(err));
        goto fail3;
    }

    if (res.tag != lucet_result_tag_returned) {
        fprintf(stderr, "final instance result wasn't returned\n");
        goto fail3;
    }

    uint64_t final_result = LUCET_UNTYPED_RETVAL_TO_U64(res.val.returned);

    lucet_instance_release(inst);
    lucet_region_release(region);
    lucet_dl_module_release(mod);

    uint64_t expected_results[5] = { 1, 2, 6, 24, 120 };
    bool     results_correct     = final_result == 120;
    for (i = 0; i < 5; i++) {
        results_correct = results_correct && (results[i] == expected_results[i]);
    }

    return results_correct;

fail3:
    lucet_instance_release(inst);
fail2:
    lucet_region_release(region);
fail1:
    lucet_dl_module_release(mod);
    return false;
}
