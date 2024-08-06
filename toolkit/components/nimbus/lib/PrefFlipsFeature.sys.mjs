/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PrefUtils: "resource://normandy/lib/PrefUtils.sys.mjs",
});

const FEATURE_ID = "prefFlips";

export class PrefFlipsFeature {
  #updating;

  static get FEATURE_ID() {
    return FEATURE_ID;
  }

  constructor({ manager }) {
    this.manager = manager;
    this._prefs = new Map();

    this.#updating = false;
  }

  onFeatureUpdate() {
    if (this.#updating) {
      return;
    }

    this.#updating = true;

    const activeEnrollment =
      this.manager.store.getExperimentForFeature(PrefFlipsFeature.FEATURE_ID) ??
      this.manager.store.getRolloutForFeature(PrefFlipsFeature.FEATURE_ID);

    const prefs = lazy.NimbusFeatures[FEATURE_ID].getVariable("prefs") ?? {};

    for (const [pref, details] of this._prefs.entries()) {
      if (Object.hasOwn(prefs, pref)) {
        // The pref may have changed.
        const newDetails = prefs[pref];

        if (
          newDetails.branch !== details.branch ||
          newDetails.value !== details.value
        ) {
          this._updatePref(pref, newDetails);
        }
      } else {
        // The pref is no longer controlled by us.
        this._unregisterPref(pref);
      }
    }

    for (const [pref, { branch, value }] of Object.entries(prefs)) {
      const known = this._prefs.get(pref);
      if (known) {
        // We have already processed this pref.
        continue;
      }

      const setPref = this.manager._prefs.get(pref);
      if (setPref) {
        const toUnenroll = Array.from(setPref.slugs.values()).map(slug =>
          this.manager.store.get(slug)
        );

        if (toUnenroll.length === 2 && !toUnenroll[0].isRollout) {
          toUnenroll.reverse();
        }

        for (const enrollment of toUnenroll) {
          this.manager._unenroll(enrollment, {
            reason: "prefFlips-conflict",
            conflictingSlug: activeEnrollment.slug,
          });
        }
      }

      this._registerPref(pref, { branch, value });
    }

    this.#updating = false;
  }

  _registerPref(pref, { branch, value }) {
    const observer = (_aSubject, _aTopic, aData) => {
      // This observer will be called for changes to `name` as well as any
      // other pref that begins with `name.`, so we have to filter to
      // exactly the pref we care about.
      if (aData === pref) {
        this._onPrefChanged(pref);
      }
    };

    // If we *just* unenrolled a setPref experiment for this pref on the default
    // branch, the pref will only be correctly restored if the pref had a value
    // on the default branch. Otherwise, it will be left as-is until restart.
    // This may result in us computing an incorrect originalValue, but (a) we
    // couldn't correct the problem even if we recorded the correct (i.e., null)
    // value and (b) the issue will resolve itself at next startup. This is
    // consistent with how setPref experiments work.
    const entry = {
      branch,
      originalValue: lazy.PrefUtils.getPref(pref, { branch }),
      value: value ?? null,
      observer,
    };

    lazy.PrefUtils.setPref(pref, value ?? null, { branch });
    Services.prefs.addObserver(pref, observer);
    this._prefs.set(pref, entry);
  }

  _updatePref(pref, { branch, value }) {
    const entry = this._prefs.get(pref);
    if (!entry) {
      return;
    }

    Services.prefs.removeObserver(pref, entry.observer);

    let originalValue = entry.originalValue;
    if (entry.branch !== branch) {
      // Restore the value on the previous branch.
      lazy.PrefUtils.setPref(pref, entry.originalValue, {
        branch: entry.branch,
      });

      originalValue = lazy.PrefUtils.getPref(pref, { branch });
    }

    Object.assign(entry, {
      branch,
      value,
      originalValue,
    });

    lazy.PrefUtils.setPref(pref, value, { branch });
    Services.prefs.addObserver(pref, entry.observer);
  }

  _unregisterPref(pref) {
    const entry = this._prefs.get(pref);
    if (!entry) {
      return;
    }

    this._prefs.delete(pref);
    Services.prefs.removeObserver(pref, entry.observer);

    const { originalValue, branch } = entry;
    lazy.PrefUtils.setPref(pref, originalValue, { branch });
  }

  _onPrefChanged(pref) {
    if (this.#updating) {
      return;
    }

    if (this.manager._prefs.get(pref)?.enrollmentChanging) {
      return;
    }

    this.#updating = true;

    const entry = this._prefs.get(pref);
    if (!entry) {
      return;
    }

    this._prefs.delete(pref);
    Services.prefs.removeObserver(pref, entry.observer);

    const changedPref = {
      name: pref,
      branch: PrefFlipsFeature.determinePrefChangeBranch(
        pref,
        entry.branch,
        entry.value
      ),
    };

    // If there is both an experiment and a rollout that would both control the
    // same pref, we unenroll both because if we only unenrolled the experiment,
    // the rollout would clobber the pref change that just happened.
    const toUnenroll = this.manager.store.getAll().filter(enrollment => {
      if (!enrollment.active || !enrollment.featureIds.includes(FEATURE_ID)) {
        return false;
      }

      const featureValue = enrollment.branch.features.find(
        featureConfig => featureConfig.featureId === FEATURE_ID
      ).value;
      return Object.hasOwn(featureValue.prefs, pref);
    });

    // We have to restore every *other* pref controlled by these enrollments.
    const toRestore = new Set(
      toUnenroll.flatMap(enrollment =>
        Object.keys(
          enrollment.branch.features.find(
            featureConfig => featureConfig.featureId === FEATURE_ID
          ).value.prefs
        )
      )
    );
    toRestore.delete(pref);

    for (const prefToRestore of toRestore) {
      this._unregisterPref(prefToRestore);
    }

    // Unenrollment doesn't matter here like it does in ExperimentManager's
    // managed prefs because we've already restored prefs before unenrollment.
    for (const enrollment of toUnenroll) {
      this.manager._unenroll(enrollment, {
        reason: "changed-pref",
        changedPref,
      });
    }

    this.#updating = false;

    // If we've caused unenrollments, we need to recompute state.
    this.onFeatureUpdate();
  }

  static determinePrefChangeBranch(pref, expectedBranch, expectedValue) {
    // We want to know what branch was changed so we can know if we should
    // restore prefs (.e.,g if we have a pref set on the user branch and the
    // user branch changed, we do not want to then overwrite the user's choice).

    // This is not complicated if a pref simply changed. However, we must also
    // detect `nsIPrefBranch::clearUserPref()`, which wipes out the user branch
    // and leaves the default branch untouched. That is where this gets
    // complicated.

    if (Services.prefs.prefHasUserValue(pref)) {
      // If there is a user branch value, then the user branch changed, because
      // a change to the default branch wouldn't have triggered the observer.
      return "user";
    } else if (!Services.prefs.prefHasDefaultValue(pref)) {
      // If there is no user branch value *or* default branch avlue, then the
      // user branch must have been cleared because you cannot clear the default
      // branch.
      return "user";
    } else if (expectedBranch === "default") {
      const value = lazy.PrefUtils.getPref(pref, { branch: "default" });
      if (value === expectedValue) {
        // The pref we control was set on the default branch and still matches
        // the expected value. Therefore, the user branch must have been
        // cleared.
        return "user";
      }
      // The default value branch does not match the value we expect, so it
      // must have just changed.
      return "default";
    }
    return "user";
  }
}
