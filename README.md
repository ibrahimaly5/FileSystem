# CMPUT 379 Assignment 3
Name: Ibrahim Aly
CCID: 1502267

# General Program Structure
The program first opens the input file name using ifstream. Then it uses getline and keeps a line number
to get each line. 

After each line is saved in a std::string, call execute_command which executes the line.

### execute_command
execute_command takes in the entire command and the line number.

The function creates the expected number of inputs based on the given input ("M", "C", etc).
Then, the function calls parse_command (returns a vector of strings) to parse the given line.

If the expected number of inputs is different from the parsed command, it prints an error statement and exits.

Then, the function checks if the arguments are logical, e.g. filename greater than 5, block number is not
between 0 and 127, etc.
If an incorrect argument is wrong, it prints an error statement and exits.

Otherwise, check if there is a disk mounted. If there isn't, it prints an error statement and exits.

If a disk is mounted, run the given function

### fs_mount
fs_mount checks if the disk exists in the current directory. If it doesn't exist in the directory, return
an error statement and exit.

If the disk exists, open the disk, read the free_block_list and check the 6 required requirements.
If all 6 requirements pass, set the disk_name to the new_disk_name, and the current working directory to root
(127).

If one of the requirements fails, close the disk, delete the super_block created on the heap, print error and exit

### fs_create
fs_create checks if the given file name is unique in the current directory. If it's not, give an error function
and exit.

If the file/directory is unique, copy the given name, set the free_block_list, used_size and dir_parent.
Then, write the super_block back to the disk, and return.

### fs_delete
If the file does not exist in the given directory, return an error statement and return. 
Otherwise, zero out the given node and the data blocks, and delete the name, and make the free_block_list
available.

### fs_write
If the file does not exist in the given directory or is a directory, return an error statement and return. 
If the start block is not in the range of the given blocks to the file, return an error statement and return. 
Otherwise, write the buffer to the given data block

### fs_read
If the file does not exist in the given directory or is a directory, return an error statement and return. 
If the start block is not in the range of the given blocks to the file, return an error statement and return. 
Otherwise, read the given data block from the disk to the buffer

### fs_buff
Read the entire line (not the parsed one) from index 2 and load it to the buffer

### fs_ls
Go through all the files/directories that have parent directory as the current directory.
If the file is a directory, call get_dir_count to count the number of children the directory contains
Then, print out the size/number of children in all the found files and '.' and '..'

### fs_resize
Find the node number that corresponds to the given name in the current directory.

Then, if the node is a directory, print an error message and exit.
Otherwise, get the old size of the node and call reduce_size/extend_size. 

Reduce size simply zeroes out all the extra blocks given.

### fs_defrag
First, sort by start_block
Loop through the blocks and the nodes. If the current block is greater than the start block of the node,
then set the new start block of the node to the found block.
If the block number is greater than the start block, go to the next node.

### fs_cd
Find the given node number that has the same name as the given argument and the same parent directory.
If there is no found node, or the node is a file, print an error message and exit.

Otherwise, change the current working directory (cwd) to the node found.


## Testing 
This assignment was tested through all consistency checks and the 4 given test cases.
Furthermore, the program was tested for memory leaks, and any warnings on the lab machines.

The read_directory function was found on StackOverflow to get all the files in the current directory.
https://stackoverflow.com/questions/23093535/file-paths-in-c