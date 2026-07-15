#include "StatsMonitor.h"
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <iomanip>

StatsMonitor::StatsMonitor(int minValidAge, int maxValidAge)
    : minValidAge_(minValidAge), maxValidAge_(maxValidAge)
{
}

void StatsMonitor::OnCreate(const Person& /*created*/, const std::vector<Person>& all)
{
    PrintReport(all);
}

void StatsMonitor::OnUpdate(const Person& /*before*/, const Person& /*after*/, const std::vector<Person>& all)
{
    PrintReport(all);
}

void StatsMonitor::OnDelete(const Person& /*deleted*/, const std::vector<Person>& all)
{
    PrintReport(all);
}

void StatsMonitor::OnExternalReload(const std::vector<Person>& all)
{
    PrintReport(all);
}

void StatsMonitor::PrintReport(const std::vector<Person>& people) const
{
    std::cout << "\n--- [통계/이상치 모니터링] --------------------------------\n";

    if (people.empty())
    {
        std::cout << "데이터가 없습니다.\n";
        std::cout << "------------------------------------------------------------\n\n";
        return;
    }

    // ---- 기본 통계: 개수, 평균/최소/최대 나이 ----
    long long ageSum = 0;
    int minAge = people.front().age;
    int maxAge = people.front().age;
    for (const auto& p : people)
    {
        ageSum += p.age;
        minAge = std::min(minAge, p.age);
        maxAge = std::max(maxAge, p.age);
    }
    double averageAge = static_cast<double>(ageSum) / static_cast<double>(people.size());

    std::cout << "전체 인원 수 : " << people.size() << "명\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "평균 나이    : " << averageAge << "세\n";
    std::cout << "최소/최대 나이: " << minAge << "세 / " << maxAge << "세\n";

    // ---- 이상치 탐지 ----
    // 1) 이메일 중복 카운트: 이메일을 key로 하는 해시맵에 등장 횟수를 누적합니다.
    std::unordered_map<std::string, int> emailCount;
    for (const auto& p : people)
    {
        if (!p.email.empty())
        {
            emailCount[p.email]++;
        }
    }

    std::vector<std::string> anomalies;
    for (const auto& p : people)
    {
        if (p.age <= minValidAge_ || p.age > maxValidAge_)
        {
            anomalies.push_back("id=" + std::to_string(p.id) + " 나이가 비정상적입니다 (age=" +
                                 std::to_string(p.age) + ")");
        }
        if (p.name.empty())
        {
            anomalies.push_back("id=" + std::to_string(p.id) + " 이름이 비어있습니다");
        }
        if (p.email.empty())
        {
            anomalies.push_back("id=" + std::to_string(p.id) + " 이메일이 비어있습니다");
        }
        else if (emailCount[p.email] > 1)
        {
            anomalies.push_back("id=" + std::to_string(p.id) + " 이메일이 중복됩니다 (email=" + p.email + ")");
        }
    }

    if (anomalies.empty())
    {
        std::cout << "이상치 없음: 모든 데이터가 정상 범위입니다.\n";
    }
    else
    {
        std::cout << "이상치 " << anomalies.size() << "건 발견:\n";
        for (const auto& message : anomalies)
        {
            std::cout << "  ! " << message << "\n";
        }
    }
    std::cout << "------------------------------------------------------------\n\n";
}
