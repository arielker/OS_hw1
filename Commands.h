#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <vector>
#include <cstdlib>
/* TODO: pay attention to this mind fuck "string"*/
#include <string>
#include <string.h>
#include <time.h>

using namespace std;

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define HISTORY_MAX_RECORDS (50)

class Command {
 protected:
	bool is_background;
	char* command [COMMAND_MAX_ARGS];
	int numOfArgs;
// TODO: Add your data members
 public:
  Command(const char* cmd_line);
  virtual ~Command();
  virtual void execute() = 0;
  char** getCommand(){
	  return this->command;
  }
  int getNumOfArgs(){
	  return this->numOfArgs;
  }
  //virtual void prepare();
  //virtual void cleanup();
  // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
 public:
  BuiltInCommand(const char* cmd_line);
  virtual ~BuiltInCommand();
};

class ExternalCommand : public Command {
	const char* c_flag = "-c";
	const char* bin_bash = "/bin/bash";
	char* external_args[4];
 public:
  ExternalCommand(const char* cmd_line);
  virtual ~ExternalCommand(); //TODO: implement
  void execute() override;
};

class PipeCommand : public Command {
	const char* out_to_in = "|";
	const char* err_to_in = "|&";
	//pid_t pid;
	int place_of_sign;
	char* cmd_without_bg_sign[COMMAND_MAX_ARGS];
	char* create_cmd_command(bool is_writes){
		string a = "";
		if(is_writes){
			a = string(this->command[0]);
			for (int i = 1; i < place_of_sign; i++) {
				a = a + " " + string(this->command[i]);
			}
		} else {
			a = string(this->command[place_of_sign + 1]);
			for (int i = place_of_sign + 2; i < numOfArgs; i++) {
				a = a + " " + string(this->command[i]);
			}
		}
		int n = a.length();
		char* ret = (char*)(malloc (n + 1));
		memset(ret, 0, n + 1);
		strcpy(ret, a.c_str());
		return ret;
	}
  // TODO: Add your data members
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command {
 // TODO: Add your data members
	const char* over = ">";
	const char* append = ">>";
	//pid_t pid;
	int place_of_sign;
	char* cmd_without_bg_sign[COMMAND_MAX_ARGS];
	char* create_cmd_command(){
		string a = "";
		for (int i = 0; i < place_of_sign; i++) {
			a = a + " " + string(this->command[i]);
		}
		int n = a.length();
		char* ret = (char*)(malloc (n + 1));
		memset(ret, 0, n + 1);
		strcpy(ret, a.c_str());
		return ret;
	}
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand(){
	  if(is_background){
		  for (int i = 0; i < COMMAND_MAX_ARGS; i++){
			  free(this->cmd_without_bg_sign[i]);
			  free(this->command[i]);
		  }
	  } else {
		  for (int i = 0; i < COMMAND_MAX_ARGS; i++){
			  free(this->command[i]);
		  }
	  }
  }
  void execute() override;
  //void prepare() override;
  //void cleanup() override;
};

class ChangeDirCommand : public BuiltInCommand {
// TODO: Add your data members public:
	char* lastPwd;
	public:
	  ChangeDirCommand(const char* cmd_line, char** plastPwd);
	  virtual ~ChangeDirCommand() override;
	  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line);
  virtual ~GetCurrDirCommand() = default;
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
	pid_t pid;
 public:
  ShowPidCommand(const char* cmd_line);
  virtual ~ShowPidCommand();
  void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
	JobsList* j;
	public:
		QuitCommand(const char* cmd_line, JobsList* jobs);
		virtual ~QuitCommand() = default;
		void execute() override;
};

class JobsList {
 public:
 //-----------
 //JobEntry
 //----------- 
  class JobEntry {
	  Command* cmd=nullptr;
	  char* job[COMMAND_MAX_ARGS];
	  pid_t pid;
	  bool isStopped;
	  time_t time;
	  int numOfArgs;
	 public:
	  JobEntry(Command* c,char** j, pid_t p, bool iS, time_t t, int n): pid(p), 
	  isStopped(iS), time(t), numOfArgs(n) {
		  cmd=c;
		  for (int i = 0; i < n; i++) {
			this->job[i] = (char*)(malloc (strlen(j[i]) + 1));
			memcpy(this->job[i], j[i], strlen(j[i]) + 1);
		  }
	  }
	  
