from Key import Key
import time
from collections import OrderedDict

def test_generation_perf(n = 100):
    results = OrderedDict()
    for bits in (192, 224, 256, 384, 521):
        t = time.time()
        for i in xrange(n):
            k = Key.generate(bits)
        t = time.time() - t
        results[bits] = t
    return results
        
def test_signing_perf(n = 100):
    results = OrderedDict()
    for bits in (192, 224, 256, 384, 521):
        k = Key.generate(bits)
        t = time.time()
        for i in xrange(n):
            k.sign("random string")
        t = time.time() - t
        results[bits] = t
    return results

def test_verification_perf(n = 100):
    results = OrderedDict()
    for bits in (192, 224, 256, 384, 521):
        k = Key.generate(bits)
        s = k.sign("random string")
        t = time.time()
        for i in xrange(n):
            k.verify("random string", s)
        t = time.time() - t
        results[bits] = t
    return results
            
def print_dict(title, d):
    print title
    print '-' * len(title)
    for k, v in d.items():
        print k, '\t', v
    print

n = 100
print_dict("Key generation", test_generation_perf(n))
print_dict("Signing", test_signing_perf(n))
print_dict("Verifying", test_verification_perf(n))

        
