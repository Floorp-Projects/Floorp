package org.mozilla.focus.findinpage

import android.arch.lifecycle.MutableLiveData
import org.mozilla.focus.web.IFindListener

class FindInPageCoordinator: IFindListener {
    val matches = MutableLiveData<Pair<Int, Int>>()

    override fun onFindResultReceived(activeMatchOrdinal: Int, numberOfMatches: Int, isDoneCounting: Boolean) {
        matches.postValue(Pair(activeMatchOrdinal, numberOfMatches))
    }
}
