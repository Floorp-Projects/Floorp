/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Connects multiple Data Sources with multiple View Models.
 * Aggregator owns Data Sources.
 * Aggregator weakly refers to View Models.
 */
export class Aggregator {
  #sources = [];
  #attachedViewModels = [];

  attachViewModel(viewModel) {
    // Weak reference the View Model so we do not keep it in memory forever
    this.#attachedViewModels.push(new WeakRef(viewModel));
  }

  detachViewModel(viewModel) {
    for (let i = this.#attachedViewModels.length - 1; i >= 0; i--) {
      const knownViewModel = this.#attachedViewModels[i].deref();
      if (viewModel == knownViewModel || !knownViewModel) {
        this.#attachedViewModels.splice(i, 1);
      }
    }
  }

  /**
   * Run action on each of the alive attached view models.
   * Remove dead consumers.
   *
   * @param {Function} action to perform on each alive consumer
   */
  forEachViewModel(action) {
    for (let i = this.#attachedViewModels.length - 1; i >= 0; i--) {
      const viewModel = this.#attachedViewModels[i].deref();
      if (viewModel) {
        action(viewModel);
      } else {
        this.#attachedViewModels.splice(i, 1);
      }
    }
  }

  *enumerateLines(searchText) {
    for (let source of this.#sources) {
      yield* source.enumerateLines(searchText);
    }
  }

  /**
   *
   * @param {Function} createSourceFn (aggregatorApi) used to create Data Source.
   *                   aggregatorApi is the way for Data Source to push data
   *                   to the Aggregator.
   */
  addSource(createSourceFn) {
    const api = this.#apiForDataSource();
    const source = createSourceFn(api);
    this.#sources.push(source);
  }

  /**
   * Exposes interface for a datasource to communicate with Aggregator.
   */
  #apiForDataSource() {
    const aggregator = this;
    return {
      refreshSingleLineOnScreen(line) {
        aggregator.forEachViewModel(vm => vm.refreshSingleLineOnScreen(line));
      },

      refreshAllLinesOnScreen() {
        aggregator.forEachViewModel(vm => vm.refreshAllLinesOnScreen());
      },
    };
  }
}
