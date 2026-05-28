//  trash_wsl.cpp
//
//  WSL implementation that sends files to the Windows Recycle Bin.
//

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <vector>

static const int VERSION_MAJOR = 0;
static const int VERSION_MINOR = 9;
static const int VERSION_BUILD = 2;

static bool arg_verbose = false;

static bool isWsl()
{
    FILE *file = fopen("/proc/version", "r");
    if (file == NULL)
        return false;

    char buffer[1024];
    size_t n = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[n] = '\0';

    for (size_t i = 0; buffer[i] != '\0'; i++)
        buffer[i] = (char)tolower(buffer[i]);

    return strstr(buffer, "microsoft") != NULL || strstr(buffer, "wsl") != NULL;
}

static bool fileExists(const char *path)
{
    struct stat st;
    return lstat(path, &st) == 0;
}

static std::string shellQuote(const char *value)
{
    std::string result = "'";
    for (const char *p = value; *p != '\0'; p++)
    {
        if (*p == '\'')
            result += "'\\''";
        else
            result += *p;
    }
    result += "'";
    return result;
}

static bool runAndCapture(const std::string &command, std::string *output)
{
    FILE *pipe = popen(command.c_str(), "r");
    if (pipe == NULL)
        return false;

    char buffer[4096];
    output->clear();
    while (fgets(buffer, sizeof(buffer), pipe) != NULL)
        *output += buffer;

    int status = pclose(pipe);
    while (!output->empty() && (output->back() == '\n' || output->back() == '\r'))
        output->pop_back();

    return status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool toWindowsPath(const char *path, std::string *windowsPath)
{
    char absolutePath[PATH_MAX];
    const char *pathForWslpath = path;
    if (path[0] != '/')
    {
        char currentDir[PATH_MAX];
        if (getcwd(currentDir, sizeof(currentDir)) == NULL)
            return false;
        int written = snprintf(absolutePath, sizeof(absolutePath), "%s/%s", currentDir, path);
        if (written < 0 || (size_t)written >= sizeof(absolutePath))
            return false;
        pathForWslpath = absolutePath;
    }

    if (strncmp(pathForWslpath, "/mnt/", 5) != 0 ||
        !isalpha((unsigned char)pathForWslpath[5]) ||
        pathForWslpath[6] != '/')
    {
        return false;
    }

    std::string command = "wslpath -w -- " + shellQuote(pathForWslpath);
    return runAndCapture(command, windowsPath) && !windowsPath->empty();
}

static int runProcess(const std::vector<std::string> &args)
{
    std::vector<char *> argv;
    for (size_t i = 0; i < args.size(); i++)
        argv.push_back(const_cast<char *>(args[i].c_str()));
    argv.push_back(NULL);

    pid_t pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "trash: can not start powershell.exe: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0)
    {
        execvp(argv[0], argv.data());
        fprintf(stderr, "trash: can not start %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        fprintf(stderr, "trash: can not wait for powershell.exe: %s\n", strerror(errno));
        return 1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return 1;
}

static std::string powershellQuote(const std::string &value)
{
    std::string result = "'";
    for (size_t i = 0; i < value.size(); i++)
    {
        if (value[i] == '\'')
            result += "''";
        else
            result += value[i];
    }
    result += "'";
    return result;
}

static int moveFilesToWindowsRecycleBin(const std::vector<std::string> &windowsPaths)
{
    std::string pathArray = "@(";
    for (size_t i = 0; i < windowsPaths.size(); i++)
    {
        if (i > 0)
            pathArray += ",";
        pathArray += powershellQuote(windowsPaths[i]);
    }
    pathArray += ")";

    std::string command =
        "$ErrorActionPreference='Stop';"
        "Add-Type -AssemblyName Microsoft.VisualBasic;"
        "foreach ($p in " + pathArray + ") {"
        "if ([IO.Directory]::Exists($p)) {"
        "[Microsoft.VisualBasic.FileIO.FileSystem]::DeleteDirectory($p,'OnlyErrorDialogs','SendToRecycleBin')"
        "} else {"
        "[Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile($p,'OnlyErrorDialogs','SendToRecycleBin')"
        "}"
        "}";

    std::vector<std::string> args;
    args.push_back("powershell.exe");
    args.push_back("-NoProfile");
    args.push_back("-ExecutionPolicy");
    args.push_back("Bypass");
    args.push_back("-Command");
    args.push_back(command);

    return runProcess(args);
}

static int listWindowsRecycleBin(bool showAdditionalInfo)
{
    std::vector<std::string> args;
    args.push_back("powershell.exe");
    args.push_back("-NoProfile");
    args.push_back("-ExecutionPolicy");
    args.push_back("Bypass");
    args.push_back("-Command");
    if (showAdditionalInfo)
    {
        args.push_back(
            "$shell=New-Object -ComObject Shell.Application;"
            "$bin=$shell.Namespace(10);"
            "$items=@($bin.Items());"
            "$items | ForEach-Object { $_.Path };"
            "$size=0; $items | ForEach-Object { $size += $_.Size };"
            "Write-Output \"\";"
            "Write-Output \"Total: $size bytes in $($items.Count) item(s)\"");
    }
    else
    {
        args.push_back(
            "$shell=New-Object -ComObject Shell.Application;"
            "$bin=$shell.Namespace(10);"
            "$bin.Items() | ForEach-Object { $_.Path }");
    }

    return runProcess(args);
}

static int emptyWindowsRecycleBin(bool skipPrompt)
{
    std::vector<std::string> args;
    args.push_back("powershell.exe");
    args.push_back("-NoProfile");
    args.push_back("-ExecutionPolicy");
    args.push_back("Bypass");
    args.push_back("-Command");
    args.push_back(skipPrompt ? "Clear-RecycleBin -Force" : "Clear-RecycleBin");
    return runProcess(args);
}

static const char *baseName(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash == NULL ? path : slash + 1;
}

static void printUsage(const char *myBasename)
{
    printf("usage: %s [-vlesyF] <file> [<file> ...]\n", myBasename);
    printf(
        "\n"
        "  Move files/folders to the trash.\n"
        "\n"
        "  Options to use with <file>:\n"
        "\n"
        "  -v  Be verbose (show files as they are trashed, or if\n"
        "      used with the -l option, show additional information\n"
        "      about the trash contents)\n"
        "  -F  Accepted for compatibility.\n"
        "\n"
        "  Stand-alone options (to use without <file>):\n"
        "\n"
        "  -l  List items currently in the trash (add the -v option\n"
        "      to see additional information)\n"
        "  -e  Empty the trash (asks for confirmation)\n"
        "  -s  Empty the trash (asks for confirmation)\n"
        "  -y  Skips the confirmation prompt for -e and -s.\n"
        "      CAUTION: Deletes permanently instantly.\n"
        "\n"
        "  Options supported by `rm` are silently accepted.\n"
        "\n"
        "Version %d.%d.%d\n"
        "Copyright (c) 2010-2018 Ali Rantakari, http://hasseg.org/trash\n"
        "\n",
        VERSION_MAJOR,
        VERSION_MINOR,
        VERSION_BUILD);
}

int main(int argc, char *argv[])
{
    bool arg_list = false;
    bool arg_empty = false;
    bool arg_emptySecurely = false;
    bool arg_skipPrompt = false;
    int exitValue = 0;

    if (argc == 1)
    {
        printUsage(baseName(argv[0]));
        return 0;
    }

    if (!isWsl())
    {
        fprintf(stderr, "trash: this Linux build currently supports WSL only\n");
        return 1;
    }

    int firstPath = argc;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--") == 0)
        {
            firstPath = i + 1;
            break;
        }

        if (argv[i][0] != '-' || argv[i][1] == '\0')
        {
            firstPath = i;
            break;
        }

        for (int j = 1; argv[i][j] != '\0'; j++)
        {
            switch (argv[i][j])
            {
                case 'v': arg_verbose = true; break;
                case 'l': arg_list = true; break;
                case 'e': arg_empty = true; break;
                case 's': arg_emptySecurely = true; break;
                case 'y': arg_skipPrompt = true; break;
                case 'F': break;
                case 'd':
                case 'f':
                case 'i':
                case 'r':
                case 'P':
                case 'R':
                case 'W':
                    break;
                default:
                    printUsage(baseName(argv[0]));
                    return 1;
            }
        }
    }

    if (arg_list)
        return listWindowsRecycleBin(arg_verbose);

    if (arg_empty || arg_emptySecurely)
        return emptyWindowsRecycleBin(arg_skipPrompt);

    if (firstPath == argc)
    {
        printUsage(baseName(argv[0]));
        return 1;
    }

    std::vector<std::string> windowsPaths;
    for (int i = firstPath; i < argc; i++)
    {
        if (!fileExists(argv[i]))
        {
            fprintf(stderr, "trash: %s: path does not exist\n", argv[i]);
            exitValue = 1;
            continue;
        }

        std::string windowsPath;
        if (!toWindowsPath(argv[i], &windowsPath))
        {
            fprintf(stderr, "trash: %s: only /mnt/<drive>/ paths can be sent to the Windows Recycle Bin\n", argv[i]);
            exitValue = 1;
            continue;
        }

        windowsPaths.push_back(windowsPath);
    }

    if (!windowsPaths.empty())
    {
        int result = moveFilesToWindowsRecycleBin(windowsPaths);
        if (result != 0)
            exitValue = result;
        else if (arg_verbose)
        {
            for (size_t i = 0; i < windowsPaths.size(); i++)
                printf("%s\n", windowsPaths[i].c_str());
        }
    }

    return exitValue;
}
