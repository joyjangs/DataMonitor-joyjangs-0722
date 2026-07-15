#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <filesystem>

// ============================================================================
// FileWatcher
// ----------------------------------------------------------------------------
// [모니터링 기능 3] 파일 변경 감지
//
// people.json 파일의 "마지막 수정 시각(last write time)"을 별도의 스레드에서
// 주기적으로 확인(polling)하다가, 우리 프로그램이 직접 저장한 것이 아닌 변경을
// 발견하면 콜백을 호출합니다. 즉, 프로그램이 실행 중인 동안 메모장 등으로
// people.json을 직접 열어 고쳐도 그 변화를 실시간으로 잡아낼 수 있습니다.
//
// 왜 "폴링(polling, 주기적으로 확인하기)" 방식인가?
//   운영체제마다 파일 변경을 실시간으로 알려주는 전용 API가 다릅니다
//   (Windows: ReadDirectoryChangesW, Linux: inotify 등). PoC 학습 목적으로는
//   플랫폼에 독립적이고 이해하기 쉬운 "1초마다 파일의 수정 시각을 확인한다"는
//   방식이 더 적합하다고 판단했습니다.
//
// 스레드 관련 핵심 개념:
//   - std::thread: 별도의 실행 흐름(스레드)을 하나 더 만듭니다. Start()를 호출하면
//     main 스레드와 "동시에" 감시용 스레드가 돌기 시작합니다.
//   - std::atomic<bool>: 여러 스레드가 동시에 읽고 쓰는 bool 변수는 반드시
//     atomic으로 선언해야 합니다. 일반 bool은 "동시에 읽고 쓰기"에 대한 보장이
//     없어서 감시 스레드를 멈추라는 신호(running_ = false)가 씹힐 수 있습니다.
// ============================================================================
class FileWatcher
{
public:
    // watchedFilePath : 감시할 파일 경로
    // pollIntervalMs  : 몇 밀리초마다 파일을 확인할지
    // onExternalChange: 우리가 쓴 게 아닌 변경이 감지되었을 때 호출할 콜백
    FileWatcher(std::string watchedFilePath, int pollIntervalMs, std::function<void()> onExternalChange);
    ~FileWatcher();

    // 감시 스레드를 시작/중지합니다. (메뉴에서 켜고 끌 수 있도록 공개 함수로 제공)
    void Start();
    void Stop();
    bool IsRunning() const;

    // Repository가 "방금 저장은 우리가 한 것"이라고 알려줄 때 호출합니다.
    // 이 시점의 파일 수정 시각을 기준선(baseline)으로 갱신해서, 다음 폴링에서
    // 같은 변경을 "외부 변경"으로 다시 보고하지 않도록 합니다.
    void MarkKnownWrite();

private:
    std::string watchedFilePath_;
    int pollIntervalMs_;
    std::function<void()> onExternalChange_;

    std::thread thread_;
    std::atomic<bool> running_{false};

    std::mutex baselineMutex_;
    std::filesystem::file_time_type lastKnownWriteTime_{};
    bool hasBaseline_ = false;

    void WatchLoop();
};
