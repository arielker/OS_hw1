#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

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
 public:
  ExternalCommand(const char* cmd_line);
  virtual ~ExternalCommand() {} //TODO: implement
  void execute() override;
};

class PipeCommand : public Command {
  // TODO: Add your data members
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command {
 // TODO: Add your data members
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
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
 public:
  ShowPidCommand(const char* cmd_line);
  virtual ~ShowPidCommand();
  void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
// TODO: Add your data members public:
  QuitCommand(const char* cmd_line, JobsList* jobs);
  virtual ~QuitCommand() {}
  void execute() override;
};

class CommandsHistory {
 protected:
  class CommandHistoryEntry {
	  // TODO: Add your data members
  };
 // TODO: Add your data members
 public:
  CommandsHistory();
  ~CommandsHistory() {}
  void addRecord(const char* cmd_line);
  void printHistory();
};

class HistoryCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
  HistoryCommand(const char* cmd_line, CommandsHistory* history);
  virtual ~HistoryCommand() {}
  void execute() override;
};

class JobsList {
 public:
 //-----------
 //JobEntry
 //----------- 
  class JobEntry {
	  char* job[COMMAND_MAX_ARGS];
	  pid_t pid;
	  bool isStopped;
	  time_t time;
	  int numOfArgs;
	 public:
	  JobEntry(char** j, pid_t p, bool iS, time_t t, int n): pid(p), 
	  isStopped(iS), time(t), numOfArgs(n) {
		  for (int i = 0; i < n; i++) {
			this->job[i] = (char*)(malloc (strlen(j[i]) + 1));
			memcpy(this->job[i], j[i], strlen(j[i]) + 1);
		  }
	  }
	  
	  ~JobEntry(){
		  for (int i = 0; i < COMMAND_MAX_ARGS; i++) {
			  free(this->job[i]);
		  }
	  }
	  
	  void printArgs(char* a[COMMAND_MAX_ARGS], int n){
		  for (int i = 0; i < n; i++) {
			  cout << " " << string(a[i]);
		  }
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
	  
	  int getNumOfArgs (){
		  return this->numOfArgs;
	  }
	  
	  void setNumOfArgs(int n){
		  this->numOfArgs = n;
	  }
  	  //----------- 
	  
  };
  private:
		vector<JobEntry*> jobs;

 public:
  JobsList();
  ~JobsList();
  void addJob(Command* cmd, bool isStopped = false);//done
  void printJobsList(); //done
  void killAllJobs();
  void removeFinishedJobs();
  JobEntry * getJobById(int jobId); //done
  void removeJobById(int jobId);
  JobEntry * getLastJob(int* lastJobId);
  JobEntry *getLastStoppedJob(int *jobId);
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
  virtual ~KillCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
  ForegroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 // TODO: Add your data members
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
  // TODO: add extra methods as needed
};

#endif //SMASH_COMMAND_H_