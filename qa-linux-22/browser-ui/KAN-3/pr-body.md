Moving a split tab group into another window can discard a loaded document when the source contains another split. The noncurrent-pane `display: none` rule destroys browser frames during Gecko's window-adoption swap. Keep those panes out of layout with absolute positioning and hidden visibility while preserving their frames.

Adds a browser regression covering two split groups, moving one to another window and adopting it back, asserting both document titles and URLs.

Validation:
- Official Linux Floorp 12.21.1 (Gecko 159.0.1), Debian 13.6, Xvfb/X11, isolated DBus, fresh profiles; browser protections enabled.
- Same source regression bundled with original CSS fails: second document title disappears during outbound adoption.
- Same regression with fixed CSS passes outbound and return adoption.
- Official Firefox 159.0.1 retains titles with the original API reproduction.
- Deno typecheck, lint of added test files, and `git diff --check` pass.

This executes the source CSS and test on the installed product runtime. A complete source build/full browser suite, native Wayland, original Fedora environment and physical drag/menu input remain untested. Draft pending broader validation.

Refs #2567, KAN-3. No merge/release/status transition requested.
