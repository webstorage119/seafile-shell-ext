/******************************** Module Header ********************************\
Module Name:  RecipeThumbnailProvider.cpp
Project:      CppShellExtThumbnailHandler
Copyright (c) Microsoft Corporation.

The code sample demonstrates the C++ implementation of a thumbnail handler
for a new file type registered with the .recipe extension.

A thumbnail image handler provides an image to represent the item. It lets you
customize the thumbnail of files with a specific file extension. Windows Vista
and newer operating systems make greater use of file-specific thumbnail images
than earlier versions of Windows. Thumbnails of 32-bit resolution and as large
as 256x256 pixels are often used. File format owners should be prepared to
display their thumbnails at that size.

The example thumbnail handler implements the IInitializeWithStream and
IThumbnailProvider interfaces, and provides thumbnails for .recipe files.
The .recipe file type is simply an XML file registered as a unique file name
extension. It includes an element called "Picture", embedding an image file.
The thumbnail handler extracts the embedded image and asks the Shell to
display it as a thumbnail.

This source is subject to the Microsoft Public License.
See http://www.microsoft.com/opensource/licenses.mspx#Ms-PL.
All other rights reserved.

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\*******************************************************************************/

#include "RecipeThumbnailProvider.h"
#include <Shlwapi.h>
#include <Wincrypt.h>   // For CryptStringToBinary.
#include <msxml6.h>
#include <shobjidl.h>   //For IShellItemImageFactory

#include <windows.h>
#include <algorithm>

#include "ext-utils.h"
#include "commands.h"
#include "log.h"

// depend extension tool file

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "msxml6.lib")
#pragma comment(lib, "User32.lib")


extern HINSTANCE g_hInst;
extern long g_cDllRef;


RecipeThumbnailProvider::RecipeThumbnailProvider() : m_cRef(1)
{
    InterlockedIncrement(&g_cDllRef);
}


RecipeThumbnailProvider::~RecipeThumbnailProvider()
{
    InterlockedDecrement(&g_cDllRef);
}


#pragma region IUnknown

// Query to the interface the component supported.
IFACEMETHODIMP RecipeThumbnailProvider::QueryInterface(REFIID riid, void **ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(RecipeThumbnailProvider, IThumbnailProvider),
        QITABENT(RecipeThumbnailProvider, IInitializeWithFile),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

// Increase the reference count for an interface on an object.
IFACEMETHODIMP_(ULONG) RecipeThumbnailProvider::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

// Decrease the reference count for an interface on an object.
IFACEMETHODIMP_(ULONG) RecipeThumbnailProvider::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (0 == cRef) {
        delete this;
    }

    return cRef;
}

#pragma endregion


#pragma region IInitializeWithFile

// Initializes the thumbnail handler with a file path
IFACEMETHODIMP RecipeThumbnailProvider::Initialize(LPCWSTR pfilePath, DWORD grfMode)
{
    // A handler instance should be initialized only once in its lifetime.
    HRESULT hr = HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
    filepath_ = seafile::utils::wStringToUtf8(pfilePath);
    seaf_ext_log("initialize with file: %s", filepath_.c_str());
    return 1;
}

#pragma endregion


#pragma region IThumbnailProvider

// Gets a thumbnail image and alpha type. The GetThumbnail is called with the
// largest desired size of the image, in pixels. Although the parameter is
// called cx, this is used as the maximum size of both the x and y dimensions.
// If the retrieved thumbnail is not square, then the longer axis is limited
// by cx and the aspect ratio of the original image respected. On exit,
// GetThumbnail provides a handle to the retrieved image. It also provides a
// value that indicates the color format of the image and whether it has
// valid alpha information.
IFACEMETHODIMP RecipeThumbnailProvider::GetThumbnail(UINT cx, HBITMAP *phbmp,
    WTS_ALPHATYPE *pdwAlpha)
{
    // Get diskletter command
    seafile::GetSeadriveMountLetter get_disk_letter_cmd;
    seafile::DISK_LETTER_TYPE seadrive_mount_disk_letter;

    if (!get_disk_letter_cmd.sendAndWait(&seadrive_mount_disk_letter)){

        seaf_ext_log("send get mount disk letter command failed");
        seadrive_mount_disk_letter.clear();
    }

    std::string current_disk_letter = seafile::utils::getDiskLetterName(filepath_);
    transform(current_disk_letter.begin(), current_disk_letter.end(), current_disk_letter.begin(), ::tolower);

    if (seadrive_mount_disk_letter == current_disk_letter) {
        // Get cache status
        seafile::GetCachedStatusCommand cmd(filepath_);
        bool status;
        if (!cmd.sendAndWait(&status)) {
            seaf_ext_log("send get file cached status failed");
        }

        if (status) {
            seaf_ext_log("the file [%s] have been cached", filepath_.c_str());

             GetsHBITMAPFromFile(seafile::utils::localeToWString(filepath_), phbmp);
             freeBitmapResource();
        } else {
            // TODO: 文件未进行缓存，请求进行缩略图的请求
            seaf_ext_log("the file have no been cached");
        }

    } else {
        
        GetsHBITMAPFromFile(seafile::utils::localeToWString(filepath_), phbmp);
        freeBitmapResource();
        seaf_ext_log("current dir is not in seadrive dir,\
            current dir in diskletter is [%s], seadrive mount diskletter is [%s]",\
            current_disk_letter.c_str(),\
            seadrive_mount_disk_letter.c_str()
        );
    }

    cx = 64;
    return 1;
}

#pragma endregion

#pragma region Helper Functions

void RecipeThumbnailProvider::GetsHBITMAPFromFile(LPCWSTR pfilePath, HBITMAP* hbmap)
{
    hbitmap_ = Gdiplus::Bitmap::FromFile(pfilePath);
    if (hbitmap_ == NULL) {
        seaf_ext_log("load bitmap from file failed");
        return;
    }
    Gdiplus::Status status = hbitmap_->GetHBITMAP(NULL, hbmap);

    if (status != Gdiplus::Ok) {
        seaf_ext_log("get hbitmap failed");
    }

    return;
}

void RecipeThumbnailProvider::freeBitmapResource() {

    if (hbitmap_) {
        delete hbitmap_;
        hbitmap_ = NULL;
    }
    return;
}

#pragma endregion
