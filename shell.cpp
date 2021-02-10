#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <vector>
#include <sstream>
#include <fcntl.h>
#include <string>
#include <algorithm>

using namespace std;

char** vec_to_char_array(vector<string>& parts) {
    char** c = new char *[parts.size() + 1];
    for (int i = 0; i < parts.size(); i++) {
        c[i] = (char*)parts[i].c_str();
    }
    c[parts.size()] = NULL;
    return c;
}
// *****************USER PROMPT*****************************
string get_date() {
    char buf[4096];
    time_t fetch_time = time(0);
    tm* t = localtime(&fetch_time);
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", t);
    string date(buf);
    return date;
}

string get_working_path() {
    char buf[4096];
    getcwd(buf, 4096);
    string cwd(buf);
    return cwd;
}

string get_user() {
    char buf[255];
    getlogin_r(buf, 255);
    string username(buf);
    return username;
}

string get_hostname() {
    char buf[255];
    gethostname(buf, 255);
    string host(buf);
    return host;
}

string prompt() {
    string p = get_date() + "    " + get_user() + "@" + get_hostname() + ":~" + get_working_path() + "$ ";
    return p;
}
// **********************************************************

// *****************PARSING**********************************

// print a prompt, call a function to read a line, 
// call a function to split the line into args, and execute the args

string trim(string line) {
    string t = line;
    if (isspace(line[0])) {
        t = t.substr(1, t.size());
    }
    if (isspace(line[line.size()-1])) {
        t = t.substr(0, t.size()-1);
    }
    return t;
}

string remove_quotes(string line) {
    if (line[0] == '"' || line[0] == '\'') {
        if (line[0] == line[line.size()-1]) {
            return line.substr(1, line.size()-2);
        }
    }
    return line;
}

vector<string> split(string line, char delimiter) {
    vector<string> vec;
    stringstream ss(line);
    string str;
    while(getline(ss, str, delimiter)) {
        vec.push_back(str);
    }
    return vec;
}

// ***********************************************************

int main() {
    int backup_in = dup(0);
    int backup_out = dup(1);
    string previous_dir = ".";
    while (true) {
        cout << prompt();
        string inputline;
        dup2(backup_in, 0);
        dup2(backup_out, 1);
        getline(cin, inputline);
        // cout << "input line: " << inputline << endl;
        char buf[255];
        
        vector<int> zombie;
        for (int i = 0; i < zombie.size(); i++) {
            if(waitpid(zombie[i], 0, WNOHANG) == zombie[i]) {
                zombie.erase(zombie.begin() + i);
                i = i - 1;
            }
        }

        if (inputline == string("exit")) {
            cout << "Bye!! End of shell" << endl;
            break;
        }

        bool bg = false;
        if (inputline[inputline.size()-1] == '&') {
            bg = true;
            inputline = inputline.substr(0, inputline.size()-1);
        }
        
        vector<string> pparts;
        if (inputline.substr(0,4) == "echo") {
            pparts.push_back(inputline);
        } else {
        pparts = (split(inputline, '|'));
        }
        for (int i = 0; i < pparts.size(); i++) {
            pparts[i] = trim(pparts[i]);
        }

        for (int i = 0; i < pparts.size(); i++) {
            vector<string> parts;
            inputline = pparts.at(i);
            // cout << "pparts at " << i << " : " << inputline << endl;
            int fds[2];
            pipe(fds);
            int pid = fork();
            if (pid == 0) {
                if (trim(inputline).find("cd") == 0) {
                    string dir = trim(split(inputline, ' ')[1]);
                    if (dir == "-") {
                        // cout << "Changing to: " << previous_dir.c_str() << endl;
                        dir = previous_dir;
                    }
                    previous_dir = getcwd(buf, sizeof(buf));
                    chdir(dir.c_str());
                    // cout << "Setting prev dir to: " << previous_dir.c_str() << endl;
                    continue; // don't go into execvp
                }
                bool echoFlag = false;
                if (trim(inputline).find("echo") == 0) {
                    echoFlag = true;
                    int single = 0, doub = 0;
                    for (int i = 0; i < inputline.size(); i++) {
                        if (inputline[i] == '\'') { single++; }
                        if (inputline[i] == '\"') { doub++; }
                    }
                    if (doub == 2) {
                        inputline.erase(remove(inputline.begin(), inputline.end(), '\"'), inputline.end());
                    }
                    if (single == 2) {
                        inputline.erase(remove(inputline.begin(), inputline.end(), '\''), inputline.end());
                    }
                    parts = split(inputline, ' ');
                
                }
                if (!echoFlag) {
                    int pos = inputline.find('>');
                    if (pos >= 0) {
                        string command = trim(inputline.substr(0, pos));
                        string filename = trim(inputline.substr(pos+1));
                        inputline = command;

                        int fd = open(filename.c_str(), O_WRONLY|O_CREAT, S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
                        dup2(fd, 1);
                        // close(fd);
                        
                    }

                    pos = inputline.find('<');
                    if (pos >= 0) {
                        string command = trim(inputline.substr(0, pos));
                        string filename = trim(inputline.substr(pos+1));
                        inputline = command;

                        int fd = open(filename.c_str(), O_RDONLY, S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
                        dup2(fd, 0);
                        // close(fd);
                    }
                    parts = split(inputline, ' ');
                }
            
                if(trim(inputline).find("awk") == 0) {
                    // cout << "before mod : " << inputline << endl;
                    inputline.erase(remove(inputline.begin(), inputline.end(), '\''), inputline.end());
                    // cout << "remove ' : " << inputline << endl;
                    int pos = inputline.find('{');
                    if (pos >= 0) {
                        inputline.erase(remove(inputline.begin() + inputline.find('{'), inputline.begin() + inputline.find('}')+1, ' '), inputline.end());
                    }
                    string str = inputline;
                    // cout << "str: " << str << endl;   
                    parts = split(inputline, ' ');
                }
                
                if (i < pparts.size() - 1) {
                    dup2(fds[1], 1);
                    close(fds[1]);
                }
                char** args = vec_to_char_array(parts);
                //execvp(args[0], args);
                execvp(vec_to_char_array(parts)[0], vec_to_char_array(parts));
            } else {
                if (!bg) {
                    if (i == pparts.size() - 1) {
                    waitpid(pid,0,0);
                    } else {
                        zombie.push_back(pid);
                    }
                } else {
                    zombie.push_back(pid);
                }
                dup2(fds[0], 0);
                close(fds[1]);
            }
        }
    }
}