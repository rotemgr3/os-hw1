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
#include <fstream>
#include <sched.h>
#include <sys/sysinfo.h>


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
  perror("smash error: execvp failed");
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
  if(cwd == 0){
    perror("smash error: getcwd failed");
    return;
  }
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
    cerr << "smash error: fg: invalid arguments" << endl;
    return;
  }

  int target_id;
  JobsList::JobEntry* target_job;
  SmallShell& smash = SmallShell::getInstance();
  if (num_of_args == 2) {
    try {
      target_id = stoi(args[1]);
    } catch (...) {
      cerr << "smash error: fg: invalid arguments" << endl;
      return;
    }
    target_job = smash.job_list.getJobById(target_id);
    if (target_job == nullptr) {
      string err = "smash error: fg: job-id " + string(args[1]) + " does not exist";
      cerr << err.c_str() << endl;
      return;
    }
  } else {
      target_job = smash.job_list.getLastJob(&target_id);
      if (target_job == nullptr) {
        cerr << "smash error: fg: jobs list is empty" << endl;
        return;
      }
  }

  cout << target_job->cmd->original_cmd_line << " : " << target_job->pid << endl;

  smash.fg_job = new JobsList::JobEntry(target_id, target_job->cmd, target_job->pid);
  smash.job_list.removeJobById(target_id);
  if(kill(smash.fg_job->pid, SIGCONT) == -1){
    perror("smash error: kill failed");
  }
  waitpid(smash.fg_job->pid, nullptr, WUNTRACED);
  delete smash.fg_job;
  smash.fg_job = nullptr;
}

void FareCommand::execute(){
  if (num_of_args != 4){
    cerr << "smash error: fare: invalid arguments" << endl;
    return;
  }
  
  string file_name = args[1];
  string src = args[2];
  string dst = args[3];

  FILE *file = fopen(file_name.c_str(), "r+");
  if (file == NULL){
    perror("smash error: open failed");
    return;
  } 
  fclose(file);

  std::stringstream buffer;
  try{
    std::ifstream in(file_name);
    buffer << in.rdbuf();
    in.close();
  }
  catch(...){
    perror("smash error: open failed");
    return;
  }

  string file_content = buffer.str();
  int times = ReplaceSubStrings(file_content, src, dst);

  try{
    std::ofstream out(file_name, std::ofstream::trunc);
    out << file_content;
    out.close();
  }
  catch(...){
    perror("smash error: open failed");
    return;
  }

  cout << "replaced " <<  times << " instances of the string \"" << src << "\"" << endl;
}

int FareCommand::ReplaceSubStrings(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    int counter = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
        counter++;
    }
    return counter;
}

void KillCommand::execute() {
  if (num_of_args != 3){
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  }
  int signum;
  int job_id;
  try {
    signum = -1 * stoi(args[1]);
    job_id = stoi(args[2]);
  } catch (...) {
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  }
  if (!(signum >= 1 && signum <= 31)) {
    cerr << "smash error: kill: invalid arguments" << endl;
    return;
  }
  JobsList::JobEntry* job = jobs_list->getJobById(job_id);
  if (job == nullptr) {
    cerr << "smash error: kill: job-id " << job_id << " does not exist" << endl;
    return;
  }
  int pid = job->pid;
  switch (signum) {
    case SIGCONT:
      if (kill(pid, SIGCONT) == -1) {
        perror("smash error: kill failed");
        return;
      }
      job->is_stopped = false;
      break;
    case SIGSTOP:
      if (kill(pid, SIGSTOP) == -1) {
        perror("smash error: kill failed");
        return;
      }
      job->is_stopped = true;
      break;
    case SIGKILL:
      if (kill(pid, SIGKILL) == -1) {
        perror("smash error: kill failed");
        return;
      }
      break;
    case SIGTERM:
      if (kill(pid, SIGTERM) == -1) {
        perror("smash error: kill failed");
        return;
      }
      break;
    default:
      if (kill(pid, signum) == -1) {
        perror("smash error: kill failed");
        return;
      }
  }
  cout << "signal number " << signum << " was sent to pid " << pid << endl;
}

