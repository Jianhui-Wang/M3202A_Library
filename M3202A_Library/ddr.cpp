#include "stdafx.h"

#include "ddr.h"

#include <string>

// MemoryMapped to Stream DMA Control Register
const uint64_t MM2S_DMACR = 0x00;
// MemoryMapped to Stream DMA Status Register
const uint64_t MM2S_DMASR = 0x04;
// MemoryMapped to Stream Source Address
const uint64_t MM2S_SA = 0x18;
// MemoryMapped to Stream Length (bytes)
const uint64_t MM2S_LENGTH = 0x28;

// Stream to MemoryMapped DMA Control Register
const uint64_t S2MM_DMACR = 0x30;
// Stream to MemoryMapped DMA Status Register
const uint64_t S2MM_DMASR = 0x34;
// Stream to MemoryMapped Destination Address
const uint64_t S2MM_DA = 0x48;
// Stream to MemoryMapped Length (bytes)
const uint64_t S2MM_LENGTH = 0x58;

// helper function to reduce boilerplate
uint64_t get_DMA_from_option(const rsp_streamer *streamer, RSP_STREAMER_DMA DMA_option)
{
	if (DMA_option != RSP_STREAMER_DMA_1 && DMA_option != RSP_STREAMER_DMA_2)
	{
		return static_cast<uint64_t>(-1);
	}

	return (DMA_option == RSP_STREAMER_DMA_1 ? streamer->DMA_1 : streamer->DMA_2);
}
uint64_t get_PCMem_from_option(const rsp_streamer *streamer, RSP_STREAMER_DMA DMA_option)
{
	if (DMA_option != RSP_STREAMER_DMA_1 && DMA_option != RSP_STREAMER_DMA_2)
	{
		return static_cast<uint64_t>(-1);
	}

	return (DMA_option == RSP_STREAMER_DMA_1 ? streamer->pc_mem_1 : streamer->pc_mem_2);
}
uint64_t get_DMA_size_from_option(const rsp_streamer *streamer, RSP_STREAMER_DMA DMA_option)
{
	if (DMA_option != RSP_STREAMER_DMA_1 && DMA_option != RSP_STREAMER_DMA_2)
	{
		return static_cast<uint64_t>(-1);
	}

	return (DMA_option == RSP_STREAMER_DMA_1 ? streamer->pc_mem_1_size : streamer->pc_mem_2_size);
}
rsp_int getAddressInfoByName(rsp_kernel_instance kernel_inst,
                             const char *name,
                             rsp_address_info param_name,
                             size_t param_value_size,
                             void *param_value,
                             size_t *param_value_size_ret)
{
	rsp_int errorCode;
	uint32_t address_count;
	rspGetAddressInfo(kernel_inst, 0/*ignored*/, RSP_ADDRESS_COUNT, 
        sizeof(address_count), &address_count, nullptr);

	for (unsigned int i = 0; i < address_count; i++)
	{
		size_t ith_name_size;
		errorCode = rspGetAddressInfo(kernel_inst, i, RSP_ADDRESS_NAME,
            0, nullptr, &ith_name_size);
		if (errorCode != RSP_SUCCESS) return errorCode;

		std::string ith_name;
		ith_name.resize(ith_name_size - 1); // minus one because the size includes also the '\0'

		errorCode = rspGetAddressInfo(kernel_inst, i, RSP_ADDRESS_NAME,
            ith_name_size, (void*)ith_name.data(), nullptr);
		if (errorCode != RSP_SUCCESS) return errorCode;


		if (ith_name == name)
		{
			return rspGetAddressInfo(kernel_inst, i, param_name,
                param_value_size, param_value, param_value_size_ret);
		}
	}

	if (param_value_size_ret) *param_value_size_ret = 0;
	return RSP_INVALID_VALUE;
}

unsigned int Minimum(unsigned int a, unsigned int b) {
    return !(b<a) ? a : b;
}


