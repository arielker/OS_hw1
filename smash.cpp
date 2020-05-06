#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char* argv[]) {
    if(signal(SIGTSTP , ctrlZHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if(signal(SIGINT , ctrlCHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }

    //TODO: setup sig alarm handler
	struct sigaction a = {{0}};
	a.sa_handler = alarmHandler;
	a.sa_flags = SA_RESTART;
	sigaction(SIGALRM, &a, nullptr);
	
    SmallShell& smash = SmallShell::getInstance();
    while(true) {
		smash.setCurrentFgPid(getpid()); //SETS CURRENT FG PID TO SMASH PID EVERY CYCLE!
        std::cout << smash.getPrompt() << "> " ; 
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str());
    }
    return 0;
}
