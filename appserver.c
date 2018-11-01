// appserver.c
// Run `make` in working directory to compile.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <pthread.h>
#include "Bank.h" // Provides in-memory, volatile "database" & access methods
#include "appserver.h"

#define PROMPT "> "
#define OUTPUT "< "
#define MAX_CMD_LEN 125
#define MAX_FILENAME_LEN 100

// A node in the Linked List data structure for storing commands
struct node {
        char cmd[MAX_CMD_LEN]; // Command to be completed
        int request_id;
        struct node *next;     // Pointer to the next node in the list
};

pthread_mutex_t buffer_lock; // Mutex to lock the entire command buffer

struct buffer {
        struct node *head;    // Points to the first element in Linked List
};

struct pthread_args {
        struct buffer *cmd_buf;
        int *running;
};


// FUNCTION PROTOTYPES
void handle_interrupt();
int extract_cmd(struct buffer *cmd_buffer, char next_cmd[MAX_CMD_LEN]);
void add_cmd(struct buffer *cmd_buffer, char command_to_add[MAX_CMD_LEN], int request_id);
void *thread_routine(void *args);


// Main thread accepts user input and places commands into command buffer
// (a linked list). The worker threads that the main thread creates place
// a lock on this buffer, read a command, execute it, and release the buffer.
// The worker threads lock user accounts in the array of structs when
// they carry out TRANS or CHECK commands and may lock more than one account 
// at any given time. If an account that is needed is locked, that thread
// must wait until the account resource becomes available.
// If the user administers the END command, the main thread waits until
// all worker threads have completed (linked list will be empty) and exits
// successfully.
void main(int argc, char **argv)
{
        int running = 1; // Keeps the program running when set to 1
        int num_workerthreads, num_accts, transac_attempt, cmd_retval;
        char output_filename[MAX_FILENAME_LEN];
        char cwd[PATH_MAX]; // PATH_MAX from limits.h
        char user_input[MAX_CMD_LEN];
        // Command buffer that main will place user input into and threads
        // will fetch from.
        struct buffer command_buffer;
        command_buffer.head = NULL;
        int request_id = 1; // The transaction ID given to user
        struct pthread_args args;
        
        // Prevent keyboard interrupts
        signal(SIGINT, handle_interrupt);
    
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

        printf("\nInitializing bank accounts.\n");
        if (initialize_accounts(num_accts) == 0) {
                perror("Failed to init bank accounts.");
                exit(EXIT_FAILURE);
        }
    
        printf("Initializing command buffer mutex\n");
        if (pthread_mutex_init(&buffer_lock, NULL) != 0) {
                perror("Failed to init command buffer mutex.");
                exit(EXIT_FAILURE);
        }
        
        printf("Spinning up worker threads\n");
        int i = 0;
        args.cmd_buf = &command_buffer;
        args.running = &running;
        pthread_t thread_ids[num_workerthreads];
        for (i = 0; i < num_workerthreads; i++) {
                if (pthread_create(&thread_ids[i], NULL, thread_routine, 
                                (void *) &args) != 0) {
                        perror("pthread_create() error");
                        exit(1);
                }
        }

        printf("Ready to accept input.\n");
       
        // Accept user commands and add them to the command buffer 
        while (running) {
                printf("%s", PROMPT);
                fgets(user_input, MAX_CMD_LEN, stdin);
                // Remove newline character at end of user input from stdin
                user_input[strlen(user_input) - 1] = '\0';
                add_cmd(&command_buffer, user_input, request_id);
                request_id++; // increment transaction id for next command
        }

        // Wait (blocks) for worker threads to finish before exiting program.
        for (i = 0; i < num_workerthreads; i++) {
                pthread_join(thread_ids[i], NULL);
        }
 
        exit(0);
}

// Accepts a pointer to the user's input as well as a pointer to the request ID
// and returns 1 if a command was ran, 0 if help was displayed, or -1 if 
// invalid request was given.
//int handle_request(char *request, int *id)
//{
//        if (strcmp(request, "END") == 0) {
//                end();
//                return 1; 
//        } else if (strncmp(request, "CHECK", 5) == 0) {
//                *id = *id + 1;
//                check();
//                return 1;
//        } else if (strncmp(request, "TRANS", 5) == 0) {
//                *id = *id + 1;
//                trans();
//                return 1;
//        } else if (strncmp(request, "HELP", 4) == 0) {
//                help();
//                return 0;
//        } else {
//                // Disallowed request
//                return -1;
//        }
//}

