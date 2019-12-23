#include <stdio.h>
#include <stdint.h>
#include "FileSystem.h"
#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <vector>
#include <dirent.h>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <math.h>
#include <map>

const int NUM_INODES = 126;

std::fstream disk;
std::string disk_name;
std::string last_disk_used = "";
Super_block *super_block;

std::ifstream input_file;
std::string input_file_name;

int cwd = 127;
uint8_t buffer[1024];
const uint8_t zero_buffer[1024] = { 0 };

std::vector<std::string> parse_command(std::string command) {
    std::vector<std::string> result;

    std::string temp = "";
    for (auto character : command) {
        if (character == ' '){
            if (temp != "")
                result.push_back(temp);
            temp = "";
        }
        else
            temp += character;
    }
    if (temp != "")
        result.push_back(temp);

    return result;
}

std::vector <std::string> read_directory(const std::string& path = std::string()) {
    std::vector <std::string> result;
    dirent* de;
    DIR* dp;
    errno = 0;
    dp = opendir( path.empty() ? "." : path.c_str() );
    if (dp) {
        while (true){
            errno = 0;
            de = readdir( dp );
            if (de == NULL) break;
            result.push_back( std::string( de->d_name ) );
        }
        closedir(dp);
        std::sort(result.begin(), result.end() );
    }
    return result;
}

void close_disk() {
    disk.close();
    delete super_block;
}

bool check_mount_1() {
    std::unordered_map<int, int> usage;

    //Initialize usage list
    for (int i=0; i<16; i++) {
        for (int j=7; j>=0; j--){
            usage[i*8 + (7-j)] = ((super_block->free_block_list[i] >> j) & 1);
        }
    }

    for (int i=0; i<126; i++) {
        if (super_block->inode[i].start_block == 0) {
            continue;
        }

        int start_block = (int) super_block->inode[i].start_block;
        int size = super_block->inode[i].used_size & ~(1 << 7);

        if (start_block > 127 or start_block < 1)
            continue;
        for (int j=start_block; j < start_block + size; j++) {
            if (usage[j] == 0)
                return false;
            else
                usage[j]--;
        }
    }

    for (auto pair : usage) {
        if (pair.first == 0)
            continue;
        
        if (pair.second == 1)
            return false;
    }

    return true;
}

bool check_mount_2() {
    std::unordered_map<std::string, std::unordered_set<int>> directory;
    
    for (int i=0; i<126; i++) {        
        std::string name = "";
        for (int j = 0; j < 5; j++){
            if (super_block->inode[i].name[j] != '\0')
                name += super_block->inode[i].name[j];
        }
        
        if (name == "")
            continue;
        int parent_node = super_block->inode[i].dir_parent & ~(1 << 7);

        if (directory.find(name) == directory.end()) {
            directory[name].insert(parent_node);
        } else {
            if (directory[name].find(parent_node) == directory[name].end()) {
                directory[name].insert(parent_node);
            } else {
                return false;
            }
        }
    }

    return true;
}

bool check_mount_3() {
    for (int i=0; i<126; i++) {
        int state = (super_block->inode[i].used_size >> 7) & 1;
        if (state == 0){
            if (
                (super_block->inode[i].dir_parent == 0) &&
                (super_block->inode[i].used_size == 0) &&
                (super_block->inode[i].start_block == 0))
            {
                for (int j=0; j<5; j++){
                    if ((int) super_block->inode[i].name[j] != 0)
                        return false;
                }
            }
        } else {
            bool all_zero = true;
            for (int j=0; j<5; j++){
                if ((int) super_block->inode[i].name[j] != 0){
                    all_zero = false;
                    break;
                }
            }
            if (all_zero)
                return false;
        }
    }

    return true;
}

bool check_mount_4() {
    for (int i=0; i<126; i++) {
        int start_block = super_block->inode[i].start_block;
        int type = (super_block->inode[i].dir_parent >> 7) & 1;
        int in_use = (super_block->inode[i].used_size >> 7) & 1;

        if (type == 0 and in_use == 1) {
            if (start_block < 1 || start_block > 127)
                return false;
        }
    }

    return true;
}

