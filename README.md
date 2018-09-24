# 8505-asn1

## Building

to keep the root folder orderly it is recomended to make from within bin as follows

```
git clone https://github.com/isaacmorneau/8505-asn1.git
mkdir 8505-asn1/bin
cd bin
cmake ../
make
```

## Running

Client usage info:

```
$ ./8505-asn1-client
-[-v]ersion - current version
-[-t]est    - run the test suite if compiled in debug mode
-[-f]ile    - the path to the file to send
-[-p]ort    - the server port to connect to
-[-h]ost    - the host to connect to
-[-d]elay   - seconds between messages
--help      - this message
```

Server usage info:

```
$ ./8505-asn1-server
-[-v]ersion - current version
-[-t]est    - run the test suite if compiled in debug mode
-[-f]ile    - the path to the file to recv
-[-p]ort    - the server port to connect to
--help      - this message

```

## Manifest

Encoding Code

- crc32
- encoder
- test

Client Code

- client
- encoder
- network

Server Code

- server
- encoder
- network

