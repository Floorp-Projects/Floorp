/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

/**
 * Class which represents an experiment associated data
 */
class ExperimentPayload {
    private val valuesMap = HashMap<String, Any>()

    /**
     * Puts a value into the payload
     *
     * @param key key
     * @param value value to put under the key
     */
    fun put(key: String, value: Any) {
        valuesMap[key] = value
    }

    /**
     * Gets a value from the payload
     *
     * @param key key
     *
     * @return value under the specified key
     */
    fun get(key: String): Any? {
        return valuesMap[key]
    }

    /**
     * Gets all the payload keys
     *
     * @return set of payload keys
     */
    fun getKeys(): Set<String> {
        return valuesMap.keys.toSet()
    }

    /**
     * Gets a value from the payload as a list of Boolean
     *
     * @param key key
     *
     * @return value under the specified key as a list of Boolean
     */
    @Suppress("UNCHECKED_CAST")
    fun getBooleanList(key: String): List<Boolean>? {
        return get(key) as List<Boolean>?
    }

    /**
     * Gets a value from the payload as a list of Int
     *
     * @param key key
     *
     * @return value under the specified key as a list of Int
     */
    @Suppress("UNCHECKED_CAST")
    fun getIntList(key: String): List<Int>? {
        return get(key) as List<Int>?
    }

    /**
     * Gets a value from the payload as a list of Long
     *
     * @param key key
     *
     * @return value under the specified key as a list of Long
     */
    @Suppress("UNCHECKED_CAST")
    fun getLongList(key: String): List<Long>? {
        return get(key) as List<Long>?
    }

    /**
     * Gets a value from the payload as a list of Double
     *
     * @param key key
     *
     * @return value under the specified key as a list of Double
     */
    @Suppress("UNCHECKED_CAST")
    fun getDoubleList(key: String): List<Double>? {
        return get(key) as List<Double>?
    }

    /**
     * Gets a value from the payload as a list of String
     *
     * @param key key
     *
     * @return value under the specified key as a list of String
     */
    @Suppress("UNCHECKED_CAST")
    fun getStringList(key: String): List<String>? {
        return get(key) as List<String>?
    }
}
