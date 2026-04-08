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
```

## Limitations / Future Work
- the server is currently single-threaded and handles one client at a time (planned: support for multiple clients using `select()` or `poll()`)
- received files are stored in a fixed directory (`./files`) (no configurable output path yet)
- file paths are interpreted using UNIX-style separators (`/`) (no platform-independent path handling)
- no authentication or encryption is implemented (intended for local or trusted network environments)
- the protocol is simple and not versioned (no backward compatibility guarantees)


## Design Decisions
- a simple text-based protocol was chosen for readability and ease of debugging
- filename length is transmitted explicitly to support spaces in filenames
- exact byte counts are used to handle TCP's stream-oriented nature
