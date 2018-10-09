// M3202A_Library.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include <iostream>
#include <vector>
#include <string>

#include "M3202A_Library.h"  



// ============ START OF HELPER FUNCTIONS ==================//
void checkError(std::string action, rsp_int errCode)
{
	if (errCode)
	{
		std::string message = action + " failed with error " + rspGetErrorCodeName(errCode);
		throw std::runtime_error(message);
	}
}
rsp_platform_id findPlatform(std::string platformName)
{
	// 1.1 First fetch the number of platforms
	rsp_uint nPlatforms = 0;
	rsp_int err = rspGetPlatformIDs(0, nullptr, &nPlatforms);
	if (err == RSP_PLATFORM_NOT_FOUND)
	{
		throw std::runtime_error("No platforms were found.\nPlease verify that the platform GTP file is in the working directory.");
	}
	else
	{
		checkError("Looking for platforms", err);
	}

	// 1.2 Then fetch the platform IDs
	std::vector<rsp_platform_id> platforms(nPlatforms);
	err = rspGetPlatformIDs(nPlatforms, platforms.data(), nullptr);

	// 1.3 Now look for the platform with the correct name
	for (rsp_platform_id platform : platforms)
	{
		// 1.3.1 Get the length of the platform name
		size_t nameSize = 0;

		err = rspGetPlatformInfo(platform, RSP_PLATFORM_NAME, 0, nullptr, &nameSize);
		checkError("Reading platform name", err);

		std::string name;
		name.resize(nameSize - 1); // minus one because the size includes also the '\0'

								   // 1.3.2 Get the name of the platform
		err = rspGetPlatformInfo(platform, RSP_PLATFORM_NAME, nameSize, (void*)name.data(), nullptr);
		checkError("Reading platform name", err);

		// 1.3.3 Check if the name matches the target
		if (name == platformName)
		{
			// Return immediately when the platform is found
			return platform;
		}

	}

	// 1.4 If the platform was not found, report an error
	std::string message = "Cannot find platform '" + platformName + "'\n";
	message += "Please verify that the platform DLL and GTP file are located in the binary directory and restart the application\n";
	throw std::runtime_error(message);
}
rsp_device_id findDevice(rsp_platform_id platform, std::string targetDeviceId)
{
	// 2.1 Get the number of devices for this platform
	rsp_uint nDevices = 0;
	rsp_int err = rspGetDeviceIDs(platform, RSP_DEVICE_TYPE_FPGA, 0, nullptr, &nDevices);

	if (err == RSP_DEVICE_NOT_FOUND)
	{
		std::string message = "No devices were found.\nPlease verify that the device is present.";
		throw std::runtime_error(message);
	}
	else
	{
		checkError("Looking for devices", err);
	}

	// 2.2 Get all the devices
	std::vector<rsp_device_id> devices(nDevices);
	err = rspGetDeviceIDs(platform, RSP_DEVICE_TYPE_FPGA, nDevices, devices.data(), nullptr);
	checkError("Fetching devices", err);

	// 2.3 Look for the device with the target ID
	std::vector<std::string> foundDevices;
	for (rsp_device_id device : devices)
	{
		// 2.3.1 Get the length of the device UUID (although this should be fixed)
		size_t idSize = 0;
		err = rspGetDeviceInfo(device, RSP_DEVICE_UUID_KS, 0, nullptr, &idSize);
		checkError("Reading device UUID length", err);

		// 2.3.2 Get the device UUID
		std::string id;
		id.resize(idSize - 1); // minus one because the size includes also the '\0'

		err = rspGetDeviceInfo(device, RSP_DEVICE_UUID_KS, idSize, (void*)id.data(), nullptr);
		checkError("Reading device UUID", err);
		foundDevices.emplace_back(id);

		// 2.3.3 Check if the UUID matches the target
		if (id == targetDeviceId)
		{
			// Return immediately when the device is found
			return device;
		}
	}

	// 2.4 If the device was not found, report an error
	std::string message = "No device with id '" + targetDeviceId + "' located.\n";
	message += "Available devices:\n";
	for (auto device : foundDevices)
	{
		message += "'" + device + "'\n";
	}
	throw std::runtime_error(message);
}
rsp_context createContext(rsp_device_id device)
{
	rsp_int err;
	rsp_context context = rspCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
	checkError("Creating the context", err);
	return context;
}
rsp_program loadProgram(rsp_context context, rsp_device_id device, std::string targetBinPath)
{
	rsp_int err;

	const char* files[1];
	files[0] = targetBinPath.c_str();
	rsp_program program = rspCreateProgramWithK7z(context, 1, &device, 1, files, &err);

	if (err == RSP_INVALID_PATH)
	{
		std::string message = "The k7z file was not found.\nPlease verify that the k7z file is in the working directory.";
		throw std::runtime_error(message);
	}
	checkError("Creating the program", err);

	return program;
}
rsp_kernel createKernel(rsp_program program, std::string targetKernelname)
{
	rsp_int err;

	// 5.1 Create the kernel
	rsp_kernel kernel = rspCreateKernel(program, targetKernelname.c_str(), &err);

	// 5.2 If the kernel was not found, report the names of the available kernels
	if (err == RSP_INVALID_KERNEL_NAME)
	{
		std::string message = "Could not find a kernel with the name " + targetKernelname + ".\n";

		// 5.2.1 Get the length of the list of names
		size_t nameSize = 0;
		err = rspGetProgramInfo(program, RSP_PROGRAM_KERNEL_NAMES, 0, nullptr, &nameSize);
		checkError("Reading the kernel names", err);

		// 5.2.2 Get the list of names
		std::string kernelNames;
		kernelNames.resize(nameSize - 1); // minus one because the size includes also the '\0'

		err = rspGetProgramInfo(program, RSP_PROGRAM_KERNEL_NAMES, nameSize, (void*)kernelNames.data(), nullptr);
		checkError("Reading the kernel names", err);

		// 5.2.3 List the kernel names
		message += "Found kernel names:\n";
		message += kernelNames;

		throw std::runtime_error(message);
	}
	else
	{
		checkError("Creating the kernel", err);
	}

	return kernel;
}
rsp_kernel_instance createKernelInstance(rsp_kernel kernel)
{
	rsp_int err;

	rsp_kernel_instance ki = rspCreateKernelInstance(kernel, &err);

	if (err == RSP_FAILED_DOWNLOAD)
	{
		std::string message = "The device failed to download the bitfile.";
		throw std::runtime_error(message);
	}
	else
	{
		checkError("Creating the kernel instance", err);
	}

	return ki;
}
// ============ END OF HELPER FUNCTIONS ==================//

