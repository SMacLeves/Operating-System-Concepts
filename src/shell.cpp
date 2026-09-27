/**
	* Shell framework
	* course Operating Systems
	* Radboud University
	* v22.09.05

	Student names:
	- ...
	- ...
*/

/**
 * Hint: in most IDEs (Visual Studio Code, Qt Creator, neovim) you can:
 * - Control-click on a function name to go to the definition
 * - Ctrl-space to auto complete functions and variables
 */

// function/class definitions you are going to use
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vector>
#include <list>
#include <optional>

// although it is good habit, you don't have to type 'std' before many objects by including this line
using namespace std;

struct Command {
  vector<string> parts = {};
};

struct Expression {
  vector<Command> commands;
  string inputFromFile;
  string outputToFile;
  bool background = false;
};

// Parses a string to form a vector of arguments. The separator is a space char (' ').
vector<string> split_string(const string& str, char delimiter = ' ') {
  vector<string> retval;
  for (size_t pos = 0; pos < str.length(); ) {
    // look for the next space
    size_t found = str.find(delimiter, pos);
    // if no space was found, this is the last word
    if (found == string::npos) {
      retval.push_back(str.substr(pos));
      break;
    }
    // filter out consequetive spaces
    if (found != pos)
      retval.push_back(str.substr(pos, found-pos));
    pos = found+1;
  }
  return retval;
}

// wrapper around the C execvp so it can be called with C++ strings (easier to work with)
// always start with the command itself
// DO NOT CHANGE THIS FUNCTION UNDER ANY CIRCUMSTANCE
int execvp(const vector<string>& args) {
  // build argument list
  const char** c_args = new const char*[args.size()+1];
  for (size_t i = 0; i < args.size(); ++i) {
    c_args[i] = args[i].c_str();
  }
  c_args[args.size()] = nullptr;
  // replace current process with new process as specified
  int rc = ::execvp(c_args[0], const_cast<char**>(c_args));
  // if we got this far, there must be an error
  int error = errno;
  // in case of failure, clean up memory (this won't overwrite errno normally, but let's be sure)
  delete[] c_args;
  errno = error;
  return rc;
}

// Executes a command with arguments. In case of failure, returns error code.
int execute_command(const Command& cmd) {
  auto& parts = cmd.parts;
  if (parts.size() == 0)
    return EINVAL;

  // execute external commands
  int retval = execvp(parts);
  return retval ? errno : 0;
}

void display_prompt() {
  char buffer[512];
  char* dir = getcwd(buffer, sizeof(buffer));
  if (dir) {
    cout << "\e[32m" << dir << "\e[39m"; // the strings starting with '\e' are escape codes, that the terminal application interpets in this case as "set color to green"/"set color to default"
  }
  cout << "$ ";
  flush(cout);
}

string request_command_line(bool showPrompt) {
  if (showPrompt) {
    display_prompt();
  }
  string retval;
  getline(cin, retval);
  return retval;
}

// note: For such a simple shell, there is little need for a full-blown parser (as in an LL or LR capable parser).
// Here, the user input can be parsed using the following approach.
// First, divide the input into the distinct commands (as they can be chained, separated by `|`).
// Next, these commands are parsed separately. The first command is checked for the `<` operator, and the last command for the `>` operator.
Expression parse_command_line(string commandLine) {
  Expression expression;
  vector<string> commands = split_string(commandLine, '|');
  for (size_t i = 0; i < commands.size(); ++i) {
    string& line = commands[i];
    vector<string> args = split_string(line, ' ');
    if (i == commands.size() - 1 && args.size() > 1 && args[args.size()-1] == "&") {
      expression.background = true;
      args.resize(args.size()-1);
    }
    if (i == commands.size() - 1 && args.size() > 2 && args[args.size()-2] == ">") {
      expression.outputToFile = args[args.size()-1];
      args.resize(args.size()-2);
    }
    if (i == 0 && args.size() > 2 && args[args.size()-2] == "<") {
      expression.inputFromFile = args[args.size()-1];
      args.resize(args.size()-2);
    }
    expression.commands.push_back({args});
  }
  return expression;
}

