#include <sstream>
#include <iomanip>
#include "Commands.h"
#include <signal.h>

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cerr << PRETTY_FUNCTION << " --> " << endl;

#define FUNC_EXIT()  \
  cerr << PRETTY_FUNCTION << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

#define DEBUG_PRINT cerr << "DEBUG: "

#define EXEC(path, arg) \
  execvp((path), (arg));

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

 static int findRedirectionCommand(char** a, int n);
 static int findPipeCommand(char** a, int n);
 static void destroyTemp (char** a, int n);

//--------------------------------
//Command
//--------------------------------

Command::Command(const char* cmd_line) : is_background(false) {
	this->numOfArgs = _parseCommandLine(cmd_line, this->command);
}

Command::~Command(){
	for (int i = 0; i < numOfArgs; i++) {
		free(this->command[i]);
	}
}

//--------------------------------
//Built In Commands
//--------------------------------

BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line){}

BuiltInCommand::~BuiltInCommand(){
	for (int i = 0; i < COMMAND_MAX_ARGS; i++) {
		free(this->command[i]);
	}
}

//--------------------------------
// External Command
//--------------------------------

ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line){
	int binbashsize = strlen(this->bin_bash);
	this->external_args[0] = (char *)(malloc(binbashsize + 1));
	memset(external_args[0], 0, binbashsize + 1);
	memcpy(external_args[0], this->bin_bash, binbashsize + 1);
	
	
	int c_flag_size = strlen(this->c_flag);
	this->external_args[1] = (char *)(malloc(c_flag_size + 1));
	memset(external_args[1], 0, c_flag_size + 1);
	memcpy(external_args[1], this->c_flag, c_flag_size + 1);
	
	this->is_background = _isBackgroundComamnd(cmd_line); //check bg command...
	char* temp_cmd = const_cast<char*>(cmd_line);
	if(this->is_background){
		_removeBackgroundSign(temp_cmd);
	}
	
	int cmd_size = strlen(temp_cmd);
	this->external_args[2] = (char *)(malloc(cmd_size + 1));
	memset(external_args[2], 0, cmd_size + 1);
	memcpy(external_args[2], temp_cmd, cmd_size + 1);
	external_args[3] = NULL;
}

ExternalCommand::~ExternalCommand(){
	for (int i = 0; i < this->numOfArgs; i++) {
		free(this->command[i]);
	}
	free(external_args[0]);
	free(external_args[1]);
	free(external_args[2]);
}

void ExternalCommand::execute() {
	SmallShell& smash = SmallShell::getInstance();
	if(!(this->is_background)) {
		pid_t pid = fork();
		if (pid == 0) {
			//setpgrp();
			int result = execv(this->bin_bash, this->external_args);
			if(result == -1) {
				perror("smash error: execv failed");
				smash.setCurrentFgPid(smash.getSmashPid());
			}
		} else if (pid > 0) {
			smash.setCurrentFgPid(pid);
			waitpid(pid, nullptr, WUNTRACED);
			smash.setCurrentFgPid(smash.getSmashPid());
		} else{
			perror("smash error: fork failed");
			smash.setCurrentFgPid(smash.getSmashPid());
		}
	} else {
		pid_t pid = fork();
		smash.getJobs()->addJob(this,pid);
		if (pid == 0) {
			//setpgrp();
			int result = execv(this->bin_bash, this->external_args);
			if(result == -1){
				perror("smash error: execv failed");
			}
		} else if (pid < 0){
			smash.setCurrentFgPid(getpid());
			perror("smash error: fork failed");
		}
	}
}

//--------------------------------
//Redirection Command
//--------------------------------

RedirectionCommand::RedirectionCommand(const char* cmd_line): 
Command(cmd_line){
	this->is_background = _isBackgroundComamnd(cmd_line);
	if(this->is_background){
		char* temp = const_cast<char*>(cmd_line);
		_removeBackgroundSign(temp);
		_parseCommandLine(temp, this->cmd_without_bg_sign);
	}
	this->place_of_sign = findRedirectionCommand(this->command, this->numOfArgs);
}

