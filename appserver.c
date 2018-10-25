// appserver.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include "Bank.h" // Provides in-memory, volatile "database" & access methods
#include "appserver.h"

#define PROMPT "> "
#define MAX_INPUT_LEN 100
#define MAX_FILENAME_LEN 100

void main(int argc, char **argv)
{
        int num_workerthreads, num_accts, transac_attempt, cmd_retval;
        int requestID = 0;
        char output_filename[MAX_FILENAME_LEN];
        char cwd[PATH_MAX]; // PATH_MAX from limits.h
        char user_input[MAX_INPUT_LEN];

        if (argc != 4) {
                printf("\nAppServer combined server and client program.\n");
                printf("\nUSAGE: ./appserver <# of worker threads> "
                       "<# of accounts> <output file>\n\n");
                exit(EXIT_FAILURE);
        }

        // Fetch and store command-line arguments        
        num_workerthreads = atoi(argv[1]);
        num_accts = atoi(argv[2]);
        strncpy(output_filename, argv[3], sizeof(argv[3]));

        if (num_workerthreads < 1) {
                printf("\nWorker threads must be at least 1 or more."
                       " Exiting.\n\n");
                exit(EXIT_FAILURE);
        } else if (num_accts < 1) {
                printf("\nNumber of accounts must be at least 1 or more."
                       " Exiting.\n\n");
                exit(EXIT_FAILURE);
        }

        printf("Number of worker threads: %d\n", num_workerthreads);
        printf("Number of accounts: %d\n", num_accts);
        getcwd(cwd, sizeof(cwd));
        printf("Log location: %s/%s\n", cwd, output_filename);

        // Prevent keyboard interrupts
        signal(SIGINT, handle_interrupt);

        printf("\nReady to accept input.\n");
        
        while (1) {
                printf("%s", PROMPT);
                fgets(user_input, MAX_INPUT_LEN, stdin);

                // Remove newline character at end of user input from stdin
                user_input[strlen(user_input) - 1] = '\0';
                transac_attempt = handle_request(user_input, &requestID);
                
                if (transac_attempt == 1) {
                        printf("ID %d\n", requestID);
                } else if (transac_attempt < 0) {
                        printf("Invalid request. Supports CHECK, TRANS,"
                               " END, and HELP.\n");
                }
        }
}

// Accepts a pointer to the user's input as well as a pointer to the request ID
// and returns 1 if a command was ran, 0 if help was displayed, or -1 if 
// invalid request was given.
int handle_request(char *request, int *id)
{
        if (strcmp(request, "END") == 0) {
                end();
                return 1; 
        } else if (strncmp(request, "CHECK", 5) == 0) {
                *id = *id + 1;
                check();
                return 1;
        } else if (strncmp(request, "TRANS", 5) == 0) {
                *id = *id + 1;
                trans();
                return 1;
        } else if (strncmp(request, "HELP", 4) == 0) {
                help();
                return 0;
        } else {
                // Disallowed request
                return -1;
        }
}

void handle_interrupt()
{
        printf("\n\nCTRL-C ignored. "
               "Please use the END command to exit program.\n\n");
        return;
}

int check()
{
        return 0;
}

int trans()
{
        return 0;
}

void end()
{
        printf("Waiting for all threads to finish...\n");
        printf("Cleaning up and exiting program, goodbye.\n");
        exit(EXIT_SUCCESS);
}

void help()
{
        printf("~ Help Desk ~\n");
        printf("CHECK <accountid>\n   Returns: <requestID> BAL "
               "<balance>\n");
        printf("TRANS <acct1> <amount1> <acct2> <amount2> ...\n   "
               "Returns: <requestID> OK on success or <requestID> ISF "
               "<acctid> on first failed account\n");
        printf("HELP\n   Returns: this information\n");
        return;
}
