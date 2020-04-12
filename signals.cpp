#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

//My addition
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
//------
using namespace std;

void ctrlCHandler(int sig_num) {
  std::cout<<"smash: got ctrl-C"<<std::endl;
  int foreground_pid=getpid(); //TODO: change this?
  std::cout<<"smash: process "<<foreground_pid<<" was killed"<<std::endl;
  kill(foreground_pid,SIGKILL); 
}

void ctrlZHandler(int sig_num) {
	std::cout<<"smash: got ctrl-Z"<<std::endl;
	//TODO: add the foreground process to jobs list. 
	//If no process is running in the foreground,
	//then nothing will be added to jobs list.
	
	int foreground_pid=getpid();//TODO: change this?
	std::cout<<"smash: process "<<foreground_pid<<" was stopped"<<std::endl;
	kill(foreground_pid,SIGSTOP); 
}



void alarmHandler(int sig_num) {
  std::cout<<"smash got an alarm"<<std::endl;
  //TODO: search which command caused the alarm,
  //and set pid to the command's pid.
  int pid=-99;//TODO: change this! (-99 to real pid)
  kill(pid,SIGKILL);
  
}

