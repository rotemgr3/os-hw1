#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
  cout << "smash: got ctrl-Z" << endl;
  JobsList::JobEntry* fg_job = SmallShell::getInstance().fg_job;
  if (fg_job == nullptr) return;
  kill(fg_job->pid, SIGSTOP);
  SmallShell::getInstance().job_list.addJob(fg_job->cmd, fg_job->pid, true, fg_job->job_id);
  cout << "smash: process " + to_string(fg_job->pid) + " was stopped" << endl;
}

void ctrlCHandler(int sig_num) {
  cout << "smash: got ctrl-C" << endl;
  JobsList::JobEntry* fg_job = SmallShell::getInstance().fg_job;
  if (fg_job == nullptr) return;
  kill(fg_job->pid, SIGKILL);
  cout << "smash: process " + to_string(fg_job->pid) + " was killed" << endl;
}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}

