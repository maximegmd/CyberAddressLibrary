#pragma once

#include <map>
#include <fstream>
#include <stdio.h>
#include <filesystem>
#include <Windows.h>

#pragma comment(lib, "version.lib")

struct VersionDb
{
	VersionDb() { Clear(); }
	~VersionDb() { }

private:

	using TMap = std::map<uintptr_t, uintptr_t>;

	TMap m_idToOffset;
	TMap m_offsetToId;
	uint32_t m_version[4];
	std::string m_versionStr;
	std::string m_moduleName;
	uintptr_t m_base;

	template <typename T>
	static T read(std::ifstream& file)
	{
		T v;
		file.read((char*)&v, sizeof(T));
		return v;
	}

	static void* ToPointer(uintptr_t v)
	{
		return (void*)v;
	}

	static uintptr_t FromPointer(void* ptr)
	{
		return (uintptr_t)ptr;
	}
	
	static bool ParseVersionFromString(const char* ptr, int& major, int& minor, int& revision, int& build)
	{
		return sscanf_s(ptr, "%d.%d.%d.%d", &major, &minor, &revision, &build) == 4 && ((major != 1 && major != 0) || minor != 0 || revision != 0 || build != 0);
	}

public:

	[[nodiscard]] const std::string& GetModuleName() const { return m_moduleName; }
	[[nodiscard]] const std::string& GetLoadedVersionString() const { return m_versionStr; }
	[[nodiscard]] uintptr_t GetModuleBase() const { return m_base; }
	[[nodiscard]] const TMap& GetOffsetMap() const { return m_idToOffset; }

	[[nodiscard]] void* FindAddressById(uint64_t id) const
	{
		uintptr_t b = m_base;
		if (b == 0)
			return nullptr;

		uintptr_t offset = 0;
		if (!FindOffsetById(id, offset))
			return nullptr;

		return ToPointer(b + offset);
	}

	[[nodiscard]] bool FindOffsetById(uint64_t id, uintptr_t& result) const
	{
		auto itr = m_idToOffset.find(id);
		if (itr != m_idToOffset.end())
		{
			result = itr->second;
			return true;
		}
		return false;
	}

	[[nodiscard]] bool FindIdByAddress(void* ptr, uint64_t& result) const
	{
		uintptr_t b = m_base;
		if (b == 0)
			return false;

		uintptr_t addr = FromPointer(ptr);
		return FindIdByOffset(addr - b, result);
	}

	[[nodiscard]] bool FindIdByOffset(uint64_t offset, uintptr_t& result) const
	{
		auto itr = m_offsetToId.find(offset);
		if (itr == m_offsetToId.end())
			return false;

		result = itr->second;
		return true;
	}

