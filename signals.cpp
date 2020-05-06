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
	cout << "smash: got an alarm" << endl;
	SmallShell& s = SmallShell::getInstance();
	time_t t;
	time(&t);
	vector<timeout_member*> vec = s.getTimeoutVector();
	auto it = vec.begin();
	while(it != vec.end()){
		/*cout << "time: "<< t << endl;
		cout << "time stamp: " << (*it)->getTimeStamp() << endl;
		cout << "duration: " << (*it)->getDuration() << endl;*/
		if((*it)->getTimeStamp() + (*it)->getDuration() <= t){
			//cout << "smash: got an alarm" << endl;
			int wstatus = 0;
			int res = waitpid((*it)->getPid(), &wstatus, WNOHANG);
			if((res == (*it)->getPid() && (WIFEXITED(wstatus) || WIFSIGNALED(wstatus))) || res == -1){
				timeout_member* temp = *it;
				vec.erase(it);
				s.setTimeoutVector(vec);
				delete temp;
				break;
			}
			if(kill((*it)->getPid(),SIGKILL) != -1){
				cout << "smash: " << (*it)->getCmdString() << " timed out!" << endl;
			}
			timeout_member* temp = *it;
			vec.erase(it);
			s.setTimeoutVector(vec);
			delete temp;
			break;
		}
		it++;
	}
	//------Minimum duration for alarm calculation------
	int min_duration;
	
	if(vec.size()>1){
		auto iter = vec.begin();
		min_duration =(*iter)->getTimeStamp() + (*iter)->getDuration() - t;
		iter++;
		while(iter != vec.end()){
			if((*iter)->getTimeStamp() + (*iter)->getDuration()-t < min_duration){
				min_duration =(*iter)->getTimeStamp() + (*iter)->getDuration() - t;
			
			}
			iter++;
		}
		alarm(min_duration);
	} else if(vec.size()==1){
		auto iter = vec.begin();
		min_duration =(*iter)->getTimeStamp() + (*iter)->getDuration() - t;
		alarm(min_duration);
	}
	//------------
	
	s.getJobs()->removeFinishedJobs();
}












