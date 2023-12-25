This is a basic C implementation of a unix-style shell using the REPL paradigm. The shell
supports functionality for input ('<') and output ('>') file redirection, piplelining commands 
('|'), and background execution ('&'). If errors occur, the user will be notified. To exit, the 
user must type Ctrl-D. Please use the Makefile to start the shell.

Assumptions:
        - only one '&' character is allowed
        - input redirection must occur on the first command (assuming pipelining)
        - output rediretion must occur on the last command (assuming pipelining)
        - the maximum length of individual tokens is 32 characters
        - the maximum length of an input line is 512 characters