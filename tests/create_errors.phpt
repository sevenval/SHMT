--TEST--
SHMT: Test SHMT::create() method errors
--FILE--
<?php
$sTmpFile = tempnam(sys_get_temp_dir(), 'shmt_');

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

try {
	SHMT::create($sTmpFile, []);
} catch (Exception $e) {
	var_dump($e->getMessage());
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

chmod($sTmpFile, 0);

try {
	SHMT::create($sTmpFile, ['x' => 'y']);
} catch (Exception $e) {
	var_dump($e->getMessage());
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

unlink($sTmpFile);
?>
===DONE===
--EXPECT--
string(28) "SHMT: Empty data array given"
string(26) "SHMT: Cannot open the file"
===DONE===
