#include "http_request_parser.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#define CHECK_COMPLETE(str,                                                   \
                       verb,                                                  \
                       resource,                                              \
                       ver_major,                                             \
                       ver_minor,                                             \
                       cont_len,                                              \
                       kp_alive)                                              \
  {                                                                           \
    http::request val;                                                        \
    size_t        parsed = 0;                                                 \
    if (parser.parse((const void *)str, strlen(str), val, &parsed) !=         \
        http::request_parser::status::done) {                                 \
      std::cerr << "unexpected problem during parsing http request: "         \
                << parsed << "/" << strlen(str) << "\n"                       \
                << str << std::endl;                                          \
      return EXIT_FAILURE;                                                    \
    }                                                                         \
    if (parsed != strlen(str)) {                                              \
      std::cerr << "not all request parsed: " << parsed << "/" << strlen(str) \
                << "\n"                                                       \
                << str << std::endl;                                          \
      return EXIT_FAILURE;                                                    \
    }                                                                         \
    if (val.method != verb) {                                                 \
      std::cerr << "invalid method, expected `" << verb                       \
                << "`, got: " << val.method << "\n"                           \
                << str << std::endl;                                          \
      return EXIT_FAILURE;                                                    \
    }                                                                         \
    if (val.target != resource) {                                             \
      std::cerr << "invalid target, expected `" << resource                   \
                << "`, got :" << val.target << "\n"                           \
                << str << std::endl;                                          \
      return EXIT_FAILURE;                                                    \
    }                                                                         \
    if (val.major != ver_major || val.minor != ver_minor) {                   \
      std::cerr << "invalid version, expected `" << ver_major << "/"          \
                << ver_minor << "`, got: " << val.major << "/" << val.minor   \
                << "\n"                                                       \
                << str << std::endl;                                          \
      return EXIT_FAILURE;                                                    \
    }                                                                         \
    if (val.content_length != cont_len) {                                     \
      std::cerr << "invalid content_length, expected `" << cont_len           \
                << "`, got: " << val.content_length << "\n"                   \
                << str << std::endl;                                          \
      return EXIT_FAILURE;                                                    \
    }                                                                         \
    if (val.keep_alive != kp_alive) {                                         \
      std::cerr << "invalid keep_alive, expected `" << kp_alive               \
                << "`, got: " << val.keep_alive << "\n"                       \
                << str << std::endl;                                          \
      return EXIT_FAILURE;                                                    \
    }                                                                         \
  };

#define CHECK_NOT_COMPLETE(str, headers_complete)                              \
  {                                                                            \
    http::request                val;                                          \
    size_t                       parsed = 0;                                   \
    http::request_parser::status status =                                      \
        parser.parse((const void *)str, strlen(str), val, &parsed);            \
    if ((status & http::request_parser::status::in_complete) == false) {       \
      std::cerr << "unexpected problem during parsing http request: "          \
                << parsed << "/" << strlen(str) << "\n"                        \
                << str << std::endl;                                           \
      return EXIT_FAILURE;                                                     \
    }                                                                          \
    if (headers_complete &&                                                    \
        (status & http::request_parser::status::headers_done) == false) {      \
      std::cerr                                                                \
          << "headers are not complete, but expected that they are complete\n" \
          << str << std::endl;                                                 \
      return EXIT_FAILURE;                                                     \
    } else if (headers_complete == false &&                                    \
               (status & http::request_parser::status::headers_done)) {        \
      std::cerr                                                                \
          << "headers are complete, but expected that they are not complete\n" \
          << str << std::endl;                                                 \
      return EXIT_FAILURE;                                                     \
    }                                                                          \
                                                                               \
    parser.clear();                                                            \
  }

#define CHECK_COMPLETE_HEADER(str, h_key, h_val)                              \
  {                                                                           \
    http::request val;                                                        \
    size_t        parsed = 0;                                                 \
    if (parser.parse((const void *)str, strlen(str), val, &parsed) !=         \
        http::request_parser::status::done) {                                 \
      std::cerr << "unexpected problem during parsing http request: "         \
                << parsed << "/" << strlen(str) << "\n"                       \
                << str << std::endl;                                          \
      return EXIT_FAILURE;                                                    \
    }                                                                         \
    if (parsed != strlen(str)) {                                              \
      std::cerr << "not all request parsed: " << parsed << "/" << strlen(str) \
                << "\n"                                                       \
                << str << std::endl;                                          \
      return EXIT_FAILURE;                                                    \
    }                                                                         \
    if (val.headers.count(h_key) == 0) {                                      \
      std::cerr << "not found expected header: " << h_key << "\n"             \
                << str << std::endl;                                          \
      return EXIT_FAILURE;                                                    \
    }                                                                         \
    if (val.headers[h_key] != h_val) {                                        \
      std::cerr << "expected `" << h_key << ": " << h_val                     \
                << "`, but got: " << h_key << ": " << val.headers[h_key]      \
                << "\n"                                                       \
                << str << std::endl;                                          \
      return EXIT_FAILURE;                                                    \
    }                                                                         \
  }


