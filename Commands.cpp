#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <algorithm>
#include <fcntl.h>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

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

// TODO: Add your implementation for classes in Commands.h 
Command::Command(const char* cmd_line) : original_cmd_line(cmd_line),
  cmd_line(cmd_line), args(), num_of_args(), is_background(false) {
  prepare();
}

Command::~Command() {
  for (int i = 0; i < num_of_args; i++) {
    free(args[i]);
    args[i] = nullptr;
  }
  free(args);
  args = nullptr;
}

void Command::prepare() {
  if (_isBackgroundComamnd(cmd_line.c_str())) {
    is_background = true;
    char temp_cmd_line[COMMAND_ARGS_MAX_LENGTH + 1];
    strcpy(temp_cmd_line, cmd_line.c_str());
    _removeBackgroundSign(temp_cmd_line);
    cmd_line = _trim(std::string(temp_cmd_line));
  }
  args = (char **)malloc(sizeof(char*) * COMMAND_MAX_ARGS);
  num_of_args = _parseCommandLine(cmd_line.c_str(), args);
}

BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line) {
  is_background = false;
}

ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line), is_complex(false) {
  if (this->cmd_line.find('*') != std::string::npos || this->cmd_line.find('?') != std::string::npos) {
    is_complex = true;
  }
}

void ExternalCommand::execute() {
  if (is_complex){
    char cmd_line_copy[201];
    strcpy(cmd_line_copy, cmd_line.c_str());
    char* ext_cmd[4];
    ext_cmd[0] = strdup("bash");
    ext_cmd[1] = strdup("-c");
    ext_cmd[2] = cmd_line_copy;
    ext_cmd[3] = nullptr;
    execvp("/bin/bash", ext_cmd);
  }
  else{
    execvp(this->args[0], args);
    // If failed search in /bin/
    string command = "/bin/" + string(this->args[0]);
    execvp(command.c_str(), args);
  }
  // If failed print error message
  cout << "External command did not execute correctly" << endl;
  exit(0);
}

ChangePrompt::ChangePrompt(const char* cmd_line) : BuiltInCommand(cmd_line), title("smash") {
  if (num_of_args < 2) return;
  title = args[1];
}

void ChangePrompt::execute() {
  SmallShell& smash = SmallShell::getInstance();
  smash.changeTitle(title);
}

void ShowPidCommand::execute() {
  cout << "smash pid is " << getpid() << endl;
}

void GetCurrDirCommand::execute() {
  char* cwd = getcwd(nullptr, 0);
  cout << cwd << endl;
  free(cwd);
}

void ChangeDirCommand::execute() {
  if (num_of_args < 2){
    std::string errormessage = "smash error:> \"";
    errormessage = errormessage + this->original_cmd_line + "\"";
    cout << errormessage << endl;
    return;
  }
  if (num_of_args > 2) {
    cerr << "smash error: cd: too many arguments" << endl;
    return;
  }

  char* cwd = getcwd(nullptr, 0);
  SmallShell& smash = SmallShell::getInstance();
  if (strcmp(args[1], "-") == 0) {
    string new_wd = smash.getLastWD();
    if (new_wd.empty()) {
      free(cwd);
      cerr << "smash error: cd: OLDPWD not set" << endl;
      return;
    }
    if (chdir(new_wd.c_str()) != 0) {
      free(cwd);
      perror("smash error: chdir failed");
      return;
    }
  }
  else if (chdir(args[1]) != 0) {
    free(cwd);
    perror("smash error: chdir failed");
    return;
  }

  smash.setLastWD(cwd);
  free(cwd);
}

void JobsCommand::execute() {
  SmallShell::getInstance().job_list.printJobsList();
}

