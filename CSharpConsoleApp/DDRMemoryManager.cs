using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CSharpConsoleApp
{
    class DDRMemoryManager : IDDRMemoryManager
    {
        private ulong _mBaseAddress;
        private ulong _mTotalSizeInBytes;
        private ulong _mMaxFreeBlockBytes;
        private ulong _mAvailableBytes;

        private const long MinAlignment = 8; // keep blocks aligned to a 8 bytes
        private const long P2PAlignment = 0x02000000;
        private bool _mStatsNeedUpdate;
        private SortedList<ulong, IDDRMemoryBlock> _mAllocatedBlocks;

        public ulong MaxFreeBlock
        {
            get
            {
                if (_mStatsNeedUpdate)
                {
                    CalculateStats();
                }
                return _mMaxFreeBlockBytes;
            }
        }
        public ulong Available {
            get
            {
                if (_mStatsNeedUpdate)
                {
                    CalculateStats();
                }
                return _mAvailableBytes;
            }
        }

        /// <summary>
        /// align input x to size given by alignment  
        /// </summary>
        private ulong Align(ulong x, ulong alignment)
        {
            if (x == _mBaseAddress) return _mBaseAddress;

            ulong remainder = (x % alignment);
            if (remainder == 0) return x;
            return x + (alignment - remainder);
        }

        private IDDRMemoryBlock AddNewBlock(ulong address, ulong sizeInBytes, bool isP2PAligned)
        {
            var newBlock = new DDRMemoryBlock(address, sizeInBytes, isP2PAligned);
            _mAllocatedBlocks.Add(newBlock.Address, newBlock);
            _mStatsNeedUpdate = true;

            return newBlock;
        }

        /// <summary>
        /// Update available memory stats.
        /// </summary>
        private void CalculateStats()
        {
            // see if it will fit after an existing block
            int blockIndex = 0;
            ulong freeSpaceBytesTotal = 0;
            ulong maxBlockBytes = 0;

            if (_mAllocatedBlocks.Count == 0)
            {
                freeSpaceBytesTotal = maxBlockBytes = _mTotalSizeInBytes;
            }
            else
            {
                IDDRMemoryBlock block = _mAllocatedBlocks.Values[0];
                freeSpaceBytesTotal = maxBlockBytes = block.Address - _mBaseAddress;

                foreach (var kvp in _mAllocatedBlocks)
                {
                    block = kvp.Value;
                    ulong freeSpaceStart = block.Address + block.SizeInBytes;

                    blockIndex++;
                    ulong freeSpaceBytes;
                    if (blockIndex == _mAllocatedBlocks.Count)
                    {
                        // in this case, next block is past the end of memory
                        freeSpaceBytes = _mTotalSizeInBytes + _mBaseAddress - freeSpaceStart;
                    }
                    else
                    {
                        IDDRMemoryBlock nextBlock = _mAllocatedBlocks.Values[blockIndex];
                        freeSpaceBytes = nextBlock.Address - freeSpaceStart;
                    }

                    freeSpaceBytesTotal += freeSpaceBytes;
                    if (freeSpaceBytes > maxBlockBytes)
                    {
                        maxBlockBytes = freeSpaceBytes;
                    }

                    if (blockIndex == _mAllocatedBlocks.Count)
                    {
                        break; // error - no room
                    }
                }
            }
            _mAvailableBytes = freeSpaceBytesTotal;
            _mMaxFreeBlockBytes = maxBlockBytes;
            _mStatsNeedUpdate = false;
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="totalSizeInBytes">The total Size in bytes</param>
        public DDRMemoryManager(ulong baseAddress, ulong totalSizeInBytes)
        {
            _mTotalSizeInBytes = totalSizeInBytes;
            _mAllocatedBlocks = new SortedList<ulong, IDDRMemoryBlock>();
            _mBaseAddress = baseAddress;
            _mMaxFreeBlockBytes = _mAvailableBytes = _mTotalSizeInBytes;
            if (_mBaseAddress % MinAlignment != 0)
                throw new Exception("Base address invalid, must be 8-byte aligned!");
        }

        /// <summary>
        /// Allocates memory in the Measurement Accelerator FPGA module and returns a handle to it.
        /// </summary>
        /// <param name="sizeInBytes">Size in bytes</param>
        /// <param name="isP2PAligned">If true, allocate with Address alignment to support Peer to Peer transfer</param>
        /// <returns>Returns a memory block object containing the Address and related properties.  </returns>
        public IDDRMemoryBlock Allocate(ulong sizeInBytes, bool isP2PAligned)
        {
            if (sizeInBytes == 0)
            {
                throw new Exception("Invalid size argument.");
            }

            // adjust sizeInBytes to minimum alignment
            sizeInBytes = Align(sizeInBytes, MinAlignment);

            if (_mAllocatedBlocks.Count == 0)
            {
                if (sizeInBytes > _mTotalSizeInBytes)
                {
                    throw new Exception("Cannot allocate specified block of memory.");
                }

                return AddNewBlock(_mBaseAddress, sizeInBytes, isP2PAligned);
            }
            else
            {
                // see if there is room at the top of memory
                IDDRMemoryBlock block = _mAllocatedBlocks.Values[0];
                if (sizeInBytes <= (block.Address - _mBaseAddress))
                {
                    return AddNewBlock(_mBaseAddress, sizeInBytes, isP2PAligned);
                }
            }

            // see if it will fit after an existing block
            int blockIndex = 0;
            foreach (var kvp in _mAllocatedBlocks)
            {
                var block = kvp.Value;
                ulong putativeStartAddress = block.Address + block.SizeInBytes;
                if (isP2PAligned)
                {
                    putativeStartAddress = Align(putativeStartAddress, P2PAlignment);
                }

                blockIndex++;
                ulong addressOfNextBlock;
                if (blockIndex == _mAllocatedBlocks.Count)
                {
                    // in this case, next block is past the end of memory
                    addressOfNextBlock = _mTotalSizeInBytes + _mBaseAddress;
                }
                else
                {
                    IDDRMemoryBlock nextBlock = _mAllocatedBlocks.Values[blockIndex];
                    addressOfNextBlock = nextBlock.Address;
                }

                if (((putativeStartAddress + sizeInBytes) <= addressOfNextBlock))
                {
                    return AddNewBlock(putativeStartAddress, sizeInBytes, isP2PAligned);
                }

                if (blockIndex == _mAllocatedBlocks.Count)
                {
                    break; // error - no room
                }
            }

            throw new Exception("Cannot allocate specified block of memory.");

        }

        /// <summary>
        /// Frees a memory block previously allocated by Allocate().
        /// </summary>
        public void Free(ulong address)
        {
            if (!_mAllocatedBlocks.ContainsKey(address))
            {
                throw new Exception("Allocated memory block not found.");
            }
            _mStatsNeedUpdate = true;
            _mAllocatedBlocks.Remove(address);
        }

        /// <summary>
        /// Dump the Memory blocks that managed by Memory manager, this is debug function
        /// </summary>
        public void Dump()
        {
            Console.WriteLine("===================================================");
            foreach (KeyValuePair<ulong, IDDRMemoryBlock> kvp in _mAllocatedBlocks)
            {
                IDDRMemoryBlock b = kvp.Value;
                ulong endAddress = b.Address + b.SizeInBytes - 1;
                Console.WriteLine("{0:X8} ({0,4}) - {1:X8} ({1,4}) ({2} bytes); isAligned = {3}", b.Address, endAddress, b.SizeInBytes, b.IsP2PAligned);
            }
            Console.WriteLine("Available memory: {0} bytes; Max Block: {1} bytes", Available, MaxFreeBlock);
            Console.WriteLine("===================================================\n");
        }

    }
}
