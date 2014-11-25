#!/usr/bin/env python3.3
# Copyright 2014 Adobe Systems Incorporated. All Rights Reserved.
#
# Adobe permits you to use, modify, and distribute this file in accordance
# with the terms of the Mozilla Public License, v 2.0 accompanying it.  If
# a copy of the MPL was not distributed with this file, You can obtain one
# at http://mozilla.org/MPL/2.0/.
#
# Create perforce changelist of modules from FTP server

import io, argparse,pyasn1, bitstring
from pyasn1.codec.der import encoder, decoder
from pyasn1.type import univ, namedtype, namedval, constraint
import hashlib

# CodeSectionDigest ::= SEQUENCE {
# offset				INTEGER --  section's file offset in the signed binary
# digestAlgorithm		OBJECT IDENTIFIER -- algorithm identifier for the hash value below. For now only supports SHA256.
# digestValue			OCTET STRING -- hash value of the TEXT segment.
# }
class CodeSectionDigest(univ.Sequence):
	componentType = namedtype.NamedTypes(
		namedtype.NamedType('offset', univ.Integer()),
		namedtype.NamedType('digestAlgorithm', univ.ObjectIdentifier()),
		namedtype.NamedType('digest', univ.OctetString()))

# CodeSegmentDigest ::= SEQUENCE {
#	 offset				INTEGER -- TEXT segment's file offset in the signed binary
#	 codeSectionDigests			SET OF CodeSectionDigests
# }

class SetOfCodeSectionDigest(univ.SetOf):
	componentType = CodeSectionDigest()

class CodeSegmentDigest(univ.Sequence):
	componentType = namedtype.NamedTypes(
		namedtype.NamedType('offset', univ.Integer()),
		namedtype.NamedType('codeSectionDigests', SetOfCodeSectionDigest()))

# ArchitectureDigest ::= SEQUENCE {
# 	cpuType                ENUMERATED CpuType
# 	cpuSubType				ENUMERATED CpuSubType
# 	CodeSegmentDigests		SET OF CodeSegmentDigests
# }

class SetOfCodeSegmentDigest(univ.SetOf):
	componentType = CodeSegmentDigest()

class CPUType(univ.Enumerated):
	namedValues = namedval.NamedValues(
		('IMAGE_FILE_MACHINE_I386', 0x14c),
		('IMAGE_FILE_MACHINE_AMD64',0x8664 )
	)
	subtypeSpec = univ.Enumerated.subtypeSpec + \
				  constraint.SingleValueConstraint(0x14c, 0x8664)


class ArchitectureDigest(univ.Sequence):
	componentType = namedtype.NamedTypes(
		namedtype.NamedType('cpuType', CPUType()),
		namedtype.NamedType('cpuSubType', univ.Integer()),
		namedtype.NamedType('CodeSegmentDigests', SetOfCodeSegmentDigest())
	)

#	 ApplicationDigest ::= SEQUENCE {
#	 version    INTEGER
#	 digests    SET OF ArchitectureDigest
#	 }
class SetOfArchitectureDigest(univ.SetOf):
	componentType = ArchitectureDigest()

class ApplicationDigest(univ.Sequence):
	componentType = namedtype.NamedTypes(
		namedtype.NamedType('version', univ.Integer()),
		namedtype.NamedType('digests', SetOfArchitectureDigest())
	)

def meetsRequirements(items, requirements):
	for r in requirements:
		for n, v in r.items():
			if n not in items or items[n] != v: return False
	return True

# return total number of bytes read from items_in excluding leaves
def parseItems(stream, items_in, items_out):
	bits_read = 0
	total_bits_read = 0

	for item in items_in:
		name = item[0]
		t = item[1]
		bits = 1 if ":" not in t else int(t[t.index(":") + 1:])

		if ":" in t and t.find("bytes") >= 0:
			bits = bits * 8

		if len(item) == 2:
			items_out[name] = stream.read(t)
			bits_read += bits
			total_bits_read += bits
		elif len(item) == 3 or len(item) == 4:
			requirements = list(filter(lambda x: isinstance(x, dict), item[2]))
			sub_items = list(filter(lambda x: isinstance(x, tuple), item[2]))

			if not meetsRequirements(items_out, requirements): continue

			# has sub-items based on length
			items_out[name] = stream.read(t)
			bits_read += bits
			total_bits_read += bits

			if len(item) == 4:
				bit_length = items_out[name] * 8

				if bit_length > 0:
					sub_read, sub_total_read = parseItems(stream, sub_items, items_out)
					bit_length -= sub_read
					total_bits_read += sub_total_read

					if bit_length > 0:
						items_out[item[3]] = stream.read('bits:' + str(bit_length))
						bits_read += bit_length
						total_bits_read += bit_length
		else:
			raise Exception("unrecognized item" + pprint.pformat(item))

	return bits_read, total_bits_read