void ForegroundCommand::execute() {
  if (num_of_args > 2) {
    perror("smash error: fg: invalid arguments");
    return;
  }

  int target_id;
  JobsList::JobEntry* target_job;
  SmallShell& smash = SmallShell::getInstance();
  if (num_of_args == 2) {
    try {
      target_id = stoi(args[1]);
    } catch (...) {
      perror("smash error: fg: invalid arguments");
      return;
    }
    target_job = smash.job_list.getJobById(target_id);
    if (target_job == nullptr) {
      string err = "smash error: fg: job-id " + string(args[1]) + " does not exist";
      perror(err.c_str());
      return;
    }
  } else {
      target_job = smash.job_list.getLastJob(&target_id);
      if (target_job == nullptr) {
        perror("smash error: fg: jobs list is empty");
        return;
      }
  }

  cout << target_job->cmd->original_cmd_line << " : " << target_job->pid << endl;

  smash.fg_job = new JobsList::JobEntry(target_id, target_job->cmd, target_job->pid);
  smash.job_list.removeJobById(target_id);
  kill(smash.fg_job->pid, SIGCONT);
  waitpid(smash.fg_job->pid, nullptr, WUNTRACED);
  delete smash.fg_job;
  smash.fg_job = nullptr;
}

void BackgroundCommand::execute() {
  if (num_of_args > 2) {
    perror("smash error: bg: invalid arguments");
    return;
  }

  int target_id;
  JobsList::JobEntry* target_job;
  SmallShell& smash = SmallShell::getInstance();
  if (num_of_args == 2) {
    try {
      target_id = stoi(args[1]);
    } catch (...) {
      perror("smash error: bg: invalid arguments");
      return;
    }
    target_job = smash.job_list.getJobById(target_id);
    if (target_job == nullptr) {
      string err = "smash error: bg: job-id " + string(args[1]) + " does not exist";
      perror(err.c_str());
      return;
    }
    else if (target_job->is_stopped == 0) {
      string err = "smash error: bg: job-id " + string(args[1]) + " is already running in the background";
      perror(err.c_str());
      return;
    }
  } else {
      target_job = smash.job_list.getLastStoppedJob(&target_id);
      if (target_job == nullptr) {
        perror("smash error: bg: there is no stopped jobs to resume");
        return;
      }
  }

  cout << target_job->cmd->original_cmd_line << " : " << target_job->pid << endl;
  target_job->is_stopped = false;
  kill(target_job->pid, SIGCONT);
}

void QuitCommand::execute() {
  bool is_kill = false;
  for (int i = 1; i < num_of_args; i++) {
    if (strcmp("kill", args[i]) == 0) {
      is_kill = true;
    }
  }
  if (is_kill) {
    SmallShell::getInstance().job_list.killAllJobs();
    SmallShell::getInstance().job_list.removeFinishedJobs();
  } 
  exit(0);
}

PipeCommand::PipeCommand(const char* cmd_line) : Command(cmd_line), to_cerr(true){
  string type = "|&";
  auto i = this->cmd_line.find("|&");
  if (i == string::npos) {
    to_cerr = false;
    type = "|";
    i = this->cmd_line.find("|");
  }
  first_cmd = SmallShell::getInstance().CreateCommand((this->cmd_line.substr(0, i)).c_str());
  second_cmd = SmallShell::getInstance().CreateCommand(this->cmd_line.substr(i + type.length() , string::npos).c_str());
} 

void PipeCommand::execute(){
  int new_pipe[2];
  int success = pipe(new_pipe);
  if(success != 0){
    cout << "smash error:> " + this->original_cmd_line << endl;
    return;
  }

  if(fork() == 0){
    if(to_cerr){
      dup2(new_pipe[1], 2);
      close(new_pipe[0]);
      close(new_pipe[1]);
    }
    else{
      dup2(new_pipe[1], 1);
      close(new_pipe[0]);
      close(new_pipe[1]);
    }
    this->first_cmd->execute();
    exit(0);
  }
  if(fork() == 0){
    dup2(new_pipe[0], 0);
    close(new_pipe[0]);
    close(new_pipe[1]);
    this->second_cmd->execute();
    exit(0);
  }
  close(new_pipe[0]);
  close(new_pipe[1]);
}

