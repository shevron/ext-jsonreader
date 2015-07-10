JSONReader - a JSON stream pull parser for PHP
===============================================================================

Introduction
------------
The jsonreader extension provides the JSONReader class - a forward-only, memory
efficient pull parser for JSON streams. The API is modeled after the XMLReader 
extension API (despite the very unrelated backend technologies) and the 
extension is based on the libvktor library which is shipped with the extension.

jsonreader currently works with PHP 5.x only. 


Why another JSON extension?
---------------------------
The existing ext/json which is shipped with PHP is very convenient and simple 
to use - but it is inefficient when working with large ammounts of JSON data,
as it requires reading the entire JSON data into memory (e.g. using 
file_get_contents()) and then converting it into a PHP variable at once - for
large data sets, this takes up a lot of memory. 

JSONReader is designed for memory efficiency - it works on streams and can 
read JSON data from any PHP stream without loading the entire data into memory.
It also allows the developer to extract specific values from a JSON stream 
without decoding and loading all data into memory. 

In short, JSONReader should be preferred over ext/json in situations where 
memory usage is a concern or when the size of the JSON-encoded data read from
a file or a stream might be significant. It might also be preferred if one
needs to extract a small part of some JSON-encoded data without loading an
entire data set into memory. 

In situations where the size of the JSON data is limited and memory is not a 
concern, it is often more convenient to use ext/json. 


Installation
------------
jsonreader is installed like any other PHP extension. To compile on most 
Unix-like systems, do the following (assuming you have the nescesary build 
toolchain available):

1. cd into the source directory of jsonreader
2. run the following commands (assuming your PHP binaries are in $PATH):

```sh
$ phpize
$ ./configure --enable-shared
$ make
$ sudo make install
```

(you can use `su` instead of `sudo` in order to install as root)

Then, add the following line to your php.ini:

```sh
extension=jsonreader.so 
```

After restarting your web server JSONReader should be available.


Basic Usage
-----------
The following code demonstrates parsing a simple JSON array of strings from
an HTTP stream:

```php
<?php

$reader = new JSONReader(); 
$reader->open('http://www.example.com/news.json');

while ($reader->read()) {
  switch($reader->tokenType) {
    case JSONReader::ARRAY_START:
      echo "Array start:\n";
      break;

    case JSONReader::ARRAY_END:
      echo "Array end.\n";
      break;

    case JSONReader::VALUE:
      echo " - " . $reader->value . "\n";
      break;
  }
}

$reader->close();

?>
```

The JSONReader class provides the following methods:


* ```php
bool JSONReader::open(mixed $URI)
```

  Open a stream to read from, or set an open stream as the stream to read 
  from. 

  $URI can be either a string representing any valid PHP stream URI (such 
  as 'json.txt', 'http://example.com/path/json' or 'php://stdin') or an
  already-open stream resource - this is useful for example if you need to 
  read the HTTP headers from an open TCP stream before passing the HTTP 
  body to the parser, or if you need to set up some stream context or 
  filters before parsing it. 

  Returns TRUE on success, FALSE otherwise. 
  
* ```php
bool JSONReader::close();
```

  Close the open stream attached to the parser. Note that if you do not call
  this method, the stream will be automatically closed when the object is 
  destroyed.

* ```php
bool JSONReader::read();
```

  Read the next JSON token. Returns TRUE as long as there is something to read,
  or FALSE when reading is done or when an error occured. 

 After calling JSONReader::read() you can check the token type, value or other
properties using one of the object properties desctibed below. 

  * ```php
  int JSONReader::tokenType 
  ```
    The type of the current token. NULL if there is no current token, or one of
    the token constants listed below. 

  * ```php
  mixed JSONReader::value
  ```
    The value of the current token, if it has one (not all tokens have a value).
    Will be NULL if there is no current token or if the token has no value. 
    Otherwise, may contain NULL, a boolean, a string, an integer or a 
    floating-point number. 

  * ```php
  int JSONReader::currentStruct
  ```
  
    The type of the current JSON structure we are in. This might be one of 
    JSONReader::ARRAY or JSONReader::OBJECT - or NULL if we are in neither. 

  * ```php
  int JSONReader::currentDepth
  ```
  
    The current nesting level inside the JSON data. 0 means root level.

 The following constants represent the different JSON token types:

