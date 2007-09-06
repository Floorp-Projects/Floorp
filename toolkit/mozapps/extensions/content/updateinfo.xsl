<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xhtml="http://www.w3.org/1999/xhtml"
                              xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:template match="text()">
  </xsl:template>
  
  <xsl:template match="text()" mode="list">
  </xsl:template>
  
  <xsl:template match="text()" mode="text">
    <xsl:value-of select="."/>
  </xsl:template>
  
  <xsl:template match="xhtml:script" mode="text">
  </xsl:template>
  
  <xsl:template match="xhtml:b|xhtml:i|xhtml:em|xhtml:strong" mode="text">
    <xsl:copy><xsl:apply-templates mode="text"/></xsl:copy>
  </xsl:template>
  
  <xsl:template match="xhtml:h1|xhtml:h2|xhtml:h3|xhtml:p">
    <xsl:copy><xsl:apply-templates mode="text"/></xsl:copy>
  </xsl:template>

  <xsl:template match="xhtml:li" mode="list">
    <xsl:copy><xsl:apply-templates mode="text"/></xsl:copy>
  </xsl:template>

  <xsl:template match="xhtml:ul|xhtml:ol">
    <xsl:copy><xsl:apply-templates mode="list"/></xsl:copy>
  </xsl:template>
  
  <xsl:template match="/">
    <xhtml:body><xsl:apply-templates/></xhtml:body>
  </xsl:template>
  
</xsl:stylesheet>
