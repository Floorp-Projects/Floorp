.. _mozilla_projects_nss_nss_tech_notes_nss_tech_note1:

nss tech note1
==============

.. _how_to_use_the_nss_asn.1_and_quickder_decoders:

`How to use the NSS ASN.1 and QuickDER decoders <#how_to_use_the_nss_asn.1_and_quickder_decoders>`__
----------------------------------------------------------------------------------------------------

.. container::

.. _nss_technical_note_1:

`NSS Technical Note: 1 <#nss_technical_note_1>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSS 3.6 contains several decoders for ASN.1 and DER.Two of them are extensively used and are part
   of the public NSS API :

   #. The "classic" ASN.1 decoder, written by Lisa Repka . This was written to be a generic decoder,
      that includes both DER (Distinguished Encoding Rules) and BER (Basic Encoding Rules).† It
      handles both streaming and non-streaming input.
   #. The "QuickDER" decoder, written by Julien Pierre for NSS 3.6 . This decoder was written when
      performance issues were discovered with the classic decoder. It can only decode DER .† It does
      not handle streaming input, and requires that all input be present before beginning to decode.

   Despite their differences, the two decoders have a lot in common. QuickDER was written to be as
   compatible as possible with the classic decoder, in order to ease migration to it in areas of
   critical performance bottlenecks. For this reason, I will first describe all the common
   functionality of the two decoders, before outlining their differences.
   The main non-streaming APIs for these two decoders have an identical prototype :

   -  SECStatus SEC_ASN1DecodeItem(PRArenaPool \*pool, void \*dest, const SEC_ASN1Template \*t,
      SECItem \*item);
   -  SECStatus SEC_QuickDERDecodeItem(PRArenaPool\* arena, void\* dest, const SEC_ASN1Template\*
      templateEntry, SECItem\* src);

   Here is a description of the arguments :

   -  *SECItem\* src*\ † is a structure containing a pointer to the binary data to be decoded, as
      well as its size.
   -  *const SEC_ASN1Template\* templateEntry* is a pointer to one or more `decoder
      templates <#templates>`__. The number of required templates is determined by the type of the
      first template.When multiple templates are required, the pointer must point to a
      NULL-terminated array of templates. The syntax of these templates is identical for both
      decoders, except where noted. A "NULL Template" is a template that is all zeros, having a zero
      kind.† The term "NULL-terminated array", as used throughout this document, means an array of
      templates, the last of which is a NULL template.
   -  *void\* dest* is a pointer to the target area. This is where the decoder stores its output.
      The type is undefined as it is completely dependent on the content of the decoder templates.†
      This typically points to a struct that is described (or partially described) by the templates.
   -  *PRArenaPool\* arena* is a pointer to an NSPR arena pool. This is the arena pool from which
      the decoder will allocate memory as needed.

   Decoder templates :
   The SEC_ASN1Template structure tells the decoder what to do with the input data. This structure
   contains four fields :

   -  *kind* . This 32-bit field tells the decoder what to do with a particular component within the
      input data. It is made of two parts : the lower byte, which can contain `ASN.1
      tags <#asn.1_tags>`__, and the upper 3 bytes, which can contain `decoder
      modifiers <#decoder_modifiers>`__. If only an ASN.1 tag is specified without a modifier, then
      the decoder will enforce the presence of a component of that type, and fail if it does not
      match. If kind is an ASN.1 SEQUENCE tag (SEC_ASN1_SEQUENCE), then you must specify additional
      templates in a NULL-terminated array to define the content of the of the ASN.1 SEQUENCE. If
      kind is the SEC_ASN1_CHOICE modifier, you must also specify additional templates in a NULL
      terminated array to list the various possible types that this component can have. In all other
      cases, only the first template structure passed to the decoder will be considered, even if
      additonal templates are passed in an array. When only one template is needed, you do not need
      a NULL template to terminate the array.
   -  *offset*\ † . This field does not apply to all template types. It is only needed if the
      template instructs the decoder to save some data, such as for primitive component types, or
      for some modifiers where noted.When needed, it tells the decoder where in the target data to
      save the current component. It is normally relative to the dest argument passed to the
      decoder. If templates are nested, the offset applies to the location of the current component
      within the target component, typically the decoded SEQUENCE.
   -  *sub*\ † . This field does not apply to all template types. If kind contains the
      SEC_ASN1_INLINE or SEC_ASN1_POINTER modifiers, then it must point to the required subtemplate.
      If kind contains the SEC_ASN1_XTRN or SEC_ASN1_DYNAMIC modifiers, this is a pointer to a
      callback function that will dynamically return the required subtemplate.
   -  *size*\ † . This field does not apply to all template types. It is only required for
      dynamically allocating memory for the structure if the template is being included from an
      ASN.1 SEQUENCE or SEQUENCE OF, or if dynamic allocation was requested from the parent template
      using the SEC_ASN1_POINTER modifier

   Here is a description of the various tags and modifiers that apply to the kind field.
   *ASN.1 tags*

   | ASN.1 tags are specified in the lower byte of the kind field of the template, as noted above.
   | The following is not an attempt to explain ASN.1 tags or their purposes. Rather, the goal here
     is to explain what type of tags the decoder supports and which macros should be used when
     defining tags in decoder templates. It should be noted that we only support an older
     specification of ASN.1; multibyte tags are not currently supported.

   The 8-bit ASN.1 tags that we support are made of three parts :

   #. The ASN.1 component class type. It is specified in the upper 2 tag bits (number 6 and 7).
      There are four classes of ASN.1 tags : universal, application-specific, context-specific, and
      private. You can specify the class of the tag using the macros SEC_ASN1_UNIVERSAL,
      SEC_ASN1_APPLICATION, SEC_ASN1_CONTEXT_SPECIFIC and SEC_ASN1_PRIVATE. Universal is the default
      tag class and does not have to be specified, as the value of the class type is zero.

   #. The method type : whether the component type is constructed or primitive. This information is
      stored in the next lowest tag bit (number 5). You can use the macro SEC_ASN1_CONSTRUCTED for a
      constructed component type. A SEC_ASN1_PRIMITIVE macro is also provided, but does not need to
      be included as it is zero.

   #. | The tag number. It is stored in the lower 5 tag bits (number 0 through 4). The ASN.1
        standard only defines tag numbers in the universal class. If you are using a tag of a
        different classes, you can define your own tag number macros or specify the tag value within
        the template definition. The following macros are provided for tag numbers within the
        universal class :
      | SEC_ASN1_BOOLEAN, SEC_ASN1_INTEGER, SEC_ASN1_BIT_STRING, SEC_ASN1_OCTET_STRING,
        SEC_ASN1_NULL, SEC_ASN1_OBJECT_ID, SEC_ASN1_OBJECT_DESCRIPTOR,† SEC_ASN1_REAL,
        SEC_ASN1_ENUMERATED, SEC_ASN1_EMBEDDED_PDV, SEC_ASN1_UTF8_STRING, SEC_ASN1_SEQUENCE,
        SEC_ASN1_SET, SEC_ASN1_NUMERIC_STRING, SEC_ASN1_PRINTABLE_STRING, SEC_ASN1_T61_STRING,
        SEC_ASN1_TELETEX_STRING, SEC_ASN1_T61_STRING, SEC_ASN1_VIDEOTEX_STRING, SEC_ASN1_IA5_STRING,
        SEC_ASN1_UTC_TIME, SEC_ASN1_GENERALIZED_TIME, SEC_ASN1_GRAPHIC_STRING,
        SEC_ASN1_VISIBLE_STRING, SEC_ASN1_GENERAL_STRING, SEC_ASN1_UNIVERSAL_STRING,
        SEC_ASN1_BMP_STRING

      Note that for SEC_ASN1_SET and SEC_ASN1_SEQUENCE types, you must also include the method type
      macro SEC_ASN1_CONSTRUCTED to construct a fully valid tag, as defined by the ASN.1 standard .

   *Decoder modifiers :*
   These modifiers are also specified in the kind field of the template structure. All the values
   are in the 9 - 31 bit range.

   -  *SEC_ASN1_OPTIONAL*: tells the decoder that this component is optional. If the component in
      the input data does not match this template, the decoder will continue processing the input
      data using the next available template.
   -  *SEC_ASN1_EXPLICIT*: tells the decoder that explicit tagging is being used. This is always a
      constructed type. It requires a subtemplate defining the types of the data within.
   -  *SEC_ASN1_ANY*: allows the decoder to match this template with any component type, regardless
      of the tag in the input data. If used in conjunction with SEC_ASN1_OPTIONAL as part of a
      sequence, this must be the last template in the template array.
   -  *SEC_ASN1_INLINE*: recurse into the specified subtemplate to continue processing. This is
      typically used for SEC_ASN1_SEQUENCE or SEC_ASN1_CHOICE definitions, which always need to be
      the first template in a template array of their own.
   -  *SEC_ASN1_POINTER*: similar to SEC_ASN1_INLINE, except that the memory in the target will be
      allocated dynamically and a pointer to the dynamically allocated memory will be stored in the
      *dest* struct at the *offset*. This requires that the subtemplate contains a non-zero size
      field.
   -  *SEC_ASN1_GROUP*: can only be used in conjunction with a SEC_ASN1_SET or SEC_ASN1_SEQUENCE. It
      tells the decoder that the component is an ASN.1 SET OF or SEQUENCE OF respectively. You can
      also use the macros SEC_ASN1_SET_OF and SEC_ASN1_SEQUENCE_OF which define both the tag number
      and this modifier (but still need the method type, this may be a bug).
   -  *SEC_ASN1_DYNAMIC* or *SEC_ASN1_XTRN* : specifies that the component format is defined in a
      dynamic subtemplate. There is no difference between the two macros. The sub field of the
      template points to a callback function of type SEC_ASN1TemplateChooser that returns the
      subtemplate depending on the component data.
   -  *SEC_ASN1_SKIP*: specifies that the decoder should skip decoding of the component.
      SEC_ASN1DecodeItem can only skip required components and will assert if you try to skip an
      OPTIONAL component. SEC_QuickDERDecodeItem supports skipping the decoding of OPTIONAL
      components if you define the tag of the component in the template
   -  *SEC_ASN1_INNER*: recurse into the component and saves its content, without the surrounding
      ASN.1 tag and length
   -  *SEC_ASN1_SAVE*: saves the component data, but does not proceed to the next component if
      within a SEQUENCE template array. This means the next template will reprocess the same
      component.
   -  *SEC_ASN1_SKIP_REST*: abort the decoding. This is used in a template array within a SEQUENCE,
      if you don't care about the fields at the end of it. SEC_ASN1DecodeItem only supports this
      modifier in the top-level template. SEC_QuickDERDecodeItem allows it at any nested sublevel.
   -  *SEC_ASN1_CHOICE*: allows decoding of components that are of variable type. This must be the
      first template in a NULL-terminated array. The offset parameter specifies where to store the
      type identifier in the target data . Subsequent templates specify a custom identifier for each
      possible component type in the size parameter .
   -  *SEC_ASN1_DEBUG_BREAK*: makes the decoder assert when processing the template. This option is
      only supported with SEC_QuickDERDecodeItem . It is useful to debug your templates or when
      writing new templates if they don't work.

   | 
   | *Differences between SEC_ASN1DecodeItem and SEC_QuickDERDecodeItem*

   #. The arena argument is required to be non-NULL for SEC_QuickDERDecodeItem . With
      SEC_ASN1DecodeItem, it can be NULL, and if so, the decoder will allocate from the heap using
      PR_Malloc . However, this usage is strongly discouraged and we recommend that you always use
      an arena pool even with SEC_ASN1DecodeItem. See `bug
      175163 <http://bugzilla.mozilla.org/show_bug.cgi?id=175163>`__ for more information about the
      reason for this recommendation.
   #. SEC_ASN1DecodeItem will make a copy of the input data into the decoded target as needed, while
      SEC_QuickDERDecodeItem will generate output with pointers into the input. This means that if
      you use SEC_QuickDERDecodeItem, you must always be careful not to free the input as long as
      you intend to use the decoded structure. Ideally, you should allocate the input data out of
      the same arena that you are passing to the decoder. This will allow you to free both the input
      data and the decoded data at once when freeing the arena.
   #. SEC_ASN1DecodeItem can decode both BER and DER data, while SEC_QuickDERDecodeItem can only
      decode DER data.
   #. SEC_QuickDERDecodeItem does not support streaming data. This feature will most likely never be
      added, as this decoder gets most of its extra speed from not making a copy of the input data,
      which would be required when streaming.
   #. SEC_QuickDERDecodeItem supports SEC_ASN1_OPTIONAL together with SEC_ASN1_SKIP
   #. SEC_ASN1_DEBUG_BREAK is not supported by SEC_ASN1DecodeItem