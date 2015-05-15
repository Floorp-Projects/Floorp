#!/usr/bin/env python2.7
#
# Copyright 2014 Adobe Systems Incorporated. All Rights Reserved.
#
# Adobe permits you to use, modify, and distribute this file in accordance
# with the terms of the Mozilla Public License, v 2.0 accompanying it.  If
# a copy of the MPL was not distributed with this file, You can obtain one
# at http://mozilla.org/MPL/2.0/.
#
# Creates an Adobe Access signed voucher for x32/x64 windows executables
#   Notes: This is currently python2.7 due to mozilla build system requirements

from __future__ import print_function

import argparse, bitstring, pprint, hashlib, os, subprocess, sys, tempfile
from pyasn1.codec.der import encoder as der_encoder
from pyasn1.type import univ, namedtype, namedval, constraint

# Defined in WinNT.h from the Windows SDK
IMAGE_SCN_MEM_EXECUTE = 0x20000000
IMAGE_REL_BASED_HIGHLOW = 3
IMAGE_REL_BASED_DIR64 = 10


# CodeSectionDigest ::= SEQUENCE {
#   offset				INTEGER --  section's file offset in the signed binary
#   digestAlgorithm		OBJECT IDENTIFIER -- algorithm identifier for the hash value below. For now only supports SHA256.
#   digestValue			OCTET STRING -- hash value of the TEXT segment.
# }
class CodeSectionDigest(univ.Sequence):
	componentType = namedtype.NamedTypes(
		namedtype.NamedType('offset', univ.Integer()),
		namedtype.NamedType('digestAlgorithm', univ.ObjectIdentifier()),
		namedtype.NamedType('digest', univ.OctetString()))


# CodeSegmentDigest ::= SEQUENCE {
#    offset				INTEGER -- TEXT segment's file offset in the signed binary
#    codeSectionDigests			SET OF CodeSectionDigests
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


class CPUSubType(univ.Enumerated):
	namedValues = namedval.NamedValues(
		('IMAGE_UNUSED', 0x0),
	)
	subtypeSpec = univ.Enumerated.subtypeSpec + \
				  constraint.SingleValueConstraint(0)


class ArchitectureDigest(univ.Sequence):
	componentType = namedtype.NamedTypes(
		namedtype.NamedType('cpuType', CPUType()),
		namedtype.NamedType('cpuSubType', CPUSubType()),
		namedtype.NamedType('CodeSegmentDigests', SetOfCodeSegmentDigest())
	)


# ApplicationDigest ::= SEQUENCE {
#   version    INTEGER
#   digests    SET OF ArchitectureDigest
# }
class SetOfArchitectureDigest(univ.SetOf):
	componentType = ArchitectureDigest()


class ApplicationDigest(univ.Sequence):
	componentType = namedtype.NamedTypes(
		namedtype.NamedType('version', univ.Integer()),
		namedtype.NamedType('digests', SetOfArchitectureDigest())
	)


def meets_requirements(items, requirements):
	for r in requirements:
		for n, v in r.items():
			if n not in items or items[n] != v: return False
	return True


# return total number of bytes read from items_in excluding leaves
# TODO: research replacing this with the python built-in struct module
def parse_items(stream, items_in, items_out):
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

			if not meets_requirements(items_out, requirements): continue

			# has sub-items based on length
			items_out[name] = stream.read(t)
			bits_read += bits
			total_bits_read += bits

			if len(item) == 4:
				bit_length = items_out[name] * 8

				if bit_length > 0:
					sub_read, sub_total_read = parse_items(stream, sub_items, items_out)
					bit_length -= sub_read
					total_bits_read += sub_total_read

					if bit_length > 0:
						items_out[item[3]] = stream.read('bits:' + str(bit_length))
						bits_read += bit_length
						total_bits_read += bit_length
		else:
			raise Exception("unrecognized item" + pprint.pformat(item))

	return bits_read, total_bits_read


# TODO: perhaps switch to pefile module when it officially supports python3
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
		self.items = dict()
		self.relocs = dict()

		_, self.bits_read = parse_items(stream, items, self.items)

		self.sectionName = self.items['Name'].decode('utf-8')
		self.offset = self.items['PointerToRawData']

