#pragma once

#include <string>
#include <mutex>
#include "IRepositoryObserver.h"

// ============================================================================
// EventLogMonitor
// ----------------------------------------------------------------------------
// [모니터링 기능 1] 실시간 이벤트 로그 모니터링
//
// PersonRepository에서 CRUD가 일어나거나, FileWatcher가 외부 변경을 감지해
// 데이터를 다시 불러올 때마다 IRepositoryObserver를 통해 알림을 받아,
//   - 콘솔 화면에 "언제, 무슨 일이 있었는지" 즉시 출력하고
//   - events.log 파일에도 같은 내용을 이어서 기록(append)합니다.
//
// 이렇게 하면 프로그램을 종료한 뒤에도 events.log를 열어 "언제 무슨 변경이
// 있었는지"를 감사 기록(audit log)처럼 확인할 수 있습니다.
// ============================================================================
class EventLogMonitor : public IRepositoryObserver
{
public:
    explicit EventLogMonitor(std::string logFilePath);

    void OnCreate(const Person& created, const std::vector<Person>& all) override;
    void OnUpdate(const Person& before, const Person& after, const std::vector<Person>& all) override;
    void OnDelete(const Person& deleted, const std::vector<Person>& all) override;
    void OnExternalReload(const std::vector<Person>& all) override;

private:
    std::string logFilePath_;
    std::mutex mutex_; // 콘솔 출력 + 파일 append 스레드 안전성 보호(FileWatcher 스레드에서도 호출될 수 있음)

    // 공통 로직: "[시간] 메시지" 형태로 콘솔에 출력하고 파일에도 append합니다.
    void WriteLine(const std::string& message);
    static std::string CurrentTimestamp();
};