void BackgroundCommand::execute() {
  if (num_of_args > 2) {
    cerr << "smash error: bg: invalid arguments" << endl;
    return;
  }

  int target_id;
  JobsList::JobEntry* target_job;
  SmallShell& smash = SmallShell::getInstance();
  if (num_of_args == 2) {
    try {
      target_id = stoi(args[1]);
    } catch (...) {
      cerr << "smash error: bg: invalid arguments" << endl;
      return;
    }
    target_job = smash.job_list.getJobById(target_id);
    if (target_job == nullptr) {
      string err = "smash error: bg: job-id " + string(args[1]) + " does not exist";
      cerr << err.c_str() << endl;
      return;
    }
    else if (target_job->is_stopped == 0) {
      string err = "smash error: bg: job-id " + string(args[1]) + " is already running in the background";
      cerr << err.c_str() << endl;
      return;
    }
  } else {
      target_job = smash.job_list.getLastStoppedJob(&target_id);
      if (target_job == nullptr) {
        cerr << "smash error: bg: there is no stopped jobs to resume" << endl;
        return;
      }
  }

  cout << target_job->cmd->original_cmd_line << " : " << target_job->pid << endl;
  target_job->is_stopped = false;
  if(kill(target_job->pid, SIGCONT) == -1){
    perror("smash error: kill failed");
  }
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
    cout << "smash error:> \"" + this->original_cmd_line << "\"" << endl;
    return;
  }

  pid_t pid1 = fork();
  if(pid1 == -1){
    perror("smash error: fork failed");
    close(new_pipe[0]);
    close(new_pipe[1]);
    return;
  }
  else if(pid1 == 0){
    setpgrp();
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

  pid_t pid2 = fork();
  if(pid2 == -1){
    perror("smash error: fork failed");
    close(new_pipe[0]);
    close(new_pipe[1]);
    return;
  }
  else if(pid2 == 0){
    setpgrp();
    dup2(new_pipe[0], 0);
    close(new_pipe[0]);
    close(new_pipe[1]);
    this->second_cmd->execute();
    exit(0);
  }
  close(new_pipe[0]);
  close(new_pipe[1]);

  waitpid(pid1, nullptr, 0);
  waitpid(pid2, nullptr, 0);
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
  char temp_cmd_line[COMMAND_ARGS_MAX_LENGTH + 1];
  strcpy(temp_cmd_line, this->cmd_line.substr(0, i).c_str());
  _removeBackgroundSign(temp_cmd_line);
  cmd = temp_cmd_line;
  output_file = _trim(this->cmd_line.substr(i + type.length() , string::npos));
} 

void RedirectionCommand::execute(){
  int flag = O_CREAT | O_TRUNC | O_RDWR;
  if(this->is_append){
    flag = O_CREAT | O_APPEND | O_RDWR;
  }
	int old_stdout = dup(1);
	close(1);
  int fd = open(this->output_file.c_str(), flag, S_IRWXU | S_IRWXO | S_IRWXG);
  if(fd == -1){
    perror("smash error: open failed");
    dup2(old_stdout, 1);
    close(old_stdout);
    return;
  }
	SmallShell::getInstance().executeCommand(cmd.c_str());
  close(fd);
	dup2(old_stdout, 1);
  close(old_stdout);
}

void SetcoreCommand::execute(){
  if (num_of_args != 3){
    cerr << "smash error: setcore: invalid arguments" << endl;
    return;
  }
  int corenum;
  int job_id;
  pid_t pid;
  try {
    corenum = stoi(args[2]);
    job_id = stoi(args[1]);
  } catch (...) {
    cerr << "smash error: setcore: invalid arguments" << endl;
    return;
  }
  JobsList::JobEntry* job = SmallShell::getInstance().job_list.getJobById(job_id);
  if(job == nullptr) {
    cerr << "smash error: setcore: job-id " << job_id << " does not exist" << endl;
    return;
  }
  pid = job->pid;
  if(corenum < 0 || corenum >= get_nprocs_conf()){
    cerr << "smash error: setcore: invalid core number" << endl;
    return;
  }
  cpu_set_t my_set;        /* Define your cpu_set bit mask. */
  CPU_ZERO(&my_set);       /* Initialize it all to 0, i.e. no CPUs selected. */
  CPU_SET(corenum, &my_set);     /* set the bit that represents core corenum. */
  /* Set affinity of pid  to the defined mask, i.e. only corenum. */
  if (sched_setaffinity(pid, sizeof(cpu_set_t), &my_set) == -1) {
    perror("smash error: sched_setaffinity failed");
  }
}

