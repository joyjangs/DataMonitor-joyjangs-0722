// ============================================================================
// Json.cpp
// ----------------------------------------------------------------------------
// Json.h에서 선언한 JsonValue의 실제 동작(구현부)입니다.
// 크게 두 부분으로 나뉩니다.
//   1) 값 조작 부분 (생성자, operator[], AsInt 등) - 위쪽
//   2) 문자열 <-> JsonValue 변환 부분 (Dump / Parse) - 아래쪽
// ============================================================================

#include "Json.h"
#include <sstream>
#include <cctype>

// ---------------------------------------------------------------------------
// 생성자들
// ---------------------------------------------------------------------------

JsonValue::JsonValue() : type_(JsonType::Null) {}
JsonValue::JsonValue(std::nullptr_t) : type_(JsonType::Null) {}
JsonValue::JsonValue(bool value) : type_(JsonType::Boolean), boolValue_(value) {}
JsonValue::JsonValue(int value) : type_(JsonType::Number), numberValue_(static_cast<double>(value)) {}
JsonValue::JsonValue(double value) : type_(JsonType::Number), numberValue_(value) {}
JsonValue::JsonValue(const std::string& value) : type_(JsonType::String), stringValue_(value) {}
JsonValue::JsonValue(const char* value) : type_(JsonType::String), stringValue_(value ? value : "") {}

JsonValue JsonValue::MakeArray()
{
    JsonValue v;
    v.type_ = JsonType::Array;
    return v;
}

JsonValue JsonValue::MakeObject()
{
    JsonValue v;
    v.type_ = JsonType::Object;
    return v;
}

bool JsonValue::IsNull() const { return type_ == JsonType::Null; }
bool JsonValue::IsObject() const { return type_ == JsonType::Object; }
bool JsonValue::IsArray() const { return type_ == JsonType::Array; }

// 현재 타입이 기대하는 타입과 다르면 예외를 던져서, 잘못된 사용을 즉시 알아채게 합니다.
// (예: 문자열 값에 대고 AsInt()를 호출하는 실수)
void JsonValue::EnsureType(JsonType expected, const char* what) const
{
    if (type_ != expected)
    {
        throw std::runtime_error(std::string("JsonValue 타입 오류: ") + what + " 을(를) 기대했지만 다른 타입입니다.");
    }
}

// ---------------------------------------------------------------------------
// 객체(Object) 접근
// ---------------------------------------------------------------------------

JsonValue& JsonValue::operator[](const std::string& key)
{
    // json["id"] = 1; 처럼 "아직 아무 타입도 아닌(Null)" 값에 대해 처음 쓰기를
    // 시도하면 자동으로 Object로 승격시켜줍니다. (JsonValue::MakeObject() 없이도
    // 편하게 객체를 만들 수 있도록 하는 편의 기능입니다.)
    if (type_ == JsonType::Null)
    {
        type_ = JsonType::Object;
    }
    EnsureType(JsonType::Object, "Object");

    for (auto& kv : objectValue_)
    {
        if (kv.first == key)
        {
            return kv.second;
        }
    }
    // 없는 키였다면 Null 값으로 새로 추가한 뒤, 그 값을 참조로 돌려줍니다.
    objectValue_.emplace_back(key, JsonValue());
    return objectValue_.back().second;
}

const JsonValue& JsonValue::operator[](const std::string& key) const
{
    EnsureType(JsonType::Object, "Object");
    for (const auto& kv : objectValue_)
    {
        if (kv.first == key)
        {
            return kv.second;
        }
    }
    throw std::out_of_range("JSON 객체에 키가 없습니다: " + key);
}

