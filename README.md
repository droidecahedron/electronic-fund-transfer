# calendar
basic c program for creating accounts with starting balances, and simulating transfers between them.

arg1 is the input file.

arg2 is the number of threads you request to process this file transfer.

input.txt is the input for said test case.

transfProg is the program for the assignment
It receives the file name from the cmd arg, not redirect
so it must be used as 
./transfProg input.txt 10
where 10 is the thread num, and input.txt is the file.
It outputs to stdio. 


If redirected to a text file via 
./transfProg input.txt 10 > output.txt

then test_op program can be used as such
./test_op output.txt EXAMPLE_output.txt

it will report mismatches on which line, or if no mismatches were reported.
(I ran transfProg ~30 times and test_op ~30 times and found no mismatches.
I had to add more cores to my virtual machine to more thoroughly test for data race conditions.)


NOTE: since these arrays were used to read the file
	// no transfer request or account creation that is more than 150 chars (Probably a terrible program for Jeff Bezos)
	char input_buffer[150]; 
	char account_number[10]; // i guess no more than 9,999,999 account numbers
	char account_balance[15]; // didnt see any balances larger than 15 digits
	
	
There was a random case where one line mismatched randomly... but i could not re-create it in hundreds of tests.
I assumed that error was fixed by some code change and i never tracked it down.
