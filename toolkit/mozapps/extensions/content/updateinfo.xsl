<?xml version="1.0" encoding="UTF-8"?>
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<xsl:stylesheet version="1.0" xmlns:xhtml="http://www.w3.org/1999/xhtml"
                              xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <!-- Any elements not otherwise specified will be stripped but the contents
       will be displayed. All attributes are stripped from copied elements. -->

  <!-- Block these elements and their contents -->
  <xsl:template match="xhtml:head|xhtml:script|xhtml:style">
  </xsl:template>

  <!-- Allowable styling elements -->
  <xsl:template match="xhtml:b|xhtml:i|xhtml:em|xhtml:strong|xhtml:u|xhtml:q|xhtml:sub|xhtml:sup|xhtml:code">
    <xsl:copy><xsl:apply-templates/></xsl:copy>
  </xsl:template>

  <!-- Allowable block formatting elements -->
  <xsl:template match="xhtml:h1|xhtml:h2|xhtml:h3|xhtml:p|xhtml:div|xhtml:blockquote|xhtml:pre">
    <xsl:copy><xsl:apply-templates/></xsl:copy>
  </xsl:template>

  <!-- Allowable list formatting elements -->
  <xsl:template match="xhtml:ul|xhtml:ol|xhtml:li|xhtml:dl|xhtml:dt|xhtml:dd">
    <xsl:copy><xsl:apply-templates/></xsl:copy>
  </xsl:template>

  <!-- These elements are copied and their contents dropped -->
  <xsl:template match="xhtml:br|xhtml:hr">
    <xsl:copy/>
  </xsl:template>

  <!-- The root document -->
  <xsl:template match="/">
    <xhtml:body><xsl:apply-templates/></xhtml:body>
  </xsl:template>
  
</xsl:stylesheet>
