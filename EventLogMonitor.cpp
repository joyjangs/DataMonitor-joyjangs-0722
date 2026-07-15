#include "EventLogMonitor.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

EventLogMonitor::EventLogMonitor(std::string logFilePath)
    : logFilePath_(std::move(logFilePath))
{
}

std::string EventLogMonitor::CurrentTimestamp()
{
    // <chrono>로 "지금 이 순간"을 구하고, <ctime>의 localtime으로 사람이 읽는
    // 연-월-일 시:분:초 형식으로 바꿔줍니다. C++20에는 더 편한 std::chrono::zoned_time
    // 등도 있지만, 컴파일 환경에 따라 타임존 데이터가 없을 수 있어 이식성이 좋은
    // 전통적인 <ctime> 방식을 사용했습니다.
    auto now = std::chrono::system_clock::now();
    std::time_t nowTimeT = std::chrono::system_clock::to_time_t(now);
    std::tm localTm{};
    localtime_s(&localTm, &nowTimeT); // Windows(MSVC) 전용 안전한 버전의 localtime

    std::ostringstream oss;
    oss << std::put_time(&localTm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void EventLogMonitor::WriteLine(const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::string line = "[" + CurrentTimestamp() + "] " + message;

    // 1) 콘솔에 즉시 출력 (실시간으로 눈에 보이는 모니터링)
    std::cout << line << std::endl;

    // 2) events.log 파일 끝에 이어서 기록 (프로그램 종료 후에도 남는 감사 기록)
    std::ofstream out(logFilePath_, std::ios::app);
    if (out.is_open())
    {
        out << line << "\n";
    }
}

void EventLogMonitor::OnCreate(const Person& created, const std::vector<Person>& /*all*/)
{
    WriteLine("[CREATE] id=" + std::to_string(created.id) +
               " name=" + created.name +
               " age=" + std::to_string(created.age) +
               " email=" + created.email);
}

void EventLogMonitor::OnUpdate(const Person& before, const Person& after, const std::vector<Person>& /*all*/)
{
    WriteLine("[UPDATE] id=" + std::to_string(after.id) +
               " name: " + before.name + " -> " + after.name +
               ", age: " + std::to_string(before.age) + " -> " + std::to_string(after.age) +
               ", email: " + before.email + " -> " + after.email);
}

void EventLogMonitor::OnDelete(const Person& deleted, const std::vector<Person>& /*all*/)
{
    WriteLine("[DELETE] id=" + std::to_string(deleted.id) +
               " name=" + deleted.name);
}

void EventLogMonitor::OnExternalReload(const std::vector<Person>& all)
{
    WriteLine("[EXTERNAL RELOAD] 데이터 파일이 프로그램 외부에서 변경되어 다시 불러왔습니다. (총 " +
               std::to_string(all.size()) + "건)");
}
