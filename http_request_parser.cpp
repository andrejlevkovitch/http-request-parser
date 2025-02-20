#include "http_request_parser.hpp"
#include <cstddef>
#include <cstring>
#include <string>
#include <unordered_map>

#define HTTP           "HTTP"
#define CONNECTION     "Connection"
#define CONTENT_LENGTH "Content-Length"
#define KEEP_ALIVE     "Keep-Alive"
#define HOST           "Host"

#define IS_UPALPHA(ch) ((ch) >= 'A' && (ch) <= 'Z')
#define IS_LOALPHA(ch) ((ch) >= 'a' && (ch) <= 'z')
#define IS_ALPHA(ch)   (IS_UPALPHA(ch) || IS_LOALPHA(ch))
#define IS_DIGIT(ch)   ((ch) >= '0' && (ch) <= '9')
#define IS_CTL(ch)     (((ch) > 0 && (ch) < 31 && (ch) != '\t') || (ch) == 127)
#define IS_VCHAR(ch)   ((ch) >= 33 && (ch) <= 126)
#define CR             '\r'
#define LF             '\n'
#define CRLF           "\r\n"
#define SP             ' '
#define HT             '\t'
#define SLASH          '/'
#define COLON          ':'
#define ASTERISK       '*'

#define DOT '.'

#define IS_TEXT(ch) (IS_CTL(ch) == false || (ch) == LF || (ch) == CR)
#define IS_HEX(ch) \
  (IS_DIGIT(ch) || ((ch) >= 'A' && (ch) <= 'F') || ((ch) >= 'a' && (ch) <= 'f'))

#define IS_SEPARATOR(ch) (strchr(", /;:=()<>@\"[]?{}\t\\", ch))
#define IS_SPACE(ch)     ((ch) == ' ' || (ch) == '\t')

namespace http {
std::size_t
string_case_insensetive_hash::operator()(std::string str) const noexcept {
  for (char &ch : str) {
    ch = tolower(ch);
  }
  return std::hash<std::string>()(str);
}

bool string_case_insensetive_comp::operator()(
    const std::string &lhs,
    const std::string &rhs) const noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.size(); ++i) {
    if (tolower(lhs[i]) != tolower(rhs[i])) {
      return false;
    }
  }
  return true;
}


request::request()
    : major{-1}
    , minor{-1}
    , content_length{0}
    , keep_alive{false}
    , body{NULL} {
}


request_parser::request_parser() noexcept
    : state_{0}
    , body_readed_{0} {
}