RedirectionCommand::RedirectionCommand(const char* cmd_line) : 
  Command(cmd_line) , output_file(), is_append(true) {
  string type = ">>";
  auto i = this->cmd_line.find(">>");
  if (i == string::npos) {
    is_append = false;
    type = ">";
    i = this->cmd_line.find(">");
  }
  cmd = SmallShell::getInstance().CreateCommand((this->cmd_line.substr(0, i)).c_str());
  output_file = _trim(this->cmd_line.substr(i + type.length() , string::npos));
} 

void RedirectionCommand::execute(){
  int flag = O_CREAT | O_TRUNC | O_RDWR;
  if(this->is_append){
    flag = O_CREAT | O_APPEND | O_RDWR;
  }
  pid_t pid = fork();
  if(pid == 0){
    close(1);
    int fd = open(this->output_file.c_str(), flag, S_IRWXU | S_IRWXO | S_IRWXG);
    this->cmd->execute();
    close(fd);
    exit(0);
  }
  else{
    waitpid(pid, nullptr, 0);
  }
}

void JobsList::addJob(std::shared_ptr<Command> cmd, int pid, bool isStopped, int job_id) {
  int next_id;
  auto it = max_element(jobs.begin(),
                             jobs.end(),
                             [](const JobEntry& a,const JobEntry& b) { return a.job_id < b.job_id; });                 
  if (it == jobs.end()) {
    next_id = 1;
  } else {
      next_id = it->job_id + 1;
  }
  next_id = job_id == 0 ? next_id : job_id;
  jobs.push_back(JobEntry(next_id, cmd, pid, isStopped));
}

void JobsList::printJobsList() {
  std::sort(jobs.begin(), jobs.end(), 
      [](const JobEntry& a,const JobEntry& b) { return a.job_id < b.job_id; });

  auto job = jobs.begin();
  while (job != jobs.end()) {
    string job_print = "[" + to_string(job->job_id) + "] ";
    job_print += job->cmd->original_cmd_line + " : ";
    job_print += to_string(job->pid) + " ";
    string stime = to_string(int(difftime(time(0), job->time_started)));
    job_print +=  stime + " secs";
    if (job->is_stopped)
      job_print += " (stopped)";
    cout << job_print << endl;
    job++;
  }
}

void JobsList::removeFinishedJobs() {
  auto job = jobs.begin();
  while (job != jobs.end()) {
    if (waitpid(job->pid, nullptr, WNOHANG) > 0) {
      job = jobs.erase(job);
    } else {
      job++;
    }
  }
}

void JobsList::killAllJobs() {
  std::sort(jobs.begin(), jobs.end(), 
      [](const JobEntry& a,const JobEntry& b) { return a.job_id < b.job_id; });
    int size = jobs.size();
    cout << "smash: sending SIGKILL signal to " << size << " jobs:" << endl;
    for (int i = 0; i < size; i++) {
      cout << jobs[i].pid << ": " << jobs[i].cmd->original_cmd_line << endl;
      kill(jobs[i].pid, 9);
    }
}

JobsList::JobEntry* JobsList::getJobById(int jobId) {
  for (size_t i = 0; i < jobs.size(); i++) {
    if (jobs[i].job_id == jobId) {
      return &jobs[i];
    }
  }
  return nullptr;
}

void JobsList::removeJobById(int jobId) {
  auto job = jobs.begin();
  while (job != jobs.end()) {
    if (job->job_id == jobId) {
      jobs.erase(job);
      return;
    }
    job++;
  }
}

JobsList::JobEntry* JobsList::getLastJob(int* lastJobId) {
  auto it = max_element(jobs.begin(),
                             jobs.end(),
                             [](const JobEntry& a,const JobEntry& b) { return a.job_id < b.job_id; });
  if (it == jobs.end()) {
    *lastJobId = -1;
    return nullptr;
  }
  *lastJobId = it->job_id;
  return &(*it);
}

