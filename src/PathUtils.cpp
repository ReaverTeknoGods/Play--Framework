#include "PathUtils.h"
#include "maybe_unused.h"
#include "Utf8.h"

#ifdef _WIN32
#include <windows.h>
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__) || defined(__FreeBSD__)
#include <unistd.h>
#endif

using namespace Framework;

// Helper function to get executable directory across platforms
static fs::path GetExecutableDirectory()
{
#ifdef _WIN32
	wchar_t path[MAX_PATH];
	GetModuleFileNameW(NULL, path, MAX_PATH);
	fs::path exePath(path);
	return exePath.parent_path();
#elif defined(__APPLE__)
	char path[1024];
	uint32_t size = sizeof(path);
	if(_NSGetExecutablePath(path, &size) == 0)
	{
		return fs::path(path).parent_path();
	}
	// Fallback to current directory
	return fs::current_path();
#elif defined(__linux__) || defined(__FreeBSD__)
	char path[1024];
	ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if(len != -1)
	{
		path[len] = '\0';
		return fs::path(path).parent_path();
	}
	// Fallback to current directory
	return fs::current_path();
#else
	// Fallback for other platforms
	return fs::current_path();
#endif
}

// Helper function to get TeknoParrot base directory
static fs::path GetTeknoParrotBaseDirectory()
{
	static fs::path teknoParrotPath;
	static bool initialized = false;

	if(!initialized)
	{
		teknoParrotPath = GetExecutableDirectory() / "TeknoParrot";

		// Ensure TeknoParrot directory exists
		std::error_code ec;
		if(!fs::exists(teknoParrotPath, ec))
		{
			fs::create_directories(teknoParrotPath, ec);
		}

		initialized = true;
	}

	return teknoParrotPath;
}

#ifdef _WIN32

#if WINAPI_FAMILY_ONE_PARTITION(WINAPI_FAMILY, WINAPI_PARTITION_APP)

fs::path PathUtils::GetPersonalDataPath()
{
	auto teknoParrotPath = GetTeknoParrotBaseDirectory() / "AppData" / "Local";
	EnsurePathExists(teknoParrotPath);
	return teknoParrotPath;
}

#else // !WINAPI_PARTITION_APP

#include <shlobj.h>

fs::path PathUtils::GetPathFromCsidl(int csidl)
{
	// Instead of using Windows CSIDL paths, redirect to TeknoParrot subdirectories
	auto basePath = GetTeknoParrotBaseDirectory();

	switch(csidl & 0x00ff)
	{ // Remove flags to get base CSIDL
	case CSIDL_APPDATA:
		return basePath / "AppData" / "Roaming";
	case CSIDL_LOCAL_APPDATA:
		return basePath / "AppData" / "Local";
	case CSIDL_PERSONAL:
		return basePath / "Documents";
	default:
		return basePath / "AppData" / "Local";
	}
}

fs::path PathUtils::GetRoamingDataPath()
{
	auto path = GetTeknoParrotBaseDirectory() / "AppData" / "Roaming";
	EnsurePathExists(path);
	return path;
}

fs::path PathUtils::GetPersonalDataPath()
{
	auto path = GetTeknoParrotBaseDirectory() / "Documents";
	EnsurePathExists(path);
	return path;
}

fs::path PathUtils::GetAppResourcesPath()
{
	// Keep resources relative to executable
	return GetExecutableDirectory();
}

fs::path PathUtils::GetCachePath()
{
	auto path = GetTeknoParrotBaseDirectory() / "AppData" / "Local" / "Cache";
	EnsurePathExists(path);
	return path;
}

#endif // !WINAPI_PARTITION_APP

#elif defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include <mach-o/dyld.h>

fs::path PathUtils::GetRoamingDataPath()
{
	auto path = GetTeknoParrotBaseDirectory() / "AppData" / "Roaming";
	EnsurePathExists(path);
	return path;
}

fs::path PathUtils::GetAppResourcesPath()
{
	// Keep resources relative to executable or use bundle if available
	NSBundle* bundle = [NSBundle mainBundle];
	if(bundle)
	{
		NSString* bundlePath = [bundle resourcePath];
		if(bundlePath)
		{
			return fs::path([bundlePath fileSystemRepresentation]);
		}
	}
	return GetExecutableDirectory();
}

fs::path PathUtils::GetPersonalDataPath()
{
	auto path = GetTeknoParrotBaseDirectory() / "Documents";
	EnsurePathExists(path);
	return path;
}

fs::path PathUtils::GetCachePath()
{
	auto path = GetTeknoParrotBaseDirectory() / "Cache";
	EnsurePathExists(path);
	return path;
}

#elif defined(__ANDROID__)

static fs::path s_filesDirPath;
static fs::path s_cacheDirPath;

fs::path PathUtils::GetAppResourcesPath()
{
	//This won't work for Android - return TeknoParrot base
	return GetTeknoParrotBaseDirectory();
}

