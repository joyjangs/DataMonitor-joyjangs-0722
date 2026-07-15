#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <functional>
#include "Person.h"
#include "IRepositoryObserver.h"

// ============================================================================
// PersonRepository
// ----------------------------------------------------------------------------
// people.json 파일을 대상으로 CRUD(Create/Read/Update/Delete)를 수행하는
// 저장소 클래스입니다. 참고 레포의 PersonRepository와 기본 골격은 같지만,
// DataMonitor에서는 다음 두 가지가 추가되었습니다.
//
//  1) 옵저버 통지: 데이터가 바뀔 때마다 등록된 IRepositoryObserver들에게 알립니다.
//     (실시간 이벤트 로그 모니터링, 통계 모니터링이 이 알림을 받아서 동작합니다.)
//
//  2) 스레드 안전성(mutex): main 스레드(콘솔 메뉴 처리)와 FileWatcher가 사용하는
//     별도의 감시 스레드(background thread)가 동시에 people_ 벡터에 접근할 수
//     있기 때문에, std::mutex로 "한 번에 한 스레드만 데이터에 접근"하도록
//     보호합니다. 이런 보호가 없으면 두 스레드가 동시에 vector를 건드리다가
//     프로그램이 예측 불가능하게 죽을 수 있습니다(data race).
// ============================================================================
class PersonRepository
{
public:
    explicit PersonRepository(std::string filePath);

    // 프로그램 시작 시 1회 호출: 파일이 있으면 읽어오고, 없으면 빈 목록으로 시작합니다.
    void Load();

    // 옵저버(로그 모니터, 통계 모니터 등)를 등록합니다.
    void AddObserver(IRepositoryObserver* observer);

    // 저장(Save)에 성공할 때마다 호출되는 훅. FileWatcher에게 "방금 건 우리가
    // 직접 쓴 변경이니 외부 변경으로 착각하지 말라"고 알려주는 용도로 사용합니다.
    // (Repository는 FileWatcher의 존재를 몰라도 되도록, 구체적인 타입 대신
    //  std::function으로 느슨하게 연결합니다.)
    void SetAfterSaveHook(std::function<void()> hook);

    Person Create(const std::string& name, int age, const std::string& email);
    std::vector<Person> ReadAll() const;
    std::optional<Person> ReadById(int id) const;
    // 성공하면 true, id를 찾지 못하면 false를 반환합니다.
    bool Update(int id, const std::string& name, int age, const std::string& email);
    bool Delete(int id);

    // FileWatcher가 "우리가 쓴 게 아닌 변경"을 감지했을 때 호출합니다.
    // 디스크에서 다시 읽어들이고, OnExternalReload 이벤트를 발생시킵니다.
    void ReloadFromDisk();

    const std::string& FilePath() const { return filePath_; }

private:
    std::string filePath_;
    mutable std::mutex mutex_;               // people_/nextId_ 접근을 보호
    std::vector<Person> people_;
    std::vector<IRepositoryObserver*> observers_;
    std::function<void()> afterSaveHook_;
    int nextId_ = 1;

    // 이름 끝에 "_NoLock"이 붙은 함수들은 "호출하는 쪽에서 이미 mutex_를 잠근 뒤에만
    // 불러야 한다"는 뜻입니다. 락을 이중으로 잡으면(재진입 불가능한 mutex이므로)
    // 그 자리에서 멈춰버리기(deadlock) 때문에, 이미 잠겨있는 상태에서 호출할
    // 내부 함수와 그렇지 않은 공개 함수를 이름으로 구분해 실수를 방지합니다.
    void SaveInternal_NoLock();

    void NotifyCreate(const Person& created);
    void NotifyUpdate(const Person& before, const Person& after);
    void NotifyDelete(const Person& deleted);
    void NotifyExternalReload(const std::vector<Person>& reloaded);
};