void RedirectionCommand::execute(){
	SmallShell& smash = SmallShell::getInstance();
	if(this->is_background){ //in background
		char* cmd_line_until_sign = this->create_cmd_command();
		Command* cmd = smash.CreateCommand(cmd_line_until_sign);
		free(cmd_line_until_sign);
		pid_t pid = fork();
		if(pid == 0){
			setpgrp();
			if(strcmp(this->command[place_of_sign], append) == 0) { //append
				pid_t pid1 = fork();
				if(pid1 == 0){
					close(1);
					int open_res = open(cmd_without_bg_sign[place_of_sign + 1], O_WRONLY | O_APPEND | O_CREAT, 0666);
					if (open_res == -1) {
						perror("smash error: open failed");
						delete cmd;
						return;
					}
					//dup2(open_res, 1);
					cmd->execute();
					if(-1 == close(open_res)){
						perror("smash error: close failed");
						delete cmd;
						return;
					}
					delete cmd;
					kill(getpid(),SIGKILL);
				} else if (pid1 > 0) {
					waitpid(pid1, nullptr, WUNTRACED);
				} else {
					perror("smash error: fork failed");
					return;
				}
				kill(getpid(),SIGKILL);
			} else { //override
				pid_t pid1 = fork();
				if(pid1 == 0){
					close(1);
					int open_res = open(cmd_without_bg_sign[place_of_sign + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
					if (open_res == -1){
						perror("smash error: open failed");
						return;
					}
					cmd->execute();
					if(-1 == close(open_res)){
						perror("smash error: close failed");
						return;
					}
					delete cmd;
					kill(getpid(),SIGKILL);
				} else if (pid1 > 0) {
					waitpid(pid1, nullptr, WUNTRACED);
				} else {
					perror("smash error: fork failed");
					return;
				}
				kill(getpid(),SIGKILL);
			}
		} else if (pid == -1){
			perror("smash error: fork failed");
			return;
		} else {
			cout << pid << endl;
			cout << getpgrp() << endl;
			smash.getJobs()->addJob(this, pid, false, getpgrp());
		}
	} else { // not in background
		if(strcmp(this->command[place_of_sign], append) == 0) { //append
			char* cmd_line_until_sign = this->create_cmd_command();
			Command* cmd = smash.CreateCommand(cmd_line_until_sign);
			free(cmd_line_until_sign);
			pid_t pid = fork();
			if(pid == 0){
				setpgrp();
				close(1);
				int open_res = open(this->command[place_of_sign + 1], O_WRONLY | O_APPEND | O_CREAT, 0666);
				if (open_res == -1){
					perror("smash error: open failed");
					return;
				}
				cmd->execute();
				if(-1 == close(open_res)){
					perror("smash error: close failed");
					return;
				}
				delete cmd;
				kill(getpid(),SIGKILL);
			} else if (pid > 0) {
				smash.setCurrentFgPid(pid);
				smash.setCurrentFgGid(pid);
				waitpid(pid, nullptr, WUNTRACED);
				smash.setCurrentFgPid(getpid());
				smash.setCurrentFgGid(0);
			} else {
				smash.setCurrentFgPid(getpid());
				smash.setCurrentFgGid(0);
				perror("smash error: fork failed");
				delete cmd;
				return;
			}
		} else { //override
			char* cmd_line_until_sign = this->create_cmd_command();
			Command* cmd = smash.CreateCommand(cmd_line_until_sign);
			free(cmd_line_until_sign);
			pid_t pid = fork();
			if(pid == 0){
				setpgrp();
				close(1);
				int open_res = open(this->command[place_of_sign + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
				if (open_res == -1){
					perror("smash error: open failed");
					return;
				}
				cmd->execute();
				if(-1 == close(open_res)){
					perror("smash error: close failed");
					return;
				}
				delete cmd;
				kill(getpid(),SIGKILL);
			} else if (pid > 0) {
				smash.setCurrentFgPid(pid);
				smash.setCurrentFgGid(pid);
				waitpid(pid, nullptr, WUNTRACED);
				smash.setCurrentFgPid(getpid());
				smash.setCurrentFgGid(0);
			} else {
				smash.setCurrentFgGid(0);
				smash.setCurrentFgPid(getpid());
				delete cmd;
				perror("smash error: fork failed");
				return;
			}
		}
	}
}

//--------------------------------
//Copy Command (cp)
//--------------------------------

CopyCommand::CopyCommand(const char* cmd_line) : Command(cmd_line), n(0){
	this->is_background=_isBackgroundComamnd(cmd_line);
	if(this->is_background){
		char* temp = const_cast<char*>(cmd_line);
		_removeBackgroundSign(temp);
		this->n = _parseCommandLine(temp,this->cmd_without_bg_sign);
	}
}

CopyCommand::~CopyCommand(){
	if(this->is_background){
		for (int i = 0; i <this->n; i++) {
			free(cmd_without_bg_sign[i]);
		}
	}
	for (int i = 0; i <this->numOfArgs; i++){
			free(command[i]);
	}
}

void CopyCommand::execute() {
	SmallShell& smash = SmallShell::getInstance();
	if(this->is_background){//background
		pid_t pid = fork();
		if(pid == 0){
			char* sourceAddress = this->cmd_without_bg_sign[1];
			char* destinationAddress = this->cmd_without_bg_sign[2];
			char buff[1024];
			ssize_t count;
			int file[2];
			file[0]= open(sourceAddress,O_RDONLY);
			if(file[0] == -1){
				perror("smash error: open failed");
				kill(getpid(), SIGKILL);
				return;
			}
			file[1] = open(destinationAddress, O_WRONLY | O_CREAT | O_TRUNC,0666);
			if(file[1] == -1){
				close(file[0]);
				perror("smash error: open failed");
				kill(getpid(), SIGKILL);
				return;
			}
			count = read(file[0], buff, sizeof(buff));
			while (count != 0){
				if(count == -1){
					perror("smash error: read failed");
					close(file[0]);
					close(file[1]);
					kill(getpid(), SIGKILL);
					return;
				}
				if(write(file[1], buff, count) == -1){
					close(file[0]);
					close(file[1]);
					perror("smash error: write failed");
					kill(getpid(), SIGKILL);
					return;
				}
				count = read(file[0], buff, sizeof(buff));
			}
			close(file[0]);
			close(file[1]);
			kill(getpid(),SIGKILL);
		} else if(pid>0){
			smash.getJobs()->addJob(this,pid);
		}else{
			perror("smash error: fork failed");
		}
	} else {
		pid_t pid = fork();
		if(pid == 0){
			char* sourceAddress=this->command[1];
			char* destinationAddress=this->command[2];
			char buff[1024];
			ssize_t count;
			int file[2];
			file[0] = open(sourceAddress,O_RDONLY);
			if(file[0] == -1){
				perror("smash error: open failed");
				kill(getpid(), SIGKILL);
				return;
			}
			file[1] = open(destinationAddress, O_WRONLY | O_CREAT | O_TRUNC,0666);
			if(file[1] == -1){
				close(file[0]);
				perror("smash error: open failed");
				kill(getpid(), SIGKILL);
				return;
			}
			count = read(file[0], buff, sizeof(buff));
			while (count != 0){
				if(count == -1) {
					perror("smash error: read failed");
					close(file[0]);
					close(file[1]);
					kill(getpid(), SIGKILL);
					return;
				}
				if(write(file[1], buff, count) == -1) {
					close(file[0]);
					close(file[1]);
					perror("smash error: write failed");
					kill(getpid(), SIGKILL);
					return;
				}
				count = read(file[0], buff, sizeof(buff));
			}
			close(file[0]);
			close(file[1]);
			kill(getpid(),SIGKILL);
		} else if (pid > 0){
			smash.setCurrentFgPid(pid);
			wait(nullptr);
			smash.setCurrentFgPid(getpid());
		} else {
			smash.setCurrentFgPid(getpid());
			perror("smash error: fork failed");
		}
	}
	smash.setCurrentFgPid(getpid());
}	

//--------------------------------
//Pipe Command
//--------------------------------

PipeCommand::PipeCommand(const char* cmd_line): Command(cmd_line){
	this->is_background = _isBackgroundComamnd(cmd_line);
	if(this->is_background){
		char* temp = const_cast<char*>(cmd_line);
		_removeBackgroundSign(temp);
		_parseCommandLine(temp, this->cmd_without_bg_sign);
	}
	this->place_of_sign = findPipeCommand(this->command, this->numOfArgs);
}

PipeCommand::~PipeCommand(){
	for (int i = 0; i < numOfArgs; i++) {
		free(this->command[i]);
	}
	for (int i = 0; i < numOfArgs; i++) {
		if(this->is_background){
			free(this->cmd_without_bg_sign[i]);
		}
	}
}

void PipeCommand::execute(){
	SmallShell& smash = SmallShell::getInstance();
	char* cmd_line_until_sign = this->create_cmd_command(true);
	char* cmd_line_after_sign = this->create_cmd_command(false);
	Command* cmd_writes = smash.CreateCommand(cmd_line_until_sign);
	Command* cmd_reads = smash.CreateCommand(cmd_line_after_sign);
	free(cmd_line_until_sign);
	free(cmd_line_after_sign);
	if(this->is_background){ //in background
		pid_t pid = fork();
		if(pid == 0){
			setpgrp();
			int my_pipe[2];
			if(strcmp(this->command[place_of_sign], err_to_in) == 0){  //error to in
				if(pipe(my_pipe) == -1){
					perror("smash error: pipe failed");
					kill(getpid(), SIGKILL);
				}
				pid_t pid1 = fork();
				if(pid1 == 0){
					close(2);
					close(my_pipe[0]);
					dup2(my_pipe[1], 2);
					cmd_writes->execute();
					close(my_pipe[1]);
					kill(getpid(), SIGKILL);
				} else if(pid1 > 0){
					waitpid(pid1, nullptr, WUNTRACED);
					close(0);
					close(my_pipe[1]);
					dup2(my_pipe[0], 0);
					cmd_reads->execute(); 
					close(my_pipe[0]);
				} else {
					perror("smash error: fork failed");
				}
			} else { //out to in
				if(pipe(my_pipe) == -1){
					perror("smash error: pipe failed");
					kill(getpid(), SIGKILL);
				}
				pid_t pid1 = fork();
				if(pid1 == 0){
					close(1);
					close(my_pipe[0]);
					dup2(my_pipe[1],1);
					cmd_writes->execute();
					close(my_pipe[1]);
					kill(getpid(), SIGKILL);
				} else if(pid1 > 0){
					waitpid(pid1, nullptr, WUNTRACED);
					close(0);
					close(my_pipe[1]);
					dup2(my_pipe[0],0);
					cmd_reads->execute(); 
					close(my_pipe[0]);
				} else {
					perror("smash error: fork failed");
				}
			}
			kill(getpid(), SIGKILL);
		} else if (pid == -1){
			perror("smash error: fork failed");
		} else {
			smash.getJobs()->addJob(this, pid, false, pid);
		}
	} else { //not in background
		pid_t pid = fork();
		if(pid == 0){
			setpgrp();
			smash.setCurrentFgGid(getpgrp());
			int my_pipe[2];
			if(strcmp(this->command[place_of_sign], err_to_in) == 0){ //error to in
				if(pipe(my_pipe) == -1){
					perror("smash error: pipe failed");
					kill(getpid(), SIGKILL);
				}
				pid_t pid1 = fork();
				if(pid1 == 0){
					close(2);
					close(my_pipe[0]);
					dup2(my_pipe[1],2);
					cmd_writes->execute();
					close(my_pipe[1]);
					kill(getpid(), SIGKILL);
				} else if(pid1 > 0){
					smash.setCurrentFgPid(pid1);
					waitpid(pid1, nullptr, WUNTRACED);
					close(0);
					close(my_pipe[1]);
					smash.setCurrentFgPid(getpid());
					dup2(my_pipe[0],0);
					cmd_reads->execute(); 
					close(my_pipe[0]);
				} else {
					smash.setCurrentFgPid(getpid());
					perror("smash error: fork failed");
				}
			} else { //out to in
				if(pipe(my_pipe) == -1){
					perror("smash error: pipe failed");
					kill(getpid(), SIGKILL);
				}
				smash.setCurrentFgPid(getpid());
				pid_t pid1 = fork();
				if(pid1 == 0){
					close(1);
					close(my_pipe[0]);
					dup2(my_pipe[1],1);
					cmd_writes->execute();
					close(my_pipe[1]);
					kill(getpid(), SIGKILL);
				} else if(pid1 > 0){
					smash.setCurrentFgPid(pid1);
					waitpid(pid1, nullptr, WUNTRACED);
					close(0);
					close(my_pipe[1]);
					smash.setCurrentFgPid(getpid());
					dup2(my_pipe[0],0);
					cmd_reads->execute(); 
					close(my_pipe[0]);
				} else {
					smash.setCurrentFgPid(getpid());
					perror("smash error: fork failed");
				}
			}
			kill(getpid(), SIGKILL);
		} else if (pid > 0){
			smash.setCurrentFgPid(pid);
			smash.setCurrentFgGid(pid);
			waitpid(pid, nullptr, WUNTRACED);
			smash.setCurrentFgPid(getpid());
			smash.setCurrentFgGid(0);
		} else {
			smash.setCurrentFgGid(0);
			smash.setCurrentFgPid(getpid());
			perror("smash error: fork failed");
		}
	}
	smash.setCurrentFgPid(getpid());
}

//--------------------------------
// Change Prompt Command
//--------------------------------

ChangePromptCommand::ChangePromptCommand(const char* cmd_line) :
BuiltInCommand(cmd_line) {
	size_t new_size = 0;
	if(numOfArgs > 1) {
		new_size = strlen(this->command[1]);
		memcpy(this->prompt, command[1], new_size);
		this->n = 2;
	} else if (numOfArgs == 1) {
		new_size = strlen("smash");
		memcpy(this->prompt, this->smash, new_size);
		this->n = 1;
	}
}

void ChangePromptCommand::execute(){
	SmallShell& s = SmallShell::getInstance();
	s.setPrompt(this->prompt);
}

ChangePromptCommand::~ChangePromptCommand(){
	for (int i = 0; i < numOfArgs; i++) {
		free(this->command[i]);
	}
	free(this->prompt);
}

//--------------------------------
//Show PID command
//--------------------------------

ShowPidCommand::ShowPidCommand(const char* cmd_line) :
BuiltInCommand(cmd_line), n(1){
	pid =  getpid();
}

ShowPidCommand::~ShowPidCommand(){
	for (int i = 0; i < numOfArgs; i++) {
		free(this->command[i]);
	}
}

void ShowPidCommand::execute(){
	cout << "smash pid is "<< pid << endl;
}

//--------------------------------
// Get Current Directory Command
//--------------------------------
GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) :
BuiltInCommand(cmd_line) {
	this->n = 1;
}

GetCurrDirCommand::~GetCurrDirCommand(){
	for (int i = 0; i < numOfArgs; i++) {
		free(command[i]);
	}
}

void GetCurrDirCommand::execute(){
	char dirpath[COMMAND_ARGS_MAX_LENGTH];
	if(nullptr == getcwd(dirpath,COMMAND_ARGS_MAX_LENGTH)){
		perror("smash error: getcwd failed");
		return;
	}
	cout << dirpath <<endl;
}

//--------------------------------
// Change directory command
//--------------------------------

ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char** plastPwd)
: BuiltInCommand(cmd_line) {
		if(plastPwd != nullptr && *plastPwd != nullptr){
			this->lastPwd = (char*)(malloc(strlen(*plastPwd) + 1));
			memcpy(this->lastPwd, *plastPwd, strlen(*plastPwd) + 1);
		} else {
			this->lastPwd = nullptr;
		}
}

void ChangeDirCommand::execute(){
	SmallShell& s = SmallShell::getInstance();
	char temp [COMMAND_ARGS_MAX_LENGTH] = {0};
	getcwd(temp, COMMAND_ARGS_MAX_LENGTH);
	if(numOfArgs > 2){
		cout << "smash error: cd: too many arguments" << endl;
		return;
	}
	if(strcmp(command[1], "-") == 0) {
		if(this->lastPwd == nullptr){
			cout << "smash error: cd: OLDPWD not set" << endl;
			return;
		} else {
			chdir(this->lastPwd);
			s.setPlastPwd(temp);
		}
		return;
	}
	int result = chdir(command[1]);
	if(result == -1){
		perror("smash error: chdir failed");
	} else {
		s.setPlastPwd(temp);
	}
}

ChangeDirCommand::~ChangeDirCommand(){
	free(this->lastPwd);
	for (int i = 0; i < numOfArgs; i++) {
		free(this->command[i]);
	}
}

//--------------------------------
//Jobs list
//--------------------------------

JobsList::JobsList(){
	vector<JobEntry*> jobs1;
	this->jobs = jobs1;
}

JobsList::~JobsList(){
	for(JobEntry* j : this->jobs){
		delete j;
	}
	//delete this->jobs (vector) ?
}

void JobsList::addJob(Command* cmd,pid_t pid, bool isStopped, pid_t g){
	this->removeFinishedJobs();
	time_t t;
	time(&t);
	pid_t p = pid;
	JobEntry* j = new JobEntry(cmd,cmd->getCommand(), p, isStopped, t, cmd->getNumOfArgs(), g);
	(this->jobs).push_back(j);
}

void JobsList::printJobsList(){
	this->removeFinishedJobs();
	int i = 1;
	for(JobEntry* j : this->jobs){
		if(j->getIsStopped()){
			time_t t;
			time(&t);
			time_t elapsed = difftime(t, j->getTime());
			cout<< "[" << i << "]";
			j->printArgs(j->getJob(), j->getNumOfArgs());
			cout << " : " << j->getPid() << " "
			<< elapsed << " secs (stopped)" << endl;
		} else {
			time_t t;
			time(&t);
			time_t elapsed = difftime(t, j->getTime());
			cout << "[" << i << "]";
			j->printArgs(j->getJob(), j->getNumOfArgs());
			cout << " : " << j->getPid() << " "
			<< elapsed << " secs" << endl;
		}
		i++;
	}
}

 JobsList::JobEntry* JobsList::getJobById(int jobId){
	this->removeFinishedJobs();
	if(((int)(this->jobs).size()) < jobId || jobId < 0){
		 return nullptr;
	}
	return (this->jobs)[jobId - 1]; 
}

void JobsList::removeJobById(int jobId){
	this->removeFinishedJobs();
	//jobId means the place in the vector minus 1
	int size = this->jobs.size();
	if(jobId < 1 || jobId > size){
		return;
	}
	JobEntry* temp = this->jobs[jobId - 1];
	this->jobs.erase(this->jobs.begin() + (jobId - 1));
	delete temp;
}

JobsList::JobEntry* JobsList::getLastStoppedJob(int* jobId){
	this->removeFinishedJobs();
	if(this->jobs.empty()){
		return nullptr;
	}
	int size = this->jobs.size();
	vector<JobEntry*>::iterator i = this->jobs.end();
	i--;
	for (; i != this->jobs.begin(); i--) {
		if((*i)->getIsStopped()){
			*jobId = size;
			return *i; // check: need to copy i and then return the copy ?
		}
		size--;
	}
	if((*i)->getIsStopped()){
		*jobId = size;
		return *i;
	}
	return nullptr;
}

void JobsList::killAllJobs(){
	//THIS FUNCTION IS USED BY QUIT COMMAND EXECUTE ACCORDING TO ITS
	//SYNTAX! DO NOT CHANGE THIS BEFORE CHANGING QUIT COMMAND EXECUTE!
	this->removeFinishedJobs();
	int numOfJobs=this->getJobs().size();
	cout<<"smash: sending SIGKILL signal to "<<numOfJobs<<" jobs:"<<endl;
	for (vector<JobEntry*>::iterator it = jobs.begin() ; it != jobs.end(); ++it){
		int pid= (*it)->getPid();
		cout<<pid<<":";
		(*it)->printArgs((*it)->getJob(), (*it)->getNumOfArgs());
		cout<<endl;
		kill((*it)->getPid(),SIGKILL);
	}
}

/*void JobsList::removeFinishedJobs(){
	int i = 1;
	for(JobEntry* j :this->jobs){
		if(kill(j->getPid(), 0) == -1 || waitpid(j->getPid(), nullptr, WNOHANG)) {
			JobEntry* temp = this->jobs[i - 1];
			this->jobs.erase(this->jobs.begin() + (i - 1));
			delete temp;
			continue;
		}
		i++;
	}
}*/

void JobsList::removeFinishedJobs(){
	int i = 1, wstatus = 0, res = 0;
	for(auto j : this->jobs){
		if(j->getGroupID() == 0) {
		pid_t pid = j->getPid();
		res = waitpid(pid, &wstatus, WNOHANG);
		if ((WIFEXITED(wstatus) || WIFSIGNALED(wstatus)) && pid == res){
			/*cout << "res: " << res << endl;
			cout << "pid: " << pid << endl;
			cout << (WIFEXITED(wstatus)) << endl;
			cout << (WIFSIGNALED(wstatus)) << endl;*/
			JobEntry* temp = this->jobs[i - 1];
			this->jobs.erase(this->jobs.begin() + (i - 1));
			delete temp;
			continue;
		} else {
			if(res == -1){
				perror("smash error: waitpid failed");
			}
		}
		i++;
		} /*else {
			//res = waitpid(j->getGroupID(), &wstatus, WNOHANG);
			if((WIFEXITED(wstatus) || WIFSIGNALED(wstatus)) && j->getGroupID() == res){
				JobEntry* temp = this->jobs[i - 1];
				this->jobs.erase(this->jobs.begin() + (i - 1));
				delete temp;
				continue;
			} else {
				if(res == -1){
					cout << "its here" << endl;
					perror("smash error: waitpid failed");
				}
			}
			i++;
		}*/
	}
}

//--------------------------------
//Jobs Command
//--------------------------------

JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs):
BuiltInCommand(cmd_line){
	j = jobs;
}

JobsCommand::~JobsCommand(){
	for (int i = 0; i < numOfArgs; i++) {
		free(command[i]);
	}
}

void JobsCommand::execute(){
	j->printJobsList();
}

JobsList::JobEntry* JobsList::getLastJob(int* last_job_id){
	if(this->jobs.empty()){
		return nullptr;
	}
	int size = this->jobs.size();
	*last_job_id = size;
	return this->jobs[size - 1];
}

//--------------------------------
//Kill Command
//--------------------------------

KillCommand::KillCommand(const char* cmd_line, JobsList* jobs):
BuiltInCommand(cmd_line){
	j = jobs;
}

KillCommand::~KillCommand(){
	for (int i = 0; i < numOfArgs; i++) {
		free(command[i]);
	}
}


void KillCommand::execute(){
	SmallShell& s = SmallShell::getInstance();
	if(numOfArgs != 3){
		cout << "smash error: kill: invalid arguments" << endl;
		return;
	}
	int signum = (-1)*(atoi(command[1]));
	int job_id = atoi(command[2]);
	if(signum <= 0 || job_id <= 0){
		cout << "smash error: kill: invalid arguments" << endl;
		return;
	}
	JobsList::JobEntry* j_entry=s.getJobs()->getJobById(job_id);
	if(j_entry == nullptr){
		cout << "smash error: kill: job-id "<<job_id<<" does not exist" << endl;
		return;
	}
	int j_pid = j_entry->getPid();
	cout<<"signal number "<<signum<<" was sent to pid "<<j_pid<<endl;
	if(kill(j_pid,signum) == -1){
		perror("smash error: kill failed");
	}
}

//--------------------------------
//Foreground command
//--------------------------------

ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs)
: BuiltInCommand(cmd_line) {
	this->j = jobs;
}

