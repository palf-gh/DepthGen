#include "DepthGen_OrtHost.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <Windows.h>

namespace {

void Require(bool condition, const char* message) {
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

std::filesystem::path MakeScratchDirectory() {
	wchar_t temp_path[MAX_PATH + 1] = {};
	const DWORD length = GetTempPathW(MAX_PATH, temp_path);
	Require(length > 0 && length < MAX_PATH, "GetTempPathW must return a usable temporary directory");
	const std::filesystem::path directory = std::filesystem::path(temp_path) /
		(L"DepthGenOrtHostTests_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
			std::to_wstring(GetTickCount64()));
	std::error_code status;
	std::filesystem::create_directories(directory, status);
	Require(!status, "the test scratch directory must be creatable under %TEMP%");
	return directory;
}

size_t CountEntriesExcluding(const std::filesystem::path& directory, const std::filesystem::path& excluding) {
	size_t count = 0;
	for (const auto& entry : std::filesystem::directory_iterator(directory)) {
		if (entry.path() != excluding) {
			++count;
		}
	}
	return count;
}

std::vector<char> ReadFile(const std::filesystem::path& path) {
	std::ifstream in(path, std::ios::binary);
	return std::vector<char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

int main() {
	const std::filesystem::path scratch_dir = MakeScratchDirectory();

	// Case 1: ScratchPathFor must be unique per call and carry the process id.
	// A fixed pid+tick suffix alone could collide within a single tick period
	// (GetTickCount64()'s resolution is coarser than two back-to-back calls),
	// so this also proves the implementation adds something that does not.
	{
		const std::filesystem::path destination = scratch_dir / "case1.bin";
		const std::filesystem::path first = depthgen::orthost_detail::ScratchPathFor(destination);
		const std::filesystem::path second = depthgen::orthost_detail::ScratchPathFor(destination);
		Require(first != second, "ScratchPathFor must return distinct paths on successive calls");
		const std::wstring pid = std::to_wstring(GetCurrentProcessId());
		Require(first.native().find(pid) != std::wstring::npos,
			"the scratch path must contain the current process id");
		Require(second.native().find(pid) != std::wstring::npos,
			"the scratch path must contain the current process id");
	}

	// Case 2: WriteFileAtomically writes exactly the requested bytes, leaves
	// no scratch file behind, and a second call overwrites correctly.
	{
		const std::filesystem::path destination = scratch_dir / "case2.bin";
		const std::vector<char> payload_a(128, 'A');
		std::string error;
		Require(depthgen::orthost_detail::WriteFileAtomically(destination, payload_a.data(), payload_a.size(),
			&error), "WriteFileAtomically must succeed for a fresh destination");
		std::error_code size_status_a;
		const uintmax_t size_a = std::filesystem::file_size(destination, size_status_a);
		Require(!size_status_a && size_a == payload_a.size(),
			"the destination must hold exactly the requested byte count");
		Require(CountEntriesExcluding(scratch_dir, destination) == 0,
			"a successful write must not leave a scratch file behind");

		const std::vector<char> payload_b(64, 'B');
		Require(depthgen::orthost_detail::WriteFileAtomically(destination, payload_b.data(), payload_b.size(),
			&error), "a second WriteFileAtomically call must overwrite the destination");
		std::error_code size_status_b;
		const uintmax_t size_b = std::filesystem::file_size(destination, size_status_b);
		Require(!size_status_b && size_b == payload_b.size(),
			"the destination must hold exactly the second payload's byte count");
		Require(ReadFile(destination) == payload_b, "the overwritten destination must contain the second payload");
		Require(CountEntriesExcluding(scratch_dir, destination) == 0,
			"a successful overwrite must not leave a scratch file behind");

		std::error_code cleanup_status;
		std::filesystem::remove(destination, cleanup_status);
	}

	// Case 3: a destination held open with only FILE_SHARE_READ blocks both
	// the direct rename and the remove-then-rename retry (no FILE_SHARE_DELETE
	// is granted). WriteFileAtomically must accept it once the sizes match,
	// and must not leave a scratch file behind.
	{
		const std::filesystem::path destination = scratch_dir / "case3.bin";
		const std::vector<char> payload(37, 'Q');
		const HANDLE blocking_handle = CreateFileW(destination.c_str(), GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		Require(blocking_handle != INVALID_HANDLE_VALUE, "test setup must open the blocking handle");
		DWORD written = 0;
		const BOOL wrote_ok = WriteFile(blocking_handle, payload.data(), static_cast<DWORD>(payload.size()),
			&written, nullptr);
		Require(wrote_ok != 0 && written == payload.size(),
			"test setup must size the blocked destination to the payload size");
		// WriteFileAtomically's size-match acceptance reads the destination's
		// size through a fresh handle; force the write to disk so that read
		// cannot race an unflushed size through this handle.
		Require(FlushFileBuffers(blocking_handle) != 0, "test setup must flush the blocked destination to disk");

		std::string error;
		const bool wrote = depthgen::orthost_detail::WriteFileAtomically(destination, payload.data(),
			payload.size(), &error);
		Require(wrote, "a same-size destination blocked from rename and delete must be accepted");
		Require(CountEntriesExcluding(scratch_dir, destination) == 0,
			"a blocked-but-accepted write must not leave a scratch file behind");

		CloseHandle(blocking_handle);
		std::error_code cleanup_status;
		std::filesystem::remove(destination, cleanup_status);
	}

	std::error_code cleanup_status;
	std::filesystem::remove_all(scratch_dir, cleanup_status);

	std::cout << "DepthGen ONNX Runtime host tests passed\n";
	return 0;
}
