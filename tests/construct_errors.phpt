--TEST--
SHMT: Test SHMT constructor errors
--FILE--
<?php
$sTmpFile = tempnam(sys_get_temp_dir(), 'shmt_');

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

try {
	new SHMT($sTmpFile . 'foo');
} catch (Exception $e) {
	var_dump($e->getMessage());
}

try {
	new SHMT($sTmpFile);
} catch (Exception $e) {
	var_dump($e->getMessage());
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

SHMT::create($sTmpFile, ['x' => 'y']);
$sContent = file_get_contents($sTmpFile);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

file_put_contents($sTmpFile, 'XXX');

try {
	new SHMT($sTmpFile);
} catch (Exception $e) {
	var_dump($e->getMessage());
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

file_put_contents($sTmpFile, substr($sContent, 0, strlen($sContent) - 1));

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
string(26) "SHMT: Cannot open the file"
string(28) "SHMT: Cannot read the header"
string(28) "SHMT: Cannot read the header"
string(32) "SHMT: Corrupt SHMT file detected"
===DONE===