ForegroundCommand::~ForegroundCommand(){
	for (int i = 0; i < numOfArgs; i++) {
		free(command[i]);
	}
}

void ForegroundCommand::execute(){
	if(nullptr == this->j){
		cout<< "smash error: fg: jobs list is empty" << endl;
	}
	if(this->j->getJobs().empty() && this->numOfArgs == 1){
		cout<< "smash error: fg: jobs list is empty" << endl;
		return;
	}
	if(this->numOfArgs > 2) {
		cout<< "smash error: fg: invalid arguments" <<endl;
		return;
	}
	if(this->numOfArgs == 2 && atoi(this->command[1]) <= 0) {
		cout<< "smash error: fg: invalid arguments" <<endl;
		return;
	}
	int last_job_id = 0;
	SmallShell& s = SmallShell::getInstance();
	if(this->numOfArgs == 2){
		int job_id = atoi(this->command[1]);
		JobsList::JobEntry* j_entry = s.getJobs()->getJobById(job_id);
		if(j_entry == nullptr){
			cout << "smash error: fg: job-id "<<job_id<<" does not exist" << endl;
			return;
		}
		pid_t pid = j_entry->getPid();
		j_entry->printArgsWithoutFirstSpace((j_entry->getJob()), j_entry->getNumOfArgs());
		cout << " : " << pid << endl;
		if(j_entry->getGroupID() == 0){ //not special command
			if(kill(pid, SIGCONT) == 0){
				s.setCurrentFgPid(pid);
				s.setCurrentFgGid(0);
				int wstatus = 0;
				s.setCurrentCommand(j_entry->getCmd());
				pid_t result = waitpid(pid, &wstatus, WUNTRACED);
				if(result != pid){
					perror("smash error: waitpid failed");
					return;
				}
			} else {
				perror("smash error: kill failed");
				return;
			}
		} else { //special command
			if(killpg(j_entry->getGroupID(), SIGCONT) == 0){
				s.setCurrentFgPid(j_entry->getPid());
				s.setCurrentFgGid(j_entry->getGroupID());
				int wstatus = 0;
				s.setSpecialCurrentCommand(j_entry->getCmd());
				pid_t result = waitpid(pid, &wstatus, WUNTRACED);
				if(result != pid){
					perror("smash error: waitpid failed");
					return;
				}
			} else {
				perror("smash error: kill failed");
				return;
			}
		}
		this->j->removeJobById(job_id);
	} else if (this->numOfArgs == 1) {
		JobsList::JobEntry* j_entry = this->j->getLastJob(&last_job_id);
		if(nullptr == j_entry){
			cout << "smash error: fg: jobs list is empty" << endl;
		}
		j_entry->printArgsWithoutFirstSpace((j_entry->getJob()), j_entry->getNumOfArgs());
		cout << " : " << (j_entry->getPid()) << endl;
		pid_t pid = j_entry->getPid();
		if(j_entry->getGroupID() == 0){
			if(kill(j_entry->getPid(), SIGCONT) == 0){
				s.setCurrentFgPid(j_entry->getPid());
				s.setCurrentFgGid(0);
				int wstatus = 0;
				s.setCurrentCommand(j_entry->getCmd());
				pid_t result = waitpid(pid, &wstatus, WUNTRACED);
				if(result != pid){
					perror("smash error: waitpid failed");
					return;
				}
			} else {
				perror("smash error: kill failed");
				return;
			}
		} else {
			if(killpg(j_entry->getGroupID(), SIGCONT) == 0){
				s.setCurrentFgPid(j_entry->getPid());
				s.setCurrentFgGid(j_entry->getGroupID());
				int wstatus = 0;
				s.setSpecialCurrentCommand(j_entry->getCmd());
				pid_t result = waitpid(pid, &wstatus, WUNTRACED);
				if(result != pid){
					perror("smash error: waitpid failed");
					return;
				}
			} else {
				perror("smash error: kill failed");
				return;
			}
		}
		this->j->removeJobById(last_job_id);
	}
}

