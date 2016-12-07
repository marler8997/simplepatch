Usage
================================================================================
```
> simplepatch <patch-file> <file-to-patch> [patched-file]
```
> Note: if no 'patched-file' is given, the original 'file-to-patch' will be
        overwritten.

The Simple Patch File Format
================================================================================
A patch file contains a set of commands of the following form:
```
<CommandLetter><Arguments><NewLine><OptionalData>
```

Example
--------------------------------------------------------------------------------
example.simplepatch:
```
c13
s48
i5
Hello
f
```

`c13` copies 13 bytes from the original file to the patched file.
`s48` skips 48 bytes in the original file.
`i5\nHello` injects "Hello" in the patched file.
`f` flushes the remaining contents of the original file to the patched file.

> Note: Every command ends with a newline character, specificaly a '\n' (0x0A).
        Any command that ends with '\r\n' will result in an error.

Commands
--------------------------------------------------------------------------------
### 'c' copy
- Arguments: length (ascii number)
- Description: copies "length" bytes from original file to patched file.

### 's' skip
- Arguments: length (ascii number)
- Description: skip "length" bytes in original file.

### 'i' inject
- Arguments: length (ascii number)
- Description: inject the following data into the patched file.
- Note: is followed by length bytes of data.  This optional data
      can also end with an optional newline. The purpose of this optional
      newline is so the next command can start at the beginning of the line.

### 'f' flush
- Arguments: none
- Description: flush the rest of the original file to the patched file.
