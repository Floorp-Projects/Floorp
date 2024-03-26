/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Aggregator } from "resource://gre/modules/megalist/aggregator/Aggregator.sys.mjs";
import { AddressesDataSource } from "resource://gre/modules/megalist/aggregator/datasources/AddressesDataSource.sys.mjs";
import { BankCardDataSource } from "resource://gre/modules/megalist/aggregator/datasources/BankCardDataSource.sys.mjs";
import { LoginDataSource } from "resource://gre/modules/megalist/aggregator/datasources/LoginDataSource.sys.mjs";

export class DefaultAggregator extends Aggregator {
  constructor() {
    super();
    this.addSource(aggregatorApi => new AddressesDataSource(aggregatorApi));
    this.addSource(aggregatorApi => new BankCardDataSource(aggregatorApi));
    this.addSource(aggregatorApi => new LoginDataSource(aggregatorApi));
  }
}