int execute_expression(Expression& expression) {
  // MOD: empty or blank command line -> do nothing
  if (expression.commands.size() == 0)
    return EINVAL;
  if (expression.commands.empty() || (expression.commands.size() == 1 && expression.commands[0].parts.empty()))
    return 0;
  // MOD: single command
  if (expression.commands.size() == 1) {
    // MOD: ref to the vector of strings that holds command and arguments
    const auto& parts = expression.commands[0].parts;
    if (!parts.empty()) {
      // MOD: terminate the shell process if command empty (e.g. User hit Enter)
      if (parts[0] == "exit") {
        exit(0);
      }
      // MOD: cd - change directory in the parent
      if (parts[0] == "cd") {
        if (parts.size() != 2) {
          // MOD: if user enters more than one arguments throw an error (cd has one only)
          cerr << "cd: expected exactly one argument\n";
          return 0;
        }
        // MOD: in case chdir fails (does not return 0), throw perror
        if (chdir(parts[1].c_str()) != 0) {
          perror("cd");
        }
        return 0;
      }
    }
  }
  // MOD: set the amount of commands to N
  int N = static_cast<int>(expression.commands.size());
  // MOD: prepare forking child processes
  vector<pid_t> pids;
  // MOD: preallocate space for N elements - not required, but nice optimization
  pids.reserve(N);
  // MOD: read end for pipe
  int prev_read = -1;
  // MOD: looping over commands in the pipeline
  for (int i = 0; i < N; ++i) {
    // MOD: prepare space for pipe
    int pipefd[2] = {-1, -1};
    // MOD: need pipe bool for not-last commands in pipeline
    bool need_pipe = (i < N - 1);
    // MOD: error handling for pipe
    if (need_pipe) {
      if (pipe(pipefd) != 0) {
        perror("pipe");
        if (prev_read != -1) close(prev_read);
        return errno ? errno : EIO;
      }
    }
    // MOD: create new process for current command
    pid_t pid = fork();
    // MOD: error handling for fork
    if (pid < 0) {
      perror("fork");
      // MOD: cleanup resources
      if (prev_read != -1) close(prev_read);
      if (need_pipe) { close(pipefd[0]); close(pipefd[1]); }
      return errno ? errno : EIO;
    }

    if (pid == 0) {
      // MOD: child process

      // STDIN
      if (i == 0 && !expression.inputFromFile.empty()) {
        // MOD: opening given file for read only
        int fd = open(expression.inputFromFile.c_str(), O_RDONLY);
        // MOD: error handling if opening for read failed
        if (fd < 0) { perror(expression.inputFromFile.c_str()); _exit(1); }
        // MOD: error handling if reading from file instead of keyboard fails
        if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2"); _exit(1); }
        close(fd);
      } 
      else if (prev_read != -1) {
        // MOD: error handling if read end fails
        if (dup2(prev_read, STDIN_FILENO) < 0) { perror("dup2"); _exit(1); }
      } 
      // MOD: handling if user puts & at the end -> background execution
      else if (expression.background && i == 0 && expression.inputFromFile.empty()) {
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
          // MOD: ensure background process does not read input
          if (dup2(devnull, STDIN_FILENO) < 0) { perror("dup2"); _exit(1); }
          close(devnull);
        } 
        else {
          close(STDIN_FILENO);
        }
      }

      // STDOUT
      if (i == N - 1 && !expression.outputToFile.empty()) {
        // MOD: open file for writing (cases + permissions set)
        int fd = open(expression.outputToFile.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0666);
        // MOD: handling open failure for writing end
        if (fd < 0) { perror(expression.outputToFile.c_str()); _exit(1); }
        if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2"); _exit(1); }
        close(fd);
      } else if (need_pipe) {
        // MOD: error handling write end of the pipe
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) { perror("dup2"); _exit(1); }
      }

      // MOD: close unneeded FDs in the child
      if (prev_read != -1) close(prev_read);
      if (need_pipe) { close(pipefd[0]); close(pipefd[1]); }

      // MOD: execute command
      int rc = execute_command(expression.commands[i]);
      // MOD: error handling if execute command fails
      if (rc != 0) {
        const auto& parts = expression.commands[i].parts;
        if (!parts.empty()) perror(parts[0].c_str());
        _exit(127);
      }
      _exit(0);
    }

    // MOD: parent process
    pids.push_back(pid);

    // MOD: Parent closes FDs it does not use, or pipes will not see EOF
    if (prev_read != -1) { close(prev_read); prev_read = -1; }
    if (need_pipe) {
      close(pipefd[1]); 
      prev_read = pipefd[0];
    }
  }

  // MOD: foreground vs background (not end with & -> foreground job)
  if (!expression.background) {
    // MOD: wait for all children in this pipeline
    for (pid_t cpid : pids) {
      while (true) {
        int status = 0;
        // MOD: wait until specific child terminates
        pid_t w = waitpid(cpid, &status, 0);
        // MOD: if waitpid was interrupted by a signal, retry wait
        if (w == -1 && errno == EINTR) continue;
        break;
      }
    }
  } else {
    // MOD: If command did not end with & -> bg job -> go back to input loop and show new prompt
  }
  // MOD: return with success
  return 0;
}


