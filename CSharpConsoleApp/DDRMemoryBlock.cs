using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CSharpConsoleApp
{
    /// <summary>
    /// A block of memory allocated in the measurement accelerator module DRAM.
    /// </summary>
    internal class DDRMemoryBlock : IDDRMemoryBlock
    {

        internal DDRMemoryBlock(ulong address, ulong sizeInBytes, bool isP2PAligned)
        {
            Address = address;
            SizeInBytes = sizeInBytes;
            IsP2PAligned = isP2PAligned;
        }

        #region IMMAccMemoryBlock implementation 

        /// <summary>
        /// Starting Address, in the module's local DRAM Address space. Bytes.
        /// </summary>
        public ulong Address { get; private set; }

        /// <summary>
        /// Allocated size in bytes.
        /// </summary>
        public ulong SizeInBytes { get; private set; }

        /// <summary>
        /// True if aligned to a DRAM window allowing Peer-to-Peer transfer to/from peer master module.  
        /// </summary>
        public bool IsP2PAligned { get; private set; }


        /// <summary>
        /// Reserved for application use.
        /// </summary>
        [Browsable(false)]
        [EditorBrowsable(EditorBrowsableState.Never)]
        public object Tag { get; set; }     // DPD uses this for additional protection of licensed data



        #endregion
    }
}
