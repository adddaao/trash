//  trash_windows.cpp
//
//  Windows implementation for moving files/folders to the Recycle Bin.
//

#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <versionhelpers.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#ifndef FOFX_RECYCLEONDELETE
#define FOFX_RECYCLEONDELETE 0x00080000
#endif
#ifndef FOFX_ADDUNDORECORD
#define FOFX_ADDUNDORECORD 0x20000000
#endif
#ifndef FOFX_EARLYFAILURE
#define FOFX_EARLYFAILURE 0x00100000
#endif

static const int VERSION_MAJOR = 0;
static const int VERSION_MINOR = 9;
static const int VERSION_BUILD = 2;

static bool arg_verbose = false;

template <typename T>
static void SafeRelease(T **pp)
{
    if (*pp != NULL)
    {
        (*pp)->Release();
        *pp = NULL;
    }
}

static void PrintLastError(const wchar_t *prefix, HRESULT hr)
{
    wchar_t *message = NULL;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        hr,
        0,
        (LPWSTR)&message,
        0,
        NULL);

    if (message != NULL)
    {
        fwprintf(stderr, L"trash: %ls (%ld: %ls)\n", prefix, (long)hr, message);
        LocalFree(message);
    }
    else
    {
        fwprintf(stderr, L"trash: %ls (%ld)\n", prefix, (long)hr);
    }
}

