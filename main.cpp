#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <optional>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <regex>
#include <fstream>
#include "Scheduler.hpp"

// 定義 UI 狀態
enum class UIState { 
    LIST, EDIT_MENU, 
    ADDING_NAME, ADDING_DATE, ADDING_EST, 
    SETTING_CAPACITY, 
    SUB_EDIT_NAME, SUB_EDIT_DATE, SUB_EDIT_REM, SUB_WORK_DONE 
};
enum class ViewMode { ALL, TODAY };

// --- 輔助函式：日期與輸入檢查 ---
// 修正：允許輸入當天日期（只要時間點還沒過）
bool isValidDate(int y, int m, int d, int hr = 23, int min = 59) {
    if (y > 2100 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    
    std::tm t = {};
    t.tm_year = y - 1900; t.tm_mon = m - 1; t.tm_mday = d;
    t.tm_hour = hr; t.tm_min = min;
    auto input_tp = std::chrono::system_clock::from_time_t(mktime(&t));
    return input_tp > std::chrono::system_clock::now() - std::chrono::hours(24); // 允許今天
}

bool parseAndCheckDate(std::string& input, std::string& errorMsg) {
    std::regex dateRegex(R"((\d{4})/(\d{2})/(\d{2}))");
    std::smatch match;
    if (std::regex_match(input, match, dateRegex)) {
        int y = std::stoi(match[1]), m = std::stoi(match[2]), d = std::stoi(match[3]);
        if (isValidDate(y, m, d)) {
            input += "/23:59";
            return true;
        }
        errorMsg = "Date must be valid & not in the past";
    } else {
        errorMsg = "Format: YYYY/MM/DD";
    }
    return false;
}

std::string getTodayStr() {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm ltm; localtime_s(&ltm, &now);
    std::ostringstream oss; oss << std::put_time(&ltm, "%Y/%m/%d");
    return oss.str();
}

std::string taskDateToStr(std::chrono::system_clock::time_point tp) {
    auto t_c = std::chrono::system_clock::to_time_t(tp);
    std::tm ltm; 
    localtime_s(&ltm, &t_c);
    std::ostringstream oss;
    oss << std::put_time(&ltm, "%Y/%m/%d");
    return oss.str();
}

// --- 排序邏輯優化：確保過期任務在最底，今日明日置頂 ---
void sortTasksByLogic(std::vector<Task>& tasks) {
    auto now = std::chrono::system_clock::now();
    std::string todayStr = getTodayStr();
    auto tomorrow_tp = now + std::chrono::hours(24);
    std::string tomorrowStr = taskDateToStr(tomorrow_tp);

    std::sort(tasks.begin(), tasks.end(), [&](const Task& a, const Task& b) {
        auto getRank = [&](const Task& t) {
            std::string dStr = taskDateToStr(t.deadlineTime);
            if (dStr == todayStr) return 0;       // 今日：最高優先
            if (dStr == tomorrowStr) return 1;    // 明日：第二
            if (t.deadlineTime < now && dStr != todayStr) return 3; // 過期：最低
            return 2; // 其他
        };
        
        int rankA = getRank(a), rankB = getRank(b);
        if (rankA != rankB) return rankA < rankB;
        return a.deadlineTime < b.deadlineTime; 
    });
}

int main() {
    Scheduler myScheduler;
    myScheduler.loadFromFile("tasks.csv");
    // 初始化時立即排序一次，確保顏色正確
    sortTasksByLogic(myScheduler.taskList); 
    
    sf::RenderWindow window(sf::VideoMode({800, 600}), "PrismFlow Pro - Smart Scheduler");
    window.setFramerateLimit(60);
    sf::Font font; if (!font.openFromFile("font.otf")) return -1;

    UIState currentState = UIState::LIST;
    ViewMode currentView = ViewMode::TODAY;
    
    // 修正：從 Scheduler 獲取 dailyCapacity，確保存檔同步
    // (注意：請確保你的 Scheduler.hpp 中有 dailyCapacity 成員)
    std::string inputBuffer = "", tempName = "", tempDate = "", errorMessage = "";
    int selectedTaskIdx = -1;

    while (window.isOpen()) {
        // 每一幀檢查並排序，確保跨日時顏色自動變更
        sortTasksByLogic(myScheduler.taskList);

        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) { 
                myScheduler.saveToFile("tasks.csv"); 
                window.close(); 
            }

            if (currentState != UIState::LIST && currentState != UIState::EDIT_MENU) {
                if (const auto* textEvent = event->getIf<sf::Event::TextEntered>()) {
                    if (textEvent->unicode == 13) { // ENTER
                        try {
                            if (currentState == UIState::ADDING_NAME) { tempName = inputBuffer; currentState = UIState::ADDING_DATE; }
                            else if (currentState == UIState::SUB_EDIT_NAME) { myScheduler.taskList[selectedTaskIdx].name = inputBuffer; currentState = UIState::LIST; }
                            else if (currentState == UIState::ADDING_DATE) {
                                if (parseAndCheckDate(inputBuffer, errorMessage)) { tempDate = inputBuffer; currentState = UIState::ADDING_EST; }
                            }
                            else if (currentState == UIState::SUB_EDIT_DATE) {
                                if (parseAndCheckDate(inputBuffer, errorMessage)) { 
                                    myScheduler.taskList[selectedTaskIdx].deadlineTime = myScheduler.stringToTimePoint(inputBuffer); 
                                    currentState = UIState::LIST; 
                                }
                            }
                            else if (currentState == UIState::ADDING_EST) { myScheduler.addTask(tempName, tempDate, std::stof(inputBuffer)); currentState = UIState::LIST; }
                            else if (currentState == UIState::SUB_EDIT_REM) { myScheduler.taskList[selectedTaskIdx].remainingTime = std::stof(inputBuffer); currentState = UIState::LIST; }
                            else if (currentState == UIState::SUB_WORK_DONE) {
                                myScheduler.taskList[selectedTaskIdx].remainingTime -= std::stof(inputBuffer);
                                if (myScheduler.taskList[selectedTaskIdx].remainingTime <= 0.1f) myScheduler.removeTaskAt(selectedTaskIdx);
                                currentState = UIState::LIST;
                            }
                            else if (currentState == UIState::SETTING_CAPACITY) {
                                float val = std::stof(inputBuffer);
                                if (val >= 0 && val <= 24) { 
                                    myScheduler.dailyCapacity = val; // 修改 Scheduler 內的變數
                                    myScheduler.saveToFile("tasks.csv"); // 立即存檔
                                    currentState = UIState::LIST; 
                                }
                                else { errorMessage = "Must be 0-24h"; inputBuffer = ""; }
                            }
                        } catch (...) { errorMessage = "Invalid Input!"; }
                        
                        if (currentState == UIState::LIST) { 
                            inputBuffer = ""; errorMessage = ""; 
                            myScheduler.saveToFile("tasks.csv"); // 任何修改後自動存檔
                        }
                        else if (errorMessage == "") inputBuffer = ""; 
                    } 
                    else if (textEvent->unicode == 8) { if (!inputBuffer.empty()) inputBuffer.pop_back(); }
                    else if (textEvent->unicode < 128) { inputBuffer += static_cast<char>(textEvent->unicode); }
                }
            }

            if (const auto* mouseBtn = event->getIf<sf::Event::MouseButtonPressed>()) {
                sf::Vector2f mPos = window.mapPixelToCoords({mouseBtn->position.x, mouseBtn->position.y});

                if (currentState == UIState::LIST) {
                    if (sf::FloatRect({520.f, 15.f}, {250.f, 40.f}).contains(mPos)) { currentState = UIState::SETTING_CAPACITY; inputBuffer = ""; }
                    if (sf::FloatRect({50.f, 70.f}, {150.f, 40.f}).contains(mPos)) currentView = ViewMode::TODAY;
                    if (sf::FloatRect({210.f, 70.f}, {150.f, 40.f}).contains(mPos)) currentView = ViewMode::ALL;
                    if (sf::FloatRect({700.f, 500.f}, {60.f, 60.f}).contains(mPos)) { currentState = UIState::ADDING_NAME; inputBuffer = ""; }

                    float ty = 130.f;
                    for (size_t i = 0; i < myScheduler.taskList.size(); ++i) {
                        Task& t = myScheduler.taskList[i];
                        std::string dStr = taskDateToStr(t.deadlineTime);
                        bool isToday = (dStr == getTodayStr());
                        bool isTomorrow = (dStr == taskDateToStr(std::chrono::system_clock::now() + std::chrono::hours(24)));
                        
                        if (currentView == ViewMode::TODAY && !isToday && !isTomorrow) continue;
                        if (sf::FloatRect({100.f, ty}, {600.f, 60.f}).contains(mPos)) { selectedTaskIdx = (int)i; currentState = UIState::EDIT_MENU; break; }
                        ty += 80.f;
                    }
                } 
                else if (currentState == UIState::EDIT_MENU) {
                    if (sf::FloatRect({250.f, 150.f}, {300.f, 40.f}).contains(mPos)) { currentState = UIState::SUB_WORK_DONE; inputBuffer = ""; }
                    else if (sf::FloatRect({250.f, 210.f}, {300.f, 40.f}).contains(mPos)) { currentState = UIState::SUB_EDIT_NAME; inputBuffer = ""; }
                    else if (sf::FloatRect({250.f, 270.f}, {300.f, 40.f}).contains(mPos)) { currentState = UIState::SUB_EDIT_DATE; inputBuffer = ""; }
                    else if (sf::FloatRect({250.f, 330.f}, {300.f, 40.f}).contains(mPos)) { currentState = UIState::SUB_EDIT_REM; inputBuffer = ""; }
                    else if (sf::FloatRect({250.f, 390.f}, {300.f, 40.f}).contains(mPos)) { myScheduler.removeTaskAt(selectedTaskIdx); myScheduler.saveToFile("tasks.csv"); currentState = UIState::LIST; }
                    else if (sf::FloatRect({250.f, 450.f}, {300.f, 40.f}).contains(mPos)) { currentState = UIState::LIST; }
                }
                else {
                    if (sf::FloatRect({600.f, 450.f}, {120.f, 45.f}).contains(mPos)) { currentState = UIState::LIST; inputBuffer = ""; errorMessage = ""; }
                }
            }
        }

        // --- 渲染 ---
        window.clear(sf::Color(31, 47, 56));
        
        sf::Text header(font, "Today: " + getTodayStr(), 20); header.setPosition({50.f, 20.f}); window.draw(header);
        sf::Text workH(font, "Work Hours: " + std::to_string((int)myScheduler.dailyCapacity) + "h", 18); workH.setPosition({550.f, 20.f}); window.draw(workH);

        auto now_tp = std::chrono::system_clock::now();
        std::string todayStr = getTodayStr();
        std::string tomorrowStr = taskDateToStr(now_tp + std::chrono::hours(24));

        float y = 130.f;
        float totalTodayNeeded = 0;

        for (auto& t : myScheduler.taskList) {
            std::string taskDateStr = taskDateToStr(t.deadlineTime);
            bool isToday = (taskDateStr == todayStr);
            bool isTomorrow = (taskDateStr == tomorrowStr);
            bool isOverdue = (t.deadlineTime < now_tp && !isToday);

            if (isToday) totalTodayNeeded += t.remainingTime;
            if (currentView == ViewMode::TODAY && !isToday && !isTomorrow) continue;

            sf::Color boxColor = sf::Color(159, 182, 195);
            if (isOverdue) boxColor = sf::Color(100, 100, 100);
            else if (isToday) boxColor = sf::Color(180, 50, 50);
            else if (isTomorrow) boxColor = sf::Color(220, 130, 160);

            sf::RectangleShape box({600.f, 60.f}); box.setPosition({100.f, y}); box.setFillColor(boxColor); window.draw(box);

            std::string infoStr = "";
            if (isOverdue) infoStr = t.name;
            else {
                infoStr = "Task Name: " + t.name + " | Suggest Work Time: ";
                if (isToday) infoStr += std::to_string((int)std::ceil(t.remainingTime)) + "h | DUE TODAY!";
                else if (isTomorrow) infoStr += std::to_string((int)std::ceil(t.remainingTime / 2.0f)) + "h | DUE TOMORROW!";
                else {
                    auto diff = std::chrono::duration_cast<std::chrono::hours>(t.deadlineTime - now_tp).count() / 24;
                    infoStr = t.name + " | Deadline: " + std::to_string(diff) + " days left";
                }
            }

            sf::Text info(font, infoStr, 15); info.setPosition({120.f, y + 15.f}); window.draw(info);
            sf::RectangleShape bar({600.f * (1.0f - (t.remainingTime / t.estimatedTime)), 6.f}); bar.setPosition({100.f, y + 54.f}); bar.setFillColor(sf::Color(40, 150, 40)); window.draw(bar);

            y += 80.f;
        }

        if (totalTodayNeeded > myScheduler.dailyCapacity) {
            sf::Text warn(font, "! Overload: Need " + std::to_string((int)totalTodayNeeded) + "h.", 14);
            warn.setFillColor(sf::Color::Yellow); warn.setPosition({300.f, 45.f}); window.draw(warn);
        }

        sf::CircleShape addBtn(30.f); addBtn.setPosition({700.f, 500.f}); addBtn.setFillColor(sf::Color(70, 130, 180)); window.draw(addBtn);
        sf::Text plus(font, "+", 40); plus.setPosition({718.f, 502.f}); window.draw(plus);

        if (currentState != UIState::LIST) {
            sf::RectangleShape overlay({800.f, 600.f}); overlay.setFillColor(sf::Color(0,0,0,220)); window.draw(overlay);
            if (currentState == UIState::EDIT_MENU) {
                std::string labels[] = {"Work Done Today", "Edit Name", "Edit Deadline", "Edit Total Time", "Delete", "Back"};
                for(int i=0; i<6; ++i) {
                    sf::RectangleShape b({300.f, 40.f}); b.setPosition({250.f, 150.f + i*60.f}); b.setFillColor(sf::Color(60,80,100)); window.draw(b);
                    sf::Text bt(font, labels[i], 18); bt.setPosition({270.f, 160.f + i*60.f}); window.draw(bt);
                }
            } else {
                sf::Text dialog(font, "", 24); 
                std::string p = "Input:";
                if(currentState == UIState::ADDING_NAME) p = "1.Task Name:";
                else if(currentState == UIState::ADDING_DATE) p = "2.Deadline (YYYY/MM/DD):";
                else if(currentState == UIState::ADDING_EST) p = "3.Estimated Hours:";
                else if(currentState == UIState::SETTING_CAPACITY) p = "New Work Hour (0-24):";
                else if(currentState == UIState::SUB_EDIT_NAME) p = "Edit Name:";
                else if(currentState == UIState::SUB_EDIT_DATE) p = "Edit Deadline (YYYY/MM/DD):";
                else if(currentState == UIState::SUB_EDIT_REM) p = "Edit Remaining Hours:";
                else if(currentState == UIState::SUB_WORK_DONE) p = "Hours Completed Today:";
                
                dialog.setString(p + "\n> " + inputBuffer + "_"); dialog.setPosition({150.f, 250.f}); window.draw(dialog);
                if (!errorMessage.empty()) { sf::Text e(font, errorMessage, 16); e.setFillColor(sf::Color::Red); e.setPosition({150.f, 350.f}); window.draw(e); }
                sf::RectangleShape cb({120.f, 45.f}); cb.setPosition({600.f, 450.f}); cb.setFillColor(sf::Color(100,100,100)); window.draw(cb);
                sf::Text ct(font, "Cancel", 18); ct.setPosition({630.f, 460.f}); window.draw(ct);
            }
        }
        window.display();
    }
    return 0;
}