bool check_mount_5() {
    for (int i=0; i<126; i++) {
        int start_block = super_block->inode[i].start_block;
        int size = super_block->inode[i].used_size & ~(1 << 7);
        int type = (super_block->inode[i].dir_parent >> 7) & 1;

        if (type == 1) {
            if (size != 0 or start_block != 0)
                return false;
        }
    }

    return true;
}

bool check_mount_6() {
    for (int i=0; i<126; i++) {
        int parent_node = super_block->inode[i].dir_parent & ~(1 << 7);

        if (parent_node == 126)
            return false;
        
        int state = (super_block->inode[i].used_size >> 7) & 1;
        if (state == 0)
            continue;

        if (parent_node >= 0 and parent_node <= 125) {
            int in_use = (super_block->inode[parent_node].used_size >> 7) & 1;
            int type = (super_block->inode[parent_node].dir_parent >> 7) & 1;

            if (in_use == 0 or type == 0)
                return false;
        }
    }

    return true;
}

void fs_mount(char *new_disk_name) {
    std::vector<std::string> files = read_directory();
    
    if (std::find(files.begin(), files.end(), new_disk_name) == files.end()){
        std::cerr << "Error: Cannot find disk " << new_disk_name << std::endl;
        return;
    }

    if (disk_name != ""){
        close_disk();
        last_disk_used = disk_name;
    }

    disk.open(new_disk_name, std::ios::out | std::ios::in | std::ios::binary);
    super_block = new Super_block;
    disk.seekg (0, std::ios::beg);
    disk.read(super_block->free_block_list, 16);

    for (int i=0; i<126; i++) {
        disk.read(super_block->inode[i].name, 5);
        disk.read((char *) &super_block->inode[i].used_size, 1);
        disk.read((char *) &super_block->inode[i].start_block, 1);
        disk.read((char *) &super_block->inode[i].dir_parent, 1);
    }

    if (!check_mount_1()){
        std::cerr << "Error: File system in " << new_disk_name << " is inconsistent (error code: 1)\n";
        close_disk();

        if (last_disk_used != ""){
            fs_mount(&last_disk_used[0]);
        }
        return;
    }

    if (!check_mount_2()){
        std::cerr << "Error: File system in " << new_disk_name << " is inconsistent (error code: 2)\n";
        close_disk();
        
        if (last_disk_used != ""){
            fs_mount(&last_disk_used[0]);
        }
        return;
    }

    if (!check_mount_3()){
        std::cerr << "Error: File system in " << new_disk_name << " is inconsistent (error code: 3)\n";
        close_disk();
        
        if (last_disk_used != ""){
            fs_mount(&last_disk_used[0]);
        }
        return;
    }

    if (!check_mount_4()){
        std::cerr << "Error: File system in " << new_disk_name << " is inconsistent (error code: 4)\n";
        close_disk();
        
        if (last_disk_used != ""){
            fs_mount(&last_disk_used[0]);
        }
        return;
    }

    if (!check_mount_5()){
        std::cerr << "Error: File system in " << new_disk_name << " is inconsistent (error code: 5)\n";
        close_disk();
        
        if (last_disk_used != ""){
            fs_mount(&last_disk_used[0]);
        }
        return;
    }

    if (!check_mount_6()){
        std::cerr << "Error: File system in " << new_disk_name << " is inconsistent (error code: 6)\n";
        close_disk();
        
        if (last_disk_used != ""){
            fs_mount(&last_disk_used[0]);
        }
        return;
    }

    disk_name = new_disk_name;
    cwd = 127;
}