	[[nodiscard]] bool GetExecutableVersion(int& major, int& minor, int& revision, int& build) const
	{
		TCHAR szVersionFile[MAX_PATH];
		GetModuleFileName(nullptr, szVersionFile, MAX_PATH);

		DWORD verHandle = 0;
		UINT size = 0;
		LPBYTE lpBuffer = nullptr;
		DWORD verSize = GetFileVersionInfoSize(szVersionFile, &verHandle);

		if (verSize != 0 && verHandle != 0)
		{
			LPSTR verData = (LPSTR)_malloca(verSize);
			if (verData == nullptr)
				return false;

			if (GetFileVersionInfo(szVersionFile, verHandle, verSize, verData))
			{
				{
					char * vstr = NULL;
					UINT vlen = 0;
					if (VerQueryValueA(verData, "\\StringFileInfo\\040904B0\\ProductVersion", (LPVOID*)&vstr, &vlen) && vlen && vstr && *vstr)
					{
						if (ParseVersionFromString(vstr, major, minor, revision, build))
						{
							return true;
						}
					}
				}

				{
					char * vstr = NULL;
					UINT vlen = 0;
					if (VerQueryValueA(verData, "\\StringFileInfo\\040904B0\\FileVersion", (LPVOID*)&vstr, &vlen) && vlen && vstr && *vstr)
					{
						if (ParseVersionFromString(vstr, major, minor, revision, build))
						{
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	void GetLoadedVersion(int& major, int& minor, int& revision, int& build) const
	{
		major = m_version[0];
		minor = m_version[1];
		revision = m_version[2];
		build = m_version[3];
	}

	void Clear()
	{
		m_idToOffset.clear();
		m_offsetToId.clear();
		for (int i = 0; i < 4; i++) m_version[i] = 0;
		m_moduleName = std::string();
		m_base = 0;
	}

	[[nodiscard]] bool Load()
	{
		int major, minor, revision, build;
		
		if (!GetExecutableVersion(major, minor, revision, build))
			return false;

		return Load(major, minor, revision, build);
	}

	[[nodiscard]] bool Load(int major, int minor, int revision, int build)
	{
		Clear();

		char fileName[256];
		_snprintf_s(fileName, 256, "versionlib-%d-%d-%d-%d.bin", major, minor, revision, build);

		auto path = std::filesystem::current_path() / ".." / ".." / "red4ext" / "versionlib" / fileName;
		path = path.lexically_normal();

		std::ifstream file(path, std::ios::binary);
		if (!file.good())
			return false;

		int format = read<int>(file);

		if (format != 2)
			return false;

		for (int i = 0; i < 4; i++)
			m_version[i] = read<int>(file);

		{
			char verName[64];
			_snprintf_s(verName, 64, "%d.%d.%d.%d", m_version[0], m_version[1], m_version[2], m_version[3]);
			m_versionStr = verName;
		}

		int tnLen = read<int>(file);

		if (tnLen < 0 || tnLen >= 0x10000)
			return false;

		if(tnLen > 0)
		{
			char* tnbuf = (char*)_malloca(tnLen + 1);
			file.read(tnbuf, tnLen);
			tnbuf[tnLen] = '\0';
			m_moduleName = tnbuf;
		}

		{
			HMODULE handle = GetModuleHandleA(m_moduleName.empty() ? nullptr : m_moduleName.c_str());
			m_base = (uintptr_t)handle;
		}

		int ptrSize = read<int>(file);
		int addrCount = read<int>(file);

		unsigned char type, low, high;
		unsigned char b1, b2;
		unsigned short w1, w2;
		unsigned int d1, d2;
		uintptr_t q1, q2;
		uint64_t pvid = 0;
		uintptr_t poffset = 0;
		uintptr_t tpoffset;
		for (int i = 0; i < addrCount; i++)
		{
			type = read<unsigned char>(file);
			low = type & 0xF;
			high = type >> 4;

			switch (low)
			{
			case 0: q1 = read<uint64_t>(file); break;
			case 1: q1 = pvid + 1; break;
			case 2: b1 = read<unsigned char>(file); q1 = pvid + b1; break;
			case 3: b1 = read<unsigned char>(file); q1 = pvid - b1; break;
			case 4: w1 = read<unsigned short>(file); q1 = pvid + w1; break;
			case 5: w1 = read<unsigned short>(file); q1 = pvid - w1; break;
			case 6: w1 = read<unsigned short>(file); q1 = w1; break;
			case 7: d1 = read<unsigned int>(file); q1 = d1; break;
			default:
			{
				Clear();
				return false;
			}
			}

			tpoffset = (high & 8) != 0 ? (poffset / (uintptr_t)ptrSize) : poffset;

			switch (high & 7)
			{
			case 0: q2 = read<uint64_t>(file); break;
			case 1: q2 = tpoffset + 1; break;
			case 2: b2 = read<unsigned char>(file); q2 = tpoffset + b2; break;
			case 3: b2 = read<unsigned char>(file); q2 = tpoffset - b2; break;
			case 4: w2 = read<unsigned short>(file); q2 = tpoffset + w2; break;
			case 5: w2 = read<unsigned short>(file); q2 = tpoffset - w2; break;
			case 6: w2 = read<unsigned short>(file); q2 = w2; break;
			case 7: d2 = read<unsigned int>(file); q2 = d2; break;
			default: throw std::exception();
			}

			if ((high & 8) != 0)
				q2 *= (uint64_t)ptrSize;

			m_idToOffset[q1] = q2;
			m_offsetToId[q2] = q1;

			poffset = q2;
			pvid = q1;
		}

		return true;
	}

	[[nodiscard]] bool Dump(std::ostream& aOut)
	{
		if (!aOut.good())
			return false;

		for (auto itr = m_idToOffset.begin(); itr != m_idToOffset.end(); itr++)
		{
			aOut << std::dec;
			aOut << itr->first;
			aOut << '\t';
			aOut << std::hex;
			aOut << itr->second;
			aOut << '\n';
		}

		return true;
	}
};
