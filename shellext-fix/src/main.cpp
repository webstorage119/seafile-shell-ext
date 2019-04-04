#include <string>
#include "registry.h"

namespace {
	const std::wstring kRGISTRYPATH = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers";
} //namespace

int main()
{
	cleanRegistryItem(HKEY_LOCAL_MACHINE, kRGISTRYPATH);
}