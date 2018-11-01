// appserver.c
// Run `make` in working directory to compile.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
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
        struct timeval tv_begin;
        struct node *next;     // Pointer to the next node in the list
};

struct buffer {
        struct node *head;    // Points to the first element in Linked List
};

struct pthread_args {
        struct buffer *cmd_buf;
        struct account *accounts; // pointer to array of accounts
        char log_filename[MAX_FILENAME_LEN];
        int *running;
};

struct account {
        pthread_mutex_t lock;
};

struct transaction {
        int account_number;
        int value;
};


// GLOBAL VARIABLES
pthread_mutex_t buffer_lock; // Mutex to lock the entire command buffer


// FUNCTION PROTOTYPES
void handle_interrupt();
int extract_cmd(struct buffer *cmd_buffer, struct node *curr_cmd_info);
void add_cmd(struct buffer *cmd_buffer, char command_to_add[MAX_CMD_LEN], int request_id, struct timeval tv_begin);
void *thread_routine(void *args);
int check_input(char *user_in);
int handle_request(char *request, struct account *accounts);
void check(struct account *accs, char *cmd, char *log_filename, struct timeval tv_begin, int request_id);
void trans(struct account *accs, char *cmd, char *log_filename, struct timeval tv_begin, int request_id);
int parse_check_cmd(char *cmd);
int parse_trans_cmd(char *cmd, struct transaction transactions[10]);

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
        command_buffer.head = NULL; // set linked list to empty
        int request_id = 1; // The transaction ID given to user
        struct pthread_args args;
        struct timeval tv_begin; // timestamp of when a command begins
 
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
        args.accounts = malloc(sizeof(struct account)*num_accts);
        strcpy(args.log_filename, output_filename);
        pthread_t thread_ids[num_workerthreads];
        for (i = 0; i < num_workerthreads; i++) {
                if (pthread_create(&thread_ids[i], NULL, thread_routine, 
                                (void *) &args) != 0) {
                        perror("pthread_create() error");
                        exit(EXIT_FAILURE);
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

                if (valid_input > 0) {
                        // Get the time that we received this command (start)
                        gettimeofday(&tv_begin, NULL);
                        if (valid_input == 1) {
                                // CHECK
                                int acc_to_check = parse_check_cmd(user_input);
                                if (acc_to_check > num_accts || acc_to_check < 1) {
                                        printf("Invalid account number.\n");
                                } else {
                                        add_cmd(&command_buffer, user_input, request_id, tv_begin);
                                        printf("%sID %d\n", OUTPUT, request_id);
                                        request_id++; // increment transaction id for next command
                                } 
                        } else {
                                // TRANS
                                struct transaction *transactions = malloc(sizeof(struct transaction)*10);
                                int num_transactions = parse_trans_cmd(user_input, transactions);

                                int i = 0;
                                int isValidTransaction = 1;
                                while (i < num_transactions) {
                                        if (transactions[i].account_number > 
                                            num_accts || 
                                            transactions[i].account_number < 1) 
                                        {
                                                isValidTransaction = 0;
                                        }
                                        i++;
                                }
                                if (isValidTransaction) {
                                        add_cmd(&command_buffer, user_input, request_id, tv_begin);
                                        printf("%sID %d\n", OUTPUT, request_id);
                                        request_id++; // increment transaction id for next command
                                } else {
                                        printf("Transaction failed, contained invalid account number.\n");
                                }
                        }
                } else if (strncmp(user_input, "END", 3) == 0) {
                        running = 0; // stop all new commands
                        printf("Waiting for all threads to finish and "
                               "exiting.\n");
                } else {
                        printf("%sNot a valid command. Accepts CHECK, TRANS,"
                               " and END.\n", OUTPUT);
                }
        }

        // Wait (blocks) for worker threads to finish before exiting program.
        for (i = 0; i < num_workerthreads; i++) {
                pthread_join(thread_ids[i], NULL);
        }
 
        exit(EXIT_SUCCESS);
}

