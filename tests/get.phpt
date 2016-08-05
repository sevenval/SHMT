--TEST--
SHMT: Test SHMT::create() method errors
--FILE--
<?php
$sTmpFile = tempnam(sys_get_temp_dir(), 'shmt_');

$aData = ['foo' => 'bar', 'bar' => 'baz', 666 => 'evil', 'evil' => 666, 'öüäas@#+&' => 'ÄÖ;§"*+@^^'];

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

var_dump(SHMT::create($sTmpFile, $aData));

try {
	$oSHMT = new SHMT($sTmpFile);
} catch (Exception $e) {
	var_dump($e->getMessage());
}

var_dump($oSHMT->get('foo'));
var_dump($oSHMT->get(666));
var_dump($oSHMT->get('evil'));
var_dump($oSHMT->get('adsf'));
var_dump($oSHMT->get('öüäas@#+&'));

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

unlink($sTmpFile);
?>
===DONE===
--EXPECT--
bool(true)
string(3) "bar"
string(4) "evil"
string(3) "666"
NULL
string(13) "ÄÖ;§"*+@^^"
===DONE===