COFF_DATA_DIRECTORY_TYPES = [
	"Export Table",
	"Import Table",
	"Resource Table",
	"Exception Table",
	"Certificate Tble",
	"Base Relocation Table",
	"Debug",
	"Architecture",
	"Global Ptr",
	"TLS Table",
	"Load Config Table",
	"Bound Import",
	"IAT",
	"Delay Import Descriptor",
	"CLR Runtime Header",
	"Reserved",
]


def chained_safe_get(obj, names, default=None):
	if obj is None: return default

	for n in names:
		if n in obj:
			obj = obj[n]
		else:
			return default

	return obj


class OptionalHeader:
	def __init__(self, stream, size):
		self.items = {}
		items = []

		if size:
			items += [
				('Magic', 'uintle:16'),
				('MajorLinkerVersion', 'uintle:8'),
				('MinorLinkerVersion', 'uintle:8'),
				('SizeOfCode', 'uintle:32'),
				('SizeOfInitializedData', 'uintle:32'),
				('SizeOfUninitializedData', 'uintle:32'),
				('AddressOfEntryPoint', 'uintle:32'),
				('BaseOfCode', 'uintle:32'),
			]

			_, self.bits_read = parse_items(stream, items, self.items)

			items = []
			if self.items['Magic'] == 0x10b:  # PE32
				items += [('BaseOfData', 'uintle:32')]

			address_size = 'uintle:64' if self.items['Magic'] == 0x20b else 'uintle:32'

			items += [
				('ImageBase', address_size),
				('SectionAlignment', 'uintle:32'),
				('FileAlignment', 'uintle:32'),
				('MajorOperatingSystemVersion', 'uintle:16'),
				('MinorOperatingSystemVersion', 'uintle:16'),
				('MajorImageVersion', 'uintle:16'),
				('MinorImageVersion', 'uintle:16'),
				('MajorSubsystemVersion', 'uintle:16'),
				('MinorSubsystemVersion', 'uintle:16'),
				('Win32VersionValue', 'uintle:32'),
				('SizeOfImage', 'uintle:32'),
				('SizeOfHeaders', 'uintle:32'),
				('CheckSum', 'uintle:32'),
				('Subsystem', 'uintle:16'),
				('DllCharacteristics', 'uintle:16'),
				('SizeOfStackReserve', address_size),
				('SizeOfStackCommit', address_size),
				('SizeOfHeapReserve', address_size),
				('SizeOfHeapCommit', address_size),
				('LoaderFlags', 'uintle:32'),
				('NumberOfRvaAndSizes', 'uintle:32'),
			]

		if size > 28:
			_, bits_read = parse_items(stream, items, self.items)
			self.bits_read += bits_read

		if 'NumberOfRvaAndSizes' in self.items:
			index = 0
			self.items['Data Directories'] = dict()
			while self.bits_read / 8 < size:
				d = self.items['Data Directories'][COFF_DATA_DIRECTORY_TYPES[index]] = dict()

				_, bits_read = parse_items(stream, [('VirtualAddress', 'uintle:32'), ('Size', 'uintle:32')], d)
				self.bits_read += bits_read
				index += 1


