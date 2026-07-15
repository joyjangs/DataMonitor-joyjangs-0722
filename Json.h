#pragma once

// ============================================================================
// Json.h
// ----------------------------------------------------------------------------
// 아주 작은(minimal), 외부 라이브러리에 의존하지 않는 JSON 처리 모듈입니다.
// 참고 레포(DataPersistence-joyjangs-0722)에서도 자체 JSON 파서를 만들어
// 사용했기 때문에, DataMonitor도 같은 방식을 그대로 따릅니다.
//
// JSON(JavaScript Object Notation)은 사람이 읽기 쉬운 텍스트 기반의 데이터
// 표현 형식입니다. 예를 들어 사람 한 명의 정보는 다음과 같이 표현됩니다.
//   { "id": 1, "name": "홍길동", "age": 20, "email": "a@a.com" }
// 그리고 여러 명의 정보는 배열(Array)로 감싸서 표현합니다.
//   [ {...}, {...}, {...} ]
//
// JsonValue 클래스는 이런 JSON 문서를 메모리에 표현하는 "값(Value)" 하나를
// 의미합니다. JSON에는 6가지 값의 종류(타입)가 있으며, JsonValue는 그중
// 어떤 타입이든 담을 수 있는 다목적 그릇(container) 역할을 합니다.
// ============================================================================

#include <string>
#include <vector>
#include <utility>
#include <stdexcept>

// JSON이 표현할 수 있는 값의 종류.
// enum class를 사용하면 이름 충돌 없이(예: Type::Array vs 다른 열거형의 Array)
// 타입 안전하게 값을 구분할 수 있습니다.
enum class JsonType
{
    Null,    // 값이 없음 (JSON의 null)
    Boolean, // true / false
    Number,  // 정수/실수 (JSON은 정수와 실수를 구분하지 않으므로 double 하나로 통일해서 저장)
    String,  // 문자열
    Array,   // 값들의 목록 [ ... ]
    Object   // "키: 값" 쌍들의 목록 { ... }
};

class JsonValue
{
public:
    // 기본 생성자: 아무 값도 지정하지 않으면 null 값이 됩니다.
    JsonValue();

    // 아래 생성자들은 "암시적 변환(implicit conversion)"을 이용해
    // JsonValue json = JsonValue(person.id); 처럼 자연스럽게 쓸 수 있게 해줍니다.
    JsonValue(std::nullptr_t);
    JsonValue(bool value);
    JsonValue(int value);
    JsonValue(double value);
    JsonValue(const std::string& value);
    JsonValue(const char* value);

    // 빈 배열/빈 객체를 만드는 팩토리(factory) 함수.
    // 생성자 대신 static 함수로 제공하는 이유는 JsonValue(0)과 JsonValue(Array)처럼
    // 인자만으로 어떤 타입을 만들지 구분하기 애매한 경우를 피하기 위함입니다.
    static JsonValue MakeArray();
    static JsonValue MakeObject();

    // 현재 값의 타입 확인용 헬퍼들.
    bool IsNull() const;
    bool IsObject() const;
    bool IsArray() const;
    JsonType GetType() const { return type_; }

    // ---- 객체(Object) 전용 접근자 ----
    // 비-const 버전: json["id"] = 1; 처럼 "쓰기"에 사용됩니다.
    // 만약 현재 값이 Null이었다면 자동으로 Object로 바뀌고, 없는 키라면 새로 추가합니다.
    JsonValue& operator[](const std::string& key);
    // const 버전: 읽기 전용 접근. 키가 없으면 예외를 던집니다(Repository에서 실수로
    // 잘못된 필드를 읽으려 할 때 조용히 넘어가지 않고 바로 알아채도록 하기 위함입니다).
    const JsonValue& operator[](const std::string& key) const;
    bool Contains(const std::string& key) const;

    // ---- 배열(Array) 전용 접근자 ----
    JsonValue& operator[](size_t index);
    const JsonValue& operator[](size_t index) const;
    void PushBack(JsonValue value);
    size_t Size() const;

    // ---- 값 꺼내기 ----
    // JSON은 타입이 느슨하기 때문에, "이 값을 int로 달라" 같은 식으로 꺼내는
    // 변환 함수들을 제공합니다. 저장된 타입과 요청한 타입이 다르면 예외를 던집니다.
    int AsInt() const;
    double AsDouble() const;
    bool AsBool() const;
    const std::string& AsString() const;

    // ---- 직렬화 / 역직렬화 ----
    // Dump: JsonValue -> 사람이 읽을 수 있는 JSON 텍스트 (파일에 저장할 때 사용)
    std::string Dump(int indent = 0) const;
    // Parse: JSON 텍스트 -> JsonValue (파일을 읽어올 때 사용)
    static JsonValue Parse(const std::string& text);

private:
    JsonType type_ = JsonType::Null;

    // 실제로는 아래 5개 멤버 중 type_에 해당하는 딱 하나만 의미 있는 값을 가집니다.
    // (std::variant를 쓰면 더 엄격하게 표현할 수 있지만, PoC 학습 목적상
    //  이해하기 쉬운 "각 타입별 필드를 다 두는" 방식을 택했습니다.)
    bool boolValue_ = false;
    double numberValue_ = 0.0;
    std::string stringValue_;
    std::vector<JsonValue> arrayValue_;
    // 객체는 std::map 대신 vector<pair<key, value>>로 표현합니다.
    // 이유: map은 키를 알파벳 순으로 재정렬해버려서 원래 넣은 순서(id, name, age, email)가
    // 흐트러집니다. vector로 두면 넣은 순서 그대로 저장/출력할 수 있어 파일을 눈으로
    // 확인하기 편해집니다. 대신 키 검색은 O(n)이지만, PoC 규모에서는 문제 없습니다.
    std::vector<std::pair<std::string, JsonValue>> objectValue_;

    void EnsureType(JsonType expected, const char* what) const;
    void DumpInternal(std::string& out, int indent, int currentDepth) const;
};

// 파싱 실패(문법이 잘못된 JSON) 시 던지는 예외 타입.
class JsonParseError : public std::runtime_error
{
public:
    explicit JsonParseError(const std::string& message) : std::runtime_error(message) {}
};
