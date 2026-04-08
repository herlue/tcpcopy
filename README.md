### TCP File Transfer Tool in C

## Features:
- custom application protocol
- supports multiple files
- handles partial send/recv
- exact byte-based receiving

## Protocol

The client sends one header line per file, followed by exactly `FILESIZE` bytes of file data

```text
<FILENAMELENGTH> <FILENAME> <FILESIZE>\n<FILEDATA>