int main() {
  http::request_parser parser;

  // check simple requests
  CHECK_COMPLETE("GET / HTTP/1.0\r\n"
                 "\r\n",
                 "GET",
                 "/",
                 1,
                 0,
                 0,
                 false);
  CHECK_COMPLETE("POST / HTTP/1.0\r\n"
                 "Content-Length: 0\r\n"
                 "\r\n",
                 "POST",
                 "/",
                 1,
                 0,
                 0,
                 false);

  // check \n instead of \r\n
  CHECK_COMPLETE("GET /hello HTTP/1.0\n"
                 "\n",
                 "GET",
                 "/hello",
                 1,
                 0,
                 0,
                 false);
  CHECK_COMPLETE("PUT /tmp HTTP/1.1\n"
                 "Content-Type:plain/text\n"
                 "Content-Length:0\n"
                 "\n",
                 "PUT",
                 "/tmp",
                 1,
                 1,
                 0,
                 false);

  // check keep-alive
  CHECK_COMPLETE("HEAD /hello HTTP/1.1\r\n"
                 "Content-Type:application/json\r\n"
                 "Connection: keep-alive\r\n"
                 "\r\n",
                 "HEAD",
                 "/hello",
                 1,
                 1,
                 0,
                 true);

  // check content-length
  CHECK_COMPLETE("POST /blah HTTP/1.1\r\n"
                 "Content-Length:5\r\n"
                 "\r\n"
                 "hello",
                 "POST",
                 "/blah",
                 1,
                 1,
                 5,
                 false);
  CHECK_COMPLETE("PUT /some/uri?tmp=blah HTTP/1.1\r\n"
                 "Content-Type: plain/text\r\n"
                 "conNection:Keep-Alive\r\n"
                 "\r\n"
                 "{\"hello\": \"world\"}",
                 "PUT",
                 "/some/uri?tmp=blah",
                 1,
                 1,
                 18,
                 true);

  // check absolute uri
  CHECK_COMPLETE("PUT http://localhost:8000/blah HTTP/1.1\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 "PUT",
                 "/blah",
                 1,
                 1,
                 0,
                 false);

  // check authority uri
  CHECK_COMPLETE("CONNECT www.example.com:80 HTTP/1.1\r\n"
                 "\r\n",
                 "CONNECT",
                 "/",
                 1,
                 1,
                 0,
                 false);
  CHECK_COMPLETE("GET www.example.com:80/tmp HTTP/1.1\r\n"
                 "\r\n",
                 "GET",
                 "/tmp",
                 1,
                 1,
                 0,
                 false);

  // check request with asterisk
  CHECK_COMPLETE("OPTIONS * HTTP/1.1\r\n"
                 "\r\n",
                 "OPTIONS",
                 "*",
                 1,
                 1,
                 0,
                 false);

  // check request with different spaces in request line
  CHECK_COMPLETE("GET\ttmp.com/tmp/blah  \t HTTP/1.1\r\n"
                 "Connection: Keep-Alive\r\n"
                 "\r\n",
                 "GET",
                 "/tmp/blah",
                 1,
                 1,
                 0,
                 true);

  // check not complete requests
  CHECK_NOT_COMPLETE("GET /blah/tmp HTTP/1.0\r\n", false);
  CHECK_NOT_COMPLETE("GET /tmp HTTP/1.1\r\n"
                     "Connection: close\r\n"
                     "Content-Type:   plain/text\r\n",
                     false);
  CHECK_NOT_COMPLETE("POST /tmp HTTP/1.1\r\n"
                     "Content-Type:\tplain/text\r\n"
                     "Content-Length:7\r\n"
                     "\r\n"
                     "body",
                     true);

  // check header
  CHECK_COMPLETE_HEADER("GET /tmp HTTP/1.1\r\n"
                        "Content-Type: plain/text\r\n"
                        "\r\n",
                        "content-type",
                        "plain/text");
  CHECK_COMPLETE_HEADER("GET /tmp HTTP/1.1\r\n"
                        "Content-Type:plain/text\r\n"
                        "\r\n",
                        "Content-Type",
                        "plain/text");
  CHECK_COMPLETE_HEADER("GET /tmp HTTP/1.1\r\n"
                        "Content-Type:\tplain/text\r\n"
                        "\r\n",
                        "Content-Type",
                        "plain/text");

  // check empty header value
  CHECK_COMPLETE_HEADER("GET /tmp HTTP/1.1\r\n"
                        "Some-Header:\r\n"
                        "\r\n",
                        "Some-Header",
                        "");
  CHECK_COMPLETE_HEADER("GET /tmp HTTP/1.1\r\n"
                        "Some-Header: \r\n"
                        "\r\n",
                        "Some-Header",
                        "");

  // check complex header value
  CHECK_COMPLETE_HEADER("GET /tmp HTTP/1.1\r\n"
                        "Some-Header: blah, tmp, val\r\n"
                        "\r\n",
                        "Some-Header",
                        "blah, tmp, val");
  CHECK_COMPLETE_HEADER("GET /tmp HTTP/1.1\r\n"
                        "Some-Header:  blah,   tmp,\tval\r\n"
                        "\r\n",
                        "Some-Header",
                        "blah, tmp, val");

  // check multiline header
  CHECK_COMPLETE_HEADER("GET /tmp HTTP/1.1\r\n"
                        "Content-Type:\tplain/text,\r\n"
                        " application/json\r\n"
                        "\r\n",
                        "Content-Type",
                        "plain/text, application/json");

  CHECK_COMPLETE_HEADER("GET /tmp HTTP/1.1\r\n"
                        "Content-Type:\tplain/text,\r\n"
                        "\tapplication/json\r\n"
                        "\r\n",
                        "Content-Type",
                        "plain/text, application/json");

  // check uri Host logic
  CHECK_COMPLETE_HEADER("DELETE http://localhost:8000/blah HTTP/1.1\r\n"
                        "Connection: close\r\n"
                        "\r\n",
                        "Host",
                        "localhost:8000");
  CHECK_COMPLETE_HEADER("CONNECT www.example.com:80 HTTP/1.1\r\n"
                        "\r\n",
                        "Host",
                        "www.example.com:80");

  return EXIT_SUCCESS;
}