static bool FileExists(const wchar_t *path)
{
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static wchar_t *GetFullPathForShell(const wchar_t *path)
{
    DWORD len = GetFullPathNameW(path, 0, NULL, NULL);
    if (len == 0)
        return NULL;

    wchar_t *fullPath = (wchar_t *)LocalAlloc(LPTR, sizeof(wchar_t) * len);
    if (fullPath == NULL)
        return NULL;

    if (GetFullPathNameW(path, len, fullPath, NULL) == 0)
    {
        LocalFree(fullPath);
        return NULL;
    }

    return fullPath;
}

static wchar_t *ResolveSubstPath(const wchar_t *path)
{
    if (path[0] == L'\0' || path[1] != L':')
        return NULL;

    wchar_t drive[3] = {path[0], L':', L'\0'};
    wchar_t target[MAX_PATH];
    DWORD length = QueryDosDeviceW(drive, target, ARRAYSIZE(target));
    if (length == 0)
        return NULL;

    if (wcsncmp(target, L"\\??\\", 4) != 0)
        return NULL;

    const wchar_t *realPrefix = target + 4;
    const wchar_t *rest = path + 2;
    size_t prefixLength = wcslen(realPrefix);
    size_t restLength = wcslen(rest);
    wchar_t *resolved = (wchar_t *)LocalAlloc(LPTR, sizeof(wchar_t) * (prefixLength + restLength + 1));
    if (resolved == NULL)
        return NULL;

    wmemcpy(resolved, realPrefix, prefixLength);
    wmemcpy(resolved + prefixLength, rest, restLength + 1);
    return resolved;
}

static void FreeIdListArray(PCIDLIST_ABSOLUTE *list, int count)
{
    if (list == NULL)
        return;

    for (int i = 0; i < count; i++)
    {
        if (list[i] != NULL)
            CoTaskMemFree((LPVOID)list[i]);
    }
    LocalFree(list);
}

static HRESULT MoveFilesToRecycleBin(wchar_t **paths, int count)
{
    IFileOperation *operation = NULL;
    IShellItemArray *items = NULL;
    PCIDLIST_ABSOLUTE *idLists = NULL;
    HRESULT hr = CoCreateInstance(
        CLSID_FileOperation,
        NULL,
        CLSCTX_ALL,
        IID_PPV_ARGS(&operation));
    if (FAILED(hr))
        return hr;

    FILEOP_FLAGS flags = FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION | FOFX_EARLYFAILURE;
    if (IsWindows8OrGreater())
        flags |= FOFX_ADDUNDORECORD | FOFX_RECYCLEONDELETE;
    else
        flags |= FOF_ALLOWUNDO;

    hr = operation->SetOperationFlags(flags);
    if (FAILED(hr))
    {
        SafeRelease(&operation);
        return hr;
    }

    idLists = (PCIDLIST_ABSOLUTE *)LocalAlloc(LPTR, sizeof(PCIDLIST_ABSOLUTE) * count);
    if (idLists == NULL)
    {
        SafeRelease(&operation);
        return E_OUTOFMEMORY;
    }

    for (int i = 0; i < count; i++)
    {
        wchar_t *fullPath = GetFullPathForShell(paths[i]);
        if (fullPath == NULL)
        {
            FreeIdListArray(idLists, count);
            SafeRelease(&operation);
            return HRESULT_FROM_WIN32(GetLastError());
        }

        wchar_t *currentPath = fullPath;
        wchar_t *resolvedPath = NULL;
        while ((resolvedPath = ResolveSubstPath(currentPath)) != NULL)
        {
            if (currentPath != fullPath)
                LocalFree(currentPath);
            currentPath = resolvedPath;
        }

        idLists[i] = ILCreateFromPathW(currentPath);
        if (currentPath != fullPath)
            LocalFree(currentPath);
        LocalFree(fullPath);

        if (idLists[i] == NULL)
        {
            FreeIdListArray(idLists, count);
            SafeRelease(&operation);
            return HRESULT_FROM_WIN32(GetLastError());
        }
    }

    hr = SHCreateShellItemArrayFromIDLists((UINT)count, idLists, &items);
    if (FAILED(hr))
    {
        FreeIdListArray(idLists, count);
        SafeRelease(&operation);
        return hr;
    }

    hr = operation->DeleteItems(items);
    if (FAILED(hr))
    {
        SafeRelease(&items);
        FreeIdListArray(idLists, count);
        SafeRelease(&operation);
        return hr;
    }

    hr = operation->PerformOperations();
    if (SUCCEEDED(hr))
    {
        BOOL aborted = FALSE;
        HRESULT abortHr = operation->GetAnyOperationsAborted(&aborted);
        if (SUCCEEDED(abortHr) && aborted)
            hr = HRESULT_FROM_WIN32(ERROR_CANCELLED);
    }

    SafeRelease(&items);
    FreeIdListArray(idLists, count);
    SafeRelease(&operation);
    return hr;
}

static HRESULT ListTrashContents(bool showAdditionalInfo)
{
    IShellFolder *desktop = NULL;
    IShellFolder *recycleBin = NULL;
    IEnumIDList *enumList = NULL;
    LPITEMIDLIST recycleBinPidl = NULL;
    ULONG eaten = 0;
    HRESULT hr = SHGetDesktopFolder(&desktop);
    if (FAILED(hr))
        return hr;

    wchar_t recycleBinName[] = L"::{645FF040-5081-101B-9F08-00AA002F954E}";
    hr = desktop->ParseDisplayName(
        NULL,
        NULL,
        recycleBinName,
        &eaten,
        &recycleBinPidl,
        NULL);
    if (FAILED(hr))
        goto done;

    hr = desktop->BindToObject(recycleBinPidl, NULL, IID_PPV_ARGS(&recycleBin));
    if (FAILED(hr))
        goto done;

    hr = recycleBin->EnumObjects(NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &enumList);
    if (FAILED(hr))
        goto done;

    for (;;)
    {
        LPITEMIDLIST child = NULL;
        ULONG fetched = 0;
        hr = enumList->Next(1, &child, &fetched);
        if (hr != S_OK)
        {
            hr = S_OK;
            break;
        }

        STRRET str;
        ZeroMemory(&str, sizeof(str));
        if (SUCCEEDED(recycleBin->GetDisplayNameOf(child, SHGDN_NORMAL, &str)))
        {
            wchar_t name[MAX_PATH];
            if (SUCCEEDED(StrRetToBufW(&str, child, name, ARRAYSIZE(name))))
                wprintf(L"%ls\n", name);
        }
        CoTaskMemFree(child);
    }

    if (showAdditionalInfo)
    {
        SHQUERYRBINFO info;
        ZeroMemory(&info, sizeof(info));
        info.cbSize = sizeof(info);
        if (SUCCEEDED(SHQueryRecycleBinW(NULL, &info)))
            wprintf(
                L"\nTotal: %llu bytes in %llu item(s)\n",
                (unsigned long long)info.i64Size,
                (unsigned long long)info.i64NumItems);
    }

done:
    if (recycleBinPidl != NULL)
        CoTaskMemFree(recycleBinPidl);
    SafeRelease(&enumList);
    SafeRelease(&recycleBin);
    SafeRelease(&desktop);
    return hr;
}

static HRESULT EmptyTrash(bool skipPrompt)
{
    DWORD flags = 0;
    if (skipPrompt)
        flags |= SHERB_NOCONFIRMATION;
    if (!arg_verbose)
        flags |= SHERB_NOPROGRESSUI;

    return SHEmptyRecycleBinW(NULL, NULL, flags);
}

static wchar_t *BaseName(wchar_t *path)
{
    wchar_t *slash = wcsrchr(path, L'\\');
    wchar_t *forwardSlash = wcsrchr(path, L'/');
    wchar_t *base = slash;
    if (base == NULL || (forwardSlash != NULL && forwardSlash > base))
        base = forwardSlash;
    return base == NULL ? path : base + 1;
}

static void PrintUsage(wchar_t *myBasename)
{
    wprintf(L"usage: %ls [-vlesyF] <file> [<file> ...]\n", myBasename);
    wprintf(
        L"\n"
        L"  Move files/folders to the trash.\n"
        L"\n"
        L"  Options to use with <file>:\n"
        L"\n"
        L"  -v  Be verbose (show files as they are trashed, or if\n"
        L"      used with the -l option, show additional information\n"
        L"      about the trash contents)\n"
        L"  -F  Accepted for compatibility.\n"
        L"\n"
        L"  Stand-alone options (to use without <file>):\n"
        L"\n"
        L"  -l  List items currently in the trash (add the -v option\n"
        L"      to see additional information)\n"
        L"  -e  Empty the trash (asks for confirmation)\n"
        L"  -s  Empty the trash (asks for confirmation)\n"
        L"  -y  Skips the confirmation prompt for -e and -s.\n"
        L"      CAUTION: Deletes permanently instantly.\n"
        L"\n"
        L"  Options supported by `rm` are silently accepted.\n"
        L"\n"
        L"Version %d.%d.%d\n"
        L"Copyright (c) 2010-2018 Ali Rantakari, http://hasseg.org/trash\n"
        L"\n",
        VERSION_MAJOR,
        VERSION_MINOR,
        VERSION_BUILD);
}

int wmain(int argc, wchar_t *argv[])
{
    int exitValue = 0;
    bool arg_list = false;
    bool arg_empty = false;
    bool arg_emptySecurely = false;
    bool arg_skipPrompt = false;

    if (argc == 1)
    {
        PrintUsage(BaseName(argv[0]));
        return 0;
    }

    int firstPath = argc;
    for (int i = 1; i < argc; i++)
    {
        if (wcscmp(argv[i], L"--") == 0)
        {
            firstPath = i + 1;
            break;
        }

        if (argv[i][0] != L'-' || argv[i][1] == L'\0')
        {
            firstPath = i;
            break;
        }

        for (int j = 1; argv[i][j] != L'\0'; j++)
        {
            switch (argv[i][j])
            {
                case L'v': arg_verbose = true; break;
                case L'l': arg_list = true; break;
                case L'e': arg_empty = true; break;
                case L's': arg_emptySecurely = true; break;
                case L'y': arg_skipPrompt = true; break;
                case L'F': break;
                case L'd':
                case L'f':
                case L'i':
                case L'r':
                case L'P':
                case L'R':
                case L'W':
                    break;
                default:
                    PrintUsage(BaseName(argv[0]));
                    return 1;
            }
        }
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        PrintLastError(L"can not initialize COM", hr);
        return 1;
    }

    if (arg_list)
    {
        hr = ListTrashContents(arg_verbose);
        CoUninitialize();
        if (FAILED(hr))
        {
            PrintLastError(L"can not list trash contents", hr);
            return 1;
        }
        return 0;
    }

    if (arg_empty || arg_emptySecurely)
    {
        hr = EmptyTrash(arg_skipPrompt);
        CoUninitialize();
        if (FAILED(hr))
        {
            PrintLastError(L"can not empty trash", hr);
            return 1;
        }
        return 0;
    }

    if (firstPath == argc)
    {
        PrintUsage(BaseName(argv[0]));
        CoUninitialize();
        return 1;
    }

    wchar_t **paths = (wchar_t **)LocalAlloc(LPTR, sizeof(wchar_t *) * (argc - firstPath));
    int pathCount = 0;
    if (paths == NULL)
    {
        CoUninitialize();
        return 1;
    }

    for (int i = firstPath; i < argc; i++)
    {
        if (!FileExists(argv[i]))
        {
            fwprintf(stderr, L"trash: %ls: path does not exist\n", argv[i]);
            exitValue = 1;
            continue;
        }
        paths[pathCount++] = argv[i];
    }

    if (pathCount > 0)
    {
        hr = MoveFilesToRecycleBin(paths, pathCount);
        if (FAILED(hr))
        {
            PrintLastError(L"can not move to trash", hr);
            exitValue = 1;
        }
        else if (arg_verbose)
        {
            for (int i = 0; i < pathCount; i++)
                wprintf(L"%ls\n", paths[i]);
        }
    }

    LocalFree(paths);
    CoUninitialize();
    return exitValue;
}