bool JsonValue::Contains(const std::string& key) const
{
    if (type_ != JsonType::Object) return false;
    for (const auto& kv : objectValue_)
    {
        if (kv.first == key) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// 배열(Array) 접근
// ---------------------------------------------------------------------------

JsonValue& JsonValue::operator[](size_t index)
{
    EnsureType(JsonType::Array, "Array");
    return arrayValue_.at(index);
}

const JsonValue& JsonValue::operator[](size_t index) const
{
    EnsureType(JsonType::Array, "Array");
    return arrayValue_.at(index);
}

void JsonValue::PushBack(JsonValue value)
{
    if (type_ == JsonType::Null)
    {
        type_ = JsonType::Array;
    }
    EnsureType(JsonType::Array, "Array");
    arrayValue_.push_back(std::move(value));
}

size_t JsonValue::Size() const
{
    if (type_ == JsonType::Array) return arrayValue_.size();
    if (type_ == JsonType::Object) return objectValue_.size();
    return 0;
}

// ---------------------------------------------------------------------------
// 값 꺼내기
// ---------------------------------------------------------------------------

int JsonValue::AsInt() const
{
    EnsureType(JsonType::Number, "Number");
    return static_cast<int>(numberValue_);
}

double JsonValue::AsDouble() const
{
    EnsureType(JsonType::Number, "Number");
    return numberValue_;
}

bool JsonValue::AsBool() const
{
    EnsureType(JsonType::Boolean, "Boolean");
    return boolValue_;
}

const std::string& JsonValue::AsString() const
{
    EnsureType(JsonType::String, "String");
    return stringValue_;
}

// ---------------------------------------------------------------------------
// Dump: JsonValue -> 텍스트
// ---------------------------------------------------------------------------
// indent(들여쓰기 칸 수)가 0보다 크면 "예쁘게 출력(pretty print)"하고,
// 0이면 한 줄로 압축해서 출력합니다. 사람이 파일을 열어봤을 때 읽기 쉽도록
// 기본적으로 들여쓰기를 사용할 예정입니다(main.cpp에서 4를 넘겨줌).
// ---------------------------------------------------------------------------

namespace
{
    // 문자열 안의 특수문자(따옴표, 역슬래시, 개행 등)를 JSON 규칙에 맞게 이스케이프합니다.
    // 예: 이메일에 \"가 들어있으면 \\\" 로 바꿔줘야 JSON 문법이 깨지지 않습니다.
    void AppendEscapedString(std::string& out, const std::string& value)
    {
        out += '"';
        for (char c : value)
        {
            switch (c)
            {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default:   out += c; break;
            }
        }
        out += '"';
    }

    void AppendIndent(std::string& out, int indent, int depth)
    {
        if (indent <= 0) return;
        out += '\n';
        out.append(static_cast<size_t>(indent) * depth, ' ');
    }
}

void JsonValue::DumpInternal(std::string& out, int indent, int currentDepth) const
{
    switch (type_)
    {
    case JsonType::Null:
        out += "null";
        break;
    case JsonType::Boolean:
        out += boolValue_ ? "true" : "false";
        break;
    case JsonType::Number:
    {
        // 정수처럼 보이는 값(예: id=1.0)은 "1"로, 아니면 실수 그대로 출력합니다.
        // 사람 눈으로 people.json을 봤을 때 "id": 1.0 이 아니라 "id": 1 로 보이게 하기 위함입니다.
        if (numberValue_ == static_cast<double>(static_cast<long long>(numberValue_)))
        {
            out += std::to_string(static_cast<long long>(numberValue_));
        }
        else
        {
            std::ostringstream oss;
            oss << numberValue_;
            out += oss.str();
        }
        break;
    }
    case JsonType::String:
        AppendEscapedString(out, stringValue_);
        break;
    case JsonType::Array:
    {
        out += '[';
        for (size_t i = 0; i < arrayValue_.size(); ++i)
        {
            AppendIndent(out, indent, currentDepth + 1);
            arrayValue_[i].DumpInternal(out, indent, currentDepth + 1);
            if (i + 1 < arrayValue_.size()) out += ',';
        }
        AppendIndent(out, indent, currentDepth);
        out += ']';
        break;
    }
    case JsonType::Object:
    {
        out += '{';
        for (size_t i = 0; i < objectValue_.size(); ++i)
        {
            AppendIndent(out, indent, currentDepth + 1);
            AppendEscapedString(out, objectValue_[i].first);
            out += ": ";
            objectValue_[i].second.DumpInternal(out, indent, currentDepth + 1);
            if (i + 1 < objectValue_.size()) out += ',';
        }
        AppendIndent(out, indent, currentDepth);
        out += '}';
        break;
    }
    }
}

std::string JsonValue::Dump(int indent) const
{
    std::string out;
    DumpInternal(out, indent, 0);
    return out;
}

// ---------------------------------------------------------------------------
// Parse: 텍스트 -> JsonValue
// ---------------------------------------------------------------------------
// "재귀 하강 파서(recursive descent parser)"라는 방식을 사용합니다.
// 원리는 간단합니다: JSON 문법 자체가 재귀적으로 정의되어 있으므로
// (객체는 값들을 담고, 그 값은 다시 객체나 배열일 수 있음), 파싱 함수도
// 자기 자신을 재귀 호출하는 구조로 짜면 자연스럽게 맞아떨어집니다.
//
// Parser 라는 작은 도우미 구조체가 "지금 문자열의 어디까지 읽었는지(pos_)"를
// 들고 다니면서 한 글자씩 앞으로 나아갑니다.
// ---------------------------------------------------------------------------

namespace
{
    class Parser
    {
    public:
        explicit Parser(const std::string& text) : text_(text) {}

        JsonValue ParseValue()
        {
            SkipWhitespace();
            if (pos_ >= text_.size())
            {
                throw JsonParseError("예상치 못하게 문자열이 끝났습니다.");
            }

            char c = text_[pos_];
            if (c == '{') return ParseObject();
            if (c == '[') return ParseArray();
            if (c == '"') return JsonValue(ParseString());
            if (c == 't' || c == 'f') return ParseBool();
            if (c == 'n') return ParseNull();
            if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return ParseNumber();

            throw JsonParseError(std::string("알 수 없는 문자로 시작하는 값: ") + c);
        }

        void ExpectEndOfInput()
        {
            SkipWhitespace();
            if (pos_ != text_.size())
            {
                throw JsonParseError("JSON 값 뒤에 불필요한 문자가 남아있습니다.");
            }
        }

    private:
        const std::string& text_;
        size_t pos_ = 0;

        void SkipWhitespace()
        {
            while (pos_ < text_.size() &&
                   (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r'))
            {
                ++pos_;
            }
        }

        char Peek()
        {
            if (pos_ >= text_.size()) throw JsonParseError("예상치 못하게 문자열이 끝났습니다.");
            return text_[pos_];
        }

        void Expect(char expected)
        {
            if (Peek() != expected)
            {
                throw JsonParseError(std::string("'") + expected + "' 문자가 필요합니다.");
            }
            ++pos_;
        }

        JsonValue ParseObject()
        {
            Expect('{');
            JsonValue obj = JsonValue::MakeObject();
            SkipWhitespace();
            if (Peek() == '}')
            {
                ++pos_;
                return obj;
            }

            while (true)
            {
                SkipWhitespace();
                std::string key = ParseString();
                SkipWhitespace();
                Expect(':');
                JsonValue value = ParseValue();
                obj[key] = std::move(value);

                SkipWhitespace();
                char next = Peek();
                if (next == ',')
                {
                    ++pos_;
                    continue;
                }
                if (next == '}')
                {
                    ++pos_;
                    break;
                }
                throw JsonParseError("객체 안에서 ',' 또는 '}' 가 필요합니다.");
            }
            return obj;
        }

        JsonValue ParseArray()
        {
            Expect('[');
            JsonValue arr = JsonValue::MakeArray();
            SkipWhitespace();
            if (Peek() == ']')
            {
                ++pos_;
                return arr;
            }

            while (true)
            {
                JsonValue value = ParseValue();
                arr.PushBack(std::move(value));

                SkipWhitespace();
                char next = Peek();
                if (next == ',')
                {
                    ++pos_;
                    continue;
                }
                if (next == ']')
                {
                    ++pos_;
                    break;
                }
                throw JsonParseError("배열 안에서 ',' 또는 ']' 가 필요합니다.");
            }
            return arr;
        }

        std::string ParseString()
        {
            Expect('"');
            std::string result;
            while (true)
            {
                char c = Peek();
                ++pos_;
                if (c == '"') break;
                if (c == '\\')
                {
                    char esc = Peek();
                    ++pos_;
                    switch (esc)
                    {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    default:
                        throw JsonParseError("지원하지 않는 이스케이프 시퀀스입니다.");
                    }
                }
                else
                {
                    result += c;
                }
            }
            return result;
        }

        JsonValue ParseBool()
        {
            if (text_.compare(pos_, 4, "true") == 0) { pos_ += 4; return JsonValue(true); }
            if (text_.compare(pos_, 5, "false") == 0) { pos_ += 5; return JsonValue(false); }
            throw JsonParseError("true/false 리터럴을 파싱할 수 없습니다.");
        }

        JsonValue ParseNull()
        {
            if (text_.compare(pos_, 4, "null") == 0) { pos_ += 4; return JsonValue(nullptr); }
            throw JsonParseError("null 리터럴을 파싱할 수 없습니다.");
        }

        JsonValue ParseNumber()
        {
            size_t start = pos_;
            if (Peek() == '-') ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
            if (pos_ < text_.size() && text_[pos_] == '.')
            {
                ++pos_;
                while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
            }
            if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E'))
            {
                ++pos_;
                if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
                while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
            }
            std::string numberText = text_.substr(start, pos_ - start);
            return JsonValue(std::stod(numberText));
        }
    };
}

JsonValue JsonValue::Parse(const std::string& text)
{
    Parser parser(text);
    JsonValue value = parser.ParseValue();
    parser.ExpectEndOfInput();
    return value;
}
