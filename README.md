
# SHMT (Static Hash Map Table)

SHMT is an implementation of a very fast key-value read-only
hash map table for PHP.


## Features

SHMT:
* is written in C.
* creates and uses its own memory-mapped binary file. This
  enable it to cache and share the loaded pages of the
  file across multiple processes.
* has an extremely fast initial load, regardless of the
  size of the binary file.
* internally implements a "perfect hash function" and
  guarantees O(1) lookup time in the worst cases.
* internally uses the very fast "MurmurHash3" hashing
  algorithm.
* doesn't require any external libraries.
* is PHP 7 ready.


## Limitations

* Supported maximum number of data array elements is
  2^26 (67.108.864 on x32 systems) and 2^31 (2.147.483.648
  on x64 systems).
* The data array keys and values are always cast to string.


## PHP Class

```
public static boolean SHMT::create(string $filename, array $array)
```

* Creates a SHMT from the `$array` and writes it into the `$filename`
* Returns `true` on success
* Throws exceptions on errors


```
public SHMT::__construct(string $filename)
```

* Constructs a new SHMT object and maps the SHMT file `$filename` into memory
* Throws exceptions on errors


```
public (string|null) SHMT::get(string $string)
```

* Attempts to find the value stored under the key `$string`
* Returns the value if the key `$string` exists, otherwise `null`


### Example

Create a SHMT:

```
$array = [
	'str_key' => 'test',
	123456789 => 12345
	-1 => ''
];

SHMT::create($filename = 'map.shmt', $array);
```

Read from a SHMT:

```
$shmt = new SHMT($filename);

echo $shmt->get('str_key');    // string(4) "test"
echo $shmt->get(123456789);    // string(5) "12345"
echo $shmt->get('123456789');  // string(5) "12345"
echo $shmt->get(-1);           // string(0) ""
echo $shmt->get('abc_xyz');    // NULL
```
