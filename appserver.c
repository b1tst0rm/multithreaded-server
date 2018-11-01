// appserver.c
// Run `make clean` followed by `make` in working directory to compile.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <pthread.h>
#include "Bank.h" // Provides in-memory, volatile "database" & access methods


#define PROMPT "> "
#define OUTPUT "< "
#define MAX_CMD_LEN 125
#define MAX_FILENAME_LEN 100


// CUSTOM STRUCTURES
// A node in the Linked List data structure for storing commands
struct node {
        char cmd[MAX_CMD_LEN]; // Command to be completed
        int request_id;
        struct node *next;     // Pointer to the next node in the list
};


struct buffer {
        struct node *head;    // Points to the first element in Linked List
};

struct pthread_args {
        struct buffer *cmd_buf;
        int *running;
};


// GLOBAL VARIABLES
pthread_mutex_t buffer_lock; // Mutex to lock the entire command buffer


// FUNCTION PROTOTYPES
void handle_interrupt();
int extract_cmd(struct buffer *cmd_buffer, char next_cmd[MAX_CMD_LEN]);
void add_cmd(struct buffer *cmd_buffer, char command_to_add[MAX_CMD_LEN], int request_id);
void *thread_routine(void *args);
int check_input(char *user_in);
int handle_request(char *request);
int check();
int trans();

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
                check_input(user_input);
                // Remove newline character at end of user input from stdin
                user_input[strlen(user_input) - 1] = '\0';
                int valid_input = check_input(user_input);

                if (valid_input == 1) {
                        add_cmd(&command_buffer, user_input, request_id);
                        request_id++; // increment transaction id for next command
                } else if (strncmp(user_input, "END", 3) == 0) {
                        running = 0; // stop all new commands
                        printf("Waiting for all threads to finish and exiting.\n");
                } else {
                        printf("Not a valid command.\n");
                }
        }

        // Wait (blocks) for worker threads to finish before exiting program.
        for (i = 0; i < num_workerthreads; i++) {
                pthread_join(thread_ids[i], NULL);
        }
 
        exit(0);
}

// Returns 1 if user input acceptable, -1 otherwise
int check_input(char *user_in)
{
        if (strncmp(user_in, "CHECK ", 6) == 0) {
                return 1;
        } else if (strncmp(user_in, "TRANS ", 6) == 0) {
                return 1;
        } else {
                // disallowed request
                return -1;
        }
}


// accepts a pointer to the user's input and returns 1 if a command was ran 
// or -1 if failure occured.
int handle_request(char *request)
{
        if (strncmp(request, "CHECK ", 6) == 0) {
                check();
                return 1;
        } else if (strncmp(request, "TRANS ", 6) == 0) {
                trans();
                return 1;
        } else {
                // Disallowed request
                return -1;
        }
}

int check()
{
        return 0;
}

int trans()
{
        return 0;
}

void handle_interrupt()
{
        printf("\n\nCTRL-C ignored. "
               "Please use the END command to exit program.\n\n");
        return;
}

// Returns request ID (nonzero) if command extracted, 
// 0 if no command to extract.
// If command exists and is retrieved from beginning of list, 
// it is placed in the given next_cmd array and is deleted from the 
// Linked List. Head is updated to point to the next command to be processed.
int extract_cmd(struct buffer *cmd_buffer, char next_cmd[MAX_CMD_LEN])
{
        int retval = 0;

        pthread_mutex_lock(&buffer_lock);
 
        // If there is command in the buffer...
        if (cmd_buffer->head != NULL) {
                // *struct1.blah === struct1->blah
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
        int *is_running = routine_args->running;

        while (*is_running) {
                request = extract_cmd(routine_args->cmd_buf, curr_cmd);
                if (request) {
                        int response = handle_request(curr_cmd); 
                        
                        if (response == -1) {
                                printf("mega error, need to print in file\n");
                        } else if (response == 0) {
                                *is_running = 0;   
                        }
                } else {
                        // Do nothing
                }
        }
        printf("Thread %ld is exiting.\n", pthread_self());
}
