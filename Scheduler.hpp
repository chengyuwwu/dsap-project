#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cmath>
#include "Task.hpp"

class Scheduler {
public:
    std::vector<Task> taskList;
    float dailyCapacity = 8.0f; // 確保這個變數存在，供 main.cpp 使用

    // --- 儲存檔案 (包含 CAPACITY) ---
    void saveToFile(std::string f) {
        std::ofstream out(f);
        if (!out) return;
        // 第一行儲存目前的工時設定
        out << "CAPACITY," << dailyCapacity << "\n"; 
        for (auto& t : taskList) {
            auto ts = std::chrono::time_point_cast<std::chrono::seconds>(t.deadlineTime).time_since_epoch().count();
            out << t.name << "," << ts << "," << t.estimatedTime << "," << t.remainingTime << "\n";
        }
    }
    
    // --- 讀取檔案 (支援 CAPACITY 辨識) ---
    void loadFromFile(std::string f) {
        std::ifstream in(f); 
        if (!in) return;
        taskList.clear(); 
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line); 
            std::string n, ts, es, rs;
            
            if (!std::getline(ss, n, ',')) continue;
            
            // 檢查是否為工時設定行
            if (n == "CAPACITY") {
                if (std::getline(ss, ts, ',')) {
                    dailyCapacity = std::stof(ts);
                }
                continue;
            }

            // 讀取一般任務資料
            if (!std::getline(ss, ts, ',')) continue;
            if (!std::getline(ss, es, ',')) continue;
            if (!std::getline(ss, rs, ',')) continue;
            
            try {
                auto tp = std::chrono::system_clock::time_point(std::chrono::seconds(std::stoll(ts)));
                taskList.push_back({n, tp, std::stof(es), std::stof(rs), 0.0f});
            } catch (...) {
                continue; // 忽略格式錯誤的列
            }
        }
        updateAndSort();
    }
    
    // --- 時間字串轉換 ---
    std::chrono::system_clock::time_point stringToTimePoint(const std::string& dateStr) {
        std::tm tm = {};
        std::stringstream ss(dateStr);
        int y, m, d, hh = 23, mm = 59; 
        char c;

        if (ss >> y >> c >> m >> c >> d) {
            if (ss >> c >> hh >> c >> mm) { }
        }

        tm.tm_year = y - 1900;
        tm.tm_mon = m - 1;
        tm.tm_mday = d;
        tm.tm_hour = hh;
        tm.tm_min = mm;
        tm.tm_sec = 0;
        tm.tm_isdst = -1;

        time_t tt = std::mktime(&tm);
        if (tt == -1) return std::chrono::system_clock::now();
        return std::chrono::system_clock::from_time_t(tt);
    }

    void addTask(std::string name, std::string dateStr, float est) {
        auto dl = stringToTimePoint(dateStr);
        taskList.push_back({name, dl, est, est, 0.0f});
        updateAndSort();
    }

    void updateAndSort() {
        for (auto& t : taskList) t.calculatePriority();
        // 內部的初步排序（根據優先級分數）
        std::sort(taskList.begin(), taskList.end(), [](const Task& a, const Task& b) {
            return a.priorityScore > b.priorityScore;
        });
    }

    void removeTaskAt(int index) {
        if (index >= 0 && index < (int)taskList.size()) {
            taskList.erase(taskList.begin() + index);
        }
    }
};

#endif