//////////////////////////////////
// Test A: Read the address map //
//////////////////////////////////
void printAddressMap(rsp_kernel_instance kernel_instance)
{
	rsp_int err;

	// A.1 Get the length of the address map string
	size_t addressMapSize;
	err = rspGetAddressInfo(kernel_instance, 0, RSP_ADDRESS_MAP, 0, nullptr, &addressMapSize);
	checkError("Reading the address map", err);


	// A.2 Get the address map string
	std::string addressMap;
	addressMap.resize(addressMapSize - 1); // minus one because the size includes also the '\0'

	err = rspGetAddressInfo(kernel_instance, 0, RSP_ADDRESS_MAP, addressMapSize, (void*)addressMap.data(), nullptr);
	checkError("Reading the address map", err);

	// A.3 Print the address map
	std::cout << "Address map contents:" << std::endl << addressMap << std::endl;
}


////////////////////////////////////////////
// Test B: Read and write register values //
////////////////////////////////////////////
void testRegisterAccess(rsp_kernel_instance kernelInst, std::string regName)
{
	// B.1 Get the address of the register by name
	uint64_t regAddress;
	rsp_int err = rspGetAddress(kernelInst, regName.c_str(), &regAddress);
	checkError("Getting address of register '" + regName + "'", err);


	// B.2 Read the initial value in the register
	uint32_t dataRead;
	err = rspKernelInstanceRegisterRead(kernelInst, &dataRead, regAddress);
	checkError("Reading register " + regName, err);

	std::cout << "\nRead value from register '" << regName << "': ";
	std::cout << std::hex << dataRead << std::endl << std::dec;

	// B.3 Write a new value to the register
	uint32_t dataWritten = 0x12345678;
	std::cout << std::hex << "Write value '" << dataWritten << "' to register '" << regName << "'" << std::endl << std::dec;
	rspKernelInstanceRegisterWrite(kernelInst, dataWritten, regAddress);

	// B.4 Read the new value in the register
	rspKernelInstanceRegisterRead(kernelInst, &dataRead, regAddress);
	std::cout << "Read value again from register '" << regName << "': ";
	std::cout << std::hex << dataRead << std::endl << std::dec;

	if (dataWritten != dataRead)
	{
		std::cout << "Read/Write register failed!" << std::endl;
	}
};


////////////////////////////////////////////////////
// Test C: Read and write the streaming interface //
////////////////////////////////////////////////////
void testStreaming(rsp_kernel_instance kernelInst)
{
	rsp_int err;

	const size_t streamIndex = 0;
	const size_t streamBufferSize = 100;

	uint64_t dataRead[streamBufferSize];
	uint64_t dataToWrite[streamBufferSize];

	// C.1 Generate random test data
	for (size_t i = 0; i < streamBufferSize; ++i)
	{
		dataToWrite[i] = std::rand();
	}

	// C.2 Write to the stream
	err = rspKernelInstanceStreamWrite(kernelInst, streamIndex, dataToWrite, streamBufferSize * sizeof(uint64_t));
	checkError("Writing stream", err);

	// C.3 Read from the stream
	err = rspKernelInstanceStreamRead(kernelInst, streamIndex, dataRead, streamBufferSize * sizeof(uint64_t));
	checkError("Reading stream", err);

	// C.5 Verify the data
	bool bEqual = std::equal(std::begin(dataToWrite), std::end(dataToWrite), dataRead);
	if (bEqual)
	{
		std::cout << "Stream transfer of " << streamBufferSize << " words successful." << std::endl << std::endl;
	}
	else
	{
		throw std::runtime_error("Stream data read did not match written.");
	}
}
// ============ END OF HELPER FUNCTIONS ==================//

