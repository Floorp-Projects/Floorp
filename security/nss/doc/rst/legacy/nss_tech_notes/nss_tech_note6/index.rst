.. _mozilla_projects_nss_nss_tech_notes_nss_tech_note6:

nss tech note6
==============

.. _nss_.chk_files_for_the_fips_140_mode:

`NSS .chk Files for the FIPS 140 Mode
 <#nss_.chk_files_for_the_fips_140_mode>`__
-------------------------------------------

.. container::

.. _nss_technical_note_6:

`NSS Technical Note: 6 <#nss_technical_note_6>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   In NSS 3.8, we added checksum files required for the NSS softoken to operate in FIPS 140 mode.
   The new checksum file is called libsoftokn3.chk on Unix/Linux and softokn3.chk on Windows.  It
   must be put in the same directory as the NSS libraries. The libsoftokn3.chk/softokn3.chk file
   contains a checksum for the softoken. When in FIPS 140 mode, the softoken is required to compute
   its checksum and compare it with the value in libsoftokn3.chk/softokn3.chk.
    
   The following applies to NSS 3.8 through 3.10 :

   | On 32-bit Solaris SPARC (i.e., not x86, and not 64-bit SPARC) and 32-bit HP-UX PA-RISC (i.e.,
     not Itanium, and not 64-bit PA-RISC), there are two more .chk files: libfreebl_pure32_3.chk and
     libfreebl_hybrid_3.chk.
   |  

   The following applies to NSS 3.11 :

   The low-level freebl cryptographic code has been separated from softoken on all platforms. Even
   on platforms for which there is only one implementation of freebl, there is now a separate freebl
   shared library. The freebl library implements a private interface internal to NSS.

   -  On 32-bit Windows and 32-bit OS/2, this shared library is called freebl3.dll, and the
      corresponding .chk file is called freebl3.chk .
   -  On 32-bit Solaris x86, 64-bit Solaris x64 (AMD64), 32-bit Linux x86, 64-bit Linux x86-64,
      32-bit AIX and 64-bit AIX, this shared library is called libfreebl3.so, and the corresponding
      .chk file is called libfreebl3.chk .
   -  On the 32-bit Solaris SPARC architecture, there are 3 freebl libraries :

      -  libfreebl_32int64_3.so for UltraSparc T1 CPUs, with a corresponding libfreebl_32int64_3.chk
      -  libfreebl_32fpu_3.so for other UltraSparc CPUs, with a corresponding libfreebl_32fpu_3.chk
      -  libfreebl_32int_3.so for SPARC V8 CPUs, with a corresponding libfreebl_32int_3.chk

   -  On the 64-bit Solaris SPARC architecture, there are 2 freebl libraries :

      -  libfreebl_64int_3.so for UltraSparc T1 CPUs, with a corresponding libfreebl_64int_3.chk
      -  libfreebl_64fpu_3.so for other UltraSparc CPUs, with a corresponding libfreebl_64fpu_3.chk

   -  On the 32-bit HP-UX PA-RISC architecture, there are 2 freebl libraries :

      -  libfreebl_32fpu_3.sl for CPUs that do multiply operations faster in floating point, with a
         corresponding libfreebl_32fpu_3.chk
      -  libfreebl_32int_3.sl for other PA-RISC CPUs, with a corresponding libfreebl_32int_3.chk

   -  On the 64-bit HP-UX PA-RISC architecture, there is only one freebl library, called
      libfreebl3.sl, with a corresponding libfreebl3.chk

   Applications should always use NSS binaries that are the output of the regular NSS build process.
   If your application modifies NSS binaries for any reason after they were built, then :

   -  The FIPS 140 mode of operation will no longer work, because the chk files will no longer match
      the softoken and freebl shared libraries .
   -  The softoken and freebl binaries won't match any NSS binaries that may have been submitted to
      NIST for validation, and thus may not be verified as being the actual FIPS 140 validated
      cryptographic module . The only way to meet this requirement is for your application never to
      modify the NSS binaries.
   -  Any offer of binary support that may have been previously made to you by NSS developers is
      null and void.

   If your build process modifies NSS libraries in any way (for example, to strip the symbols), it
   should consider not doing so for the reasons cited above. If you still decide to make unsupported
   changes, you can allow the softoken to come up in FIPS 140 mode of operation by regenerating the
   .chk files yourself.  The tool to do that is called shlibsign.  It is released as part of the NSS
   binary distributions.
   If your build process does not modify NSS shared libraries, you can just use the .chk files in
   the NSS binary distributions.
    
   So you have two options.
    
   1. Do not modify NSS libraries in your build process. Specifically, do not modify libsoftokn3.so,
   libsoftokn3.sl, softokn3.dll, libfreebl_pure32_3.so, libfreebl_pure32_3.sl,
   libfreebl_hybrid_3.so,libfreebl_hybrid_3.sl, libfreebl3.so, libfreebl3.sl, freebl3.dll,
   libfreebl_32int64_3.so, libfreebl_32int_3.so, libfreebl_32fpu_3.so, libfreebl_64int_3.so,
   libfreebl_64fpu_3.so, libfreebl_32int_3.sl, libfreebl_32fpu_3.sl; or
    
   2. Use shlibsign to regenerate the .chk files.  For example, on 32-bit Solaris SPARC for NSS
   3.11, say
    
   shlibsign -v -i libsoftokn3.so
   shlibsign -v -i libfreebl_32int64_3.so
   shlibsign -v -i libfreebl_32fpu_3.so
   shlibsign -v -i libfreebl_32int_3.so
    
   (You need to set LD_LIBRARY_PATH appropriately and specify the correct pathnames of the
   libraries.)
    
   Option 1 is simpler and highly preferred.