	  ~JobEntry(){
		  for (int i = 0; i < numOfArgs; i++) {
			  free(this->job[i]);
		  }
	  }
	  
	  void printArgs(char* a[COMMAND_MAX_ARGS], int n){
		  for (int i = 0; i < n; i++) {
			  cout << " " << string(a[i]);
		  }
	  }
	  
	  void printArgsWithoutFirstSpace(char* a[COMMAND_MAX_ARGS], int n){
		  cout << string(a[0]);
		  for (int i = 1; i < n; i++) {
			  cout << " " << string(a[i]);
		  }
	  }
	  Command* getCmd(){
		  return this->cmd;
	  }
	  char** getJob(){
		  return this->job;
	  }
	  
	  pid_t getPid(){
		  return this->pid;
	  }
	  
	  bool getIsStopped(){
		  return this->isStopped;
	  }
	  
	  time_t getTime(){
		  return this->time;
	  }
	  
	  void setIsStopped(bool new_stopped){
		  this->isStopped = new_stopped;
	  }
	  
	  int getNumOfArgs(){
		  return this->numOfArgs;
	  }
	  
	  void setNumOfArgs(int n){
		  this->numOfArgs = n;
	  }
		//-----------
		//JobEntry
		//----------- 
  };
  private:
		vector<JobEntry*> jobs;
 public:
  JobsList();
  ~JobsList();
  void addJob(Command* cmd, pid_t pid, bool isStopped = false); //done
  void printJobsList(); //done
  void killAllJobs(); //done
  void removeFinishedJobs();
  JobEntry * getJobById(int jobId); //done
  void removeJobById(int jobId); //done
  JobEntry * getLastJob(int* lastJobId);
  JobEntry *getLastStoppedJob(int *jobId);
  vector<JobEntry*> getJobs(){
	  return this->jobs;
  }
  // TODO: Add extra methods or modify exisitng ones as needed
};

class JobsCommand : public BuiltInCommand {
 // TODO: Add your data members
	JobsList* j;
 public:
  JobsCommand(const char* cmd_line, JobsList* jobs);
  virtual ~JobsCommand() = default;
  void execute() override;
};

class KillCommand : public BuiltInCommand {
	JobsList* j;
 public:
  KillCommand(const char* cmd_line, JobsList* jobs);
  virtual ~KillCommand() = default;
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 // TODO: Add your data members
	JobsList* j;
 public:
  ForegroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~ForegroundCommand() = default;
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 // TODO: Add your data members
	JobsList* j;
 public:
  BackgroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~BackgroundCommand() {}
  void execute() override;
};


// TODO: should it really inhirit from BuiltInCommand ?
class CopyCommand : public BuiltInCommand {
 public:
  CopyCommand(const char* cmd_line);
  virtual ~CopyCommand() {}
  void execute() override;
};

// TODO: add more classes if needed 
// maybe chprompt , timeout ?

class ChangePromptCommand : public BuiltInCommand {
	const char* smash = "smash";
	char prompt[80] = {0};
	public:
		ChangePromptCommand(const char* cmd_line);
		virtual ~ChangePromptCommand() override;
		void execute() override;
};

class SmallShell {
 private:
  // TODO: Add your data members
  Command* current_command = nullptr;
  const pid_t smash_pid = getpid();
  pid_t current_fg_pid;
  JobsList* jobs;
  char prompt[80] = "smash";
  char *plastPwd = nullptr;
  SmallShell();
 public:
  Command *CreateCommand(const char* cmd_line);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  static SmallShell& getInstance() // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  ~SmallShell();
  void executeCommand(const char* cmd_line);
  void setPrompt(char* prompt);
  char* getPrompt();
  char* getlastPwd();
  void setPlastPwd(char* pwd_new);
  JobsList* getJobs();
  void setCurrentFgPid(pid_t p){
	  this->current_fg_pid = p;
  }
  const pid_t getSmashPid() {
	  return this->smash_pid;
  }
  pid_t getCurrentFgPid(){
	  return this->current_fg_pid;
  }
  void setCurrentCommand(Command* c){
	  this->current_command = c;
  }
  Command* getCurrentCommand(){
	  return this->current_command;
  }
  // TODO: add extra methods as needed
};

#endif //SMASH_COMMAND_H_