bool is_unique_name(char new_name[5]) {
    std::unordered_map<std::string, std::unordered_set<int>> directory;
    std::string new_name_string = "";
    for (int j = 0; j < 5; j++){
        new_name_string += new_name[j];
    }

    if (strncmp(new_name, ".", 5) == 0 || strncmp(new_name, "..", 5) == 0){
        return false;
    }
    
    directory[new_name_string].insert(cwd);
    for (int i=0; i<126; i++) {
        if (strncmp(super_block->inode[i].name, "", 5) == 0) {
            continue;
        }

        std::string name = "";
        for (int j = 0; j < 5; j++){
            name += super_block->inode[i].name[j];
        }
        
        int parent_node = super_block->inode[i].dir_parent & ~(1 << 7);

        if (directory.find(name) == directory.end()) {
            directory[name].insert(parent_node);
        } else {
            if (directory[name].find(parent_node) == directory[name].end()) {
                directory[name].insert(parent_node);
            } else {
                return false;
            }
        }
    }

    return true;
}

int get_start_block(int req_size) {
    for (int i=1; i<128; i++){
        int start_index = floor(i/8);
        int start_block_state = (super_block->free_block_list[start_index] >> (7-(i%8))) & 1;

        if (start_block_state == 0 and (i + req_size) <= 128) {
            for (int j=0; j<req_size; j++) {
                int end_index = floor((i + j)/8);
                int end_block_state = (super_block->free_block_list[end_index] >> (7-((i+j)%8))) & 1;

                if (end_block_state == 1)
                    break;

                if (end_block_state == 0 and j == req_size - 1){
                    return i;
                }
            }
        }
    }

    return -1;
}

void write_super_block() {
    disk.seekp(0, std::ios::beg);
    disk.write(super_block->free_block_list, 16);
    for (int i=0; i<NUM_INODES; i++){
        disk.write(super_block->inode[i].name, 5);
        disk.write((char *) &super_block->inode[i].used_size, 1);
        disk.write((char *) &super_block->inode[i].start_block, 1);
        disk.write((char *) &super_block->inode[i].dir_parent, 1);
    }
}

void fs_create(char name[5], int size) {
    for (int i=0; i < NUM_INODES; i++) {
        int state = (super_block->inode[i].used_size >> 7) & 1;
        
        if (state == 1)
            continue;

        if (!is_unique_name(name)){
            std::cerr << "Error: File or directory " << name << " already exists" << std::endl;
            return;
        }

        //If directory
        if (size == 0) {
            super_block->inode[i].start_block = 0;
            super_block->inode[i].dir_parent |= 1 << 7;
        } else {
            int start_block = get_start_block(size);
            if (start_block == -1) {
                std::cerr << "Error: Cannot allocate " << size << " on " << disk_name << std::endl;
                return;
            }

            super_block->inode[i].used_size += size;

            super_block->inode[i].start_block = start_block;

            super_block->inode[i].dir_parent &= ~(1 << 7);

            for (int i=start_block; i<start_block+size; i++) {
                int index = floor(i/8);
                super_block->free_block_list[index] |= 1 << (7-(i%8));
            }
        }
        
        strncpy(super_block->inode[i].name, name, 5);

        super_block->inode[i].used_size |= (1 << 7);

        super_block->inode[i].dir_parent += cwd;
        
        write_super_block();
        return;
    }

    std::cerr << "Error: Superblock in disk " << disk_name << " is full, cannot create " << name << std::endl;
}

std::vector<uint8_t> load_buffer(std::vector<std::string> result) {
    std::vector<uint8_t> new_buffer;
    for (unsigned int i=1; i<result.size(); i++) {
        for (uint8_t character : result[i])
            new_buffer.push_back(character);
        
        if (i < result.size() - 1) {
            new_buffer.push_back(' ');
        }
    }

    for (unsigned int i=new_buffer.size(); i<1024; i++){
        new_buffer.push_back(0);
    }

    return new_buffer;
}

void fs_buff(uint8_t buff[1024]) {
    for (int i=0; i<1024; i++){
        buffer[i] = buff[i];
    }
}

