#include "vm.h"
#include "robotFunctions.h"
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/time.h>

PiELo::Variable doNothingButTalkAboutIt() {
    std::cout << "doing nothing!!" << std::endl;
    
    return 0;
}

PiELo::Variable goForward() {
    robot.setRobotVel({1, 0, 0});
    return 0;
}

PiELo::Variable printRobotPos() {
    vec pos = robot.getRobotPos();
    std::cout << "Robot pos: x = " << pos.x << " y = " << pos.y << " z = " << pos.z << std::endl;
    return 0;
}

PiELo::Variable randomSleep() {
    int maxSleepUs = 3000000;
    int sleepTime = rand() % maxSleepUs;
    std::cout << "Sleeping for " << sleepTime / 1000.0 << "ms " << std::endl;
    usleep(sleepTime);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Please provide a filename\n");
        exit(-1);
    }
    // Seed the RNG. If PIELO_SEED is set, use it for a deterministic, reproducible
    // run (the RL verifier / sandbox sets this so random_sleep and any other rand()
    // use are repeatable). Otherwise fall back to the microsecond-precision wall clock,
    // exactly as before -- so an unset PIELO_SEED leaves existing behavior unchanged.
    const char* seedEnv = std::getenv("PIELO_SEED");
    if (seedEnv != nullptr) {
        srand(static_cast<unsigned>(std::strtoul(seedEnv, nullptr, 10)));
    } else {
        struct timeval time;
        gettimeofday(&time, NULL);
        srand(time.tv_sec * time.tv_usec);
    }

    PiELo::registerFunction("do_nothing", &doNothingButTalkAboutIt);
    PiELo::registerFunction("go_forward", &goForward);
    PiELo::registerFunction("print_robot_pos", &printRobotPos);
    PiELo::registerFunction("random_sleep", &randomSleep);
    PiELo::load(argv[1]);
    while(PiELo::step() == PiELo::VMState::READY);
    printf("Done!\n");
}