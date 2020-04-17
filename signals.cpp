#include <signal.h>
#include "signals.h"
#include "Commands.h"

//My addition
#include <sstream>
#include <iomanip>
//------
using namespace std;

void ctrlCHandler(int sig_num) {
	SmallShell& smash = SmallShell::getInstance();
	std::cout<<"smash: got ctrl-C"<<std::endl;
	pid_t p = smash.getCurrentFgPid();
	if(p != smash.getSmashPid()){
		if(kill(p, SIGKILL) == -1){
			perror("smash error: kill failed");
		} else {
			cout << "smash: process " << p << " was killed" << endl;
			smash.setCurrentFgPid(smash.getSmashPid());
		}
	}
}

void ctrlZHandler(int sig_num) {
	SmallShell& smash = SmallShell::getInstance();
	std::cout<<"smash: got ctrl-Z"<<std::endl;
	pid_t p = smash.getCurrentFgPid();
	if(p != smash.getSmashPid()){
		if(kill(p, SIGSTOP) == -1){
			perror("smash error: kill failed");
		} else {
			cout << "smash: process " << p << " was stopped" << endl;
			smash.getJobs()->addJob(smash.getCurrentCommand(), p, true);
			smash.setCurrentFgPid(smash.getSmashPid());
		}
	}
}

void alarmHandler(int sig_num) {
  std::cout<<"smash got an alarm"<<std::endl;
  //TODO: search which command caused the alarm,
  //and set pid to the command's pid.
  int pid=-99;//TODO: change this! (-99 to real pid)
  kill(pid,SIGKILL);
  //cout<<"smash: "<<[command-line]<<" timed out!"<<endl;
}