void fs_write(char name[5], int block_num) {
    for (int i=0; i<NUM_INODES; i++) {
        if (strncmp(name, super_block->inode[i].name, 5) != 0) {
            continue; 
        }

        int type = (super_block->inode[i].dir_parent >> 7) & 1;
        int parent_node = super_block->inode[i].dir_parent & ~(1 << 7);

        if (type == 1 and parent_node == cwd) {
            std::cerr << "Error: File " << name << " does not exist" << std::endl;
            return;   
        }

        if (parent_node != cwd) {
            continue;
        }

        int size = super_block->inode[i].used_size & ~(1 << 7);
        if (block_num >= size) {
            std::cerr << "Error: " << name << " does not have block " << block_num << std::endl;
            return;
        }

        int start_block = super_block->inode[i].start_block;

        disk.seekp((start_block + block_num)*1024, std::ios::beg);
        disk.write((char *) &buffer, 1024);

        return;
    }
    std::cerr << "Error: File " << name << " does not exist" << std::endl;
}

int get_dir_count(int dir) {
    int count = 2;
    for (int i=0; i<NUM_INODES; i++) {
        int parent_node = super_block->inode[i].dir_parent & ~(1 << 7);

        if (parent_node == dir)
            count++;
    }

    return count;
}

void fs_ls() {
    int parent_node;
    if (cwd != 127) {
        parent_node = super_block->inode[cwd].dir_parent & ~(1 << 7);
    } else {
        parent_node = cwd;
    }

    int parent_count = get_dir_count(parent_node);

    int node_parent;
    std::vector<int> children;
    for (int i=0; i<NUM_INODES; i++) {
        node_parent = super_block->inode[i].dir_parent & ~(1 << 7);

        if (node_parent == cwd) {
            children.push_back(i);
        }
    }

    printf("%-5s %3d\n", ".", (int) children.size() + 2);
    printf("%-5s %3d\n", "..", parent_count);
    for (int i : children) {
        int type = (super_block->inode[i].dir_parent >> 7) & 1;
        std::string name = "";
        for (int j=0; j<5; j++) {
            name += super_block->inode[i].name[j];
        }
        if (type == 0) {
            int size = super_block->inode[i].used_size & ~(1 << 7);
            printf("%-5.5s %3d KB\n", super_block->inode[i].name, size);
        } else {
            printf("%-5.5s %3d\n", super_block->inode[i].name, get_dir_count(i));
        }
    }
}

void delete_node(int node_num) {
    int type = (super_block->inode[node_num].dir_parent >> 7) & 1;

    if (type == 1) {
        for (int i=0; i<NUM_INODES; i++) {
            int parent_node = super_block->inode[i].dir_parent & ~(1 << 7);
            if (parent_node == node_num) {
                delete_node(i);
            }
        }
    }

    super_block->inode[node_num].dir_parent = 0;
    strncpy(super_block->inode[node_num].name, "", 5);

    int start_index = super_block->inode[node_num].start_block;
    int size = super_block->inode[node_num].used_size & ~(1 << 7);

    if (start_index > 0) {
        for (int i=start_index; i<start_index+size; i++) {
            int list_index = floor(i/8);

            super_block->free_block_list[list_index] &= ~(1 << (7-(i%8)));
            disk.seekp(i*1024, std::ios::beg);
            disk.write((char *) &zero_buffer, 1024);
        }
    }

    super_block->inode[node_num].start_block = 0;
    super_block->inode[node_num].used_size = 0;
}

void fs_delete(char name[5]) { 
    for (int i=0; i<NUM_INODES; i++) {
        int parent_node = super_block->inode[i].dir_parent & ~(1 << 7);
        
        if (parent_node != cwd)
            continue;
        
        if (strncmp(name, super_block->inode[i].name, 5) != 0) {
            continue; 
        }

        delete_node(i);
        write_super_block();
        return;
    }

    std::cerr << "Error: File or directory " << name << " does not exist\n";
}

