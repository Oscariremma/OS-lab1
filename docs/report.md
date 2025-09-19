# Report Lab 1 Group 29
## Oscar Eriksson, Geetha Jeyapaul, Aubin Roche

### Have you met all the specifications outlined for the lab?
Our code passes all the automated tests as well as our manual testing of the different features. This leads us to believe that our implementation follows all of the outlined specifications in the lab. 

### In which order?

Most of the specifications were implemented in the order given in the README, with the exception of zombie process handling which we implemented early.

First, we started by implementing the Ctrl-D handling by checking if the returned `char*` from `readline()` was `NULL` which means that it received End-of-File or EOF. In that case we simply exited the `while(true)` loop which after cleaning up exits the process.

Then we moved on to implementing some basic command handler to allow the user to enter commands such as `ls` or `date`. This was done by using `fork()` to start a child and then `execvp()` which takes in a program and the arguments to it and then replacing the new child with it while taking the `PATH` environment variable in to account. In order to wait for the command to finish before returning we utilized `wait` in the parent process.

We then continued by implementing background processes by allowing for running commands without having to wait for them to complete and thus not blocking the parent/foreground process. This was implemented by simply not calling `wait()` after launching the process. This would however cause our background processes to become zombies when they exited as we would never wait for them.

To fix this we implemented a signal handler to catch the `SIGCHLD` signal that is sent whenever one of our children dies. Once we receive a `SIGCHILD` we call `waitpid` with `-1` and `WNOHANG` as parameters in a loop which causes us to clear any dead/zombie children that we currently have, causing then to be freed by the kernel.

The next feature we implemented was piping, allowing us to enter command such as `ls -l | grep a`. This was done by creating pipes using the `pipe()` function and then replacing the `STDOUT` of the child (`ls` for example) with the write end of the pipe by using `dup2()`. When we then moved on to `grep a` we could take the read end of the previously created pipe and replace the `STDIN` of `grep a` which connects the to processes causing the output of the first (`ls`) to flow in to the output of the next (`grep a`).

After that, we wanted to allow the user to utilize I/O redirection (`test.txt > wc` or `ls > contents.txt`) to read input from and store output to files instead of the console. This was implemented similarly to how piping was implemented but instead of creating pipe pairs using `pipe` we instead used `open` to open/create files that could then be used to replace the `STDIN/STDOUT` of the first and last child in the chain respectively. This causes the programs to write/read from the files provided instead of the console.

The next step was to implement the built-ins `cd` and `exit`. The difference with other commands is that these are not external program we need/can launch, rather we need to directly implement their functionality within our shell. The `exit` built in was easy to implement as we essentially only needed to call the `exit()` function.
The `cd` built-in, on the other hand, required a bit more work. First we needed to handle if the path began with `~` or if it was simply empty (which should be the same as `cd ~`). If the path stated with `~` we needed to get the users current home directory by reading the `HOME` environment variable and replacing `~` with its contents. When we have our complete path we can then simply call `chdir()` to change our current working directory to the new path.

However when we exit our shell we need to tell any potential running children be they foreground or background that we have exited. This is done by sending the `SIGHUP` signal to them. The way we implemented sending this to all running children without having to manually keep track of them was to read what our current children are from the `/proc` file system provided by the kernel. Once we had our list we could simply utilize `kill()` in a loop to send `SIGHUP` to everyone.

Finally, we completed the implementation of our shell by implementing forwarding of the `SIGINT` signal caused by Ctrl+C to any running foreground processes. This was done by assigning all processes started within a "piped" command to the same PID group and then saving the current PGID so that from our `SIGINT` handler we could simply send the `SIGINT` forward by calling `kill()` with the negative of the PGID which tells the kernel to send the signal to all members of the group.


### What challenges did you encounter in meeting each specification?

For the most part everything went smoothly with the exception of pipe handling. Here we ran in to problems with pipes not sending end of file when a program ran to completion in the pipeline. This caused the following programs to hang waiting for more input that would never come. This was caused by us "leaking" FD's to the write end of the pipes in the parent which meant that the read end would never give end of file as there were still write end's open. This was fixed by more carefully ensuring that all unused FD were closed as soon as they were not needed any more. This was also done at the same time as we reversed the order we walked the commands to go from left to right instead of the right to left order the parser gives us. This was done to make it easier to think about the order of operations while launching the programs and making the pipe logic simpler.

A challenge we also encountered was handling how `cd` behaves in a classic shell depending on the options. We therefore managed both the case where no argument is provided, and the case where the `~` symbol is used to indicate the users home directory. This was relatively easy to solve by using `getenv('HOME')` to get the home folder and then replacing the `~` with that.

Another small "challenge" (or rather user error) we ran into was the tests failing in an unexpected way on GitHub after implementing the SIGINT handling. Both the CTRL-C tests where failing when running in Github Actions but when we tested locally everything worked perfectly. This turned out to be a stupid user error where all the code was committed except for the line where we actually registered our signal handler... This caused major confusion but once we saw the issue the fix was as simple as actually committing the line where we register the handler.

### Do you have any feedback for improving the lab materials?
  #### Did you find the automated tests useful?

  The automated test were nice to have as it gave us a reliable way to test that our implementation worked properly as well as that we did not introduce any regressions in previous functionality as we worked on more features. Of course it was useful to manually test the program as well to see that a feature worked as expected but the automated testing gave us confidence that it worked according to the specifications.

  #### Do you feel that there is any test case missing?

  We did not encounter any cases where the program had a bug that only our manual testing caught but that passed the tests. All the cases where the program misbehaved in our manual testing it also failed the automated testing. There are certainly more subtle ways that a program could misbehave while still passing the current tests but we never (as far as we know) ran into any such case.
  