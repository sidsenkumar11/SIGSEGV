#include "StackBasedBlockManager.h"
#include <iostream>

// Anonymous namespace for file-local types:
namespace {
  struct DatablockNode {
    static const int NREFS = (Block::BLOCK_SIZE / sizeof(Block::ID));
    Block::ID free_blocks[NREFS];
  };

  struct Config {
    uint64_t  magic; // Magic number to identify this block manager (ignored for now)
    Block::ID block; // Block ID of the top block on the stack
    uint64_t  index; // Index of the first free ref in that block
    uint64_t  last_block;
    uint64_t  first_block;
  };
}

StackBasedBlockManager::StackBasedBlockManager(Storage& storage): disk(&storage) {
  Block block;
  Superblock* superblock = (Superblock*) &block;
  Config* config = (Config*) superblock->data_config;
  this->disk->get(0, block);

  this->top_block_num = config->block;
  this->index         = config->index;
  this->last_block    = config->last_block;
  this->first_block    = config->first_block;
}

StackBasedBlockManager::~StackBasedBlockManager() {}


void StackBasedBlockManager::mkfs() {

  // Read superblock
  Block block;
  this->disk->get(0,block);
  Superblock* superblock = (Superblock*) &block;

  // Setting up the start and count of blocks from superblock
  // this should be setup before calling mkfs()
  Block::ID start = superblock->data_block_start;
  uint64_t count  = superblock->data_block_count;

  // The first available free block
  Block::ID free_block = start + count - 1;
  DatablockNode* data = (DatablockNode*) &block;
  memset(&block, 0, Block::BLOCK_SIZE);

  //debugging statements
  std::cout << "-------------\nWithin StackBasedBlockManager::mkfs()\n-------------\n";
  std::cout << "\nCurrent free_block is: " << free_block << std::endl;

  // create the free_blocks[] list
  Block::ID curr = start;
  while(true) {

    for(size_t i = DatablockNode::NREFS - 1; i >= 0; --i) {
      std::cout << "Current free_block is: " << free_block << std::endl;

      // If there is a collision
      // exit condition
      if(free_block == curr) {

        // Write what we've got so far
        this->disk->set(free_block, block);

        // Update superblock with true number of datablocks
        this->disk->get(0, block);
        superblock->data_block_count = superblock->data_block_count - free_block + 1;
        this->disk->set(0, block);
        return;
      }

      //otherwise set the first free_block into the free block list
      data->free_blocks[i] = curr;
      curr++;
    }

    // write the block to disk
    this->disk->set(free_block, block);
    free_block--;
  }
}

void StackBasedBlockManager::update_superblock() {
  Block block;
  Superblock* superblock = (Superblock*) &block;
  Config* config = (Config*) superblock->data_config;

  this->disk->get(0, block);
  config->block = this->top_block_num;
  config->index = this->index;
  this->disk->set(0, block);
}

void StackBasedBlockManager::release(Block::ID free_block_num) {
  // Read top block in freelist from disk
  Block block;
  DatablockNode *node = (DatablockNode *) &block;

  // If insertion causes index to overflow the block,
  // move to previous block in free list.
  if (this->index + 1 >= DatablockNode::NREFS) {
    if (this->top_block_num == this->last_block) {
      throw std::out_of_range("Can't insert block at top of data block free list!");
    }

    this->top_block_num = this->top_block_num + 1;
    this->index = 0;
  } else {
    this->index++;
  }

  // Update the free list
  this->disk->get(this->top_block_num, block);
  node->free_blocks[this->index] = free_block_num;
  this->disk->set(this->top_block_num, block);
  this->update_superblock();
}

Block::ID StackBasedBlockManager::reserve() {

  // Read top block
  Block block;
  DatablockNode *node = (DatablockNode *) &block;

  // Check if free list is almost empty and refuse allocation of last block
  if (this->index == 0 && this->top_block_num == this->first_block) {
    throw std::out_of_range("Can't get any more free blocks - free list is empty!");
  }

  // Get next free block
  this->disk->get(this->top_block_num, block);
  Block::ID free_block_num = node->free_blocks[this->index];
  if (this->index == 0) {
    this->index = DatablockNode::NREFS - 1;
    this->top_block_num--;
  } else {
    this->index--;
  }

  this->update_superblock();
  return free_block_num;
}
