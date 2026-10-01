#pragma once

#include <stddef.h>
#include <stdint.h>

// The Link wire contract, in C++ (`docs/protocol.md`, ADR-0006).
//
// This is the Device's mirror of the message set, deliberately pure: no
// Arduino, no allocation, no I/O. `parse` reads one flat JSON object into a
// small fixed table and the typed getters pull values out; `Writer` serializes
// one message into a caller-owned buffer. That is enough for the Link's NDJSON
// frames and keeps the codec host-testable.
//
// Nested objects and arrays (the device's `caps` list, `cfg.set`) are kept
// verbatim as their value text; the Device never needs to interpret them.

namespace protocol {

constexpr size_t kMaxFields = 12;
constexpr size_t kMaxKeyLength = 16;
constexpr size_t kMaxValueLength = 96;

struct Field {
  char key[kMaxKeyLength];
  char value[kMaxValueLength];
};

struct Object {
  Field fields[kMaxFields];
  size_t count = 0;

  // The raw value text for `key`, or nullptr when absent.
  const char* find(const char* key) const;
};

// Parses one flat JSON object. Returns false if the text is not a single,
// well-formed object or if it does not fit the fixed table.
bool parse(const char* text, Object& out);

// Typed accessors. Each returns false when the key is absent or the value has
// the wrong JSON type.
bool getInt(const Object& object, const char* key, long& out);
bool getNumber(const Object& object, const char* key, double& out);
bool getString(const Object& object, const char* key, char* out, size_t capacity);

// Serializes one compact JSON object into `buffer`, NUL-terminated while it
// fits. `key()` manages the separating commas, so a message is objectStart(),
// then key/value pairs, then objectEnd(). If `ok()` is false afterwards the
// buffer overflowed and the caller must discard the frame.
class Writer {
 public:
  Writer(char* buffer, size_t capacity);

  bool objectStart();
  bool objectEnd();
  bool key(const char* name);
  bool string(const char* value);
  bool number(long value);
  bool number(double value);
  bool raw(const char* value);

  const char* cStr() const { return buffer_; }
  size_t length() const { return length_; }
  bool ok() const { return ok_; }

 private:
  bool put(char c);
  bool put(const char* text);

  char* buffer_;
  size_t capacity_;
  size_t length_ = 0;
  bool ok_ = true;
  bool needComma_ = false;
};

}  // namespace protocol
