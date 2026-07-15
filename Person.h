#pragma once

#include <string>
#include "Json.h"

// ============================================================================
// Person
// ----------------------------------------------------------------------------
// DataMonitor가 감시(모니터링)할 대상이 되는 데이터 한 건을 표현하는 구조체입니다.
// 참고 레포(DataPersistence-joyjangs-0722)의 Person 구조를 그대로 재사용했습니다.
//
// struct와 class의 차이는 딱 하나, 기본 접근 제어자가 public이냐(struct) private이냐
// (class)입니다. 이 구조체는 필드를 자유롭게 읽고 쓰는 순수 데이터 묶음(POD에 가까움)
// 이라서 굳이 class로 캡슐화하지 않고 struct로 선언했습니다.
// ============================================================================
struct Person
{
    int id = 0;             // 레코드 고유 번호. PersonRepository가 자동으로 채번합니다.
    std::string name;       // 이름
    int age = 0;            // 나이 (StatsMonitor가 이상치 여부를 판단하는 데 사용)
    std::string email;      // 이메일 (StatsMonitor가 중복 여부를 판단하는 데 사용)

    // Person -> JSON. people.json 파일에 저장할 때 호출됩니다.
    JsonValue ToJson() const;

    // JSON -> Person. people.json 파일을 읽어올 때 호출됩니다.
    static Person FromJson(const JsonValue& json);
};