class COFFFileHeader:
	def __init__(self, stream):
		self.items = {}
		self.section_headers = []

		items = [
			('Machine', 'uintle:16'),
			('NumberOfSections', 'uintle:16'),
			('TimeDateStamp', 'uintle:32'),
			('PointerToSymbolTable', 'uintle:32'),
			('NumberOfSymbols', 'uintle:32'),
			('SizeOfOptionalHeader', 'uintle:16'),
			('Characteristics', 'uintle:16')
		]
		_, self.bits_read = parse_items(stream, items, self.items)

		self.OptionalHeader = OptionalHeader(stream, self.items['SizeOfOptionalHeader'])
		self.bits_read += self.OptionalHeader.bits_read

		# start reading section headers
		num_sections = self.items['NumberOfSections']

		while num_sections > 0 :
			section_header = SectionHeader(stream)
			self.bits_read += section_header.bits_read
			self.section_headers.append(section_header)
			num_sections -= 1

		self.section_headers.sort(key=lambda header: header.offset)

		# Read Relocations
		self.process_relocs(stream)

	def process_relocs(self, stream):
		reloc_table = chained_safe_get(self.OptionalHeader.items, ['Data Directories', 'Base Relocation Table'])
		if reloc_table is None: return

		orig_pos = stream.bitpos
		_, stream.bytepos = self.get_rva_section(reloc_table['VirtualAddress'])
		end_pos = stream.bitpos + reloc_table['Size'] * 8

		while stream.bitpos < end_pos:
			page_rva = stream.read('uintle:32')
			block_size = stream.read('uintle:32')

			for i in range(0, int((block_size - 8) / 2)):
				data = stream.read('uintle:16')
				typ = data >> 12
				offset = data & 0xFFF

				if offset == 0 and i > 0: continue

				assert(typ == IMAGE_REL_BASED_HIGHLOW or typ == IMAGE_REL_BASED_DIR64)

				cur_pos = stream.bitpos
				sh, value_bytepos = self.get_rva_section(page_rva + offset)
				stream.bytepos = value_bytepos
				value = stream.read('uintle:32' if typ == IMAGE_REL_BASED_HIGHLOW else 'uintle:64')

				# remove BaseAddress
				value -= self.OptionalHeader.items['ImageBase']

				bit_size = (4 if typ == IMAGE_REL_BASED_HIGHLOW else 8) * 8
				stream.overwrite(bitstring.BitArray(uint=value, length=bit_size), pos=value_bytepos * 8)
				stream.pos = cur_pos

		stream.bitpos = orig_pos

	def get_rva_section(self, rva):
		for sh in self.section_headers:
			if rva < sh.items['VirtualAddress'] or rva >= sh.items['VirtualAddress'] + sh.items['VirtualSize']:
				continue

			file_pointer = rva - sh.items['VirtualAddress'] + sh.items['PointerToRawData']
			return sh, file_pointer

		raise Exception('Could not match RVA to section')


def create_temp_file(suffix=""):
	fd, path = tempfile.mkstemp(suffix=suffix)
	os.close(fd)
	return path


class ExpandPath(argparse.Action):
	def __call__(self, parser, namespace, values, option_string=None):
		setattr(namespace, self.dest, os.path.abspath(os.path.expanduser(values)))


# this does a naming trick since windows doesn't allow multiple usernames for the same server
def get_password(service_name, user_name):
	try:
		import keyring

		# windows doesn't allow multiple usernames for the same server, argh
		if sys.platform == "win32":
			password = keyring.get_password(service_name + "-" + user_name, user_name)
		else:
			password = keyring.get_password(service_name, user_name)

		return password
	except:
	    # This allows for manual testing where you do not wish to cache the password on the system 
		print("Missing keyring module...getting password manually")

	return None


def openssl_cmd(app_args, args, password_in, password_out):
	password = get_password(app_args.password_service, app_args.password_user) if (password_in or password_out) else None
	env = None
	args = [app_args.openssl_path] + args

	if password is not None:
		env = os.environ.copy()
		env["COFF_PW"] = password

		if password_in: args += ["-passin", "env:COFF_PW"]
		if password_out: args += ["-passout", "env:COFF_PW", "-password", "env:COFF_PW"]

	p = subprocess.Popen(args, env=env)
	assert p.wait() == 0


