#include "PersonRepository.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

PersonRepository::PersonRepository(std::string filePath)
    : filePath_(std::move(filePath))
{
}

void PersonRepository::Load()
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream in(filePath_);
    if (!in.is_open())
    {
        // 파일이 아직 없다면(최초 실행) 에러가 아니라 "빈 목록으로 시작"으로 처리합니다.
        people_.clear();
        nextId_ = 1;
        return;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    std::string content = buffer.str();

    if (content.empty())
    {
        people_.clear();
        nextId_ = 1;
        return;
    }

    JsonValue root = JsonValue::Parse(content);
    people_.clear();
    for (size_t i = 0; i < root.Size(); ++i)
    {
        people_.push_back(Person::FromJson(root[i]));
    }

    // 다음에 발급할 id는 "지금까지 저장된 id 중 가장 큰 값 + 1"로 정합니다.
    nextId_ = 1;
    for (const auto& person : people_)
    {
        nextId_ = std::max(nextId_, person.id + 1);
    }
}

void PersonRepository::AddObserver(IRepositoryObserver* observer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.push_back(observer);
}

void PersonRepository::SetAfterSaveHook(std::function<void()> hook)
{
    std::lock_guard<std::mutex> lock(mutex_);
    afterSaveHook_ = std::move(hook);
}

// 반드시 mutex_가 이미 잠긴 상태에서만 호출해야 합니다.
void PersonRepository::SaveInternal_NoLock()
{
    JsonValue root = JsonValue::MakeArray();
    for (const auto& person : people_)
    {
        root.PushBack(person.ToJson());
    }

    std::ofstream out(filePath_, std::ios::trunc);
    if (!out.is_open())
    {
        throw std::runtime_error("데이터 파일을 쓰기 위해 열 수 없습니다: " + filePath_);
    }
    out << root.Dump(/*indent=*/4);
    out.close();

    // FileWatcher에게 "방금 변경은 우리가 한 것"이라고 알려서 오탐(false alarm)을 막습니다.
    if (afterSaveHook_)
    {
        afterSaveHook_();
    }
}

Person PersonRepository::Create(const std::string& name, int age, const std::string& email)
{
    Person created;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        created.id = nextId_++;
        created.name = name;
        created.age = age;
        created.email = email;
        people_.push_back(created);
        SaveInternal_NoLock();
    }
    NotifyCreate(created);
    return created;
}

std::vector<Person> PersonRepository::ReadAll() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return people_; // 복사본을 반환하여, 호출자가 락 밖에서 안전하게 사용하도록 합니다.
}

std::optional<Person> PersonRepository::ReadById(int id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& person : people_)
    {
        if (person.id == id) return person;
    }
    return std::nullopt;
}

bool PersonRepository::Update(int id, const std::string& name, int age, const std::string& email)
{
    Person before;
    Person after;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(people_.begin(), people_.end(),
                                [id](const Person& p) { return p.id == id; });
        if (it == people_.end()) return false;

        before = *it;
        it->name = name;
        it->age = age;
        it->email = email;
        after = *it;

        SaveInternal_NoLock();
    }
    NotifyUpdate(before, after);
    return true;
}

bool PersonRepository::Delete(int id)
{
    Person deleted;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(people_.begin(), people_.end(),
                                [id](const Person& p) { return p.id == id; });
        if (it == people_.end()) return false;

        deleted = *it;
        people_.erase(it);
        SaveInternal_NoLock();
    }
    NotifyDelete(deleted);
    return true;
}

void PersonRepository::ReloadFromDisk()
{
    // 이 함수는 FileWatcher(감시 스레드)에서 호출됩니다. Load()를 재사용하되,
    // 로드가 끝난 뒤 옵저버들에게 "외부 변경으로 다시 불러왔다"고 알립니다.
    Load();
    std::vector<Person> snapshot = ReadAll();
    NotifyExternalReload(snapshot);
}

void PersonRepository::NotifyCreate(const Person& created)
{
    std::vector<Person> snapshot = ReadAll();
    for (auto* observer : observers_) observer->OnCreate(created, snapshot);
}

void PersonRepository::NotifyUpdate(const Person& before, const Person& after)
{
    std::vector<Person> snapshot = ReadAll();
    for (auto* observer : observers_) observer->OnUpdate(before, after, snapshot);
}

void PersonRepository::NotifyDelete(const Person& deleted)
{
    std::vector<Person> snapshot = ReadAll();
    for (auto* observer : observers_) observer->OnDelete(deleted, snapshot);
}

void PersonRepository::NotifyExternalReload(const std::vector<Person>& reloaded)
{
    for (auto* observer : observers_) observer->OnExternalReload(reloaded);
}