void fs_read(char name[5], int block_num) {
    for (int i=0; i<NUM_INODES; i++) {
        int parent_node = super_block->inode[i].dir_parent & ~(1 << 7);
        
        if (parent_node != cwd)
            continue;
        
        if (strncmp(name, super_block->inode[i].name, 5) != 0) {
            continue; 
        }

        int start_block = super_block->inode[i].start_block;
        int size = super_block->inode[i].used_size & ~(1 << 7);
 
        if (block_num >= size) {
            std::cerr << "Error: " << name << " does not have block " << block_num << std::endl; 
            return;
        }

        disk.seekg((start_block + block_num)*1024, std::ios::beg);
        disk.read((char *) buffer, 1024);

        return;
    }

    std::cerr << "Error: File or directory " << name << " does not exist\n";
}

void fs_cd(char name[5]) {
    if (strncmp(name, "..", 5) == 0) {
        if (cwd == 127) {
            return;
        }

        cwd = super_block->inode[cwd].dir_parent & ~(1 << 7);
        return;
    }

    if (strncmp(name, ".", 5) == 0) 
        return;
    
    for (int i=0; i<NUM_INODES; i++) {
        int parent_node = super_block->inode[i].dir_parent & ~(1 << 7);
        int type = (super_block->inode[i].dir_parent >> 7) & 1;

        if (parent_node == cwd and type == 1 and strncmp(name, super_block->inode[i].name, 5) == 0) {
            cwd = i;
            return;
        }
    }

    std::cerr << "Error: Directory " << name << " does not exist\n";
}

void move_start_block(int node_num, int new_size, int new_start_block) {
    uint8_t temp_buffer[1024];
    int old_start_block = super_block->inode[node_num].start_block;
    int old_size = super_block->inode[node_num].used_size & ~(1 << 7);

    for (int i=old_start_block; i<old_start_block+old_size; i++) {
        //Read the old block
        disk.seekg(i*1024, std::ios::beg);
        disk.read((char *) temp_buffer, 1024);

        //Zero out old block
        disk.seekp(i*1024, std::ios::beg);
        disk.write((char *) &zero_buffer, 1024);

        //Write old block to new block
        int new_index = new_start_block + (i - old_start_block);
        disk.seekp(new_index*1024, std::ios::beg);
        disk.write((char *) &temp_buffer, 1024);
    }

    //Update the free_list_block
    for (int i=new_start_block; i < new_start_block + new_size; i++) {
        int index = floor(i/8);
        super_block->free_block_list[index] |= (1 << (7 - (i%8)));
    }

    //Set the size and start_blocks now
    super_block->inode[node_num].start_block = new_start_block;
    super_block->inode[node_num].used_size = new_size;
    super_block->inode[node_num].used_size |= (1 << 7);
}

void reduce_size(int node_num, int new_size) {
    int start_block = super_block->inode[node_num].start_block;
    int size = super_block->inode[node_num].used_size & ~(1 << 7);

    for (int i=start_block+new_size; i<start_block + size; i++) {
        disk.seekp(i*1024, std::ios::beg);
        disk.write((char *) &zero_buffer, 1024);

        int index = floor(i/8);
        super_block->free_block_list[index] &= ~(1 << (7-(i%8)));
    }    

    super_block->inode[node_num].used_size = new_size;
    super_block->inode[node_num].used_size |= (1 << 7);
}

