<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:template match="/">
<html>
<head>
<title>decom test results</title>
<style type="text/css">
body {
  background-color: #CCCCCC;
  font-family: verdana, sans-serif;
}
table
{
  border-collapse: collapse;
}
table, td, th
{
  border: 1px solid black;
}
.okay {
  color: #00AA00;
}
.fail {
  color: #FF0000;
}
.info {
  color: #000000;
}
.skip {
  color: #AA8000;
}
</style>
</head>
<body>
  <h2>decom test results</h2>
  <table>
    <tr bgcolor="#9acd32">
      <th>Test case</th>
      <th>Result</th>
    </tr>
    <xsl:for-each select="tests/module/test">
    <tr>
      <td><xsl:value-of select="case"/><br /><xsl:value-of select="error"/></td>
      <td class="{result[@type]}"><xsl:value-of select="result"/></td>
    </tr>
    </xsl:for-each>
  </table>
</body>
</html>
</xsl:template>

<xsl:template match="tests/module/test/result/type">
  <span class="okay"><xsl:value-of select="result" /></span>
</xsl:template>

</xsl:stylesheet>