rsp_streamer rspSetupStreamer(rsp_kernel_instance kernel_inst,
                              const char *pc_mem_1,
                              const char *pc_mem_2,
                              const char *axi_control,
                              const char *axi_host, 
                              rsp_int *error)
{
	rsp_streamer streamer = rsp_streamer();
	rsp_int returnCode = 0;

	if (!kernel_inst)
	{
		if (error) *error = RSP_INVALID_KERNEL_INSTANCE;
		return rsp_streamer();
	}

	if (axi_control == nullptr)
	{
		if (error) *error = returnCode;
		return rsp_streamer();
	}


	streamer.kernel_inst = kernel_inst;

	// get addresses relating to the axilite
	returnCode = rspGetAddress(kernel_inst, axi_control, &streamer.DMA_1);
	if (returnCode != RSP_SUCCESS) {
		if (error) *error = returnCode;
		return rsp_streamer();
	}

	streamer.DMA_2 = streamer.DMA_1 + 1024;
	streamer.pager = streamer.DMA_1 + 2048;

	// reason for value: 1024 * 8192 = 8192kB which is how many bytes
	// the 23 bits of length gets you. Then subtract one word off
	// because 0 is included in the range
	streamer.max_dma_length = 1024 * 8192 - 4;

	// get PC Mem ports, if they are set up
	if (pc_mem_1 != nullptr)
	{
		returnCode = rspGetAddress(kernel_inst, pc_mem_1, &streamer.pc_mem_1);
		if (returnCode != RSP_SUCCESS)
		{
			if (error) *error = returnCode;
			return rsp_streamer();
		}

        returnCode = getAddressInfoByName(kernel_inst, pc_mem_1, RSP_ADDRESS_LENGTH,
            sizeof(rsp_ulong), &streamer.pc_mem_1_size, nullptr);
		if (returnCode != RSP_SUCCESS)
		{
			if (error) *error = returnCode;
			return rsp_streamer();
		}
	}
	else
	{
		streamer.pc_mem_1 = static_cast<uint64_t>(-1);
	}

	if (pc_mem_2 != nullptr)
	{
		returnCode = rspGetAddress(kernel_inst, pc_mem_2, &streamer.pc_mem_2);
		if (returnCode != RSP_SUCCESS)
		{
			if (error) *error = returnCode;
			return rsp_streamer();
		}

		auto e = getAddressInfoByName(kernel_inst, pc_mem_2, RSP_ADDRESS_LENGTH,
            sizeof(rsp_ulong), &streamer.pc_mem_2_size, nullptr);
		if (e != RSP_SUCCESS)
		{
			if (error) *error = returnCode;
			return rsp_streamer();
		}
	}
	else
	{
		streamer.pc_mem_2 = static_cast<uint64_t>(-1);
	}

	// hardcoded for now
	streamer.bit_width = 32;

	// get host port, if it is set up
	if (axi_host != nullptr)
	{
		returnCode = rspGetAddress(kernel_inst, axi_host, &streamer.axi_host);
		if (returnCode != RSP_SUCCESS)
		{
			if (error) *error = returnCode;
			return rsp_streamer();
		}

		rsp_ulong page_size_long;
		auto e = getAddressInfoByName(kernel_inst, axi_host, RSP_ADDRESS_LENGTH,
            sizeof(rsp_ulong), &page_size_long, nullptr);
		if (e != RSP_SUCCESS)
		{
			if (error) *error = returnCode;
			return rsp_streamer();
		}

		streamer.page_size = static_cast<uint32_t>(page_size_long);
	}
	else
	{
		streamer.page_size = static_cast<uint32_t>(-1);
		streamer.axi_host = static_cast<uint64_t>(-1);
	}

	if (error) *error = RSP_SUCCESS;
	return streamer;
}

rsp_int rspStreamerResetDMA(const rsp_streamer *streamer, RSP_STREAMER_DMA DMA_option)
{
	uint32_t buffer;
	rsp_int returnCode;

	if (!streamer) return RSP_INVALID_VALUE;

	auto DMACR = S2MM_DMACR; // S2MM or MM2S doesn't seem to matter

	auto DMA = get_DMA_from_option(streamer, DMA_option);
	if (DMA == -1) return RSP_INVALID_ENUM;

	// reset
	buffer = 0x4;    // bit 0 is run/start, bit 2 is reset
	returnCode = rspKernelInstanceRegisterWrite(streamer->kernel_inst, buffer, DMA + DMACR);
	if (returnCode != RSP_SUCCESS) return returnCode;

	do { // is this even necessary? Was recommended
		returnCode = rspKernelInstanceRegisterRead(streamer->kernel_inst, &buffer, DMA + DMACR);
		if (returnCode != RSP_SUCCESS) return returnCode;
	} while ((buffer & 0x4) == 0x4);

	return RSP_SUCCESS;
}

