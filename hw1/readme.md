# Basic shell #

## 1. Built-in commands :
- cd path
- pwd
- jobs
- fg [-gid] gid
- fg [job_number]
- bg , same as fg
- wait
## 2. support syntax:
```shell
# Input/Output Redirection
ls >> output
ls < input

# pipe
ls | cat
wc | cat

# sequencial execution
ls && wc

#background execution
ls &

#combination of any above
ls | cat > hello && ls hello &
```
## 3. Signal Handling and Terminal Control
## 4. Foreground/Background Switching
