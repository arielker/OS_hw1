#include <unistd.h>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cerr << _PRETTY_FUNCTION_ << " --> " << endl;

#define FUNC_EXIT()  \
  cerr << _PRETTY_FUNCTION_ << " <-- " << endl;
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

//--------------------------------
//Command
//--------------------------------

Command::Command(const char* cmd_line){
	this->numOfArgs = _parseCommandLine(cmd_line, this->command);
}

Command::~Command(){
	for (int i = 0; i < COMMAND_MAX_ARGS; i++) {
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
// Change Prompt
//--------------------------------

ChangePromptCommand::ChangePromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {
	size_t new_size = 0;
	if(numOfArgs > 1) {
		new_size = strlen(this->command[1]);
		memcpy(this->prompt, command[1], new_size);
		this->numOfArgs = 2;
	} else if (numOfArgs == 1) {
		new_size = strlen("smash");
		memcpy(this->prompt, this->smash, new_size);
		this->numOfArgs = 1;
	}
}

void ChangePromptCommand::execute(){
	SmallShell& s = SmallShell::getInstance();
	s.setPrompt(this->prompt);
}

ChangePromptCommand::~ChangePromptCommand(){
	for (int i = 0; i < COMMAND_MAX_ARGS; i++) {
		free(this->command[i]);
	}
}

//--------------------------------
//Show PID command
//--------------------------------

ShowPidCommand::ShowPidCommand(const char* cmd_line): BuiltInCommand(cmd_line){
	this->numOfArgs = 1;
}

ShowPidCommand::~ShowPidCommand(){
	for (int i = 0; i < COMMAND_MAX_ARGS; i++) {
		free(this->command[i]);
	}
}

void ShowPidCommand::execute(){
	pid_t pid =  getpid();
	std::cout << pid <<std::endl;
}

//--------------------------------
// Get Current Directory Command
//--------------------------------
GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {
	this->numOfArgs = 1;
}

void GetCurrDirCommand::execute(){
	char dirpath[COMMAND_ARGS_MAX_LENGTH];
	getcwd(dirpath,COMMAND_ARGS_MAX_LENGTH);
	std::cout << dirpath<<std::endl;
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
	for (int i = 0; i < COMMAND_MAX_ARGS; i++) {
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

void JobsList::addJob(Command* cmd, bool isStopped){
	time_t t;
	time(&t);
	pid_t p = getpid();
	JobEntry* j = new JobEntry(cmd->getCommand(), p, isStopped, t, cmd->getNumOfArgs());
	(this->jobs).push_back(j);
}

void JobsList::printJobsList(){
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
		if(((int)(this->jobs).size()-1)<jobId ||jobId<0) return nullptr;
		return (this->jobs)[jobId]; 
	 }

//--------------------------------
//Jobs Command
//--------------------------------

JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs):
BuiltInCommand(cmd_line){
	j = jobs;
}

void JobsCommand::execute(){
	j->printJobsList();
}
//--------------------------------
//Kill Command
//--------------------------------

KillCommand::KillCommand(const char* cmd_line, JobsList* jobs):
BuiltInCommand(cmd_line){
	j = jobs;
}

bool isCharNegativeNumber(char* s)
{
	char * t=s; 
	if(*t!='-') return false;
	t++;
    for (; *t != '\0'; t++) {
        if(!(*t>'0' && *t<'9')) return false;
    }

    return true;
}
bool isCharPositiveNumber(char* s)
{
	char * t; 
    for (t=s; *t != '\0'; t++) {
        if(!(*t>'0' && *t<'9')) return false;
    }

    return true;
}

void KillCommand::execute(){
	SmallShell& s = SmallShell::getInstance();
	if(numOfArgs > 3){
		cout << "smash error: kill: invalid arguments" << endl;
		return;
	}
	if(!isCharNegativeNumber(command[1]) || !isCharPositiveNumber(command[2])){
		cout << "smash error: kill: invalid arguments" << endl;
		return;
	}
	int signum=(-1)*(atoi( command[1]));
	int job_id=atoi( command[2]);
	JobsList::JobEntry* j_entry=s.getJobs()->getJobById(job_id);
	if(j_entry==nullptr){
		cout << "smash error: kill: job-id "<<job_id<<" does not exist" << endl;
		return;
	}
	int j_pid=j_entry->getPid();
	cout<<"signal number "<<signum<<" was sent to pid "<<j_pid<<endl;
	if(kill(j_pid,signum) == -1){
		perror("smash error: kill failed");
	}
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
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
	// For example:
	string cmd_s = string(cmd_line);
	if (cmd_s.find("chprompt")==0) {
		return new ChangePromptCommand(cmd_line);
	}
	if (cmd_s.find("showpid") == 0) {
		return new ShowPidCommand(cmd_line);
	}
	if (cmd_s.find("pwd") == 0) {
		return new GetCurrDirCommand(cmd_line);
	}
	if(cmd_s.find("cd") == 0){
		return new ChangeDirCommand(cmd_line, &(this->plastPwd));
	}
	SmallShell& smash = SmallShell::getInstance();
	if(cmd_s.find("jobs") == 0){
		return new JobsCommand(cmd_line, smash.getJobs());
	}
	if(cmd_s.find("kill") == 0){
		return new KillCommand(cmd_line, smash.getJobs());
	}
	if(cmd_s.find("fg") == 0){
		//return new ForegroundCommand(cmd_line, /*TODO: JobsList *jobs  */);
	}
	if(cmd_s.find("bg") == 0){
		//return new BackgroundCommand(cmd_line, /*TODO: JobsList *jobs  */);
	}
	if(cmd_s.find("quit") == 0){
		//return new QuitCommand(cmd_line, /*TODO: JobsList *jobs  */);
	}

  return nullptr;
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

JobsList* SmallShell::getJobs(){
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
   this->jobs->addJob(cmd);
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}