// These variables contain the RSP objects used in this example
rsp_device_id device = nullptr;
rsp_context context = nullptr;
rsp_program program = nullptr;
rsp_kernel kernel = nullptr;
rsp_kernel_instance kernelInst = nullptr;
rsp_streamer rspStreamer;


void SessionOpen()
{
	// These strings identify the platform, device, program archive, and kernel used in the tests
	const std::string targetBinPath = "PWFPGA_EnvelopeTracker.k7z";

	// The following strings must match the corresponding fields in the target binary package
	// To debug them, you can extract the manifest.json  file with some zip style tools
	// or the command line (if 7za.exe is in your path)
	//      7za.exe e <targetBinPath k7z file> manifest.json
	// Then compare the fields in the manifest with these strings.
	//
	const std::string targetPlatformName = "M31xx_M32xx_M33xx";
	const std::string targetDeviceId = "80090200-0a2f-62f7-a2bb-edab00034900";
	const std::string targetKernelname = "PWFPGA_EnvelopeTracker";

	// These are the names of the registers used in Test B
	std::vector<std::string> registerNames = {
		"DDR_Inst",
		"Host_axilite_Inst",
		"Host_aximm_Inst"
	};


	try
	{
		//////////////////////////////////////
		// Step 1: Find the target platform //
		//////////////////////////////////////
		rsp_platform_id platform = findPlatform(targetPlatformName);


		////////////////////////////////////
		// Step 2: Find the target device //
		////////////////////////////////////
		device = findDevice(platform, targetDeviceId);


		////////////////////////////////////////////////
		// Step 3: Create the context from the device //
		////////////////////////////////////////////////
		context = createContext(device);


		//////////////////////////////////////////////////
		// Step 4: Create the program from the k7z file //
		//////////////////////////////////////////////////
		program = loadProgram(context, device, targetBinPath);


		////////////////////////////////////////////////
		// Step 5: Create the kernel from the program //
		////////////////////////////////////////////////
		kernel = createKernel(program, targetKernelname);


		//////////////////////////////////////////////
		// Step 6: Create an instance of the kernel //
		//////////////////////////////////////////////
		kernelInst = createKernelInstance(kernel);


		//////////////////////////////////////////////
		// Step 7: Setup DDR Streamer32             //
		//////////////////////////////////////////////
		rsp_int error;
		rspStreamer = rspSetupStreamer(kernelInst, NULL, NULL, "Host_axilite_Inst", "Host_aximm_Inst", &error);

		/////////////////////////////////////////////////////////////
		// Setup complete. Now the binary is loaded in the device. //
		/////////////////////////////////////////////////////////////
		std::cout << "Session Open complete." << std::endl << std::endl;
	}
	catch (const std::runtime_error& err)
	{
		std::cerr << err.what() << std::endl;
	}
}

void SessionClose()
{
	/////////////////////////////////////////////////////////////////////////////
	// Cleanup: release the resources in the opposite order they were acquired //
	/////////////////////////////////////////////////////////////////////////////
	if (kernelInst != nullptr)
	{
		rspReleaseKernelInstance(kernelInst);
	}

	if (kernel != nullptr)
	{
		rspReleaseKernel(kernel);
	}

	if (program != nullptr)
	{
		rspReleaseProgram(program);
	}

	if (context != nullptr)
	{
		rspReleaseContext(context);
	}

	if (device != nullptr)
	{
		rspReleaseDevice(device);
	}

	std::cout << "Session Closed complete." << std::endl << std::endl;
}

void ShowAddressMap()
{
	printAddressMap(kernelInst);
}

int RegRead(uint32_t *data, uint64_t address)
{
	rsp_int ret;
	ret = rspKernelInstanceRegisterRead(kernelInst, data, (const uint64_t)address);
	return ret;
}

int RegWrite(uint64_t address, uint32_t value)
{
	rsp_int ret;
	ret = rspKernelInstanceRegisterWrite(kernelInst, value, (const uint64_t)address);
	return ret;
}

int RegArrayRead(uint32_t *data, uint64_t address, size_t length)
{
	rsp_int ret;
	ret = rspKernelInstanceArrayRead(kernelInst, data, address, length*4);
	return ret;
}

int RegArrayWrite(uint32_t *data, uint64_t address, size_t length)
{
	rsp_int ret;
	ret = rspKernelInstanceArrayWrite(kernelInst, (const uint32_t*)data, address, length*4);
	return ret;
}

int DdrRead(uint32_t *data, uint64_t address, size_t length)
{
	return rspStreamerReadHost(&rspStreamer, address, data, length*4);
}

int DdrWrite(uint32_t *data, uint64_t address, size_t length)
{
	return rspStreamerWriteHost(&rspStreamer, address, data, length*4);
}

int DdrCopy(uint64_t startAddress, uint64_t endAddress, size_t length)
{
	return rspStreamerCopyDMA(&rspStreamer, RSP_STREAMER_DMA_1, startAddress, endAddress, length * 4);
}