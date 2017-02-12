// bindgen-flags: --whitelist-function dl_iterate_phdr

struct dl_phdr_info {
    int x;
};

int dl_iterate_phdr(struct dl_phdr_info *);