```php
  JSONReader::NULL         - a JSON 'null' (this does not equal a PHP NULL)
  JSONReader::FALSE        - Boolean false
  JSONReader::TRUE         - Boolean true
  JSONReader::INT          - An integer value
  JSONReader::FLOAT        - A floating-point value
  JSONReader::STRING       - A string
  
  JSONReader::ARRAY_START  - the beginning of an array
  JSONReader::ARRAY_END    - the end of an array
  JSONReader::OBJECT_START - the beginning of an object 
  JSONReader::OBJECT_KEY   - an object key (::value will contain the key)
  JSONReader::OBJECT_END   - the end of an object
```

Additionally, you may test the value of JSONReader->tokenType against the 
following constants that represent classes of token types:

```php
  JSONReader::BOOLEAN      - Either TRUE or FALSE
  JSONReader::NUMBER       - A numeric type - either an INT or a FLOAT 
  JSONReader::VALUE        - Any value token (including null, booleans,  
                             integers, floats and strings)
```

Note that you shoud use "bitwise and" (&) to compare against these constants - 
the value of JSONReader->tokenType will never be equal (==) to any of them:

```php
<?php

// Check if the current token is a number
if ($reader->tokenType & JSONReader::NUMBER) {
  // do something...
}

// Check if the current token is boolean
if ($reader->tokenTyppe & JSONReader::BOOLEAN) {
  // do something...
}

// Check if the current token is a value token but not a string
if ($reader->tokenType & (JSONReader::VALUE & ~JSONReader::STRING)) {
  // do something ...
}

// NOTE: This will never be true:
if ($reader->tokenType == JSONReader::VALUE) {
  // will never happen!
}

?>
```

Constructor Attributes
----------------------
The JSONReader constructor optionally accepts an array of attributes:

```php
void JSONReader::__construct([array $attributes])
```

`$attributes` is an associative (attribute => value) array, with the following
constants representing different attributes:

  ```php
  JSONReader::ATTR_ERRMODE (NOTE:: Not implemented yet!)
  ```

Set the reader's error handling method. This might be one of:

```php
JSONReader::ERRMODE_PHPERR - report a PHP error / warning (default)
JSONReader::ERRMODE_EXCEPT - throw a JSONReaderException
JSONReader::ERRMODE_INTERN - do not report errors, but keep them internally and allow the programmer to manually check for any errors
```

```php
JSONReader::ATTR_MAX_DEPTH
 ```
 
Set the maximal nesting level of the JSON parser. If the maximal nesting
level is reached, the parser will return with an error. If not specified, 
the value of the jsonreader.max_depth INI setting is used, and the default
value is 64. 
    
There is usually no reason to modify this, unless you know in advance the
JSON structure you are about it read might contain very deep nesting
arrays or objects. 

```php
JSONReader::ATTR_READ_BUFF 
```

Set the stream read buffer size. This sets the largest chunk of data which
is read from the stream per iteration and passed on to the parser. Smaller
values may reduce memory usage but will require more work. If not
specified, the value of the jsonreader.read_buffer INI setting is used,
and the default value of that is 4096.

There is usually no need to change this. 

The following example demonstrates passing attributes when creating the
object:

```php
<?php

// Create a reader that can handle 128 nesting levels and throws exceptions in
// case of error
$reader = new JSONReader(array(
  JSONReader::ATTR_MAX_DEPTH => 128,
  JSONReader::ATTR_ERRMODE   => JSONReader::ERRMODE_EXCEPT
));

?>
```


INI Settings
------------
The following php.ini directives are available:

* `jsonreader.max_depth`    - The default maximal depth that the reader can handle. The default value is 64. This can be overriden using the `ATTR_MAX_DEPTH` attribute. 

* `jsonreader.read_buffer`  - The default read buffer size in bytes. The default value is 4096. This can be overriden using the `ATTR_READ_BUFF` attribute. 


Caveats / Known Issues
----------------------
- Please note that the extension expects input in UTF-8 encoding and decodes any
  JSON-encoded special characters into UTF-8. In the future other encodings 
  might be supported but for now you are adviced to apply an iconv stream filter
  if you are reading from a stream which is not UTF-8 encoded.

- This extension is experimental, the API is likely to change and break in 
  future versions.

- See the TODO file for planned functionality which is currently not implemented.

