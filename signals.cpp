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
	cout<<"smash: got ctrl-C"<<endl;
	pid_t p = smash.getCurrentFgPid();
	if(p != smash.getSmashPid()){
		if(smash.getCurrentFgGid() != 0){
			if(killpg(smash.getCurrentFgGid(), SIGKILL) == -1){
				perror("smash error: kill failed");
				return;
			} else {
				cout << "smash: process " << p << " was killed" << endl;
				smash.setCurrentFgPid(smash.getSmashPid());
				smash.setCurrentFgGid(0);
			}
		} else {
			if(kill(p, SIGKILL) == -1){
				perror("smash error: kill failed");
			} else {
				cout << "smash: process " << p << " was killed" << endl;
				smash.setCurrentFgPid(smash.getSmashPid());
				smash.setCurrentFgGid(0);
			}
		}
	}
}

void ctrlZHandler(int sig_num) {
	SmallShell& smash = SmallShell::getInstance();
	cout << "smash: got ctrl-Z" << endl;
	pid_t p = smash.getCurrentFgPid();
	if(p != smash.getSmashPid()){
		if(smash.getCurrentFgGid() != 0){
			if(killpg(smash.getCurrentFgGid(), SIGSTOP) == -1) {
				perror("smash error: kill failed");
				return;
			} else {
				cout << "smash: process " << p << " was stopped" << endl;
				for(auto j : smash.getJobs()->getJobs()){
					if(p == (*j).getPid()){
						(*j).setIsStopped(true);
						smash.setCurrentFgPid(smash.getSmashPid());
						smash.setCurrentFgGid(0);
						return;
					}	
				}
				smash.getJobs()->addJob(smash.getSpecialCurrentCommand(), p, true, smash.getCurrentFgGid());
				smash.setCurrentFgPid(smash.getSmashPid());
				smash.setCurrentFgGid(0);
			}
		} else {
			if(kill(p, SIGSTOP) == -1){
				perror("smash error: kill failed");
			} else {
				cout << "smash: process " << p << " was stopped" << endl;
				for(auto j : smash.getJobs()->getJobs()){
					if(p == (*j).getPid()){
						(*j).setIsStopped(true);
						smash.setCurrentFgPid(smash.getSmashPid());
						smash.setCurrentFgGid(0);
						return;
					}	
				}
				smash.getJobs()->addJob(smash.getCurrentCommand(), p, true, 0);
				smash.setCurrentFgPid(smash.getSmashPid());
				smash.setCurrentFgGid(0);
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
