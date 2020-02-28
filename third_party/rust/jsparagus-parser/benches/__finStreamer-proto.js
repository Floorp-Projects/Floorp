/*eslint-disable block-scoped-var, id-length, no-control-regex, no-magic-numbers, no-prototype-builtins, no-redeclare, no-shadow, no-var, sort-vars*/
(function($protobuf) {
    var $Reader = $protobuf.Reader, $util = $protobuf.util;
    
    var $root = $protobuf.roots["default"] || ($protobuf.roots["default"] = {});
    
    $root.quotefeeder = (function() {
    
        var quotefeeder = {};
    
        quotefeeder.QuoteType = (function() {
            var valuesById = {}, values = Object.create(valuesById);
            values[valuesById[0] = "NONE"] = 0;
            values[valuesById[5] = "ALTSYMBOL"] = 5;
            values[valuesById[7] = "HEARTBEAT"] = 7;
            values[valuesById[8] = "EQUITY"] = 8;
            values[valuesById[9] = "INDEX"] = 9;
            values[valuesById[11] = "MUTUALFUND"] = 11;
            values[valuesById[12] = "MONEYMARKET"] = 12;
            values[valuesById[13] = "OPTION"] = 13;
            values[valuesById[14] = "CURRENCY"] = 14;
            values[valuesById[15] = "WARRANT"] = 15;
            values[valuesById[17] = "BOND"] = 17;
            values[valuesById[18] = "FUTURE"] = 18;
            values[valuesById[20] = "ETF"] = 20;
            values[valuesById[23] = "COMMODITY"] = 23;
            values[valuesById[28] = "ECNQUOTE"] = 28;
            values[valuesById[41] = "CRYPTOCURRENCY"] = 41;
            values[valuesById[42] = "INDICATOR"] = 42;
            values[valuesById[1000] = "INDUSTRY"] = 1000;
            return values;
        })();
    
        quotefeeder.MarketHours = (function() {
            var valuesById = {}, values = Object.create(valuesById);
            values[valuesById[0] = "PRE_MARKET"] = 0;
            values[valuesById[1] = "REGULAR_MARKET"] = 1;
            values[valuesById[2] = "POST_MARKET"] = 2;
            values[valuesById[3] = "EXTENDED_HOURS_MARKET"] = 3;
            return values;
        })();
    
        quotefeeder.OptionType = (function() {
            var valuesById = {}, values = Object.create(valuesById);
            values[valuesById[0] = "CALL"] = 0;
            values[valuesById[1] = "PUT"] = 1;
            return values;
        })();
    
        quotefeeder.PricingData = (function() {
    
            function PricingData(p) {
                if (p)
                    for (var ks = Object.keys(p), i = 0; i < ks.length; ++i)
                        if (p[ks[i]] != null)
                            this[ks[i]] = p[ks[i]];
            }
    
            PricingData.prototype.id = "";
            PricingData.prototype.price = 0;
            PricingData.prototype.time = $util.Long ? $util.Long.fromBits(0,0,false) : 0;
            PricingData.prototype.currency = "";
            PricingData.prototype.exchange = "";
            PricingData.prototype.quoteType = 0;
            PricingData.prototype.marketHours = 0;
            PricingData.prototype.changePercent = 0;
            PricingData.prototype.dayVolume = $util.Long ? $util.Long.fromBits(0,0,false) : 0;
            PricingData.prototype.dayHigh = 0;
            PricingData.prototype.dayLow = 0;
            PricingData.prototype.change = 0;
            PricingData.prototype.shortName = "";
            PricingData.prototype.expireDate = $util.Long ? $util.Long.fromBits(0,0,false) : 0;
            PricingData.prototype.openPrice = 0;
            PricingData.prototype.previousClose = 0;
            PricingData.prototype.strikePrice = 0;
            PricingData.prototype.underlyingSymbol = "";
            PricingData.prototype.openInterest = $util.Long ? $util.Long.fromBits(0,0,false) : 0;
            PricingData.prototype.optionsType = $util.Long ? $util.Long.fromBits(0,0,false) : 0;
            PricingData.prototype.miniOption = $util.Long ? $util.Long.fromBits(0,0,false) : 0;
            PricingData.prototype.lastSize = $util.Long ? $util.Long.fromBits(0,0,false) : 0;
            PricingData.prototype.bid = 0;
            PricingData.prototype.bidSize = $util.Long ? $util.Long.fromBits(0,0,false) : 0;
            PricingData.prototype.ask = 0;
            PricingData.prototype.askSize = $util.Long ? $util.Long.fromBits(0,0,false) : 0;
            PricingData.prototype.priceHint = $util.Long ? $util.Long.fromBits(0,0,false) : 0;
            PricingData.prototype.vol_24hr = $util.Long ? $util.Long.fromBits(0,0,false) : 0;
            PricingData.prototype.volAllCurrencies = $util.Long ? $util.Long.fromBits(0,0,false) : 0;
            PricingData.prototype.fromcurrency = "";
            PricingData.prototype.lastMarket = "";
            PricingData.prototype.circulatingSupply = 0;
            PricingData.prototype.marketcap = 0;
    
            PricingData.decode = function decode(r, l) {
                if (!(r instanceof $Reader))
                    r = $Reader.create(r);
                var c = l === undefined ? r.len : r.pos + l, m = new $root.quotefeeder.PricingData();
                while (r.pos < c) {
                    var t = r.uint32();
                    switch (t >>> 3) {
                    case 1:
                        m.id = r.string();
                        break;
                    case 2:
                        m.price = r.float();
                        break;
                    case 3:
                        m.time = r.sint64();
                        break;
                    case 4:
                        m.currency = r.string();
                        break;
                    case 5:
                        m.exchange = r.string();
                        break;
                    case 6:
                        m.quoteType = r.int32();
                        break;
                    case 7:
                        m.marketHours = r.int32();
                        break;
                    case 8:
                        m.changePercent = r.float();
                        break;
                    case 9:
                        m.dayVolume = r.sint64();
                        break;
                    case 10:
                        m.dayHigh = r.float();
                        break;
                    case 11:
                        m.dayLow = r.float();
                        break;
                    case 12:
                        m.change = r.float();
                        break;
                    case 13:
                        m.shortName = r.string();
                        break;
                    case 14:
                        m.expireDate = r.sint64();
                        break;
                    case 15:
                        m.openPrice = r.float();
                        break;
                    case 16:
                        m.previousClose = r.float();
                        break;
                    case 17:
                        m.strikePrice = r.float();
                        break;
                    case 18:
                        m.underlyingSymbol = r.string();
                        break;
                    case 19:
                        m.openInterest = r.sint64();
                        break;
                    case 20:
                        m.optionsType = r.sint64();
                        break;
                    case 21:
                        m.miniOption = r.sint64();
                        break;
                    case 22:
                        m.lastSize = r.sint64();
                        break;
                    case 23:
                        m.bid = r.float();
                        break;
                    case 24:
                        m.bidSize = r.sint64();
                        break;
                    case 25:
                        m.ask = r.float();
                        break;
                    case 26:
                        m.askSize = r.sint64();
                        break;
                    case 27:
                        m.priceHint = r.sint64();
                        break;
                    case 28:
                        m.vol_24hr = r.sint64();
                        break;
                    case 29:
                        m.volAllCurrencies = r.sint64();
                        break;
                    case 30:
                        m.fromcurrency = r.string();
                        break;
                    case 31:
                        m.lastMarket = r.string();
                        break;
                    case 32:
                        m.circulatingSupply = r.double();
                        break;
                    case 33:
                        m.marketcap = r.double();
                        break;
                    default:
                        r.skipType(t & 7);
                        break;
                    }
                }
                return m;
            };
    
            PricingData.fromObject = function fromObject(d) {
                if (d instanceof $root.quotefeeder.PricingData)
                    return d;
                var m = new $root.quotefeeder.PricingData();
                if (d.id != null) {
                    m.id = String(d.id);
                }
                if (d.price != null) {
                    m.price = Number(d.price);
                }
                if (d.time != null) {
                    if ($util.Long)
                        (m.time = $util.Long.fromValue(d.time)).unsigned = false;
                    else if (typeof d.time === "string")
                        m.time = parseInt(d.time, 10);
                    else if (typeof d.time === "number")
                        m.time = d.time;
                    else if (typeof d.time === "object")
                        m.time = new $util.LongBits(d.time.low >>> 0, d.time.high >>> 0).toNumber();
                }
                if (d.currency != null) {
                    m.currency = String(d.currency);
                }
                if (d.exchange != null) {
                    m.exchange = String(d.exchange);
                }
                switch (d.quoteType) {
                case "NONE":
                case 0:
                    m.quoteType = 0;
                    break;
                case "ALTSYMBOL":
                case 5:
                    m.quoteType = 5;
                    break;
                case "HEARTBEAT":
                case 7:
                    m.quoteType = 7;
                    break;
                case "EQUITY":
                case 8:
                    m.quoteType = 8;
                    break;
                case "INDEX":
                case 9:
                    m.quoteType = 9;
                    break;
                case "MUTUALFUND":
                case 11:
                    m.quoteType = 11;
                    break;
                case "MONEYMARKET":
                case 12:
                    m.quoteType = 12;
                    break;
                case "OPTION":
                case 13:
                    m.quoteType = 13;
                    break;
                case "CURRENCY":
                case 14:
                    m.quoteType = 14;
                    break;
                case "WARRANT":
                case 15:
                    m.quoteType = 15;
                    break;
                case "BOND":
                case 17:
                    m.quoteType = 17;
                    break;
                case "FUTURE":
                case 18:
                    m.quoteType = 18;
                    break;
                case "ETF":
                case 20:
                    m.quoteType = 20;
                    break;
                case "COMMODITY":
                case 23:
                    m.quoteType = 23;
                    break;
                case "ECNQUOTE":
                case 28:
                    m.quoteType = 28;
                    break;
                case "CRYPTOCURRENCY":
                case 41:
                    m.quoteType = 41;
                    break;
                case "INDICATOR":
                case 42:
                    m.quoteType = 42;
                    break;
                case "INDUSTRY":
                case 1000:
                    m.quoteType = 1000;
                    break;
                }
                switch (d.marketHours) {
                case "PRE_MARKET":
                case 0:
                    m.marketHours = 0;
                    break;
                case "REGULAR_MARKET":
                case 1:
                    m.marketHours = 1;
                    break;
                case "POST_MARKET":
                case 2:
                    m.marketHours = 2;
                    break;
                case "EXTENDED_HOURS_MARKET":
                case 3:
                    m.marketHours = 3;
                    break;
                }
                if (d.changePercent != null) {
                    m.changePercent = Number(d.changePercent);
                }
                if (d.dayVolume != null) {
                    if ($util.Long)
                        (m.dayVolume = $util.Long.fromValue(d.dayVolume)).unsigned = false;
                    else if (typeof d.dayVolume === "string")
                        m.dayVolume = parseInt(d.dayVolume, 10);
                    else if (typeof d.dayVolume === "number")
                        m.dayVolume = d.dayVolume;
                    else if (typeof d.dayVolume === "object")
                        m.dayVolume = new $util.LongBits(d.dayVolume.low >>> 0, d.dayVolume.high >>> 0).toNumber();
                }
                if (d.dayHigh != null) {
                    m.dayHigh = Number(d.dayHigh);
                }
                if (d.dayLow != null) {
                    m.dayLow = Number(d.dayLow);
                }
                if (d.change != null) {
                    m.change = Number(d.change);
                }
                if (d.shortName != null) {
                    m.shortName = String(d.shortName);
                }
                if (d.expireDate != null) {
                    if ($util.Long)
                        (m.expireDate = $util.Long.fromValue(d.expireDate)).unsigned = false;
                    else if (typeof d.expireDate === "string")
                        m.expireDate = parseInt(d.expireDate, 10);
                    else if (typeof d.expireDate === "number")
                        m.expireDate = d.expireDate;
                    else if (typeof d.expireDate === "object")
                        m.expireDate = new $util.LongBits(d.expireDate.low >>> 0, d.expireDate.high >>> 0).toNumber();
                }
                if (d.openPrice != null) {
                    m.openPrice = Number(d.openPrice);
                }
                if (d.previousClose != null) {
                    m.previousClose = Number(d.previousClose);
                }
                if (d.strikePrice != null) {
                    m.strikePrice = Number(d.strikePrice);
                }
                if (d.underlyingSymbol != null) {
                    m.underlyingSymbol = String(d.underlyingSymbol);
                }
                if (d.openInterest != null) {
                    if ($util.Long)
                        (m.openInterest = $util.Long.fromValue(d.openInterest)).unsigned = false;
                    else if (typeof d.openInterest === "string")
                        m.openInterest = parseInt(d.openInterest, 10);
                    else if (typeof d.openInterest === "number")
                        m.openInterest = d.openInterest;
                    else if (typeof d.openInterest === "object")
                        m.openInterest = new $util.LongBits(d.openInterest.low >>> 0, d.openInterest.high >>> 0).toNumber();
                }
                if (d.optionsType != null) {
                    if ($util.Long)
                        (m.optionsType = $util.Long.fromValue(d.optionsType)).unsigned = false;
                    else if (typeof d.optionsType === "string")
                        m.optionsType = parseInt(d.optionsType, 10);
                    else if (typeof d.optionsType === "number")
                        m.optionsType = d.optionsType;
                    else if (typeof d.optionsType === "object")
                        m.optionsType = new $util.LongBits(d.optionsType.low >>> 0, d.optionsType.high >>> 0).toNumber();
                }
                if (d.miniOption != null) {
                    if ($util.Long)
                        (m.miniOption = $util.Long.fromValue(d.miniOption)).unsigned = false;
                    else if (typeof d.miniOption === "string")
                        m.miniOption = parseInt(d.miniOption, 10);
                    else if (typeof d.miniOption === "number")
                        m.miniOption = d.miniOption;
                    else if (typeof d.miniOption === "object")
                        m.miniOption = new $util.LongBits(d.miniOption.low >>> 0, d.miniOption.high >>> 0).toNumber();
                }
                if (d.lastSize != null) {
                    if ($util.Long)
                        (m.lastSize = $util.Long.fromValue(d.lastSize)).unsigned = false;
                    else if (typeof d.lastSize === "string")
                        m.lastSize = parseInt(d.lastSize, 10);
                    else if (typeof d.lastSize === "number")
                        m.lastSize = d.lastSize;
                    else if (typeof d.lastSize === "object")
                        m.lastSize = new $util.LongBits(d.lastSize.low >>> 0, d.lastSize.high >>> 0).toNumber();
                }
                if (d.bid != null) {
                    m.bid = Number(d.bid);
                }
                if (d.bidSize != null) {
                    if ($util.Long)
                        (m.bidSize = $util.Long.fromValue(d.bidSize)).unsigned = false;
                    else if (typeof d.bidSize === "string")
                        m.bidSize = parseInt(d.bidSize, 10);
                    else if (typeof d.bidSize === "number")
                        m.bidSize = d.bidSize;
                    else if (typeof d.bidSize === "object")
                        m.bidSize = new $util.LongBits(d.bidSize.low >>> 0, d.bidSize.high >>> 0).toNumber();
                }
                if (d.ask != null) {
                    m.ask = Number(d.ask);
                }
                if (d.askSize != null) {
                    if ($util.Long)
                        (m.askSize = $util.Long.fromValue(d.askSize)).unsigned = false;
                    else if (typeof d.askSize === "string")
                        m.askSize = parseInt(d.askSize, 10);
                    else if (typeof d.askSize === "number")
                        m.askSize = d.askSize;
                    else if (typeof d.askSize === "object")
                        m.askSize = new $util.LongBits(d.askSize.low >>> 0, d.askSize.high >>> 0).toNumber();
                }
                if (d.priceHint != null) {
                    if ($util.Long)
                        (m.priceHint = $util.Long.fromValue(d.priceHint)).unsigned = false;
                    else if (typeof d.priceHint === "string")
                        m.priceHint = parseInt(d.priceHint, 10);
                    else if (typeof d.priceHint === "number")
                        m.priceHint = d.priceHint;
                    else if (typeof d.priceHint === "object")
                        m.priceHint = new $util.LongBits(d.priceHint.low >>> 0, d.priceHint.high >>> 0).toNumber();
                }
                if (d.vol_24hr != null) {
                    if ($util.Long)
                        (m.vol_24hr = $util.Long.fromValue(d.vol_24hr)).unsigned = false;
                    else if (typeof d.vol_24hr === "string")
                        m.vol_24hr = parseInt(d.vol_24hr, 10);
                    else if (typeof d.vol_24hr === "number")
                        m.vol_24hr = d.vol_24hr;
                    else if (typeof d.vol_24hr === "object")
                        m.vol_24hr = new $util.LongBits(d.vol_24hr.low >>> 0, d.vol_24hr.high >>> 0).toNumber();
                }
                if (d.volAllCurrencies != null) {
                    if ($util.Long)
                        (m.volAllCurrencies = $util.Long.fromValue(d.volAllCurrencies)).unsigned = false;
                    else if (typeof d.volAllCurrencies === "string")
                        m.volAllCurrencies = parseInt(d.volAllCurrencies, 10);
                    else if (typeof d.volAllCurrencies === "number")
                        m.volAllCurrencies = d.volAllCurrencies;
                    else if (typeof d.volAllCurrencies === "object")
                        m.volAllCurrencies = new $util.LongBits(d.volAllCurrencies.low >>> 0, d.volAllCurrencies.high >>> 0).toNumber();
                }
                if (d.fromcurrency != null) {
                    m.fromcurrency = String(d.fromcurrency);
                }
                if (d.lastMarket != null) {
                    m.lastMarket = String(d.lastMarket);
                }
                if (d.circulatingSupply != null) {
                    m.circulatingSupply = Number(d.circulatingSupply);
                }
                if (d.marketcap != null) {
                    m.marketcap = Number(d.marketcap);
                }
                return m;
            };
    
            PricingData.toObject = function toObject(m, o) {
                if (!o)
                    o = {};
                var d = {};
                if (o.defaults) {
                    d.id = "";
                    d.price = 0;
                    if ($util.Long) {
                        var n = new $util.Long(0, 0, false);
                        d.time = o.longs === String ? n.toString() : o.longs === Number ? n.toNumber() : n;
                    } else
                        d.time = o.longs === String ? "0" : 0;
                    d.currency = "";
                    d.exchange = "";
                    d.quoteType = o.enums === String ? "NONE" : 0;
                    d.marketHours = o.enums === String ? "PRE_MARKET" : 0;
                    d.changePercent = 0;
                    if ($util.Long) {
                        var n = new $util.Long(0, 0, false);
                        d.dayVolume = o.longs === String ? n.toString() : o.longs === Number ? n.toNumber() : n;
                    } else
                        d.dayVolume = o.longs === String ? "0" : 0;
                    d.dayHigh = 0;
                    d.dayLow = 0;
                    d.change = 0;
                    d.shortName = "";
                    if ($util.Long) {
                        var n = new $util.Long(0, 0, false);
                        d.expireDate = o.longs === String ? n.toString() : o.longs === Number ? n.toNumber() : n;
                    } else
                        d.expireDate = o.longs === String ? "0" : 0;
                    d.openPrice = 0;
                    d.previousClose = 0;
                    d.strikePrice = 0;
                    d.underlyingSymbol = "";
                    if ($util.Long) {
                        var n = new $util.Long(0, 0, false);
                        d.openInterest = o.longs === String ? n.toString() : o.longs === Number ? n.toNumber() : n;
                    } else
                        d.openInterest = o.longs === String ? "0" : 0;
                    if ($util.Long) {
                        var n = new $util.Long(0, 0, false);
                        d.optionsType = o.longs === String ? n.toString() : o.longs === Number ? n.toNumber() : n;
                    } else
                        d.optionsType = o.longs === String ? "0" : 0;
                    if ($util.Long) {
                        var n = new $util.Long(0, 0, false);
                        d.miniOption = o.longs === String ? n.toString() : o.longs === Number ? n.toNumber() : n;
                    } else
                        d.miniOption = o.longs === String ? "0" : 0;
                    if ($util.Long) {
                        var n = new $util.Long(0, 0, false);
                        d.lastSize = o.longs === String ? n.toString() : o.longs === Number ? n.toNumber() : n;
                    } else
                        d.lastSize = o.longs === String ? "0" : 0;
                    d.bid = 0;
                    if ($util.Long) {
                        var n = new $util.Long(0, 0, false);
                        d.bidSize = o.longs === String ? n.toString() : o.longs === Number ? n.toNumber() : n;
                    } else
                        d.bidSize = o.longs === String ? "0" : 0;
                    d.ask = 0;
                    if ($util.Long) {
                        var n = new $util.Long(0, 0, false);
                        d.askSize = o.longs === String ? n.toString() : o.longs === Number ? n.toNumber() : n;
                    } else
                        d.askSize = o.longs === String ? "0" : 0;
                    if ($util.Long) {
                        var n = new $util.Long(0, 0, false);
                        d.priceHint = o.longs === String ? n.toString() : o.longs === Number ? n.toNumber() : n;
                    } else
                        d.priceHint = o.longs === String ? "0" : 0;
                    if ($util.Long) {
                        var n = new $util.Long(0, 0, false);
                        d.vol_24hr = o.longs === String ? n.toString() : o.longs === Number ? n.toNumber() : n;
                    } else
                        d.vol_24hr = o.longs === String ? "0" : 0;
                    if ($util.Long) {
                        var n = new $util.Long(0, 0, false);
                        d.volAllCurrencies = o.longs === String ? n.toString() : o.longs === Number ? n.toNumber() : n;
                    } else
                        d.volAllCurrencies = o.longs === String ? "0" : 0;
                    d.fromcurrency = "";
                    d.lastMarket = "";
                    d.circulatingSupply = 0;
                    d.marketcap = 0;
                }
                if (m.id != null && m.hasOwnProperty("id")) {
                    d.id = m.id;
                }
                if (m.price != null && m.hasOwnProperty("price")) {
                    d.price = o.json && !isFinite(m.price) ? String(m.price) : m.price;
                }
                if (m.time != null && m.hasOwnProperty("time")) {
                    if (typeof m.time === "number")
                        d.time = o.longs === String ? String(m.time) : m.time;
                    else
                        d.time = o.longs === String ? $util.Long.prototype.toString.call(m.time) : o.longs === Number ? new $util.LongBits(m.time.low >>> 0, m.time.high >>> 0).toNumber() : m.time;
                }
                if (m.currency != null && m.hasOwnProperty("currency")) {
                    d.currency = m.currency;
                }
                if (m.exchange != null && m.hasOwnProperty("exchange")) {
                    d.exchange = m.exchange;
                }
                if (m.quoteType != null && m.hasOwnProperty("quoteType")) {
                    d.quoteType = o.enums === String ? $root.quotefeeder.QuoteType[m.quoteType] : m.quoteType;
                }
                if (m.marketHours != null && m.hasOwnProperty("marketHours")) {
                    d.marketHours = o.enums === String ? $root.quotefeeder.MarketHours[m.marketHours] : m.marketHours;
                }
                if (m.changePercent != null && m.hasOwnProperty("changePercent")) {
                    d.changePercent = o.json && !isFinite(m.changePercent) ? String(m.changePercent) : m.changePercent;
                }
                if (m.dayVolume != null && m.hasOwnProperty("dayVolume")) {
                    if (typeof m.dayVolume === "number")
                        d.dayVolume = o.longs === String ? String(m.dayVolume) : m.dayVolume;
                    else
                        d.dayVolume = o.longs === String ? $util.Long.prototype.toString.call(m.dayVolume) : o.longs === Number ? new $util.LongBits(m.dayVolume.low >>> 0, m.dayVolume.high >>> 0).toNumber() : m.dayVolume;
                }
                if (m.dayHigh != null && m.hasOwnProperty("dayHigh")) {
                    d.dayHigh = o.json && !isFinite(m.dayHigh) ? String(m.dayHigh) : m.dayHigh;
                }
                if (m.dayLow != null && m.hasOwnProperty("dayLow")) {
                    d.dayLow = o.json && !isFinite(m.dayLow) ? String(m.dayLow) : m.dayLow;
                }
                if (m.change != null && m.hasOwnProperty("change")) {
                    d.change = o.json && !isFinite(m.change) ? String(m.change) : m.change;
                }
                if (m.shortName != null && m.hasOwnProperty("shortName")) {
                    d.shortName = m.shortName;
                }
                if (m.expireDate != null && m.hasOwnProperty("expireDate")) {
                    if (typeof m.expireDate === "number")
                        d.expireDate = o.longs === String ? String(m.expireDate) : m.expireDate;
                    else
                        d.expireDate = o.longs === String ? $util.Long.prototype.toString.call(m.expireDate) : o.longs === Number ? new $util.LongBits(m.expireDate.low >>> 0, m.expireDate.high >>> 0).toNumber() : m.expireDate;
                }
                if (m.openPrice != null && m.hasOwnProperty("openPrice")) {
                    d.openPrice = o.json && !isFinite(m.openPrice) ? String(m.openPrice) : m.openPrice;
                }
                if (m.previousClose != null && m.hasOwnProperty("previousClose")) {
                    d.previousClose = o.json && !isFinite(m.previousClose) ? String(m.previousClose) : m.previousClose;
                }
                if (m.strikePrice != null && m.hasOwnProperty("strikePrice")) {
                    d.strikePrice = o.json && !isFinite(m.strikePrice) ? String(m.strikePrice) : m.strikePrice;
                }
                if (m.underlyingSymbol != null && m.hasOwnProperty("underlyingSymbol")) {
                    d.underlyingSymbol = m.underlyingSymbol;
                }
                if (m.openInterest != null && m.hasOwnProperty("openInterest")) {
                    if (typeof m.openInterest === "number")
                        d.openInterest = o.longs === String ? String(m.openInterest) : m.openInterest;
                    else
                        d.openInterest = o.longs === String ? $util.Long.prototype.toString.call(m.openInterest) : o.longs === Number ? new $util.LongBits(m.openInterest.low >>> 0, m.openInterest.high >>> 0).toNumber() : m.openInterest;
                }
                if (m.optionsType != null && m.hasOwnProperty("optionsType")) {
                    if (typeof m.optionsType === "number")
                        d.optionsType = o.longs === String ? String(m.optionsType) : m.optionsType;
                    else
                        d.optionsType = o.longs === String ? $util.Long.prototype.toString.call(m.optionsType) : o.longs === Number ? new $util.LongBits(m.optionsType.low >>> 0, m.optionsType.high >>> 0).toNumber() : m.optionsType;
                }
                if (m.miniOption != null && m.hasOwnProperty("miniOption")) {
                    if (typeof m.miniOption === "number")
                        d.miniOption = o.longs === String ? String(m.miniOption) : m.miniOption;
                    else
                        d.miniOption = o.longs === String ? $util.Long.prototype.toString.call(m.miniOption) : o.longs === Number ? new $util.LongBits(m.miniOption.low >>> 0, m.miniOption.high >>> 0).toNumber() : m.miniOption;
                }
                if (m.lastSize != null && m.hasOwnProperty("lastSize")) {
                    if (typeof m.lastSize === "number")
                        d.lastSize = o.longs === String ? String(m.lastSize) : m.lastSize;
                    else
                        d.lastSize = o.longs === String ? $util.Long.prototype.toString.call(m.lastSize) : o.longs === Number ? new $util.LongBits(m.lastSize.low >>> 0, m.lastSize.high >>> 0).toNumber() : m.lastSize;
                }
                if (m.bid != null && m.hasOwnProperty("bid")) {
                    d.bid = o.json && !isFinite(m.bid) ? String(m.bid) : m.bid;
                }
                if (m.bidSize != null && m.hasOwnProperty("bidSize")) {
                    if (typeof m.bidSize === "number")
                        d.bidSize = o.longs === String ? String(m.bidSize) : m.bidSize;
                    else
                        d.bidSize = o.longs === String ? $util.Long.prototype.toString.call(m.bidSize) : o.longs === Number ? new $util.LongBits(m.bidSize.low >>> 0, m.bidSize.high >>> 0).toNumber() : m.bidSize;
                }
                if (m.ask != null && m.hasOwnProperty("ask")) {
                    d.ask = o.json && !isFinite(m.ask) ? String(m.ask) : m.ask;
                }
                if (m.askSize != null && m.hasOwnProperty("askSize")) {
                    if (typeof m.askSize === "number")
                        d.askSize = o.longs === String ? String(m.askSize) : m.askSize;
                    else
                        d.askSize = o.longs === String ? $util.Long.prototype.toString.call(m.askSize) : o.longs === Number ? new $util.LongBits(m.askSize.low >>> 0, m.askSize.high >>> 0).toNumber() : m.askSize;
                }
                if (m.priceHint != null && m.hasOwnProperty("priceHint")) {
                    if (typeof m.priceHint === "number")
                        d.priceHint = o.longs === String ? String(m.priceHint) : m.priceHint;
                    else
                        d.priceHint = o.longs === String ? $util.Long.prototype.toString.call(m.priceHint) : o.longs === Number ? new $util.LongBits(m.priceHint.low >>> 0, m.priceHint.high >>> 0).toNumber() : m.priceHint;
                }
                if (m.vol_24hr != null && m.hasOwnProperty("vol_24hr")) {
                    if (typeof m.vol_24hr === "number")
                        d.vol_24hr = o.longs === String ? String(m.vol_24hr) : m.vol_24hr;
                    else
                        d.vol_24hr = o.longs === String ? $util.Long.prototype.toString.call(m.vol_24hr) : o.longs === Number ? new $util.LongBits(m.vol_24hr.low >>> 0, m.vol_24hr.high >>> 0).toNumber() : m.vol_24hr;
                }
                if (m.volAllCurrencies != null && m.hasOwnProperty("volAllCurrencies")) {
                    if (typeof m.volAllCurrencies === "number")
                        d.volAllCurrencies = o.longs === String ? String(m.volAllCurrencies) : m.volAllCurrencies;
                    else
                        d.volAllCurrencies = o.longs === String ? $util.Long.prototype.toString.call(m.volAllCurrencies) : o.longs === Number ? new $util.LongBits(m.volAllCurrencies.low >>> 0, m.volAllCurrencies.high >>> 0).toNumber() : m.volAllCurrencies;
                }
                if (m.fromcurrency != null && m.hasOwnProperty("fromcurrency")) {
                    d.fromcurrency = m.fromcurrency;
                }
                if (m.lastMarket != null && m.hasOwnProperty("lastMarket")) {
                    d.lastMarket = m.lastMarket;
                }
                if (m.circulatingSupply != null && m.hasOwnProperty("circulatingSupply")) {
                    d.circulatingSupply = o.json && !isFinite(m.circulatingSupply) ? String(m.circulatingSupply) : m.circulatingSupply;
                }
                if (m.marketcap != null && m.hasOwnProperty("marketcap")) {
                    d.marketcap = o.json && !isFinite(m.marketcap) ? String(m.marketcap) : m.marketcap;
                }
                return d;
            };
    
            PricingData.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };
    
            return PricingData;
        })();
    
        quotefeeder.StaticData = (function() {
    
            function StaticData(p) {
                if (p)
                    for (var ks = Object.keys(p), i = 0; i < ks.length; ++i)
                        if (p[ks[i]] != null)
                            this[ks[i]] = p[ks[i]];
            }
    
            StaticData.prototype.id = "";
            StaticData.prototype.displayName = "";
            StaticData.prototype.currency = "";
            StaticData.prototype.exchange = "";
            StaticData.prototype.openPrice = 0;
            StaticData.prototype.closePrice = 0;
            StaticData.prototype.fiftytwoWkMovingAvgPrice = 0;
            StaticData.prototype.twohundredDataMovingAvgPrice = 0;
    
            StaticData.decode = function decode(r, l) {
                if (!(r instanceof $Reader))
                    r = $Reader.create(r);
                var c = l === undefined ? r.len : r.pos + l, m = new $root.quotefeeder.StaticData();
                while (r.pos < c) {
                    var t = r.uint32();
                    switch (t >>> 3) {
                    case 1:
                        m.id = r.string();
                        break;
                    case 2:
                        m.displayName = r.string();
                        break;
                    case 3:
                        m.currency = r.string();
                        break;
                    case 4:
                        m.exchange = r.string();
                        break;
                    case 5:
                        m.openPrice = r.float();
                        break;
                    case 6:
                        m.closePrice = r.float();
                        break;
                    case 7:
                        m.fiftytwoWkMovingAvgPrice = r.float();
                        break;
                    case 8:
                        m.twohundredDataMovingAvgPrice = r.float();
                        break;
                    default:
                        r.skipType(t & 7);
                        break;
                    }
                }
                return m;
            };
    
            StaticData.fromObject = function fromObject(d) {
                if (d instanceof $root.quotefeeder.StaticData)
                    return d;
                var m = new $root.quotefeeder.StaticData();
                if (d.id != null) {
                    m.id = String(d.id);
                }
                if (d.displayName != null) {
                    m.displayName = String(d.displayName);
                }
                if (d.currency != null) {
                    m.currency = String(d.currency);
                }
                if (d.exchange != null) {
                    m.exchange = String(d.exchange);
                }
                if (d.openPrice != null) {
                    m.openPrice = Number(d.openPrice);
                }
                if (d.closePrice != null) {
                    m.closePrice = Number(d.closePrice);
                }
                if (d.fiftytwoWkMovingAvgPrice != null) {
                    m.fiftytwoWkMovingAvgPrice = Number(d.fiftytwoWkMovingAvgPrice);
                }
                if (d.twohundredDataMovingAvgPrice != null) {
                    m.twohundredDataMovingAvgPrice = Number(d.twohundredDataMovingAvgPrice);
                }
                return m;
            };
    
            StaticData.toObject = function toObject(m, o) {
                if (!o)
                    o = {};
                var d = {};
                if (o.defaults) {
                    d.id = "";
                    d.displayName = "";
                    d.currency = "";
                    d.exchange = "";
                    d.openPrice = 0;
                    d.closePrice = 0;
                    d.fiftytwoWkMovingAvgPrice = 0;
                    d.twohundredDataMovingAvgPrice = 0;
                }
                if (m.id != null && m.hasOwnProperty("id")) {
                    d.id = m.id;
                }
                if (m.displayName != null && m.hasOwnProperty("displayName")) {
                    d.displayName = m.displayName;
                }
                if (m.currency != null && m.hasOwnProperty("currency")) {
                    d.currency = m.currency;
                }
                if (m.exchange != null && m.hasOwnProperty("exchange")) {
                    d.exchange = m.exchange;
                }
                if (m.openPrice != null && m.hasOwnProperty("openPrice")) {
                    d.openPrice = o.json && !isFinite(m.openPrice) ? String(m.openPrice) : m.openPrice;
                }
                if (m.closePrice != null && m.hasOwnProperty("closePrice")) {
                    d.closePrice = o.json && !isFinite(m.closePrice) ? String(m.closePrice) : m.closePrice;
                }
                if (m.fiftytwoWkMovingAvgPrice != null && m.hasOwnProperty("fiftytwoWkMovingAvgPrice")) {
                    d.fiftytwoWkMovingAvgPrice = o.json && !isFinite(m.fiftytwoWkMovingAvgPrice) ? String(m.fiftytwoWkMovingAvgPrice) : m.fiftytwoWkMovingAvgPrice;
                }
                if (m.twohundredDataMovingAvgPrice != null && m.hasOwnProperty("twohundredDataMovingAvgPrice")) {
                    d.twohundredDataMovingAvgPrice = o.json && !isFinite(m.twohundredDataMovingAvgPrice) ? String(m.twohundredDataMovingAvgPrice) : m.twohundredDataMovingAvgPrice;
                }
                return d;
            };
    
            StaticData.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };
    
            return StaticData;
        })();
    
        quotefeeder.PriceUpdate = (function() {
    
            function PriceUpdate(p) {
                if (p)
                    for (var ks = Object.keys(p), i = 0; i < ks.length; ++i)
                        if (p[ks[i]] != null)
                            this[ks[i]] = p[ks[i]];
            }
    
            PriceUpdate.prototype.pricingData = null;
    
            PriceUpdate.decode = function decode(r, l) {
                if (!(r instanceof $Reader))
                    r = $Reader.create(r);
                var c = l === undefined ? r.len : r.pos + l, m = new $root.quotefeeder.PriceUpdate();
                while (r.pos < c) {
                    var t = r.uint32();
                    switch (t >>> 3) {
                    case 1:
                        m.pricingData = $root.quotefeeder.PricingData.decode(r, r.uint32());
                        break;
                    default:
                        r.skipType(t & 7);
                        break;
                    }
                }
                return m;
            };
    
            PriceUpdate.fromObject = function fromObject(d) {
                if (d instanceof $root.quotefeeder.PriceUpdate)
                    return d;
                var m = new $root.quotefeeder.PriceUpdate();
                if (d.pricingData != null) {
                    if (typeof d.pricingData !== "object")
                        throw TypeError(".quotefeeder.PriceUpdate.pricingData: object expected");
                    m.pricingData = $root.quotefeeder.PricingData.fromObject(d.pricingData);
                }
                return m;
            };
    
            PriceUpdate.toObject = function toObject(m, o) {
                if (!o)
                    o = {};
                var d = {};
                if (o.defaults) {
                    d.pricingData = null;
                }
                if (m.pricingData != null && m.hasOwnProperty("pricingData")) {
                    d.pricingData = $root.quotefeeder.PricingData.toObject(m.pricingData, o);
                }
                return d;
            };
    
            PriceUpdate.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };
    
            return PriceUpdate;
        })();
    
        quotefeeder.StaticUpdate = (function() {
    
            function StaticUpdate(p) {
                if (p)
                    for (var ks = Object.keys(p), i = 0; i < ks.length; ++i)
                        if (p[ks[i]] != null)
                            this[ks[i]] = p[ks[i]];
            }
    
            StaticUpdate.prototype.staticData = null;
    
            StaticUpdate.decode = function decode(r, l) {
                if (!(r instanceof $Reader))
                    r = $Reader.create(r);
                var c = l === undefined ? r.len : r.pos + l, m = new $root.quotefeeder.StaticUpdate();
                while (r.pos < c) {
                    var t = r.uint32();
                    switch (t >>> 3) {
                    case 1:
                        m.staticData = $root.quotefeeder.StaticData.decode(r, r.uint32());
                        break;
                    default:
                        r.skipType(t & 7);
                        break;
                    }
                }
                return m;
            };
    
            StaticUpdate.fromObject = function fromObject(d) {
                if (d instanceof $root.quotefeeder.StaticUpdate)
                    return d;
                var m = new $root.quotefeeder.StaticUpdate();
                if (d.staticData != null) {
                    if (typeof d.staticData !== "object")
                        throw TypeError(".quotefeeder.StaticUpdate.staticData: object expected");
                    m.staticData = $root.quotefeeder.StaticData.fromObject(d.staticData);
                }
                return m;
            };
    
            StaticUpdate.toObject = function toObject(m, o) {
                if (!o)
                    o = {};
                var d = {};
                if (o.defaults) {
                    d.staticData = null;
                }
                if (m.staticData != null && m.hasOwnProperty("staticData")) {
                    d.staticData = $root.quotefeeder.StaticData.toObject(m.staticData, o);
                }
                return d;
            };
    
            StaticUpdate.prototype.toJSON = function toJSON() {
                return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
            };
    
            return StaticUpdate;
        })();
    
        return quotefeeder;
    })();

    return $root;
})(protobuf);
