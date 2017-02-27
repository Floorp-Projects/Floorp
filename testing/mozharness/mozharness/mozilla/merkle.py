#!/usr/bin/env python

import struct

def _round2(n):
    k = 1
    while k < n:
        k <<= 1
    return k >> 1

def _leaf_hash(hash_fn, leaf):
    return hash_fn(b'\x00' + leaf).digest()

def _pair_hash(hash_fn, left, right):
    return hash_fn(b'\x01' + left + right).digest()

class InclusionProof:
    """
    Represents a Merkle inclusion proof for purposes of serialization,
    deserialization, and verification of the proof.  The format for inclusion
    proofs in RFC 6962-bis is as follows:

        opaque LogID<2..127>;
        opaque NodeHash<32..2^8-1>;

        struct {
            LogID log_id;
            uint64 tree_size;
            uint64 leaf_index;
            NodeHash inclusion_path<1..2^16-1>;
        } InclusionProofDataV2;

    In other words:
      - 1 + N octets of log_id (currently zero)
      - 8 octets of tree_size = self.n
      - 8 octets of leaf_index = m
      - 2 octets of path length, followed by
      * 1 + N octets of NodeHash
    """

    # Pre-generated 'log ID'.  Not used by Firefox; it is only needed because
    # there's a slot in the RFC 6962-bis format that requires a value at least
    # two bytes long (plus a length byte).
    LOG_ID = b'\x02\x00\x00'

    def __init__(self, tree_size, leaf_index, path_elements):
        self.tree_size = tree_size
        self.leaf_index = leaf_index
        self.path_elements = path_elements

    @staticmethod
    def from_rfc6962_bis(serialized):
        start = 0
        read = 1
        if len(serialized) < start + read:
            raise Exception('Inclusion proof too short for log ID header')
        log_id_len, = struct.unpack('B', serialized[start:start+read])
        start += read
        start += log_id_len # Ignore the log ID itself

        read = 8 + 8 + 2
        if len(serialized) < start + read:
            raise Exception('Inclusion proof too short for middle section')
        tree_size, leaf_index, path_len = struct.unpack('!QQH', serialized[start:start+read])
        start += read

        path_elements = []
        end = 1 + log_id_len + 8 + 8 + 2 + path_len
        while start < end:
            read = 1
            if len(serialized) < start + read:
                raise Exception('Inclusion proof too short for middle section')
            elem_len, = struct.unpack('!B', serialized[start:start+read])
            start += read

            read = elem_len
            if len(serialized) < start + read:
                raise Exception('Inclusion proof too short for middle section')
            if end < start + read:
                raise Exception('Inclusion proof element exceeds declared length')
            path_elements.append(serialized[start:start+read])
            start += read

        return InclusionProof(tree_size, leaf_index, path_elements)

    def to_rfc6962_bis(self):
        inclusion_path = b''
        for step in self.path_elements:
            step_len = struct.pack('B', len(step))
            inclusion_path += step_len + step

        middle = struct.pack('!QQH', self.tree_size, self.leaf_index, len(inclusion_path))
        return self.LOG_ID + middle + inclusion_path

    def _expected_head(self, hash_fn, leaf, leaf_index, tree_size):
        node = _leaf_hash(hash_fn, leaf)

        # Compute indicators of which direction the pair hashes should be done.
        # Derived from the PATH logic in draft-ietf-trans-rfc6962-bis
        lr = []
        while tree_size > 1:
            k = _round2(tree_size)
            left = leaf_index < k
            lr = [left] + lr

            if left:
                tree_size = k
            else:
                tree_size = tree_size - k
                leaf_index = leaf_index - k

        assert(len(lr) == len(self.path_elements))
        for i, elem in enumerate(self.path_elements):
            if lr[i]:
                node = _pair_hash(hash_fn, node, elem)
            else:
                node = _pair_hash(hash_fn, elem, node)

        return node


    def verify(self, hash_fn, leaf, leaf_index, tree_size, tree_head):
        return self._expected_head(hash_fn, leaf, leaf_index, tree_size) == tree_head

class MerkleTree:
    """
    Implements a Merkle tree on a set of data items following the
    structure defined in RFC 6962-bis.  This allows us to create a
    single hash value that summarizes the data (the 'head'), and an
    'inclusion proof' for each element that connects it to the head.

    https://tools.ietf.org/html/draft-ietf-trans-rfc6962-bis-24
    """

    def __init__(self, hash_fn, data):
        self.n = len(data)
        self.hash_fn = hash_fn

        # We cache intermediate node values, as a dictionary of dictionaries,
        # where the node representing data elements data[m:n] is represented by
        # nodes[m][n]. This corresponds to the 'D[m:n]' notation in RFC
        # 6962-bis.  In particular, the leaves are stored in nodes[i][i+1] and
        # the head is nodes[0][n].
        self.nodes = {}
        for i in range(self.n):
            self.nodes[i, i+1] = _leaf_hash(self.hash_fn, data[i])

    def _node(self, start, end):
        if (start, end) in self.nodes:
            return self.nodes[start, end]

        k = _round2(end - start)
        left = self._node(start, start + k)
        right = self._node(start + k, end)
        node = _pair_hash(self.hash_fn, left, right)

        self.nodes[start, end] = node
        return node

    def head(self):
        return self._node(0, self.n)

    def _relative_proof(self, target, start, end):
        n = end - start
        k = _round2(n)

        if n == 1:
            return []
        elif target - start < k:
            return self._relative_proof(target, start, start + k) + [self._node(start + k, end)]
        elif target - start >= k:
            return self._relative_proof(target, start + k, end) + [self._node(start, start + k)]

    def inclusion_proof(self, leaf_index):
        path_elements = self._relative_proof(leaf_index, 0, self.n)
        return InclusionProof(self.n, leaf_index, path_elements)
