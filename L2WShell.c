#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define MAX_LINE 1024
#define MAX_ARGS 16

typedef int (__stdcall *ParseCommandFn)(char *, char *[], int);
typedef int (__cdecl *ExecuteCommandFn)(const char *, const char *const [], int);

typedef struct {
    const char *linux_name;
    const char *windows_name;
    int min_args;
    int max_args;
} Command;

static const Command commands[] = {
    {"ls", "dir", 0, 1}, {"cat", "type", 1, 1},
    {"cp", "copy", 2, 2}, {"mv", "move", 2, 2},
    {"rm", "del", 1, 1}, {"mkdir", "mkdir", 1, 1},
    {"rmdir", "rmdir", 1, 1}, {"echo", "echo", 1, MAX_ARGS - 1}
};

static const Command *find_command(const char *name)
{
    size_t i;
    for (i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i)
        if (strcmp(commands[i].linux_name, name) == 0)
            return &commands[i];
    return NULL;
}

static int safe_path(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    if (!p || !*p)
        return 0;
    while (*p) {
        if (*p < 32 || *p == 127 || strchr("&|<>^*%!?()\"", *p))
            return 0;
        ++p;
    }
    return 1;
}

static int dispatch(char *args[], int argc, ExecuteCommandFn execute)
{
    const Command *cmd;
    if (argc == 0)
        return 0;
    if (strcmp(args[0], "exit") == 0) {
        if (argc != 1) { puts("Usage: exit"); return 1; }
        return -1;
    }
    if (strcmp(args[0], "help") == 0) {
        if (argc != 1) { puts("Usage: help"); return 1; }
        puts("Commands: ls cat pwd cd cp mv rm mkdir rmdir clear echo help exit");
        return 0;
    }
    if (strcmp(args[0], "pwd") == 0) {
        char path[MAX_PATH];
        DWORD n;
        if (argc != 1) { puts("Usage: pwd"); return 1; }
        n = GetCurrentDirectoryA(sizeof(path), path);
        if (!n || n >= sizeof(path)) { puts("Unable to read working directory."); return 1; }
        puts(path);
        return 0;
    }
    if (strcmp(args[0], "cd") == 0) {
        if (argc != 2 || !safe_path(args[1])) { puts("Usage: cd DIRECTORY"); return 1; }
        if (!SetCurrentDirectoryA(args[1])) { puts("Cannot change directory."); return 1; }
        return 0;
    }
    if (strcmp(args[0], "clear") == 0) {
        HANDLE h;
        CONSOLE_SCREEN_BUFFER_INFO info;
        DWORD cells, written;
        COORD origin = {0, 0};
        if (argc != 1) { puts("Usage: clear"); return 1; }
        h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!GetConsoleScreenBufferInfo(h, &info)) return 1;
        cells = (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y;
        if (!FillConsoleOutputCharacterA(h, ' ', cells, origin, &written)) return 1;
        if (!FillConsoleOutputAttribute(h, info.wAttributes, cells, origin, &written)) return 1;
        if (!SetConsoleCursorPosition(h, origin)) return 1;
        return 0;
    }

    cmd = find_command(args[0]);
    if (!cmd) { puts("Unknown command. Type help."); return 1; }
    if (argc - 1 < cmd->min_args || argc - 1 > cmd->max_args) {
        puts("Incorrect number of arguments.");
        return 1;
    }
    return execute(cmd->windows_name, (const char *const *)(args + 1), argc - 1);
}

int main(void)
{
    HMODULE dll;
    ParseCommandFn parse;
    ExecuteCommandFn execute;
    char line[MAX_LINE];
    char *args[MAX_ARGS];
    int argc;

    dll = LoadLibraryA("L2WShellDLL.dll");
    if (!dll) {
        fprintf(stderr, "Unable to load L2WShellDLL.dll: %lu\n", GetLastError());
        return 1;
    }
    parse = GetProcAddress(dll, "ParseCmd");
    execute = GetProcAddress(dll, "ExecuteCmd");
    if (!parse || !execute) {
        fprintf(stderr, "Missing required DLL export.\n");
        FreeLibrary(dll);
        return 1;
    }
    FreeLibrary(dll);
    puts("L2WShell - Linux commands on Windows. Type help.");
    for (;;) {
        fputs("l2w> ", stdout);
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin))
            break;
        if (!strchr(line, '\n') && !feof(stdin)) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) { }
            puts("Input too long.");
            continue;
        }
        argc = parse(line, args, MAX_ARGS);
        if (argc < 0) { puts("Malformed command."); continue; }
        if (dispatch(args, argc, execute) == -1)
            break;
    }
    FreeLibrary(dll);
    return 0;
}
