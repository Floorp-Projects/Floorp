# TPS
## Table of Contents
- [What is TPS](#what-is-tps)
- [Why TPS Exists](#why-tps-exists)
- [Architecture](#architecture)
- [Test Format](#test-format)
- [Writing new TPS tests](#writing-new-tps-tests)
- [How to run TPS](#how-to-run-tps)

The majority of this document is targeting the Firefox developer that would like to understand TPS and/or write TPS tests. If you'd like to simply run TPS, see the [How to run TPS](#how-to-run-tps) section.

## What is TPS
TPS is a test automation framework for Firefox Sync. TPS takes in test configuration as JavaScript files defined in [a configuration file](#test-format) and runs all tests in sequence.

Each TPS test is made of one or more [phases](#phases) that each have their own assertions and run in sequence. If **any** phase fails, then the test fails and the remaining phases are not run. However, other tests will still run.

## Why TPS exists
TPS runs against the real Mozilla Accounts server and Sync servers and thus is a good way to test Firefox Sync end-to-end without mocking the servers.

## Architecture
### High level Diagram
The following diagram describes the flow when running TPS.

```{mermaid}
flowchart TD
TPS[TPS Runner]-->T[Run Test Suite]
T-->T1[Run Next Single Test]
T1-->S[Setup]
S-->P[Run Next Phase]
P-->LP[Launch Test Profile]
LP-->Q{Phase Success?}
Q-->|No| CL[Clean up]
Q-->|Yes| Q2{Any more phases?}
Q2-->|Yes|P
Q2-->|No| CL
CL-->CR[Collect Results]
CR-->Q3{Any more tests?}
Q3-->|Yes|T1
Q3-->|No| D[Done]
```

### Single Test Sequence Diagram
The following sequence diagram describes the involved entities executing a single TPS test
```{mermaid}
sequenceDiagram
actor U as User
participant TPS as TPS Runner
participant F as Firefox Profile
participant TE as TPS Extension
U->>TPS: Start test
TPS->>TPS: Parse Test file
loop Every Phase
TPS->>F: Launch Phase w/prefs to install TPS extension
F->>TE: Install Extension
TE->>F: Read prefs to get test file
F->>TE: Test file and TPS Runner configuration
loop Every instruction
TE->>F: Execute test instructions and assertions
F->>TE: Result
end
TE->>F: Phase Done
F->>TPS: Done
TPS->>F: Read logs to get test results
end
TPS->>U: Print test results
```


### Phases
Each Phase is mapped to a Firefox profile, phases may re-use profiles if they are mapped to them, see [the phases object](#the-phases-object) for details on the mapping. All phases are signed in to the same Mozilla Account.
#### Example
For example, a simple test that defines two phases, one that uploads bookmarks and another that downloads them can be described as follows, see the section on [Test Format](#test-format) for details on the format of the tests.
```js
EnableEngines(["bookmarks"]); // Enables the bookmark engine across all profiles(phases) that this test runs

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in JSON format as it will get parsed by the Python
 * testrunner. It is parsed by the YAML package, so it relatively flexible.
 */
var phases = {
  phase1: "profile1",
  phase2: "profile2",
};

// the initial list of bookmarks to add to the browser
var bookmarksInitial = {
  menu: [
    { folder: "foldera" },
    { folder: "folderb" },
  ],

  "menu/foldera": [{ uri: "http://www.cnn.com", title: "CNN" }], // One bookmark in menu/foldera pointing to CNN
  "menu/folderb": [{ uri: "http://www.apple.com", title: "Apple"}], // One bookmark in menu/folderb pointing to apple
};

// Add bookmarks to profile1 and sync.
Phase("phase1", [
  [Bookmarks.add, bookmarksInitial],
  [Bookmarks.verify, bookmarksInitial],
  [Sync],
  [Bookmarks.verify, bookmarksInitial],
]);


// Sync, then verify that bookmarks added by phase 1 are present.
Phase("phase2", [
  [Sync],
  [Bookmarks.verify, bookmarksInitial],
]);
```

### The TPS Extension
When the Firefox profile representing the phase loads, it first installs an [extension](https://searchfox.org/mozilla-central/source/services/sync/tps/extensions/tps). The extension is what executes the tests by instructing Firefox and reading from Firefox to assert that Sync is working properly.

The test files execute in the extension, and the [extension defines all the functions that the test files may use](https://searchfox.org/mozilla-central/source/services/sync/tps/extensions/tps/resource/tps.sys.mjs). For instance, in the above [example](#example) the `Phase` function is defined [here](https://searchfox.org/mozilla-central/rev/1f27a4022f9f1269d897526c1c892a57743e650c/services/sync/tps/extensions/tps/resource/tps.sys.mjs#1234)

## Test Format
### Test group configuration
The tests are referenced by a `json` file that references all the tests TPS will run. By default TPS will run the configuration in [`services/sync/tests/tps/all_tests.json`](https://searchfox.org/mozilla-central/source/services/sync/tests/tps/all_tests.json).
The test group configuration is a `json` object with one key named `tests` that is itself a `json` object. The `tests` object has a key for every test to run, and the key should be the name of the [test file](#test-files). The value for each test file is the empty object `{}` if the test should run, or
`{"disabled": "Bug <BUG NUMBER>"}` where `<BUG NUMBER>` is a Bugzilla bug number referencing why the test was disabled.


### Test Files
#### The phases object
Test Files are JavaScript files that will be run by the TPS extension once the Firefox profile is loaded. However, before that is done the TPS framework will load the first object defined in test file as `yaml`. In other words, for the following example:
```js
var phases = {
    phase1: "profile1",
    phase2: "profile2",
    phase3: "profile1", // phase 1 and 3 reuse the same profile, "profile1"
}

// ... rest of the test file
```
The inside of the curly brackets will be parsed as yaml to identify the profiles that TPS will run and map each phase to a profile. In the example above, `phase1` and `phase3` reuse the same profile, but `phase2` uses it's own profile. The rest of the file is regular JavaScript that will be loaded in the [TPS Extension](#the-tps-extension).
#### Test File Capabilities
The TPS Extension exports a set of functions and objects that test files may use. See the [`tps.sys.mjs`](https://searchfox.org/mozilla-central/source/services/sync/tps/extensions/tps/resource/tps.sys.mjs) for up-to-date details, the following is the list of export capabilities as of April 10th, 2024:
##### Enabling Sync Engines
To enable sync engines, which you should do before executing a test that depends on an engine, use the `EnableEngines` function. The function takes an array of strings representing the engine names. Eg:
```js
EnableEngines(["bookmarks", "history"]);
```
##### Start a phase
Phases are run on their assigned profiles profiles in the order they are declared in [the phases object](#the-phases-object). To define what a phase does use the `Phase` function that takes in a phase name as the first argument - **this should be the same name defined in the [phases object](#the-phases-object)** - and a 2D array of actions as a second argument. Each action array in the 2D is run in sequence. The inner array defines what the action is, and any arguments to that action. For example:
```js
Phase("phase1", [[Sync], [Bookmarks.add, bookmarkInitial]]);
```

Note that the example assumes that the phases object and `bookmarkInitial` are defined in test file.

In the above example:
- `"phase1"` is the name of the phase, which should match exactly the phase name as defined in [the phases object](#the-phases-object).
- The 2D array passed as the second argument has:
  - `[Sync]` as its first array which means that the phase will first Sync
  - `[Bookmarks.add, bookmarkInitial]` as its second member which means that after the Sync, a Bookmarks.add operation will run, and it will add the `bookmarkInitial`.

##### Actions
Actions are run inside phases as arguments to the [`Phase`](#start-a-phase) function.
###### General Actions
There are few general actions that aren't tied to a data type:
- `Sync`: Will trigger a sync
- `Login`: Logs in to Mozilla Account. **You shouldn't need to do this in most cases as it is done automatically**
- `WipeServer`: Will wipe all data from the server
- `EnsureTracking`: Will wait until sync tracking has started. **You shouldn't need to do this in most cases**

###### Data Type specific Actions

Some actions are supported for individual data types. Those actions are triggered by using `<DataType>.<Action>`. The following is a list of all possible `<DataType>` values:
For more details on which actions are supported for which data type, see the following sections.
- `Bookmarks`
- `Tabs`
- `Formdata`
- `History`
- `Passwords`
- `Addons`
- `Addresses`
- `CreditCards`
- `ExtStorage`
- `Prefs`
- `Windows`

**Bookmarks Actions**


Example in [test_existing_bookmarks.js](https://searchfox.org/mozilla-central/rev/1f27a4022f9f1269d897526c1c892a57743e650c/services/sync/tests/tps/test_existing_bookmarks.js)
- `add`: To add a bookmark tree
- `modify`: To modify the existing bookmark tree
- `delete`: To remove a bookmark node from the tree
- `verify`: To verify the bookmark tree matches exactly the tree given, otherwise fails the phase
- `verifyNot`: The inverse of verify, fails the phase if the bookmark tree matches the given tree
- `skipValidation`: To tell Firefox to stop validating bookmark trees.


**Addresses Actions**


Example in [test_addresses.js](https://searchfox.org/mozilla-central/rev/1f27a4022f9f1269d897526c1c892a57743e650c/services/sync/tests/tps/test_addresses.js)
- `add`: Adds an array of addresses
- `modify`: Modifies the list of addresses to what was given
- `delete`: Deletes the given addresses
- `verify`: Verifies that the addresses match exactly the given list, otherwise fails the phase
- `verifyNot`: The inverse of verify, fails the phase if the list of addresses matches the given list.

**CreditCard Actions**


Example in [test_creditcards.js](https://searchfox.org/mozilla-central/rev/1f27a4022f9f1269d897526c1c892a57743e650c/services/sync/tests/tps/test_creditcards.js)
- `add`: Adds an array of credit cards
- `modify`: Modifies the list of credit cards to what was given
- `delete`: Deletes the given credit cards
- `verify`: Verifies that the credit cards match exactly the given list, otherwise fails the phase
- `verifyNot`: The inverse of verify, fails the phase if the list of credit cards matches the given list.

**Addons Actions**


Example in [test_addon_reconciling.js](https://searchfox.org/mozilla-central/rev/1f27a4022f9f1269d897526c1c892a57743e650c/services/sync/tests/tps/test_addon_reconciling.js)
- `installs`: installs a list of addons
- `setEnabled`: Enables or disables a list of addons
- `uninstall`: Uninstalls a list of addons
- `verify`: Verifies that the addons match exactly the given list, otherwise fails the phase
- `verifyNot`: The inverse of verify, fails the phase if the list of addons matches the given list.
- `skipValidation`: Tells Firefox to stop validating Addons.

**Formdata Actions**


Example in [test_formdata.js](https://searchfox.org/mozilla-central/rev/1f27a4022f9f1269d897526c1c892a57743e650c/services/sync/tests/tps/test_formdata.js)
- `add`: Adds an array of form data
- `delete`: Deletes the given form data
- `verify`: Verifies that form data match exactly the given list, otherwise fails the phase
- `verifyNot`: The inverse of verify, fails the phase if the form data matches the given list.

**History Actions**


Example in [test_history.js](https://searchfox.org/mozilla-central/rev/1f27a4022f9f1269d897526c1c892a57743e650c/services/sync/tests/tps/test_history.js)
- `add`: Adds an array of history items
- `delete`: Deletes the given history items
- `verify`: Verifies that form data match exactly the given list, otherwise fails the phase
- `verifyNot`: The inverse of verify, fails the phase if the form data matches the given list.

**Passwords Actions**


Example in [test_passwords.js](https://searchfox.org/mozilla-central/rev/1f27a4022f9f1269d897526c1c892a57743e650c/services/sync/tests/tps/test_passwords.js)
- `add`: To add a list of logins
- `modify`: To modify the existing list of logins based on changes in the given list
- `delete`: To remove a list of logins
- `verify`: To verify the list of logins matches exactly the list given, otherwise fails the phase
- `verifyNot`: The inverse of verify, fails the phase if the logins matches the given list
- `skipValidation`: To tell Firefox to stop validating logins

**Prefs Actions**


Example in [test_prefs.js](https://searchfox.org/mozilla-central/rev/1f27a4022f9f1269d897526c1c892a57743e650c/services/sync/tests/tps/test_prefs.js)
- `modify`: To modify the existing prefs based on changes in the given list
- `verify`: To verify the values of the given prefs match the values given, otherwise fails the phase

**Tabs Actions**


Example in [test_tabs.js](https://searchfox.org/mozilla-central/rev/1f27a4022f9f1269d897526c1c892a57743e650c/services/sync/tests/tps/test_tabs.js)
- `add`: To add a new list of remote tabs, each annotated with which profile it belongs to
- `verify`: To verify the values of remote tabs matches the given list, otherwise fails the phase

**Windows Actions**


Example in [test_privbrw_tabs.js](https://searchfox.org/mozilla-central/rev/1f27a4022f9f1269d897526c1c892a57743e650c/services/sync/tests/tps/test_privbrw_tabs.js#79-84)
- `add`: Adds a new window with the given configuration

**ExtStorage Actions**


Example in [test_extstorage.js](https://searchfox.org/mozilla-central/rev/1f27a4022f9f1269d897526c1c892a57743e650c/services/sync/tests/tps/test_extstorage.js)

- `set`: Sets the value of given key to the given value
- `verify`: Verifies that the value of the given key is the given value, fails the phase otherwise.

## Writing new TPS tests
To write new TPS tests follow the following instructions:
1. Create a new `.js` file in `services/sync/tests/tps` named `test_<my_test_name>.js`
2. Link to the file in the `services/sync/tests/tps/all_tests.json`
3. Follow the format described in [Test Files](#test-files)
4. Make sure to test the file by [running TPS](#how-to-run-tps)

## How to run TPS
### Installation
TPS requires several packages to operate properly. To install TPS and
required packages, use the create_venv.py script provided in the `testing/tps` directory:
```sh
python3 create_venv.py /path/to/create/virtualenv
```

This script will create a virtalenv and install TPS into it.

You must then activate the virtualenv by executing:

-  (mac/linux):
```sh
source /path/to/virtualenv/Scripts/activate
```
-  (win):
```sh
/path/to/virtualenv/Scripts/activate.bat
```

TPS can then be run by executing:
```sh
runtps --binary=/path/to/firefox
```

> Note: You can run the tps tests in headless mode by using `MOZ_HEADLESS=1`. This will make
> your computer somewhat useable while the tests are running.

When you are done with TPS, you can deactivate the virtualenv by executing
`deactivate`

### Configuration
To edit the TPS configuration, do not edit config/config.json.in in the tree.
Instead, edit config.json inside your virtualenv; it will be located at the
top level of where you specified the virtualenv be created - eg, for the
example above, it will be `/path/to/create/virtualenv/config.json`

#### Setting Up Test Accounts

##### Mozilla Accounts
To create a test account for using the Mozilla Account authentication perform the
following steps:

> Note: Currently, the TPS tests rely on how restmail returns the verification code
> You should use restmail or something very similar.
> Gmail and other providers might give a `The request was blocked for security reasons`

1. Go to the URL: http://restmail.net/mail/%account_prefix%@restmail.net
  - Replace `%account_prefix%` with your own test name
2. Go to https://accounts.firefox.com/signup?service=sync&context=fx_desktop_v1
3. Sign in with the previous chosen email address and a password
4. Go back to the Restmail URL, reload the page
5. Search for the verification link and open that page

Now you will be able to use this account for TPS. Note that these
steps can be done in either a test profile or in a private browsing window - you
might want to avoid doing that in a "real" profile that's already connected to
Sync.