//--------------------------------
//Background Command
//--------------------------------

BackgroundCommand::BackgroundCommand(const char* cmd_line, JobsList* jobs):
BuiltInCommand(cmd_line){
	this->j = jobs;
}

BackgroundCommand::~BackgroundCommand(){
	for (int i = 0; i < numOfArgs; i++) {
		free(command[i]);
	}
}

void BackgroundCommand::execute(){
	if(this->numOfArgs > 2){
		cout << "smash error: bg: invalid arguments" <<endl;
		return;
	}
	if(this->numOfArgs == 2 && atoi(this->command[1]) <= 0){
		cout << "smash error: bg: invalid arguments" <<endl;
		return;
	}
	if(this->numOfArgs == 1) {
		int jobId = 0;
		JobsList::JobEntry* j_entry = this->j->getLastStoppedJob(&jobId);
		if(nullptr == j_entry){
			cout << "smash error: bg: there is no stopped jobs to resume" << endl;
			return;
		}
		pid_t pid = j_entry->getPid();
		j_entry->printArgsWithoutFirstSpace((j_entry->getJob()), j_entry->getNumOfArgs());
		cout << " : " << pid << endl;
		if(j_entry->getGroupID() == 0){
			if(kill(pid, SIGCONT) == -1){
				perror("smash error: kill failed");
				return;
			} else {
				j_entry->setIsStopped(false);
			}
		} else {
			if(killpg(j_entry->getGroupID(), SIGCONT) == -1){
				perror("smash error: kill failed");
				return;
			} else {
				cout << "1" <<endl;
				j_entry->setIsStopped(false);
			}
		}
	} else { //numOfArgs = 2
		int jobId = atoi(this->command[1]);
		JobsList::JobEntry* j_entry = this->j->getJobById(jobId);
		if(nullptr == j_entry){
			cout << "smash error: bg: job-id " << jobId << " does not exist" << endl;
			return;
		}
		if(!(j_entry->getIsStopped())){
			cout<< "smash error: bg: job-id " << jobId << " is already running in the background" <<endl;
			return;
		}
		pid_t pid = j_entry->getPid();
		j_entry->printArgsWithoutFirstSpace((j_entry->getJob()), j_entry->getNumOfArgs());
		cout << " : " << pid << endl;
		if(j_entry->getGroupID() == 0){	
			if(kill(pid, SIGCONT) == -1){
				perror("smash error: kill failed");
				return;
			} else {
				j_entry->setIsStopped(false);
			}
		} else {
			if(killpg(j_entry->getGroupID(), SIGCONT) == -1){
				perror("smash error: kill failed");
				return;
			} else {
				j_entry->setIsStopped(false);
			}
		}
	}
}