request_parser::status request_parser::parse(const void    *buf,
                                             size_t         len,
                                             http::request &req,
                                             size_t        *parsed) noexcept {
  enum state {
    none,
    verb,
    target,
    target_colon,
    target_origin,
    target_scheme,
    target_host,
    target_asterisk,
    protocol,
    major,
    minor,
    cr,
    header_key,
    header_val,
    second_cr,
    body,
  };

  status      retval = status::error;
  const char *start  = NULL;
  const char *octets = reinterpret_cast<const char *>(buf);
  const char *iter   = octets;
  for (; iter != octets + len; ++iter) {
    char octet = *iter;
    retval     = status::error;

    switch (state_) {
    case none:
      state_       = verb;
      body_readed_ = 0;
      [[fallthrough]];
    case verb:
      if (IS_ALPHA(octet)) {
        if (start == NULL) {
          start = iter;
        }
        retval = status::in_complete;
      } else if (IS_SPACE(octet)) {
        if (start != NULL) {
          req.method = std::string{start, iter};
          start      = NULL;
          state_     = target;
          retval     = status::in_complete;
        }
      }
      break;
    case target:
      if (start == NULL) {
        if (octet == SLASH) {
          state_ = target_origin;
          goto TargetOrigin;
        } else if (octet == ASTERISK) {
          state_ = target_asterisk;
          goto TargetAsterisk;
        } else if (IS_ALPHA(octet)) { // absolute or authority
          start  = iter;
          retval = status::in_complete;
        } else if (IS_SPACE(octet)) {
          retval = status::in_complete;
        }
      } else { // absolute or authority
        if (octet == COLON) {
          state_ = target_colon;
          retval = status::in_complete;
        } else if (IS_DIGIT(octet) || octet == DOT || octet == SLASH) {
          state_ = target_host;
          goto TargetHost;
        } else if (IS_ALPHA(octet)) {
          retval = status::in_complete;
        }
      }
      break;
    case target_colon:
      if (octet == SLASH) {
        state_ = target_scheme;
        retval = status::in_complete;
      } else if (IS_VCHAR(octet)) {
        state_ = target_host;
        retval = status::in_complete;
      }
      break;
    case target_scheme:
      if (octet == SLASH) { // second slash, like http://
        start  = NULL;
        state_ = target_host;
        retval = status::in_complete;
      }
      break;
    case target_host:
    TargetHost:
      if (IS_ALPHA(octet) || IS_DIGIT(octet) || octet == DOT ||
          octet == COLON) {
        if (start == NULL) {
          start = iter;
        }
        retval = status::in_complete;
      } else if (IS_SPACE(octet)) {
        if (start != NULL) {
          req.headers[HOST] = std::string{start, iter};
          req.target        = SLASH;
          start             = NULL;
          state_            = protocol;
          retval            = status::in_complete;
        }
      } else if (octet == SLASH) {
        if (start != NULL) {
          req.headers[HOST] = std::string{start, iter};
          start             = NULL;
          state_            = target_origin;
          goto TargetOrigin;
        }
      }
      break;
    case target_origin:
    TargetOrigin:
      if (IS_VCHAR(octet)) {
        if (start == NULL) {
          start = iter;
        }
        retval = status::in_complete;
      } else if (IS_SPACE(octet)) {
        if (start != NULL) {
          req.target = std::string{start, iter};
          start      = NULL;
          state_     = protocol;
          retval     = status::in_complete;
        }
      }
      break;
    case target_asterisk:
    TargetAsterisk:
      req.target = ASTERISK;
      state_     = protocol;
      retval     = status::in_complete;
      break;
    case protocol:
      if (IS_ALPHA(octet)) {
        if (start == NULL) {
          start = iter;
        }
        retval = status::in_complete;
      } else if (IS_SPACE(octet)) {
        if (start == NULL) {
          retval = status::in_complete;
        } else {
          retval = status::error;
        }
      } else if (octet == SLASH) {
        if (start != NULL && iter - start == strlen(HTTP) &&
            strncmp(start, HTTP, strlen(HTTP)) == 0) {
          state_ = major;
          start  = NULL;
          retval = status::in_complete;
        }
      }
      break;
    case major:
      if (IS_DIGIT(octet)) {
        if (start == NULL) {
          start = iter;
        }
        retval = status::in_complete;
      } else if (octet == DOT) {
        if (start != NULL) {
          req.major = atoi(start);
          start     = NULL;
          state_    = minor;
          retval    = status::in_complete;
        }
      }
      break;
    case minor:
      if (IS_DIGIT(octet)) {
        if (start == NULL) {
          start = iter;
        }
        retval = status::in_complete;
      } else if (octet == CR || octet == LF) {
        if (start != NULL) {
          req.minor = atoi(start);
          start     = NULL;
          if (octet == CR) {
            state_ = cr;
          } else {
            state_ = header_key;
          }
          retval = status::in_complete;
        }
      }
      break;
    case cr:
      if (octet == LF) {
        state_ = header_key;
        retval = status::in_complete;
      }
      break;
    case header_key:
      if (IS_VCHAR(octet)) {
        if (octet == COLON) {
          if (start != NULL) {
            header_key_ = std::string{start, iter};
            start       = NULL;
            state_      = header_val;
            retval      = status::in_complete;
          }
        } else {
          if (start == NULL) {
            start = iter;
          }
          retval = status::in_complete;
        }
      } else if (start == NULL) {
        if (IS_SPACE(octet)) {
          state_ = header_val;
          retval = status::in_complete;
        } else if (octet == CR) {
          state_ = second_cr;
          retval = status::in_complete;
        } else if (octet == LF) {
          goto PreBodyLogic;
        }
      }
      break;
    case header_val:
      if (IS_TEXT(octet)) {
        if (IS_VCHAR(octet)) {
          if (start == NULL) {
            start = iter;
          }
        } else {
          if (start != NULL) {
            if (req.headers.count(header_key_) == 0) {
              req.headers.emplace(header_key_, std::string{start, iter});
            } else {
              std::string &val = req.headers[header_key_];
              if (val.empty() == false) {
                val.reserve(val.size() + 1 + iter - start);
                val.append(" ");
                req.headers[header_key_].append(start, iter);
              } else {
                val = std::string{start, iter};
              }
            }
          } else if ((octet == CR || octet == LF) &&
                     req.headers.count(header_key_) == 0) {
            req.headers.emplace(header_key_, "");
          }

          start = NULL;
          if (octet == CR) {
            state_ = cr;
          } else if (octet == LF) {
            state_ = header_key;
          }
        }
        retval = status::in_complete;
      }
      break;
    case second_cr:
      if (octet == LF) {
      PreBodyLogic:
        if (req.headers.count(CONNECTION) != 0 &&
            string_case_insensetive_comp()(req.headers[CONNECTION],
                                           KEEP_ALIVE)) {
          req.keep_alive = true;
        } else {
          req.keep_alive = false;
        }

        if (req.headers.count(CONTENT_LENGTH) != 0) {
          req.content_length = atoi(req.headers[CONTENT_LENGTH].c_str());
        } else {
          req.content_length = (octets + len) - (iter + 1);
        }

        if (req.content_length == 0) {
          state_ = none;
          retval = status::done;
          ++iter;
        } else {
          state_ = state::body;
          retval = (status)(status::headers_done | status::in_complete);
        }
      }
      break;
    case body: {
      if (body_readed_ == 0) {
        req.body = iter;
      }

      long content_left = req.content_length - body_readed_;
      long buf_left     = octets + len - iter;
      if (content_left == buf_left) {
        body_readed_ += content_left;
        state_ = none;
        retval = status::done;
        iter   = octets + len;
      } else if (content_left < buf_left) {
        body_readed_ += content_left;
        state_ = none;
        retval = status::done;
        iter += content_left;
      } else {
        body_readed_ += buf_left;
        state_ = state::body;
        retval = (status)(status::headers_done | status::in_complete);
        iter   = octets + len - 1 /*because we increment iter in for loop*/;
      }
    } break;
    default:
      break;
    }

    if (retval == status::error) {
      state_ = none;
      break;
    } else if ((retval & status::in_complete) == false) {
      // parsing is done
      break;
    }
  }
  if (parsed) {
    *parsed = iter - octets;
  }
  return retval;
}

void request_parser::clear() noexcept {
  state_ = 0;
  header_key_.clear();
  body_readed_ = 0;
}
} // namespace http
