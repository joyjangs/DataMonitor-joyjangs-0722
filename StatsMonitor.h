#pragma once

#include <vector>
#include "IRepositoryObserver.h"

// ============================================================================
// StatsMonitor
// ----------------------------------------------------------------------------
// [모니터링 기능 2] 데이터 통계 / 이상치(anomaly) 모니터링
//
// 데이터가 바뀔 때마다(OnCreate/OnUpdate/OnDelete/OnExternalReload) 전체 목록을
// 다시 훑어보면서
//   - 기본 통계: 전체 인원 수, 평균/최소/최대 나이
//   - 이상치: 나이가 비정상적인 값(0 이하이거나 120 초과), 이름/이메일이 비어있음,
//             이메일 중복
// 을 계산해서 콘솔에 보여줍니다. "이상치가 있다"는 사실 자체가 데이터 입력 과정에
// 문제가 있었다는 신호이므로, 모니터링 시스템에서 매우 중요한 부분입니다.
//
// PrintReport()는 옵저버 콜백 없이도(메뉴에서 "통계 보기"를 직접 선택했을 때)
// 재사용할 수 있도록 public으로 분리해 두었습니다.
// ============================================================================
class StatsMonitor : public IRepositoryObserver
{
public:
    // 나이의 정상 범위를 [minValidAge, maxValidAge]로 간주합니다. 이 범위를 벗어나면
    // "이상치"로 보고합니다. (사람 나이 기준으로 0~120을 기본값으로 사용합니다.)
    StatsMonitor(int minValidAge = 0, int maxValidAge = 120);

    void OnCreate(const Person& created, const std::vector<Person>& all) override;
    void OnUpdate(const Person& before, const Person& after, const std::vector<Person>& all) override;
    void OnDelete(const Person& deleted, const std::vector<Person>& all) override;
    void OnExternalReload(const std::vector<Person>& all) override;

    // 현재 전체 데이터 목록을 받아 통계/이상치 리포트를 콘솔에 출력합니다.
    void PrintReport(const std::vector<Person>& people) const;

private:
    int minValidAge_;
    int maxValidAge_;
};
