/**
 * @author Yunfan Long (A16665173), Muchan Li (A15887258)
 * @author Additional information regarding authorship and licensing are available in Supplementary.txt
**/

#include <iostream>
#include <memory>

#include "pagebuffer.h"
#include "exceptions_header.h"


namespace badgerdb {

	PageBufferManager::PageBufferManager(std::uint32_t buffers)
		: numBufs(buffers) {
		bufferStatTable = new BufferStatus[buffers];

		for (FrameId i = 0; i < buffers; i++) 
		{
			bufferStatTable[i].frameNo = i;
			bufferStatTable[i].valid = false;
		}

	  	pageBufferPool = new Page[buffers];

		int htsize = ((((int) (buffers * 1.2))*2)/2)+1;
	  	hashTable = new BufHashTbl (htsize);  // allocate the buffer hash table

	  	clockHand = buffers - 1;
	}


	PageBufferManager::~PageBufferManager() 
	{
		//BEGINNING of your solution -- do not remove this comment
		for (FrameId i = 0; i < numBufs; i++) {
			if (bufferStatTable[i].dirty) {
				bufferStatTable[i].file->writePage(pageBufferPool[i]);
				bufStats.diskwrites++;
			}
		}

		delete[] bufferStatTable;
		delete[] pageBufferPool;
		delete hashTable;
		//END of your solution -- do not remove this comment
	}

	void PageBufferManager::readPage(File* file, const PageId pageNumber, Page*& page)
	{
		//BEGINNING of your solution -- do not remove this comment
		FrameId frame;
		try {
			hashTable->lookup(file, pageNumber, frame);
			bufferStatTable[frame].pinCnt++;
			bufferStatTable[frame].refbit = true;
			page = &pageBufferPool[frame];
		} catch (HashNotFoundException& e) {
			allocateBuffer(frame);
			pageBufferPool[frame] = file->readPage(pageNumber);
			bufStats.diskreads++;
			hashTable->insert(file, pageNumber, frame);
			bufferStatTable[frame].Set(file, pageNumber);
			page = &pageBufferPool[frame];
		}
		//END of your solution -- do not remove this comment
	}

	void PageBufferManager::allocatePage(File* file, PageId &pageNumber, Page*& page) 
	{
		//BEGINNING of your solution -- do not remove this comment
		Page newPage = file->allocatePage();
		pageNumber = newPage.page_number();
		FrameId frame;
		allocateBuffer(frame);
		pageBufferPool[frame] = newPage;
		hashTable->insert(file, pageNumber, frame);
		bufferStatTable[frame].Set(file, pageNumber);
		page = &pageBufferPool[frame];
		//END of your solution -- do not remove this comment
	}

	void PageBufferManager::unPinPage(File* file, const PageId pageNumber, const bool dirty) 
	{
		//BEGINNING of your solution -- do not remove this comment
		FrameId frame;
		try {
			hashTable->lookup(file, pageNumber, frame);
			if (bufferStatTable[frame].pinCnt > 0) {
				bufferStatTable[frame].pinCnt--;
				if (dirty){
					bufferStatTable[frame].dirty = dirty;
				}
			} else {
				throw PageNotPinnedException(file->filename(), pageNumber, frame);
			}
		} catch (HashNotFoundException& e) {
			// Do nothing if page is not found in the hash table.
		}
		//END of your solution -- do not remove this comment
	}

	void PageBufferManager::disposePage(File* file, const PageId pageNumber)
	{
		//BEGINNING of your solution -- do not remove this comment
		FrameId frameNo = -1;
		hashTable->lookup(file, pageNumber, frameNo);
		if (frameNo != -1) {
			bufferStatTable[frameNo].Clear();
			hashTable->remove(file, pageNumber);
		}
		file->deletePage(pageNumber);
		//END of your solution -- do not remove this comment
	}

	void PageBufferManager::advanceClock()
	{
		//BEGINNING of your solution -- do not remove this comment
		clockHand = (clockHand + 1) % numBufs;
		//END of your solution -- do not remove this comment
	}

	void PageBufferManager::allocateBuffer(FrameId & frame) 
	{
		// BEGINNING of your solution -- do not remove this comment
		int numAttempts = 0; // Count the number of attempts to allocate a buffer

		while (true) {
			// Step 1: Advance Clock Pointer
			advanceClock();

			// Step 2: Check if frame is valid
			if (!bufferStatTable[clockHand].valid) {
				// Step 6: Call Set() on Frame
				frame = clockHand;
				break;
			} else {
				// Step 3: Check refbit
				if (bufferStatTable[clockHand].refbit) {
					bufferStatTable[clockHand].refbit = false;
					continue;
				} else {
					// Step 4: Check if page is pinned
					if (bufferStatTable[clockHand].pinCnt == 0) {
						// Step 5: Check dirty bit
						if (bufferStatTable[clockHand].dirty) {
							bufferStatTable[clockHand].file->writePage(pageBufferPool[clockHand]);
							bufStats.diskwrites++;
							bufferStatTable[clockHand].dirty = false;
						}
						// Step 6: Call Set() on Frame
						try {
							hashTable->remove(bufferStatTable[clockHand].file, bufferStatTable[clockHand].pageNo);
						} catch (HashNotFoundException e) {
							
						}
						frame = clockHand;
						break;
					}
				}
			}

			// Increment the number of attempts
			numAttempts++;

			if (numAttempts >= numBufs) {
				// If all buffer frames are pinned, throw a BufferExceededException
				throw BufferExceededException();
			}
		}
		// Step 7: Use Frame
		//END of your solution -- do not remove this comment
	}

	void PageBufferManager::flushFile(const File* file) 
	{
		//BEGINNING of your solution -- do not remove this comment
		for (FrameId i = 0; i < numBufs; i++) {
			if (bufferStatTable[i].file == file) {
				if (!bufferStatTable[i].valid) {
					throw BadBufferException(i, bufferStatTable[i].dirty, bufferStatTable[i].pinCnt, bufferStatTable[i].refbit);
				}
				if (bufferStatTable[i].pinCnt > 0) {
					throw PagePinnedException(file->filename(), bufferStatTable[i].pageNo, i);
				}
				if (bufferStatTable[i].dirty) {
					bufferStatTable[i].file->writePage(pageBufferPool[i]);
					bufStats.diskwrites++;
					bufferStatTable[i].dirty = false;
				}
				hashTable->remove(file, bufferStatTable[i].pageNo);
				bufferStatTable[i].Clear();
			}
		}
		//END of your solution -- do not remove this comment
	}

	void PageBufferManager::printSelf(void) 
	{
	  	BufferStatus* tempPageBuffer;
		int validFrames = 0;
	  
	  for (std::uint32_t i = 0; i < numBufs; i++)
		{
	  		tempPageBuffer = &(bufferStatTable[i]);
			std::cout << "FrameNo:" << i << " ";
			tempPageBuffer->Print();

	  	if (tempPageBuffer->valid == true)
	    	validFrames++;
	  	}

		std::cout << "Total Number of Valid Frames:" << validFrames << "\n";
	}

}
