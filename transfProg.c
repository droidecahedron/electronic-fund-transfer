
#include <sys/wait.h>
#include "unistd.h"
#include <stdlib.h>

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

static sem_t read_data_ptr; // binary semaphore

int max_threads;
int num_accounts;
int num_EFT;
int EFT_processed = 0;

// global semaphore pointer for accounts. We will have num_account semaphores of this.
//The reason is that there can be hundreds of accounts, and we only care about the accounts we are modifying
//values from in terms of shared state. i.e. a thread can be writing to accounts 3 and 4, 
//and another one can be writing to 5 and 6 concurrently.
//If we use only one "account" semaphore, only one account gets written to at a time, so only one thread does work at a time.
//which would result in bad parallelism.
sem_t* accounts_sem; 


struct account* accounts; // global account pointer
struct transfer* EFTs; // global pointer to buffer of transfer operations

struct account
{ 
	int accountNum;
	int accountBalance;
}; 

struct transfer
{
	int accountFromNum;
	int accountToNum;
	int amount;
};


//thread prototype
void* eft_thread(void* threadNum);



int main(int argc, char *argv[]) 
{

  	
	char input_buffer[150];
	char account_number[10]; // i guess no more than 999,999,999 account numbers
	char account_balance[15]; // didnt see any balances larger than 15 digits
	
	char accountFromNum[10];
	char accountToNum[10];
	char transferAmount[15];
	
	//open the file
	FILE *ip_file_ptr = fopen(argv[1],"r");
	
	//get the number of accounts
	while(fgets(input_buffer, sizeof input_buffer+1, ip_file_ptr) != NULL)
	{
		if(input_buffer[0] == 'T')
			break;	
		num_accounts++;	
	}
	
	rewind(ip_file_ptr); // reset the file pointer
	//global pointer to the accounts struct. this way, threads can access it.
	accounts = malloc(num_accounts * sizeof(*accounts)); // allocate the memory
	
	//allocate memory for the semaphores
	accounts_sem = malloc(num_accounts * sizeof(*accounts_sem));
	
	
	//~~~~~~~~~~~~~~~~~process accounts~~~~~~~~~~~~~~~~~~~~~~~~~
	//start reading the file again to populate our accounts
	int space_index,newline_index;
	for(int i=0;i<num_accounts;i++)
	{
		fgets(input_buffer, sizeof input_buffer+1, ip_file_ptr); // now holding an account and balance, separated by a space
		int j = 0;
		while(input_buffer[j-1] != '\n')
		{
			if(input_buffer[j] == ' ')
				space_index = j; // save where the first space is.
			if(input_buffer[j] == '\n')
				newline_index = j; // save where the new line is
			j++;
		}
		
		memcpy(account_number,&input_buffer[0],space_index); // memcpy from start to the first space for account number
		memcpy(account_balance,&input_buffer[space_index+1],newline_index-space_index); // memcpy from space to newline
		
			
		accounts[i].accountNum = atoi(account_number); // convert to integer and store it
		accounts[i].accountBalance = atoi(account_balance);	// convert to integer and store it
	}
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	
	//now we need to find how many transfers we have to allocate the memory
	
	while(fgets(input_buffer,sizeof input_buffer+1, ip_file_ptr) != NULL)
		num_EFT++;
	EFTs = malloc(num_EFT * sizeof(*EFTs));
	
	rewind(ip_file_ptr); // reset the file pointer
	//now we need to get back to the first transfer
	for(int i=0;i<num_accounts;i++)
		fgets(input_buffer, sizeof input_buffer+1, ip_file_ptr);
	//file pointer is now pointing to the first EFT! next fgets will pull it.
		
	//~~~~~~~~~~~~~~~~~process transfers~~~~~~~~~~~~~~~~~~~
	int eft_index = 0;
	while(fgets(input_buffer, sizeof input_buffer+1, ip_file_ptr) != NULL) // runs to end of file since only transfers are left
	{
		//process the transfer line
		int j=0;
		int space_counter = 0;
		int space_one, space_two, space_three;
		//there will be three spaces
		while(input_buffer[j-1] != '\n')
		{
			if(input_buffer[j] == ' ' && space_counter == 0)
			{
				space_one = j;
				space_counter++;
			}
			else if(input_buffer[j] == ' ' && space_counter == 1)
			{
				space_two = j;
				space_counter++;
			}
			else if(input_buffer[j] == ' ' && space_counter == 2)
				space_three = j;
			if(input_buffer[j] == '\n')
				newline_index = j; // save where the new line is	
			j++;
		}
		
		//memcpy. WE SAVE THE SPACE, atoi will stop copying integers after.
		//if you dont save the space, you will have issues with a previous reading being more digits than the next.
		memcpy(accountFromNum,&input_buffer[space_one+1],space_two-space_one); //after space1 by 1 byte
		memcpy(accountToNum,&input_buffer[space_two+1],space_three-space_two);
		memcpy(transferAmount,&input_buffer[space_three+1],newline_index-space_three);
	
		//update struct
		EFTs[eft_index].accountFromNum = atoi(accountFromNum);
		EFTs[eft_index].accountToNum = atoi(accountToNum);
		EFTs[eft_index].amount = atoi(transferAmount);
		
		eft_index++;
	}
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	  		
	fclose(ip_file_ptr); // done with file
	
	
	
	
	//now all accounts are created and stored, and we know how many accounts we have. [num_accounts]
	//all transfers are also stored, and we know how many transfers we have pending [num_EFT]
	max_threads = atoi(argv[2]); // command line passed argument for how many threads we can run, convert string to int
	pthread_t threads[max_threads]; // this holds the unique thread identities
	int s; // error check variable
	
  	
	if(sem_init(&read_data_ptr, 0, 1) == -1) // initialize to 1 to use as a "lock"
		exit(3); // if it returns -1, semaphore was not able to be initialized
		
	//one semaphore per account. all accounts are mutually exclusive.
	for(int i=0;i<num_accounts;i++)
		if(sem_init(&accounts_sem[i],0,1) == -1)
			exit(3); // if it returns -1, semaphore wasnt able to be initialized.
		
	//we need a semaphore for each account, to show that account in use.
		
	//create max threads
	for(int i=0;i<max_threads;i++)	
	  	s = pthread_create(&threads[i],NULL,eft_thread,(void*)(intptr_t)i);	
		if(s != 0)
	  		exit(1); // error check
	
	//wait for all our threads
	for(int i=0;i<max_threads;i++)
		s = pthread_join(threads[i],NULL); // wait for the threads to finish executing
	  	if(s != 0)
	  		exit(2); // error check
  		

	
	//once all threads return, print final balances
	for(int i=0;i<num_accounts;i++)
		printf("%d %d\n",accounts[i].accountNum,accounts[i].accountBalance);
	
	return 0; //main thread returns 0
}



