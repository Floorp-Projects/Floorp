# Plan: Add tests for PR #2429 (Improvement command palette)

## Context

PR #2429 adds multi-step command support to the command palette (Open URL, Search Web, Reopen in Container), 3 new gesture actions, and F2 default shortcut. The implementation has no test coverage for any of the new functionality. The project uses browser-integration tests that run inside the actual Firefox browser, so all Firefox APIs are available.

## Test files to modify/create

### 1. `browser-features/chrome/common/command-palette/test/commandPalette.test.ts` (modify)

Add tests for:

**A. New step commands exist in registry**
- `floorp-open-url`, `floorp-search-web`, `floorp-reopen-in-container` returned by `getPaletteCommands()`
- Each has `steps` array with length > 0

**B. Step command structure validation**
- For each step command: every `CommandStep` has `id`, `label`, `placeholder` (non-empty strings)
- `fn` accepts 2 args (`fn(win, args)`) — verify it doesn't throw when called with minimal args

**C. Step validate functions**
- `open-url` step 1 (`url`): empty string returns string error, non-empty returns `true`
- `search-web` step 1 (`query`): empty string returns string error, non-empty returns `true`

**D. Static choices validation**
- `open-url` step 2 (`where`): has 3 choices with `label`, `value`, `description`; values are `"new-tab"`, `"background-tab"`, `"current-tab"`
- `search-web` step 3 (`where`): same pattern

**E. choicesLoader functions**
- `loadSearchEngines()` returns `Promise<CommandStepChoice[]>` — each has `label` and `value`
- `loadContainers()` returns `Promise<CommandStepChoice[]>` — each has `label` and `value`; first entry has `value: "0"` (No Container)

### 2. `browser-features/chrome/common/command-palette/test/commandPaletteController.test.ts` (create)

New file for controller state machine tests:

**A. Controller instantiation**
- Construct with `new CommandPaletteController(window)` — succeeds
- Initial state: `mode() === "command"`, `isVisible() === false`

**B. enterInputMode via executeCommand**
- Call `executeCommand(stepCommand)` → `mode()` becomes `"input"`
- `activeCommand()` is set to the command
- `currentStepIndex()` is `0`
- `stepInputs()` is `{}`
- `stepError()` is `null`

**C. advanceStep progression**
- With a test command that has 2 steps (no validate):
  - Set query via `updateSearch("test value")`
  - Call `advanceStep()` → `currentStepIndex()` becomes `1`
  - `stepInputs()` has `{ [step1Id]: "test value" }`
  - Call `advanceStep()` again → command executes (check fn was called with args)

**D. advanceStep with validation failure**
- With a step that has `validate: (input) => input.trim() ? true : "error"`
- Empty input → `advanceStep()` → `stepError()` is `"error"`, `currentStepIndex()` stays same

**E. goBackStep**
- Advance to step 1, then `goBackStep()` → returns to step 0
- `goBackStep()` at step 0 → `mode()` returns `"command"` (exitInputMode)

**F. exitInputMode state reset**
- Enter input mode, then call `goBackStep()` from step 0
- Verify: `mode() === "command"`, `activeCommand() === null`, `currentStepIndex() === 0`

**G. loadStepChoices with static choices**
- Enter input mode with a command whose step has `choices`
- `filteredStepChoices()` returns the choices array

**H. loadStepChoices with choicesLoader**
- Enter input mode with a command whose step has `choicesLoader: () => Promise.resolve([{label: "A", value: "a"}])`
- After promise resolves: `filteredStepChoices()` returns the loaded choices
- `stepChoicesLoading()` becomes `false`

**I. updateStepChoices filtering**
- Set up step with choices `[label: "Alpha", ...}, {label: "Beta", ...}]`
- `updateSearch("alp")` → `filteredStepChoices()` only contains "Alpha"

**J. handleInputModeKeyDown**
- Enter input mode with choices
- Simulate `ArrowDown` → `selectedChoiceIndex()` increments
- Simulate `ArrowUp` → `selectedChoiceIndex()` decrements (wraps)
- Simulate `Enter` → calls `advanceStep()`

Helper for tests:
```typescript
// Create a minimal PaletteCommand with steps for testing
function makeStepCommand(steps: CommandStep[]): PaletteCommand {
  return {
    id: "__test-step-command__",
    label: "Test",
    description: "Test command",
    category: "test",
    keywords: [],
    fn: (_win, args) => { /* record args */ },
    steps,
  };
}
```

### 3. `browser-features/chrome/common/mouse-gesture/test/mouseGestureActions.test.ts` (modify)

Add to the existing `rawTests` array (follows pattern at line 75):

```typescript
{
  name: "getAllGestureActions includes close-other-tabs actions",
  fn() {
    const actions = getAllGestureActions();
    const names = actions.map((a) => a.name);
    assert(names.includes("gecko-close-other-tabs"), "should include gecko-close-other-tabs");
    assert(names.includes("gecko-close-tabs-to-start"), "should include gecko-close-tabs-to-start");
    assert(names.includes("gecko-close-tabs-to-end"), "should include gecko-close-tabs-to-end");
  },
},
```

## Files summary

| File | Action |
|------|--------|
| `browser-features/chrome/common/command-palette/test/commandPalette.test.ts` | Modify — add ~15 tests |
| `browser-features/chrome/common/command-palette/test/commandPaletteController.test.ts` | Create — add ~12 tests |
| `browser-features/chrome/common/mouse-gesture/test/mouseGestureActions.test.ts` | Modify — add 1 test |

## Verification

1. `deno task dev-tool start` — start browser
2. `deno task test -- --near browser-features/chrome/common/command-palette/test/` — run command palette tests
3. `deno task test -- --near browser-features/chrome/common/mouse-gesture/test/mouseGestureActions.test.ts` — run gesture tests
4. `deno task dev-tool console --level error` — check for runtime errors
