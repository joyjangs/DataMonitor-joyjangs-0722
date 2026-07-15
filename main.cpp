// ============================================================================
// main.cpp
// ----------------------------------------------------------------------------
// DataMonitor - 데이터 모니터링 시스템 PoC
//
// 참고 레포(DataPersistence-joyjangs-0722)의 "Person 데이터를 JSON 파일로
// CRUD하는 콘솔 프로그램" 구조를 그대로 재사용하되, 여기에 세 가지 모니터링
// 기능을 추가했습니다.
//
//   1) EventLogMonitor : CRUD가 일어날 때마다 "언제, 무슨 일이 있었는지"를
//                        콘솔 + events.log 파일에 실시간으로 기록합니다.
//   2) StatsMonitor    : 데이터가 바뀔 때마다 전체 데이터의 통계(평균 나이 등)와
//                        이상치(비정상 나이, 중복 이메일 등)를 계산해 보여줍니다.
//   3) FileWatcher     : people.json 파일이 "프로그램을 거치지 않고" 외부에서
//                        (예: 메모장으로) 수정되는 것을 감지해 자동으로 다시
//                        읽어들이고, 이 사실도 위 두 모니터에게 알립니다.
//
// 이 세 기능은 모두 IRepositoryObserver라는 하나의 인터페이스를 통해
// PersonRepository에 "구독"하는 형태로 연결됩니다(옵저버 패턴). main()은
// 이들을 생성해서 연결(wiring)해주는 역할만 합니다.
// ============================================================================

#include <iostream>
#include <limits>
#include <string>

// Windows.h는 기본적으로 max/min을 매크로로 정의하는데, 이 매크로가
// std::numeric_limits<...>::max() 같은 표준 라이브러리 호출과 충돌합니다.
// NOMINMAX를 Windows.h보다 먼저 정의해서 이 매크로 정의 자체를 막습니다.
#define NOMINMAX
#include <Windows.h>

#include "PersonRepository.h"
#include "EventLogMonitor.h"
#include "StatsMonitor.h"
#include "FileWatcher.h"

namespace
{
    // 데이터 파일 및 로그 파일 경로. 실행 파일과 같은 폴더에 생성됩니다.
    constexpr const char* kDataFilePath = "people.json";
    constexpr const char* kEventLogFilePath = "events.log";
    // 파일 감시 주기(밀리초). 짧을수록 빨리 감지하지만 CPU를 더 씁니다. PoC라 1초로 설정.
    constexpr int kWatchPollIntervalMs = 1000;

    void PrintMenu()
    {
        std::cout << "\n============= DataMonitor =============\n"
                  << " 1. 사람 등록 (Create)\n"
                  << " 2. 전체 조회 (Read All)\n"
                  << " 3. ID로 조회 (Read by Id)\n"
                  << " 4. 수정 (Update)\n"
                  << " 5. 삭제 (Delete)\n"
                  << " 6. 통계 / 이상치 보기\n"
                  << " 7. 파일 변경 감시 시작/중지 전환\n"
                  << " 0. 종료\n"
                  << "=========================================\n"
                  << "선택: ";
    }