void extend_size(int node_num, int new_size) {
    int start_block = super_block->inode[node_num].start_block;
    int size = super_block->inode[node_num].used_size & ~(1 << 7);
    
    char name[5];
    strncpy(name, super_block->inode[node_num].name, 5);

    if (new_size >= 128) {
        std::cerr << "Error: File " << name << " cannot expand to size " << new_size << std::endl;
        return;
    }

    if (start_block + new_size >= 128) {
        //"Virtually" make the file disappear
        for (int j=start_block; j<start_block+size; j++) {
            int index = floor(j/8);
            super_block->free_block_list[index] &= ~(1 << (7-(j%8)));
        }

        int new_start_block = get_start_block(new_size);
        //No spot found, re-set the blocks to used and exit
        if (new_start_block == -1){
            for (int j=start_block; j<start_block+size; j++) {
                int index = floor(j/8);
                super_block->free_block_list[index] |= (1 << (7-(j%8)));
            }
            std::cerr << "Error: File " << name << " cannot expand to size " << new_size << std::endl;
        } else {
            move_start_block(node_num, new_size, new_start_block);
        }
        return;
    }

    for (int i=start_block+size; i < start_block + new_size; i++) {
        int end_index = floor(i/8);
        int end_block_state = (super_block->free_block_list[end_index] >> (7-(i%8))) & 1;

        if (end_block_state == 1){
            //"Virtually" make the file disappear
            for (int j=start_block; j<start_block+size; j++) {
                int index = floor(j/8);
                super_block->free_block_list[index] &= ~(1 << (7-(j%8)));
            }

            int new_start_block = get_start_block(new_size);
            //No spot found, re-set the blocks to used and exit
            if (new_start_block == -1){
                for (int j=start_block; j<start_block+size; j++) {
                    int index = floor(j/8);
                    super_block->free_block_list[index] |= (1 << (7-(j%8)));
                }
                std::cerr << "Error: File " << name << " cannot expand to size " << new_size << std::endl;
            } else {
                move_start_block(node_num, new_size, new_start_block);
            }
            return;
        }
    }

    //Can simply extend the current file since enough size can be extended
    for (int i=start_block+size; i < start_block + new_size; i++) {
        int index = floor(i/8);
        super_block->free_block_list[index] |= (1 << (7-(i%8)));
    }

    super_block->inode[node_num].used_size = new_size;
    super_block->inode[node_num].used_size |= (1 << 7);
}

void fs_resize(char name[5], int new_size) {
    for (int i=0; i<NUM_INODES; i++) {
        if (strncmp(name, super_block->inode[i].name, 5) != 0)
            continue;

        int parent_node = super_block->inode[i].dir_parent & ~(1 << 7);
        if (parent_node != cwd)
            continue;
        
        int type = (super_block->inode[i].dir_parent >> 7) & 1;
        if (type == 1) {
            std::cerr << "Error: File " << name << " does not exist\n";
            return;
        }

        int size = super_block->inode[i].used_size & ~(1 << 7);
        if (new_size > size) {
            extend_size(i, new_size);
        } else if (new_size < size) {
            reduce_size(i, new_size);
        }

        write_super_block();
        return;
    }
    std::cerr << "Error: File " << name << " does not exist\n";
}

void fs_defrag() { 
    //Sort by start block
    std::map<int, int> sorted_nodes;

    for (int i=0; i<NUM_INODES; i++) {
        int start_block = super_block->inode[i].start_block;
        if (start_block == 0)
            continue;

        sorted_nodes[start_block] = i;
    }
    
    if (sorted_nodes.empty())
        return;

    std::map<int,int>::iterator it=sorted_nodes.begin();
    
    int i=1;
    while(i < 128 and it != sorted_nodes.end()) {
        int start_index = floor(i/8);
        int block_state = (super_block->free_block_list[start_index] >> (7-(i%8))) & 1;

        int node_num = it->second;
        int size = super_block->inode[node_num].used_size & ~(1 << 7);
        int old_start_block = super_block->inode[node_num].start_block;

        if (i <= old_start_block) {
            if (block_state == 1) {
                i++;
            } else {
                for (int j=old_start_block; j<old_start_block+size; j++) {
                    int index = floor(j/8);
                    super_block->free_block_list[index] &= ~(1 << (7-(j%8)));
                }

                move_start_block(node_num, size, i);
                write_super_block();
                it++;
                i += size;
            }
        } else {
            it++;
        }
    }
}

