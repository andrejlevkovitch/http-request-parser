# simple-http-request-parser

Extremely simple http request parser, written on c++11 (zero-dependency)


## FixMe

1. parser doesn't decode url encoded symbols
2. parser doesn't unpack quoted strings and symbols
3. parser fails with quoted spaces in header name


## BUGS

1. The parser doesn't support eof semantic. If request doesn't contain
`Content-Length`, then parser assume that buffer contains complete message. So
all data after http headers and to buffer end will be marked as body and parsing
will be finished (done)

2. The parser doesn't copy buffer data. So, if your request split to several
buffers, you should manually save result of request::body before start next
parsing operation
