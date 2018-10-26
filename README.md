# cpre308-project2: Multithreaded server
Note: `Bank.c`/`Bank.h` were provided by the instructor to provide a backend
database. I wrote `appserver.c`/`appserver.h`. `testscript.pl` was also
provided.

## Usage
Run `make` to compile the server and run the server with `./appserver`
(among other arguments that the program accepts.

## Test Script

To run the script use the command:
`./testscript.pl ./appserver [numThreads] [numAccounts] [seed]`
For example, a test with 10 threads, 1000 accounts, and a seed of 0 would be:
`./testscript.pl ./appserver 10 1000 0`

If the test fails, you should receive some message `ERR: <error message>` at 
the end of the script output.

The script will operate as follows:

1) 1000000 is added to every account and the script waits to allow your server 
to process all these requests
1) 300 random transactions occur (these are based off the seed value provided) 
and the script then waits to allow your server to process all the requests
1) The balance of every account is checked, followed by an END command and the 
script then waits for your server to finish
1) The script checks the output file  (which is stored in testout-xxx, where 
xxx is some number) and reports back if it finds and discrepancies.

In some cases if the tested program does not process transactions as fast as 
expected, step four will start before step three has finished causing the test 
script to report a failure. If this is the case increase the `$testSleepTime` 
variable on line 31 of the test script.
