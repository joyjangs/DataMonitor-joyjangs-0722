#include "Person.h"

JsonValue Person::ToJson() const
{
    // MakeObject()로 빈 객체를 만든 뒤, operator[]로 필드를 하나씩 채워 넣습니다.
    JsonValue json = JsonValue::MakeObject();
    json["id"] = JsonValue(id);
    json["name"] = JsonValue(name);
    json["age"] = JsonValue(age);
    json["email"] = JsonValue(email);
    return json;
}

Person Person::FromJson(const JsonValue& json)
{
    Person person;
    person.id = json["id"].AsInt();
    person.name = json["name"].AsString();
    person.age = json["age"].AsInt();
    person.email = json["email"].AsString();
    return person;
}