def main():
	parser = argparse.ArgumentParser(description='PE/COFF Signer')
	parser.add_argument('-input', action=ExpandPath, required=True, help="File to parse.")
	parser.add_argument('-output', action=ExpandPath, required=True, help="File to write to.")
	parser.add_argument('-openssl_path', action=ExpandPath, help="Path to OpenSSL to create signed voucher")
	parser.add_argument('-signer_pfx', action=ExpandPath, help="Path to certificate to use to sign voucher.  Must contain full certificate chain.")
	parser.add_argument('-password_service', help="Name of Keyring/Wallet service/host")
	parser.add_argument('-password_user', help="Name of Keyring/Wallet user name")
	parser.add_argument('-verbose', action='store_true', help="Verbose output.")
	app_args = parser.parse_args()

	# to simplify relocation handling we use a mutable BitStream so we can remove
	# the BaseAddress from each relocation
	stream = bitstring.BitStream(filename=app_args.input)

	# find the COFF header.
	# skip forward past the MSDOS stub header to 0x3c.
	stream.bytepos = 0x3c

	# read 4 bytes, this is the file offset of the PE signature.
	pe_sig_offset = stream.read('uintle:32')
	stream.bytepos = pe_sig_offset

	# read 4 bytes, make sure it's a PE signature.
	signature = stream.read('uintle:32')
	if signature != 0x00004550:
		raise Exception("Invalid File")

	# after signature is the actual COFF file header.
	coff_header = COFFFileHeader(stream)

	arch_digest = ArchitectureDigest()
	if coff_header.items['Machine'] == 0x14c:
		arch_digest.setComponentByName('cpuType', CPUType('IMAGE_FILE_MACHINE_I386'))
	elif coff_header.items['Machine'] == 0x8664:
		arch_digest.setComponentByName('cpuType', CPUType('IMAGE_FILE_MACHINE_AMD64'))

	arch_digest.setComponentByName('cpuSubType', CPUSubType('IMAGE_UNUSED'))

	text_section_headers = list(filter(lambda x: (x.items['Characteristics'] & IMAGE_SCN_MEM_EXECUTE) == IMAGE_SCN_MEM_EXECUTE, coff_header.section_headers))

	code_segment_digests = SetOfCodeSegmentDigest()
	code_segment_idx = 0
	for code_sect_header in text_section_headers:
		stream.bytepos = code_sect_header.offset
		code_sect_bytes = stream.read('bytes:' + str(code_sect_header.items['VirtualSize']))

		digester = hashlib.sha256()
		digester.update(code_sect_bytes)
		digest = digester.digest()

		# with open('segment_' + str(code_sect_header.offset) + ".bin", 'wb') as f:
		#   f.write(code_sect_bytes)

		code_section_digest = CodeSectionDigest()
		code_section_digest.setComponentByName('offset', code_sect_header.offset)
		code_section_digest.setComponentByName('digestAlgorithm', univ.ObjectIdentifier('2.16.840.1.101.3.4.2.1'))
		code_section_digest.setComponentByName('digest', univ.OctetString(digest))

		set_of_digest = SetOfCodeSectionDigest()
		set_of_digest.setComponentByPosition(0, code_section_digest)

		codeSegmentDigest = CodeSegmentDigest()
		codeSegmentDigest.setComponentByName('offset', code_sect_header.offset)
		codeSegmentDigest.setComponentByName('codeSectionDigests', set_of_digest)

		code_segment_digests.setComponentByPosition(code_segment_idx, codeSegmentDigest)
		code_segment_idx += 1

	arch_digest.setComponentByName('CodeSegmentDigests', code_segment_digests)

	setOfArchDigests = SetOfArchitectureDigest()
	setOfArchDigests.setComponentByPosition(0, arch_digest)

	appDigest = ApplicationDigest()

	appDigest.setComponentByName('version', 1)
	appDigest.setComponentByName('digests', setOfArchDigests)

	binaryDigest = der_encoder.encode(appDigest)

	with open(app_args.output, 'wb') as f:
		f.write(binaryDigest)

	# sign with openssl if specified
	if app_args.openssl_path is not None:
		assert app_args.signer_pfx is not None

		out_base, out_ext = os.path.splitext(app_args.output)
		signed_path = out_base + ".signed" + out_ext

		# http://stackoverflow.com/questions/12507277/how-to-fix-unable-to-write-random-state-in-openssl
		temp_files = []
		if sys.platform == "win32" and "RANDFILE" not in os.environ:
			temp_file = create_temp_file()
			temp_files += [temp_file]
			os.environ["RANDFILE"] = temp_file

		try:
			# create PEM from PFX
			pfx_pem_path = create_temp_file(".pem")
			temp_files += [pfx_pem_path]
			print("Extracting PEM from PFX to:" + pfx_pem_path)
			openssl_cmd(app_args, ["pkcs12", "-in", app_args.signer_pfx, "-out", pfx_pem_path], True, True)

			# extract CA certs
			pfx_cert_path = create_temp_file(".cert")
			temp_files += [pfx_cert_path]
			print("Extracting cert from PFX to:" + pfx_cert_path)
			openssl_cmd(app_args, ["pkcs12", "-in", app_args.signer_pfx, "-cacerts", "-nokeys", "-out", pfx_cert_path], True, False)

			# we embed the public keychain for client validation
			openssl_cmd(app_args, ["cms", "-sign", "-nodetach", "-md", "sha256", "-binary", "-in", app_args.output, "-outform", "der", "-out", signed_path, "-signer", pfx_pem_path, "-certfile", pfx_cert_path], True, False)
		finally:
			for t in temp_files:
				if "RANDFILE" in os.environ and t == os.environ["RANDFILE"]:
					del os.environ["RANDFILE"]
				os.unlink(t)

if __name__ == '__main__':
	main()
