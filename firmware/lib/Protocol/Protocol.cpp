#include "Protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace protocol {
namespace {

bool isSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

const char* skipSpace(const char* p) {
  while (isSpace(*p)) {
    ++p;
  }
  return p;
}

// Returns the pointer just past the closing quote of the string at `p`, or
// nullptr if the string never closes.
const char* scanString(const char* p) {
  if (*p != '"') {
    return nullptr;
  }
  ++p;
  while (*p != '\0') {
    if (*p == '\\') {
      ++p;
      if (*p == '\0') {
        return nullptr;
      }
      ++p;
      continue;
    }
    if (*p == '"') {
      return p + 1;
    }
    ++p;
  }
  return nullptr;
}

// Returns the pointer just past the object/array that opens at `p`. Valid JSON
// balances each bracket kind independently, so counting the opening kind is
// enough; strings are skipped so a bracket inside one does not count.
const char* scanContainer(const char* p) {
  const char open = *p;
  const char close = open == '{' ? '}' : ']';
  int depth = 0;
  while (*p != '\0') {
    if (*p == '"') {
      p = scanString(p);
      if (p == nullptr) {
        return nullptr;
      }
      continue;
    }
    if (*p == open) {
      ++depth;
    } else if (*p == close) {
      --depth;
      if (depth == 0) {
        return p + 1;
      }
    }
    ++p;
  }
  return nullptr;
}

}  // namespace

const char* Object::find(const char* key) const {
  for (size_t i = 0; i < count; ++i) {
    if (strcmp(fields[i].key, key) == 0) {
      return fields[i].value;
    }
  }
  return nullptr;
}

bool parse(const char* text, Object& out) {
  out.count = 0;
  const char* p = skipSpace(text);
  if (*p != '{') {
    return false;
  }
  p = skipSpace(p + 1);
  if (*p == '}') {
    return skipSpace(p + 1)[0] == '\0';
  }

  while (true) {
    p = skipSpace(p);
    if (*p != '"') {
      return false;
    }
    const char* keyEnd = scanString(p);
    if (keyEnd == nullptr) {
      return false;
    }
    const size_t keyLength = static_cast<size_t>(keyEnd - p) - 2;
    if (keyLength == 0 || keyLength >= kMaxKeyLength || out.count >= kMaxFields) {
      return false;
    }

    const char* q = skipSpace(keyEnd);
    if (*q != ':') {
      return false;
    }
    q = skipSpace(q + 1);

    const char* valueEnd = nullptr;
    if (*q == '"') {
      valueEnd = scanString(q);
    } else if (*q == '{' || *q == '[') {
      valueEnd = scanContainer(q);
    } else {
      const char* r = q;
      while (*r != '\0' && *r != ',' && *r != '}') {
        ++r;
      }
      while (r > q && isSpace(*(r - 1))) {
        --r;
      }
      if (r == q) {
        return false;  // an empty token is not a value
      }
      valueEnd = r;
    }
    if (valueEnd == nullptr) {
      return false;
    }

    const size_t valueLength = static_cast<size_t>(valueEnd - q);
    if (valueLength >= kMaxValueLength) {
      return false;
    }

    Field& field = out.fields[out.count];
    for (size_t i = 0; i < keyLength; ++i) {
      field.key[i] = p[1 + i];
    }
    field.key[keyLength] = '\0';
    for (size_t i = 0; i < valueLength; ++i) {
      field.value[i] = q[i];
    }
    field.value[valueLength] = '\0';
    ++out.count;

    p = skipSpace(valueEnd);
    if (*p == ',') {
      p += 1;
      continue;
    }
    if (*p == '}') {
      return skipSpace(p + 1)[0] == '\0';
    }
    return false;
  }
}

bool getInt(const Object& object, const char* key, long& out) {
  const char* value = object.find(key);
  if (value == nullptr) {
    return false;
  }
  char* end = nullptr;
  const long parsed = strtol(value, &end, 10);
  if (end == value || *end != '\0') {
    return false;
  }
  out = parsed;
  return true;
}

bool getNumber(const Object& object, const char* key, double& out) {
  const char* value = object.find(key);
  if (value == nullptr) {
    return false;
  }
  char* end = nullptr;
  const double parsed = strtod(value, &end);
  if (end == value || *end != '\0') {
    return false;
  }
  out = parsed;
  return true;
}

bool getString(const Object& object, const char* key, char* out, size_t capacity) {
  const char* value = object.find(key);
  if (value == nullptr || value[0] != '"' || capacity == 0) {
    return false;
  }
  size_t written = 0;
  const char* p = value + 1;
  while (*p != '\0' && *p != '"') {
    char c = *p;
    if (c == '\\') {
      ++p;
      switch (*p) {
        case 'n': c = '\n'; break;
        case 'r': c = '\r'; break;
        case 't': c = '\t'; break;
        case 'b': c = '\b'; break;
        case 'f': c = '\f'; break;
        case '\0': return false;
        default: c = *p; break;  // \" \\ \/ and anything unknown
      }
    }
    if (written + 1 >= capacity) {
      return false;
    }
    out[written++] = c;
    ++p;
  }
  if (*p != '"') {
    return false;
  }
  out[written] = '\0';
  return true;
}

Writer::Writer(char* buffer, size_t capacity) : buffer_(buffer), capacity_(capacity) {
  if (capacity_ == 0) {
    ok_ = false;
  } else {
    buffer_[0] = '\0';
  }
}

bool Writer::put(char c) {
  if (!ok_ || length_ + 1 >= capacity_) {
    ok_ = false;
    return false;
  }
  buffer_[length_++] = c;
  buffer_[length_] = '\0';
  return true;
}

bool Writer::put(const char* text) {
  while (ok_ && *text != '\0') {
    put(*text++);
  }
  return ok_;
}

bool Writer::objectStart() {
  needComma_ = false;
  return put('{');
}

bool Writer::objectEnd() {
  needComma_ = false;
  return put('}');
}

bool Writer::key(const char* name) {
  if (needComma_) {
    put(',');
  }
  put('"');
  put(name);
  put('"');
  put(':');
  needComma_ = true;
  return ok_;
}

bool Writer::string(const char* value) {
  put('"');
  for (const char* p = value; *p != '\0'; ++p) {
    switch (*p) {
      case '"': put("\\\""); break;
      case '\\': put("\\\\"); break;
      case '\n': put("\\n"); break;
      case '\r': put("\\r"); break;
      case '\t': put("\\t"); break;
      default: put(*p); break;
    }
  }
  put('"');
  return ok_;
}

bool Writer::number(long value) {
  char text[24];
  snprintf(text, sizeof(text), "%ld", value);
  return put(text);
}

bool Writer::number(double value) {
  char text[32];
  snprintf(text, sizeof(text), "%.6g", value);
  return put(text);
}

bool Writer::raw(const char* value) { return put(value); }

}  // namespace protocol
