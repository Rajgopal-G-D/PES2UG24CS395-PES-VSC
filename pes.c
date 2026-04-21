#include "pes.h"
#include "index.h"
#include "commit.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

// Forward declarations from object/tree modules.
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);
int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out);

// Phase 5 APIs are analysis-only in this repository; provide stubs so current phases link.
int branch_create(const char *name) { (void)name; return -1; }
int branch_delete(const char *name) { (void)name; return -1; }
void branch_list(void) { fprintf(stderr, "error: branch command not implemented in this lab phase\n"); }
int checkout(const char *target) { (void)target; return -1; }

static int ensure_dir(const char *path) {
#ifdef _WIN32
    if (mkdir(path) == 0) return 0;
#else
    if (mkdir(path, 0755) == 0) return 0;
#endif
    if (errno == EEXIST) return 0;
    return -1;
}

void cmd_init(void) {
    if (ensure_dir(PES_DIR) != 0 || ensure_dir(OBJECTS_DIR) != 0 ||
        ensure_dir(".pes/refs") != 0 || ensure_dir(REFS_DIR) != 0) {
        fprintf(stderr, "error: failed to initialize repository\n");
        return;
    }

    FILE *head = fopen(HEAD_FILE, "w");
    if (!head) {
        fprintf(stderr, "error: failed to write HEAD\n");
        return;
    }
    fprintf(head, "ref: refs/heads/main\n");
    fclose(head);

    FILE *main_ref = fopen(".pes/refs/heads/main", "a");
    if (main_ref) fclose(main_ref);

    printf("Initialized empty PES repository in .pes\n");
}

void cmd_add(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: pes add <file>...\n");
        return;
    }

    Index *index = malloc(sizeof(Index));
    if (!index) {
        fprintf(stderr, "error: out of memory\n");
        return;
    }

    if (index_load(index) != 0) {
        free(index);
        fprintf(stderr, "error: failed to load index\n");
        return;
    }

    int ok = 1;
    for (int i = 2; i < argc; i++) {
        if (index_add(index, argv[i]) != 0) {
            fprintf(stderr, "error: failed to add '%s'\n", argv[i]);
            ok = 0;
        }
    }

    if (ok && index_save(index) == 0) {
        printf("Added %d file(s) to index\n", argc - 2);
    } else if (ok) {
        fprintf(stderr, "error: failed to save index\n");
    }

    free(index);
}

void cmd_status(void) {
    Index *index = malloc(sizeof(Index));
    if (!index) {
        fprintf(stderr, "error: out of memory\n");
        return;
    }

    if (index_load(index) != 0) {
        free(index);
        fprintf(stderr, "error: failed to load index\n");
        return;
    }
    index_status(index);
    free(index);
}

void cmd_commit(int argc, char *argv[]) {
    const char *message = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            message = argv[i + 1];
            break;
        }
    }

    if (!message || message[0] == '\0') {
        fprintf(stderr, "error: commit requires a message (-m \"message\")\n");
        return;
    }

    ObjectID commit_id;
    if (commit_create(message, &commit_id) != 0) {
        fprintf(stderr, "error: commit failed\n");
        return;
    }

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&commit_id, hex);
    printf("Committed: %.12s... %s\n", hex, message);
}

static void print_log_entry(const ObjectID *id, const Commit *commit, void *ctx) {
    (void)ctx;
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);
    printf("commit %s\n", hex);
    printf("Author: %s\n", commit->author);
    printf("Date:   %llu\n\n", (unsigned long long)commit->timestamp);
    printf("    %s\n\n", commit->message);
}

void cmd_log(void) {
    if (commit_walk(print_log_entry, NULL) != 0) {
        fprintf(stderr, "No commits yet.\n");
    }
}

// ─── PROVIDED: Phase 5 Command Wrappers ─────────────────────────────────────

// Usage: 
//   pes branch          (lists branches)
//   pes branch <name>   (creates a branch)
//   pes branch -d <name>(deletes a branch)
void cmd_branch(int argc, char *argv[]) {
    if (argc == 2) {
        branch_list();
    } else if (argc == 3) {
        if (branch_create(argv[2]) == 0) {
            printf("Created branch '%s'\n", argv[2]);
        } else {
            fprintf(stderr, "error: failed to create branch '%s'\n", argv[2]);
        }
    } else if (argc == 4 && strcmp(argv[2], "-d") == 0) {
        if (branch_delete(argv[3]) == 0) {
            printf("Deleted branch '%s'\n", argv[3]);
        } else {
            fprintf(stderr, "error: failed to delete branch '%s'\n", argv[3]);
        }
    } else {
        fprintf(stderr, "Usage:\n  pes branch\n  pes branch <name>\n  pes branch -d <name>\n");
    }
}

// Usage: pes checkout <branch_or_commit>
void cmd_checkout(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: pes checkout <branch_or_commit>\n");
        return;
    }

    const char *target = argv[2];
    if (checkout(target) == 0) {
        printf("Switched to '%s'\n", target);
    } else {
        fprintf(stderr, "error: checkout failed. Do you have uncommitted changes?\n");
    }
}

// ─── PROVIDED: Command dispatch ─────────────────────────────────────────────

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pes <command> [args]\n");
        fprintf(stderr, "\nCommands:\n");
        fprintf(stderr, "  init            Create a new PES repository\n");
        fprintf(stderr, "  add <file>...   Stage files for commit\n");
        fprintf(stderr, "  status          Show working directory status\n");
        fprintf(stderr, "  commit -m <msg> Create a commit from staged files\n");
        fprintf(stderr, "  log             Show commit history\n");
        fprintf(stderr, "  branch          List, create, or delete branches\n");
        fprintf(stderr, "  checkout <ref>  Switch branches or restore working tree\n");
        return 1;
    }

    const char *cmd = argv[1];

    if      (strcmp(cmd, "init") == 0)     cmd_init();
    else if (strcmp(cmd, "add") == 0)      cmd_add(argc, argv);
    else if (strcmp(cmd, "status") == 0)   cmd_status();
    else if (strcmp(cmd, "commit") == 0)   cmd_commit(argc, argv);
    else if (strcmp(cmd, "log") == 0)      cmd_log();
    else if (strcmp(cmd, "branch") == 0)   cmd_branch(argc, argv);
    else if (strcmp(cmd, "checkout") == 0) cmd_checkout(argc, argv);
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        fprintf(stderr, "Run 'pes' with no arguments for usage.\n");
        return 1;
    }

    return 0;
}