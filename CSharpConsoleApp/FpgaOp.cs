using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace CSharpConsoleApp
{
    class FpgaOp
    {
        [DllImport("M3202A_Library.dll", CallingConvention = CallingConvention.Cdecl, EntryPoint = "RegRead")]
        public static extern int RegRead(ref UInt32 data, UInt64 address);

        [DllImport("M3202A_Library.dll", CallingConvention = CallingConvention.Cdecl, EntryPoint = "RegWrite")]
        public static extern int RegWrite(UInt64 address, UInt32 value);

        [DllImport("M3202A_Library.dll", CallingConvention = CallingConvention.Cdecl, EntryPoint = "RegArrayRead")]
        public static extern int RegArrayRead(IntPtr dataPointer, UInt64 address, UInt32 length);

        public static int RegArrayRead(ref UInt32[] data, UInt64 address, UInt32 length)
        {
            data = new UInt32[length];
            IntPtr pd = Marshal.AllocHGlobal((int)(sizeof(int) * length));
            IntPtr pv = pd; 
            var ret = FpgaOp.RegArrayRead(pd, address, length);
            int[] temp = (int[])(object)data;
            Marshal.Copy(pd, temp, 0, (int)length);
            Marshal.FreeHGlobal(pv);
            return ret;
        }

        [DllImport("M3202A_Library.dll", CallingConvention = CallingConvention.Cdecl, EntryPoint = "RegArrayWrite")]
        public static extern int RegArrayWrite(UInt32[]data, UInt64 address, UInt32 length);

        public int[] StreamRead(int streamIdx, int length) { return null; }
        public void StreamWrite(int streamIdx, int[] data, int length) {}

        [DllImport("M3202A_Library.dll", CallingConvention = CallingConvention.Cdecl, EntryPoint = "DdrRead")]
        public static extern int DdrRead(UInt32[] data, UInt64 address, UInt32 length);

        [DllImport("M3202A_Library.dll", CallingConvention = CallingConvention.Cdecl, EntryPoint = "DdrWrite")]
        public static extern int DdrWrite(UInt32[] data, UInt64 address, UInt32 length);

        [DllImport("M3202A_Library.dll", CallingConvention = CallingConvention.Cdecl, EntryPoint = "DdrCopy")]
        public static extern int DdrCopy(UInt64 srcAddr, UInt64 destAddr, UInt32 length);
    }
}