rsp_int rspStreamerConfigureDMA(const rsp_streamer *streamer,
                                RSP_STREAMER_DMA DMA_option,
                                uint32_t address,
                                uint32_t length,
                                RSP_STREAMER_IO io)
{
    if (!streamer) return RSP_INVALID_VALUE;

	auto DMA = get_DMA_from_option(streamer, DMA_option);
	if (DMA == -1) return RSP_INVALID_ENUM;

	auto quant = streamer->bit_width / 8;

	if (io != RSP_STREAMER_READ && io != RSP_STREAMER_WRITE)
	{
		return RSP_INVALID_ENUM;
	}

	if (length % quant != 0 || address % quant != 0)
	{
		return RSP_INVALID_VALUE;
	}

	uint32_t buffer;
	rsp_int returnCode;

	auto DMACR = (io == RSP_STREAMER_WRITE ? S2MM_DMACR : MM2S_DMACR);
	auto ADDRESS = (io == RSP_STREAMER_WRITE ? S2MM_DA : MM2S_SA);
	auto LENGTH = (io == RSP_STREAMER_WRITE ? S2MM_LENGTH : MM2S_LENGTH);

	// set run/stop bit to 1 (MM2S_DMACR.RS = 1)
	buffer = 0x1;    // bit 0 is run/start, the rest of the bits don't matter
	returnCode = rspKernelInstanceRegisterWrite(streamer->kernel_inst, buffer, DMA + DMACR);
	if (returnCode != RSP_SUCCESS) return returnCode;

	// write source address to MM2S_SA
	buffer = address;  // read from address 0
	returnCode = rspKernelInstanceRegisterWrite(streamer->kernel_inst, buffer, DMA + ADDRESS);
	if (returnCode != RSP_SUCCESS) return returnCode;

	// write number of bytes to transfer to MM2S_LENGTH
	buffer = length;
	returnCode = rspKernelInstanceRegisterWrite(streamer->kernel_inst, buffer, DMA + LENGTH);
	if (returnCode != RSP_SUCCESS) return returnCode;

	return RSP_SUCCESS;
}

rsp_int rspStreamerDMAIsIdle(const rsp_streamer *streamer,
                             RSP_STREAMER_DMA DMA_option, 
                             RSP_STREAMER_IO io,
                             bool *idle)
{
	if (!idle || !streamer) return RSP_INVALID_VALUE;

	auto DMA = get_DMA_from_option(streamer, DMA_option);
	if (DMA == -1) return RSP_INVALID_ENUM;

	if (io != RSP_STREAMER_READ && io != RSP_STREAMER_WRITE)
	{
		return RSP_INVALID_ENUM;
	}

	uint32_t buffer;
	rsp_int returnCode;

	auto DMASR = (io == RSP_STREAMER_WRITE ? S2MM_DMASR : MM2S_DMASR);

	returnCode = rspKernelInstanceRegisterRead(streamer->kernel_inst, &buffer, DMA + DMASR);
	if (returnCode != RSP_SUCCESS) return returnCode;

	// counts as idle if the DMA is either "idle" or "halted"
	*idle = (buffer & 0x3) != 0;

	return RSP_SUCCESS;
}

rsp_int rspStreamerDMAHasError(const rsp_streamer *streamer,
                               RSP_STREAMER_DMA DMA_option,
                               RSP_STREAMER_IO io,
                               bool *error)
{
	if (!error || !streamer) return RSP_INVALID_VALUE;

	auto DMA = get_DMA_from_option(streamer, DMA_option);
	if (DMA == -1) return RSP_INVALID_ENUM;

	if (io != RSP_STREAMER_READ && io != RSP_STREAMER_WRITE)
	{
		return RSP_INVALID_ENUM;
	}

	uint32_t buffer;
	rsp_int returnCode;

	auto DMASR = (io == RSP_STREAMER_WRITE ? S2MM_DMASR : MM2S_DMASR);

	returnCode = rspKernelInstanceRegisterRead(streamer->kernel_inst, &buffer, DMA + DMASR);
	if (returnCode != RSP_SUCCESS) return returnCode;

	// covers DMAIntErr, DMASlvErr, DMADecErr
	*error = (buffer & 0x70) != 0;

	return RSP_SUCCESS;
}

