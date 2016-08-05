--TEST--
SHMT: Test SHMT constructor
--FILE--
<?php
$sTmpFile = tempnam(sys_get_temp_dir(), 'shmt_');

$aData = ['foo' => 'bar', 'bar' => 'baz', 666 => 'evil', 'evil' => 666];

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

var_dump(SHMT::create($sTmpFile, $aData));

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

try {
	new SHMT($sTmpFile);
} catch (Exception $e) {
	var_dump($e->getMessage());
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

unlink($sTmpFile);
?>
===DONE===
--EXPECT--
bool(true)
===DONE===
