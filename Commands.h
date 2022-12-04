#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <time.h>
#include <map>
#include <memory>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
 public:
  const std::string original_cmd_line;
  std::string cmd_line;
  char ** args;
  int num_of_args;
  bool is_background;

  Command(const char* cmd_line);
  virtual ~Command();
  virtual void execute() = 0;
  virtual void prepare();
  // virtual void cleanup();
};

class BuiltInCommand : public Command {
 public:
  BuiltInCommand(const char* cmd_line);
  virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
 public:
  bool is_complex;
  ExternalCommand(const char* cmd_line);
  virtual ~ExternalCommand() {}
  void execute() override;
};

class PipeCommand : public Command {
  std::shared_ptr<Command> first_cmd;
  std::shared_ptr<Command> second_cmd;
  bool to_cerr;
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command {
 public:
  std::string cmd;
  std::string output_file;
  bool is_append;
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;
  //void prepare() override;
  //void cleanup() override;
};

class ChangePrompt : public BuiltInCommand {
  public:
    std::string title;
    ChangePrompt(const char* cmd_line);
    virtual ~ChangePrompt() {}
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
  ChangeDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
  virtual ~ChangeDirCommand() {}
  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
  virtual ~GetCurrDirCommand() = default;
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
  ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
  virtual ~ShowPidCommand() = default;
  void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
public:
  JobsList* jobs;
  QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {};
  virtual ~QuitCommand() {}
  void execute() override;
};


class JobsList {
 public:
  class JobEntry {
    public:
      int job_id;
      std::shared_ptr<Command> cmd;
      pid_t pid;
      time_t time_started;
      bool is_stopped;
      JobEntry(int job_id, std::shared_ptr<Command> cmd, int pid, bool is_stopped = false) : job_id(job_id), cmd(cmd), pid(pid), time_started(time(0)), is_stopped(is_stopped) {}
      JobEntry(const JobEntry &job_entry) = default;
      ~JobEntry() = default;
  };
  std::vector<JobEntry> jobs;
  JobsList() = default;
  ~JobsList() = default;
  void addJob(std::shared_ptr<Command> cmd, int pid, bool isStopped = false, int job_id = 0);
  void printJobsList();
  void killAllJobs();
  void removeFinishedJobs();
  JobEntry * getJobById(int jobId);
  void removeJobById(int jobId);
  JobEntry * getLastJob(int* lastJobId);
  JobEntry *getLastStoppedJob(int *jobId);
};

class TimedJobsList {
  std::map<time_t, std::vector<std::shared_ptr<JobsList::JobEntry>>> jobs;
  public:
  TimedJobsList() = default;
  ~TimedJobsList() = default;
  void addTimedJob(time_t end_time, std::shared_ptr<JobsList::JobEntry> job);
  void handleAlarm();
};

class JobsCommand : public BuiltInCommand {
 public:
  JobsList* jobs_list;
  JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {}
  virtual ~JobsCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 public:
  JobsList* jobs_list;
  ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {}
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 public:
  JobsList* jobs_list;
  BackgroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {}
  virtual ~BackgroundCommand() {}
  void execute() override;
};

class TimeoutCommand : public BuiltInCommand {
 public:
  explicit TimeoutCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
  virtual ~TimeoutCommand() {}
  void execute() {}
  void timed_execute(std::shared_ptr<Command> cmd_ptr);
};

class FareCommand : public BuiltInCommand {
 public:
  FareCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
  virtual ~FareCommand() {}
  void execute() override;
  static int ReplaceSubStrings(std::string& str, const std::string& from, const std::string& to);
};

class SetcoreCommand : public BuiltInCommand {
  int cores;
 public:
  SetcoreCommand(const char* cmd_line) : BuiltInCommand(cmd_line) , cores(0) {};
  virtual ~SetcoreCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
 public:
  JobsList* jobs_list;
  KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {}
  virtual ~KillCommand() {}
  void execute() override;
};

class SmallShell {
 private:
  std::string title;
  std::string last_wd;
  SmallShell();
 public:
  JobsList job_list;
  TimedJobsList timed_jobs;
  JobsList::JobEntry* fg_job;
  std::shared_ptr<Command> CreateCommand(const char* cmd_line);
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
  void changeTitle(const std::string& title);
  std::string getTitle() const { return title; }
  std::string getLastWD() const { return last_wd; }
  void setLastWD(char* new_lwd);
};

#endif //SMASH_COMMAND_H_