    // 표준입력(cin)은 "3abc" 같은 잘못된 형식이 들어오면 실패 상태(fail bit)가 되어
    // 이후 입력을 전부 무시해버립니다. 매번 실패 상태를 풀어주고(clear), 버퍼에 남은
    // 나머지 입력을 지워주는(ignore) 이 함수를 거쳐야 다음 입력을 정상적으로 받을 수
    // 있습니다. C++ 콘솔 입출력을 다룰 때 자주 나오는 관용구(idiom)입니다.
    // 입력 스트림이 완전히 끊어졌을 때(EOF, 예: 입력 리다이렉션이 끝났거나 파이프가
    // 닫힌 경우) 종료를 요청하는 신호로 0(메뉴의 "종료")을 반환합니다. 이 처리가
    // 없으면 std::cin이 실패 상태에서 회복되지 않아 "숫자를 입력해주세요"가
    // 무한히 반복 출력되는 버그가 생깁니다.
    int ReadInt(const std::string& prompt)
    {
        while (true)
        {
            std::cout << prompt;
            int value;
            if (std::cin >> value)
            {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                return value;
            }
            if (std::cin.eof())
            {
                return 0;
            }
            std::cout << "숫자를 입력해주세요.\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }

    std::string ReadLine(const std::string& prompt)
    {
        std::cout << prompt;
        std::string line;
        std::getline(std::cin, line);
        return line;
    }

    void PrintPerson(const Person& person)
    {
        std::cout << "  [id=" << person.id << "] "
                  << "이름=" << person.name
                  << ", 나이=" << person.age
                  << ", 이메일=" << person.email << "\n";
    }
}

int main()
{
    // 이 프로젝트는 /utf-8 컴파일 옵션으로 빌드되어, 소스 코드에 적힌 한글 문자열
    // 리터럴("이름: " 등)이 실행 파일 안에 "UTF-8 바이트"로 그대로 저장됩니다.
    // 반면 Windows 콘솔(cmd.exe)의 기본 코드페이지는 한국어 환경에서도 CP949(EUC-KR
    // 계열)인 경우가 많아서, UTF-8 바이트를 CP949 규칙으로 잘못 해석해 한글이
    // 깨져 보이는 문제가 생깁니다. 프로그램 시작 시 콘솔의 입출력 코드페이지를
    // 우리가 실제로 사용하는 인코딩인 UTF-8(65001)로 맞춰서 이 문제를 해결합니다.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 1) 데이터 저장소 준비: 프로그램 시작 시 기존 people.json을 읽어옵니다.
    PersonRepository repository(kDataFilePath);
    repository.Load();

    // 2) 모니터 두 개를 만들고, Repository에 "구독자"로 등록합니다.
    //    이 시점부터 Create/Update/Delete/ReloadFromDisk가 호출될 때마다
    //    아래 두 객체의 On... 함수가 자동으로 실행됩니다.
    EventLogMonitor eventLogMonitor(kEventLogFilePath);
    StatsMonitor statsMonitor;
    repository.AddObserver(&eventLogMonitor);
    repository.AddObserver(&statsMonitor);

    // 3) 파일 감시자 준비: people.json이 외부에서 바뀌면 repository.ReloadFromDisk()를
    //    호출하도록 콜백을 연결합니다. ReloadFromDisk 내부에서 옵저버들의
    //    OnExternalReload가 호출되므로, 로그/통계도 자동으로 갱신됩니다.
    FileWatcher fileWatcher(kDataFilePath, kWatchPollIntervalMs,
                            [&repository]() { repository.ReloadFromDisk(); });

    // Repository가 저장(Save)에 성공할 때마다, FileWatcher에게 "이건 우리가 쓴
    // 변경"이라고 알려서, 우리가 방금 CRUD로 만든 변경을 "외부 변경"으로
    // 착각해 중복 보고하지 않도록 합니다.
    repository.SetAfterSaveHook([&fileWatcher]() { fileWatcher.MarkKnownWrite(); });

    // 프로그램 시작과 동시에 파일 감시를 켜 둡니다. (메뉴 7번으로 끄고 켤 수 있음)
    fileWatcher.Start();
    std::cout << "파일 변경 감시를 시작했습니다. (people.json을 다른 프로그램으로 직접 수정해도 감지됩니다)\n";

    // 시작 시점의 데이터로 한 번 통계를 보여줍니다.
    statsMonitor.PrintReport(repository.ReadAll());

    bool running = true;
    while (running)
    {
        PrintMenu();
        int choice = ReadInt("");

        switch (choice)
        {
        case 1: // Create
        {
            std::string name = ReadLine("이름: ");
            int age = ReadInt("나이: ");
            std::string email = ReadLine("이메일: ");
            Person created = repository.Create(name, age, email);
            std::cout << "등록되었습니다.\n";
            PrintPerson(created);
            break;
        }
        case 2: // Read All
        {
            std::vector<Person> all = repository.ReadAll();
            if (all.empty())
            {
                std::cout << "등록된 데이터가 없습니다.\n";
            }
            else
            {
                for (const auto& person : all) PrintPerson(person);
            }
            break;
        }
        case 3: // Read by Id
        {
            int id = ReadInt("조회할 ID: ");
            auto found = repository.ReadById(id);
            if (found)
            {
                PrintPerson(*found);
            }
            else
            {
                std::cout << "해당 ID의 데이터가 없습니다.\n";
            }
            break;
        }
        case 4: // Update
        {
            int id = ReadInt("수정할 ID: ");
            std::string name = ReadLine("새 이름: ");
            int age = ReadInt("새 나이: ");
            std::string email = ReadLine("새 이메일: ");
            if (repository.Update(id, name, age, email))
            {
                std::cout << "수정되었습니다.\n";
            }
            else
            {
                std::cout << "해당 ID의 데이터가 없습니다.\n";
            }
            break;
        }
        case 5: // Delete
        {
            int id = ReadInt("삭제할 ID: ");
            if (repository.Delete(id))
            {
                std::cout << "삭제되었습니다.\n";
            }
            else
            {
                std::cout << "해당 ID의 데이터가 없습니다.\n";
            }
            break;
        }
        case 6: // 통계/이상치 수동 조회
        {
            statsMonitor.PrintReport(repository.ReadAll());
            break;
        }
        case 7: // 파일 감시 토글
        {
            if (fileWatcher.IsRunning())
            {
                fileWatcher.Stop();
                std::cout << "파일 변경 감시를 중지했습니다.\n";
            }
            else
            {
                fileWatcher.Start();
                std::cout << "파일 변경 감시를 다시 시작했습니다.\n";
            }
            break;
        }
        case 0: // 종료
        {
            running = false;
            break;
        }
        default:
            std::cout << "올바르지 않은 선택입니다.\n";
            break;
        }
    }

    // FileWatcher의 소멸자가 감시 스레드를 안전하게 정지(join)시켜줍니다.
    std::cout << "DataMonitor를 종료합니다.\n";
    return 0;
}
