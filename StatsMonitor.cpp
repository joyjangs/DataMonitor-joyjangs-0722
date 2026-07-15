#include "StatsMonitor.h"
#include <iostream>
#include <algorithm>
#include <array>
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

    // ---- 나이대별 분포 ----
    PrintAgeDistribution(people);

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

void StatsMonitor::PrintAgeDistribution(const std::vector<Person>& people) const
{
    // 나이를 10 단위 구간(10대, 20대, ...)으로 묶는 것을 "버킷(bucket)에 담는다"고
    // 표현합니다. 배열의 인덱스 하나가 나이대 구간 하나에 대응됩니다.
    //   인덱스 0  : 10세 미만
    //   인덱스 1  : 10대(10~19세)
    //   인덱스 2  : 20대(20~29세)
    //   ...
    //   인덱스 9  : 90대(90~99세)
    //   인덱스 10 : 100세 이상
    // std::array<int, 11>은 크기가 11로 고정된 배열이며, {}로 초기화하면 모든
    // 원소가 0으로 채워집니다.
    std::array<int, 11> bucketCounts{};
    for (const auto& p : people)
    {
        int bucketIndex;
        if (p.age < 10)
        {
            bucketIndex = 0;
        }
        else if (p.age >= 100)
        {
            bucketIndex = 10;
        }
        else
        {
            bucketIndex = p.age / 10; // 정수 나눗셈: 23 / 10 == 2 (20대)
        }
        bucketCounts[bucketIndex]++;
    }

    const int totalCount = static_cast<int>(people.size());

    std::cout << "\n총 인원 : " << totalCount << "명\n";
    std::cout << "나이대별 인원\n";

    // 각 나이대 한 줄을 "라벨 : N명 ( 비율%) 그래프" 형태로 출력하는 도우미 람다.
    // 10대~90대는 항상 출력하고, 범위를 벗어난 "10세 미만"/"100세 이상"은
    // 실제로 해당하는 사람이 있을 때만 출력해 요청하신 출력 형태를 그대로 지킵니다.
    auto printBucketLine = [&](const std::string& label, int count, bool alwaysShow)
    {
        if (count == 0 && !alwaysShow) return;

        double ratio = totalCount > 0 ? (static_cast<double>(count) / totalCount * 100.0) : 0.0;

        // 그래프(막대)는 5% 당 블록 하나로 표현합니다. 예를 들어 비율이 33%면
        // 블록 6~7개 정도가 그려져서, 숫자를 눈으로 다시 계산하지 않아도 한눈에
        // "많다/적다"를 비교할 수 있습니다.
        int barLength = static_cast<int>(ratio / 5.0 + 0.5); // 0.5를 더해 반올림 효과
        std::string bar;
        bar.reserve(barLength * 3); // "■" 한 글자가 UTF-8로 3바이트이므로 미리 예약
        for (int i = 0; i < barLength; ++i)
        {
            bar += "■";
        }

        std::cout << "  " << std::left << std::setw(8) << label << ": "
                  << std::right << std::setw(3) << count << "명 ("
                  << std::fixed << std::setprecision(1) << std::setw(5) << ratio << "%) "
                  << bar << "\n";
    };

    printBucketLine("10세 미만", bucketCounts[0], /*alwaysShow=*/false);
    printBucketLine("10대", bucketCounts[1], true);
    printBucketLine("20대", bucketCounts[2], true);
    printBucketLine("30대", bucketCounts[3], true);
    printBucketLine("40대", bucketCounts[4], true);
    printBucketLine("50대", bucketCounts[5], true);
    printBucketLine("60대", bucketCounts[6], true);
    printBucketLine("70대", bucketCounts[7], true);
    printBucketLine("80대", bucketCounts[8], true);
    printBucketLine("90대", bucketCounts[9], true);
    printBucketLine("100세 이상", bucketCounts[10], /*alwaysShow=*/false);
    std::cout << "\n";
}
