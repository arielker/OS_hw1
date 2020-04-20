#include <signal.h>
#include "signals.h"
#include "Commands.h"

//My addition
#include <sstream>
#include <iomanip>
//------
using namespace std;

/**
 * find Pipe sign
 **/
 static int findPipeCommand2(char** a, int n) {
	 for (int i = 1; i < n; i++) {
		 if (strcmp(a[i], "|") == 0 || strcmp(a[i], "|&") == 0){
			 return i;
		 }
	 }
	 return -1;
 }

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
	//cout<<"CURRENT:"<<p<<" ACTUAL SMASH:"<<smash.getSmashPid()<<endl;
	if(p != smash.getSmashPid()){
		//Command* cmd = smash.getCurrentCommand();
		if(smash.getPipe2Pid() != 0 && smash.getPipe1Pid() != 0){
			cout << smash.getPipe2Pid() << endl;
			cout << smash.getPipe1Pid() << endl;
			if(kill(p, SIGSTOP) == -1){
				perror("smash error: kill failed");
				return;
			}
			if(kill(smash.getPipe2Pid(), SIGSTOP) == -1){
				perror("smash error: kill failed");
				return;
			}
			if(kill(smash.getPipe1Pid(), SIGSTOP) == -1){
				perror("smash error: kill failed");
				return;
			}
			cout << "smash: process " << p << " was killed" << endl;
			//cout << "smash: process " << smash.getPipe2Pid() << " was killed" << endl;
			smash.getJobs()->addJob(smash.getCurrentCommand(), p, true);
			smash.setCurrentFgPid(smash.getSmashPid());
		} else {
			smash.setPipe2Pid(0);
			smash.setPipe1Pid(0);
			if(kill(p, SIGSTOP) == -1){
				perror("smash error: kill failed");
			} else {
				cout << "smash: process " << p << " was stopped" << endl;
				smash.getJobs()->addJob(smash.getCurrentCommand(), p, true);
				smash.setCurrentFgPid(smash.getSmashPid());
			}
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
