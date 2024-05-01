/**
 * @brief timer的测试
 */

#include <ctime>
#include <iostream>
#include "timer.h"

void print()
{
    std::cout << "task exec, current time: " << time(nullptr) << std::endl;
}

int main()
{
    TimerManager::ptr timerManager = std::make_shared<TimerManager>();

    Timer::ptr task = std::make_shared<Timer>(10, print, true);

    timerManager->AddTimer(task);

    sleep(35);
    timerManager->DelTimer(task);

    return 0;
}