fs::path PathUtils::GetRoamingDataPath()
{
	auto path = GetTeknoParrotBaseDirectory() / "AppData" / "Roaming";
	EnsurePathExists(path);
	return path;
}

fs::path PathUtils::GetPersonalDataPath()
{
	auto path = GetTeknoParrotBaseDirectory() / "Documents";
	EnsurePathExists(path);
	return path;
}

fs::path PathUtils::GetCachePath()
{
	auto path = GetTeknoParrotBaseDirectory() / "Cache";
	EnsurePathExists(path);
	return path;
}

void PathUtils::SetFilesDirPath(const char* filesDirPath)
{
	// Note: For full Android portability, you might want to override
	// GetTeknoParrotBaseDirectory() to use this path instead
	s_filesDirPath = filesDirPath;
}

void PathUtils::SetCacheDirPath(const char* cacheDirPath)
{
	s_cacheDirPath = cacheDirPath;
}

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__EMSCRIPTEN__)

#include <unistd.h>

fs::path PathUtils::GetAppResourcesPath()
{
	// Check for special environments first, but fall back to executable directory
	if(getenv("APPIMAGE"))
	{
		auto appImagePath = fs::path(getenv("APPDIR")) / "usr/share";
		std::error_code ec;
		if(fs::exists(appImagePath, ec) && !ec)
		{
			return appImagePath;
		}
	}

	//Flatpak
	auto flatpakPath = fs::path("/app/share");
	std::error_code existsErrorCode;
	bool exists = fs::exists(flatpakPath, existsErrorCode);
	if(!existsErrorCode && exists)
	{
		return flatpakPath;
	}

	// Default to executable directory for portable installation
	return GetExecutableDirectory();
}

fs::path PathUtils::GetRoamingDataPath()
{
	auto path = GetTeknoParrotBaseDirectory() / "AppData" / "Roaming";
	EnsurePathExists(path);
	return path;
}

fs::path PathUtils::GetPersonalDataPath()
{
	auto path = GetTeknoParrotBaseDirectory() / "Documents";
	EnsurePathExists(path);
	return path;
}

fs::path PathUtils::GetCachePath()
{
	auto path = GetTeknoParrotBaseDirectory() / "Cache";
	EnsurePathExists(path);
	return path;
}

#else // !DEFINED(__ANDROID__) || !DEFINED(__APPLE__) || !DEFINED(__linux__) || !DEFINED(__FreeBSD__)

#include <pwd.h>

fs::path PathUtils::GetPersonalDataPath()
{
	auto path = GetTeknoParrotBaseDirectory() / "Documents";
	EnsurePathExists(path);
	return path;
}

#endif

void PathUtils::EnsurePathExists(const fs::path& path)
{
	typedef fs::path PathType;
	PathType buildPath;
	for(PathType::iterator pathIterator(path.begin());
	    pathIterator != path.end(); pathIterator++)
	{
		buildPath /= (*pathIterator);
		std::error_code existsErrorCode;
		bool exists = fs::exists(buildPath, existsErrorCode);
		if(existsErrorCode)
		{
#ifdef _WIN32
			if(existsErrorCode.value() == ERROR_ACCESS_DENIED)
			{
				//No problem, it exists, but we just don't have access
				continue;
			}
			else if(existsErrorCode.value() == ERROR_FILE_NOT_FOUND)
			{
				exists = false;
			}
#else
			if(existsErrorCode.value() == ENOENT)
			{
				exists = false;
			}
#endif
			else
			{
				throw std::runtime_error("Couldn't ensure that path exists.");
			}
		}
		if(!exists)
		{
			fs::create_directory(buildPath);
		}
	}
}

////////////////////////////////////////////
//NativeString <-> Path Function Utils
////////////////////////////////////////////

template <typename StringType>
FRAMEWORK_MAYBE_UNUSED static std::string GetNativeStringFromPathInternal(const StringType&);

template <>
std::string GetNativeStringFromPathInternal(const std::string& str)
{
	return str;
}

template <>
std::string GetNativeStringFromPathInternal(const std::wstring& str)
{
	return Framework::Utf8::ConvertTo(str);
}

template <typename StringType>
FRAMEWORK_MAYBE_UNUSED static StringType GetPathFromNativeStringInternal(const std::string&);

template <>
std::string GetPathFromNativeStringInternal(const std::string& str)
{
	return str;
}

template <>
std::wstring GetPathFromNativeStringInternal(const std::string& str)
{
	return Framework::Utf8::ConvertFrom(str);
}

////////////////////////////////////////////
//NativeString <-> Path Function Implementations
////////////////////////////////////////////

std::string PathUtils::GetNativeStringFromPath(const fs::path& path)
{
	return GetNativeStringFromPathInternal(path.native());
}

fs::path PathUtils::GetPathFromNativeString(const std::string& str)
{
	auto cvtStr = GetPathFromNativeStringInternal<fs::path::string_type>(str);
	return fs::path(cvtStr);
}