void handle_interrupt()
{
        printf("\n\nCTRL-C ignored. "
               "Please use the END command to exit program.\n\n");
        return;
}

//int check()
//{
//        return 0;
//}
//
//int trans()
//{
//        return 0;
//}
//
//void end()
//{
//        printf("Waiting for all threads to finish...\n");
//        printf("Cleaning up and exiting program, goodbye.\n");
//        exit(EXIT_SUCCESS);
//}
//
//void help()
//{
//        printf("~ Help Desk ~\n");
//        printf("CHECK <accountid>\n   Returns: <requestID> BAL "
//               "<balance>\n");
//        printf("TRANS <acct1> <amount1> <acct2> <amount2> ...\n   "
//               "Returns: <requestID> OK on success or <requestID> ISF "
//               "<acctid> on first failed account\n");
//        printf("HELP\n   Returns: this information\n");
//        printf("END\n   Exits the program\n");
//        return;
//}

// Returns request ID (nonzero) if command extracted, 
// 0 if no command to extract.
// If command exits and is retrieved, it is placed in the given next_cmd
// array and is deleted from the Linked List. Head is updated to point
// to the next command to be processed.
int extract_cmd(struct buffer *cmd_buffer, char next_cmd[MAX_CMD_LEN])
{
        int retval;

        pthread_mutex_lock(&buffer_lock);
 
        // If there is command in the buffer...
        if (cmd_buffer->head != NULL) {
                // *struct1.blah === struct1->blah
                printf("extract_cmd: head's cmd: %s\n", cmd_buffer->head->cmd);
                strcpy(next_cmd, cmd_buffer->head->cmd);
                retval = cmd_buffer->head->request_id;
                // Delete the command from the Linked List by resetting 
                // head to next node.
                struct node *new_next = cmd_buffer->head->next;
                free(cmd_buffer->head); // free previously malloc-ed pointer
                cmd_buffer->head = new_next;
        } else {
                retval = 0;
        }
 
        pthread_mutex_unlock(&buffer_lock);

        return retval;
}

// Add a node to the end of Linked List and update head. Returns nothing as this
// should always succeed. Should only be called by the main thread.
void add_cmd(struct buffer *cmd_buffer, char command_to_add[MAX_CMD_LEN], int request_id)
{
        pthread_mutex_lock(&buffer_lock);

        // Build new node
        struct node *node_to_add = (struct node*)malloc(sizeof(struct node));
        strcpy(node_to_add->cmd, command_to_add);
        node_to_add->next = NULL; // Node will be placed at the END of the list
        node_to_add->request_id = request_id;

        // Append the new node to the end of the list
        struct node *find_end = cmd_buffer->head;
        if (cmd_buffer->head == NULL) {
                // Then this is the FIRST command in the buffer
                cmd_buffer->head = node_to_add;
        } else {
                // then this is NOT the first command in the buffer
                while (find_end->next != NULL) {
                        find_end = find_end->next;
                }
                find_end->next = node_to_add;
        }
 
        pthread_mutex_unlock(&buffer_lock);
}


// This is the function that the worker threads will be assigned.
void *thread_routine(void *args)
{
        struct pthread_args *routine_args = (struct pthread_args*) args;
        char curr_cmd[MAX_CMD_LEN];
        int request;
        int *isRunningPtr = routine_args->running;

        while (*isRunningPtr) {
                request = extract_cmd(routine_args->cmd_buf, curr_cmd);

                if (request) {
                        // Run it
                        pthread_t tid = pthread_self();
                        printf("\nWorker %ld got request ID %d: %s\n", tid, request, curr_cmd);
                        // Do something with it
                        //transac_attempt = handle_request(user_input, &requestID);
                        //
                        //if (transac_attempt == 1) {
                        //        printf("%sID %d\n", OUTPUT, requestID);
                        //} else if (transac_attempt < 0) {
                        //        printf("%sInvalid request. Supports CHECK, TRANS,"
                        //               " END, and HELP.\n", OUTPUT);
                        //}
                } else {
                        // Do nothing
                }
        }
        printf("Exiting thread routine\n");
}
