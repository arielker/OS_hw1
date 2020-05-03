#include <sstream>
#include <iomanip>
#include "Commands.h"
#include <signal.h>

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cerr << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cerr << __PRETTY_FUNCTION__ << " <-- " << endl;
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

static void destroyTemp (char** a, int n);

//--------------------------------
//Command
//--------------------------------

Command::Command(const char* cmd_line) : is_background(false) {
	this->cmd_line=(char*) malloc(strlen(cmd_line)+1);
	memcpy(this->cmd_line,cmd_line, strlen(cmd_line) + 1);
	this->numOfArgs = _parseCommandLine(cmd_line, this->command);
}

Command::~Command(){
	/*for (int i = 0; i < numOfArgs; i++) {
		free(this->command[i]);
	}*/
	free(this->cmd_line);
}

//--------------------------------
//Built In Commands
//--------------------------------

BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line){}

BuiltInCommand::~BuiltInCommand(){
	for (int i = 0; i < COMMAND_MAX_ARGS; i++) {
		//free(this->command[i]);
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
	FUNC_ENTRY()
	SmallShell& smash = SmallShell::getInstance();
	if(!(this->is_background)) {
		if(!smash.getIsForked()){
			pid_t pid = fork();
			if (pid == 0) {
				setpgrp();
				int result = execv(this->bin_bash, this->external_args);
				if(result == -1) {
					perror("smash error: execv failed");
					smash.setCurrentFgPid(smash.getSmashPid());
					exit(0);
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
			int result = execv(this->bin_bash, this->external_args);
			if(result == -1) {
				perror("smash error: execv failed");
				smash.setCurrentFgPid(smash.getSmashPid());
				exit(0);
			}
		}
	} else {
		if(!smash.getIsForked()){
			pid_t pid = fork();
			if (pid == 0) {
				setpgrp();
				int result = execv(this->bin_bash, this->external_args);
				if(result == -1){
					perror("smash error: execv failed");
					exit(0);
				}
			} else if (pid < 0){
				smash.setCurrentFgPid(getpid());
				perror("smash error: fork failed");
			} else {
				smash.getJobs()->addJob(this,pid); //why is this here?
			}
		} else {
			int result = execv(this->bin_bash, this->external_args);
			if(result == -1){
				perror("smash error: execv failed");
				exit(0);
			}
		}
	}
	FUNC_EXIT()
}

//--------------------------------
//Redirection Command
//--------------------------------

RedirectionCommand::RedirectionCommand(const char* cmd_line): 
Command(cmd_line){
	FUNC_ENTRY()
	this->cmd_line_tmp = string(cmd_line);
	this->red_sign_len = 0;
	this->red_index = 0;
	this->is_append = true;
	if((red_index = cmd_line_tmp.find(">>")) != string::npos){
		this->red_sign_len = 2;
	} else if((red_index = cmd_line_tmp.find(">")) != string::npos) {
		this->red_sign_len = 1;
		this->is_append = false;
	}
	this->is_background = _isBackgroundComamnd(cmd_line);
	if(this->is_background){
		char* temp = const_cast<char*>(cmd_line);
		_removeBackgroundSign(temp);
		string t1 = string(temp);
		_trim(t1);
		this->file_name = t1.substr(red_index + red_sign_len, cmd_line_tmp.size() - red_index - red_sign_len);
		this->file_name = _trim(this->file_name);
	} else {
		file_name = this->cmd_line_tmp.substr(red_index + red_sign_len, cmd_line_tmp.size() - red_index - red_sign_len);
		file_name = _trim(file_name);
	}
	FUNC_EXIT()
}

void RedirectionCommand::execute(){
	FUNC_ENTRY()
	SmallShell& smash = SmallShell::getInstance();
	smash.getJobs()->removeFinishedJobs();
	string cmd_line_until_sign = this->cmd_line_tmp.substr(0, red_index);
	cmd_line_until_sign = _trim(cmd_line_until_sign);
	Command* cmd = smash.CreateCommand(cmd_line_until_sign.c_str());
	if(this->is_background){ //in background
		pid_t pid = fork();
		if(pid == 0){
			setpgrp();
			if(this->is_append) { //append
				pid_t pid1 = fork();
				if(pid1 == 0){
					close(1);
					int open_res = open(file_name.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0666);
					if (open_res == -1) {
						perror("smash error: open failed");
						delete cmd;
						return;
					}
					smash.setIsForked(true);
					cmd->execute();
					smash.setIsForked(false);
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
					int open_res = open(file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
					if (open_res == -1){
						perror("smash error: open failed");
						return;
					}
					smash.setIsForked(true);
					cmd->execute();
					smash.setIsForked(false);
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
			smash.getJobs()->addJob(this, pid, false, pid);
		}
	} else { // not in background
		if(this->is_append) { //append
			pid_t pid = fork();
			if(pid == 0){
				setpgrp();
				close(1);
				int open_res = open(file_name.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0666);
				if (open_res == -1){
					perror("smash error: open failed");
					return;
				}
				smash.setIsForked(true);
				cmd->execute();
				smash.setIsForked(false);
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
			pid_t pid = fork();
			if(pid == 0){
				setpgrp();
				close(1);
				int open_res = open(file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
				if (open_res == -1){
					perror("smash error: open failed");
					exit(0);
					return;
				}
				smash.setIsForked(true);
				cmd->execute();
				smash.setIsForked(false);
				if(-1 == close(open_res)){
					perror("smash error: close failed");
					return;
				}
				delete cmd;
				kill(getpid(),SIGKILL);
				exit(0);
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
	FUNC_EXIT()
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
		for (int i = 0; i < this->n; i++) {
			free(cmd_without_bg_sign[i]);
		}
	}
	for (int i = 0; i < this->numOfArgs; i++){
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
			char buff[4096];
			ssize_t count;
			int file[2];
			file[0]= open(sourceAddress,O_RDONLY);
			if(file[0] == -1){
				perror("smash error: open failed");
				kill(getpid(), SIGKILL);
				return;
			}
			char a[4096];
			realpath(sourceAddress,a);
			char b[4096];
			realpath(destinationAddress,b);
			if(a != nullptr && b != nullptr){
				if(strcmp(a,b) == 0){
					close(file[0]);
					cout<<"smash: "<<sourceAddress<<" was copied to "<<destinationAddress<<endl;
					kill(getpid(),SIGKILL);
					return;
				}
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
			if(close(file[0]) == -1){
				perror("smash error: close failed");
				if(close(file[1]) == -1){
					perror("smash error: close failed");
				}
				kill(getpid(), SIGKILL);
				return;
			}
			if(close(file[1]) == -1){
				perror("smash error: close failed");
				kill(getpid(), SIGKILL);
				return;
			}
			cout<<"smash: "<<sourceAddress<<" was copied to "<<destinationAddress<<endl;
			kill(getpid(),SIGKILL);
		} else if(pid > 0){
			smash.getJobs()->addJob(this,pid);
		} else {
			perror("smash error: fork failed");
			return;
		}
	} else { //not in background
		pid_t pid = fork();
		if(pid == 0){
			char* sourceAddress=this->command[1];
			char* destinationAddress=this->command[2];
			char buff[4096];
			ssize_t count;
			int file[2];
			file[0] = open(sourceAddress,O_RDONLY);
			if(file[0] == -1){
				perror("smash error: open failed");
				kill(getpid(), SIGKILL);
				return;
			}
			char a[4096];
			realpath(sourceAddress,a);
			char b[4096];
			realpath(destinationAddress,b);
			if(a != nullptr && b != nullptr){
				if(strcmp(a,b)==0){
					close(file[0]);
					cout<<"smash: "<<sourceAddress<<" was copied to "<<destinationAddress<<endl;
					kill(getpid(),SIGKILL);
					return;
				}		
			}
			file[1] = open(destinationAddress, O_WRONLY | O_CREAT | O_TRUNC,0666);
			if(file[1] == -1){
				if(close(file[0]) == -1){
					perror("smash error: close failed");
				}
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
			if(close(file[0]) == -1){
				perror("smash error: close failed");
				if(close(file[1]) == -1){
					perror("smash error: close failed");
				}
				kill(getpid(), SIGKILL);
				return;
			}
			if(close(file[1]) == -1){
				perror("smash error: close failed");
				kill(getpid(), SIGKILL);
				return;
			}
			cout<<"smash: "<<sourceAddress<<" was copied to "<<destinationAddress<<endl;
			kill(getpid(),SIGKILL);
		} else if (pid > 0){
			smash.setCurrentFgPid(pid);
			waitpid(pid, nullptr, WUNTRACED);
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
	this->cmd_line_tmp = string(cmd_line);
	this->pipe_sign_len = 0;
	this->pipe_index = 0;
	this->is_err_to_in = true;
	this->is_background = _isBackgroundComamnd(cmd_line);
	if((pipe_index = cmd_line_tmp.find("|&")) != string::npos){
		this->pipe_sign_len = 2;
	} else if((pipe_index = cmd_line_tmp.find("|")) != string::npos) {
		this->pipe_sign_len = 1;
		this->is_err_to_in = false;
	}
	this->cmd1 = this->cmd_line_tmp.substr(0, pipe_index);
	this->cmd1 = _trim(cmd1);
	if(this->is_background){
		char* temp = const_cast<char*>(cmd_line);
		_removeBackgroundSign(temp);
		string t1 = string(temp);
		_trim(t1);
		this->cmd2 = t1.substr(pipe_index + pipe_sign_len, cmd_line_tmp.size() - pipe_index - pipe_sign_len);
		this->cmd2 = _trim(this->cmd2);
	} else {
		this->cmd2 = this->cmd_line_tmp.substr(pipe_index + pipe_sign_len, cmd_line_tmp.size() - pipe_index - pipe_sign_len);
		this->cmd2 = _trim(this->cmd2);
	}
}

PipeCommand::~PipeCommand(){
	for (int i = 0; i < numOfArgs; i++) {
		free(this->command[i]);
	}
}

void PipeCommand::execute(){
	SmallShell& smash = SmallShell::getInstance();
	if(this->is_background){ //in background
		Command* cmd_writes = smash.CreateCommand(cmd1.c_str());
		Command* cmd_reads = smash.CreateCommand(cmd2.c_str());
		smash.getJobs()->removeFinishedJobs();
		pid_t pid = fork();
		if(pid == 0){
			setpgrp();
			int my_pipe[2];
			if(this->is_err_to_in){  //error to in
				if(pipe(my_pipe) == -1){
					perror("smash error: pipe failed");
					kill(getpid(), SIGKILL);
				}
				pid_t pid1 = fork();
				if(pid1 == 0){
					close(2);
					close(my_pipe[0]);
					dup2(my_pipe[1], 2);
					smash.setIsForked(true);
					cmd_writes->execute();
					close(my_pipe[1]);
					smash.setIsForked(false);
					kill(getpid(), SIGKILL);
				} else if(pid1 > 0){
					pid_t pid2 = fork();
					if(pid2 == 0){
						close(0);
						close(my_pipe[1]);
						dup2(my_pipe[0], 0);
						smash.setIsForked(true);
						cmd_reads->execute();
						smash.setIsForked(false);
						close(my_pipe[0]);
					} else if(pid2 == -1){
						perror("smash error: fork failed");
					} else {
						close(my_pipe[1]);
						close(my_pipe[0]);
						while(wait(nullptr) != -1);
					}
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
					smash.setIsForked(true);
					cmd_writes->execute();
					close(my_pipe[1]);
					smash.setIsForked(false);
					kill(getpid(), SIGKILL);
				} else if(pid1 > 0){
					pid_t pid2 = fork();
					if(pid2 == 0){
						close(0);
						close(my_pipe[1]);
						dup2(my_pipe[0],0);
						smash.setIsForked(true);
						cmd_reads->execute();
						smash.setIsForked(false);
						close(my_pipe[0]);
					} else if(pid2 == -1){
						perror("smash error: fork failed");
					} else {
						close(my_pipe[1]);
						close(my_pipe[0]);
						while(wait(nullptr) != -1);
						smash.setIsForked(false);
					}
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
		Command* cmd_writes = smash.CreateCommand(cmd1.c_str());
		Command* cmd_reads = smash.CreateCommand(cmd2.c_str());
		smash.getJobs()->removeFinishedJobs();
		pid_t pid = fork();
		if(pid == 0){
			setpgrp();
			smash.setCurrentFgGid(getpgrp());
			int my_pipe[2];
			if(this->is_err_to_in){ //error to in
				if(pipe(my_pipe) == -1){
					perror("smash error: pipe failed");
					kill(getpid(), SIGKILL);
				}
				pid_t pid1 = fork();
				if(pid1 == 0){
					close(2);
					close(my_pipe[0]);
					dup2(my_pipe[1],2);
					smash.setIsForked(true);
					cmd_writes->execute();
					smash.setIsForked(false);
					close(my_pipe[1]);
					kill(getpid(), SIGKILL);
					exit(0);
				} else if(pid1 > 0){
					pid_t pid2 = fork();
					if(pid2 == 0){
						close(0);
						close(my_pipe[1]);
						smash.setCurrentFgPid(getpid());
						dup2(my_pipe[0],0);
						smash.setIsForked(true);
						cmd_reads->execute(); 
						smash.setIsForked(false);
						close(my_pipe[0]);
						exit(0);
					} else if (pid2 == -1){
						perror("smash error: fork failed");
					} else {
						close(my_pipe[1]);
						close(my_pipe[0]);
						while(wait(nullptr) != -1);
					}
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
					smash.setIsForked(true);
					cmd_writes->execute();
					smash.setIsForked(false);
					close(my_pipe[1]);
					kill(getpid(), SIGKILL);
				} else if(pid1 > 0){
					pid_t pid2 = fork();
					if(pid2 == 0){
						close(0);
						close(my_pipe[1]);
						smash.setCurrentFgPid(getpid());
						dup2(my_pipe[0],0);
						smash.setIsForked(true);
						cmd_reads->execute(); 
						smash.setIsForked(false);
						close(my_pipe[0]);
						kill(getpid(),SIGKILL);
					} else if (pid2 == -1){
						perror("smash error: fork failed");
					} else {
						close(my_pipe[1]);
						close(my_pipe[0]);
						while(wait(nullptr) != -1);
					}
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
	smash.setCurrentFgPid(smash.getSmashPid());
	smash.setCurrentFgGid(0);
}

//--------------------------------
// Change Prompt Command
//--------------------------------

ChangePromptCommand::ChangePromptCommand(const char* cmd_line) :
BuiltInCommand(cmd_line) {
	FUNC_ENTRY()
	size_t new_size = 0;
	memset(this->prompt, 0, 80);
	if(numOfArgs > 1) {
		if(strcmp(this->command[1], "&") == 0){
			new_size = strlen("smash");
			memcpy(this->prompt, this->smash, new_size);
			this->n = 1;
		} else { 
			_removeBackgroundSign(this->command[1]);
			string t1 = string(this->command[1]);
			t1 = _trim(t1);
			new_size = strlen(t1.c_str());
			memcpy(this->prompt, t1.c_str(), new_size);
			this->n = 2;
		}
	} else if (numOfArgs == 1) {
		new_size = strlen("smash");
		memcpy(this->prompt, this->smash, new_size);
		this->n = 1;
	}
	SmallShell& s = SmallShell::getInstance();
	s.setPrompt(this->prompt);
	FUNC_EXIT()
}

void ChangePromptCommand::execute(){
	FUNC_ENTRY()
	/*SmallShell& s = SmallShell::getInstance();
	s.setPrompt(this->prompt);*/
	FUNC_EXIT()
	return;
}

ChangePromptCommand::~ChangePromptCommand(){
	for (int i = 0; i < numOfArgs; i++) {
		free(this->command[i]);
	}
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
	char* dirpath = get_current_dir_name();
	if(nullptr == dirpath){
		perror("smash error: get_current_dir_name failed");
		return;
	}
	cout << dirpath <<endl;
	free(dirpath);
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
	if(numOfArgs > 2 && strcmp(this->command[2], "&") != 0){
		cerr << "smash error: cd: too many arguments" << endl;
		return;
	}
	if(strcmp(command[1], "&") == 0){
		return;
	}
	_removeBackgroundSign(this->command[1]);
	string t1 = string(this->command[1]);
	t1 =_trim(t1); 
	if(strcmp(t1.c_str(), "-") == 0) {
		if(this->lastPwd == nullptr){
			cerr << "smash error: cd: OLDPWD not set" << endl;
			return;
		} else {
			if(chdir(this->lastPwd) == -1){
				perror("smash error: chdir failed");
				return;
			}
			s.setPlastPwd(temp);
		}
		return;
	}
	int result = chdir(t1.c_str());
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
	FUNC_ENTRY()
	SmallShell& s = SmallShell::getInstance();
	if(!s.getIsForked()){
		this->removeFinishedJobs();
	}
	time_t t;
	time(&t);
	pid_t p = pid;
	int max = 0;
	for(JobEntry* j : this->jobs){
		if(nullptr != j && j->getJobId() > max){
			max = j->getJobId();
		}
	}
	JobEntry* j = new JobEntry(cmd,cmd->getCommandLine(), p, isStopped, t, cmd->getNumOfArgs(), g, max + 1);
	(this->jobs).push_back(j); // insert at max + 1 ?
	FUNC_EXIT()
}

void JobsList::printJobsList(){
	SmallShell& s = SmallShell::getInstance();
	if(!s.getIsForked()) this->removeFinishedJobs();
	for(JobEntry* j : this->jobs){
		if(j->getIsStopped()){
			time_t t;
			time(&t);
			time_t elapsed = difftime(t, j->getTime());
			cout<< "[" << j->getJobId() << "] "<<j->getJob()
			<< " : " << j->getPid() << " "
			<< elapsed << " secs (stopped)" << endl;
		} else {
			time_t t;
			time(&t);
			time_t elapsed = difftime(t, j->getTime());
			cout<< "[" << j->getJobId() << "] "<<j->getJob()
			<< " : " << j->getPid() << " "
			<< elapsed << " secs" << endl;
		}
	}
}

 JobsList::JobEntry* JobsList::getJobById(int jobId){
	SmallShell& s = SmallShell::getInstance();
	if(!s.getIsForked()){
		this->removeFinishedJobs();
	}
	if(jobId < 0){
		 return nullptr;
	}
	for(JobEntry* j : this->jobs){
		if(j->getJobId() == jobId){
			return j;
		}
	}
	return nullptr;
}

void JobsList::removeJobById(int jobId){
	SmallShell& s = SmallShell::getInstance();
	if(!s.getIsForked()){
		this->removeFinishedJobs();
	}
	int size = this->jobs.size();
	if(jobId < 1 || jobId > size){
		return;
	}
	//JobEntry* temp = this->jobs[jobId - 1];
	//this->jobs.erase(this->jobs.begin() + (jobId - 1));
	//delete temp;
	//int i = 0;
	auto j = this->jobs.begin();
	while(j != this->jobs.end()){
		if((*j)->getJobId() == jobId){
			JobEntry* temp = *j;
			this->jobs.erase(j);
			delete temp;
			break;
		}
		j++;
	}
}

JobsList::JobEntry* JobsList::getLastStoppedJob(int* jobId){
	SmallShell& s = SmallShell::getInstance();
	if(!s.getIsForked()){
		this->removeFinishedJobs();
	}
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
	SmallShell& s = SmallShell::getInstance();
	if(!s.getIsForked())this->removeFinishedJobs();
	int numOfJobs=this->getJobs().size();
	cout<<"smash: sending SIGKILL signal to "<<numOfJobs<<" jobs:"<<endl;
	for (vector<JobEntry*>::iterator it = jobs.begin() ; it != jobs.end(); ++it){
		int pid= (*it)->getPid();
		cout<<pid<<": "<<(*it)->getJob()<<endl;
		kill((*it)->getPid(),SIGKILL);
	}
}

void JobsList::removeFinishedJobs(){
	FUNC_ENTRY()
	int wstatus = 0, res = 0;
	auto j = this->jobs.begin();
	while(j != this->jobs.end()){
		pid_t pid = (*j)->getPid();
		res = waitpid(pid, &wstatus, WNOHANG);
		if ((WIFEXITED(wstatus) || WIFSIGNALED(wstatus)) && pid == res){
			JobEntry* temp = *j;
			this->jobs.erase(j);
			delete temp;
			j = this->jobs.begin();
			continue;
		} else {
			if(res == -1){
				//---this is a cosmetic fix for foreground command problem---
				JobEntry* temp = *j;
				this->jobs.erase(j);
				delete temp;
				j = this->jobs.begin();
				continue;
				//---
				perror("smash error: waitpid failed");
			
			}
		}
		j++;
	}
	FUNC_EXIT()
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
	FUNC_ENTRY()
	if(this->jobs.empty()){
		return nullptr;
	}
	JobEntry* jmax;
	int max = 0;
	for(JobEntry* j : this->jobs){
		if(nullptr != j && j->getJobId() > max){
			max = j->getJobId();
			jmax = j;
		}
	}
	*last_job_id = max;
	return jmax;
	FUNC_EXIT()
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
	if(this->numOfArgs != 3){
		bool flag = false;
		if(numOfArgs == 4 && strcmp(this->command[3], "&") == 0){
			flag = true;
		}
		if(!flag) {	
			cerr << "smash error: kill: invalid arguments" << endl;
			return;
		}
	}
	if(this->numOfArgs == 3 && strcmp(this->command[2], "&") == 0){
		cerr << "smash error: kill: invalid arguments" << endl;
		return;
	}
	int signum;
	if(strcmp(command[1], "-0") == 0){
		signum = 0;
	} else {
		if(atoi(command[1]) == 0){
			cerr << "smash error: kill: invalid arguments" << endl;
			return;
		}
		else {
			signum = (-1)*(atoi(command[1]));
		}
	}
	int job_id = atoi(command[2]);
	if(strcmp(this->command[2], "0") != 0){
		if(job_id == 0){
			cerr << "smash error: kill: invalid arguments" << endl;
			return;
		}
	}
	if(signum < 0 || signum > 31){
		cerr << "smash error: kill: invalid arguments" << endl;
		return;
	}
	JobsList::JobEntry* j_entry = j->getJobById(job_id);
	if(j_entry == nullptr){
		cerr << "smash error: kill: job-id "<< job_id <<" does not exist" << endl;
		return;
	}
	int j_pid = j_entry->getPid();
	if(kill(j_pid,signum) == -1){
		perror("smash error: kill failed");
		return;
	}
	cout << "signal number "<< signum << " was sent to pid "<< j_pid << endl;
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
	FUNC_ENTRY()
	if(nullptr == this->j){
		cerr<< "smash error: fg: jobs list is empty" << endl;
	}
	if(this->j->getJobs().empty() && this->numOfArgs == 1){
		cerr<< "smash error: fg: jobs list is empty" << endl;
		return;
	}
	if(this->numOfArgs > 2 && strcmp(this->command[2], "&") != 0) {
		cerr<< "smash error: fg: invalid arguments" <<endl;
		return;
	}
	int last_job_id = 0;
	SmallShell& s = SmallShell::getInstance();
	if(this->numOfArgs == 2 && strcmp(this->command[1], "&") != 0){
		int wstatus = 0;
		int job_id = atoi(this->command[1]);
		if(strcmp(this->command[1], "0") != 0){ //what if fg -0 ?
			if(job_id == 0){
				cerr << "smash error: fg: invalid arguments" << endl;
				return;
			}
		}
		JobsList::JobEntry* j_entry = s.getJobs()->getJobById(job_id);
		if(j_entry == nullptr){
			cerr << "smash error: fg: job-id "<<job_id<<" does not exist" << endl;
			return;
		}
		pid_t pid = j_entry->getPid();
		cout<<j_entry->getJob();
		cout << " : " << pid << endl;
		if(j_entry->getGroupID() == 0){ //not special command
			if(kill(pid, SIGCONT) == 0){
				s.setCurrentFgPid(pid);
				s.setCurrentFgGid(0);
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
		
		this->j->removeFinishedJobs(); 
	} else {
		int wstatus = 0;
		JobsList::JobEntry* j_entry = this->j->getLastJob(&last_job_id);
		if(nullptr == j_entry){
			cerr << "smash error: fg: jobs list is empty" << endl;
			return;
		}
		cout<<j_entry->getJob();
		cout << " : " << (j_entry->getPid()) << endl;
		pid_t pid = j_entry->getPid();
		if(j_entry->getGroupID() == 0){
			if(kill(j_entry->getPid(), SIGCONT) == 0){
				s.setCurrentFgPid(j_entry->getPid());
				s.setCurrentFgGid(0);
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
		 this->j->removeFinishedJobs(); 
	}
	FUNC_EXIT()
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
	if(this->numOfArgs > 2 && strcmp(this->command[2], "&") != 0){
		cerr << "smash error: bg: invalid arguments" <<endl;
		return;
	}
	if(this->numOfArgs == 1 || (this->numOfArgs == 2 && strcmp(this->command[1], "&") == 0)) {
		int jobId = 0;
		JobsList::JobEntry* j_entry = this->j->getLastStoppedJob(&jobId);
		if(nullptr == j_entry){
			cerr << "smash error: bg: there is no stopped jobs to resume" << endl;
			return;
		}
		pid_t pid = j_entry->getPid();
		cout<<j_entry->getJob();
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
	} else { //numOfArgs = 2
		int jobId = atoi(this->command[1]);
		if (strcmp(this->command[1], "0") != 0){ //what if bg -0 ?
			if(jobId == 0){
				cerr << "smash error: bg: invalid arguments" << endl;
				return;
			}
		}
		JobsList::JobEntry* j_entry = this->j->getJobById(jobId);
		if(nullptr == j_entry){
			cerr << "smash error: bg: job-id " << jobId << " does not exist" << endl;
			return;
		}
		if(!(j_entry->getIsStopped())){
			cerr << "smash error: bg: job-id " << jobId << " is already running in the background" << endl;
			return;
		}
		pid_t pid = j_entry->getPid();
		cout<<j_entry->getJob();
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
	bool flag = false;
	for(int i = 0; i < COMMAND_MAX_ARGS; i++){
		if(this->command[i] != nullptr && strcmp(this->command[i],"kill") == 0){
			flag = true;
		}
	}
	if((this->numOfArgs >= 2) && flag) {
		j->killAllJobs();
	}
	exit(0);
}

//--------------------------------
//Small Shell
//--------------------------------

SmallShell::SmallShell() {
// TODO: add your implementation
	this->jobs = new JobsList();
}

SmallShell::~SmallShell() {
	//free(this->plastPwd);
	delete this->jobs;
// TODO: add your implementation
}


/**
 * destroys temporary array which is used in Create Command
 * */
static void destroyTemp (char** a, int n){
	for (int i = 0; i < n; i++) {
		//free(a[i]);
	}
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
	FUNC_ENTRY()
	// For example:
	char* temp[COMMAND_MAX_ARGS] = {0};
	int n = _parseCommandLine(cmd_line, temp);
	if(n == 0){
		return nullptr;
	}
	if((string(cmd_line)).find_first_of(">") != string::npos){
		destroyTemp(temp,n);
		RedirectionCommand* c = new RedirectionCommand(cmd_line);
		this->setSpecialCurrentCommand(c);
		return c;
	}
	if((string(cmd_line)).find_first_of("|") != string::npos){
		destroyTemp(temp,n);
		PipeCommand* c = new PipeCommand(cmd_line);
		this->setSpecialCurrentCommand(c);
		return c;
	}
	
	_removeBackgroundSign(temp[0]);
	string str=_trim(string(temp[0]));
	strcpy(temp[0], str.c_str());
	
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
		if(n == 1){
			return nullptr;
		}
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
	destroyTemp(temp, n);
	this->setCurrentCommand(c);
	return c;
	FUNC_EXIT()
	//return nullptr;
}

void SmallShell::setPrompt(char* new_prompt){
	FUNC_ENTRY()
	/*char promptempty[80] = {0};
	memcpy(this->prompt, promptempty, 80);
	memcpy(this->prompt, new_prompt, strlen(new_prompt));*/
	memset(prompt, 0, 80);
	strcpy(prompt, new_prompt);
	FUNC_EXIT()
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
