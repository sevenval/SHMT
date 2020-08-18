--TEST--
SHMT: Test SHMT::keys() method errors
--FILE--
<?php
$sTmpFile = tempnam(sys_get_temp_dir(), 'shmt_');

/* the keys contain collisions*/
$aData = ['foo' => 'bar', 'bar' => 'baz', 666 => 'evil', 'evil' => 666, 'öüäas@#+&' => 'ÄÖ;§"*+@^^', 123456789 => 12345, -1 => 112];

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

var_dump(SHMT::create($sTmpFile, $aData));

try {
	$oSHMT = new SHMT($sTmpFile);
} catch (Exception $e) {
	var_dump($e->getMessage());
}

var_dump($oSHMT->keys());

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

unlink($sTmpFile);
?>
===DONE===
--EXPECT--
bool(true)
array(7) {
  [0]=>
  string(3) "foo"
  [1]=>
  string(3) "666"
  [2]=>
  string(9) "123456789"
  [3]=>
  string(2) "-1"
  [4]=>
  string(4) "evil"
  [5]=>
  string(3) "bar"
  [6]=>
  string(12) "öüäas@#+&"
}
===DONE===
