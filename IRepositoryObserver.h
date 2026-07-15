#pragma once

#include <vector>
#include "Person.h"

// ============================================================================
// IRepositoryObserver
// ----------------------------------------------------------------------------
// "옵저버 패턴(Observer Pattern)" 인터페이스입니다.
//
// PersonRepository(데이터 저장소)는 데이터가 생성/수정/삭제될 때마다 "누가 이걸
// 구독하고 있는지" 알 필요 없이, 등록된 옵저버들에게 그냥 "이런 일이 있었어!"
// 라고 알려주기만 합니다(Notify). 그러면 각 옵저버가 알아서 자기 할 일(로그를
// 남기거나, 통계를 다시 계산하거나)을 합니다.
//
// 이렇게 분리해두면 "실시간 이벤트 로그 모니터링"을 담당하는 EventLogMonitor와
// "통계/이상치 모니터링"을 담당하는 StatsMonitor가 서로 전혀 몰라도 되고,
// PersonRepository의 CRUD 코드도 로그나 통계 계산 코드로 지저분해지지 않습니다.
//
// 이 인터페이스는 순수 가상 함수(= 0)만 가진 "추상 클래스"입니다. C++에는 Java의
// interface 키워드가 없어서, 모든 메서드를 순수 가상으로 선언한 클래스로 흉내냅니다.
// ============================================================================
class IRepositoryObserver
{
public:
    virtual ~IRepositoryObserver() = default;

    // 모든 콜백은 이벤트 당사자뿐 아니라 "이벤트 발생 직후의 전체 데이터 목록(all)"도
    // 함께 받습니다. 그래야 StatsMonitor처럼 전체 목록을 훑어야 하는 옵저버가 매번
    // Repository에 별도로 다시 물어보지 않고도 통계/이상치를 계산할 수 있습니다.

    // 새 레코드가 생성되었을 때 호출됩니다.
    virtual void OnCreate(const Person& created, const std::vector<Person>& all) = 0;

    // 기존 레코드가 수정되었을 때 호출됩니다. (수정 전/후 값을 모두 전달)
    virtual void OnUpdate(const Person& before, const Person& after, const std::vector<Person>& all) = 0;

    // 레코드가 삭제되었을 때 호출됩니다.
    virtual void OnDelete(const Person& deleted, const std::vector<Person>& all) = 0;

    // 파일 감시(FileWatcher)에 의해 "외부에서" 데이터 파일이 변경되어
    // 전체 데이터를 다시 불러왔을 때 호출됩니다.
    virtual void OnExternalReload(const std::vector<Person>& all) = 0;
};
