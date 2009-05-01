--TEST--
Parse some integers and floating-point numbers
--SKIPIF--
<?php if (!extension_loaded("jsonreader")) print "skip"; ?>
--STDIN--
[
  12345, 66.666, -444, 123544009,
  12.5, 10011, 12.5e-1, 0.0005 ]
--FILE--
<?php
$jr = new JSONReader;

// Open manually and pass an existing file pointer
$in = fopen('php://stdin', 'r');
$jr->open($in);

$ints = array();
$floats = array();
while ($jr->read()) {
  if ($jr->tokenType & JSONReader::NUMBER) {
    switch($jr->tokenType) {
      case JSONReader::INT: 
        $ints[] = $jr->value;
        break;

      case JSONReader::FLOAT:
        $floats[] = $jr->value;
        break;

      default:
        echo "Unexpected type: ";
        var_dump($jr->value);
        break;
    }
  }
}
var_dump($ints, $floats);
?>
--EXPECT--
array(4) {
  [0]=>
  int(12345)
  [1]=>
  int(-444)
  [2]=>
  int(123544009)
  [3]=>
  int(10011)
}
array(4) {
  [0]=>
  float(66.666)
  [1]=>
  float(12.5)
  [2]=>
  float(1.25)
  [3]=>
  float(0.0005)
}

