/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Preferences } from "resource://gre/modules/Preferences.sys.mjs";
import { AddonStudies } from "resource://normandy/lib/AddonStudies.sys.mjs";
import { RecipeRunner } from "resource://normandy/lib/RecipeRunner.sys.mjs";
import { sinon } from "resource://testing-common/Sinon.sys.mjs";

const FIXTURE_ADDON_ID = "normandydriver-a@example.com";
const UUID_REGEX =
  /[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}/i;

// Factory IDs
let _addonStudyFactoryId = 0;
let _preferenceStudyFactoryId = 0;
let _preferenceRolloutFactoryId = 0;

let testGlobals = {};

const preferenceBranches = {
  user: Preferences,
  default: new Preferences({ defaultBranch: true }),
};

export const NormandyTestUtils = {
  init({ add_task, Assert } = {}) {
    testGlobals.add_task = add_task;
    testGlobals.Assert = Assert;
  },

  factories: {
    addonStudyFactory(attrs = {}) {
      for (const key of ["name", "description"]) {
        if (attrs && attrs[key]) {
          throw new Error(
            `${key} is no longer a valid key for addon studies, please update to v2 study schema`
          );
        }
      }

      // Generate a slug from userFacingName
      let recipeId = _addonStudyFactoryId++;
      let { userFacingName = `Test study ${recipeId}`, slug } = attrs;
      delete attrs.slug;
      if (userFacingName && !slug) {
        slug = userFacingName.replace(" ", "-").toLowerCase();
      }

      return Object.assign(
        {
          recipeId,
          slug,
          userFacingName: "Test study",
          userFacingDescription: "test description",
          branch: AddonStudies.NO_BRANCHES_MARKER,
          active: true,
          addonId: FIXTURE_ADDON_ID,
          addonUrl: "http://test/addon.xpi",
          addonVersion: "1.0.0",
          studyStartDate: new Date(),
          studyEndDate: null,
          extensionApiId: 1,
          extensionHash:
            "ade1c14196ec4fe0aa0a6ba40ac433d7c8d1ec985581a8a94d43dc58991b5171",
          extensionHashAlgorithm: "sha256",
          temporaryErrorDeadline: null,
        },
        attrs
      );
    },

    branchedAddonStudyFactory(attrs = {}) {
      return NormandyTestUtils.factories.addonStudyFactory(
        Object.assign(
          {
            branch: "a",
          },
          attrs
        )
      );
    },

    preferenceStudyFactory(attrs = {}) {
      const defaultPref = {
        "test.study": {},
      };
      const defaultPrefInfo = {
        preferenceValue: false,
        preferenceType: "boolean",
        previousPreferenceValue: null,
        preferenceBranchType: "default",
        overridden: false,
      };
      const preferences = {};
      for (const [prefName, prefInfo] of Object.entries(
        attrs.preferences || defaultPref
      )) {
        preferences[prefName] = { ...defaultPrefInfo, ...prefInfo };
      }

      // Generate a slug from userFacingName
      let {
        userFacingName = `Test study ${_preferenceStudyFactoryId++}`,
        slug,
      } = attrs;
      delete attrs.slug;
      if (userFacingName && !slug) {
        slug = userFacingName.replace(" ", "-").toLowerCase();
      }

      return Object.assign(
        {
          userFacingName,
          userFacingDescription: `${userFacingName} description`,
          slug,
          branch: "control",
          expired: false,
          lastSeen: new Date().toJSON(),
          experimentType: "exp",
          actionName: "PreferenceExperimentAction",
        },
        attrs,
        {
          preferences,
        }
      );
    },

    preferenceRolloutFactory(attrs = {}) {
      const defaultPrefInfo = {
        preferenceName: "test.rollout.{}",
        value: true,
        previousValue: false,
      };
      const preferences = (attrs.preferences ?? [{}]).map((override, idx) => ({
        ...defaultPrefInfo,
        preferenceName: defaultPrefInfo.preferenceName.replace(
          "{}",
          (idx + 1).toString()
        ),
        ...override,
      }));

      return Object.assign(
        {
          slug: `test-rollout-${_preferenceRolloutFactoryId++}`,
          state: "active",
        },
        attrs,
        {
          preferences,
        }
      );
    },
  },

  /**
   * Combine a list of functions right to left. The rightmost function is passed
   * to the preceding function as the argument; the result of this is passed to
   * the next function until all are exhausted. For example, this:
   *
   * decorate(func1, func2, func3);
   *
   * is equivalent to this:
   *
   * func1(func2(func3));
   */
  decorate(...args) {
    const funcs = Array.from(args);
    let decorated = funcs.pop();
    const origName = decorated.name;
    funcs.reverse();
    for (const func of funcs) {
      decorated = func(decorated);
    }
    Object.defineProperty(decorated, "name", { value: origName });
    return decorated;
  },

  /**
   * Wrapper around add_task for declaring tests that use several with-style
   * wrappers. The last argument should be your test function; all other arguments
   * should be functions that accept a single test function argument.
   *
   * The arguments are combined using decorate and passed to add_task as a single
   * test function.
   *
   * @param {[Function]} args
   * @example
   *   decorate_task(
   *     withMockPreferences(),
   *     withMockNormandyApi(),
   *     async function myTest(mockPreferences, mockApi) {
   *       // Do a test
   *     }
   *   );
   */
  decorate_task(...args) {
    return testGlobals.add_task(NormandyTestUtils.decorate(...args));
  },

  isUuid(s) {
    return UUID_REGEX.test(s);
  },

  withMockRecipeCollection(recipes = []) {
    return function wrapper(testFunc) {
      return async function inner(args) {
        let recipeIds = new Set();
        for (const recipe of recipes) {
          if (!recipe.id || recipeIds.has(recipe.id)) {
            throw new Error(
              "To use withMockRecipeCollection each recipe must have a unique ID"
            );
          }
          recipeIds.add(recipe.id);
        }

        let db = await RecipeRunner._remoteSettingsClientForTesting.db;
        await db.clear();
        const fakeSig = { signature: "abc" };

        for (const recipe of recipes) {
          await db.create({
            id: `recipe-${recipe.id}`,
            recipe,
            signature: fakeSig,
          });
        }

        // last modified needs to be some positive integer
        let lastModified = await db.getLastModified();
        await db.importChanges({}, lastModified + 1);

        const mockRecipeCollection = {
          async addRecipes(newRecipes) {
            for (const recipe of newRecipes) {
              if (!recipe.id || recipeIds.has(recipe)) {
                throw new Error(
                  "To use withMockRecipeCollection each recipe must have a unique ID"
                );
              }
            }
            db = await RecipeRunner._remoteSettingsClientForTesting.db;
            for (const recipe of newRecipes) {
              recipeIds.add(recipe.id);
              await db.create({
                id: `recipe-${recipe.id}`,
                recipe,
                signature: fakeSig,
              });
            }
            lastModified = (await db.getLastModified()) || 0;
            await db.importChanges({}, lastModified + 1);
          },
        };

        try {
          await testFunc({ ...args, mockRecipeCollection });
        } finally {
          db = await RecipeRunner._remoteSettingsClientForTesting.db;
          await db.clear();
          lastModified = await db.getLastModified();
          await db.importChanges({}, lastModified + 1);
        }
      };
    };
  },

  MockPreferences: class {
    constructor() {
      this.oldValues = { user: {}, default: {} };
    }

    set(name, value, branch = "user") {
      this.preserve(name, branch);
      preferenceBranches[branch].set(name, value);
    }

    preserve(name, branch) {
      if (!(name in this.oldValues[branch])) {
        this.oldValues[branch][name] = preferenceBranches[branch].get(
          name,
          undefined
        );
      }
    }

    cleanup() {
      for (const [branchName, values] of Object.entries(this.oldValues)) {
        const preferenceBranch = preferenceBranches[branchName];
        for (const [name, value] of Object.entries(values)) {
          if (value !== undefined) {
            preferenceBranch.set(name, value);
          } else {
            preferenceBranch.reset(name);
          }
        }
      }
    }
  },

  withMockPreferences() {
    return function (testFunction) {
      return async function inner(args) {
        const mockPreferences = new NormandyTestUtils.MockPreferences();
        try {
          await testFunction({ ...args, mockPreferences });
        } finally {
          mockPreferences.cleanup();
        }
      };
    };
  },

  withStub(object, method, { returnValue, as = `${method}Stub` } = {}) {
    return function wrapper(testFunction) {
      return async function wrappedTestFunction(args) {
        const stub = sinon.stub(object, method);
        if (returnValue) {
          stub.returns(returnValue);
        }
        try {
          await testFunction({ ...args, [as]: stub });
        } finally {
          stub.restore();
        }
      };
    };
  },

  withSpy(object, method, { as = `${method}Spy` } = {}) {
    return function wrapper(testFunction) {
      return async function wrappedTestFunction(args) {
        const spy = sinon.spy(object, method);
        try {
          await testFunction({ ...args, [as]: spy });
        } finally {
          spy.restore();
        }
      };
    };
  },
};
