
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>
#include "policy.h"

/*
 * Get the original username (before sudo)
 * Returns the detected username or NULL on error
 */
static const char *get_username(void)
{
    const char *username = getenv("SUDO_USER");
    if (username == NULL)
    {
        username = getenv("USER");
    }
    return username;
}

/*
 * Expand USERNAME placeholder in paths
 * Example: /home/USERNAME/.ssh -> /home/raghottam/.ssh
 */
static void expand_username(char *path, const char *username)
{
    char temp[MAX_LEN];
    char *pos = strstr(path, "USERNAME");

    if (pos == NULL)
    {
        return; // No USERNAME placeholder found
    }

    // Copy before USERNAME
    size_t prefix_len = pos - path;
    strncpy(temp, path, prefix_len);
    temp[prefix_len] = '\0';

    // Add actual username
    strcat(temp, username);

    // Add after USERNAME
    strcat(temp, pos + 8); // 8 = strlen("USERNAME")

    // Copy back to original
    strncpy(path, temp, MAX_LEN);
}

int load_policy(const char *filename, Policy *policy)
{
    FILE *fh = fopen(filename, "r");
    if (!fh)
    {
        perror("fopen");
        return -1;
    }

    // Get username for path expansion
    const char *username = get_username();
    if (username == NULL)
    {
        fprintf(stderr, "[!] Error: Could not detect username\n");
        fclose(fh);
        return -1;
    }

    printf("[+] Detected user: %s\n", username);

    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fh);

    policy->protected_count = 0;
    int in_protected = 0;

    while (1)
    {
        yaml_parser_parse(&parser, &event);

        if (event.type == YAML_SCALAR_EVENT)
        {
            char *val = (char *)event.data.scalar.value;

            if (strcmp(val, "protected_files") == 0)
            {
                in_protected = 1;
            }
            else if (in_protected)
            {
                // Copy the path
                strncpy(policy->protected_files[policy->protected_count],
                        val, MAX_LEN);

                // Expand USERNAME placeholder
                expand_username(policy->protected_files[policy->protected_count],
                                username);

                policy->protected_count++;
            }
        }

        if (event.type == YAML_SEQUENCE_END_EVENT)
        {
            in_protected = 0;
        }

        if (event.type == YAML_STREAM_END_EVENT)
            break;

        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(fh);
    return 0;
}

void print_policy(const Policy *policy)
{
    printf("\n--- Security Policy ---\n");
    printf("Protected files (%d):\n", policy->protected_count);
    for (int i = 0; i < policy->protected_count; i++)
    {
        printf("  - %s\n", policy->protected_files[i]);
    }
    printf("-----------------------\n\n");
}
