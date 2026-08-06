//
// alloc.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2021  R. Stange <rsta2@o2online.de>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include <circle/alloc.h>
#include <circle/memory.h>
#include <circle/sysconfig.h>
#include <circle/util.h>
#include <assert.h>

// The heap allocator serves any request from a fixed ladder of bucket sizes
// (see HEAP_BLOCK_BUCKET_SIZES in sysconfig.h) and can only reuse a freed block
// for a future allocation that rounds to the SAME bucket - anything requested at
// its raw, unrounded byte count spreads unrelated callers across many buckets and
// each new distinct size permanently claims a fresh slice of the heap arena.
// This mainly bites large, size-varying allocations (ROM/save-state buffers, CD
// image hunk/codec buffers, etc.) coming from many different call sites across
// this project's five emulator cores and their third-party CD-image libraries -
// rounding those up here, in one place, means callers that ask for similar sizes
// share a bucket instead of each opening a new one. Small allocations (below the
// threshold below) are left untouched: Circle's own drivers (USB/SD/sound/etc.)
// never request blocks this large, so this only affects game-data-sized requests.
static size_t RoundLargeAllocation (size_t nSize)
{
	static const size_t s_Sizes[] = {
		0x40000,  // 256 KB
		0x80000,  // 512 KB
		0x100000, // 1 MB
		0x200000, // 2 MB
		0x400000, // 4 MB
		0x800000, // 8 MB
	};

	if (nSize < 0x20000)		// below 128 KB: leave as-is
	{
		return nSize;
	}

	for (unsigned i = 0; i < sizeof s_Sizes / sizeof s_Sizes[0]; i++)
	{
		if (nSize <= s_Sizes[i])
		{
			return s_Sizes[i];
		}
	}

	return nSize;
}

void *malloc (size_t nSize)
{
	return CMemorySystem::HeapAllocate (RoundLargeAllocation (nSize), HEAP_DEFAULT_MALLOC);
}

void *memalign (size_t nAlign, size_t nSize)
{
	assert (nAlign <= HEAP_BLOCK_ALIGN);
	return CMemorySystem::HeapAllocate (RoundLargeAllocation (nSize), HEAP_DEFAULT_MALLOC);
}

void free (void *pBlock)
{
	CMemorySystem::HeapFree (pBlock);
}

void *calloc (size_t nBlocks, size_t nSize)
{
	nSize *= nBlocks;
	if (nSize == 0)
	{
		nSize = 1;
	}
	assert (nSize >= nBlocks);

	void *pNewBlock = CMemorySystem::HeapAllocate (RoundLargeAllocation (nSize), HEAP_DEFAULT_MALLOC);
	if (pNewBlock != 0)
	{
		memset (pNewBlock, 0, nSize);
	}

	return pNewBlock;
}

void *realloc (void *pBlock, size_t nSize)
{
	return CMemorySystem::HeapReAllocate (pBlock, RoundLargeAllocation (nSize));
}

void *palloc (void)
{
	return CMemorySystem::PageAllocate ();
}

void pfree (void *pPage)
{
	CMemorySystem::PageFree (pPage);
}
