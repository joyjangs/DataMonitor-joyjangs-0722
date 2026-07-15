#include "FileWatcher.h"
#include <chrono>

namespace fs = std::filesystem;

FileWatcher::FileWatcher(std::string watchedFilePath, int pollIntervalMs, std::function<void()> onExternalChange)
    : watchedFilePath_(std::move(watchedFilePath))
    , pollIntervalMs_(pollIntervalMs)
    , onExternalChange_(std::move(onExternalChange))
{
}

FileWatcher::~FileWatcher()
{
    // 소멸자에서 반드시 스레드를 정지시킵니다. 만약 스레드가 아직 돌고 있는데
    // FileWatcher 객체가 사라지면(join도 detach도 안 된 채로), std::thread
    // 소멸자가 std::terminate()를 호출해 프로그램이 강제 종료됩니다.
    Stop();
}

void FileWatcher::Start()
{
    if (running_) return; // 이미 실행 중이면 중복 시작하지 않음

    // 감시를 시작하는 시점의 파일 수정 시각을 기준선으로 삼습니다.
    MarkKnownWrite();

    running_ = true;
    // std::thread 생성자에 멤버 함수를 넘길 때는 "이 멤버 함수를 어느 객체(this)에
    // 대해 실행할지"를 함께 지정해야 합니다.
    thread_ = std::thread(&FileWatcher::WatchLoop, this);
}

void FileWatcher::Stop()
{
    if (!running_) return;

    running_ = false;      // 감시 스레드에게 "그만 돌아도 된다"고 신호를 보냄
    if (thread_.joinable())
    {
        thread_.join();    // 감시 스레드가 실제로 끝날 때까지 기다림
    }
}

bool FileWatcher::IsRunning() const
{
    return running_;
}

void FileWatcher::MarkKnownWrite()
{
    std::lock_guard<std::mutex> lock(baselineMutex_);
    std::error_code ec;
    auto writeTime = fs::last_write_time(watchedFilePath_, ec);
    if (!ec)
    {
        lastKnownWriteTime_ = writeTime;
        hasBaseline_ = true;
    }
    // 파일이 아직 없다면(ec가 설정됨) 기준선을 세우지 않고 넘어갑니다.
    // 다음 폴링에서 파일이 생기면 그때 자연스럽게 "변경"으로 감지됩니다.
}

void FileWatcher::WatchLoop()
{
    while (running_)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs_));
        if (!running_) break; // Stop()이 sleep 도중 호출되었을 수도 있으므로 다시 확인

        std::error_code ec;
        auto currentWriteTime = fs::last_write_time(watchedFilePath_, ec);
        if (ec)
        {
            continue; // 파일이 일시적으로 없거나 접근할 수 없으면 다음 주기에 다시 시도
        }

        bool changedByOthers = false;
        {
            std::lock_guard<std::mutex> lock(baselineMutex_);
            if (!hasBaseline_ || currentWriteTime != lastKnownWriteTime_)
            {
                lastKnownWriteTime_ = currentWriteTime;
                hasBaseline_ = true;
                changedByOthers = true;
            }
        }

        if (changedByOthers && onExternalChange_)
        {
            onExternalChange_();
        }
    }
}
