import unittest
import hashlib
import random
from mozharness.mozilla.merkle import MerkleTree, InclusionProof

# Pre-computed tree on 7 inputs
#
#         ______F_____
#        /            \
#     __D__           _E_
#    /     \         /   \
#   A       B       C     |
#  / \     / \     / \    |
# 0   1   2   3   4   5   6
hash_fn = hashlib.sha256

data = [
    'fbc459361fc111024c6d1fd83d23a9ff'.decode('hex'),
    'ae3a44925afec860451cd8658b3cadde'.decode('hex'),
    '418903fe6ef29fc8cab93d778a7b018b'.decode('hex'),
    '3d1c53c00b2e137af8c4c23a06388c6b'.decode('hex'),
    'e656ebd8e2758bc72599e5896be357be'.decode('hex'),
    '81aae91cf90be172eedd1c75c349bf9e'.decode('hex'),
    '00c262edf8b0bc345aca769e8733e25e'.decode('hex'),
]

leaves = [
    '5cb551f87797381a24a5359a986e2cef25b1f2113b387197fe48e8babc9ad5c7'.decode('hex'),
    '9899dc0be00306bda2a8e69cec32525ca6244f132479bcf840d8c1bc8bdfbff2'.decode('hex'),
    'fdd27d0393e32637b474efb9b3efad29568c3ec9b091fdda40fd57ec9196f06d'.decode('hex'),
    'c87292a6c8528c2a0679b6c1eefb47e4dbac7840d23645d5b7cb47cf1a8d365f'.decode('hex'),
    '2ff3bdac9bec3580b82da8a357746f15919414d9cbe517e2dd96910c9814c30c'.decode('hex'),
    '883e318240eccc0e2effafebdb0fd4fd26d0996da1b01439566cb9babef8725f'.decode('hex'),
    'bb13dfb7b202a95f241ea1715c8549dc048d9936ec747028002f7c795de72fcf'.decode('hex'),
]

nodeA = '06447a7baa079cb0b4b6119d0f575bec508915403fdc30923eba982b63759805'.decode('hex')
nodeB = '3db98027c655ead4fe897bef3a4b361839a337941a9e624b475580c9d4e882ee'.decode('hex')
nodeC = '17524f8b0169b2745c67846925d55449ae80a8022ef8189dcf4cbb0ec7fcc470'.decode('hex')
nodeD = '380d0dc6fd7d4f37859a12dbfc7171b3cce29ab0688c6cffd2b15f3e0b21af49'.decode('hex')
nodeE = '3a9c2886a5179a6e1948876034f99d52a8f393f47a09887adee6d1b4a5c5fbd6'.decode('hex')
nodeF = 'd1a0d3947db4ae8305f2ac32985957e02659b2ea3c10da52a48d2526e9af3bbc'.decode('hex')

proofs = [
    [leaves[1], nodeB, nodeE],
    [leaves[0], nodeB, nodeE],
    [leaves[3], nodeA, nodeE],
    [leaves[2], nodeA, nodeE],
    [leaves[5], leaves[6], nodeD],
    [leaves[4], leaves[6], nodeD],
    [nodeC, nodeD],
]

known_proof5 = ('020000' + \
                '0000000000000007' + '0000000000000005' + \
                '0063' + \
                '20' + leaves[4].encode('hex') + \
                '20' + leaves[6].encode('hex') + \
                '20' + nodeD.encode('hex')).decode('hex')


class TestMerkleTree(unittest.TestCase):
    def testPreComputed(self):
        tree = MerkleTree(hash_fn, data)
        head = tree.head()
        self.assertEquals(head, nodeF)

        for i in range(len(data)):
            proof = tree.inclusion_proof(i)

            self.assertTrue(proof.verify(hash_fn, data[i], i, len(data), head))
            self.assertEquals(proof.leaf_index, i)
            self.assertEquals(proof.tree_size, tree.n)
            self.assertEquals(proof.path_elements, proofs[i])


    def testInclusionProofEncodeDecode(self):
        tree = MerkleTree(hash_fn, data)

        # Inclusion proof encode/decode round trip test
        proof5 = tree.inclusion_proof(5)
        serialized5 = proof5.to_rfc6962_bis()
        deserialized5 = InclusionProof.from_rfc6962_bis(serialized5)
        reserialized5 = deserialized5.to_rfc6962_bis()
        self.assertEquals(serialized5, reserialized5)

        # Inclusion proof encode known answer test
        serialized5 = proof5.to_rfc6962_bis()
        self.assertEquals(serialized5, known_proof5)

        # Inclusion proof decode known answer test
        known_deserialized5 = InclusionProof.from_rfc6962_bis(known_proof5)
        self.assertEquals(proof5.leaf_index, known_deserialized5.leaf_index)
        self.assertEquals(proof5.tree_size, known_deserialized5.tree_size)
        self.assertEquals(proof5.path_elements, known_deserialized5.path_elements)

    def testLargeTree(self):
        TEST_SIZE = 5000
        ELEM_SIZE_BYTES = 16
        data = [bytearray(random.getrandbits(8) for _ in xrange(ELEM_SIZE_BYTES)) for _ in xrange(TEST_SIZE)]
        tree = MerkleTree(hash_fn, data)
        head = tree.head()

        for i in range(len(data)):
            proof = tree.inclusion_proof(i)

            self.assertTrue(proof.verify(hash_fn, data[i], i, len(data), head))
            self.assertEquals(proof.leaf_index, i)
            self.assertEquals(proof.tree_size, tree.n)
