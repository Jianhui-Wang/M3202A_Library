using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CSharpConsoleApp
{
    interface IDDRMemoryManager
    {
        /// <summary>
        /// Returns largest contiguous free space in bytes.
        /// </summary>
        ulong MaxFreeBlock { get; }

        /// <summary>
        /// Returns free memory in bytes.
        /// </summary>
        ulong Available { get; }

        /// <summary>
        /// Allocates memory in the Measurement Accelerator FPGA module and returns a handle to it.
        /// </summary>

        /// <param name="Size">Size in bytes</param>
        /// <param name="IsP2PAlign">If true, allocate with Address alignment to support Peer to Peer transfer</param>
        /// <returns>Returns a memory block object containing the Address and related properties.  </returns>
        IDDRMemoryBlock Allocate(ulong size, bool isP2PAlign);

        /// <summary>
        /// Frees a memory block previously allocated by Allocate().
        /// </summary>
        void Free(ulong address);

        /// <summary>
        /// Dump memory allocation for debug
        /// </summary>
        void Dump();
    }
}