void* eft_thread(void* threadNum)
{
	//can do int x = (int)threadNum; now
	int account_from_index, account_to_index, EFT_request;
	while(1)
	{
		//save value of buffer pointer. Since other threads can change it, mutex
		sem_wait(&read_data_ptr);
		EFT_request = EFT_processed; // Get a task.
		EFT_processed++; // increment task pointer so another thread can get next task [current task will be done]
		sem_post(&read_data_ptr);
	
		if(EFT_request>=num_EFT) //no more tasks to do, we can leave.
			break; //we're about to leave the loop, since there is no more data to process
		
		
		//using local reference, reading from accounts is fine.
		//the account NUMBERS never change. 
		for(int i=0;i<num_accounts;i++)
			if(accounts[i].accountNum == EFTs[EFT_request].accountFromNum)
				account_from_index = i;
		
		for(int i=0;i<num_accounts;i++)
			if(accounts[i].accountNum == EFTs[EFT_request].accountToNum)
				account_to_index = i;
		
		//we must wait for these specific accounts to be done being accessed
		//The reason for this wait sequence is because of the situation
		//Transfer 1 2 50
		//Transfer 2 3 50
		//If thread1 is done with account 2, then it should release so thread2 can grab account 2
		sem_wait(&accounts_sem[account_from_index]);
		accounts[account_from_index].accountBalance -= EFTs[EFT_request].amount; // subtract eft amount from account	
		sem_post(&accounts_sem[account_to_index]);
		
		sem_wait(&accounts_sem[account_to_index]);
		accounts[account_to_index].accountBalance += EFTs[EFT_request].amount; // add eft amount to account
		//we are done with these two accounts.
		sem_post(&accounts_sem[account_from_index]);
		
	}
	//pthread exit implicitly called here.

}



