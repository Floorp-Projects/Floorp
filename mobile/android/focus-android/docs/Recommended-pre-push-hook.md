# Recommended pre-push hook

If you want to reduce your PR turn-around time, I'd recommend adding a
pre-push hook: this script will stop a push if the unit tests or linters
fail, finding the failures before it hits TaskCluster (which takes
forever to dig through the logs):
```sh
#!/bin/sh

./gradlew -q \
        checkstyle \
        ktlint \
        pmd \
        detektCheck \
        app:assembleFocusArmDebug


# Tasks omitted because they take a long time to run:
# - unit test on all variants
# - UI tests
# - lint (compiles all variants)
```

To use it:
1. Create a file with these ^ contents (exclude the "\`") at `<repo>/.git/hooks/pre-push`
1. Make it executable: `chmod 755 <repo>/.git/hooks/pre-push`

And it will run before pushes. Notes:
- Run `git push ... --no-verify` to push without making the check
- It takes ~30 seconds to run. If you think this hook takes too long, you can remove the unit test line and it becomes almost instant.
