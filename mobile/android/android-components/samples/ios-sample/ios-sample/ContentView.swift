/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import SwiftUI
import dummy

struct ContentView: View {
    var body: some View {
        let delegate = DummyDelegate()
        let dummy = Dummy(delegate: delegate)
        
        Text(dummy.getValue())
            .padding()
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