rsp_int rspStreamerWait(const rsp_streamer *streamer,
                        RSP_STREAMER_DMA DMA_option,
                        RSP_STREAMER_IO io)
{
	if (!streamer) return RSP_INVALID_VALUE;

	auto DMA = get_DMA_from_option(streamer, DMA_option);
	if (DMA == -1) return RSP_INVALID_ENUM;

	if (io != RSP_STREAMER_READ && io != RSP_STREAMER_WRITE)
	{
		return RSP_INVALID_ENUM;
	}

	rsp_int returnCode;

	// check the resource isn't being used elsewhere
	while (true)
	{
		bool idle;
		returnCode = rspStreamerDMAIsIdle(streamer, DMA_option, RSP_STREAMER_READ, &idle);
		if (returnCode != RSP_SUCCESS) return returnCode;

		if (idle) break;
	}


	return RSP_SUCCESS;
}

rsp_int rspStreamerWriteDMA(const rsp_streamer *streamer,
                            RSP_STREAMER_DMA DMA_option,
                            uint32_t address,
                            uint32_t *data,
                            uint32_t length)
{
	auto DMA = get_DMA_from_option(streamer, DMA_option);
	auto pc_mem = get_PCMem_from_option(streamer, DMA_option);
	auto pc_mem_size = get_DMA_size_from_option(streamer, DMA_option);
	if (DMA == -1) return RSP_INVALID_ENUM;
	if (pc_mem == -1) return RSP_INVALID_VALUE;

	if (!streamer) return RSP_INVALID_VALUE;

	rsp_int returnCode;


	int offset = 0;
	while (length > 0)
	{
        unsigned int lengthInPage = Minimum(length, static_cast<unsigned int>(streamer->max_dma_length));

		returnCode = rspStreamerResetDMA(streamer, DMA_option);
		if (returnCode != RSP_SUCCESS) return returnCode;

		returnCode = rspStreamerConfigureDMA(streamer, DMA_option, address,
            lengthInPage, RSP_STREAMER_WRITE);
		if (returnCode != RSP_SUCCESS) return returnCode;


		// Write test values
		int i = 0;
		for (unsigned int remaining = lengthInPage; remaining > 0;)
		{
            unsigned int LengthInSubPage = Minimum(remaining, static_cast<unsigned int>(pc_mem_size));

			returnCode = rspKernelInstanceArrayWrite(streamer->kernel_inst, &data[i + offset],
                pc_mem, LengthInSubPage);
			if (returnCode != RSP_SUCCESS) return returnCode;

			i += LengthInSubPage / 4;
			remaining -= LengthInSubPage;
		}

		length -= lengthInPage;
		address += lengthInPage;
		offset += lengthInPage / 4;
	}
	return RSP_SUCCESS;
}


rsp_int rspStreamerReadDMA(const rsp_streamer *streamer,
                           RSP_STREAMER_DMA DMA_option,
                           uint32_t address,
                           uint32_t *data,
                           uint32_t length)
{
	auto DMA = get_DMA_from_option(streamer, DMA_option);
	auto pc_mem = get_PCMem_from_option(streamer, DMA_option);
	auto pc_mem_size = get_DMA_size_from_option(streamer, DMA_option);
	if (DMA == -1) return RSP_INVALID_ENUM;
	if (pc_mem == -1) return RSP_INVALID_VALUE;

	if (!streamer) return RSP_INVALID_VALUE;

	rsp_int returnCode;


	int offset = 0;
	while (length > 0)
	{
        unsigned int lengthInPage = Minimum(length, static_cast<unsigned int>(streamer->max_dma_length));

		returnCode = rspStreamerResetDMA(streamer, DMA_option);
		if (returnCode != RSP_SUCCESS) return returnCode;

		returnCode = rspStreamerConfigureDMA(streamer, DMA_option, address,
            lengthInPage, RSP_STREAMER_READ);
		if (returnCode != RSP_SUCCESS) return returnCode;

		// Write test values
		int i = 0;
		for (unsigned int remaining = lengthInPage; remaining > 0;)
		{
            unsigned int LengthInSubPage = Minimum(remaining, static_cast<unsigned int>(pc_mem_size));

			returnCode = rspKernelInstanceArrayRead(streamer->kernel_inst, &data[i + offset],
                pc_mem, LengthInSubPage);
			if (returnCode != RSP_SUCCESS) return returnCode;

			i += LengthInSubPage / 4;
			remaining -= LengthInSubPage;
		}

		length -= lengthInPage;
		address += lengthInPage;
		offset += lengthInPage / 4;
	}
	return RSP_SUCCESS;
}

