#pragma once

#ifdef M3202A_LIBRARY_EXPORTS  
#define M3202A_LIBRARY_EXPORTS_API __declspec(dllexport)   
#else  
#define M3202A_LIBRARY_EXPORTS_API __declspec(dllimport)   
#endif  

M3202A_LIBRARY_EXPORTS_API void SessionOpen();
M3202A_LIBRARY_EXPORTS_API void SessionClose();
M3202A_LIBRARY_EXPORTS_API void ShowAddressMap();
M3202A_LIBRARY_EXPORTS_API int RegRead(uint32_t *data, uint64_t address);
M3202A_LIBRARY_EXPORTS_API int RegWrite(uint64_t address, uint32_t value);
M3202A_LIBRARY_EXPORTS_API int RegArrayRead(uint32_t *data, uint64_t address, size_t length);
M3202A_LIBRARY_EXPORTS_API int RegArrayWrite(uint32_t *data, uint64_t address, size_t length);
M3202A_LIBRARY_EXPORTS_API int DdrRead(uint32_t *data, uint64_t address, size_t length);
M3202A_LIBRARY_EXPORTS_API int DdrWrite(uint32_t *data, uint64_t address, size_t length);
M3202A_LIBRARY_EXPORTS_API int DdrCopy(uint64_t startAddress, uint64_t endAddress, size_t length);