// framework for executing "date | tail -c 5" using raw commands
// two processes are created, and connected to each other
int step1(bool showPrompt) {
  // create communication channel shared between the two processes
  // MOD: read end, write end
  int fds[2];
  // MOD: error handling for pipe
  if (pipe(fds) != 0) { perror("pipe"); return errno; }

  pid_t child1 = fork();
  if (child1 == 0) {
    // redirect standard output (STDOUT_FILENO) to the input of the shared communication channel
    if (dup2(fds[1], STDOUT_FILENO) < 0) { perror("dup2"); _exit(1); }
    // MOD: closing unused resources - no leakage, reader sees EOF, children do not inherit useless FD's
    close(fds[0]);
    close(fds[1]);
    Command cmd = {{string("date")}};
    execute_command(cmd);
    // MOD: not found executable -> child dies immediately
    int rc = execute_command(cmd);
    if (rc) { perror("date"); _exit(127); }
  }

  pid_t child2 = fork();
  if (child2 == 0) {
    // redirect the output of the shared communication channel to the standard input (STDIN_FILENO).
    // MOD: same as by child1
    if (dup2(fds[0], STDIN_FILENO) < 0) { perror("dup2"); _exit(1); }
    close(fds[1]);
    close(fds[0]);
    Command cmd = {{string("tail"), string("-c"), string("5")}};
    int rc = execute_command(cmd);
    if (rc) { perror("tail"); _exit(127); }
  }

  close(fds[0]);
  close(fds[1]);
  // wait on child processes to finish - because they would become zombies otherwise
  waitpid(child1, nullptr, 0);
  waitpid(child2, nullptr, 0);
  return 0;
}

int shell(bool showPrompt) {
  //* <- remove one '/' in front of the other '/' to switch from the normal code to step1 code
  while (cin.good()) {
    string commandLine = request_command_line(showPrompt);
    Expression expression = parse_command_line(commandLine);
    int rc = execute_expression(expression);
    if (rc != 0)
      cerr << strerror(rc) << endl;
  }
  return 0;
  /*/
  return step1(showPrompt);
  //*/
}

// Added for this repo: the course-provided driver that calls shell() was not
// part of the submitted files, so this minimal entry point is added to make
// the file buildable and runnable standalone.
int main() {
  return shell(true);
}