//--------------------------------
//Quit Command
//--------------------------------

QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs):
BuiltInCommand(cmd_line){
	this->j = jobs;
}

void QuitCommand::execute(){
	if((this->numOfArgs >= 2) && strcmp(this->command[1],"kill") == 0) {
		j->killAllJobs();
	}
	exit(0);
}


// TODO: Add your implementation for classes in Commands.h 


//--------------------------------
//Small Shell
//--------------------------------

SmallShell::SmallShell() {
// TODO: add your implementation
	this->jobs = new JobsList();
}

SmallShell::~SmallShell() {
	free(this->plastPwd);
	delete this->jobs;
// TODO: add your implementation
}


/**
 * destroys temporary array which is used in Create Command
 * */
static void destroyTemp (char** a, int n){
	for (int i = 0; i < n; i++) {
		free(a[i]);
	}
}

/**
 * find Pipe sign
 **/
 static int findPipeCommand(char** a, int n) {
	 for (int i = 1; i < n; i++) {
		 if (strcmp(a[i], "|") == 0 || strcmp(a[i], "|&") == 0){
			 return i;
		 }
	 }
	 return -1;
 }

/**
 * find Redirection sign
 **/
 static int findRedirectionCommand(char** a, int n){
	 for (int i = 1; i < n; i++) {
		 if (strcmp(a[i], ">") == 0 || strcmp(a[i], ">>") == 0){
			 return i;
		 }
	 }
	 return -1;
 }

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
	// For example:
	char* temp[COMMAND_MAX_ARGS] = {0};
	int n = _parseCommandLine(cmd_line, temp);
	if(n == 0){
		return nullptr;
	}
	if(n >= 3 && findRedirectionCommand(temp, n) > 0){
		destroyTemp(temp, n);
		RedirectionCommand *c = new RedirectionCommand(cmd_line);
		this->setSpecialCurrentCommand(c);
		return c;
	}
	if(n >= 3 && findPipeCommand(temp, n) > 0){
		destroyTemp(temp, n);
		PipeCommand *c = new PipeCommand(cmd_line);
		this->setSpecialCurrentCommand(c);
		return c;
	}
	if (strcmp(temp[0], "chprompt") == 0) {
		destroyTemp(temp, n);
		ChangePromptCommand* c = new ChangePromptCommand(cmd_line);
		this->setCurrentCommand(c);
		return c;
	}
	if (strcmp(temp[0], "showpid") == 0) {
		destroyTemp(temp, n);
		ShowPidCommand* c = new ShowPidCommand(cmd_line);
		this->setCurrentCommand(c);
		return c;
	}
	if (strcmp(temp[0], "pwd") == 0) {
		destroyTemp(temp, n);
		GetCurrDirCommand* c = new GetCurrDirCommand(cmd_line);
		this->setCurrentCommand(c);
		return c;
	}
	if(strcmp(temp[0], "cd") == 0){
		destroyTemp(temp, n);
		ChangeDirCommand* c = new ChangeDirCommand(cmd_line, &(this->plastPwd));
		this->setCurrentCommand(c);
		return c;
	}
	SmallShell& smash = SmallShell::getInstance();
	if(strcmp(temp[0], "jobs") == 0){
		destroyTemp(temp, n);
		JobsCommand* c = new JobsCommand(cmd_line, smash.getJobs());
		this->setCurrentCommand(c);
		return c;
	}
	if(strcmp(temp[0], "kill") == 0){
		destroyTemp(temp, n);
		KillCommand* c = new KillCommand(cmd_line, smash.getJobs());
		this->setCurrentCommand(c);
		return c;
	}
	if(strcmp(temp[0], "fg") == 0){
		destroyTemp(temp, n);
		ForegroundCommand* c = new ForegroundCommand(cmd_line, smash.getJobs());
		this->setCurrentCommand(c);
		return c;
	}
	if(strcmp(temp[0], "bg") == 0){
		destroyTemp(temp, n);
		BackgroundCommand *c = new BackgroundCommand(cmd_line, smash.getJobs());
		this->setCurrentCommand(c);
		return c;
	}
	if(strcmp(temp[0], "quit") == 0){
		destroyTemp(temp, n);
		QuitCommand *c = new QuitCommand(cmd_line, smash.getJobs());
		this->setCurrentCommand(c);
		return c;
	}
	if (strcmp(temp[0], "cp") == 0) {
		destroyTemp(temp, n);
		CopyCommand* c = new CopyCommand(cmd_line);
		this->setCurrentCommand(c);
		return c;
	}
	
	
	ExternalCommand *c = new ExternalCommand(cmd_line);
	this->setCurrentCommand(c);
	return c;
	//return nullptr;
}

void SmallShell::setPrompt(char* new_prompt){
	char promptempty[80] = {0};
	memcpy(this->prompt, promptempty, 80);
	memcpy(this->prompt, new_prompt, strlen(new_prompt));
}

char* SmallShell::getPrompt(){
	return this->prompt;
}

char* SmallShell::getlastPwd(){
	return this->plastPwd;
}

void SmallShell::setPlastPwd(char *pwd_new){
	free(this->plastPwd);
	this->plastPwd = (char*)(malloc((strlen(pwd_new) + 1)));
	memcpy(this->plastPwd, pwd_new, strlen(pwd_new) + 1);
}

JobsList* SmallShell::getJobs() {
	return this->jobs;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
   Command* cmd = CreateCommand(cmd_line);
   if(nullptr == cmd){
	   return;
   }
   cmd->execute();
   //delete cmd;
	// Please note that you must fork smash process for some commands (e.g., external commands....)
}
