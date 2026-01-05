#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>
#include "policy.h"

/*
 * Parse state machine states
 */
typedef enum {
    STATE_NONE,
    STATE_PROTECTED_FILES,
    STATE_NETWORK_WHITELIST,
    STATE_DEFAULT_NETWORK_POLICY,
    STATE_ALLOW_ALL_HTTPS
} ParseState;

int load_policy(const char *filename, Policy *policy)
{
    FILE *fh = fopen(filename, "r");
    if (!fh)
    {
        perror("fopen");
        return -1;
    }

    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fh);

    /* Initialize policy defaults */
    policy->protected_count = 0;
    policy->whitelist_count = 0;
    policy->network_mode = NET_POLICY_DENY;  /* Default: deny all */
    policy->allow_all_https = 0;

    ParseState state = STATE_NONE;
    int expecting_value = 0;
    ParseState pending_scalar_state = STATE_NONE;

    while (1)
    {
        if (!yaml_parser_parse(&parser, &event))
        {
            fprintf(stderr, "[!] YAML parse error\n");
            break;
        }

        if (event.type == YAML_SCALAR_EVENT)
        {
            char *val = (char *)event.data.scalar.value;

            /* Check if this is a key */
            if (strcmp(val, "protected_files") == 0)
            {
                state = STATE_PROTECTED_FILES;
            }
            else if (strcmp(val, "network_whitelist") == 0)
            {
                state = STATE_NETWORK_WHITELIST;
            }
            else if (strcmp(val, "default_network_policy") == 0)
            {
                pending_scalar_state = STATE_DEFAULT_NETWORK_POLICY;
                expecting_value = 1;
            }
            else if (strcmp(val, "allow_all_https") == 0)
            {
                pending_scalar_state = STATE_ALLOW_ALL_HTTPS;
                expecting_value = 1;
            }
            else if (expecting_value)
            {
                /* Process the value based on pending state */
                if (pending_scalar_state == STATE_DEFAULT_NETWORK_POLICY)
                {
                    if (strcmp(val, "ALLOW") == 0 || strcmp(val, "allow") == 0)
                    {
                        policy->network_mode = NET_POLICY_ALLOW;
                    }
                    else
                    {
                        policy->network_mode = NET_POLICY_DENY;
                    }
                }
                else if (pending_scalar_state == STATE_ALLOW_ALL_HTTPS)
                {
                    if (strcmp(val, "true") == 0 || strcmp(val, "yes") == 0 || strcmp(val, "1") == 0)
                    {
                        policy->allow_all_https = 1;
                    }
                }
                expecting_value = 0;
                pending_scalar_state = STATE_NONE;
            }
            else if (state == STATE_PROTECTED_FILES && policy->protected_count < MAX_PATHS)
            {
                strncpy(policy->protected_files[policy->protected_count],
                        val, MAX_LEN - 1);
                policy->protected_files[policy->protected_count][MAX_LEN - 1] = '\0';
                policy->protected_count++;
            }
            else if (state == STATE_NETWORK_WHITELIST && policy->whitelist_count < MAX_PATHS)
            {
                strncpy(policy->network_whitelist[policy->whitelist_count],
                        val, MAX_LEN - 1);
                policy->network_whitelist[policy->whitelist_count][MAX_LEN - 1] = '\0';
                policy->whitelist_count++;
            }
        }

        if (event.type == YAML_SEQUENCE_END_EVENT)
        {
            state = STATE_NONE;
        }

        if (event.type == YAML_STREAM_END_EVENT)
        {
            yaml_event_delete(&event);
            break;
        }

        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(fh);
    return 0;
}

void print_policy(const Policy *policy)
{
    printf("\n========== Security Policy ==========\n");
    
    /* Protected files */
    printf("\n[File Protection]\n");
    printf("  Protected paths (%d):\n", policy->protected_count);
    for (int i = 0; i < policy->protected_count; i++)
    {
        printf("    - %s\n", policy->protected_files[i]);
    }
    
    /* Network policy */
    printf("\n[Network Policy]\n");
    printf("  Default mode: %s\n", 
           policy->network_mode == NET_POLICY_ALLOW ? "ALLOW" : "DENY");
    
    if (policy->whitelist_count > 0)
    {
        printf("  Whitelisted hosts (%d):\n", policy->whitelist_count);
        for (int i = 0; i < policy->whitelist_count; i++)
        {
            printf("    - %s\n", policy->network_whitelist[i]);
        }
    }
    else
    {
        printf("  Whitelisted hosts: (none)\n");
    }
    
    printf("  Allow all HTTPS: %s\n", policy->allow_all_https ? "yes" : "no");
    
    printf("\n======================================\n\n");
}