// Returns 1 if CHECK command, 2 if TRANS command, -1 otherwise
int check_input(char *user_in)
{
        if (strncmp(user_in, "CHECK ", 6) == 0) {
                return 1;
        } else if (strncmp(user_in, "TRANS ", 6) == 0) {
                return 2;
        } else {
                // disallowed request
                return -1;
        }
}

// Returns account number to check
int parse_check_cmd(char *cmd)
{
        // Parse command for account number
        int begin = 6;
        int end = 6;
        while(cmd[end] != '\0' && cmd[end] != ' ') {
                end++;
        }
        char num[(end-begin) + 1];
        char *begin_arr = &cmd[begin];
        strncpy((char *) num, begin_arr, (end-begin) + 1);

        return atoi(num);
}

void check(struct account *accs, char *cmd, char *log_filename, struct timeval tv_begin, int request_id)
{
        FILE *fp;
        int account_num = parse_check_cmd(cmd);
 
        pthread_mutex_lock(&accs[account_num].lock);
        int amount = read_account(account_num);
        // Time that this command finishes
        struct timeval tv_end;
        gettimeofday(&tv_end, NULL);
        // Append to logfile
        fp = fopen(log_filename, "a");
        fprintf(fp, "%d BAL %d TIME %ld.%06ld %ld.%06ld\n", request_id, amount, tv_begin.tv_sec, tv_begin.tv_usec, tv_end.tv_sec, tv_end.tv_usec);
        fclose(fp);
        pthread_mutex_unlock(&accs[account_num].lock);
}

// Returns pointer to array of SORTED (lowest acc num to highest) transaction structs
int parse_trans_cmd(char *cmd, struct transaction transactions[10])
{
        // Need to pull account numbers out
        int numbers[20];
        int count = 0;
        int begin = 6;
        int end = 6;

        int cmd_len = strlen(cmd);

        while (end < cmd_len) {
                while (cmd[end] != ' ' && cmd[end] != '\0') {
                        end++;
                }
                char num[(end-begin) + 1];
                char *begin_arr = &cmd[begin];
                strncpy((char *) num, begin_arr, (end-begin) + 1);
                numbers[count] = atoi(num);
 
                // Reset for the next number to extract
                count++;
                end += 1;
                begin = end;
        }

        int i = 0;
        int trans_counter = 0;

        // Set the upper limit to 10 transactions...
        if (count > 20) {
                count = 20;
        }
        for (i = 0; i < count; i += 2) {
                transactions[trans_counter].account_number = numbers[i];
                transactions[trans_counter].value = numbers[i+1];
                trans_counter++;
        }
    
        // Need to sort the struct array smallest account_number to biggest
        int j;
        struct transaction temp;
        for (i = 0; i < trans_counter; i++) {
                for (j = i + 1; j < trans_counter; j++) {
                        if (transactions[i].account_number > transactions[j].account_number) {
                                temp = transactions[i];
                                transactions[i] = transactions[j];
                                transactions[j] = temp;
                        }
                }
        }

        return trans_counter;
}

