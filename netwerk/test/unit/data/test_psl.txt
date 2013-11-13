// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

// null input.
checkPublicSuffix(null, null);
// Mixed case.
checkPublicSuffix('COM', null);
checkPublicSuffix('example.COM', 'example.com');
checkPublicSuffix('WwW.example.COM', 'example.com');
// Leading dot.
checkPublicSuffix('.com', null);
checkPublicSuffix('.example', null);
checkPublicSuffix('.example.com', null);
checkPublicSuffix('.example.example', null);
// Unlisted TLD.
checkPublicSuffix('example', null);
checkPublicSuffix('example.example', 'example.example');
checkPublicSuffix('b.example.example', 'example.example');
checkPublicSuffix('a.b.example.example', 'example.example');
// Listed, but non-Internet, TLD.
//checkPublicSuffix('local', null);
//checkPublicSuffix('example.local', null);
//checkPublicSuffix('b.example.local', null);
//checkPublicSuffix('a.b.example.local', null);
// TLD with only 1 rule.
checkPublicSuffix('biz', null);
checkPublicSuffix('domain.biz', 'domain.biz');
checkPublicSuffix('b.domain.biz', 'domain.biz');
checkPublicSuffix('a.b.domain.biz', 'domain.biz');
// TLD with some 2-level rules.
checkPublicSuffix('com', null);
checkPublicSuffix('example.com', 'example.com');
checkPublicSuffix('b.example.com', 'example.com');
checkPublicSuffix('a.b.example.com', 'example.com');
checkPublicSuffix('uk.com', null);
checkPublicSuffix('example.uk.com', 'example.uk.com');
checkPublicSuffix('b.example.uk.com', 'example.uk.com');
checkPublicSuffix('a.b.example.uk.com', 'example.uk.com');
checkPublicSuffix('test.ac', 'test.ac');
// TLD with only 1 (wildcard) rule.
checkPublicSuffix('cy', null);
checkPublicSuffix('c.cy', null);
checkPublicSuffix('b.c.cy', 'b.c.cy');
checkPublicSuffix('a.b.c.cy', 'b.c.cy');
// More complex TLD.
checkPublicSuffix('jp', null);
checkPublicSuffix('test.jp', 'test.jp');
checkPublicSuffix('www.test.jp', 'test.jp');
checkPublicSuffix('ac.jp', null);
checkPublicSuffix('test.ac.jp', 'test.ac.jp');
checkPublicSuffix('www.test.ac.jp', 'test.ac.jp');
checkPublicSuffix('kyoto.jp', null);
checkPublicSuffix('test.kyoto.jp', 'test.kyoto.jp');
checkPublicSuffix('ide.kyoto.jp', null);
checkPublicSuffix('b.ide.kyoto.jp', 'b.ide.kyoto.jp');
checkPublicSuffix('a.b.ide.kyoto.jp', 'b.ide.kyoto.jp');
checkPublicSuffix('c.kobe.jp', null);
checkPublicSuffix('b.c.kobe.jp', 'b.c.kobe.jp');
checkPublicSuffix('a.b.c.kobe.jp', 'b.c.kobe.jp');
checkPublicSuffix('city.kobe.jp', 'city.kobe.jp');
checkPublicSuffix('www.city.kobe.jp', 'city.kobe.jp');
// TLD with a wildcard rule and exceptions.
checkPublicSuffix('ck', null);
checkPublicSuffix('test.ck', null);
checkPublicSuffix('b.test.ck', 'b.test.ck');
checkPublicSuffix('a.b.test.ck', 'b.test.ck');
checkPublicSuffix('www.ck', 'www.ck');
checkPublicSuffix('www.www.ck', 'www.ck');
// US K12.
checkPublicSuffix('us', null);
checkPublicSuffix('test.us', 'test.us');
checkPublicSuffix('www.test.us', 'test.us');
checkPublicSuffix('ak.us', null);
checkPublicSuffix('test.ak.us', 'test.ak.us');
checkPublicSuffix('www.test.ak.us', 'test.ak.us');
checkPublicSuffix('k12.ak.us', null);
checkPublicSuffix('test.k12.ak.us', 'test.k12.ak.us');
checkPublicSuffix('www.test.k12.ak.us', 'test.k12.ak.us');
// IDN labels.
checkPublicSuffix('食狮.com.cn', '食狮.com.cn');
checkPublicSuffix('食狮.公司.cn', '食狮.公司.cn');
checkPublicSuffix('www.食狮.公司.cn', '食狮.公司.cn');
checkPublicSuffix('shishi.公司.cn', 'shishi.公司.cn');
checkPublicSuffix('公司.cn', null);
checkPublicSuffix('食狮.中国', '食狮.中国');
checkPublicSuffix('www.食狮.中国', '食狮.中国');
checkPublicSuffix('shishi.中国', 'shishi.中国');
checkPublicSuffix('中国', null);
// Same as above, but punycoded.
checkPublicSuffix('xn--85x722f.com.cn', 'xn--85x722f.com.cn');
checkPublicSuffix('xn--85x722f.xn--55qx5d.cn', 'xn--85x722f.xn--55qx5d.cn');
checkPublicSuffix('www.xn--85x722f.xn--55qx5d.cn', 'xn--85x722f.xn--55qx5d.cn');
checkPublicSuffix('shishi.xn--55qx5d.cn', 'shishi.xn--55qx5d.cn');
checkPublicSuffix('xn--55qx5d.cn', null);
checkPublicSuffix('xn--85x722f.xn--fiqs8s', 'xn--85x722f.xn--fiqs8s');
checkPublicSuffix('www.xn--85x722f.xn--fiqs8s', 'xn--85x722f.xn--fiqs8s');
checkPublicSuffix('shishi.xn--fiqs8s', 'shishi.xn--fiqs8s');
checkPublicSuffix('xn--fiqs8s', null);