void execute_command(std::string command, int line_number) {
    std::vector<std::string> result;
    std::vector<std::string> empty_vector;

    if (command.size() < 1){
        return;
    }

    result = parse_command(command);

    int expected_length = 0;
    if (command[0] == 'M')
        expected_length = 2;
    else if (command[0] == 'C')
        expected_length = 3;    
    else if (command[0] == 'D')
        expected_length = 2;    
    else if (command[0] == 'R')
        expected_length = 3;    
    else if (command[0] == 'W')
        expected_length = 3;    
    else if (command[0] == 'L')
        expected_length = 1;    
    else if (command[0] == 'E')
        expected_length = 3;    
    else if (command[0] == 'O')
        expected_length = 1;    
    else if (command[0] == 'Y')
        expected_length = 2;    

    if (expected_length != (int) result.size() and command[0] != 'B'){
        std::cerr << "Command Error: " << input_file_name << ", " << line_number+1 << std::endl;
        return;
    }

    if (result[0].compare("C") == 0) {
        std::string file_name = result[1];
        int size = std::stoi(result[2]);

        if (file_name.size() > 5 or (size < 0) or (size > 127)) {
            std::cerr << "Command Error: " << input_file_name << ", " << line_number+1 << std::endl;
            return;
        }
    } else if (result[0].compare("D") == 0) {
        std::string file_name = result[1];
        if (file_name.size() > 5) {
            std::cerr << "Command Error: " << input_file_name << ", " << line_number+1 << std::endl;
            return;
        }
    } else if (result[0].compare("R") == 0) {
        std::string file_name = result[1];
        int block_num = std::stoi(result[2]);

        if (file_name.size() > 5 or (block_num < 0) or (block_num > 127)) {
            std::cerr << "Command Error: " << input_file_name << ", " << line_number+1 << std::endl;
            return;
        }
    } else if (result[0].compare("W") == 0) {
        std::string file_name = result[1];
        int block_num = std::stoi(result[2]);

        if (file_name.size() > 5 or (block_num < 0) or (block_num > 127)) {
            std::cerr << "Command Error: " << input_file_name << ", " << line_number+1 << std::endl;
            return;
        }
    } else if (result[0].compare("B") == 0) {
        //Nothing in buffer
        if (command.size() == 1) {
            std::cerr << "Command Error: " << input_file_name << ", " << line_number+1 << std::endl;
            return;
        }
    } else if (result[0].compare("E") == 0) {
        std::string file_name = result[1];
        int size = std::stoi(result[2]);

        if (file_name.size() > 5 or (size < 0) or (size > 127)) {
            std::cerr << "Command Error: " << input_file_name << ", " << line_number+1 << std::endl;
            return;
        }
    }
    
    if (result[0].compare("M") == 0){
        fs_mount((char *) result[1].c_str());
    } else if (disk.is_open()) {
        if (result[0].compare("C") == 0)
            fs_create((char *) result[1].c_str(), std::stoi(result[2]));
        else if (result[0].compare("R") == 0)
            fs_read((char *) result[1].c_str(), std::stoi(result[2]));
        else if (result[0].compare("W") == 0)
            fs_write((char *) result[1].c_str(), std::stoi(result[2]));
        else if (result[0].compare("B") == 0){
            uint8_t buff[1024] = { 0 };
            strcpy((char *) buff, command.substr(2, std::string::npos).c_str());
            fs_buff(buff);
        }
        else if (result[0].compare("L") == 0)
            fs_ls();    
        else if (result[0].compare("E") == 0)
            fs_resize((char *) result[1].c_str(), std::stoi(result[2]));    
        else if (result[0].compare("O") == 0)
            fs_defrag();
        else if (result[0].compare("Y") == 0)
            fs_cd((char *) result[1].c_str());
        else if (result[0].compare("D") == 0)
            fs_delete((char *) result[1].c_str());
    } else {
        std::cerr << "Error: No file system is mounted\n";
    }
}

int main(int argc, char const *argv[])
{
    input_file_name = argv[1];

    std::string command;

    input_file.open(input_file_name, std::ios::in);
    
    int line_number = 0;
    while (std::getline(input_file, command))
    {
        execute_command(command, line_number);
        
        line_number++;
    }
    
    if (disk_name != "")
        close_disk();
    return 0;
}
