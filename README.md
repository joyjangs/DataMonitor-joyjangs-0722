# DataMonitor

JSON 파일로 저장되는 사람(Person) 데이터를 대상으로, **CRUD**뿐 아니라
**모니터링**까지 함께 제공하는 C++ 콘솔 PoC(Proof of Concept)입니다.

데이터 형태와 CRUD 구조는 [DataPersistence-joyjangs-0722](https://github.com/joyjangs/DataPersistence-joyjangs-0722)
프로젝트(Person을 JSON 파일로 저장/조회/수정/삭제하는 콘솔 프로그램)를 참고했고,
여기에 아래 세 가지 모니터링 기능을 추가로 얹었습니다.

## 모니터링 기능

1. **실시간 이벤트 로그 모니터링** (`EventLogMonitor`)
   등록/수정/삭제가 일어날 때마다 "언제, 무슨 일이 있었는지"를 콘솔에 즉시
   출력하고 `events.log` 파일에도 이어서 기록합니다. 프로그램을 종료한 뒤에도
   `events.log`를 열어보면 지금까지의 변경 이력을 감사 기록처럼 확인할 수
   있습니다.

2. **통계 / 이상치 모니터링** (`StatsMonitor`)
   데이터가 바뀔 때마다 전체 인원 수, 평균/최소/최대 나이를 계산하고,
   나이가 비정상적이거나(0세 이하, 120세 초과) 이름·이메일이 비어있거나
   이메일이 중복되는 경우를 이상치로 탐지해 보여줍니다.

   여기에 **나이대별 인원 분포**도 함께 제공합니다. 각 나이대(10대~90대)의
   인원수와 전체 대비 비율(%)을 계산하고, 비율을 막대 그래프(`■`)로
   시각화해 한눈에 비교할 수 있습니다.

   ```
   총 인원 : 7명
   나이대별 인원
     10대   :   1명 ( 14.3%) ■■■
     20대   :   2명 ( 28.6%) ■■■■■■
     30대   :   0명 (  0.0%)
     40대   :   1명 ( 14.3%) ■■■
     50대   :   1명 ( 14.3%) ■■■
     70대   :   1명 ( 14.3%) ■■■
     90대   :   1명 ( 14.3%) ■■■
   ```

3. **파일 변경 감지** (`FileWatcher`)
   `people.json` 파일을 프로그램을 거치지 않고 외부(메모장 등)에서 직접
   수정해도, 백그라운드 스레드가 1초 간격으로 파일 수정 시각을 확인해
   변경을 감지하고 자동으로 다시 불러옵니다. 이 사실 역시 이벤트 로그와
   통계에 곧바로 반영됩니다.

이 세 기능은 모두 `IRepositoryObserver` 인터페이스(옵저버 패턴)를 통해
`PersonRepository`에 "구독자"로 연결되어 있어서, CRUD 로직과 모니터링
로직이 서로 분리되어 있습니다.

## 프로젝트 구조

| 파일 | 역할 |
|---|---|
| `Json.h` / `Json.cpp` | 외부 의존성 없는 JSON 파서 및 직렬화기 |
| `Person.h` / `Person.cpp` | 데이터 레코드(id/name/age/email) 구조체 |
| `IRepositoryObserver.h` | CRUD 이벤트를 구독하기 위한 옵저버 인터페이스 |
| `PersonRepository.h` / `.cpp` | `people.json` 대상 CRUD 저장소 |
| `EventLogMonitor.h` / `.cpp` | 실시간 이벤트 로그 모니터링 |
| `StatsMonitor.h` / `.cpp` | 통계 / 이상치 / 나이대별 분포 모니터링 |
| `FileWatcher.h` / `.cpp` | 외부 파일 변경 감지 |
| `main.cpp` | 콘솔 메뉴 및 각 모듈 연결(wiring) |

## 빌드 및 실행

Windows + Visual Studio(v145 도구 집합, C++20) 환경을 기준으로 합니다.

```
MSBuild DataMonitor.vcxproj /p:Configuration=Debug /p:Platform=x64
```

빌드 후 생성된 실행 파일을 실행하면 콘솔 메뉴가 나타납니다.

```
============= DataMonitor =============
 1. 사람 등록 (Create)
 2. 전체 조회 (Read All)
 3. ID로 조회 (Read by Id)
 4. 수정 (Update)
 5. 삭제 (Delete)
 6. 통계 / 이상치 보기
 7. 파일 변경 감시 시작/중지 전환
 0. 종료
=========================================
```

- 실행 파일과 같은 폴더에 `people.json`(데이터), `events.log`(이벤트 로그)가
  생성됩니다.
- `people.json`을 프로그램 실행 중에 다른 편집기로 직접 수정해도 자동으로
  감지되어 다시 불러와집니다.
