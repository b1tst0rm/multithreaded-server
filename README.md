# Multithreaded command line server
A multithread command line server written in C that performs balance
checks and transactions on a simple in-memory database.

`appserver.c` uses fine-grain mutex locking for each account. A global
lock is used on a command buffer that is implemented using a linked
list data structure.

`appserver-coarse.c` provides the same functionality but uses coarse-grain
mutex locking in that each thread locks the entire bank (all accounts) when
it needs to access account(s). This decreases concurrency and, in most cases,
hurts performance.

Note: `Bank.c`/`Bank.h` were provided by another entity and not written
by myself.

## Usage
Run `make` (requires GCC) to compile the server and run the server with:

`./appserver <worker threads> <accounts> <output file>`

`worker threads`: number of worker threads to use in the program. The workers
pull user-inputted commands from command buffer and execute them, writing
successful output to `output file`

`accounts`: number of accounts in the bank, these are numbered 1 to `accounts`

`output file`: name of the log file that threads will output to upon command
completion - use `tail -f <output file>` to watch output live

## Commands
Once running the program, it will only accept the following syntax:
`CHECK <account number`: fetches the current balance for given account number
and places output into file in following format: `<request id> BAL <balance> TIME <time started> <time ended>`

`TRANS <account number> <amount>`: adds the amount to the account number's balance.
Negative numbers subtract from the balance. If the balance falls below zero, all transactions in the same
line are voided and ISF is printed in the output file. Else, the following format appears
upon successful transaction: `<request id> OK TIME <time started> <time ended>` ... note that
you may add up to 10 transactions on one TRANS command. For example, `TRANS 5 10 6 10 7 10` would
place 10 cents into accounts 5, 6 and 7.

`END`: waits for threads to complete all current commands and exits the program gracefully
