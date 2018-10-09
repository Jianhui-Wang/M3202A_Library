using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CSharpConsoleApp
{
    interface IDDRMemoryBlock
    {
        /// <summary>
        /// Starting Address, in the module's local DRAM Address space. Bytes.
        /// </summary>
        UInt64 Address { get; }

        /// <summary>
        /// Allocated size in bytes.
        /// </summary>
        UInt64 SizeInBytes { get; }

        /// <summary>
        /// True if aligned to a DRAM window allowing Peer-to-Peer transfer to/from peer master module.  
        /// </summary>
        bool IsP2PAligned { get; }

        /// <summary>
        /// Reserved for application use.
        /// </summary>
        [Browsable(false)]
        [EditorBrowsable(EditorBrowsableState.Never)]
        object Tag { get; set; }
    }
}
