#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include  <algorithm>

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
    cmd_line = std::string(temp_cmd_line);
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
    char* ext_cmd[2];
    ext_cmd[0] = strdup("-c");
    ext_cmd[1] = cmd_line_copy;
    execv("/bin/bash", ext_cmd);
  }
  else{
    string command = "/bin/" + string(this->args[0]);
    execv(command.c_str(), args);
  }
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
    perror(errormessage.c_str());
    return;
  }
  if (num_of_args > 2) {
    perror("smash error: cd: too many arguments");
    return;
  }

  char* cwd = getcwd(nullptr, 0);
  SmallShell& smash = SmallShell::getInstance();
  if (strcmp(args[1], "-") == 0) {
    string new_wd = smash.getLastWD();
    if (new_wd.empty()) {
      free(cwd);
      perror("smash error: cd: OLDPWD not set");
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

void JobsList::addJob(Command* cmd, int pid, bool isStopped) {
  int next_id;
  auto it = max_element(jobs.begin(),
                             jobs.end(),
                             [](const JobEntry& a,const JobEntry& b) { return a.job_id < b.job_id; });                 
  if (it == jobs.end()) {
    next_id = 1;
  } else {
      next_id = it->job_id + 1;
  }
  jobs.push_back(JobEntry(next_id, cmd, pid));
}

SmallShell::SmallShell() : title("smash"), job_list(), last_wd() {}

void SmallShell::changeTitle(const string& title) {
  this->title = title;
}

void SmallShell::setLastWD(char* new_lwd) {
  this->last_wd = string(new_lwd);
}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {

  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord.compare("chprompt") == 0) {
    return new ChangePrompt(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if(firstWord.compare("jobs") == 0) {
    cout << "jobs is not implemented" << endl;
  }
  else if(firstWord.compare("fg") == 0) {
    cout << "fg is not implemented" << endl;
  }
  else if(firstWord.compare("bg") == 0) {
    cout << "bg is not implemented" << endl;
  }
  else if(firstWord.compare("quit") == 0) {
    cout << "quit is not implemented" << endl;
  }
  else if(firstWord.compare("kill") == 0) {
    cout << "kill is not implemented" << endl;
  }
  else {
    return new ExternalCommand(cmd_line);
  }
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
  // Command* cmd = CreateCommand(cmd_line);
  // cmd->execute();
  // Please note that you must fork smash process for some commands (e.g., external commands....)
  Command* cmd = CreateCommand(cmd_line);
  if (cmd == nullptr) return;
  ExternalCommand* ext_cmd = dynamic_cast<ExternalCommand*>(cmd);
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
        job_list.addJob(ext_cmd, pid, false);
      }
      else{
        pid_t ret_pid = wait(nullptr);
        cout << ret_pid << endl;
      }
    }
  }
}