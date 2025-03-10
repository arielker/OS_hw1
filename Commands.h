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
	char* cmd_line;
	char* command [COMMAND_MAX_ARGS];
	int numOfArgs;
	bool is_external;
// TODO: Add your data members
 public:
  Command(const char* cmd_line);
  virtual ~Command();
  virtual void execute() = 0;
  char** getCommand(){
	  return this->command;
  }
  char* getCommandLine(){
	  return this->cmd_line;
  }
  int getNumOfArgs(){
	  return this->numOfArgs;
  }
  bool isExternal(){
	  return this->is_external;
  }
  //virtual void prepare();
  //virtual void cleanup();
  // TODO: Add your extra methods if needed
};

class timeout_member {
		int duration;
		time_t time_stamp;
		pid_t pid;
		string cmd_string;
	public:
		timeout_member(int d, time_t t, pid_t p, string s): 
		duration(d), time_stamp(t), pid(p), cmd_string(s){}
		
		~timeout_member() = default;
		
		int getDuration(){
			return this->duration;
		}
		
		time_t getTimeStamp(){
			return this->time_stamp;
		}
		
		pid_t getPid(){
			return this->pid;
		}
		
		void setDuration(int& i){
			this->duration = i;
		}
		
		void setTimeStamp(time_t& t){
			this->time_stamp = t;
		}
		
		void setPid(pid_t& p){
			this->pid = p;
		}
		string getCmdString(){
			return this->cmd_string;
		}
	};

class TimeoutCommand : public Command {
	string cmd_string;
public:
	TimeoutCommand(const char* cmd_line);
	virtual ~TimeoutCommand() override = default;
	void execute() override;
};

class BuiltInCommand : public Command {
 public:
  BuiltInCommand(const char* cmd_line): Command(cmd_line){}
  virtual ~BuiltInCommand() = default;
};

class ExternalCommand : public Command {
	const char* c_flag = "-c";
	const char* bin_bash = "/bin/bash";
	char* external_args[4];
 public:
  ExternalCommand(const char* cmd_line);
  virtual ~ExternalCommand() override; //TODO: implement
  void execute() override;
};

class PipeCommand : public Command {
	const char* out_to_in = "|";
	const char* err_to_in = "|&";
	string cmd_line_tmp;
	bool is_err_to_in;
	int pipe_sign_len;
	int pipe_index;
	string cmd1;
	string cmd2;
  // TODO: Add your data members
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() override = default;
  void execute() override;
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
	int n;
 public:
  GetCurrDirCommand(const char* cmd_line);
  virtual ~GetCurrDirCommand() override = default;
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
	pid_t pid;
	int n;
 public:
  ShowPidCommand(const char* cmd_line);
  virtual ~ShowPidCommand() override  = default;
  void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
	JobsList* j;
	public:
		QuitCommand(const char* cmd_line, JobsList* jobs);
		virtual ~QuitCommand() override = default;
		void execute() override;
};

class JobsList {
 public:
 //-----------
 //JobEntry
 //----------- 
  class JobEntry {
	  Command* cmd = nullptr;
	  char* job;
	  pid_t pid;
	  bool isStopped;
	  time_t time;
	  int numOfArgs;
	  pid_t grp_id;
	  int job_id;
	 public:
	  JobEntry(Command* c,char* j, pid_t p, bool iS, time_t t, int n, pid_t g, int jid):
	  pid(p), isStopped(iS), time(t), numOfArgs(n), grp_id(g), job_id(jid) {
			cmd = c;
			this->job = (char*)(malloc (strlen(j) + 1));
			memcpy(this->job, j,strlen(j) + 1);
	  }
	  ~JobEntry(){ 
		  free(this->job);
		  if(nullptr != this->cmd){
			delete this->cmd;
		  }
	  }
	  Command* getCmd(){
		  return this->cmd;
	  }
	  char* getJob(){
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
	  
	  pid_t getGroupID(){
		  return this->grp_id;
	  }
	  
	  void setGroupID(pid_t g){
		  this->grp_id = g;
	  }
	  
	  int getJobId(){
		  return this->job_id;
	  }
	  
	  void setJobId(int j){
		  this->job_id = j;
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
  void addJob(Command* cmd, pid_t pid, bool isStopped = false, pid_t g = 0); //done
  void printJobsList(); //done
  void killAllJobs(); //done
  void removeFinishedJobs(); //done
  JobEntry * getJobById(int jobId); //done
  void removeJobById(int jobId); //done
  JobEntry * getLastJob(int* lastJobId); //done
  JobEntry *getLastStoppedJob(int *jobId); //done
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
  virtual ~JobsCommand() override = default;
  void execute() override;
};

class KillCommand : public BuiltInCommand {
	JobsList* j;
 public:
  KillCommand(const char* cmd_line, JobsList* jobs);
  virtual ~KillCommand() override = default;
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 // TODO: Add your data members
	JobsList* j;
 public:
 int* waitForPid=nullptr;
  ForegroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~ForegroundCommand() override = default;
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 // TODO: Add your data members
	JobsList* j;
 public:
  BackgroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~BackgroundCommand() override = default;
  void execute() override;
};


class CopyCommand : public Command {
	char* cmd_without_bg_sign[COMMAND_MAX_ARGS];
	int n;
 public:
  CopyCommand(const char* cmd_line);
  virtual ~CopyCommand() override;
  void execute() override;
};

// TODO: add more classes if needed 
// maybe chprompt , timeout ?

class ChangePromptCommand : public BuiltInCommand {
	const char* smash = "smash";
	char prompt[80] = {0};
	int n;
	public:
		ChangePromptCommand(const char* cmd_line);
		virtual ~ChangePromptCommand() override = default;
		void execute() override;
};

class SmallShell {
 private:
  // TODO: Add your data members
  Command* current_command = nullptr;
  Command* special_current_command = nullptr;
  const pid_t smash_pid = getpid();
  pid_t current_fg_pid;
  pid_t current_fg_gid;
  JobsList* jobs;
  char prompt[80] = "smash";
  char *plastPwd = nullptr;
  bool is_forked = false;
  vector<timeout_member*> timeout_vector;
  SmallShell();
 public:
  int* waitForPid = nullptr;
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
  void setCurrentFgGid(pid_t g){
	  this->current_fg_gid = g;
  }
  pid_t getCurrentFgGid(){
	  return this->current_fg_gid;
  }
  void setSpecialCurrentCommand(Command *c){
	  this->special_current_command = c;
  }
  Command* getSpecialCurrentCommand(){
	  return this->special_current_command;
  }
  bool getIsForked(){
	  return this->is_forked;
  }
  void setIsForked(bool b){
	  this->is_forked = b;
  }
  vector<timeout_member*> getTimeoutVector(){
	  return this->timeout_vector;
  }
  void setTimeoutVector(vector<timeout_member*> x){
	 this->timeout_vector=x;
  }
  // TODO: add extra methods as needed
};

class RedirectionCommand : public Command {
 // TODO: Add your data members
	const char* over = ">";
	const char* append = ">>";
	string cmd_line_tmp;
	bool is_append;
	int red_sign_len;
	int red_index;
	string file_name;
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() override {
	  SmallShell& smash = SmallShell::getInstance();
	  if(smash.waitForPid != nullptr){
		  free(smash.waitForPid);
		  smash.waitForPid = nullptr;
	  }
  }
  void execute() override;
  //void prepare() override;
  //void cleanup() override;
};
#endif //SMASH_COMMAND_H_


