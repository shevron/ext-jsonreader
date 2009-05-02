--TEST--
Test that we can modify the max nesting level through a construcot attribute
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
$rdr = new JSONReader(array(
  JSONReader::ATTR_MAX_DEPTH => 100
));
$rdr->open('php://stdin');
while ($rdr->read()) {
  if ($rdr->tokenType & JSONReader::VALUE) echo "FOUND IT at {$rdr->currentDepth}!\n";
}
$rdr->close();
?>
--EXPECTF--
FOUND IT at 96!

