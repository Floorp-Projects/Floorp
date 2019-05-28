package org.mozilla.focus.findinpage

import androidx.lifecycle.MutableLiveData
import org.mozilla.focus.web.IFindListener

class FindInPageCoordinator : IFindListener {
    val matches = MutableLiveData<Pair<Int, Int>>()

    fun reset() = matches.postValue(Pair(0, 0))

    override fun onFindResultReceived(activeMatchOrdinal: Int, numberOfMatches: Int, isDoneCounting: Boolean) {
        matches.postValue(Pair(activeMatchOrdinal, numberOfMatches))
    }
}
