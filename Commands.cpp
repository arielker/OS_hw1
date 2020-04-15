#include <unistd.h>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <signal.h>

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

Command::Command(const char* cmd_line) : is_background(false) {
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
			int result = execv(this->bin_bash, this->external_args);
			if(result == -1){
				perror("smash error: execv failed");
			}
		} else if (pid > 0) {
			wait(nullptr);
		} else{
			perror("smash error: fork failed");
		}
	} else {
		pid_t pid = fork();
		smash.getJobs()->addJob(this,pid);
		if (pid == 0) {
			int result = execv(this->bin_bash, this->external_args);
			if(result == -1){
				perror("smash error: execv failed");
			}
		} else if (pid < 0){
			perror("smash error: fork failed");
		}
	}
}

//--------------------------------
// Change Prompt
//--------------------------------

ChangePromptCommand::ChangePromptCommand(const char* cmd_line) :
BuiltInCommand(cmd_line) {
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

ShowPidCommand::ShowPidCommand(const char* cmd_line) :
BuiltInCommand(cmd_line){
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
GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) :
BuiltInCommand(cmd_line) {
	this->numOfArgs = 1;
}

void GetCurrDirCommand::execute(){
	char dirpath[COMMAND_ARGS_MAX_LENGTH];
	if(nullptr == getcwd(dirpath,COMMAND_ARGS_MAX_LENGTH)){
		perror("smash error: getcwd failed");
		return;
	}
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

void JobsList::addJob(Command* cmd,pid_t pid, bool isStopped){
	time_t t;
	time(&t);
	pid_t p = pid;
	JobEntry* j = new JobEntry(cmd->getCommand(), p, isStopped, t, cmd->getNumOfArgs());
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
		if(((int)(this->jobs).size()) < jobId || jobId < 0){
			 return nullptr;
		}
		return (this->jobs)[jobId - 1]; 
}

void JobsList::removeJobById(int jobId){
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
	int numOfJobs=this->getJobs().size();
	cout<<"smash: sending SIGKILL signal to "<<numOfJobs<<" jobs:"<<endl;
	for (vector<JobEntry*>::iterator it = jobs.begin() ; it != jobs.end(); ++it)
    {
		int pid= (*it)->getPid();
		cout<<pid<<":";
		(*it)->printArgs((*it)->getJob(), (*it)->getNumOfArgs());
		cout<<endl;
		kill((*it)->getPid(),SIGKILL);
	}
}

void JobsList::removeFinishedJobs(){
	int i = 1;
	for(JobEntry* j :this->jobs){
		if(kill(j->getPid(), 0) == -1 || waitpid(j->getPid(), nullptr, WNOHANG)) {
			removeJobById(i);
			continue;
		}
		i++;
	}
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

JobsList::JobEntry* JobsList::getLastJob(int* last_job_id){
	int size = this->jobs.size();
	*last_job_id = size - 1;
	return this->jobs[size - 1];
}

//--------------------------------
//Kill Command
//--------------------------------

KillCommand::KillCommand(const char* cmd_line, JobsList* jobs):
BuiltInCommand(cmd_line){
	j = jobs;
}

/*bool isCharNegativeNumber(char* s) {
	char * t=s; 
	if(*t!='-') {
		 return false;
	}
	t++;
    for (; *t != '\0'; t++) {
        if(!(*t>'0' && *t<'9')) {
			return false;
		}
	}
    return true;
}


bool isCharPositiveNumber(char* s) {
	char * t; 
    for (t=s; *t != '\0'; t++) {
        if(!(*t>'0' && *t<'9')){
			 return false;
		}
	}
    return true;
}*/

void KillCommand::execute(){
	SmallShell& s = SmallShell::getInstance();
	if(numOfArgs != 3){
		cout << "smash error: kill: invalid arguments" << endl;
		return;
	}
	int signum = (-1)*(atoi(command[1]));
	int job_id = atoi(command[2]);
	//if(!isCharNegativeNumber(command[1]) || !isCharPositiveNumber(command[2])){
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

void ForegroundCommand::execute(){
	if((this->j)->getJobs().empty() && this->numOfArgs == 1){
		cout<< "smash error: fg: jobs list is empty" << endl;
		return;
	}
	int temp = atoi(this->command[1]);
	if((this->numOfArgs > 2) || (this->numOfArgs == 2 && temp <= 0)){
		cout<< "smash error: fg: invalid arguments" <<endl;
		return;
	}
	int last_job_id = 0;
	if(this->numOfArgs == 2){
		int job_id = atoi(this->command[1]);
		SmallShell& s = SmallShell::getInstance();
		JobsList::JobEntry* j_entry = s.getJobs()->getJobById(job_id);
		if(j_entry == nullptr){
			cout << "smash error: fg: job-id "<<job_id<<" does not exist" << endl;
			return;
		}
		pid_t pid = j_entry->getPid();
		j_entry->printArgs((j_entry->getJob()), j_entry->getNumOfArgs());
		cout << " : " << pid << endl;
		this->j->removeJobById(job_id - 1);
		kill(pid, SIGCONT);
		int wstatus = 0;
		pid_t result = waitpid(pid, &wstatus, WCONTINUED); //TODO: parse if we got Ctrl + Z
		if(result != pid){
			perror("smash error: waitpid failed");
			return;
		}
	} else if (this->numOfArgs == 1) {
		JobsList::JobEntry* j_entry = this->j->getLastJob(&last_job_id);
		j_entry->printArgs((j_entry->getJob()), j_entry->getNumOfArgs());
		cout << " : " << (j_entry->getPid()) << endl;
		this->j->removeJobById(last_job_id - 1);
		kill(j_entry->getPid(), SIGCONT);
		int wstatus = 0;
		pid_t result = waitpid(j_entry->getPid(), &wstatus, WCONTINUED); //TODO: parse if we got Ctrl + Z
		if(result != j_entry->getPid()) {
			perror("smash error: waitpid failed");
		}
	}
}

//--------------------------------
//Background Command
//--------------------------------

BackgroundCommand::BackgroundCommand(const char* cmd_line, JobsList* jobs):
BuiltInCommand(cmd_line){
	this->j = jobs;
}

void BackgroundCommand::execute(){
	if(this->numOfArgs > 2){
		cout << "smash error: bg: invalid arguments" <<endl;
		return;
	}
	int jobId = atoi(this->command[1]);
	if(this->numOfArgs == 2 && jobId <= 0){
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
		j_entry->printArgs((j_entry->getJob()), j_entry->getNumOfArgs());
		cout << " : " << pid << endl;
		j_entry->setIsStopped(false);
		kill(pid, SIGCONT);
	} else {
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
		j_entry->printArgs((j_entry->getJob()), j_entry->getNumOfArgs());
		cout << " : " << pid << endl;
		j_entry->setIsStopped(false);
		kill(pid, SIGCONT);
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
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
	// For example:
	char* temp[COMMAND_MAX_ARGS] = {0};
	int n = _parseCommandLine(cmd_line, temp);
	if (strcmp(temp[0], "chprompt") == 0) {
		destroyTemp(temp, n);
		return new ChangePromptCommand(cmd_line);
	}
	if (strcmp(temp[0], "showpid") == 0) {
		destroyTemp(temp, n);
		return new ShowPidCommand(cmd_line);
	}
	if (strcmp(temp[0], "pwd") == 0) {
		destroyTemp(temp, n);
		return new GetCurrDirCommand(cmd_line);
	}
	if(strcmp(temp[0], "cd") == 0){
		destroyTemp(temp, n);
		return new ChangeDirCommand(cmd_line, &(this->plastPwd));
	}
	SmallShell& smash = SmallShell::getInstance();
	if(strcmp(temp[0], "jobs") == 0){
		destroyTemp(temp, n);
		return new JobsCommand(cmd_line, smash.getJobs());
	}
	if(strcmp(temp[0], "kill") == 0){
		destroyTemp(temp, n);
		return new KillCommand(cmd_line, smash.getJobs());
	}
	if(strcmp(temp[0], "fg") == 0){
		destroyTemp(temp, n);
		return new ForegroundCommand(cmd_line, smash.getJobs());
	}
	if(strcmp(temp[0], "bg") == 0){
		destroyTemp(temp, n);
		return new BackgroundCommand(cmd_line, smash.getJobs());
	}
	if(strcmp(temp[0], "quit") == 0){
		destroyTemp(temp, n);
		return new QuitCommand(cmd_line, smash.getJobs());
	}
	return new ExternalCommand(cmd_line);
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
	// Please note that you must fork smash process for some commands (e.g., external commands....)
}
