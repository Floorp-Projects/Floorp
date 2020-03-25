#/bin/bash

# record current bench results
cargo bench --bench benchmark -- --noplot --save-baseline after

# switch to master and record its bench results
git checkout master && \
cargo bench --bench benchmark -- --noplot --save-baseline before

# compare
cargo install critcmp --force && \
critcmp before after
