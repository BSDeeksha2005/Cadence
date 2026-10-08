#include "cadence/adapter/scenario_json.hpp"

#include <charconv>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "cadence/validate.hpp"

namespace cadence {
namespace {

struct JsonValue {
    enum class Kind { Null, Boolean, Number, String, Array, Object };
    Kind kind = Kind::Null;
    std::string scalar;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;
};

class JsonParser {
public:
    explicit JsonParser(const std::string& input) : input_(input) {}

    JsonValue parse() {
        JsonValue value = parse_value();
        whitespace();
        if (position_ != input_.size()) fail("trailing data");
        return value;
    }

private:
    [[noreturn]] void fail(const char* reason) const {
        throw std::invalid_argument(
            "invalid JSON at byte " + std::to_string(position_) + ": " + reason);
    }

    void whitespace() {
        while (position_ < input_.size() &&
               std::isspace(static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
    }

    bool take(char expected) {
        whitespace();
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    void require(char expected) {
        if (!take(expected)) fail("unexpected token");
    }

    std::uint32_t parse_hex4() {
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            if (position_ >= input_.size()) fail("short Unicode escape");
            const char c = input_[position_++];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<std::uint32_t>(c - 'A' + 10);
            else fail("invalid Unicode escape");
        }
        return value;
    }

    static void append_utf8(std::string& output, std::uint32_t codepoint) {
        if (codepoint <= 0x7f) {
            output.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7ff) {
            output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else if (codepoint <= 0xffff) {
            output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else {
            output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
    }

    JsonValue parse_value() {
        whitespace();
        if (position_ >= input_.size()) fail("expected value");
        const char c = input_[position_];
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return parse_string();
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number();
        if (input_.compare(position_, 4, "null") == 0) {
            position_ += 4;
            return JsonValue{};
        }
        if (input_.compare(position_, 4, "true") == 0 ||
            input_.compare(position_, 5, "false") == 0) {
            const std::size_t size = input_[position_] == 't' ? 4 : 5;
            JsonValue value;
            value.kind = JsonValue::Kind::Boolean;
            value.scalar = input_.substr(position_, size);
            position_ += size;
            return value;
        }
        fail("unsupported value");
    }

    JsonValue parse_object() {
        require('{');
        JsonValue value;
        value.kind = JsonValue::Kind::Object;
        if (take('}')) return value;
        do {
            whitespace();
            if (position_ >= input_.size() || input_[position_] != '"') {
                fail("object key must be a string");
            }
            const std::string key = parse_string().scalar;
            require(':');
            if (!value.object.emplace(key, parse_value()).second) {
                fail("duplicate object key");
            }
        } while (take(','));
        require('}');
        return value;
    }

    JsonValue parse_array() {
        require('[');
        JsonValue value;
        value.kind = JsonValue::Kind::Array;
        if (take(']')) return value;
        do {
            value.array.push_back(parse_value());
        } while (take(','));
        require(']');
        return value;
    }

    JsonValue parse_string() {
        require('"');
        JsonValue value;
        value.kind = JsonValue::Kind::String;
        while (position_ < input_.size()) {
            const char c = input_[position_++];
            if (c == '"') return value;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') {
                value.scalar.push_back(c);
                continue;
            }
            if (position_ >= input_.size()) fail("unfinished escape");
            const char escape = input_[position_++];
            switch (escape) {
                case '"': value.scalar.push_back('"'); break;
                case '\\': value.scalar.push_back('\\'); break;
                case '/': value.scalar.push_back('/'); break;
                case 'b': value.scalar.push_back('\b'); break;
                case 'f': value.scalar.push_back('\f'); break;
                case 'n': value.scalar.push_back('\n'); break;
                case 'r': value.scalar.push_back('\r'); break;
                case 't': value.scalar.push_back('\t'); break;
                case 'u': {
                    std::uint32_t codepoint = parse_hex4();
                    if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                        if (position_ + 2 > input_.size() || input_[position_] != '\\' ||
                            input_[position_ + 1] != 'u') fail("missing low surrogate");
                        position_ += 2;
                        const std::uint32_t low = parse_hex4();
                        if (low < 0xdc00 || low > 0xdfff) fail("invalid low surrogate");
                        codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                    } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                        fail("unexpected low surrogate");
                    }
                    append_utf8(value.scalar, codepoint);
                    break;
                }
                default: fail("unsupported string escape");
            }
        }
        fail("unterminated string");
    }

    JsonValue parse_number() {
        const std::size_t start = position_;
        if (input_[position_] == '-') ++position_;
        if (position_ >= input_.size() || !std::isdigit(static_cast<unsigned char>(input_[position_]))) {
            fail("invalid number");
        }
        if (input_[position_] == '0') {
            ++position_;
        } else {
            while (position_ < input_.size() &&
                   std::isdigit(static_cast<unsigned char>(input_[position_]))) ++position_;
        }
        if (position_ < input_.size() &&
            (input_[position_] == '.' || input_[position_] == 'e' || input_[position_] == 'E')) {
            fail("simulation values must be integers");
        }
        JsonValue value;
        value.kind = JsonValue::Kind::Number;
        value.scalar = input_.substr(start, position_ - start);
        return value;
    }

    const std::string& input_;
    std::size_t position_ = 0;
};

const JsonValue& member(const JsonValue& object, const std::string& key) {
    if (object.kind != JsonValue::Kind::Object) {
        throw std::invalid_argument("expected JSON object");
    }
    const auto it = object.object.find(key);
    if (it == object.object.end()) throw std::invalid_argument("missing field: " + key);
    return it->second;
}

const JsonValue* optional_member(const JsonValue& object, const std::string& key) {
    if (object.kind != JsonValue::Kind::Object) throw std::invalid_argument("expected JSON object");
    const auto it = object.object.find(key);
    return it == object.object.end() ? nullptr : &it->second;
}

const std::vector<JsonValue>& array(const JsonValue& value, const char* field) {
    if (value.kind != JsonValue::Kind::Array) {
        throw std::invalid_argument(std::string(field) + " must be an array");
    }
    return value.array;
}

std::string string_value(const JsonValue& value, const char* field) {
    if (value.kind != JsonValue::Kind::String) {
        throw std::invalid_argument(std::string(field) + " must be a string");
    }
    return value.scalar;
}

std::int64_t integer_value(const JsonValue& value, const char* field) {
    if (value.kind != JsonValue::Kind::Number) {
        throw std::invalid_argument(std::string(field) + " must be an integer");
    }
    std::int64_t result = 0;
    const char* begin = value.scalar.data();
    const char* end = begin + value.scalar.size();
    const auto parsed = std::from_chars(begin, end, result);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        throw std::invalid_argument(std::string(field) + " is outside the supported integer range");
    }
    return result;
}

template <typename T>
T checked_integer(const JsonValue& value, const char* field) {
    const std::int64_t number = integer_value(value, field);
    if (number < std::numeric_limits<T>::min() || number > std::numeric_limits<T>::max()) {
        throw std::invalid_argument(std::string(field) + " is outside the supported integer range");
    }
    return static_cast<T>(number);
}

}  // namespace

Scenario parse_scenario_json(const std::string& text) {
    const JsonValue root = JsonParser(text).parse();
    Scenario scenario;
    if (const JsonValue* name = optional_member(root, "name")) {
        scenario.name = string_value(*name, "name");
    }
    scenario.horizon = checked_integer<Tick>(member(root, "horizon"), "horizon");

    std::map<std::string, MutexId> mutex_names;
    for (const JsonValue& value : array(member(root, "mutexes"), "mutexes")) {
        const std::string name = string_value(value, "mutex name");
        const MutexId id = static_cast<MutexId>(scenario.mutexes.size());
        if (!mutex_names.emplace(name, id).second) {
            throw std::invalid_argument("duplicate mutex name: " + name);
        }
        scenario.mutexes.push_back(id);
    }

    for (const JsonValue& task_json : array(member(root, "tasks"), "tasks")) {
        const TaskId id = checked_integer<TaskId>(member(task_json, "id"), "task id");
        const std::string name = string_value(member(task_json, "name"), "task name");
        const Priority priority = checked_integer<Priority>(member(task_json, "priority"), "priority");
        const Tick release = checked_integer<Tick>(member(task_json, "release"), "release");
        std::optional<Tick> deadline;
        if (const JsonValue* value = optional_member(task_json, "deadline")) {
            if (value->kind != JsonValue::Kind::Null) deadline = checked_integer<Tick>(*value, "deadline");
        }

        std::vector<Operation> program;
        for (const JsonValue& op_json : array(member(task_json, "program"), "program")) {
            const auto& parts = array(op_json, "operation");
            if (parts.size() != 2) throw std::invalid_argument("operation must contain kind and argument");
            const std::string kind = string_value(parts[0], "operation kind");
            if (kind == "COMPUTE") {
                program.push_back(Operation::compute(checked_integer<Tick>(parts[1], "compute duration")));
            } else if (kind == "SLEEP") {
                program.push_back(Operation::sleep(checked_integer<Tick>(parts[1], "sleep duration")));
            } else if (kind == "LOCK" || kind == "UNLOCK") {
                MutexId mutex = 0;
                if (parts[1].kind == JsonValue::Kind::String) {
                    const std::string mutex_name = parts[1].scalar;
                    const auto it = mutex_names.find(mutex_name);
                    if (it == mutex_names.end()) throw std::invalid_argument("unknown mutex name: " + mutex_name);
                    mutex = it->second;
                } else {
                    mutex = checked_integer<MutexId>(parts[1], "mutex id");
                }
                program.push_back(kind == "LOCK" ? Operation::lock(mutex) : Operation::unlock(mutex));
            } else {
                throw std::invalid_argument("unknown operation kind: " + kind);
            }
        }
        scenario.tasks.emplace_back(id, name, priority, std::move(program), release, deadline);
    }

    const ValidationResult validation = validate(scenario);
    if (!validation.valid()) {
        const ValidationError& error = validation.errors.front();
        throw std::invalid_argument(std::string(to_string(error.code)) + " at " + error.path + ": " + error.message);
    }
    return scenario;
}

Scenario load_scenario_json_file(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open scenario file: " + path);
    std::ostringstream contents;
    contents << input.rdbuf();
    return parse_scenario_json(contents.str());
}

}  // namespace cadence
