# smproj-nsh

A semester project about writing a shell.

## What does it implement?

* Executing commands
* Backslash quoting and single quote quoting
* Pipeline
* Internal variable
* (Partial) IO redirection
* Executing commands and pipelines in background
* Alias substitution
* Basic parameter expansion
* History (through GNU readline, saves them at ``~/.nsh_history``)
* Line editing (through GNU readline)
* (Partial) File name completion
* (Partial) History substitution

## How to build it?

```
mkdir build
cd build
qmake ../nsh.pro
make
```

## Some examples that will run on it?

```
# Long pipeline
cat /etc/os-release | sed -e 's/=/ /' -e '/^\s*$/d' | awk '{print $1;}' | sort | uniq

# Execute a pipeline in background
uname -a | tr ' ' '\n' &

# Pipeline and IO redirection
</etc/os-release cat | tr '=' '\n' > /tmp/foobar
cat /tmp/foobar

# History substitution
ls /
!! -la
!! --color
sudo !!

# alias
alias ll='ls -la'
ll
alias ls='ls --color'
ll /
alias foobar='cat /etc/os-release | wc -l'
foobar

# unalias
alias
alias ll
unalias ll
ll  # will raise an error

# history
history
history 10
HISTSIZE=5
history
HISTSIZE=

# Internal variable
FOO=BAR
echo $FOO
bash -c 'echo $FOO'

# Environment variable
export FOO=lorem
export
bash -c 'echo $FOO'
unexport FOO
bash -c 'echo $FOO'
```
