#ifndef TASK_HPP
#define TASK_HPP

#include <string>
#include <chrono>

struct Task {
    std::string name;
    std::chrono::system_clock::time_point deadlineTime; // 統一使用此名稱
    float estimatedTime; 
    float remainingTime; 
    float priorityScore;

    void calculatePriority() {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(deadlineTime - now);
        
        float hoursLeft = duration.count() / 60.0f;
        if (hoursLeft <= 0.1f) hoursLeft = 0.1f; 

        // 邏輯：時間越急、工作量越大，分數越高
        priorityScore = (100.0f / hoursLeft) + (remainingTime * 2.0f);
    }
};

#endif
