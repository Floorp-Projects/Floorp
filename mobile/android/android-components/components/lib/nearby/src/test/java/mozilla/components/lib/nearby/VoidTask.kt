/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.nearby

import android.app.Activity
import com.google.android.gms.tasks.Continuation
import com.google.android.gms.tasks.OnCompleteListener
import com.google.android.gms.tasks.OnFailureListener
import com.google.android.gms.tasks.OnSuccessListener
import com.google.android.gms.tasks.Task
import java.util.concurrent.Executor

/**
 * Trivial [Task] that completes immediately and does not return a value. The methods
 * [continueWith] and [continueWithTask] are mocks that throw exceptions when called.
 */
class VoidTask(private val success: Boolean) : Task<Void>() {
    override fun isComplete(): Boolean {
        return true
    }

    override fun isSuccessful(): Boolean {
        return success
    }

    override fun isCanceled(): Boolean {
        return false
    }

    override fun getResult(): Void? {
        if (success) {
            return null
        }
        throw RuntimeException()
    }

    override fun getException(): Exception? {
        return if (success) null else RuntimeException()
    }

    override fun addOnCompleteListener(onCompleteListener: OnCompleteListener<Void>): Task<Void> {
        onCompleteListener.onComplete(this)
        return this
    }

    override fun addOnSuccessListener(onSuccessListener: OnSuccessListener<in Void>): Task<Void> {
        if (success) {
            onSuccessListener.onSuccess(result)
        }
        return this
    }

    override fun addOnSuccessListener(
        executor: Executor,
        onSuccessListener: OnSuccessListener<in Void>
    ): Task<Void> {
        return addOnSuccessListener(onSuccessListener)
    }

    override fun addOnSuccessListener(
        activity: Activity,
        onSuccessListener: OnSuccessListener<in Void>
    ): Task<Void> {
        return addOnSuccessListener(onSuccessListener)
    }

    override fun addOnFailureListener(onFailureListener: OnFailureListener): Task<Void> {
        if (!success) {
            onFailureListener.onFailure(RuntimeException())
        }
        return this
    }

    override fun addOnFailureListener(
        executor: Executor,
        onFailureListener: OnFailureListener
    ): Task<Void> {
        throw RuntimeException("Method not implemented")
    }

    override fun addOnFailureListener(
        activity: Activity,
        onFailureListener: OnFailureListener
    ): Task<Void> {
        return addOnFailureListener(onFailureListener)
    }

    override fun <TContinuationResult> continueWith(
        continuation: Continuation<Void, TContinuationResult>
    ): Task<TContinuationResult> {
        throw RuntimeException("Method not implemented")
    }

    override fun <TContinuationResult> continueWithTask(
        continuation: Continuation<Void, Task<TContinuationResult>>
    ): Task<TContinuationResult> {
        throw RuntimeException("Method not implemented")
    }

    override fun <X : Throwable?> getResult(p0: Class<X>): Void? {
        if (success) {
            return result
        }
        throw RuntimeException()
    }

    companion object {
        val SUCCESS = VoidTask(true)
        val FAILURE = VoidTask(false)
    }
}