JobsList::JobEntry* JobsList::getLastStoppedJob(int* lastJobId) {
  vector<JobEntry> stopped_jobs;
  auto job = jobs.begin();
  while (job != jobs.end()) {
    if (job->is_stopped) {
      stopped_jobs.push_back(*job);
    }
    job++;
  }

  auto it = max_element(stopped_jobs.begin(),
                             stopped_jobs.end(),
                             [](const JobEntry& a,const JobEntry& b) { return a.job_id < b.job_id; });
  if (it == stopped_jobs.end()) {
    *lastJobId = -1;
    return nullptr;
  }
  *lastJobId = it->job_id;
  return getJobById(it->job_id);
}

SmallShell::SmallShell() : title("smash"), last_wd(), job_list(), fg_job() {}

void SmallShell::changeTitle(const string& title) {
  this->title = title;
}

void SmallShell::setLastWD(char* new_lwd) {
  this->last_wd = string(new_lwd);
}

SmallShell::~SmallShell() {
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
shared_ptr<Command> SmallShell::CreateCommand(const char* cmd_line) {

  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (cmd_s.find(">>") != string::npos || cmd_s.find(">") != string::npos) {
    return shared_ptr<RedirectionCommand>(new RedirectionCommand(cmd_line));
  }
  else if (cmd_s.find("|&") != string::npos || cmd_s.find("|") != string::npos) {
    return shared_ptr<PipeCommand>(new PipeCommand(cmd_line));
  }
  else if (firstWord.compare("chprompt") == 0) {
    return shared_ptr<ChangePrompt>(new ChangePrompt(cmd_line));
  }
  else if (firstWord.compare("showpid") == 0) {
    return shared_ptr<ShowPidCommand>(new ShowPidCommand(cmd_line));
  }
  else if (firstWord.compare("pwd") == 0) {
    return shared_ptr<GetCurrDirCommand>(new GetCurrDirCommand(cmd_line));
  }
  else if(firstWord.compare("jobs") == 0) {
    return shared_ptr<JobsCommand>(new JobsCommand(cmd_line, &job_list));
  }
  else if(firstWord.compare("fg") == 0) {
    return shared_ptr<ForegroundCommand>(new ForegroundCommand(cmd_line, &job_list));
  }
  else if(firstWord.compare("bg") == 0) {
    return shared_ptr<BackgroundCommand>(new BackgroundCommand(cmd_line, &job_list));
  }
  else if(firstWord.compare("quit") == 0) {
    return shared_ptr<QuitCommand>(new QuitCommand(cmd_line, &job_list));
  }
  else if(firstWord.compare("kill") == 0) {
    cout << "kill is not implemented" << endl;
  }
  else {
    return shared_ptr<ExternalCommand>(new ExternalCommand(cmd_line));
  }
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
  // Command* cmd = CreateCommand(cmd_line);
  // cmd->execute();
  // Please note that you must fork smash process for some commands (e.g., external commands....)
  shared_ptr<Command> cmd = CreateCommand(cmd_line);
  if (cmd == nullptr) return;
  ExternalCommand* ext_cmd = dynamic_cast<ExternalCommand*>(cmd.get());
  if (!ext_cmd){
    cmd->execute();
    return;
  }
  else{
    pid_t pid = fork();
    if(pid == 0){
      ext_cmd->execute();
    }
    else{
      if (ext_cmd->is_background){
        getInstance().job_list.removeFinishedJobs();
        job_list.addJob(cmd, pid, false);
      }
      else{
        fg_job = new JobsList::JobEntry(0, cmd, pid);
        waitpid(pid, nullptr, WUNTRACED);
        delete fg_job;
        fg_job = nullptr;
      }
    }
  }
}