class SectionHeader:
	def __init__(self, stream):
		items = [
			('Name', 'bytes:8'),
			('VirtualSize', 'uintle:32'),
			('VirtualAddress', 'uintle:32'),
			('SizeOfRawData', 'uintle:32'),
			('PointerToRawData', 'uintle:32'),
			('PointerToRelocations', 'uintle:32'),
			('PointerToLineNumber', 'uintle:32'),
			('NumberOfRelocations', 'uintle:16'),
			('NumberOfLineNumbers', 'uintle:16'),
			('Characteristics', 'uintle:32')
		]
		self.items = {}
		_, self.bits_read = parseItems(stream, items, self.items)

		self.sectionName = self.items['Name'].decode('utf-8')
		self.offset = self.items['PointerToRawData']


class COFFFileHeader:
	def __init__(self, stream):
		items = [
			('Machine', 'uintle:16'),
			('NumberOfSections', 'uintle:16'),
			('TimeDateStamp', 'uintle:32'),
			('PointerToSymbolTable', 'uintle:32'),
			('NumberOfSymbols', 'uintle:32'),
			('SizeOfOptionalHeader', 'uintle:16'),
			('Characteristics', 'uintle:16')
		]
		self.items = {}
		_, self.bits_read = parseItems(stream, items, self.items)

		#skip over optional header.
		if self.items['SizeOfOptionalHeader'] > 0:
			stream.read(self.items['SizeOfOptionalHeader'] * 8)
			self.bits_read += self.items['SizeOfOptionalHeader'] * 8

		#start reading section headers
		numberOfSections = self.items['NumberOfSections']
		self.codeSectionHeaders = []

		while numberOfSections > 0 :
			sectionHeader = SectionHeader(stream)
			if (sectionHeader.items['Characteristics'] & 0x20000000) == 0x20000000:
				self.codeSectionHeaders.append(sectionHeader)
			numberOfSections -= 1

		self.codeSectionHeaders.sort(key=lambda header: header.offset)


def main():
	parser = argparse.ArgumentParser(description='PE/COFF Parser.')
	parser.add_argument('-input', required=True, help="File to parse.")
	parser.add_argument('-output', required=True, help="File to write to.")
	parser.add_argument('-verbose', action='store_true', help="Verbose output.")
	app_args = parser.parse_args()

	stream = bitstring.ConstBitStream(filename=app_args.input)
	# find the COFF header.
	# skip forward past the MSDOS stub header to 0x3c.

	stream.bytepos = 0x3c

	# read 4 bytes, this is the file offset of the PE signature.
	peSignatureOffset = stream.read('uintle:32')
	stream.bytepos = peSignatureOffset

	#read 4 bytes, make sure it's a PE signature.
	signature = stream.read('uintle:32')
	if signature != 0x00004550 :
		return


	archDigest = ArchitectureDigest()

	codeSegmentDigests = SetOfCodeSegmentDigest()
	codeSegmentIdx = 0

	#after signature is the actual COFF file header.
	coffFileHeader = COFFFileHeader(stream)

	if coffFileHeader.items['Machine'] == 0x14c:
		archDigest.setComponentByName('cpuType', CPUType('IMAGE_FILE_MACHINE_I386'))
	elif coffFileHeader.items['Machine'] == 0x8664:
		archDigest.setComponentByName('cpuType', CPUType('IMAGE_FILE_MACHINE_AMD64'))

	archDigest.setComponentByName('cpuSubType', 0)


	for codeSectionHeader in coffFileHeader.codeSectionHeaders:
		stream.bytepos = codeSectionHeader.offset
		codeSectionBytes = stream.read('bytes:'+ str(codeSectionHeader.items['SizeOfRawData']))
		if codeSectionHeader.items['SizeOfRawData'] < codeSectionHeader.items['VirtualSize']:
			#zero pad up to virtualSize
			codeSectionBytes += "\0" * (codeSectionHeader.items['VirtualSize']-codeSectionHeader.items['SizeOfRawData'])

		digester = hashlib.sha256()
		digester.update(codeSectionBytes)
		digest = digester.digest()

		codeSectionDigest = CodeSectionDigest()
		codeSectionDigest.setComponentByName('offset', codeSectionHeader.offset)
		codeSectionDigest.setComponentByName('digestAlgorithm', univ.ObjectIdentifier('2.16.840.1.101.3.4.2.1'))
		codeSectionDigest.setComponentByName('digest', univ.OctetString(digest))

		setOfDigest = SetOfCodeSectionDigest()
		setOfDigest.setComponentByPosition(0, codeSectionDigest)

		codeSegmentDigest = CodeSegmentDigest()
		codeSegmentDigest.setComponentByName('offset', codeSectionHeader.offset)
		codeSegmentDigest.setComponentByName('codeSectionDigests', setOfDigest)

		codeSegmentDigests.setComponentByPosition(codeSegmentIdx, codeSegmentDigest)
		codeSegmentIdx += 1

	archDigest.setComponentByName('CodeSegmentDigests', codeSegmentDigests)

	setOfArchDigests = SetOfArchitectureDigest()
	setOfArchDigests.setComponentByPosition(0, archDigest)

	appDigest = ApplicationDigest()

	appDigest.setComponentByName('version', 1)
	appDigest.setComponentByName('digests', setOfArchDigests)

	binaryDigest = encoder.encode(appDigest)

	outFile = open(app_args.output, 'wb')
	outFile.write(binaryDigest)

if __name__ == '__main__':
	main()
