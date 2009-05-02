--TEST--
Test that an error occurs when passing the maximal nesting level
--SKIPIF--
<?php if (!extension_loaded("jsonreader")) print "skip"; ?>
--STDIN--
[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[
[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[
[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[
	"Some Value"
]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]
]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]
]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]
--FILE--
<?php
$rdr = new JSONReader();
$rdr->open('php://stdin');
while ($rdr->read()) {
  if ($rdr->tokenType & JSONReader::VALUE) echo "FOUND IT!\n";
}
$rdr->close();
?>
--EXPECTF--
Warning: JSONReader::read(): parser error [#%d]: maximal nesting level of 64 reached in %s on line %d