void TimeoutCommand::timed_execute(shared_ptr<Command> cmd_ptr) {
  if (num_of_args <= 2) {
    // cout << "smash error:> \"" + this->original_cmd_line << "\"" << endl;
    cerr << "smash error: " << "timeout: "<< "invalid arguments" << endl;
    return;
  }
  int secs;
  try {
      secs = stoi(args[1]);
  } catch (...) {
      // cout << "smash error:> \"" + this->original_cmd_line << "\"" << endl;
      cerr << "smash error: " << "timeout: "<< "invalid arguments" << endl;
      return;
  }
  if (secs <= 0 ) {
    // cout << "smash error:> \"" + this->original_cmd_line << "\"" << endl;
    cerr << "smash error: " << "timeout: "<< "invalid arguments" << endl;
    return;
  }
  SmallShell& smash = SmallShell::getInstance();

  auto sec_pos = this->cmd_line.find(args[1]);
  std::shared_ptr<Command> internal_cmd = smash.CreateCommand(
    this->original_cmd_line.substr(sec_pos + string(args[1]).length() , string::npos).c_str());
  
  // Execution
  pid_t pid = fork();
  if(pid == -1){
    perror("smash error: fork failed");
    return;
  }
  else if(pid == 0){
    setpgrp();
    internal_cmd->execute();
  }
  else{
    shared_ptr<JobsList::JobEntry> timed_job(new JobsList::JobEntry(0, cmd_ptr, pid));
    smash.timed_jobs.addTimedJob(time(0) + secs, timed_job);

    if (internal_cmd->is_background) {
      smash.job_list.removeFinishedJobs();
      smash.job_list.addJob(cmd_ptr, pid, false);
    }
    else{
      smash.fg_job = new JobsList::JobEntry(0, cmd_ptr, pid);
      waitpid(pid, nullptr, WUNTRACED);
      delete smash.fg_job;
      smash.fg_job = nullptr;
    }
  }
}

void TimedJobsList::addTimedJob(time_t end_time, shared_ptr<JobsList::JobEntry> job) {
  if (jobs.find(end_time) != jobs.end()) {
      jobs[end_time].push_back(job);
  }
  else {
      jobs.insert({end_time, {job}});
  }

  // Create alarm for first object
  auto it = jobs.begin();
  if (it != jobs.end()) {
    alarm(it->first - time(0));
  }
}

void TimedJobsList::handleAlarm() {
  time_t curr = time(0);
  auto jobs_vec = jobs.find(curr);
  if (jobs_vec != jobs.end())
  {
    auto job = jobs_vec->second.begin();
    while (job != jobs_vec->second.end()) {
      if (waitpid((*job)->pid, nullptr, WNOHANG) == 0) {
        if (kill((*job)->pid, SIGKILL) == -1) {
          perror("smash error: kill failed");
        } 
        cout << "smash: " << (*job)->cmd->original_cmd_line << " timed out!" << endl;
      }
      job++;
    }
    jobs_vec->second.clear();
    jobs.erase(jobs_vec);

    // Restore alarm for next job in line
    auto it = jobs.begin();
    if (it != jobs.end()) {
      alarm(it->first - time(0));
    }
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
    if (waitpid(job->pid, nullptr, WNOHANG) != 0) {
      job = jobs.erase(job);
    } else {
      job++;
    }
  }
}

void JobsList::killAllJobs() {
  removeFinishedJobs();
  std::sort(jobs.begin(), jobs.end(), 
      [](const JobEntry& a,const JobEntry& b) { return a.job_id < b.job_id; });
    int size = jobs.size();
    cout << "smash: sending SIGKILL signal to " << size << " jobs:" << endl;
    for (int i = 0; i < size; i++) {
      cout << jobs[i].pid << ": " << jobs[i].cmd->original_cmd_line << endl; 
      if (kill(jobs[i].pid, 9) == -1){
        perror("smash error: kill failed");
      }
    }
}

JobsList::JobEntry* JobsList::getJobById(int jobId) {
  removeFinishedJobs();
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
  removeFinishedJobs();
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
  removeFinishedJobs();
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

SmallShell::SmallShell() : title("smash"), last_wd(), job_list(), timed_jobs(), fg_job() {}

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
  else if (firstWord.compare("timeout") == 0) {
    return shared_ptr<TimeoutCommand>(new TimeoutCommand(cmd_line));
  }
  else if (firstWord.compare("setcore") == 0) {
    return shared_ptr<SetcoreCommand>(new SetcoreCommand(cmd_line));
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
  else if (firstWord.compare("cd") == 0) {
    return shared_ptr<ChangeDirCommand>(new ChangeDirCommand(cmd_line));
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
   return shared_ptr<KillCommand>(new KillCommand(cmd_line, &job_list));
  }
  else if(firstWord.compare("fare") == 0) {
    return shared_ptr<FareCommand>(new FareCommand(cmd_line));
  }
  else {
    return shared_ptr<ExternalCommand>(new ExternalCommand(cmd_line));
  }
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  shared_ptr<Command> cmd = CreateCommand(cmd_line);
  if (cmd == nullptr) return;
  ExternalCommand* ext_cmd = dynamic_cast<ExternalCommand*>(cmd.get());
  TimeoutCommand* timed_cmd = dynamic_cast<TimeoutCommand*>(cmd.get());
  if (timed_cmd){
    timed_cmd->timed_execute(cmd);
    return;
  }
  if (!ext_cmd){
    cmd->execute();
    return;
  }
  else{
    pid_t pid = fork();
    if(pid == -1){
      perror("smash error: fork failed");
      return;
    }
    else if(pid == 0){
      setpgrp();
      ext_cmd->execute();
    }
    else{
      if (ext_cmd->is_background){
        job_list.removeFinishedJobs();
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