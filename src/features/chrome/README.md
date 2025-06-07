# @nora/core

When build, placed on noraneko/content

This component handles almost all of Noraneko's core code.

If you are to make feature modifying preferences, or other internal html, go to `root/apps/main/about/`.

## Directory Structure

common/

- main codes
- supports hot reload

static/

- codes that can't be hot-reloaded

nora/

- codes that requires noraneko-runtime (esp. cpp patch)
- should be disabled easily for other-runtime

utils/

- Utilities that helps you to code comfortable.
- It is preferred that single file but folder with index.ts can be in.

example/

- as test and template for basic codes
