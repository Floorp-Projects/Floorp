/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.session

/**
 * A tiny wrapper around MutableList that allows observing adding, removing and selecting items.
 */
class ObservableList<T>(
    initialValues: List<T> = emptyList(),
    private var selectedIndex: Int = NO_SELECTION
) {
    private val values = initialValues.toMutableList()
    private var observers = mutableListOf<Observer<T>>()

    /**
     * Returns the size of the collection.
     */
    val size: Int
        get() = values.size

    /**
     * Returns the currently selected item.
     */
    val selected: T
        get() = values[selectedIndex]

    /**
     * Register an observer that will be notified if the list changes.
     */
    @Synchronized
    fun register(observer: Observer<T>) {
        observers.add(observer)
    }

    /**
     * Unregister observer.
     */
    @Synchronized
    fun unregister(observer: Observer<T>) {
        observers.remove(observer)
    }

    /**
     * Add a new item to the list and optionally select it.
     */
    @Synchronized
    fun add(value: T, select: Boolean = false): Boolean {
        val result = values.add(value)

        observers.forEach { it.onValueAdded(value) }

        if (select) {
            select(value)
        }

        return result
    }

    /**
     * Remove an item from the list.
     */
    @Synchronized
    fun remove(value: T) {
        if (!values.remove(value)) {
            return
        }

        observers.forEach { it.onValueRemoved(value) }
    }

    /**
     * Mark the given item as selected.
     */
    @Synchronized
    fun select(value: T) {
        val index = values.indexOf(value)

        if (index == -1) {
            throw IllegalArgumentException("Value to select is not in list")
        }

        selectedIndex = index

        observers.forEach { it.onValueSelected(value) }
    }

    /**
     * Interface to be implemented by classes that want to observe the list.
     */
    interface Observer<T> {
        fun onValueAdded(value: T)
        fun onValueRemoved(value: T)
        fun onValueSelected(value: T)
    }

    companion object {
        const val NO_SELECTION = -1
    }
}
