#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

namespace http {
class request_parser;

struct string_case_insensetive_hash {
  std::size_t operator()(std::string str) const noexcept;
};

struct string_case_insensetive_comp {
  bool operator()(const std::string &lhs,
                  const std::string &rhs) const noexcept;
};


using headers = std::unordered_map<std::string,
                                   std::string,
                                   string_case_insensetive_hash,
                                   string_case_insensetive_comp>;

class request {
  friend request_parser;

public:
  request();

  std::string   method;
  std::string   target;
  int           major;
  int           minor;
  http::headers headers;
  size_t        content_length;
  bool          keep_alive;
  const void *  body;
};

class request_parser {
public:
  enum status {
    error        = 0b000,
    in_complete  = 0b001,
    headers_done = 0b010,
    body_done    = 0b100,
    done         = 0b110,
  };

  request_parser() noexcept;

  /**\param parsed capacity of octets that was parsed
   * \note if Content-Length is empty, then parser assume that message in buffer
   * is complete, so all octets after header will be marked as message body
   */
  enum status parse(const void *   buf,
                    size_t         len,
                    http::request &req,
                    size_t *       parsed = NULL) noexcept;

  /**\brief restore parser to default state
   */
  void clear() noexcept;

private:
  int         state_;
  std::string header_key_;
  size_t      body_readed_;
};
} // namespace http
