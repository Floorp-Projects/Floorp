/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.containers

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.paging.DataSource
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import mozilla.components.feature.containers.adapter.ContainerAdapter
import mozilla.components.feature.containers.db.ContainerDatabase
import mozilla.components.feature.containers.db.ContainerEntity
import java.util.UUID

/**
 * A storage implementation for organizing containers (contextual identities).
 */
class ContainerStorage(context: Context) {

    @VisibleForTesting
    internal var database: Lazy<ContainerDatabase> =
        lazy { ContainerDatabase.get(context) }
    private val containerDao by lazy { database.value.containerDao() }

    /**
     * Adds a new [Container].
     */
    suspend fun addContainer(name: String, color: String, icon: String) {
        database.value.containerDao().insertContainer(
            ContainerEntity(
                contextId = UUID.randomUUID().toString(),
                name = name,
                color = color,
                icon = icon
            )
        )
    }

    /**
     * Returns a [Flow] list of all the [Container] instances.
     */
    fun getContainers(): Flow<List<Container>> {
        return containerDao.getContainers().map { list ->
            list.map { entity -> ContainerAdapter(entity) }
        }
    }

    /**
     * Returns all saved [Container] instances as a [DataSource.Factory].
     */
    fun getContainersPaged(): DataSource.Factory<Int, Container> = containerDao
        .getContainersPaged()
        .map { entity ->
            ContainerAdapter(
                entity
            )
        }

    /**
     * Removes the given [Container].
     */
    suspend fun removeContainer(container: Container) {
        val containerEntity = (container as ContainerAdapter).entity
        containerDao.deleteContainer(containerEntity)
    }
}