void trans(struct account *accs, char *cmd, char *log_filename, struct timeval tv_begin, int request_id)
{
        struct transaction *transactions = malloc(sizeof(struct transaction)*10);
        int num_transactions = parse_trans_cmd(cmd, transactions);
        FILE *fp;
        int ISF = 0;
        int current_balance;
        int current_account;
        int trans_value;
        int predicted_value;
        int new_balances[num_transactions];

        // Lock all the accounts, starting with smallest account number
        int i = 0;
        for (i = 0; i < num_transactions; i++) {
                pthread_mutex_lock(&accs[transactions[i].account_number].lock);
        }

        // Do the transactions
        for (i = 0; i < num_transactions; i++) {
                current_account = transactions[i].account_number;
                current_balance = read_account(current_account);
                trans_value = transactions[i].value;
            
                predicted_value = current_balance + trans_value;
                if (predicted_value < 0 && ISF == 0) {
                        ISF = current_account;
                } else {
                        new_balances[i] = predicted_value;
                }
        }

        // All accounts had sufficient funds, apply the new balances
        if (ISF == 0) {
                for (i = 0; i < num_transactions; i++) {
                        write_account(transactions[i].account_number, new_balances[i]);
                }
        }

        // Time that this command finishes
        struct timeval tv_end;
        gettimeofday(&tv_end, NULL);
        // Append to logfile
        fp = fopen(log_filename, "a");
        
        if (ISF != 0) {
                // then ISF == account number with insufficient funds
                fprintf(fp, "%d ISF %d TIME %ld.%06ld %ld.%06ld\n", request_id, ISF, tv_begin.tv_sec, tv_begin.tv_usec, tv_end.tv_sec, tv_end.tv_usec);
        } else {
                fprintf(fp, "%d OK TIME %ld.%06ld %ld.%06ld\n", request_id, tv_begin.tv_sec, tv_begin.tv_usec, tv_end.tv_sec, tv_end.tv_usec);
        }
        
        fclose(fp);

        // Unlock all the accounts
        for (i = 0; i < num_transactions; i++) {
                pthread_mutex_unlock(&accs[transactions[i].account_number].lock);
        }
}

void handle_interrupt()
{
        printf("\n\nCTRL-C ignored. "
               "Please use the END command to exit program.\n\n");
}

// Returns request ID (nonzero) if command extracted, 
// 0 if no command to extract.
// If command exists and is retrieved from beginning of list, 
// it is placed in the given next_cmd array and is deleted from the 
// Linked List. Head is updated to point to the next command to be processed.
int extract_cmd(struct buffer *cmd_buffer, struct node *curr_cmd_info)
{
        int retval = 0;

        pthread_mutex_lock(&buffer_lock);
 
        // If there is command in the buffer...
        if (cmd_buffer->head != NULL) {
                // *struct1.blah === struct1->blah
                strcpy(curr_cmd_info->cmd, cmd_buffer->head->cmd);
                curr_cmd_info->request_id = cmd_buffer->head->request_id;
                curr_cmd_info->tv_begin = cmd_buffer->head->tv_begin;
                // Delete the command from the Linked List by resetting 
                // head to next node.
                struct node *new_next = cmd_buffer->head->next;
                free(cmd_buffer->head); // free previously malloc-ed pointer
                cmd_buffer->head = new_next;
                retval = 1;
        } else {
                retval = 0;
        }
 
        pthread_mutex_unlock(&buffer_lock);

        return retval;
}

// Add a node to the end of Linked List and update head. Returns nothing as this
// should always succeed. Should only be called by the main thread.
void add_cmd(struct buffer *cmd_buffer, char command_to_add[MAX_CMD_LEN], int request_id, struct timeval tv_begin)
{
        pthread_mutex_lock(&buffer_lock);

        // Build new node
        struct node *node_to_add = (struct node*)malloc(sizeof(struct node));
        strcpy(node_to_add->cmd, command_to_add);
        node_to_add->next = NULL; // Node will be placed at the END of the list
        node_to_add->request_id = request_id;
        node_to_add->tv_begin = tv_begin;

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
        int request;
        int *is_running = routine_args->running;
        char *log_file_loc = routine_args->log_filename;
        struct node current_command_info;
        while (*is_running) {
                request = extract_cmd(routine_args->cmd_buf, &current_command_info);
                if (request) {
                        if (strncmp(current_command_info.cmd, "CHECK ", 6) == 0) {
                                check(routine_args->accounts, 
                                      current_command_info.cmd, log_file_loc, 
                                      current_command_info.tv_begin,
                                      current_command_info.request_id);
                        } else if (strncmp(current_command_info.cmd, "TRANS ", 6) == 0) {
                                trans(routine_args->accounts, 
                                      current_command_info.cmd, log_file_loc, 
                                      current_command_info.tv_begin, 
                                      current_command_info.request_id);
                        } else {
                                // Do nothing, unrecognized command
                        }
                } else {
                        // Do nothing
                }
        }
        printf("Thread %ld is exiting.\n", pthread_self());
}