rsp_int rspStreamerCopyDMA(const rsp_streamer *streamer,
                           RSP_STREAMER_DMA DMA_option,
                           uint32_t startAddress,
                           uint32_t endAddress,
                           uint32_t length)
{
	auto DMA = get_DMA_from_option(streamer, DMA_option);
	if (DMA == -1) return RSP_INVALID_ENUM;

	if (!streamer) return RSP_INVALID_VALUE;

	rsp_int returnCode;


	while (length > 0)
	{
		// the streamer will need to wait for the previous page to finish or it will get clobbered
		rspStreamerWait(streamer, DMA_option, RSP_STREAMER_READ);
		rspStreamerWait(streamer, DMA_option, RSP_STREAMER_WRITE);

        int lengthInPage = Minimum(length, static_cast<unsigned int>(streamer->max_dma_length));

		returnCode = rspStreamerResetDMA(streamer, DMA_option);
		if (returnCode != RSP_SUCCESS) return returnCode;

		returnCode = rspStreamerConfigureDMA(streamer, DMA_option, startAddress,
            lengthInPage, RSP_STREAMER_READ);
		if (returnCode != RSP_SUCCESS) return returnCode;

		returnCode = rspStreamerConfigureDMA(streamer, DMA_option, endAddress,
            lengthInPage, RSP_STREAMER_WRITE);
		if (returnCode != RSP_SUCCESS) return returnCode;

		length -= lengthInPage;
		startAddress += lengthInPage;
		endAddress += lengthInPage;
	}

	return RSP_SUCCESS;
}

rsp_int rspStreamerWriteHost(const rsp_streamer *streamer,
                             uint32_t address,
                             uint32_t *data,
                             uint32_t length)
{
	auto quant = streamer->bit_width / 8;

	if (length % quant != 0 || address % quant != 0)
	{
		return RSP_INVALID_VALUE;
	}

	const uint32_t max_page_length = streamer->page_size;

	uint32_t page_number = address / max_page_length;
	uint32_t page_offset = address % max_page_length;
	int idx = 0;
	while (length > 0)
	{
		uint32_t page_length = max_page_length - page_offset;
		if (page_length > length) page_length = length;

		auto returnCode = rspKernelInstanceRegisterWrite(streamer->kernel_inst,
            page_number, streamer->pager);
		if (returnCode != RSP_SUCCESS) return returnCode;

		returnCode = rspKernelInstanceArrayWrite(streamer->kernel_inst, data + idx,
            page_offset + streamer->axi_host, page_length);
		if (returnCode != RSP_SUCCESS) return returnCode;

		page_number++;
		page_offset = 0;
		idx += page_length / 4;
		length -= page_length;
	}
	return RSP_SUCCESS;
}

rsp_int rspStreamerReadHost(const rsp_streamer *streamer,
                            uint32_t address,
                            uint32_t *data,
                            uint32_t length)
{
	auto quant = streamer->bit_width / 8;

	if (length % quant != 0 || address % quant != 0)
	{
		return RSP_INVALID_VALUE;
	}

	const uint32_t max_page_length = streamer->page_size;

	uint32_t page_number = address / max_page_length;
	uint32_t page_offset = address % max_page_length;
	int idx = 0;
	while (length > 0)
	{
		uint32_t page_length = max_page_length - page_offset;
		if (page_length > length) page_length = length;

		auto returnCode = rspKernelInstanceRegisterWrite(streamer->kernel_inst,
            page_number, streamer->pager);
		if (returnCode != RSP_SUCCESS) return returnCode;

		returnCode = rspKernelInstanceArrayRead(streamer->kernel_inst, data + idx,
            page_offset + streamer->axi_host, page_length);
		if (returnCode != RSP_SUCCESS) return returnCode;

		page_number++;
		page_offset = 0;
		idx += page_length / 4;
		length -= page_length;
	}
	return RSP_SUCCESS;
}
