--TEST--
Test that a warning is thrown in case of a JSON parse error
--SKIPIF--
<?php if (!extension_loaded("jsonreader")) print "skip"; ?>
--STDIN--
{ 12: "<= Numeric keys are not valid in JSON!" }
--FILE--
<?php
$rdr = new JSONReader();
$rdr->open('php://stdin');
while ($rdr->read()) {
  echo "- $rdr->tokenType\n";
}
$rdr->close();
?>
--EXPECTF--
- %d

Warning: JSONReader::read(): parser error [#%d]: [%s] Unexpected character in input: '1' (0x31) in %s on line %d

