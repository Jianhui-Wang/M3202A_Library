using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace CSharpConsoleApp
{

    class Program
    {
        [DllImport("M3202A_Library.dll", CallingConvention = CallingConvention.Cdecl, EntryPoint = "SessionClose")]
        public static extern void SessionClose();

        [DllImport("M3202A_Library.dll", CallingConvention = CallingConvention.Cdecl, EntryPoint = "SessionOpen")]
        public static extern void SessionOpen();

        [DllImport("M3202A_Library.dll", CallingConvention = CallingConvention.Cdecl, EntryPoint = "ShowAddressMap")]
        public static extern void ShowAddressMap();

        static void TestMemory()
        {
            DDRMemoryManager memMgr = new DDRMemoryManager(0x10000000, 256 * 1024 * 1024);
            var b1 = memMgr.Allocate(10, true);
            memMgr.Dump(); Console.ReadKey();
            var b2 = memMgr.Allocate(100000, true);
            memMgr.Dump(); Console.ReadKey();
            memMgr.Free(b2.Address);
            memMgr.Dump(); Console.ReadKey();
        }

        static void TestRegOperation()
        {
            UInt32 myInt = 0;
            var a = FpgaOp.RegWrite(0, 94);
            var b = FpgaOp.RegRead(ref myInt, 0);

            uint[] dataIn = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
            uint[] dataOut = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

            var c = FpgaOp.RegArrayWrite(dataIn, 0x100, 10);
            var d = FpgaOp.RegArrayRead(ref dataOut, 0x100, 10);
        }

        static void TestDDR()
        {
            const uint dataLength = 16*1024;
            const UInt32 srcAddr = 0x10000000;
            const UInt32 destAddr = 0x20000000;
            int i;

            UInt32[] dataIn = new UInt32[dataLength];
            for (i = 0; i < dataLength; i++) dataIn[i] = (UInt32)i;
            FpgaOp.DdrWrite(dataIn, srcAddr, dataLength);

            // DMA copy
            FpgaOp.DdrCopy(srcAddr, destAddr, dataLength);

            UInt32[] dataOut = new UInt32[dataLength];
            for (i = 0; i < dataLength; i++) dataOut[i] = 0;
            FpgaOp.DdrRead(dataOut, destAddr, dataLength);

            for (i=0; i<dataLength; i++)
            {
                if (dataIn[i] != dataOut[i]) break;
            }

            if (i == dataLength)
                Console.WriteLine("DDR Test Passed!");
            else
                Console.WriteLine("DDR Test Failed!");
        }

        static double GetMaxMagnitude(double[] data)
        {
            double max = 0.0;
            for (int i = 0; i < data.Length / 2; i++)
            {
                double magSq = data[i * 2 + 0] * data[i * 2 + 0] + data[i * 2 + 1] * data[i * 2 + 1];
                if (magSq > max)
                    max = magSq;
            }
            return Math.Sqrt(max);
        }

        /// <summary>
        /// Convert an array of floating point real-world values into fixed-point MMAcc integer data, 
        /// using current MmAccDataFormat and either auto-scaling or using MmAccScaleFactor.
        /// </summary>
        static short[] ScaleToFixedPoint(Array data, bool autoScaleEnabled, ref double scaleFactor)
        {
            if (!autoScaleEnabled)
            {
                if (scaleFactor < 1.0e-64)
                    throw new Exception("Data scaling requires non-zero scaleFactor.");
            }

            double v;
            short[] fixedPointData = new short[data.Length];
            double[] array = data as double[];

            if (array == null)
                throw new Exception("Data scaling requires double[] array type.");

            // largest integer (fixed-point) value for the data format, as a double.
            double maxFixedPoint = Math.Pow(2, 15) - 1;

            if (autoScaleEnabled)
            {
                // get scale factor to just fit peak within fixed-point numeric range
                double maxMag = GetMaxMagnitude(array);
                if (maxMag > 0.0)
                    scaleFactor = maxMag / maxFixedPoint;
                else
                    scaleFactor = 1.0;
            }
            double factor = 1.0 / scaleFactor;

            for (int i = 0; i < data.Length; i++)
            {
                v = array[i];
                // 32-bit values, 24 bits used for data - pack 1 IQ pair per 64 bit value
                // can accept marks in the low 4 bits of the 64 bit value.
                // From $/MPO/Apogee/branches/ApogeeS/Apogee/software/src/RfSource/MoonstoneAPI/SourceArb.cs
                v = Math.Round(v * factor);
                if ((v > maxFixedPoint) || (v < -maxFixedPoint))
                {
                    throw new Exception(String.Format(
                        "Numeric overflow during scaling to fixed-point. Value {0} at index {1} exceeds 2^15 when scaled by 1/{2}.",
                        v, i, scaleFactor));
                }
                short intV = (short)v;
                fixedPointData[i] = intV;
            }

            return fixedPointData;
        }

        static double[] ScaleToFloatingPoint(short[] data)
        {
            if (scaleFactor < 1.0e-64)
                throw new Exception("Data scaling requires non-zero scaleFactor.");

            double[] scaledData = new double[data.Length];
            for (int i = 0; i < data.Length; i++)
            {
                scaledData[i] = data[i] * scaleFactor;
            }
            return scaledData;
        }

        static UInt32[] PackToUInt32(short[] data)
        {
            UInt32[] outData = new UInt32[data.Length / 2];
            for (int i = 0; i < data.Length/2; i++)
            {
                UInt32 temp = (UInt32)(((UInt16)data[2*i+1] << 16) | (UInt16)data[2*i]);
                outData[i] = temp;
            }
            return outData;
        }

        static short[] UnpackToShort(UInt32[] data)
        {
            short[] outData = new short[data.Length * 2];
            for (int i = 0; i < data.Length; i++)
            {
                outData[2 * i] = (short)(data[i] & 0xFFFF);
                outData[2 * i + 1] = (short)((data[i] >> 16) & 0xFFFF);
            }
            return outData;
        }


        static UInt32[] CreateShapingTable()
        {
            UInt32[] ShapingTable = new UInt32[256];
            for (int i=0; i<256; i++)
            {
                ShapingTable[i] = (0x0100 << 16) | ((UInt32)i << 8);
            }

            return ShapingTable;
        }

        static void ConfigS2MM(UInt64 dmaBaseAddr, UInt64 ddrDstAddr, UInt32 bytes)
        {
            // Stream to MemoryMapped DMA Control Register
            const UInt64 S2MM_DMACR = 0x30;
            // Stream to MemoryMapped Destination Address, Lower 32 bits
            const UInt64 S2MM_DA = 0x48;
            // Stream to MemoryMapped Destination Address, Upper 32 bits
            const UInt64 S2MM_DA_MSB = 0x4C;
            // Stream to MemoryMapped Length (bytes)
            const UInt64 S2MM_LENGTH = 0x58;

            FpgaOp.RegWrite(dmaBaseAddr + S2MM_DMACR, 0x01);
            FpgaOp.RegWrite(dmaBaseAddr + S2MM_DA_MSB, (UInt32)(ddrDstAddr >> 32) & 0xFFFFFFFF);
            FpgaOp.RegWrite(dmaBaseAddr + S2MM_DA, (UInt32)(ddrDstAddr & 0xFFFFFFFF));
            FpgaOp.RegWrite(dmaBaseAddr + S2MM_LENGTH, bytes);
        }

        static void ConfigMM2S(UInt64 dmaBaseAddr, UInt64 ddrSrcAddr, UInt32 bytes)
        {
            // MemoryMapped to Stream DMA Control Register
            const UInt64 MM2S_DMACR = 0x00;
            // MemoryMapped to Stream Source Address, lower 32 bits
            const UInt64 MM2S_SA = 0x18;
            // MemoryMapped to Stream Source Address, upper 32 bits
            const UInt64 MM2S_SA_MSB = 0x1C;
            // MemoryMapped to Stream Length (bytes)
            const UInt64 MM2S_LENGTH = 0x28;

            FpgaOp.RegWrite(dmaBaseAddr + MM2S_DMACR, 0x01);
            FpgaOp.RegWrite(dmaBaseAddr + MM2S_SA_MSB, (UInt32)(ddrSrcAddr >> 32) & 0xFFFFFFFF);
            FpgaOp.RegWrite(dmaBaseAddr + MM2S_SA, (UInt32)(ddrSrcAddr & 0xFFFFFFFF));
            FpgaOp.RegWrite(dmaBaseAddr + MM2S_LENGTH, bytes);
        }

        static double scaleFactor = 1;

        static void Main(string[] args)
        {
            SessionOpen();

            ShowAddressMap();

            #region Test
            //TestMemory();
            //TestRegOperation();
            //TestDDR();
            #endregion

            //goto label;

            #region ET Test

            const int numOfSamples = 1024;
            const int osr = 8;
            const UInt64 Et_RegBase = 0x21000;
            const UInt64 Streamer_DMA_RegBase = 0x20000;
            const UInt64 paInAddr = 0x02000000;
            const UInt64 envOutAddr = 0x03000000;

            //Step 1. Load Waveform into DDR
            StreamReader sr = new StreamReader(@"c:\wjh\IQ.csv");
            double[] iqData = new double[numOfSamples * 2];
            string line = "";
            int LineNumber = 0;
            while ((line = sr.ReadLine()) != null)
            {
                var iq = line.Split(',');
                var iData = float.Parse(iq[0]);
                var qData = float.Parse(iq[1]);

                if (LineNumber < numOfSamples)
                {
                    iqData[2 * LineNumber] = iData;
                    iqData[2 * LineNumber + 1] = qData;
                }
                LineNumber++;
            }
            var fixedPointData = ScaleToFixedPoint(iqData, true, ref scaleFactor);
            Console.WriteLine("scaleFactor = {0}", scaleFactor);
            var dataToDdr = PackToUInt32(fixedPointData);
            FpgaOp.DdrWrite(dataToDdr, paInAddr, numOfSamples);

            //Step 2. Setup ET registers
            FpgaOp.RegWrite(Et_RegBase + 0x4, 0x80000000/osr); // Resampler rate = 1
            FpgaOp.RegWrite(Et_RegBase + 0x10, numOfSamples);  // Input Sample=1024
            FpgaOp.RegWrite(Et_RegBase + 0x14, numOfSamples * osr); // Output Sample=1024

            FpgaOp.RegWrite(Et_RegBase + 0x8, 0); 
            FpgaOp.RegWrite(Et_RegBase + 0xc, 0); 
            FpgaOp.RegWrite(Et_RegBase + 0x18, 0); 

            var shapingTable = CreateShapingTable();
            FpgaOp.RegArrayWrite(shapingTable, Et_RegBase + 0x400, 256); //Shaping Table
            FpgaOp.RegWrite(Et_RegBase + 0x0, 1); // Clr

            //Step 3. Setup Streamer32
            ConfigS2MM(Streamer_DMA_RegBase, envOutAddr, numOfSamples * osr * 2); // envOut each sample 2 bytes
            ConfigMM2S(Streamer_DMA_RegBase, paInAddr, numOfSamples * 4); // paIn each sample 4 bytes

            //Step 4. Read back the ET output
            UInt32[] dataFromDdr = new UInt32[numOfSamples * osr / 2]; 
            FpgaOp.DdrRead(dataFromDdr, envOutAddr, numOfSamples * osr / 2);
            var envData = UnpackToShort(dataFromDdr);
            var envData_Double = ScaleToFloatingPoint(envData);
            #endregion

            //label:

            //Step 5. Write Env output to file
            StreamWriter sw = new StreamWriter(@"c:\wjh\EnvOut.csv");
            for (int i=0; i<envData_Double.Length; i++)
            {
                sw.WriteLine("{0}", envData_Double[i]);
            }
            sw.Close();

            SessionClose();

            Console.ReadKey();
        }
    }
}
